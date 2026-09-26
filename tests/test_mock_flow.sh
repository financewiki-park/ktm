#!/bin/sh
set -eu
BIN=${1:?binary required}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/ktm-flow.XXXXXX")
trap 'rm -rf "$TMP"' EXIT INT TERM
export KTM_DATA_DIR="$TMP/data"
export TG_CURL="$ROOT/tests/mock_curl.sh"

printf 'not-a-real-token\n' | "$BIN" setup >/dev/null
code=$($BIN pair | awk '/Pairing code/{print $NF}')
MOCK_MODE=pair MOCK_PAIR_CODE="$code" "$BIN" sync >/dev/null
MOCK_MODE=message "$BIN" sync >/dev/null
"$BIN" last > "$TMP/last.txt"
test "$(wc -c < "$TMP/last.txt")" -eq 38
test "$(tr '\n' '|' < "$TMP/last.txt")" = '한글|ssh --help|특수문자 ! $HOME'
"$BIN" copy >/dev/null
test "$(wc -c < "$TMP/data/inbox/latest.txt")" -eq 38
printf 'mock pairing and UTF-8 flow passed\n'
