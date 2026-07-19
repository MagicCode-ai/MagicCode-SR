#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
NATIVE_DIR=$(cd "$SCRIPT_DIR/../Native" && pwd)
REPORT_PATH="$SCRIPT_DIR/verification_report.md"

echo "[verify] building Unity Android native plugin..."
bash "$NATIVE_DIR/build_android.sh"

OUTPUT_SO="$NATIVE_DIR/build/libmagic_sr_unity.so"
if [ ! -f "$OUTPUT_SO" ]; then
  echo "[verify] missing output .so: $OUTPUT_SO"
  exit 1
fi

echo "[verify] exported symbol check..."
SYMBOLS=$(nm -gU "$OUTPUT_SO" | rg "MagicSR_(GetVersion|Create|Process|QueryStatus|Destroy)" || true)
if [ -z "$SYMBOLS" ]; then
  echo "[verify] expected exported symbols are missing."
  exit 1
fi

cat > "$REPORT_PATH" <<EOF
# Unity Android Native Verification Report

- Build output: \`$OUTPUT_SO\`
- Timestamp: \`$(date)\`

## Exported symbols
\`\`\`
$SYMBOLS
\`\`\`

## Result
- PASS: Native plugin was built and required API symbols were exported.
- NOTE: Engine runtime validation must be executed on Unity Android player/device.
EOF

echo "[verify] report generated: $REPORT_PATH"
