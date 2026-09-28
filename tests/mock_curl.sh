#!/bin/sh
set -eu
CFG=$2
OUT=$(awk -F ' = ' '$1 == "output" {print $2}' "$CFG")
URL=$(awk -F ' = ' '$1 == "url" {print $2}' "$CFG")
case "$URL" in
  */getMe) printf '%s' '{"ok":true,"result":{"id":123,"is_bot":true,"first_name":"KTM Test","username":"ktm_test_bot"}}' > "$OUT"; printf '200'; exit 0 ;;
  */getWebhookInfo)
    if [ "${MOCK_WEBHOOK:-inactive}" = active ]; then
      printf '%s' '{"ok":true,"result":{"url":"https://example.invalid/hook","has_custom_certificate":false,"pending_update_count":3}}' > "$OUT"
    else
      printf '%s' '{"ok":true,"result":{"url":"","has_custom_certificate":false,"pending_update_count":0}}' > "$OUT"
    fi
    printf '200'; exit 0 ;;
esac
case "${MOCK_MODE:-getme}" in
  pair) printf '%s' "{\"ok\":true,\"result\":[{\"update_id\":100,\"message\":{\"message_id\":7,\"date\":1700000000,\"from\":{\"id\":42},\"chat\":{\"id\":42},\"text\":\"${MOCK_PAIR_TEXT:-/pair ${MOCK_PAIR_CODE}}\"}}]}" > "$OUT" ;;
  message) printf '%s' '{"ok":true,"result":[{"update_id":101,"message":{"message_id":8,"date":1700000001,"from":{"id":42},"chat":{"id":42},"text":"한글\nssh --help\n특수문자 ! $HOME"}}]}' > "$OUT" ;;
  empty) printf '%s' '{"ok":true,"result":[]}' > "$OUT" ;;
  conflict) printf '%s' '{"ok":false,"error_code":409,"description":"Conflict: webhook is active"}' > "$OUT"; printf '409'; exit 0 ;;
  send) printf '%s' '{"ok":true,"result":{"message_id":9}}' > "$OUT" ;;
  *) printf '%s' '{"ok":false,"error_code":500}' > "$OUT" ;;
esac
printf '200'
