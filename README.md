# ktm

Kindle KPM용 1인 Telegram Bot 브리지. 기기에 KUAL, Python, Bash, jq는 필요하지 않습니다.
현재 지원 플랫폼은 **kindlehf**입니다.

## 명령 실행 버전 0.1.13

0.1.7–0.1.10의 자체 GTK/VTE 실행 파일은 기기 구동이 확인되지 않았고,
사용자에게 5번 선택 시 종료 및 0.1.10 앱 시작 실패가 발생했습니다.
0.1.13은 설치된 공식 KPM `kterm` 화면에서 Telegram으로 받은 명령을 실제 실행합니다.
빌드·자동 테스트 통과는 실제 Kindle 화면 동작 검증과 다릅니다.
검토 내용은 [구조 및 검증 기록](docs/0.1.12-review.md)에 있습니다.
현재 명령 실행 구조는 [0.1.13 실행 흐름](docs/0.1.13-commands.md)에 있습니다.

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

설치 후 라이브러리에서 `ktm`을 엽니다. 메뉴 상단은 `ktm 0.1.13`입니다.
업데이트는 저장된 봇 토큰, 페어링, 메시지를 보존합니다.
KPM이 공식 `kterm` 패키지를 의존성으로 관리합니다. ktm은 그 바이너리와 키보드를 덮어쓰지 않습니다.

## 메뉴

1. Setup: BotFather가 준 전체 토큰을 입력합니다.
2. Pair: 표시된 `/pair 코드`를 자신의 Telegram 앱에서 만든 봇에게 보냅니다.
3. Sync & Inbox: 메시지를 받아 표시합니다.
4. Send: 해당 Telegram 채팅으로 답장합니다.
5. Run received command: 수신한 명령 전체를 확인하고 `r` + Enter로 실제 실행합니다.
6. Status: 버전·페어링·동기화 상태를 표시합니다.
7. Exit: 앱을 닫습니다.
8. Edit one-line command: 한 줄 명령을 셸 입력 줄에서 수정한 뒤 Enter로 실행합니다.

5번은 Telegram에서 보낸 설치 명령, 변수 지정, 파이프, 여러 줄 명령을 실제 `/bin/sh`에서 실행합니다.
명령문을 확인한 뒤 `r`과 Enter를 누르면 실행하고, Enter만 누르면 취소합니다.
실행 중 설치 프로그램의 질문에 키보드로 답할 수 있습니다.
출력과 오류, 종료 코드는 같은 KTerm 화면에 남으며 Enter를 누르면 메뉴로 돌아갑니다.
명령 실행은 `/mnt/us`에서 시작합니다. 한 메시지 안의 `cd`와 변수는 다음 줄에도 적용되지만
다음 실행까지 셸 환경이 유지되지는 않습니다. 메시지 수신/동기화만으로 실행하지 않습니다.

5번에는 기존 512바이트/한 줄 제한이 없습니다. 명령 본문을 일반 텍스트로 보내십시오.
Markdown의 코드블록 표시나 설명문은 명령에 포함하지 않습니다.
수신 원문은 유지하며, Windows CRLF 줄바꿈은 실행할 사본에서만 LF로 변환합니다.
8번의 편집 입력은 기존 셸 버퍼 때문에 **512바이트 이하의 한 줄**만 지원합니다.
8번에서는 Ctrl+U로 지우기, Ctrl+C로 취소, `exit`로 메뉴 복귀가 가능합니다.
한글 키보드 및 글꼴의 실기기 동작은 별도 검증이 필요합니다.

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
