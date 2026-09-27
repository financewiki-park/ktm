#!/bin/sh
# Run the menu first.  Choice 5 exits with 42 after it has written a one-shot
# native-paste request; then this same VTE child becomes the shell to receive it.
set -u

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$SELF_DIR/ui.sh"
result=$?

if [ "$result" -eq 42 ]; then
    exec /bin/sh
fi

exit "$result"
