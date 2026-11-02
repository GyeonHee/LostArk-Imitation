# Source/LoA — 캐릭터 · 컨트롤러 · 게임 흐름 · 밸런스

## ALoACharacter
- 스탯은 `UCharacterDataAsset`(`DA_Sorceress`)이 `PostInitializeComponents`에서 덮어씀 → 실제 캐릭터는 **`BP_Sorceress`**(`BP_LostArkCharacter`는 미사용)
- **외형 = 원본 소서리스 모델**(`Magician/Sorceress/Model/` — `Mesh/SK_Sorceress` 본 346·키 1.32m → `BodyMeshScale` 1.3, 정면 +X라 Yaw 0, 무기 `Weapon/SK_Sorceress_Staff`를 `b_weapon_rhand`에). BP_Sorceress에는 아직 마네킹이 들어 있고 **`ALoACharacter::ApplyBodyMeshOverride`(PostInitializeComponents)가 런타임에 교체** — 값은 `ASorceress` 생성자(`BodyMeshOverride`·`WeaponMesh`·`*Anim`), 비어 있으면 BP 메시·AnimBP 그대로. BP 머티리얼 오버라이드는 비움
  - **AnimBP 없음** — `ULoACharacterAnimInstance`(C++)의 프록시가 Evaluate에서 시퀀스를 직접 샘플링: 기본 = 대기(`idle_battle_1`)↔달리기(`run_battle_1`) 속도 블렌드(`RunAnimFullSpeed`), 위에 액션 레이어(`PlayAction`/`PlayActionChain` 크로스페이드, 반복 아닌 단계는 다음 단계·마지막 프레임 유지·자동 종료). MCP로는 AnimBP 대상 스켈레톤을 못 정해서 이 방식. 몽타주·슬롯은 못 씀
  - **루트 본 `b_root`의 수평 이동은 포즈에서 고정**(`RootLockBone`, 높이는 남겨 점프가 보임) — 스켈레톤 맨 위가 "Sorceress"(아마추어)라 엔진 루트 모션으로는 못 뽑음. 대신 **`bRootMotion` 단계(스킬 애니메이션)는 b_root 수평 이동량을 매 업데이트 쌓아 캐릭터 Tick이 `AddActorWorldOffset`(sweep, 메시 배율·회전 적용)** — 익스플로전 발동 `sk_greatfire_02`가 뒤로 약 2.2m 점프, 돌풍·혹한 마무리 앞으로 약 0.8m. CC 애니메이션·대시는 코드가 이미 밀어서 false. 행동 잠금(CC) 중엔 적용 안 함
  - 대시·점멸이 끝나면 그 자리에 선다(컨트롤러 `bWasAutoMovingBeforeDash` = false — 예전엔 찍어 둔 목적지로 다시 걸어갔음)
  - **대시(스페이스)** = `DashAnim`(`sk_movingillusion`)의 `b_root` 이동량 곡선(시작 대비 거리 비율)을 진행도로 DT `DashImpulse`(800 = 8m, 로아 값)만큼 매 틱 `SetActorLocation`(sweep, 벽에 막히면 멈춤). 순간적으로 빠르게 나가는 구간(진행도 `DashHideStartProgress` 0.04 ~ `DashShowProgress` 0.85, 약 0.1~0.47초 — 빠른 구간만 숨기면 0.1초라 너무 깜빡여서 넓힘)은 **몸·무기·아바타 메시를 숨겼다가 도착하면 다시 보임**(원래 꺼져 있던 부위는 안 켬). 대시 중 `bScriptedMoveLock` → `IsActionLocked`, CC·사망이면 그 자리에서 멈춤. 애니메이션이 없으면(마네킹) 예전처럼 순간이동
  - 상태 연결(C++, 유저가 정한 짝): **넉다운은 첫 타에 동작 전체를 체인으로 틀고 마지막 프레임(누운 자세)을 기상까지 유지**(정착 `SettleKnockdown`은 애니메이션 안 건드림) — 보통 `down`(선 자세에서 2m 뒤로 날아가 눕기까지 한 번에 1.33초) / 장풍(`ApplyKnockdown(..., bBlown=true)`) `knockdown`→`knockdown_land` 또는 `twistknockdown`→`twistknockdown_land` 랜덤 / 공중에 뜬 채 또 맞음(에어본) `bound`→`bound_land` / **누운 채 또 맞음 `bound_hit_land`**(시간 연장 없음 — `bound_land`와 시작 자세는 같고, 맞아서 몸이 25cm 들썩·50cm 밀리는 버전) / 저절로 기상 `resurrect_1`(2초). **기상 동작 중 `bGetUpLock`으로 조작 잠금**(`StandUpLockTime` 0.55 / `InstantGetUpLockTime` 1.07초 — 마우스를 누르고 있으면 바로 걸어가 기상 애니메이션이 끊겼음), 루트 이동(구르기)은 이 잠금과 무관하게 적용 / 스페이스 즉시 기상 `sk_standup_1`(**커서 쪽으로 돌아** 앞으로 약 4m 구르며, 루트 이동 적용 — 루트 이동은 `SafeMoveUpdatedComponent`: 정착 직후 캡슐이 바닥에 붙어 `AddActorWorldOffset` sweep이 막혀 제자리였음 — `sk_standup_normal_1`과 1cm 이내 차이, 끝 자세만 전투/평상) / 기절·붙잡힘·**매혹** `abn_stun_1`(매혹 중 무작위 스킬 동작이 끝나면 Tick이 다시 틈) / 경직 `dmg_idle_1`·`dmg_idle_2` 랜덤 / 사망 `dead_1` → `dead_1_loop` / 저스트 가드 `evt_defence_loop_1`.
  - **대시·점멸 이동 중 `bMoveImmune`** → `IsImmune`(모든 피해·CC·매혹) + 붙잡힘(`SetHeldByPattern`)도 무시 해제는 `StopStateAnim(그 시퀀스)` — 다른 상태가 덮어쓴 뒤 늦게 오는 해제는 무시. 스킬·기본공격 애니메이션은 `Skill/` 문서. 머티리얼은 FBX 기본(색·노멀만, 불투명) — **속눈썹 `lashes_hiP`만 투명 카드라 Masked + Diffuse 알파 → OpacityMask·양면**(불투명이면 판 전체가 검게 칠해져 눈을 덮었음). **홍채 `pupill_hi_P`는 불투명 그대로** — RGB가 흰자까지 그려진 눈알 전체이고 알파는 가운데 작은 원(반사·동공 마스크)뿐이라, 알파를 투명도로 쓰면 흰자가 뚫림
  - 애니메이션 117개는 **`Model/Anims/`**(원본 이름). PSA → `Tools/Blender/psa_to_fbx_psk.py`(기준 `Sorceress/psk/v1/upper.psk`, 아마추어 이름 `Sorceress`, 115개 = `pc_mem_00_ani.psa`, `idle_normal_1`·`sk_thunderbolt`은 별도 PSA라 `_mem04_high`·`_hr_high`를 붙임). **함정: PSK 0.01은 아마추어 오브젝트 스케일이라 PSA `translation_scale`은 1** — 0.01로 넣었던 첫 임포트는 몸이 쪼그라들었음(옛 `Model/Animations/`는 삭제함)
  - 의상·무기 변형은 아바타로 옮김(`Item/` 문서 아바타). 헤어 변형·공용 애니메이션·음성은 `LocalOnly/Sorceress/`
- `ReceiveDamage(Amount, Kind)` 한 곳에서 광폭화 배율·방어력·각인 피격 효과 처리. `Fixed`(최대 HP × N 기믹)는 셋 다 안 받음 — 비율 그대로 들어감. `TakeDamage`는 인자 `DamageAmount` 사용
- `IsActionLocked()` = 넉다운 · 경직 · 기절 · 끌려감 · 붙잡힘 · 저스트 가드 자세(`BeginJustGuardStance`, BP 훅 `OnJustGuardStanceChanged`) · 사망 → 컨트롤러의 이동·스킬·대시 차단 지점이 전부 이걸 봄. 매혹은 일부러 **제외**(무작위 이동이 Tick 이동 경로를 써야 해서 — 대신 실제 입력 지점마다 `IsCharmed()` 체크)
- `IsImmune()` = 사망 또는 무적(`SetInvulnerable`, 클리어·중단 시) → 데미지·CC·매혹 무시
- `TryConsumeTickDamage(Source, Interval)`: 같은 종류 장판이 여러 개 겹쳐도 Interval당 1회(90%만 요구)
- 카메라: `SetCameraZoomOverride`/`SetCameraRotationOverride` + Clear, Tick 보간, 기본값은 BeginPlay 값

### CC 등급
| CC | 함수 | 동작 |
|---|---|---|
| 넉다운 | `ApplyKnockdown(Source, HopScale=1)` | 반대 방향 Launch(`HopScale`로 수평 힘 배율, 위로 뜨는 힘·정착 시간은 √배 — 장풍) → `KnockdownHopSettleTime`(0.4초) 뒤 정착(맞을 때마다 타이머 재시작 — 착지 폴링은 여러 Launch가 겹치면 실패해서 폐기) → 3초 누움. 스페이스 = 즉시 기상(슬롯 19, 15초 쿨, 완전히 누운 뒤만). 넉다운 중 `bConstrainToPlane` 임시 해제. 발사 전 `StopActiveMovement` + `CancelAutoMove` — 클릭 이동 요청 속도가 남아 있으면 낙하 중 다시 적용돼 튕김이 상쇄됨 |
| 기절 | `ApplyStun(Duration)` | 제자리 정지, 지속시간은 호출마다 |
| 경직 | `ApplyStagger()` | 기절과 같은 방식, `StaggerDuration` 0.3초 고정 |
| 끌려감 | `ApplyPull` | 정지 `PullHoldDuration` → Tick에서 `AddActorWorldOffset(sweep)`로 목표까지(속도 = StateTree `PullStrength` cm/s) → **패턴이 `ReleasePull()` 할 때까지 묶임**, 안전장치 `PullMaxHoldTime` 8초 |
| 붙잡힘 | `SetHeldByPattern(bool, bShowStunAnim=true)` | 시간 제한 없음 — 건 쪽이 반드시 해제(ExitState). **조작만 막는 용도(입장 이동·레이드 인트로·NPC 메뉴·중단·패턴 실패·발악 연출/격돌)는 `false`** — 로비 → 보스맵에서 기절 모션이 나왔던 원인. 기절 모션은 실제로 잡히는 패턴(파리지옥)만 — 거울잇기 발사 순간은 조작만 막음(유저 요청) |
- 넉다운이 더 강해서 경직·기절은 넉다운 중 무시. 모든 CC는 캐스팅·사거리이동 취소 + 정지. BP 훅 `On*VisualChanged`
- 사망 `Die()`: `OnDied`, 매혹 해제, 이후 데미지·CC 무시

### 매혹
- 3스택(`MaxCharmGauge`), 1스택당 30초마다 1개씩 감소(루핑, 재히트 시 재시작). 3스택 도달 → `bIsCharmed` 5초 → `EndCharm`이 스택 0. 매혹 중 `AddCharmGauge`는 바로 return(연장 금지). 스택 증가는 보스 정산 게이지에도 알림
- 매혹 중 컨트롤러 `PerformRandomCharmAction`(0.8초마다): 스킬을 붙잡고 있으면 그대로 둠(이동 목표를 덮으면 사거리 이동과 싸움) → 무작위 이동 목표 → **쿨타임 아닌 슬롯만** 골라 `SkillManager->HandleKeyDown` 직접 → 타입별로 발동에 필요한 만큼 붙잡음(Cast = CastTime+0.3 등), Tick이 `HandleKeyHeld` 대신 흘림. 화면 연기 출처 "Charm"

### 소서리스 아이덴티티 (`ASorceress` + `USorceressIdentityComponent`)
- `BP_Sorceress`의 부모가 `ASorceress`. 수치는 `LoASorceressIdentity`(헤더) — 로아 값(나무위키) + 자료에 없는 것은 "추정" 주석
- 신비한 마력: 보스·허수아비 피격마다 **스킬별 수급량(풀히트 기준, `SorceressIdentity_SkillGauge` — 유저 제공 표의 특화 0·노룬 값 ÷ 100) × 이 타격의 몫(타격 계수 ÷ DT 스킬 계수) × 특화 배율(1 + 특화 × 0.136%, 1800이면 ×3.45 = 표의 실전값)**. 혹한 11.5 · 아이스 에로우 6.2 · 인페르노 3.1 · 블레이즈 2.2(틱 합) · 돌풍 0.5 · 종말의 날 2.5 · 익스플로전 2.3 · 천벌 2.0. 표에 없는 기본공격만 계수 × 1.8%. 타격 계수는 `NotifyIdentityHit`(피해 ÷ 계수 1 피해, BP 기본공격은 공격력으로 나눔). 종말의 날 BP 행 이름 "Meteor"도 키로 넣음. 강화·해방 중엔 안 참
- Z 마력 방출: 30~99% 강화(속성 피해 +10%) / 100% 해방(+20%·**발동 즉시** Q~F 남은 쿨 25% 감소·슈퍼아머 = 넉다운·경직만 무시). 점화를 배우면 발동 쿨 감소가 점화 값(33/41/50%)으로 **대체**(곱하지 않음), 환류면 Z 불가. 화염 부위파괴 +1·번개 무력화 증가는 시스템이 없어 생략. **둘 다 캐스팅 스킬 시전 시간 1초 감소**(정량 감소 — "1초로 단축" 아님, 유저 확인. `GetCastTimeReduction` → `USkillBase::GetEffectiveCastTime`, 하한 0.1초, 캐스팅 시작 때 정해서 도중에 끝나도 바 길이 유지 — 예전 추정치 캐스팅 속도 +15/30%는 뺌). 지속 = 게이지 × 0.08초, 쿨 2초. 효과는 수정치 출처 "Identity"
- Z 애니메이션 `sk_arcanereinforce` + 몸에 붙는 오라 `Sorceress/VFX/NS_SorceressMagickAmplify`(파랑)·`NS_SorceressMagickRelease`(보라) — `Mixed_Magic_VFX_Pack/VFX/NS_Potion` 복사본에 InitializeParticle Color만 바꿈(원래 밝기 유지). 원본 NS_Potion은 0.5초짜리 1회 이펙트라 복사본은 4개 이미터 Loop Behavior = Infinite(파동·연기는 Loop Duration 1초 + SpawnRate 0 → 1초마다 한 번)로 바꿈. 캡슐 중심에 붙임(발밑이면 몸에 가려 안 보임). 잘 보이게 입자 크기 1.5배·색 채도↑·밝기 2.5배(원본 밝기 기준), 컴포넌트 배율 `MagickVFXScale` 1.3. **Z = 멈춰 서서 애니메이션 → 주먹 쥘 때(`MagickReleaseDelay` 0.88초) 실제 방출**(게이지 소모·버프·오라 동시). 그 사이 CC·사망이면 취소(게이지 안 씀), 검사는 `CanRelease`로 먼저. 강화·해방이 끝나면 오라 Deactivate(`UpdateMagickVFX` 상태 변화 감지). 컨트롤러는 Z 성공 시 클릭 이동 목표를 지움 **Z 키는 `ASorceress::TryActivateIdentity`로** — 컴포넌트 `TryRelease`를 직접 부르면 애니메이션이 빠짐(실제로 그랬음)
- X 점멸: 30%, 15m, 쿨 1초. WorldStatic에서 멈추고 보스는 넘음, 바닥 없으면 당김. 애니메이션 `sk_arcaneshift`의 b_root 곡선대로 목적지까지 **통과 이동**(대시와 같은 `StartAnimMove`, 끝에서 `TeleportTo`로 보스 겹침 해소), 진행도 `BlinkHideStartProgress` 0.14 ~ `BlinkShowProgress` 0.95 동안 안 보임(약 0.4~0.7초). 아크 패시브 "점멸" 노드는 여기(스페이스 대시는 추진력만)
- 환류·응집되는 마력이면 Z 사용 불가. 마력 방출·점멸도 `OnSkillUsed(INDEX_NONE)`(아드레날린·달인)

## ALoAPlayerController
- 이동: 클릭 → `RequestDirectMove`(NavMesh 없음). `BP_LoAPlayerController` EventGraph의 이동 노드는 C++와 충돌해서 제거됨
- Tick 순서: `UpdateRaidIntro`/`UpdateDefeat`/`UpdateClear`는 **폰 체크보다 앞**, `UpdateCastBar`/`UpdateEnrageTimer`는 **`IsActionLocked` 조기 return보다 앞**
- **HUD 슬롯 클릭 차단**: HUD가 HitTestInvisible이라 컨트롤러가 커서와 슬롯 위젯 지오메트리를 직접 비교(`FindHUDSlotUnderCursor`) → 슬롯 위 클릭은 이동·기본공격으로 안 씀(`ConsumeHUDSlotPress`, 창 위(`IsCursorOverPanel`)도). UI 위에서 시작된 클릭은 Triggered·Released 이동도 무시(`bMovePressSuppressed`). 6px 이상 끌면 같은 그룹끼리 스왑
- 키 1~4·I·Z·X 등은 입력 액션 없이 `SetupInputComponent`의 `FInputKeyBinding`
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
