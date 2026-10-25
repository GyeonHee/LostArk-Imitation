# Source/LoA — 캐릭터 · 컨트롤러 · 게임 흐름 · 밸런스

## ALoACharacter
- 스탯은 `UCharacterDataAsset`(`DA_Sorceress`)이 `PostInitializeComponents`에서 덮어씀 → 실제 캐릭터는 **`BP_Sorceress`**(`BP_LostArkCharacter`는 미사용)
- `ReceiveDamage(Amount, Kind)` 한 곳에서 광폭화 배율·방어력·각인 피격 효과 처리. `TakeDamage`는 인자 `DamageAmount` 사용
- `IsActionLocked()` = 넉다운 · 경직 · 기절 · 끌려감 · 붙잡힘 · 사망 → 컨트롤러의 이동·스킬·대시 차단 지점이 전부 이걸 봄. 매혹은 일부러 **제외**(무작위 이동이 Tick 이동 경로를 써야 해서 — 대신 실제 입력 지점마다 `IsCharmed()` 체크)
- `IsImmune()` = 사망 또는 무적(`SetInvulnerable`, 클리어·중단 시) → 데미지·CC·매혹 무시
- `TryConsumeTickDamage(Source, Interval)`: 같은 종류 장판이 여러 개 겹쳐도 Interval당 1회(90%만 요구)
- 카메라: `SetCameraZoomOverride`/`SetCameraRotationOverride` + Clear, Tick 보간, 기본값은 BeginPlay 값

### CC 등급
| CC | 함수 | 동작 |
|---|---|---|
| 넉다운 | `ApplyKnockdown(Source)` | 반대 방향 Launch → `KnockdownHopSettleTime`(0.4초) 뒤 정착(맞을 때마다 타이머 재시작 — 착지 폴링은 여러 Launch가 겹치면 실패해서 폐기) → 3초 누움. 스페이스 = 즉시 기상(슬롯 19, 15초 쿨, 완전히 누운 뒤만). 넉다운 중 `bConstrainToPlane` 임시 해제 |
| 기절 | `ApplyStun(Duration)` | 제자리 정지, 지속시간은 호출마다 |
| 경직 | `ApplyStagger()` | 기절과 같은 방식, `StaggerDuration` 0.3초 고정 |
| 끌려감 | `ApplyPull` | 정지 `PullHoldDuration` → Tick에서 `AddActorWorldOffset(sweep)`로 목표까지(속도 = StateTree `PullStrength` cm/s) → **패턴이 `ReleasePull()` 할 때까지 묶임**, 안전장치 `PullMaxHoldTime` 8초 |
| 붙잡힘 | `SetHeldByPattern(bool)` | 시간 제한 없음 — 건 쪽이 반드시 해제(ExitState) |
- 넉다운이 더 강해서 경직·기절은 넉다운 중 무시. 모든 CC는 캐스팅·사거리이동 취소 + 정지. BP 훅 `On*VisualChanged`
- 사망 `Die()`: `OnDied`, 매혹 해제, 이후 데미지·CC 무시

### 매혹
- 3스택(`MaxCharmGauge`), 1스택당 30초마다 1개씩 감소(루핑, 재히트 시 재시작). 3스택 도달 → `bIsCharmed` 5초 → `EndCharm`이 스택 0. 매혹 중 `AddCharmGauge`는 바로 return(연장 금지). 스택 증가는 보스 정산 게이지에도 알림
- 매혹 중 컨트롤러 `PerformRandomCharmAction`(0.8초마다): 스킬을 붙잡고 있으면 그대로 둠(이동 목표를 덮으면 사거리 이동과 싸움) → 무작위 이동 목표 → **쿨타임 아닌 슬롯만** 골라 `SkillManager->HandleKeyDown` 직접 → 타입별로 발동에 필요한 만큼 붙잡음(Cast = CastTime+0.3 등), Tick이 `HandleKeyHeld` 대신 흘림. 화면 연기 출처 "Charm"

## ALoAPlayerController
- 이동: 클릭 → `RequestDirectMove`(NavMesh 없음). `BP_LoAPlayerController` EventGraph의 이동 노드는 C++와 충돌해서 제거됨
- Tick 순서: `UpdateRaidIntro`/`UpdateDefeat`/`UpdateClear`는 **폰 체크보다 앞**, `UpdateCastBar`/`UpdateEnrageTimer`는 **`IsActionLocked` 조기 return보다 앞**
- **HUD 슬롯 클릭 차단**: HUD가 HitTestInvisible이라 컨트롤러가 커서와 슬롯 위젯 지오메트리를 직접 비교(`FindHUDSlotUnderCursor`) → 슬롯 위 클릭은 이동·기본공격으로 안 씀(`ConsumeHUDSlotPress`, 창 위(`IsCursorOverPanel`)도). UI 위에서 시작된 클릭은 Triggered·Released 이동도 무시(`bMovePressSuppressed`). 6px 이상 끌면 같은 그룹끼리 스왑
- 키 1~4·I 등은 입력 액션 없이 `SetupInputComponent`의 `FInputKeyBinding`
- 위젯 생성은 BeginPlay(보스·아레나가 있을 때만 해당 위젯), 위젯 클래스는 BP Details에서 유저가 직접 할당
- **레벨 이동은 전부 `TravelToLevelWithFade()`**(페이드 아웃 → OpenLevel, 도착 `bFadeInOnArrive`). 레벨은 소프트 참조라 패키징 `+MapsToCook` 필요
- 보스 처치(`OnBossDefeated`): 무적 + 보상 지급 + 클리어 화면(12초 또는 [정비소로 이동]) / 사망(`OnPlayerDied`): 흑백 후처리 + 패배 화면 3초 / 중단: 확인 창 → 무적+붙잡힘 → 복귀. 셋 다 `ReturnToLobby()`, 먼저 뜬 쪽 우선

## 저장 `ULoAGameInstance` + `ULoASaveGame`
- `DefaultEngine.ini` `GameInstanceClass=/Script/LoA.LoAGameInstance`. 슬롯 `LoA` = `Saved/SaveGames/LoA.sav`(지우면 초기 상태)
- 스킬 배치·레벨·포인트, 소지품·화폐·장비·보석·각인·아크 패시브, 최고 기록·클리어 횟수, 대기 보상, 각종 1회 지급 플래그. `CurrentSaveVersion` 2
- 테스트 명령(`LoAKillBoss`, `LoAKillSelf`, `LoAGiveItem` 등) 쓴 판은 최고 기록·첫 클리어 제외. 전체 목록 `Docs/테스트_명령어.txt`

## 데미지 / HP 밸런스
- 보스 4,746,719,168 / 285줄(줄당 약 1,665만), 킬타임 완벽 3분 · 실전 약 4분. 킬타임 조절은 K(`Stat/` 문서) 하나로
- 설계: 짤패턴 **5~8대 맞으면 사망**. 패턴 데미지는 원래 HP 10만 기준 값이고 `IncomingDamageScale`로 비율 유지

| 패턴 | 값 (10만 기준) | 위치 |
|---|---|---|
| 4거울 1틱 / 8거울 1틱 / 유도레이저 | 4,000 / 3,000 / 5,000 | `ST_Echidna` 4Mirror·8Mirror |
| 좌우장판 / 끌고간후 / 두번긋고 | 12,000 / 15,000 / 15,000 | `ST_Echidna` |
| 하트발사 | 10,000 + 3초 기절 | `ST_Echidna` HeartBurst |
| 오염 장판 틱 | 2,000/초 | `ABP_HexTile` CDO |
