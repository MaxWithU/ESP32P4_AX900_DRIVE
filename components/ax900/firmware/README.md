# AX900 USB firmware profile

The four embedded binary files are a matching vendor USB D80 U02 set from the A40/Android7 directory of gtxaspec/aic8800-wifi. SOURCE.json pins the source commit, paths, sizes and SHA-256 values. The radio power table is from the accompanying aic_userconfig_8800d80.txt. Run `python3 tools/fetch_firmware.py` from the repository root to retrieve and verify these five vendor files; they are not committed to this repository.

This profile was tested on AX900 with chip register 0xe1078820. It loads the BT patch at 0x20b43c and FMAC at 0x120000. The latter reports version 0x06090100, supports_5ghz=1, and successfully scans both bands. A newer CanMV profile uses a BT patch at 0x1e0000, which did not pass RAM write/read checks on the tested adapter. Do not mix that profile, SDIO blobs, or a newer patch table with this set.

Firmware blobs are proprietary vendor binaries, separate from the Apache-2.0 host protocol adaptation. The host driver's license does not grant additional redistribution rights for these binaries.
