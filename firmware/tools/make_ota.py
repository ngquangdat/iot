#!/usr/bin/env python3
"""Build a signed Nordic Secure DFU package (application only) for OTA updates.

usage: make_ota.py app.hex key.pem app_version out.zip
Requires nrfutil 6.x (pip install --ignore-requires-python nrfutil==6.1.7).
"""
import sys
from nordicsemi.dfu.package import Package
from nordicsemi.dfu.signing import Signing

S112_7_3_0 = 0x126  # SoftDevice firmware ID the device's bootloader expects

hex_file, key_file, version, out = sys.argv[1], sys.argv[2], int(sys.argv[3], 0), sys.argv[4]
signer = Signing()
signer.load_key(key_file)
pkg = Package(
    debug_mode=False,
    hw_version=52,
    app_version=version,
    sd_req=[S112_7_3_0],
    sd_id=[S112_7_3_0],
    app_fw=hex_file,
    signer=signer,
)
pkg.generate_package(out)
print(f"wrote {out} (app version {version})")
