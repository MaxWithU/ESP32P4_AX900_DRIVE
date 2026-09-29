#!/usr/bin/env python3
"""Run repeatable synthetic checks, saving raw serial only under ignored logs/."""
import argparse,json,time
from pathlib import Path
from test_recovery_hardware import Device

def check(d,kind,host,port,dns=''):
    d.check={};d.check_start=None
    d.command(f'ax900 check {kind} {host} {port}'+(f' {dns}' if dns else ''))
    deadline=time.monotonic()+5
    while d.check_start is None and time.monotonic()<deadline:d.pump(.2)
    if d.check_start!=0:return {'kind':kind,'start_error':d.check_start}
    deadline=time.monotonic()+40
    while time.monotonic()<deadline:
        d.command('ax900 check-status');d.pump(.5)
        if d.check.get('state',0)>=2:return d.check.copy()
    d.command('ax900 check-cancel')
    return {'kind':kind,'test_timeout':True}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host',required=True);p.add_argument('--port',type=int,default=5201)
    p.add_argument('--serial',default='/dev/cu.usbmodem1101');p.add_argument('--rounds',type=int,default=3)
    p.add_argument('--label',default='benchmark');p.add_argument('--dns-name');p.add_argument('--dns-server',default='')
    args=p.parse_args()
    if not args.label.replace('-','').replace('_','').isalnum() or not 1<=args.rounds<=10000:p.error('Invalid label/rounds')
    folder=Path(__file__).resolve().parents[1]/'logs';folder.mkdir(exist_ok=True)
    results=[];d=Device(args.serial,folder/(args.label+'.serial.log'))
    try:
        d.connected()
        for round_index in range(args.rounds):
            row={'round':round_index+1,'gateway':d.ping(),'checks':[]}
            for kind in (1,2,3,4,5):row['checks'].append(check(d,kind,args.host,args.port))
            if args.dns_name:row['checks'].append(check(d,0,args.dns_name,53,args.dns_server))
            results.append(row);(folder/(args.label+'.local.json')).write_text(json.dumps(results,indent=2))
            print(json.dumps(row),flush=True)
    finally:d.close()
if __name__=='__main__':main()
