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
    printf '%s\n' 'ktm 0.1.15 — Kindle Telegram Bridge'
    printf '%s\n' '1 Setup  2 Pair  3 Sync & Inbox  4 Send'
    printf '%s\n\n' '5 Run received command  6 Status  7 Exit  8 Edit one-line command'
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
        5|8)
            printf '\nSyncing one pending Telegram message…\n'
            if ! "$KTM" sync; then
                printf '\nSync failed. Checking the message already saved on this Kindle.\n'
            fi
            if [ "$choice" = 5 ]; then
                "$KTM" run
            elif pending_path=$("$KTM" pending-path); then
                if "$SELF_DIR/bin/ktm-input" --check "$pending_path"; then
                    exit 42
                fi
            fi
            pause
            ;;
       6)
           printf '\n'
            "$KTM" bot-info || true
           "$KTM" status
            pause
            ;;
        7|q|Q)
            exit 0
            ;;
        *)
            printf '\nChoose 1 through 8.\n'
            pause
            ;;
    esac
done
