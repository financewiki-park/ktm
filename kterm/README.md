# KTerm companion boundary

현재 설치된 KTerm binary를 patch하거나 덮어쓰지 않는다. 실기기에서 다음을 먼저 확인한다.

```sh
kpm --version
command -v kterm
file "$(command -v kterm)"
strings "$(command -v kterm)" | grep -E 'VTE|gtk|Paste|clipboard' || true
```

KTerm 소스와 VTE ABI가 확보되면 companion/fork에서 `Paste Telegram` 메뉴를 추가하고, 선택 시 다음 파일을 읽어 terminal child input에 feed한다.

```text
<ktm-data-dir>/inbox/latest.txt
```

입력 전달은 VTE의 버전에 맞는 input-feed API를 사용한다. 구현 계약은 다음과 같다.

1. 파일 바이트를 그대로 읽는다. UTF-8과 줄바꿈을 변경하지 않는다.
2. system clipboard가 아니라 terminal input stream에만 넣을 수 있다.
3. Enter, carriage return 삽입, shell 실행, `sh -c`, command substitution을 하지 않는다.
4. 파일을 읽은 뒤에도 KTerm의 사용자가 직접 확인하고 Enter를 눌러야 한다.

KTerm 소스/ABI 없이 이 저장소가 제공하는 것은 안전한 `latest.txt` producer와 경계 문서까지다. 임의 binary patch는 하지 않는다.
