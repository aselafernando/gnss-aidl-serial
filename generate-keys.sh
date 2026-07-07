#!/usr/bin/env bash
#
# Generate the signing material this APEX needs, into keys/.
#
#   keys/<name>.pem        RSA-4096 private key for the APEX image (AVB)
#   keys/<name>.avbpubkey  matching AVB public key (avbtool format)
#   keys/<name>.pk8        private key for the APEX container certificate
#   keys/<name>.x509.pem   matching X.509 certificate
#
# Run once after cloning, before `m`. The files are git-ignored (see
# .gitignore); they never leave your machine. Re-running is a no-op unless you
# pass --force, which regenerates from scratch (invalidates any APEX already
# signed with the old keys).
#
# Dependencies: openssl and python3 (both present in an AOSP checkout and on a
# typical Linux/macOS host). avbtool is used for the .avbpubkey if it happens
# to be on PATH; otherwise an equivalent pure-python generator is used, so no
# lunched build environment is required.

set -euo pipefail

name="com.android.hardware.gnss.serial"
keydir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/keys"
force=0
[[ "${1:-}" == "--force" ]] && force=1

py() {
    for p in python3 python; do
        if "$p" -c 'import sys; sys.exit(0)' >/dev/null 2>&1; then echo "$p"; return; fi
    done
    echo "ERROR: no working python3/python found" >&2; exit 1
}
PYTHON="$(py)"

pem="$keydir/$name.pem"
avbpubkey="$keydir/$name.avbpubkey"
pk8="$keydir/$name.pk8"
x509="$keydir/$name.x509.pem"

mkdir -p "$keydir"

if [[ $force -eq 0 && -f "$pem" && -f "$avbpubkey" && -f "$pk8" && -f "$x509" ]]; then
    echo "keys/ already populated — nothing to do (use --force to regenerate)."
    exit 0
fi

echo "==> AVB key pair (APEX image)"
openssl genrsa -out "$pem" 4096 2>/dev/null

if command -v avbtool >/dev/null 2>&1; then
    avbtool extract_public_key --key "$pem" --output "$avbpubkey"
    echo "    .avbpubkey via avbtool"
else
    # avbtool's AvbRSAPublicKeyHeader format: key_num_bits, n0inv (both BE
    # uint32), then modulus n and R^2 mod n (each key_bits/8 bytes, BE).
    "$PYTHON" - "$pem" "$avbpubkey" <<'PY'
import subprocess, struct, sys
key_pem, out = sys.argv[1], sys.argv[2]
mod = subprocess.check_output(["openssl","rsa","-in",key_pem,"-noout","-modulus"],text=True)
n = int(mod.strip().split("=",1)[1], 16)
txt = subprocess.check_output(["openssl","rsa","-in",key_pem,"-noout","-text"],text=True)
assert any("publicExponent" in l and "65537" in l for l in txt.splitlines()), "exponent must be 65537"
bits = n.bit_length()
assert bits % 32 == 0, bits
b = 2**32
n0inv = b - pow(n % b, -1, b)
rr = (2**bits)**2 % n
blob = struct.pack(">II", bits, n0inv) + n.to_bytes(bits//8,"big") + rr.to_bytes(bits//8,"big")
open(out,"wb").write(blob)
print(f"    .avbpubkey via python ({len(blob)} bytes, {bits}-bit)")
PY
fi

echo "==> Container certificate (APEX package signature)"
tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT
openssl genrsa -out "$tmp" 4096 2>/dev/null
# leading // keeps the subject from being mangled as a path on some platforms
openssl req -new -x509 -sha256 -key "$tmp" -out "$x509" -days 10950 \
    -subj "//CN=$name" 2>/dev/null
openssl pkcs8 -topk8 -inform PEM -outform DER -in "$tmp" -out "$pk8" -nocrypt 2>/dev/null

# Sanity: the container cert and its private key must share a modulus.
a="$(openssl pkcs8 -inform DER -nocrypt -in "$pk8" 2>/dev/null | openssl rsa -noout -modulus 2>/dev/null)"
b="$(openssl x509 -in "$x509" -noout -modulus 2>/dev/null)"
[[ "$a" == "$b" ]] || { echo "ERROR: pk8/x509 modulus mismatch" >&2; exit 1; }

echo
echo "Done. keys/ now contains:"
ls -1 "$keydir"
echo
echo "Next: add the APEX to your product and build —"
echo "  PRODUCT_PACKAGES += $name"
echo "  BOARD_VENDOR_SEPOLICY_DIRS += \$(this dir)/sepolicy"
echo "  m $name"
