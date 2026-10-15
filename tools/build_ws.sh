#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v idf.py >/dev/null 2>&1; then
    echo 'Activate ESP-IDF v6.0.1 first: source <esp-idf>/export.sh.' >&2
    exit 1
fi
idf.py -B build-waveshare -D SDKCONFIG="$PWD/sdkconfig.waveshare" \
    -D PROJECT_VER=0.1.0-ws.1 \
    -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.waveshare' build
