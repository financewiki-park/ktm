#!/bin/sh
set -eu
SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -n "${KTM_DATA_DIR:-}" ]; then
    export KTM_DATA_DIR
elif [ -w /var/local ]; then
    export KTM_DATA_DIR=/var/local/ktm
else
    export KTM_DATA_DIR=/mnt/us/ktm
fi
case "${1:-}" in
    --ui) exec /bin/sh "$SELF_DIR/session.sh" ;;
    "") ;;
    *) exec "$SELF_DIR/bin/ktm" "$@" ;;
esac
umask 077
mkdir -p "$KTM_DATA_DIR/logs"
KPM=${KTM_KPM:-/var/local/kmc/bin/kpm}
# KTerm 2.6 splits -e on spaces using strtok, without shell quote parsing.
# Pass only the fixed, space-free KPM command; --ui resolves package paths.
exec "$KPM" launch kterm -e "$KPM launch ktm --ui" \
    2>"$KTM_DATA_DIR/logs/startup.log"
