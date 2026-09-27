#!/bin/sh
set -eu
CFG=$2
OUT=$(awk -F ' = ' '$1 == "output" {print $2}' "$CFG")
case "${MOCK_MODE:-getme}" in
  getme) printf '%s' '{"ok":true,"result":{"id":123,"is_bot":true,"first_name":"KTM Test","username":"ktm_test_bot"}}' > "$OUT" ;;
  pair) printf '%s' "{\"ok\":true,\"result\":[{\"update_id\":100,\"message\":{\"message_id\":7,\"date\":1700000000,\"from\":{\"id\":42},\"chat\":{\"id\":42},\"text\":\"/pair ${MOCK_PAIR_CODE}\"}}]}" > "$OUT" ;;
  message) printf '%s' '{"ok":true,"result":[{"update_id":101,"message":{"message_id":8,"date":1700000001,"from":{"id":42},"chat":{"id":42},"text":"한글\nssh --help\n특수문자 ! $HOME"}}]}' > "$OUT" ;;
  empty) printf '%s' '{"ok":true,"result":[]}' > "$OUT" ;;
  send) printf '%s' '{"ok":true,"result":{"message_id":9}}' > "$OUT" ;;
  *) printf '%s' '{"ok":false,"error_code":500}' > "$OUT" ;;
esac
printf '200'
