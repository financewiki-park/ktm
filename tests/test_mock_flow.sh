#!/bin/sh
set -eu
BIN=${1:?binary required}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/ktm-flow.XXXXXX")
trap 'rm -rf "$TMP"' EXIT INT TERM
export KTM_DATA_DIR="$TMP/data"
export TG_CURL="$ROOT/tests/mock_curl.sh"

printf 'not-a-real-token\n' | "$BIN" setup >/dev/null
"$BIN" bot-info | grep -q 'Configured bot: @ktm_test_bot (id 123)'
code=$($BIN pair | awk '/Pairing code/{print $NF}')
MOCK_MODE=empty "$BIN" sync | grep -q 'Pairing still waiting'
MOCK_MODE=pair MOCK_PAIR_CODE="$code" "$BIN" sync >/dev/null
MOCK_MODE=message "$BIN" sync >/dev/null
"$BIN" last > "$TMP/last.txt"
test "$(wc -c < "$TMP/last.txt")" -eq 38
test "$(tr '\n' '|' < "$TMP/last.txt")" = '한글|ssh --help|특수문자 ! $HOME'
"$BIN" copy >/dev/null
test "$(wc -c < "$TMP/data/inbox/latest.txt")" -eq 38
printf 'queued for old bot\n' > "$TMP/data/outbox/pending.jsonl"
printf 'new-token\n' | "$BIN" setup > "$TMP/token-change.out"
grep -q 'Old pairing, inbox and update state were reset' "$TMP/token-change.out"
"$BIN" status | grep -q 'pairing=none'
test ! -e "$TMP/data/inbox/current.txt"
test ! -e "$TMP/data/inbox/latest.txt"
test "$(cat "$TMP/data/state/update_offset")" = 0
test ! -e "$TMP/data/outbox/pending.jsonl"
test -n "$(find "$TMP/data/state" -name 'outbox-before-token-*.jsonl' -print -quit)"
new_code=$($BIN pair | awk '/Pairing code/{print $NF}')
MOCK_MODE=pair MOCK_PAIR_CODE="$new_code" "$BIN" sync >/dev/null
printf 'new-token\n' | "$BIN" setup > "$TMP/same-token.out"
grep -q 'Existing pairing was kept' "$TMP/same-token.out"
"$BIN" status | grep -q 'pairing=paired'
printf 'mock pairing and UTF-8 flow passed\n'
