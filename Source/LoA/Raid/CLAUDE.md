# Raid — 아레나 · 에키드나 보스 · 패턴

## 패턴 액터 공통 규칙 (새 패턴을 만들 때 반드시)
- **광폭화 대응**: `BeginPlay`에서 `CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this)`, `SetTimer` 시간은 `/ CustomTimeDilation`. Tick 기반 이동은 자동으로 빨라짐. 예외: `AEchidnaTetherActor::PullResolveDelay`(플레이어가 끌려오는 시간이라 안 나눔), `SetLifeSpan`
- **Task는 스폰만, 완료는 액터의 `IsFinished()` 폴링.** 스스로 소멸하기 전에 Task가 결과(`DidHit` 등)를 읽어야 하면 소멸을 늦출 것(null을 "완료"로 세는 오판 전례)
- **발밑 높이**: `GetActorLocation()`은 캡슐 중심 → 장판은 캡슐 절반 높이를 빼고, 발사체는 발밑 + 약 100cm(플레이어 캡슐 중심 높이)
- **EditDefaultsOnly 크기·반경은 `BeginPlay`에서 컴포넌트에 적용** — 생성자에서 쓰면 BP 오버라이드가 안 먹음. 반대로 BP 컴포넌트 트리에서 크기를 직접 만지면 BeginPlay가 덮어씀 → Class Defaults에서 바꿀 것
- 클래스 프로퍼티가 비면 네이티브 클래스로 폴백 스폰(BP 할당 깜빡 전례). StateTree에는 **네이티브가 아니라 BP**를 넣을 것(하트에서 네이티브를 물려 BP 수정이 안 먹었음)
- 비주얼 기본 = 엔진 BasicShapes 또는 PMC(`CreateMeshSection`, 양면 감김) + `M_MirrorLaser` MID. 이 머티리얼의 색 파라미터 이름은 **`"Base Color"`**(공백), Translucent라 Alpha = 불투명도
- 판정 연출 공용 Niagara: `NS_EchidnaRetreatFan`(`/Game/LostArk/Raid/Echidna/Pattern/VFX/`, NS_Blaze 복제 핑크) — `PatternVFX::SpawnFanArea`/`SpawnRectArea`(`PatternVFXUtil`)로 영역을 격자로 채움. 예고는 반투명 PMC 메시, 판정 순간 메시를 숨기고 VFX만(`bShowHitMesh`/`bShowExplodeMesh`/`bShowFireMesh` 켜면 예전 메시)
- 낙사 방지: 보스 홉·백스텝 전에 착지 예상 지점에 `ECC_Visibility` 수직 트레이스, 바닥 없으면 Launch 생략
- 보스 이동 연출이 타이밍에 정확해야 하면 `LaunchCharacter`/`MoveToLocation` 대신 `SetActorLocation` 보간(Flying 모드) — `ExitState`에서 Walking 복구

## AHexArena / AHexTile
- Pointy-top Axial (q,r). `X = D*(q + r*0.5)`, `Y = D*√3/2*r`, `D = TileSpacing + HexGap`, R = SideCount-1, 타일 수 3R²+3R+1(SideCount 4 → 37). `TileYaw` 30(Flat-top 메시 회전). 좌표 계산은 `ComputeTileLocalTransform` 하나로 공유
- **에디터는 HISM 프리뷰(`OnConstruction`), 플레이는 `BeginPlay`에서 HISM 숨기고 좌표마다 `AHexTile` 스폰**(`TileMap`, `GetTile`, `WorldToTileCoord`, `GetHexDistance`). `EndPlay`에서 정리
- 외곽 벽: 노출 변(SideCount 4 → 24개)을 PMC 사다리꼴로 — 꼭짓점 용접 + 미터 오프셋(`1/cos(반각)`)이라 인접 조각이 맞물림. `WallMaterial`. 보이지 않는 `BarrierMesh`(1500cm, **Pawn만 Block**) — 벽 위에 올라가는 버그 방지, Visibility를 막으면 클릭 이동 트레이스를 가로채서 분리
- 타일: `TileMesh`(BlockAll, 바닥) + `OverlapBox`(서 있는지 감지). 타입 Normal/PoopZone/Flower, 머티리얼만 교체. 위에 선 캐릭터를 타입 무관하게 기억
- **오염 장판**: 비활성(핑크 `MI_HexTile_PoopInactive`, 효과 없음) / 활성(`SetAllPoopTilesActive` — 빨강 `MI_HexTile_PoopActive` + 빨간 테두리, 틱마다 매혹 + 데미지). 서 있는 도중 활성화되면 `PoopActivateGraceTime`(0.5초) 뒤 첫 틱. 머티리얼 `M_HexTileTinted`(`Tint`/`Glow`)
- **꽃**: 실제 이웃이 전부 PoopZone인 Normal 타일이 자동으로 Flower(`NotifyTileTypeChanged`, 외곽은 이웃 수만큼만) → 바닥은 PoopMaterial + `AEchidnaBigFlowerActor`(PMC 순수 비주얼). `TickFlowerAura`: 꽃 1칸 이내면 1초마다 매혹 1. 예전 `FlowerMaterial` 필드는 PIE에서 옛 값이 남아 삭제함
- **시작 배치**(`SetupInitialLayout`, 폰 스폰 뒤라 한 틱 미룸): 비활성 오염 8칸(폰 타일 제외, 시작부터 꽃이 피는 배치 스킵) + 파란 테두리 2칸(`MarkerTileCoords[0]` 외곽 랜덤, `[1]` 거기서 거리 2인 안쪽). 거울잇기 끝에 `RerollMarkerTiles`
- 테두리(`HighlightMesh` PMC) 우선순위: 위험 점멸·활성 오염(빨강) > 거울잇기·그네(노랑 `SetLinkHighlighted`) > 파란 테두리. HDR 색으로 블룸

## AEchidnaBoss / AI
- **HP는 `double`**(싱글 4,746,719,168 / 285줄 — float는 뭉개지고 int32는 오버플로). 표기 `FText::AsNumber(int64)`. `TotalLines`는 `BP_Echidna`·레벨 인스턴스 값, MaxHP는 C++ 기본값을 따름
- `TakeDamage`는 인자 `DamageAmount`를 씀(Super 반환값이 가끔 0). 피격 시 데미지 숫자(`ADamageNumberActor`)
- 발밑 앞/뒤 표시 `UBossDirectionIndicatorComponent`(PMC 서브클래스, 캡슐에 붙어 같이 돎, 정면 호 가운데가 뾰족)
- `bOrientRotationToMovement=false` — 밀려나도 회전 안 함, 회전은 Task가 직접
- **AI 시작 경로 2개**(`bStartAILogicOnPossess`, `UStateTreeComponent::bStartLogicAutomatically`) 둘 다 꺼둠 → `StartCombat()`이 광폭화·정산·AI를 함께 시작(`bWaitForIntro`, 인트로가 안 부르면 `IntroMaxWait` 10초 뒤 자동)
- 처치(`HandleDefeated`): ClearTime 기록, `StopBossLogic`(진행 Task의 ExitState가 정리), 이후 데미지 무시
- **광폭화**: `EnrageTimeLimit` 540초 → `CustomTimeDilation` 2배(보스·AI 컨트롤러·이후 스폰 패턴 액터) + 플레이어 받는 피해 ×2(`ALoACharacter::ReceiveDamage`). 진행 중이던 패턴 액터는 원래 속도. static `GetEnrageTimeScale`/`GetEnrageDamageMultiplier`
- **정산 게이지** 0~100%: 자연 1.4~1.8초마다 1%, 매혹 스택 +2~3%, 3스택 +10~15%. 발동 지점 `SettlementThresholds` {25,50,75,100} — `GetNextSettlementThreshold()`(넘었고 안 쓴 가장 낮은 지점), Task 진입 시 `Consume...`. 25/75 = 똥장판, 50/100 = 거울잇기, 100 끝나면 0으로 초기화. **큰 패턴 중엔 정지**(`IsSettlementPaused`) + UI 흑백
- **카운터**: `OpenCounterWindow` = 오버레이 머티리얼 청백 발광. 성공 = 창 열림 + 헤드어택(시전자 위치가 정면 ±60도) + `bCanCounter` 스킬. 판정 대상은 `ICounterable::TryCounterHit(Attacker, HitComponent)`(보스 본체·거울 줄). 성공 시 파란 `Counter!`(`ActivateLabel`) + 그로기
- **그로기**(`StartGroggy`)는 플래그 + 정지 + BP 훅뿐, AI를 안 멈춘다 — 부른 Task가 `IsGroggy()` 동안 Running으로 버텨야 보스가 가만히 있음. UI 무력화 게이지가 남은 시간 표시
- `IncomingDamageScale`(11.356): 플레이어 방어력 도입 후에도 패턴 체감이 같도록 곱하는 배율 — 자세한 건 `Source/LoA/Stat/CLAUDE.md`
- **애니메이션**: `LocalOnly/Echidna/Animations/Battle`(281 — 대기·이동·회전·`att_battle_*`·그로기 `dmg_critical_*`·사망) / `Event`(47 — 공중 등장 `sky_*`·변신 `sden_change`·추락 `fall*`·캐스팅·흡수). 전부 `Echidna_mo_Skeleton`, 원본 이름 그대로. 아직 BP·몽타주 연결 없음 — **안 쓰는 동안 LocalOnly(깃 제외)**, 보스에 연결할 때 `EchidnaModling/Echidna/Animations`로 옮겨서 푸시
  - 원본은 PSA(다운로드 폴더 `Echidna (1)`) → `Tools/Blender/psa_to_fbx.py`(Blender 5.2 + PSK/PSA 애드온)로 FBX → 기존 스켈레톤에 임포트. 함정: **PSA 위치는 cm라 `translation_scale=0.01`**(안 하면 메시가 100배로 터짐), Blender 5 FBX 내보내기는 `animation_data` 없는 아마추어의 액션을 통째로 빠뜨림, 메시 없는 FBX는 MCP 스켈레탈 임포트가 실패 → 메시를 같이 넣고 생긴 중복 메시는 삭제
  - 안 넣은 것: `for_Echidna_BIG.psa`(왕키드나 50개), 뱀·거울·스킬·그네 메시와 애니메이션(PSK/PSA 그대로 다운로드 폴더에)

## StateTree `ST_Echidna` 규칙
- **MCP로는 읽기만 가능**(`StateTreeTools.get_root_states/get_children/get_tasks`) — 배치·값 변경은 에디터에서 유저가 직접. Task 인스턴스에 한 번 입력한 값은 C++ 기본값을 바꿔도 안 바뀜(노란 화살표 → Reset to Default)
- 새 Task/Enum/UCLASS 추가는 Live Coding으로 노드 목록에 안 뜰 수 있음 → 에디터 닫고 풀 빌드. 인스턴스 데이터 레이아웃 변경도 Live Coding 크래시 위험 — 필드를 지우기보다 의미를 재해석한 선례 있음(`PullStrength`)
- **짤패턴**: `SmallPatternRotation` 안에 State마다 **완료 조건이 있는 Task 하나만**, On State Completed → 밖의 형제 `Cooldown`(Wait 3초 + `FStateTreeTask_EchidnaPatrol`, NavMesh 필요). Wait를 패턴 State에 나란히 넣으면 "Any" 정책으로 먼저 끝나버림(버그 전례). 완료를 안 리턴하는 Task만 두면 영원히 안 끝남
- **큰 패턴**(시간·정산·줄): Root 자식 State + Task 하나, **Root On Tick Transition**(Enter Condition 쓰지 말 것), On State Completed → Patrol
  - 조건: `Boss Enrage Time Reached`(RemainingSeconds) / `Boss Settlement Gauge Reached`(GaugePercent) / `Boss Line Threshold Reached`(TriggerLine, **TotalLines보다 작아야**)
  - 큰 패턴 Task는 진입 시 `MarkBigPatternTriggered()`(Task PatternName과 **State 이름 둘 다** 표시 — 이름 불일치로 매 틱 재진입·무한 반복 버그가 두 번 남) + `SetActiveTimedPattern`, Exit에서 해제. 조건은 큰 패턴 진행 중이거나 `WaitWhileStatesActive`(`SmallPatternRotation`) 활성이면 false → 짤패턴이 끝난 뒤 시작. `재진입` 경고 로그 = 이름 불일치
- 빠른 테스트: 시간 패턴은 조건 `RemainingSeconds`를 크게(예: 530 → 시작 10초 뒤), 정산 패턴은 `BP_Echidna`의 `SettlementNaturalAmount`를 크게(예: 20). 보스 인트로 없이 바로 싸우려면 레벨 보스의 `bWaitForIntro` 끄기
- **짤패턴 연속 방지**: `SmallPatternRotation`(TrySelectChildrenAtRandom) 자식 State마다 Enter Condition `Echidna Not Recent Small Pattern`(파라미터 없음, `AvoidRecentCount` 1 = 직전 패턴만 제외). 짤패턴 Task가 EnterState에서 자기 State 이름을 `RecordSmallPattern`으로 기록 — **새 짤패턴 Task를 만들면 `RecordSmallPatternEntered` 호출을 넣을 것**
- 배치 현황(2026-09-28 확인): 짤패턴 9종 + 큰 패턴 5종 배치됨. **`Echidna Counter Pattern`(보스 본체 카운터)은 미배치**. Donut Slash의 `FanZoneClass`(3번)가 `BP_EchidnaFanZone_Stagger`로 들어가 있음(설계는 넉다운 `BP_EhidnaFanZone`)

## 공용 액터
- **`AEchidnaFanZoneActor`**(부채꼴/고리/원): 예고(`TelegraphDuration`, 전체 범위 표시) → `RingCount`등분 고리를 `RingInterval`마다 안→밖 확장, 고리마다 그 구간만 1회 판정. `FanAngle 360` = 원/도넛, `FanInnerRadius` > 0 = 고리. 단발 판정이 필요한 패턴은 `RingCount 1`을 강제로 넘김. CC는 BP 기본값: `bApplyKnockdownOnHit`(우선) / `bApplyStaggerOnHit` / `bApplyCharmGaugeOnHit`
  - BP: `BP_EhidnaFanZone`(오타 그대로, 넉다운) / `BP_EchidnaFanZone_Stagger`(경직) / `BP_EchidnaFanZone_KnockdownCharm`(넉다운+매혹)
- **`AEchidnaTetherActor`**(줄기/리본): 예고 → `PerformSnap` 판정 + 핑크 줄기 뻗었다 빨려 들어감(`StrikeMeshComp`). 스위치 `bApplyPullOnHit`/`bApplyStunOnHit`/`CharmGaugeAmount`. 끌기면 `PullResolveDelay`(1.2초, 플레이어 `PullHoldDuration`+드래그보다 길게) 뒤 `FinishSnap`에서 완료·소멸 예약. `BP_EchidnaTether`(끌기) / `BP_EchidnaRibbon`(기절+매혹, 길이 1600) / 육각 리본은 Task가 `HitDamage`·`bApplyKnockdownOnHit`·`bApplyStaggerOnHit`를 켜서 직접 때리는 리본으로 씀(넉다운은 중심선 바깥쪽으로)
- **`AEchidnaHeartActor`**: Launch 전엔 제자리(예고 마커 겸용), 후엔 직선 비행. 첫 대상 1명만, 맞으면 소멸. 데미지 + 기절 + 매혹(이미 기절 중이면 매혹 생략 — `ApplyStun` **전에** 검사). `SetSpawnPoopMarkOnHit(true)`면 매혹 대신 발밑 오염 게이지. 메시는 PMC 압출 하트(세운 카드, 비행 중 회전) — 컴포넌트 타입을 바꾼 이력이 있어 `BP_EchidnaHeart` 재컴파일 확인 필요
- **`AEchidnaMirrorActor`**(4거울·8거울·유도): Tracking(장판이 플레이어 추적) → Firing(방향 고정, 1/3초마다 데미지 틱 + 넉다운, `MaxDamageTicks = floor(Firing/Interval)+1`). 추적은 **따라잡기 방식**(각도 차 25도 이하 8도/초 ~ 100도 이상 70도/초, SmoothStep) — 걸어서 피하려면 각속도 × 3000cm < 600cm/s → 약 11도/초 이하. 레이저 VFX `NS_EchidnaMirrorLaser`(번개 줄기만, 로컬 +X로 뻗음, `User.BeamSize`/`User.BeamLife`) — **C++ 타이머가 0.12초마다 1회 재생 줄기를 띄우고 매 틱 방향 맞춤**(나이아가라 무한 반복은 1초 만에 꺼지는 문제). 미활성 대기 거울은 `ZoneMeshComp` 숨김
- **`AEchidnaOrbActor`**: Outgoing `MaxRange` → Returning은 발사 지점을 관통해 반대편 `-MaxRange`까지(보스 뒤도 위험). 왕복 구간마다 `AlreadyHit` 초기화(최대 2회 피격), 맞아도 소멸 안 함. Task `Orb|Override`는 스폰 후 `ApplyOverrides()`
- 타일 플래그·카메라: `ALoACharacter::SetCameraZoomOverride/SetCameraRotationOverride`, 화면 연기 `SetScreenFog`(출처 "Pattern"), 대사 `ShowPatternDialogue` → `ALoAPlayerController::ShowBossDialogue`

## 짤패턴 (`EchidnaBossStateTreeUtility`)
- **4거울**: 보스 정면 기준 대각 4방향 거울 동시 스폰, 전부 끝날 때까지 Running
- **8거울**: 4개씩 두 차례(나중 4개는 미리 스폰 후 늦게 Activate) + 개인 유도레이저가 `GuidedSpawnDelay`(2초) 뒤 보스 위치에서 스폰(즉시 스폰은 피할 틈이 없었음). 유도 거울은 **넉다운 없이 데미지만**(넉다운이면 스포크 레이저를 못 피함). 스포크 8개 폭 = BP 값 × Task `SpokeBeamWidthScale`(1.5) — 늘어난 폭은 번개 줄기를 옆으로 나란히 더 띄워 채움(`ScaleBeamWidth`, 줄기 하나의 `LaserVFXWidth`를 키우면 눈에 띄게 안 넓어짐)
- **뒤로 빠지며 좌우장판**(RetreatFan, 공략 그림 4번): 조준 방향은 **패턴 시작 시 1회 고정**(`BaseAimRotation`), 보스는 패턴 내내 그 정면을 봄(`LockedYaw`). 백점프 2번: 시작하자마자 1번째 백점프 → **착지 자리**에서 1번 장판(왼쪽 -`FanSideYaw` 20) → 1번이 터지기 시작하면(`HasStartedExploding`) 2번째 백점프 → **착지 자리**에서 2번 장판(오른쪽 +20). 2번은 1번 폭발 시작부터 `SecondFanInterval`(1.75초, 광폭화 배율) 뒤에 터지도록 남은 시간을 예고로(최소 `SecondFanMinTelegraph` 0.3), 1번 원점에서 물러난 거리만큼 사거리 추가. 1·2번 다 끝나야 종료. 장판은 앞이 잘린 부채꼴(`FanAngle` 50 → 가운데 10도 겹침, `NearHalfWidth` 150, `FanRangeScale` 1.4). 옛 `FanYawOffset` 32 + 보스가 1번 방향을 보던 방식은 두 장판이 64도 벌어져 2번이 꺾여 보였음. 착지 판정은 발사 후 최소 0.15초 + `!IsFalling`(LaunchCharacter가 다음 이동 틱에 적용돼 발사 직후엔 아직 걷는 중으로 보임). 착지 검사는 아레나 그리드 우선(라인트레이스만 쓰면 타일 틈 `HexGap`에 빠져 홉이 매번 취소됐음)
- **끌고간후 장판터지는**(DragFan): 부채꼴 줄기 7개 → 맞으면 끌려옴 → 1번 장판(좁게) → 2번(넓게). 아무도 안 맞아도 장판은 터짐. `ExitState`에서 `ReleasePull()`
- **두번긋고 도넛장판**(DonutSlash): 보스 Yaw를 매 틱 고정. 1·2번 슬래시 고리(350~850, 130도 · ±45도 → 정면 40도 겹침, 뒤 140도 안전, 경직 — 공략 그림 비율) → **안밖안 5타**: 3번 작은 도넛(구멍 250~650, 넉다운 — 안에 붙기) → 에어본 꽉 찬 원(0~450, 넉다운, 예고 0.8) **터진 뒤에** 상승(Flying, Lerp — 밖으로) → 정점 대기 → 하강 시작에 4번 큰 도넛(구멍 350~1000, 넉다운+매혹 1, 예고 ≈ `FallDuration` — 다시 안으로). 구멍 반지름 < `AirborneRadius`여야 성립. 전부 RingCount 1. 4번만 매혹
- **전방향 하트발사**(HeartBurst): 2초 머리 위 하트 예고(보스 정지) → 2초간 0.3초마다 2~6개를 0~360 랜덤 각도로. 맞으면 데미지 + 3초 기절 + 발밑 오염 게이지(`AEchidnaPoopMarkActor`, 기절 끝난 뒤 시작, 다 차면 **비활성** 오염 장판)
- **백스탭 후 하트발사**: 백스텝 중 매 틱 플레이어 바라봄 → 착지 무렵 그 시점 방향 기준 부채꼴 4개(-30/-10/+10/+30, 양 끝이 경계). 기절 + 매혹(오염 게이지 아님). 발사와 동시에 같은 방향 부채꼴 장판(하트 벌어짐 + 20도, 1200, 예고 1초) → 한 번에 폭발(RingCount 1, 기본 넉다운) — 하트 적중과 무관
- **되돌아오는 구체**(ReturningOrb): 조준 1 → 1.2초 뒤 재조준 2 → 백스텝 → 고정 조준 부채꼴 4, 총 6개 전부 귀환 시 종료(`MaxWaitDuration` 10초 안전장치)
- **정면 리본**(Ribbon): 2갈래 리본(데미지 0, 기절 + 매혹 — 겹쳐도 1스택: 먼저 걸린 기절이 신호) → 맞음: 보스 중심 원형 장판(넉다운) / 빗나감: 리본 끝(`RibbonEndLocation`, 실제 `TetherRange`)으로 보간 이동 → 재조준 2차 → 맞으면 원형 / 빗나가면 좌측 전방 호(`ArcYawOffset` 음수 = 좌측). `Ribbon|Override`는 `Activate()` 전에 대입
- **뒤로 빠지며 연꽃 + 부채꼴**(RetreatLotus, 그림 9번): 사이클마다 플레이어 조준 고정 → 도착 지점(뒤로 `SlideDistance` 600, 아레나 밖이면 줄임) 기준 정면 부채꼴 예고(120도·1400, 예고 = `SlideDuration`) → 보스가 그 방향을 본 채 1초간 ease-in-out으로 뒤로 미끄러짐(Flying + `SetActorLocation`, 가운데서 60cm 뜸) → 그동안 주변 250~700에 연꽃 6개가 고르게 피고 각각 1초 뒤 폭발(랜잡 연꽃 방식) → 도착 순간 부채꼴 판정. 사이클 사이 `CycleRestDuration` 1.5초 쉬며 플레이어 쪽으로 `RestWalkDistance` 200 걸음(플레이어 300 앞에서 멈춤, 0이면 제자리). `RepeatCount` 2. 전부 경직 + 매혹 1. 슬라이드 시간은 광폭화 배율을 곱해 부채꼴 예고와 맞춤
- **육각 리본**(HexRibbon): 보스는 시작 방향 그대로(조준·회전 없음). 1·2번 둘 다 "앞으로 나란히" 오른손 위치(`HandForwardOffset`·`HandSideOffset`)에서 시작 — 1번 정면으로 세로 → 1초 → 2번 왼쪽으로 가로 → 1초 → 육각형(꼭짓점 정면 기준 ±30·±90·±150, 반지름 600) 변을 왼쪽 → 왼쪽 뒤 → 뒤 → 오른쪽 뒤 → 오른쪽 앞 → 앞 → 왼쪽 앞 순서로 `HexEdgeInterval` 0.25초 연속(3~8번), 변마다 그 변이 **끝나는** 꼭짓점에서 바깥 리본. 1·2번 경직, 육각형·바깥 리본 넉다운. 간격은 광폭화 배율 적용. 판정 연출은 분홍 줄기 대신 `StrikeVFX`(기본 `NS_EchidnaRetreatFan`)를 리본 범위에 `SpawnRectArea` — 리본 액터에 `StrikeVFX`가 있으면 줄기 메시를 안 씀
- **장풍**(PalmBlast): `TrackDuration` 1초 동안 플레이어 쪽으로 `TrackTurnRate`(60도/초)로 따라 돎 — 가까이서 옆으로 뛰면 아슬아슬하게 빠져나가는 속도. 각도 차가 20도를 넘으면 빨라져 90도 이상에서 `TrackCatchUpRate` 400도/초(정반대에서 시작해도 약 0.9초 안에 정면) 추적 동안 판정 직사각형이 게이지 예고(`AEchidnaGaugeRectActor` — PMC 배경+채움, 다른 예고와 같은 빨강, 표시만, Task가 매 틱 위치·진행도 갱신)로 보스 쪽부터 차오름. 끝나는 순간 그 방향 직사각형(1500×500) 데미지 + 넉다운 `HopScale` 3(멀리) — 진행 방향으로 아레나 타일이 이어지는 거리를 재서 그 안에 착지하게 HopScale을 줄임(거리 ≈ Strength·Settle·H^1.5). 연출은 플레이어 돌풍 `NS_Gust`를 `User.Color_*` 6개만 런타임에 핑크로 덮어씀(에셋 복제 없음, 밝기 × `BlastVFXBrightness`, 알파는 1 이상 — 원래 알파 0.5 연기 때문에 반투명해 보였음)
- **카운터**(Counter Pattern): 정지 + 플레이어 바라봄 → 0.75초 카운터 창(청백 발광) → 성공 시 `GroggyDuration` 2.5초로 그로기를 다시 걸고(보스 공용 `CounterGroggyDuration` 5초 대신) 끝까지 Running / 실패 시 본체 숨김(충돌 끔 → **Flying 필수**, 안 하면 바닥을 뚫고 떨어짐) + 분신 4개(`ASkeletalMeshActor` + 보스 메시·머티리얼, `SetLeaderPoseComponent`)가 정면 기준 + 모양 돌진(갈래 끝은 아레나 타일 안으로 줄임) → 분신 사라지고 랜덤 갈래 끝에 본체 등장, `ReturnDelay` 뒤 중심으로 돌진. 판정은 Task가 매 틱 지나간 선분 대 플레이어(반폭 `DashHitRadius`), 갈라질 때 1회·돌아올 때 1회, 데미지 + 넉다운(돌진 방향으로). 지나간 길(폭 = 판정 폭)에 `NS_EchidnaRetreatFan`을 `SpawnRectArea`로 한 자리에 한 번만 이어 깔고, 버스트형(1초 뒤 꺼짐)이라 `TrailHoldTime` 0.3초 뒤 `SetPaused(true)`로 멈춰 유지 → 본체가 돌아오면 재생을 풀고 `Deactivate`(주기적으로 다시 까는 방식은 잔상이 반복돼 보여 폐기)

## 큰 패턴
- **똥장판**(정산 25·75, `PoopPattern`): 보스 정지, 카메라 1500, 오염 전부 활성. 플레이어 발밑 5초 게이지(`AEchidnaPoopMarkActor`, 다 차면 그 타일 **비활성** 오염 — 활성이면 즉시 매혹이 쌓였음) + 4초 뒤 추적 장판(`AEchidnaPoopBeamActor`, 직사각형 + 보스 원, 8도/초 추적 → 보스 쪽부터 게이지 차오름 → 원 먼저, 직사각형은 8칸 순차 폭발, 한 캐릭터 1회, 넉다운). `BeamLength` 4000(맵 끝과 끝). Exit에서 오염 비활성·카메라 복구
- **랜잡**(광폭화 460초 = 7분 40초, `RandomGrab`): 대사 → 3초 뒤 화면 연기 + 오염 활성 → 앞 장판 꽃이 다 나오면 플레이어 타일에 다음 파란 장판(`AEchidnaFlytrapZoneActor`, 같은 타일 중복 불가, 총 5개, 1초 차오름) → 파리지옥 꽃은 패턴 끝까지 남고 그 타일/이후 밟으면 먹힘(최대 HP × 0.9 Fixed + `SetHeldByPattern`). 연기 순간 작은 연꽃 10~15개(서로 다른 타일, 1~2초 뒤 핌 → 2초 퓨즈 → 반지름 110 폭발 5000 + 경직 + 매혹, FanZone 재사용·예고 메시 투명). 장판 단계 15초 상한. Exit에서 꽃 `Dismiss`, 붙잡힘 해제, 연기·오염 정리
- **그네**(광폭화 220초 = 3분 40초, `Swing`): 대사 3초 → 보스 사라짐 → 외곽 변 가운데 타일(꼭짓점 제외 12칸) 중 랜덤에 아레나 중심 보고 등장 → 3초 뒤 그네 장판(`AEchidnaSwingZoneActor`, 반지름 900 안만 안전, 바깥 최대 HP × 10) → 폭발 직후 연기 + 오염 활성 + 사슬(`AEchidnaSwingChainActor`, 보스가 선 변과 마주보는 반대 변 타일 4개(꼭짓점 포함, 큐브 좌표에서 보스의 |축|=R인 축이 -값인 외곽 타일) 노란 테두리 중 하나를 7초 안에 밟으면 파훼 — 꼭짓점 등장은 변이 두 개라 제외, 실패 시 매혹 3스택) + 나비 8마리(`AEchidnaButterflyActor`, 닿으면 10초 기절 후 소멸, 기절 중엔 통과). `ButterflyClass`가 네이티브로 할당돼 있음 — BP 값을 쓰려면 `BP_EchidnaButterfly`로
- **거울잇기**(정산 50·100, `MirrorLink`): 보스 사라짐 + 대사 + 시점 1700 → 파란 테두리 외곽 타일에 거울(`AEchidnaLinkMirrorActor`, 지름 `MirrorDiameter` 240·바닥에서 `MirrorFloatHeight` 60 띄움), 안쪽 타일에 보스. 5초 추적 후 플레이어 붙잡힘 + 빛 덩어리가 **타일 한 칸씩**: 1칸째 = 거울 정면(±30도) 이웃 타일에 플레이어가 있어야, 2칸째 = 플레이어 옆 칸에 보스가 있어야 성공. 실패 = 전 타일 빨간 점멸(`SetAllTilesDangerFlash`) + 최대 HP × 70%(`FailMaxHPRatio`, 고정 피해). Exit에서 보스 보이게, 파란 테두리 다시 뽑기(직전 외곽 제외). 성공 보상 없음
- **거울 카운터**(217줄, `MirrorCounter`): 보스 사라짐, 카메라 1500 + 회전 오버라이드(Pitch -30, Yaw = 출발 변 바깥 법선 — 변은 진입 시 1회 결정, 4웨이브 같은 변). 시네마틱(`bPlayIntro`): 연출 카메라 줌인 → 맵 안쪽 연출 줄(`SetupDisplayRow`)이 돌면서 솟음 → 줌아웃 + 오른쪽부터 빠져나감 → 웨이브 시작. **정사각형 구역**(타일 간격 D 단위, 앞 2.33·뒤 2.03·좌우 2.1 — 유저 그림): 앞 선에 줄이 나타나 뒤 선까지 전진, 좌우 변엔 줄 거울과 같은 크기 유리 거울이 맞닿아 늘어선 기둥 벽(장식), 네 변 투명 벽(`AEchidnaBarrierActor`, Pawn만 Block·WorldStatic이라 돌풍 오버랩·클릭 무시). 뒤 선엔 `BackWallRayVFX`(`NS_EchidnaBackWallRays` = `NS_MagicalGlowRays` 복제 + 파티클 색 HDR 분홍 — 원본은 흰색 가산 스프라이트 1개라 밝은 타일 위에서 안 보였음, 바닥에서 `BackWallVFXHeight` 150 띄움·Scale 2)를 가로로 150 간격 — 뒤 벽 액터에 붙여 벽과 함께 사라짐. **이 NS는 Loop Once(약 2초)라 `BackWallVFXRespawnInterval` 1.2초마다 새로 깖**(한 번만 깔면 벽 생기는 순간 카메라 전환 중에 끝나 안 보였음). 시작 때 구역 밖이면 플레이어를 가운데로 옮김. 시네마틱 줄도 앞 선에
  - 웨이브(`AEchidnaMirrorWallActor`) 12초마다 4개: 거울 7개(정사각형 폭에 맞닿게 `FitMirrorSize` — 폭 = 줄 폭/7, 키는 `MirrorHeight:MirrorWidth` 비율 유지. 속도 `MoveSpeed` 450 — `BP_EchidnaMirrorWall`에서 조절), 랜덤 1개가 카운터 거울(좌우 반전 초상화 + 청백 발광). 거울 콜리전 QueryOnly·WorldDynamic·채널 전부 Ignore(토네이도 오브젝트 오버랩에만). 몸통에 닿으면 줄 앞으로 밀려남(매 틱 sweep으로 거울 앞면 앞에 둠, 넉다운·데미지 없음 — `MirrorHitDamage` > 0이면 1회 데미지), 지나간 자리 불길(0.5초마다, 여러 줄 겹쳐도 `TryConsumeTickDamage("MirrorWallFire")`로 1회). 카운터 성공 → 쓰러짐 + 불길 꺼짐 + 시전자 카운터 스킬 쿨 초기화. 뒤 선 도착 = 실패: 거울만 사라지고 불길은 패턴 끝까지 남음(`VanishRow(true)`), 이미 나와 있는 줄은 `RemoveCounterMirror`(일반 거울로), 다음 줄은 안 나오고 남은 줄이 끝나면 패턴 종료(그로기 없음)
  - 패턴 동안 카운터 스킬 쿨타임 5초(`SetCounterSkillCooldownOverride`). **같은 프레임 `StartCooldown` 덮어쓰기 함정**: 돌풍은 Execute 안에서 판정 → 그 뒤 쿨타임 시작이라 `CounterResetFrame = GFrameCounter`로 같은 프레임은 건너뜀
  - 끝: `CleanupPattern()` → `bGroggyOnlyOnSuccess`(true)면 모든 줄을 카운터쳤을 때만(마지막 줄만 성공은 실패 — 놓친 줄 뒤엔 카운터 거울이 없음) 그 줄의 카운터 거울 자리(`GetCounteredMirrorLocation`, 카운터 순간 기록)에 보스가 줄 진행 방향을 보고 나타나 5초 그로기(Running 유지), 잠금 해제는 ExitState. 실패 페널티·갈급 디버프 미구현

## 0줄 발악 페이즈 (`AEchidnaFinalPhaseActor` 진행자)
- **StateTree 밖에서 돈다**: HP가 0 이하로 깎이는 데미지에서 처치 대신 `BeginFinalPhase`(`bUseFinalPhase`, 끄면 즉시 처치) — HP 0 고정(이후 데미지·미터·아이덴티티 무시), 처치와 같은 정리(`StopBossLogic` → 진행 Task ExitState), 광폭화·정산 정지, 진행자 스폰. 결과는 `CompleteFinalPhase(성공, 사유)` — 성공 = `HandleDefeated`(클리어), 실패 = `OnRaidFailed(사유)` → 컨트롤러 `BeginDefeat`(사망과 같은 패배 화면, 문구만 다름)
- 단계: Cutscene(자리표시 — 무적+붙잡힘+줌인+대사) → MirrorShuffle → Platform → Clash. 진행자는 실시간(CustomTimeDilation 1)
- **거울 섞기**: 보스 숨김(충돌 끔 + Flying) → 가운데 타일에 큰 거울 + 외곽 원형 8개(`AEchidnaShuffleMirrorActor`, 12시부터 시계 방향, 초상화 없이 똑바로 서서 가운데 거울을 마주 봄 — 가운데 거울만 카메라 쪽). 카메라는 패턴 동안 3/4 시점(`ShuffleCameraPitch` -40, 암 2000 — 로아 거울 무력화 화면). 라운드: 랜덤 외곽 1개 빨간 발광 1.5초 → **가운데 거울을 초상화가 5번 가로지름**(화면 왼→오 = +1, 오→왼 = -1, 인물이 치우친 쪽에서 들어옴 — 01(오른쪽 치우침)은 오→왼, 02(반전)는 왼→오 — 합계만큼 빨간 거울이 시계 방향(+)으로 이동, 8칸 순환. 거울 타원 밖은 매 틱 다각형 클리핑으로 잘라 그림) → 1.5초 → 두 거울 가라앉았다 올라옴(거울은 제자리, **빨간 여부만** `RedIndex`가 옮겨 감, 직전과 같은 쌍 금지) × 3 → 카운터 대기 8초(외곽 8개 전부 파란 발광 `SetCounterReady` + 가운데 거울 위 `8s` 카운트다운 `UWorldCountdownWidget`). 정답 = 성공 누적(2번이면 보스 등장), 오답·시간 초과 = 그 거울(시간 초과면 정답 거울)에서 저스트 가드 → 새 라운드(성공 수 유지 — 2번 성공할 때까지 무한 반복, 체력이 버티는 한)
  - 카운터는 돌풍 토네이도 오버랩 → `ICounterable`(방향 무관). 거울은 진행자가 `SetAcceptCounter`로 켤 때만, 가라앉는 중엔 무시. 패턴 동안 카운터 스킬 쿨 5초 오버라이드, 정답이면 쿨 초기화
- **저스트 가드**(`AEchidnaJustGuardZoneActor`): 노란 원(`JustGuardRadius` 1300 — 거울 섞기 카메라에 다 담기는 크기, 판정은 위치 무관)이 중심에서 차오르는 게이지 `FillDuration` 2.5초 → 다 차는 순간 ±0.15초 안에 G + 커서가 "플레이어→거울" ±30도(거울 200cm 안이면 방향 무시). 한 장판에 한 번만 — 일찍·방향 틀림은 그 자리에서 실패 확정(피해는 게이지가 다 찰 때), 안 누르면 늦은 창 끝에 실패. 실패 = 최대 HP × 80% Fixed. 판정 창 진입 시 게이지 색이 밝아짐. 로그 `[JustGuard] G 입력 — 타이밍 ±ms, 방향 도`로 오차 튜닝
- 2번 성공 → 1초 뒤 거울 전부 가라앉고 보스가 마지막 정답 거울 자리에서 맵 가운데를 보고 등장 → 정면 `PlatformFrontDistance` 350에 격돌 발판(`AEchidnaClashPlatformActor`, `[G] 격돌`, 충돌 없음) → `PlatformTimeLimit` 20초 안에 G 안 누르면 실패
- **격돌 QTE**(`AEchidnaClashQTEActor` + `UClashQTEWidget` C++ 트리, ZOrder 25): 타이밍 1개 → 2개 → 3개 → 연타 = 총 7번. 키는 Q W E R A S D F 랜덤(한 단계 안 연속 같은 키 금지). 노트 링이 `NoteApproachTime` 1.1초 동안 줄어 키 원에 겹치는 순간 ±0.15초(±0.06 PERFECT), 노트 간격 0.55초, 판정 시각보다 0.35초 넘게 이른 입력은 무시. **하나라도 놓치면 격돌 실패**(`MaxMisses` 0). 연타 = 서로 다른 키 2개 번갈아, 맞는 키 +0.06·초당 -0.18·틀린 키 -0.03, 5초 안에 가득. 게이지가 찰수록 카메라 900 → 600. 결과 글자 1.2초 뒤 진행자가 `Finish`
  - 키는 컨트롤러 `InputKey` 오버라이드가 Enhanced Input보다 먼저 QTE에 넘김 — 후보 키는 먹어서 스킬로 안 샘
- G 입력 순서(컨트롤러 `OnInteractKey`): 저스트 가드 장판 → 격돌 발판 → NPC. 라벨("Just Guard!"·"Counter!")은 `AEchidnaBoss::SpawnLabelText`
- 발악 중 플레이어가 죽으면 진행자는 멈추고 일반 사망 흐름

## 보스 맵 흐름 (컨트롤러 쪽은 `Source/LoA/CLAUDE.md`)
- 인트로: 페이드 인 → `ACameraActor`가 보스 정면 높은 곳 → 얼굴 앞 ease-in-out → 레터박스·타이틀 → 플레이어 카메라 블렌드 → `StartCombat()`. 연출 중 붙잡힘(폰이 늦게 빙의될 수 있어 매 틱), HUD 숨김, 뷰 타깃 매 틱 재설정, 카메라는 `SetLifeSpan(0.5)`로 정리
- 사망/클리어/중단 → 정비소 복귀. 레이드 보상은 `Source/LoA/Item/CLAUDE.md`
