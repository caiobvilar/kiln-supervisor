#!/usr/bin/env python3
"""sign_image.py -- sign a firmware image with Ed25519.

Portfolio posture (03-toolchain.md §4.4): the private key lives in GitHub
Secrets, the *public* key is committed to the repo and verified by the
bootloader at run time. A production system would use an HSM; that sentence
belongs in the writeup.

Usage:
  python tools/sign_image.py firmware.bin --key <secret-file> -o firmware.signed.bin
  python tools/sign_image.py verify firmware.signed.bin --pubkey <pub-key-file>
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import nacl.signing  # PyNaCl
except ImportError:
    print("error: PyNaCl required: pip install pynacl", file=sys.stderr)
    sys.exit(2)

MAGIC = b"PORTFOLIO-SIG-V1\x00"
SIG_LEN = 64


def build_signed(image: bytes, sig: bytes) -> bytes:
    return MAGIC + sig + image


def parse_signed(data: bytes) -> tuple[bytes, bytes]:
    if not data.startswith(MAGIC):
        raise ValueError("not a signed image (bad magic)")
    return data[SIG_LEN + len(MAGIC):], data[len(MAGIC):len(MAGIC) + SIG_LEN]


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("sign")
    p.add_argument("image")
    p.add_argument("--key", required=True, help="path to Ed25519 private key seed")
    p.add_argument("-o", "--out", required=True)

    p = sub.add_parser("verify")
    p.add_argument("image")
    p.add_argument("--pubkey", required=True, help="path to Ed25519 public key")
    args = ap.parse_args()

    if args.cmd == "sign":
        key = nacl.signing.SigningKey(Path(args.key).read_bytes())
        image = Path(args.image).read_bytes()
        sig = key.sign(image).signature
        Path(args.out).write_bytes(build_signed(image, sig))
        print(f"signed {args.image} -> {args.out} ({len(image)} bytes image)")
        return 0

    # verify
    pub = nacl.signing.VerifyKey(Path(args.pubkey).read_bytes())
    data = Path(args.image).read_bytes()
    image, sig = parse_signed(data)
    try:
        pub.verify(sig + image)
    except Exception as e:
        print(f"VERIFY FAILED: {e}")
        return 1
    print(f"OK: signature valid ({len(image)} bytes image)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
