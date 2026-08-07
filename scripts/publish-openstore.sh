#!/usr/bin/env bash
# Upload arm64 + armhf .click revisions to OpenStore and mark the app published.
#
# OpenStore's nginx occasionally returns 502/503 on large multipart uploads
# (seen with boot-asset Click packages ~120 MB). Retry transient failures with
# backoff; treat most 4xx as permanent.
set -euo pipefail

CLICK_NAME="${CLICK_NAME:-supertuxkarttouch.dixonsolutions}"
VERSION="${VERSION:?VERSION is required}"
OPENSTORE_API_KEY="${OPENSTORE_API_KEY:?OPENSTORE_API_KEY is required}"
CHANNEL="${OPENSTORE_CHANNEL:-focal}"
MAX_ATTEMPTS="${OPENSTORE_MAX_ATTEMPTS:-6}"
REVISION_CHANGELOG="${REVISION_CHANGELOG:-SuperTuxKart Touch ${VERSION}: glass HUD, thermal profile, packaging.}"

CLICKS=("$@")
if [ "${#CLICKS[@]}" -lt 1 ]; then
  echo "Usage: $0 <file.click> [more.click...]" >&2
  exit 1
fi

is_success_json() {
  local path="$1"
  python3 - "$path" <<'PY'
import json, sys
path = sys.argv[1]
try:
    data = json.load(open(path, encoding="utf-8"))
except Exception as exc:
    print(f"non-JSON response ({exc})", file=sys.stderr)
    sys.exit(1)
if data.get("success"):
    sys.exit(0)
print(data, file=sys.stderr)
sys.exit(1)
PY
}

upload_revision() {
  local file="$1"
  local attempt=1
  local http_code delay

  while [ "$attempt" -le "$MAX_ATTEMPTS" ]; do
    echo "OpenStore revision upload attempt ${attempt}/${MAX_ATTEMPTS}: $(basename "$file")"
    http_code="$(
      curl -sS -o /tmp/openstore-resp.json -w '%{http_code}' \
        --connect-timeout 30 --max-time 1200 \
        -X POST \
        -F "file=@${file}" \
        -F "channel=${CHANNEL}" \
        -F "changelog=${REVISION_CHANGELOG}" \
        "https://open-store.io/api/v3/manage/${CLICK_NAME}/revision?apikey=${OPENSTORE_API_KEY}"
    )"
    echo "HTTP ${http_code}"
    head -c 800 /tmp/openstore-resp.json 2>/dev/null || true
    echo

    if [ "$http_code" = "200" ] && is_success_json /tmp/openstore-resp.json; then
      echo "Uploaded $(basename "$file")"
      return 0
    fi

    # Permanent client errors (except request timeout / rate limit).
    if [ "$http_code" -ge 400 ] && [ "$http_code" -lt 500 ] \
      && [ "$http_code" != "408" ] && [ "$http_code" != "429" ]; then
      echo "OpenStore rejected $(basename "$file") with HTTP ${http_code}" >&2
      return 1
    fi

    delay=$((15 * attempt))
    echo "Transient OpenStore failure (HTTP ${http_code}); retrying in ${delay}s..."
    sleep "$delay"
    attempt=$((attempt + 1))
  done

  echo "OpenStore upload failed for $(basename "$file") after ${MAX_ATTEMPTS} attempts" >&2
  return 1
}

for f in "${CLICKS[@]}"; do
  [ -f "$f" ] || { echo "Missing click: $f" >&2; exit 1; }
  upload_revision "$f"
  # Brief pause so a large arm64 upload is less likely to 502 the next arch.
  sleep 5
done

echo "Marking ${CLICK_NAME} published..."
http_code="$(
  curl -sS -o /tmp/openstore-pub.json -w '%{http_code}' \
    --connect-timeout 30 --max-time 120 \
    -X PUT \
    -F "published=true" \
    -F "changelog=${REVISION_CHANGELOG}" \
    "https://open-store.io/api/v3/manage/${CLICK_NAME}?apikey=${OPENSTORE_API_KEY}"
)"
echo "publish HTTP ${http_code}"
cat /tmp/openstore-pub.json || true
echo
python3 - <<'PY'
import json, sys
d = json.load(open("/tmp/openstore-pub.json", encoding="utf-8"))
sys.exit(0 if d.get("success") and d.get("data", {}).get("published") else 1)
PY
echo "OpenStore published: https://open-store.io/app/${CLICK_NAME}"
