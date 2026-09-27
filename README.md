# ktm

Kindle KPM용 1인 Telegram Bot 브리지. 기기에 KUAL, Python, Bash, jq는 필요하지 않습니다.
현재 지원 플랫폼은 **kindlehf**입니다.

## 복구 버전 0.1.11

0.1.7–0.1.10의 자체 GTK/VTE 실행 파일은 기기 구동이 확인되지 않았고,
사용자에게 5번 선택 시 종료 및 0.1.10 앱 시작 실패가 발생했습니다.
0.1.11은 자체 GUI를 제거하고 설치된 공식 KPM `kterm`을 다시 사용합니다.
빌드·자동 테스트 통과는 실제 Kindle 화면 동작 검증과 다릅니다.
검토 내용은 [구조 및 검증 기록](docs/0.1.11-review.md)에 있습니다.

## 설치 및 실행

이미 저장소를 등록했다면 Kindle의 **기존 KTerm**에서:

```sh
/var/local/kmc/bin/kpm install ktm
```

최초 등록만 필요한 경우:

```sh
/var/local/kmc/bin/kpm add-repo https://raw.githubusercontent.com/financewiki-park/ktm/main/manifest.json
/var/local/kmc/bin/kpm install ktm
```

설치 후 라이브러리에서 `ktm`을 엽니다. 메뉴 상단은 `ktm 0.1.11`입니다.
업데이트는 저장된 봇 토큰, 페어링, 메시지를 보존합니다.
KPM이 공식 `kterm` 패키지를 의존성으로 관리합니다. ktm은 그 바이너리와 키보드를 덮어쓰지 않습니다.

## 메뉴

1. Setup: BotFather가 준 전체 토큰을 입력합니다.
2. Pair: 표시된 `/pair 코드`를 자신의 Telegram 앱에서 만든 봇에게 보냅니다.
3. Sync & Inbox: 메시지를 받아 표시합니다.
4. Send: 해당 Telegram 채팅으로 답장합니다.
5. Replace KTerm line: 수신한 한 줄을 같은 KTerm 창의 새 셸 입력 줄에 놓습니다.
6. Status: 버전·페어링·동기화 상태를 표시합니다.
7. Exit: 앱을 닫습니다.

5번은 기존에 따로 열어 둔 KTerm의 명령줄을 바꾸는 기능이 아닙니다.
ktm이 열린 공식 KTerm 창 안에 입력용 셸을 열며, 실제 프롬프트를 확인한 뒤 텍스트만 전달합니다.
사용자가 Enter를 누르기 전에는 실행하지 않습니다.
Ctrl+U로 입력을 지우거나 Ctrl+C로 취소할 수 있습니다.
셸에서 `exit`를 입력하면 ktm 메뉴로 돌아갑니다.

직접 입력은 현재 **512바이트 이하의 한 줄**만 지원합니다.
줄바꿈·탭·제어문자 또는 더 긴 메시지는 메뉴에 이유를 표시하고 입력하지 않습니다.
원문은 Inbox에 그대로 남습니다. 여러 줄을 그대로 셸에 보내면 마지막 Enter가 없어도
중간 줄이 실행될 수 있으므로, 여러 줄 붙여넣기는 아직 지원하지 않습니다.
수신 원문은 한글과 줄바꿈을 보존하지만, 한글 키보드 및 글꼴의 실기기 동작은 별도 검증이 필요합니다.

## 저장과 오류 기록

데이터는 `/var/local/ktm`에 저장하고 해당 위치에 쓸 수 없으면 `/mnt/us/ktm`을 사용합니다.
`KTM_DATA_DIR` 설정이 있으면 우선합니다.
`/mnt/us` 사용 시 토큰은 USB 저장소에서도 보일 수 있습니다.

- `config/bridge.conf`: 토큰과 허용 사용자/채팅, 0600 권한
- `state/`: 페어링과 Telegram 업데이트 위치
- `inbox/current.txt`: 최신 수신 원문 **한 건**, 새 메시지로 덮어씀
- `inbox/latest.txt`: Copy 명령으로 복사한 원문
- `outbox/pending.jsonl`: 전송에 실패한 메시지
- `logs/bridge.log`: 통신 진단
- `logs/startup.log`: 가장 최근 KTerm 시작 오류

5번은 메시지를 삭제하거나 누적하지 않습니다. 이전 버전의 `messages.jsonl`은 읽거나 추가하지 않습니다.
앱이 열리지 않으면 기존 KTerm에서 아래 명령으로 기록을 볼 수 있습니다.

```sh
cat /var/local/ktm/logs/startup.log
```

CLI는 `/var/local/kmc/bin/kpm launch ktm status`처럼 사용합니다.
`launch ktm`에 인자가 없으면 GUI를 열고, `status`, `sync`, `diagnose` 등을 붙이면 helper를 실행합니다.
시스템 clipboard는 설치된 `xclip`/`xsel`이 있는 경우에만 가능하며 보장하지 않습니다.
텔레그램 수신은 지정된 사용자와 채팅 한 개만 허용합니다.
Telegram의 장기 미수신 보존 한계, 응답 유실 시 송신 중복 가능성은 남아 있습니다.

## 빌드·검증·배포

개발 PC에서 `make test`는 통신 mock 및 실제 PTY/셸 테스트를 실행합니다.
테스트용 Python은 개발 PC/CI에만 필요합니다.

배포 workflow는 Kindle용 libc를 가진 ARM 도구 모음으로 두 helper를 정적으로 빌드합니다.
GTK/VTE를 빌드하거나 새 공유 라이브러리를 기기에 요구하지 않습니다.
Linux 셸, BusyBox 셸, QEMU에서 ARM 실행 파일을 테스트한 뒤
KPM manifest v2와 ELF 아키텍처·정적 링크를 검사합니다.
통과한 `.kpkg`와 루트 `manifest.json`을 한 커밋으로 게시합니다.

Kindle cross compiler를 설정한 새 빌드 디렉터리에서:

```sh
make CC=arm-kindlehf-linux-gnueabihf-gcc LDFLAGS=-static
make package TARGET_PLATFORM=kindlehf
```

기존 host 바이너리가 있으면 먼저 빌드 결과를 분리해야 합니다.
실기기 GTK 화면, 키보드, 절전 동작은 CI/QEMU로 검증되지 않습니다.
