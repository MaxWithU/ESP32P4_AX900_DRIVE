#!/usr/bin/env python3
"""Regenerate the UserDemo integration patch and verify it against the pinned baseline."""
import argparse
import difflib
import io
from pathlib import Path
import subprocess
import tarfile
import tempfile

ROOT=Path(__file__).resolve().parents[1]
BASE='b4e356bc491ca070d54004718dad789c07d5fc93'
TRACKED=['app/apps/app_launcher/view/view.cpp','app/apps/app_launcher/view/view.h',
         'platforms/tab5/CMakeLists.txt','platforms/tab5/main/CMakeLists.txt',
         'platforms/tab5/main/app_main.cpp','platforms/tab5/main/hal/hal_esp32.cpp']
NEW=['app/apps/app_launcher/view/panel_ax900.cpp','app/apps/app_launcher/view/ax900_test_view.h',
     'app/apps/app_launcher/view/ax900_test_view.cpp','platforms/tab5/main/ax900_console.cpp',
     'platforms/tab5/main/hal/components/tab5_keyboard.h',
     'platforms/tab5/main/hal/components/tab5_keyboard_map.h',
     'platforms/tab5/main/hal/components/tab5_keyboard.cpp']
CONFIG=['CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF','CONFIG_MBEDTLS_HAVE_TIME','CONFIG_MBEDTLS_HAVE_TIME_DATE']

def verify(repo, patch):
    with tempfile.TemporaryDirectory(prefix='ax900-patch-') as tmp:
        archive=subprocess.check_output(['git','-C',str(repo),'archive',BASE])
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            tar.extractall(tmp,filter='data')
        subprocess.run(['git','apply','--check',str(patch)],cwd=tmp,check=True,capture_output=True)
        subprocess.run(['git','apply',str(patch)],cwd=tmp,check=True,capture_output=True)
        for name in TRACKED+NEW:
            if (Path(tmp)/name).read_bytes() != (repo/name).read_bytes():
                raise ValueError('Integration patch differs from source: '+name)
        for name in ['platforms/tab5/sdkconfig','platforms/tab5/sdkconfig.defaults']:
            config=(Path(tmp)/name).read_text().splitlines()
            if not all(key+'=y' in config for key in CONFIG):
                raise ValueError('Required configuration absent from patch: '+name)

def generate(repo, out):
    patch=subprocess.check_output(['git','-C',str(repo),'diff',BASE,'--',*TRACKED],text=True)
    for name in NEW:
        patch+='diff --git a/'+name+' b/'+name+'\nnew file mode 100644\n'
        patch+=''.join(difflib.unified_diff([], (repo/name).read_text().splitlines(True),fromfile='/dev/null',tofile='b/'+name))
    for name in ['platforms/tab5/sdkconfig.defaults','platforms/tab5/sdkconfig']:
        original=subprocess.check_output(['git','-C',str(repo),'show',BASE+':'+name],text=True)
        if name.endswith('.defaults'):
            expected=original+'CONFIG_LV_USE_SNAPSHOT=y\nCONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM=y\n'+''.join(k+'=y\n' for k in CONFIG)
        else:
            expected=original
            for key in CONFIG:
                expected=expected.replace('# '+key+' is not set',key+'=y')
        patch+='diff --git a/'+name+' b/'+name+'\n'+''.join(difflib.unified_diff(original.splitlines(True),expected.splitlines(True),fromfile='a/'+name,tofile='b/'+name))
    out.write_text(patch.replace('\n \n','\n\n'))
    verify(repo,out)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--official',type=Path,required=True)
    p.add_argument('--check',action='store_true');a=p.parse_args()
    patch=ROOT/'patches/m5tab5-userdemo-ax900.patch'
    if a.check:verify(a.official.resolve(),patch)
    else:generate(a.official.resolve(),patch)
    print('Pinned official patch verified; 13 source/CMake files match.')
