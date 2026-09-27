#!/bin/sh
# Visible ktm interface, run inside the KPM-provided kTerm GTK window.
set -u

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
KTM="$SELF_DIR/bin/ktm"

pause() {
    printf '\nPress Enter to return to ktm. '
    IFS= read -r _ || true
}

while :; do
    clear 2>/dev/null || true
    printf '%s\n' 'ktm — Kindle Telegram Bridge'
    printf '%s\n\n' '1 Setup  2 Pair  3 Sync & Inbox  4 Send  5 Replace KTerm line  6 Status  7 Exit'
    printf 'Choose: '
    IFS= read -r choice || exit 0
    case "$choice" in
        1)
            printf '\nPaste the BotFather token, then press Enter. It is not echoed in logs.\n'
            "$KTM" setup
            pause
            ;;
        2)
            printf '\n'
            "$KTM" pair
            pause
            ;;
        3)
            printf '\nSyncing…\n'
            "$KTM" sync
            "$KTM" inbox
            pause
            ;;
        4)
            printf '\nMessage (one line): '
            IFS= read -r message || message=
            if [ -n "$message" ]; then
                printf '%s' "$message" | "$KTM" send
            else
                printf 'Nothing sent.\n'
            fi
            pause
            ;;
        5)
            printf '\nSyncing one pending Telegram message…\n'
            if "$KTM" sync; then
                if pending_path=$("$KTM" pending-path); then
                    request_path=${pending_path%/*}/paste-request
                    (umask 077; : > "$request_path")
                    printf '\nOpening the shell with the message on its editable line. Enter is not sent.\n'
                    exit 42
                fi
            fi
            pause
            ;;
        6)
            printf '\n'
            "$KTM" status
            pause
            ;;
        7|q|Q)
            exit 0
            ;;
        *)
            printf '\nChoose 1 through 7.\n'
            pause
            ;;
    esac
done
