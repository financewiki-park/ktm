#!/bin/sh
set -eu
SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export TG_BRIDGE_PACKAGE_DIR="$SELF_DIR"
if [ "${1:-}" = "--ui" ]; then
    shift
    exec "$SELF_DIR/ui.sh" "$@"
fi
exec "$SELF_DIR/bin/ktm" "$@"
