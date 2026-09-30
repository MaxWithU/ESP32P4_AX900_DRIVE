#!/usr/bin/env python3
"""Build an allowlisted local release archive. Never includes logs or device backups."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile
from tab5_flash import image_info, validate_table, load_bundle, FLASH_SIZE, APP_OFFSET, APP_SIZE
from update_official_patch import verify

ROOT = Path(__file__).resolve().parents[1]
BASELINE = 'b4e356bc491ca070d54004718dad789c07d5fc93'
def sha(data):
    return hashlib.sha256(data).hexdigest()

def package(official, destination):
    version = (ROOT/'release/VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+-rc\.\d+', version):
        raise ValueError('Only an explicitly versioned release candidate is supported.')
    if subprocess.check_output(['git','status','--porcelain'],cwd=ROOT).strip():
        raise ValueError('Commit source/test/doc changes before packaging.')
    source = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    if subprocess.check_output(['git','rev-parse','HEAD'],cwd=official,text=True).strip() != BASELINE:
        raise ValueError('Official checkout must use the pinned baseline.')
    build=official/'platforms/tab5/build'
    verify(official,ROOT/'patches/m5tab5-userdemo-ax900.patch')
    pending=subprocess.run(['ninja','-C',str(build),'-n','gen_project_binary'],capture_output=True,text=True,check=True)
    if 'ninja: no work to do.' not in pending.stdout:
        raise ValueError('Firmware build is stale; rebuild before packaging.')
    description=json.loads((build/'project_description.json').read_text())
    idf=Path(description['idf_path'])
    config=json.loads((build/'config/sdkconfig.json').read_text())
    required=['ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF','MBEDTLS_HAVE_TIME','MBEDTLS_HAVE_TIME_DATE']
    if not all(config.get(k) for k in required) or config.get('AX900_FAULT_INJECTION'):
        raise ValueError('Build configuration does not meet release requirements.')
    app=(build/'m5stack_tab5.bin').read_bytes()
    table=(build/'partition_table/partition-table.bin').read_bytes()
    image_info(app);validate_table(table)
    # Use the IDF app descriptor to reject old/foreign build output.
    if image_info(app)['version'] != version:
        raise ValueError('Application version does not match release/VERSION; rebuild with -DPROJECT_VER='+version)
    destination.mkdir(parents=True,exist_ok=True)
    output=destination/('tab5-ax900-'+version+'.zip')
    if output.exists():
        raise ValueError('Release archive already exists; choose a new output directory.')
    with tempfile.TemporaryDirectory(prefix='ax900-package-') as temporary:
        folder=Path(temporary)/('tab5-ax900-'+version);folder.mkdir()
        (folder/'application.bin').write_bytes(app)
        (folder/'partition-table.bin').write_bytes(table)
        files={
            'tab5_flash.py':ROOT/'tools/tab5_flash.py',
            'requirements.txt':ROOT/'release/requirements.txt',
            'QUICKSTART.md':ROOT/'release/QUICKSTART.md',
            'README.md':ROOT/'README.md', 'VALIDATION.json':ROOT/'VALIDATION.json',
            'licenses/DRIVER-LICENSE':ROOT/'LICENSE', 'licenses/DRIVER-NOTICE':ROOT/'NOTICE',
            'licenses/HOSTAP-LICENSE':ROOT/'components/ax900/supplicant/COPYING',
            'licenses/USERDEMO-LICENSE':official/'LICENSE',
            'licenses/AX900-VENDOR-NOTICE':ROOT/'components/ax900/firmware/NOTICE',
            'licenses/AX900-VENDOR-README.md':ROOT/'components/ax900/firmware/README.md',
            'licenses/ESP-IDF-LICENSE':idf/'LICENSE',
            'sources/ax900-firmware.json':ROOT/'components/ax900/firmware/SOURCE.json',
            'sources/official-dependencies.lock':official/'platforms/tab5/dependencies.lock',
            'sources/official-repos.json':official/'repos.json',
        }
        for name,path in files.items():
            target=folder/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
        # Include dependency license notices, never dependency runtime data or keys.
        for parent in [official/'dependencies',official/'platforms/tab5/components',official/'platforms/tab5/managed_components',idf/'components']:
            if parent.exists():
                for path in sorted(parent.rglob('*')):
                    if path.is_file() and path.name.lower() in ('license','license.txt','license.md','copying','notice','notice.txt') and '.git' not in path.parts:
                        group='esp-idf' if parent==idf/'components' else 'official-'+parent.name
                        target=folder/'licenses/third-party'/group/path.relative_to(parent)
                        target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
        (folder/'.gitignore').write_text('/backups/\n/logs/\n/.venv/\n')
        manifest={'schema':1,'version':version,'source_commit':source,'official_baseline':BASELINE,
                  'board':'tab5-v3','chip':'esp32p4','chip_revision':103,'flash_bytes':FLASH_SIZE,
                  'app_offset':APP_OFFSET,'app_partition_bytes':APP_SIZE,'esp_idf':'5.5.2',
                  'hardware_validation':'This revision has not been flashed or validated on hardware.',
                  'files':{p.relative_to(folder).as_posix():{'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())}
                           for p in sorted(folder.rglob('*')) if p.is_file()}}
        (folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        (folder/'SHA256SUMS').write_text(''.join(sha(p.read_bytes())+'  '+p.relative_to(folder).as_posix()+'\n'
                                               for p in sorted(folder.rglob('*')) if p.is_file()))
        load_bundle(folder)
        with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(folder.rglob('*')):
                if path.is_file():archive.write(path,path.relative_to(folder.parent))
    checksum=sha(output.read_bytes())
    output.with_suffix('.zip.sha256').write_text(checksum+'  '+output.name+'\n')
    print('Packaged '+output.name+'; SHA-256 '+checksum)
    return output

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--official',type=Path,required=True)
    p.add_argument('--output',type=Path,default=ROOT/'dist')
    a=p.parse_args()
    package(a.official.resolve(),a.output.resolve())
