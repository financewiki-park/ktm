#!/bin/sh
set -eu
SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -w /var/local ]; then
    export KTM_DATA_DIR=/var/local/ktm
else
    export KTM_DATA_DIR=/mnt/us/ktm
fi
export DISPLAY="${DISPLAY:-:0}"
export TERM=xterm TERMINFO=/mnt/us/kterm/vte/terminfo

set -- -e "$SELF_DIR/session.sh" -P "$KTM_DATA_DIR/inbox/current.txt" -R "$KTM_DATA_DIR/inbox/paste-request"
DPI=$(sed -n 's/.*(\([0-9][0-9]*\), [0-9][0-9]*).*/\1/p' /var/log/Xorg.0.log | head -n 1)
if [ "${DPI:-0}" -gt 290 ] 2>/dev/null; then
    set -- -l /mnt/us/kterm/layouts/keyboard-300dpi.xml "$@"
elif [ "${DPI:-0}" -gt 200 ] 2>/dev/null; then
    set -- -l /mnt/us/kterm/layouts/keyboard-200dpi.xml "$@"
fi
exec "$SELF_DIR/bin/ktmterm" "$@"
