#!/bin/sh
# The official KTerm stays open for the menu and the input relay.
set -u

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
while :; do
    /bin/sh "$SELF_DIR/ui.sh"
    result=$?
    if [ "$result" -ne 42 ]; then exit "$result"; fi
    if pending_path=$("$SELF_DIR/bin/ktm" pending-path); then
        printf '\nStarting shell. Check the text before pressing Enter.\nType exit to return to ktm.\n'
        "$SELF_DIR/bin/ktm-input" "$pending_path"
        result=$?
        if [ "$result" -ne 0 ]; then
            printf '\nInput could not be opened (code %s). Message retained.\nPress Enter to return. ' "$result"
            IFS= read -r _ || exit 0
        fi
    fi
done
