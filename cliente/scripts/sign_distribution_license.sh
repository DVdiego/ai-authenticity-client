#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LICENSE_PATH="${1:-$ROOT_DIR/distribution/license.json}"
SIGNATURE_PATH="${2:-$ROOT_DIR/distribution/license.sig}"
KEY="ai_authenticity_client_tfg_license_v1"

if [[ ! -f "$LICENSE_PATH" ]]; then
  echo "Missing license file: $LICENSE_PATH" >&2
  exit 1
fi

python3 - "$LICENSE_PATH" "$SIGNATURE_PATH" "$KEY" <<'PY'
import hashlib
import hmac
import pathlib
import sys

license_path = pathlib.Path(sys.argv[1])
signature_path = pathlib.Path(sys.argv[2])
key = sys.argv[3].encode("utf-8")
payload = license_path.read_bytes().strip()
signature = hmac.new(key, payload, hashlib.sha256).hexdigest()
signature_path.write_text(signature + "\n", encoding="utf-8")
print(f"Updated signature: {signature_path}")
PY
