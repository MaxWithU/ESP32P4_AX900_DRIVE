#!/usr/bin/env python3
"""Run local-only host regression with sanitizers; never opens hardware or uploads logs."""
import argparse, os, subprocess, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--official', type=Path, help='Patched M5Tab5-UserDemo checkout, for keyboard/UI tests')
p.add_argument('--mbedtls', type=Path, help='mbedTLS 3.6.x source, for TLS and WPA2 tests')
a = p.parse_args()
logs = ROOT / 'logs'
logs.mkdir(exist_ok=True)
def run(name, commands):
    with (logs / ('regression-' + name + '.log')).open('wb') as out:
        for command in commands:
            result = subprocess.run(command, cwd=ROOT, stdout=out, stderr=subprocess.STDOUT)
            if result.returncode:
                raise SystemExit(f'FAIL {name}; inspect local logs/regression-{name}.log')
    print('PASS ' + name, flush=True)
with tempfile.TemporaryDirectory(prefix='ax900-regression-') as folder:
    common = ['-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-Wno-misleading-indentation', '-fsanitize=address,undefined']
    def host(name, files=None, includes=None, cxx=False, defines=()):
        output = str(Path(folder) / name)
        command = ['c++' if cxx else 'cc', '-std=c++17' if cxx else '-std=c11', *common, *defines]
        for include in includes or ['tests/probe_host', 'components/ax900/include', 'components/ax900']:
            command += ['-I', str(include)]
        command += [str(f) for f in files or ['tests/' + name + '_test.c']]
        run(name, [command + ['-o', output], [output]])
    for name in ['frame', 'rx', 'networks', 'dns', 'channels']:
        host(name)
    host('pool', ['tests/pool_test.c', 'components/ax900/ax900_pool.c', 'components/ax900/ax900_metrics.c'])
    host('profile', ['tests/profile_test.c', 'components/ax900/ax900_profile.c'], ['tests/profile_host', 'tests/probe_host', 'components/ax900/include', 'components/ax900'])
    host('secure_profile', includes=['tests/secure_profile_host', 'tests/recovery_host', 'tests/profile_host', 'tests/probe_host', 'components/ax900/include', 'components/ax900'], defines=['-DESP_PLATFORM'])
    host('probe', ['tests/probe_test.c', 'components/ax900/ax900_probe.c'])
    host('ping', includes=['tests/ping_host', 'tests/recovery_host', 'tests/probe_host', 'components/ax900/include', 'components/ax900'])
    for per_interface in [0,1]:
        host('net_test' if per_interface else 'net_test_global_dns', files=['tests/net_test_test.c'], includes=['tests/net_test_host', 'tests/recovery_host', 'tests/probe_host', 'components/ax900/include', 'components/ax900'], defines=['-Wno-sign-compare', f'-DCONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF={per_interface}'])
    includes = ['tests/recovery_host', 'tests/profile_host', 'tests/probe_host', 'components/ax900/include', 'components/ax900']
    host('help', ['tests/help_test.c', 'components/ax900/ax900_help.c'], includes=includes)
    host('recovery', ['tests/recovery_test.c', 'components/ax900/ax900_metrics.c', 'components/ax900/ax900_help.c'], includes=includes)
    host('wifi_recovery', includes=includes)
    run('release_tools', [['python3', '-m', 'unittest', 'discover', '-s', 'tests', '-p', 'release_tools_test.py']])
    if a.official:
        keyboard = a.official.resolve() / 'platforms/tab5/main/hal/components'
        ui = a.official.resolve() / 'dependencies/smooth_ui_toolkit/src'
        host('keyboard', ['tests/keyboard_test.cpp'], [keyboard], cxx=True)
        host('keyboard_input', ['tests/keyboard_input_test.cpp', keyboard / 'tab5_keyboard.cpp'], ['tests/keyboard_host', 'tests/probe_host', keyboard], cxx=True)
        host('spring_initialization', ['tests/spring_initialization_test.cpp', ui / 'animation/generators/spring/spring.cpp'], [ui], cxx=True)
    else:
        print('SKIP official keyboard/animation tests; pass --official')
    if a.mbedtls:
        os.environ['MBEDTLS_SOURCE'] = str(a.mbedtls.resolve())
        os.environ.setdefault('MBEDTLS_HOST_BUILD', str(Path(folder) / 'mbedtls'))
        run('tls', [['sh', 'tests/run_tls_test.sh']])
        run('wpa2', ['python3 tests/run_wpa2_test.py'.split()])
    else:
        print('SKIP TLS/WPA2 cryptographic tests; pass --mbedtls')
print('Local regression complete; hardware/IDF builds remain separate')
