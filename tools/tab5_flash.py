#!/usr/bin/env python3
"""Local-only Tab5 application update/restore. Never writes NVS, bootloader or eFuses."""
import argparse
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace

FLASH_SIZE = 16 * 1024 * 1024
APP_OFFSET = 0x10000
APP_SIZE = 10 * 1024 * 1024
TABLE_OFFSET = 0x8000
TABLE_SIZE = 0xC00
CHIP_ID = 18

class FlashError(Exception):
    pass

def require(condition, message):
    if not condition:
        raise FlashError(message)

def digest(data):
    return hashlib.sha256(data).hexdigest()

def read_json(path):
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as error:
        raise FlashError('Cannot read metadata; use an intact release or backup directory.') from error

def image_info(data):
    require(len(data) >= 288 and data[0] == 0xE9, 'Not an ESP application image.')
    require(struct.unpack_from('<H', data, 12)[0] == CHIP_ID, 'Firmware is not for ESP32-P4.')
    require(struct.unpack_from('<I', data, 32)[0] == 0xABCD5432, 'Application descriptor is missing.')
    require(data[80:112].split(b'\0')[0] == b'm5stack_tab5', 'Firmware is not the Tab5 UserDemo application.')
    require(data[14] <= 1, 'Firmware requires a newer chip revision.')
    minimum, maximum = struct.unpack_from('<HH', data, 15)
    require(minimum <= 103 and (maximum in (0, 65535) or maximum >= 103), 'Firmware is incompatible with revision 1.3.')
    return {'version': data[48:80].split(b'\0')[0].decode('ascii', errors='replace'),
            'project': data[80:112].split(b'\0')[0].decode('ascii', errors='replace')}

def validate_table(data):
    require(len(data) == TABLE_SIZE, 'Unexpected partition table size.')
    entries = []
    md5_found = False
    for pos in range(0, len(data), 32):
        entry = data[pos:pos+32]
        if entry[:2] == b'\xeb\xeb':
            require(entry[16:] == hashlib.md5(data[:pos]).digest(), 'Partition table checksum failed.')
            require(all(b == 255 for b in data[pos+32:]), 'Unexpected partition table trailer.')
            md5_found = True
            break
        require(entry[:2] == b'\xaa\x50', 'Invalid partition table entry.')
        _, kind, subtype, offset, size, label, flags = struct.unpack('<HBBII16sI', entry)
        require(flags == 0 and offset >= 0x9000 and size > 0 and offset+size <= FLASH_SIZE,
                'Unsupported encrypted or out-of-range partition.')
        entries.append((kind, subtype, offset, size, label.rstrip(b'\0')))
    require(md5_found, 'Partition table has no checksum.')
    expected = [(1,2,0x9000,0x6000,b'nvs'), (1,1,0xF000,0x1000,b'phy_init'),
                (0,0,APP_OFFSET,APP_SIZE,b'factory'),
                (1,0x82,0xA10000,400*1024,b'human_face_det'),
                (1,0x82,0xA74000,2*1024*1024,b'storage')]
    require(entries == expected, 'Partition layout is not the supported official Tab5 layout. No flash was erased.')

def load_bundle(folder):
    manifest = read_json(folder / 'manifest.json')
    require(manifest.get('schema') == 1 and manifest.get('board') == 'tab5-v3' and
            manifest.get('chip') == 'esp32p4' and manifest.get('flash_bytes') == FLASH_SIZE,
            'Unsupported release target.')
    require(manifest.get('app_offset') == APP_OFFSET and manifest.get('app_partition_bytes') == APP_SIZE,
            'Unsupported application range.')
    files = manifest.get('files', {})
    require('application.bin' in files and 'partition-table.bin' in files, 'Release image manifest is incomplete.')
    for name, spec in files.items():
        path=folder/name
        require(not Path(name).is_absolute() and path.resolve().is_relative_to(folder.resolve()), 'Invalid release file path.')
        data = path.read_bytes()
        require(len(data) == spec.get('bytes') and digest(data) == spec.get('sha256'),
                'Release checksum mismatch. Download and extract the release again.')
    app = (folder / 'application.bin').read_bytes()
    table = (folder / 'partition-table.bin').read_bytes()
    require(288 <= len(app) <= APP_SIZE and len(app) % 4 == 0, 'Application size is invalid.')
    image_info(app)
    validate_table(table)
    return manifest, app, table

def validate_device(info):
    require(info['chip'] == 'ESP32-P4', 'Connected chip is not ESP32-P4; nothing was written.')
    require(info['revision'] == 103, 'This release is limited to ESP32-P4 revision 1.3.')
    require(info['flash_bytes'] == FLASH_SIZE, 'This release requires 16 MB flash.')
    require(not info['secure'], 'Protected flash requires its provisioned update process; this tool cannot update it.')

def private_write(path, data):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'wb') as out:
        out.write(data)
        out.flush()
        os.fsync(out.fileno())

def create_backup(device, info, table, parent):
    parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    folder = Path(tempfile.mkdtemp(prefix='tab5-', dir=parent))
    app = device.read(APP_OFFSET, APP_SIZE)
    require(len(app) == APP_SIZE and device.matches(APP_OFFSET, app),
            'Backup read-back check failed; update has not started.')
    require(device.read(TABLE_OFFSET, TABLE_SIZE) == table, 'Partition table changed during backup.')
    private_write(folder / 'previous-application.bin', app)
    private_write(folder / 'partition-table.bin', table)
    metadata = {'schema':1, 'device':info['identity'], 'chip':info['chip'],
                'revision':info['revision'], 'flash_bytes':info['flash_bytes'],
                'application_sha256':digest(app), 'table_sha256':digest(table)}
    # Metadata is the commit marker: an interrupted backup is never restorable.
    private_write(folder / 'backup.json', (json.dumps(metadata, indent=2)+'\n').encode())
    require(digest((folder/'previous-application.bin').read_bytes()) == metadata['application_sha256'],
            'Backup file verification failed; update has not started.')
    return folder

def update(device, bundle, backup_root, report=print):
    manifest, app, expected_table = load_bundle(bundle)
    info = device.info()
    validate_device(info)
    table = device.read(TABLE_OFFSET, TABLE_SIZE)
    validate_table(table)
    require(table == expected_table, 'Device partition table differs from the release; update refused.')
    if device.matches(APP_OFFSET, app):
        report('This application is already installed. No write needed.')
        return None
    report('Creating and verifying a local application backup...')
    backup = create_backup(device, info, table, backup_root)
    report('Backup verified: '+str(backup))
    # Keep the verified image in memory so a changed file cannot bypass checks.
    report('Updating application only; keep USB connected...')
    device.write(APP_OFFSET, app)
    require(device.matches(APP_OFFSET, app), 'Firmware verification failed. Restore the backup shown above.')
    require(device.read(TABLE_OFFSET, TABLE_SIZE) == table, 'Partition verification failed after update.')
    report('Application verified. Saved Wi-Fi and other data partitions were not written.')
    return backup

def restore(device, folder, backup_root, report=print):
    meta = read_json(folder / 'backup.json')
    require(meta.get('schema') == 1, 'Unsupported backup format.')
    app = (folder / 'previous-application.bin').read_bytes()
    table = (folder / 'partition-table.bin').read_bytes()
    require(len(app) == APP_SIZE and digest(app) == meta.get('application_sha256') and
            digest(table) == meta.get('table_sha256'), 'Backup checksum mismatch; restore refused.')
    validate_table(table)
    info = device.info()
    validate_device(info)
    require(meta.get('device') == info['identity'], 'Backup belongs to another device; restore refused.')
    require(device.read(TABLE_OFFSET, TABLE_SIZE) == table, 'Device layout changed; application-only restore refused.')
    report('Backing up the current application before restoration...')
    safety = create_backup(device, info, table, backup_root)
    report('Current application backup verified: '+str(safety))
    device.write(APP_OFFSET, app)
    require(device.matches(APP_OFFSET, app), 'Restore verification failed; keep backups and retry.')
    require(device.read(TABLE_OFFSET, TABLE_SIZE) == table, 'Partition verification failed after restore.')
    report('Previous application restored and verified; current saved Wi-Fi is preserved.')
    return safety

class EspDevice:
    def __init__(self, port):
        import esptool
        from esptool import cmds
        require(esptool.__version__ == '4.12.0', 'Install the bundled requirements.txt (esptool 4.12.0).')
        self.cmds = cmds
        self.esp = cmds.detect_chip(port=port, baud=115200)
        try:
            self.prepare()
        except BaseException:
            self.esp._port.close()
            raise

    def prepare(self):
        # Identify before uploading a RAM stub or using chip-specific commands.
        require(self.esp.CHIP_NAME == 'ESP32-P4', 'Connected device is not ESP32-P4.')
        require(not self.esp.secure_download_mode, 'Secure download mode is unsupported.')
        security = self.esp.get_security_info()
        self.secure = bool(security['flags'] & 5 or self.esp.get_secure_boot_enabled() or
                           self.esp.get_flash_encryption_enabled())
        require(not self.secure, 'Protected device: use the provisioned update process.')
        self.identity = digest(bytes(self.esp.read_mac()))
        self.revision = self.esp.get_chip_revision()
        self.esp = self.esp.run_stub()
        capacity = (self.esp.flash_id() >> 16) & 255
        self.flash_bytes = 1 << capacity if capacity < 32 else 0
        validate_device(self.info())
        self.esp.flash_set_parameters(FLASH_SIZE)
        self.esp.change_baud(921600)

    def info(self):
        return {'chip':self.esp.CHIP_NAME, 'revision':self.revision,
                'flash_bytes':self.flash_bytes, 'identity':self.identity, 'secure':self.secure}

    def read(self, offset, size):
        return self.esp.read_flash(offset, size)

    def matches(self, offset, data):
        return self.esp.flash_md5sum(offset, len(data)) == hashlib.md5(data).hexdigest()

    def write(self, offset, data):
        require(offset == APP_OFFSET and 0 < len(data) <= APP_SIZE, 'Write is outside application region.')
        stream = io.BytesIO(data)
        stream.name = 'verified-application.bin'
        args = SimpleNamespace(addr_filename=[(offset, stream)], compress=True, no_compress=False,
                no_stub=False, force=False, encrypt=False, encrypt_files=None,
                ignore_flash_encryption_efuse_setting=False, flash_size='keep', flash_freq='keep',
                flash_mode='keep', chip='esp32p4', erase_all=False)
        self.cmds.write_flash(self.esp, args)

    def close(self, reboot=False):
        try:
            if reboot:
                self.esp.hard_reset()
        finally:
            self.esp._port.close()

def choose_port(port):
    if port != 'auto':
        require('://' not in port, 'Only local serial ports are allowed.')
        return port
    from serial.tools import list_ports
    candidates = [p.device for p in list_ports.comports() if p.vid == 0x303A]
    require(len(candidates) == 1, 'Connect one Tab5 data cable, or specify its local --port explicitly.')
    return candidates[0]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['verify', 'check', 'update', 'restore'])
    parser.add_argument('--bundle', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--port', default='auto')
    parser.add_argument('--board', choices=['tab5-v3'], help='Confirm the physical board; ROM cannot identify the board model')
    parser.add_argument('--backup', type=Path, help='Backup directory to restore')
    parser.add_argument('--backup-dir', type=Path, default=Path('backups'))
    parser.add_argument('--log-dir', type=Path, default=Path('logs'))
    args = parser.parse_args()
    device = None
    try:
        if args.command != 'restore':
            manifest, _, expected = load_bundle(args.bundle)
        if args.command == 'verify':
            print('Bundle verified: '+manifest['version']+'; no device accessed.')
            return 0
        require(args.board == 'tab5-v3', 'Specify --board tab5-v3 after checking the physical board model.')
        if args.command == 'restore':
            require(args.backup is not None, 'Specify --backup with the previous backup directory.')
        args.log_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
        fd, logfile = tempfile.mkstemp(prefix='tab5-flash-', suffix='.log', dir=args.log_dir)
        console = sys.stdout
        def report(message):
            print(message, file=console, flush=True)
        report('Connecting resets Tab5. Close any serial monitor and keep the data cable attached.')
        with os.fdopen(fd, 'w') as log, contextlib.redirect_stdout(log), contextlib.redirect_stderr(log):
            device = EspDevice(choose_port(args.port))
            report('Detected ESP32-P4 revision 1.3 / 16 MB; security configuration supported.')
            if args.command == 'check':
                table = device.read(TABLE_OFFSET, TABLE_SIZE)
                validate_table(table)
                require(table == expected, 'Partition layout differs from this release.')
                report('Device layout is compatible. No flash written.')
            elif args.command == 'update':
                update(device, args.bundle, args.backup_dir, report)
            else:
                restore(device, args.backup, args.backup_dir, report)
            device.close(reboot=True)
            device = None
        report('Finished. Raw diagnostics remain local: '+str(logfile))
        return 0
    except (FlashError, OSError, ImportError) as error:
        print('Stopped: '+str(error), file=sys.stderr)
        return 1
    except Exception:
        print('Device operation failed. Keep the backup; check the local log and retry or restore.', file=sys.stderr)
        return 1
    finally:
        if device:
            device.close(reboot=False)

if __name__ == '__main__':
    sys.exit(main())
