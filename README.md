# ktm

Kindle용 KPM 패키지에서 동작하는 1인용 Telegram Bot API 브리지입니다. 앱 이름과 실행 명령은 `ktm`입니다. KUAL, Python, jq, Bash, rootfs 영구 수정에 의존하지 않습니다.

현재 저장소에는 POSIX `sh` lifecycle/launcher, 외부 `curl`을 설정 파일 방식으로 호출하는 작은 C helper, Telegram `getUpdates`/`sendMessage`/`getMe`, pairing, 허용 user/chat ID 검사, update offset, inbox/outbox JSONL 저장, `latest.txt` 기반 복사 경로와 KTerm companion 연동 문서가 포함됩니다.

## 중요한 상태

실제 Kindle의 KPM 버전, `curl` TLS 지원, KTerm/VTE 버전, 화면 UI 프레임워크는 이 개발 PC에서 확인할 수 없습니다. `kpm/manifest.json`의 필드는 패키지 메타데이터 골격으로 제공하며, 실제 장치에서 아래 진단을 먼저 실행해야 합니다.

```sh
uname -a
command -v kpm && kpm --version
command -v curl && curl --version
command -v kterm || true
```

실기기 검증 전에는 기존 KPM/KTerm 패키지를 덮어쓰거나 `/` 아래 파일을 수정하지 마십시오.

## 빌드

```sh
make
make test
make package TARGET_PLATFORM=kindlehf
```

Kindle용 cross compiler가 있는 경우 `CC`, `CFLAGS`, `SYSROOT`를 명시합니다. 플랫폼을 모르는 상태에서는 양쪽을 무조건 같은 바이너리로 포장하지 않습니다.

## GitHub 배포

저장소 이름은 `ktm`으로 사용하고, `v0.1.0`처럼 태그를 push하면 GitHub Actions가 테스트 후 두 플랫폼 패키지를 Release에 자동 업로드합니다.

```sh
git tag v0.1.0
git push origin v0.1.0
```

저장소를 `financewiki-park/ktm`으로 만들 경우 가장 짧은 실사용 다운로드 주소는 다음과 같습니다.

- [ktm-kindlehf.kpkg](https://github.com/financewiki-park/ktm/releases/latest/download/ktm-kindlehf.kpkg)
- [ktm-kindlepw2.kpkg](https://github.com/financewiki-park/ktm/releases/latest/download/ktm-kindlepw2.kpkg)

Release 페이지는 [github.com/financewiki-park/ktm/releases/latest](https://github.com/financewiki-park/ktm/releases/latest)입니다. GitHub 사용자명이나 저장소 소유자가 다르면 URL의 `financewiki-park`만 바꾸면 됩니다.

## 사용 흐름

```sh
ktm setup < bot-token.txt
ktm pair
ktm sync
ktm inbox
ktm last
ktm copy
ktm status
ktm diagnose
ktm reset-pairing
```

`setup`은 `getMe`로 토큰을 확인한 뒤 저장합니다. 토큰은 일반 로그나 process argument에 넣지 않도록 curl config 파일에만 기록되며, config 파일은 mode 0600으로 생성합니다. 단, `/mnt/us` fallback을 사용하면 USB mass storage에 토큰이 노출될 수 있습니다.

`pair`가 출력한 일회용 코드를 Telegram Bot에 `/pair CODE`로 보내면 그 메시지의 `from.id`와 `chat.id`가 저장됩니다. pairing 뒤에는 다른 user/chat의 메시지를 표시하지 않습니다. Telegram에서 수신한 문자열은 자동 실행하지 않으며, KTerm 경로도 Enter를 넣지 않습니다.

## 저장 위치

기본 우선순위는 `/var/local/ktm`, 쓰기 불가 시 `/mnt/us/ktm`입니다. `KTM_DATA_DIR`로 명시할 수 있습니다.

```text
config/bridge.conf
state/update_offset
state/pairing_state
state/pairing_code
state/pairing_expires
state/version
inbox/messages.jsonl
inbox/latest.txt
outbox/pending.jsonl
logs/bridge.log
```

패키지 디렉터리에는 이 데이터를 저장하지 않습니다. 업데이트/삭제 시 사용자 데이터는 보존되어야 합니다.

## 제한과 후속 작업

- 앱 실행/수동 `sync` 중심이며 상시 polling daemon을 설치하지 않습니다.
- 24시간 이상 sync가 없으면 `status`에서 경고합니다. Telegram의 미수신 update 보존 한계 때문에 장기 offline 메시지는 복구되지 않을 수 있습니다.
- Bot API는 네트워크 응답이 끊긴 뒤 이미 처리된 `sendMessage`를 완전히 구분할 idempotency key를 제공하지 않습니다. 따라서 outbox는 실패 시 보존하지만, 응답 유실 구간의 중복 전송 가능성은 운영상 한계입니다.
- 실제 Kindle GUI와 KTerm VTE 직접 삽입은 해당 장치의 소스/ABI 확인 후 companion fork에서 구현해야 합니다. 자세한 안전 경계는 `kterm/README.md`를 참조하십시오.
