#!/usr/bin/env python3
"""Synthetic flash; no physical device, serial connection, or network access."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import Mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import tab5_flash as flash

def table_image():
    entries=[(1,2,0x9000,0x6000,b'nvs'),(1,1,0xF000,0x1000,b'phy_init'),
             (0,0,0x10000,0xA00000,b'factory'),(1,0x82,0xA10000,0x64000,b'human_face_det'),
             (1,0x82,0xA74000,0x200000,b'storage')]
    table=b''.join(struct.pack('<HBBII16sI',0x50aa,*e,0) for e in entries)
    table+=b'\xeb\xeb'+b'\xff'*14+hashlib.md5(table).digest()
    return table.ljust(flash.TABLE_SIZE,b'\xff')

def app_image():
    app=bytearray(320);app[0]=0xe9
    struct.pack_into('<H',app,12,18);struct.pack_into('<H',app,17,65535)
    struct.pack_into('<I',app,32,0xabcd5432)
    app[48:58]=b'0.1.0-rc.1';app[80:92]=b'm5stack_tab5'
    return bytes(app)

class Device:
    def __init__(self):
        self.memory=bytearray(b'\xa5'*flash.FLASH_SIZE)
        self.memory[flash.TABLE_OFFSET:flash.TABLE_OFFSET+flash.TABLE_SIZE]=table_image()
        self.properties=dict(chip='ESP32-P4',revision=103,flash_bytes=flash.FLASH_SIZE,secure=False,identity='test-device')
        self.writes=[];self.bad_backup=False;self.fail_write=False;self.bad_verify=False
    def info(self):return self.properties
    def read(self,offset,size):return bytes(self.memory[offset:offset+size])
    def matches(self,offset,data):
        if self.bad_backup and len(data)==flash.APP_SIZE:return False
        if self.bad_verify and self.writes:return False
        return self.read(offset,len(data))==data
    def write(self,offset,data):
        self.writes.append((offset,len(data)))
        if self.fail_write:
            self.memory[offset:offset+128]=data[:128];raise OSError('simulated USB loss')
        self.memory[offset:offset+len(data)]=data

class ReleaseToolsTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.bundle=self.root/'release';self.bundle.mkdir();self.backups=self.root/'backups'
        self.app=app_image();self.table=table_image()
        for name,data in [('application.bin',self.app),('partition-table.bin',self.table),('tab5_flash.py',b'test-tool')]:
            (self.bundle/name).write_bytes(data)
        self.manifest={'schema':1,'version':'0.1.0-rc.1','board':'tab5-v3','chip':'esp32p4',
                       'flash_bytes':flash.FLASH_SIZE,'app_offset':flash.APP_OFFSET,'app_partition_bytes':flash.APP_SIZE,
                       'files':{p.name:{'bytes':p.stat().st_size,'sha256':flash.digest(p.read_bytes())} for p in self.bundle.iterdir()}}
        self.save_manifest();self.device=Device()
    def tearDown(self):self.tmp.cleanup()
    def save_manifest(self):(self.bundle/'manifest.json').write_text(json.dumps(self.manifest))
    def update(self):return flash.update(self.device,self.bundle,self.backups,lambda _:None)
    def test_update_restore_preserve_every_other_partition(self):
        before=flash.digest(self.device.memory[:flash.APP_OFFSET]+self.device.memory[flash.APP_OFFSET+flash.APP_SIZE:])
        old=self.device.read(flash.APP_OFFSET,flash.APP_SIZE)
        backup=self.update();self.assertEqual(self.device.writes,[(flash.APP_OFFSET,len(self.app))])
        self.assertEqual((backup/'previous-application.bin').read_bytes(),old)
        flash.restore(self.device,backup,self.backups,lambda _:None)
        self.assertEqual(self.device.read(flash.APP_OFFSET,flash.APP_SIZE),old)
        self.assertEqual(flash.digest(self.device.memory[:flash.APP_OFFSET]+self.device.memory[flash.APP_OFFSET+flash.APP_SIZE:]),before)
        self.assertEqual((backup/'backup.json').stat().st_mode&0o777,0o600)
    def test_wrong_hardware_and_security_never_write(self):
        for field,value in [('chip','ESP32-S3'),('revision',300),('flash_bytes',8*1024*1024),('secure',True)]:
            with self.subTest(field=field):
                previous=self.device.properties[field];self.device.properties[field]=value
                with self.assertRaises(flash.FlashError):self.update()
                self.device.properties[field]=previous
                self.assertEqual(self.device.writes,[])
    def test_corrupt_image_and_tool_rejected_before_device_access(self):
        for name in ['application.bin','tab5_flash.py']:
            path=self.bundle/name;original=path.read_bytes();path.write_bytes(original+b'x')
            with self.assertRaises(flash.FlashError):self.update()
            path.write_bytes(original)
        self.assertEqual(self.device.writes,[])
    def test_wrong_layout_and_offset_never_write(self):
        self.device.memory[flash.TABLE_OFFSET+7]^=1
        with self.assertRaises(flash.FlashError):self.update()
        self.device.memory[flash.TABLE_OFFSET:flash.TABLE_OFFSET+flash.TABLE_SIZE]=self.table
        self.manifest['app_offset']=0x9000;self.save_manifest()
        with self.assertRaises(flash.FlashError):self.update()
        self.assertEqual(self.device.writes,[])
    def test_backup_failure_never_writes(self):
        self.device.bad_backup=True
        with self.assertRaises(flash.FlashError):self.update()
        self.assertEqual(self.device.writes,[])
        self.assertFalse(list(self.backups.rglob('backup.json')))
    def test_interrupted_update_keeps_verified_restorable_backup(self):
        original=self.device.read(flash.APP_OFFSET,flash.APP_SIZE);self.device.fail_write=True
        with self.assertRaises(OSError):self.update()
        backup=next(self.backups.glob('*/backup.json')).parent
        self.device.fail_write=False
        flash.restore(self.device,backup,self.backups,lambda _:None)
        self.assertEqual(self.device.read(flash.APP_OFFSET,flash.APP_SIZE),original)
    def test_bad_write_verification_is_not_success(self):
        self.device.bad_verify=True
        with self.assertRaises(flash.FlashError):self.update()
        self.assertEqual(len(list(self.backups.glob('*/backup.json'))),1)
    def test_restore_rejects_other_device_and_corrupt_backup(self):
        backup=self.update();writes=len(self.device.writes)
        self.device.properties['identity']='other-device'
        with self.assertRaises(flash.FlashError):flash.restore(self.device,backup,self.backups,lambda _:None)
        self.device.properties['identity']='test-device'
        with (backup/'previous-application.bin').open('r+b') as f:f.write(b'bad')
        with self.assertRaises(flash.FlashError):flash.restore(self.device,backup,self.backups,lambda _:None)
        self.assertEqual(len(self.device.writes),writes)
    def test_same_image_skips_write_and_backup(self):
        self.device.memory[flash.APP_OFFSET:flash.APP_OFFSET+len(self.app)]=self.app
        self.assertIsNone(self.update());self.assertEqual(self.device.writes,[])
        self.assertFalse(self.backups.exists())
    def test_manifest_escape_rejected(self):
        self.manifest['files']['../escape']={};self.save_manifest()
        with self.assertRaises(flash.FlashError):flash.load_bundle(self.bundle)
    def test_pinned_esptool_adapter_has_no_force_or_global_erase(self):
        device=flash.EspDevice.__new__(flash.EspDevice);device.esp=object();device.cmds=Mock()
        device.write(flash.APP_OFFSET,self.app)
        args=device.cmds.write_flash.call_args.args[1]
        self.assertFalse(args.force or args.erase_all or args.encrypt)
        self.assertEqual(args.flash_size,'keep')
        self.assertEqual(args.addr_filename[0][1].read(),self.app)
        with self.assertRaises(flash.FlashError):device.write(0x9000,self.app)
    def test_image_descriptor_bounds_and_target(self):
        self.assertEqual(flash.image_info(self.app)['version'],'0.1.0-rc.1')
        for length in [0,12,32,287]:
            with self.assertRaises(flash.FlashError):flash.image_info(self.app[:length])
        damaged=bytearray(self.app);damaged[12]=9
        with self.assertRaises(flash.FlashError):flash.image_info(damaged)

if __name__=='__main__':unittest.main()
