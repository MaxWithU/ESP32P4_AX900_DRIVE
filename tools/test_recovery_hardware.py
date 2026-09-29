#!/usr/bin/env python3
"""Local-only, credential-free fault test for the optional diagnostic firmware.
Raw serial data stays under the ignored logs directory; stdout contains aggregates only.
Requires pyserial. Interrupting closes the port without intentionally resetting the board.
"""
import argparse,json,re,time
from pathlib import Path
import serial

class Device:
    def __init__(self,port,log):
        self.port=serial.Serial(port=None,baudrate=115200,timeout=.1)
        self.port.dtr=self.port.rts=False;self.port.port=port;self.port.open()
        self.log=log.open('ab');self.buffer=b'';self.link={};self.recovery={};self.probe={}
        self.link_at=0;self.fault=None;self.probe_start=None;self.crashed=False
    def command(self,s):self.port.write((s+'\n').encode())
    def pump(self,seconds=.2):
        until=time.monotonic()+seconds
        while time.monotonic()<until:
            data=self.port.read(8192)
            if not data:continue
            self.log.write(data);self.log.flush();self.buffer+=data
            while b'\n' in self.buffer:
                raw,self.buffer=self.buffer.split(b'\n',1)
                line=raw.decode(errors='replace')
                if any(x in line for x in ['Guru Meditation','assert failed','abort()','Stack protection fault']):self.crashed=True
                values={k:int(v) for k,v in re.findall(r'([a-z_]+)=(-?\d+)(?=\s|$)',line)}
                if line.startswith('AX900_LINK '):self.link=values;self.link_at=time.monotonic()
                elif line.startswith('AX900_RECOVERY '):self.recovery=values
                elif line.startswith('AX900_PROBE '):self.probe=values
                elif line.startswith('AX900_FAULT '):self.fault=int(line.split()[1])
                elif line.startswith('AX900_PROBE_START '):self.probe_start=int(line.split()[1])
            if self.crashed:raise RuntimeError('Device crash; inspect local log')
    def connected(self,timeout=100,after=0):
        until=time.monotonic()+timeout
        while time.monotonic()<until:
            self.command('ax900 status');self.pump(1)
            if self.link_at>after and self.link.get('has_ip')==1:return
        raise RuntimeError('Connection timeout; inspect local log')
    def ping(self):
        self.probe={};self.probe_start=None;self.command('ax900 probe')
        acknowledgement_deadline=time.monotonic()+5
        while self.probe_start is None and time.monotonic()<acknowledgement_deadline:self.pump(.25)
        if self.probe_start!=0:raise RuntimeError('Gateway test did not start')
        until=time.monotonic()+15
        while time.monotonic()<until:
            self.command('ax900 test-status');self.pump(1)
            if self.probe.get('state') in (2,3,4,5):
                if self.probe.get('state') in (4,5) or self.probe.get('received',0)==0:raise RuntimeError('Gateway unreachable')
                return self.probe.copy()
        raise RuntimeError('Gateway test timeout')
    def close(self):self.port.close();self.log.close()

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--port',default='/dev/cu.usbmodem1101');parser.add_argument('--rounds',type=int,default=1)
    args=parser.parse_args();folder=Path(__file__).resolve().parents[1]/'logs';folder.mkdir(exist_ok=True)
    d=Device(args.port,folder/'recovery-hardware.log');results=[]
    def report(value):
        results.append(value);(folder/'recovery-results.local.json').write_text(json.dumps(results,indent=2));print(json.dumps(value),flush=True)
    try:
        d.connected();report({'stage':'boot','connected':True,'ping':d.ping(),'memory':{k:d.recovery.get(k) for k in ['heap','internal']}})
        for round_index in range(args.rounds):
            if round_index:
                until=time.monotonic()+65
                while time.monotonic()<until:d.command('ax900 status');d.pump(1)
            for name in ['rx','link','dhcp']:
                before=d.recovery.copy();d.fault=None;start=time.monotonic();d.command('ax900 fault-'+name);d.pump(.4)
                if d.fault!=0:raise RuntimeError('Fault injection not accepted: '+name)
                # Do not accept an old, pre-fault connected status as proof of recovery.
                until=time.monotonic()+20;offline=False
                while time.monotonic()<until:
                    d.command('ax900 status');d.pump(.5)
                    if d.link_at>start and d.link.get('has_ip')==0:offline=True;break
                if not offline:raise RuntimeError('No observed link-down transition: '+name)
                d.connected(after=d.link_at);elapsed=round(time.monotonic()-start,2)
                ping=d.ping();d.command('ax900 status');d.pump(.3)
                report({'stage':name,'round':round_index+1,'recovered_seconds':elapsed,'ping':ping,
                        'usb_error_delta':d.recovery.get('usb_errors',0)-before.get('usb_errors',0),
                        'memory':{k:d.recovery.get(k) for k in ['heap','internal']}})
        d.command('ax900 disconnect');start=time.monotonic();d.pump(1)
        while time.monotonic()-start<25:
            d.command('ax900 status');d.pump(1)
            if d.link.get('has_ip') or d.recovery.get('enabled'):raise RuntimeError('Manual disconnect was overridden')
        report({'stage':'manual_disconnect','remained_offline_seconds':25,'auto_reconnect_enabled':False})
    finally:d.close()
if __name__=='__main__':main()
