#!/bin/sh
set -eu
BIN=${1:?binary required}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/ktm-test.XXXXXX")
trap 'rm -rf "$TMP"' EXIT INT TERM
export KTM_DATA_DIR="$TMP/data"
export KINDLE_PLATFORM=kindlepw2
"$BIN" diagnose | grep -q 'platform=kindlepw2'
"$BIN" status | grep -q 'pairing=none'
"$BIN" inbox | grep -q 'no messages'
"$BIN" reset-pairing | grep -q 'pairing reset'
echo 'host smoke test passed'
