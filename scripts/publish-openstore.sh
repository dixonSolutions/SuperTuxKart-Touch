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

# Exit 0 = success (or already present), 1 = permanent reject, 2 = transient/retryable.
classify_revision_response() {
  local http_code="$1" path="$2"
  python3 - "$http_code" "$path" <<'PY'
import json, sys
code, path = sys.argv[1], sys.argv[2]
body = open(path, encoding="utf-8", errors="replace").read()
try:
    data = json.loads(body)
except Exception:
    # nginx 502 HTML etc.
    sys.exit(2 if code.startswith("5") or code in {"408", "429"} else 1)

if data.get("success"):
    sys.exit(0)

message = str(data.get("message") or "").lower()
# Idempotent republish: arm64 often lands before armhf fails mid-job.
if "already exists" in message and "revision" in message:
    print("Revision already on OpenStore — treating as success", file=sys.stderr)
    sys.exit(0)

if code in {"408", "429"} or code.startswith("5"):
    sys.exit(2)
if code.startswith("4"):
    print(data, file=sys.stderr)
    sys.exit(1)
sys.exit(2)
PY
}

upload_revision() {
  local file="$1"
  local attempt=1
  local http_code delay rc

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

    rc=0
    classify_revision_response "$http_code" /tmp/openstore-resp.json || rc=$?
    case "$rc" in
      0)
        echo "OK: $(basename "$file")"
        return 0
        ;;
      1)
        echo "OpenStore rejected $(basename "$file") with HTTP ${http_code}" >&2
        return 1
        ;;
    esac

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
