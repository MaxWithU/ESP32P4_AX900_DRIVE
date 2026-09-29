#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
repo=Path(__file__).resolve().parents[1]
root=repo/'components/ax900';source=Path(os.environ['MBEDTLS_SOURCE']);build=Path(os.environ['MBEDTLS_HOST_BUILD'])
files=['rsn_supp/wpa.c','rsn_supp/wpa_ie.c','rsn_supp/pmksa_cache.c','common/wpa_common.c','common/ieee802_11_common.c',
'utils/common.c','utils/wpabuf.c','utils/wpa_debug.c','crypto/sha1-prf.c','crypto/sha1-pbkdf2.c','crypto/sha256-prf.c','crypto/sha256-kdf.c','crypto/aes-wrap.c','crypto/aes-unwrap.c','crypto/aes-omac1.c','crypto/rc4.c']
cmd=['cc','-g','-Wall','-Wextra','-Wno-unused-parameter','-Wno-misleading-indentation','-fsanitize=address,undefined',
'-DCONFIG_NO_RANDOM_POOL','-DCONFIG_NO_STDOUT_DEBUG','-DCONFIG_NO_WPA_MSG','-DCONFIG_NO_TKIP','-DCONFIG_NO_SOCKLEN_T_TYPEDEF']
for include in [repo/'tests/host',repo/'tests/probe_host',root/'supplicant/src',root/'supplicant/src/utils',source/'include']:cmd+=['-I',str(include)]
cmd += [str(repo/'tests/wpa2_handshake_test.c'),str(root/'ax900_supplicant_port.c')]+[str(root/'supplicant/src'/f) for f in files]+[str(build/'library/libmbedcrypto.a'),'-o','/tmp/ax900-wpa2-test']
subprocess.run(cmd,check=True);subprocess.run(['/tmp/ax900-wpa2-test'],check=True)
