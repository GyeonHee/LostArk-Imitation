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

## StateTree `ST_Echidna` 규칙
- **MCP로는 읽기만 가능**(`StateTreeTools.get_root_states/get_children/get_tasks`) — 배치·값 변경은 에디터에서 유저가 직접. Task 인스턴스에 한 번 입력한 값은 C++ 기본값을 바꿔도 안 바뀜(노란 화살표 → Reset to Default)
- 새 Task/Enum/UCLASS 추가는 Live Coding으로 노드 목록에 안 뜰 수 있음 → 에디터 닫고 풀 빌드. 인스턴스 데이터 레이아웃 변경도 Live Coding 크래시 위험 — 필드를 지우기보다 의미를 재해석한 선례 있음(`PullStrength`)
- **짤패턴**: `SmallPatternRotation` 안에 State마다 **완료 조건이 있는 Task 하나만**, On State Completed → 밖의 형제 `Cooldown`(Wait 3초 + `FStateTreeTask_EchidnaPatrol`, NavMesh 필요). Wait를 패턴 State에 나란히 넣으면 "Any" 정책으로 먼저 끝나버림(버그 전례). 완료를 안 리턴하는 Task만 두면 영원히 안 끝남
- **큰 패턴**(시간·정산·줄): Root 자식 State + Task 하나, **Root On Tick Transition**(Enter Condition 쓰지 말 것), On State Completed → Patrol
  - 조건: `Boss Enrage Time Reached`(RemainingSeconds) / `Boss Settlement Gauge Reached`(GaugePercent) / `Boss Line Threshold Reached`(TriggerLine, **TotalLines보다 작아야**)
  - 큰 패턴 Task는 진입 시 `MarkBigPatternTriggered()`(Task PatternName과 **State 이름 둘 다** 표시 — 이름 불일치로 매 틱 재진입·무한 반복 버그가 두 번 남) + `SetActiveTimedPattern`, Exit에서 해제. 조건은 큰 패턴 진행 중이거나 `WaitWhileStatesActive`(`SmallPatternRotation`) 활성이면 false → 짤패턴이 끝난 뒤 시작. `재진입` 경고 로그 = 이름 불일치
- 빠른 테스트: 시간 패턴은 조건 `RemainingSeconds`를 크게(예: 530 → 시작 10초 뒤), 정산 패턴은 `BP_Echidna`의 `SettlementNaturalAmount`를 크게(예: 20). 보스 인트로 없이 바로 싸우려면 레벨 보스의 `bWaitForIntro` 끄기
- 배치 현황(2026-09-28 확인): 짤패턴 9종 + 큰 패턴 5종 배치됨. **`Echidna Counter Pattern`(보스 본체 카운터)은 미배치**. Donut Slash의 `FanZoneClass`(3번)가 `BP_EchidnaFanZone_Stagger`로 들어가 있음(설계는 넉다운 `BP_EhidnaFanZone`)

## 공용 액터
- **`AEchidnaFanZoneActor`**(부채꼴/고리/원): 예고(`TelegraphDuration`, 전체 범위 표시) → `RingCount`등분 고리를 `RingInterval`마다 안→밖 확장, 고리마다 그 구간만 1회 판정. `FanAngle 360` = 원/도넛, `FanInnerRadius` > 0 = 고리. 단발 판정이 필요한 패턴은 `RingCount 1`을 강제로 넘김. CC는 BP 기본값: `bApplyKnockdownOnHit`(우선) / `bApplyStaggerOnHit` / `bApplyCharmGaugeOnHit`
  - BP: `BP_EhidnaFanZone`(오타 그대로, 넉다운) / `BP_EchidnaFanZone_Stagger`(경직) / `BP_EchidnaFanZone_KnockdownCharm`(넉다운+매혹)
- **`AEchidnaTetherActor`**(줄기/리본): 예고 → `PerformSnap` 판정 + 핑크 줄기 뻗었다 빨려 들어감(`StrikeMeshComp`). 스위치 `bApplyPullOnHit`/`bApplyStunOnHit`/`CharmGaugeAmount`. 끌기면 `PullResolveDelay`(1.2초, 플레이어 `PullHoldDuration`+드래그보다 길게) 뒤 `FinishSnap`에서 완료·소멸 예약. `BP_EchidnaTether`(끌기) / `BP_EchidnaRibbon`(기절+매혹, 길이 1600)
- **`AEchidnaHeartActor`**: Launch 전엔 제자리(예고 마커 겸용), 후엔 직선 비행. 첫 대상 1명만, 맞으면 소멸. 데미지 + 기절 + 매혹(이미 기절 중이면 매혹 생략 — `ApplyStun` **전에** 검사). `SetSpawnPoopMarkOnHit(true)`면 매혹 대신 발밑 오염 게이지. 메시는 PMC 압출 하트(세운 카드, 비행 중 회전) — 컴포넌트 타입을 바꾼 이력이 있어 `BP_EchidnaHeart` 재컴파일 확인 필요
- **`AEchidnaMirrorActor`**(4거울·8거울·유도): Tracking(장판이 플레이어 추적) → Firing(방향 고정, 1/3초마다 데미지 틱 + 넉다운, `MaxDamageTicks = floor(Firing/Interval)+1`). 추적은 **따라잡기 방식**(각도 차 25도 이하 8도/초 ~ 100도 이상 70도/초, SmoothStep) — 걸어서 피하려면 각속도 × 3000cm < 600cm/s → 약 11도/초 이하. 레이저 VFX `NS_EchidnaMirrorLaser`(번개 줄기만, 로컬 +X로 뻗음, `User.BeamSize`/`User.BeamLife`) — **C++ 타이머가 0.12초마다 1회 재생 줄기를 띄우고 매 틱 방향 맞춤**(나이아가라 무한 반복은 1초 만에 꺼지는 문제). 미활성 대기 거울은 `ZoneMeshComp` 숨김
- **`AEchidnaOrbActor`**: Outgoing `MaxRange` → Returning은 발사 지점을 관통해 반대편 `-MaxRange`까지(보스 뒤도 위험). 왕복 구간마다 `AlreadyHit` 초기화(최대 2회 피격), 맞아도 소멸 안 함. Task `Orb|Override`는 스폰 후 `ApplyOverrides()`
- 타일 플래그·카메라: `ALoACharacter::SetCameraZoomOverride/SetCameraRotationOverride`, 화면 연기 `SetScreenFog`(출처 "Pattern"), 대사 `ShowPatternDialogue` → `ALoAPlayerController::ShowBossDialogue`

## 짤패턴 (`EchidnaBossStateTreeUtility`)
- **4거울**: 보스 정면 기준 대각 4방향 거울 동시 스폰, 전부 끝날 때까지 Running
- **8거울**: 4개씩 두 차례(나중 4개는 미리 스폰 후 늦게 Activate) + 개인 유도레이저가 `GuidedSpawnDelay`(2초) 뒤 보스 위치에서 스폰(즉시 스폰은 피할 틈이 없었음)
- **뒤로 빠지며 좌우장판**(RetreatFan): 조준 방향은 **패턴 시작 시 1회 고정**(`BaseAimRotation`), 왼쪽(-`FanYawOffset` 32) → 오른쪽(+32) 부채꼴(FanAngle 70 → 가운데 6도 겹침). 장판 스폰 시 보스도 그 방향으로 회전. 고리 확장 시작(`HasStartedExploding`) 순간 장판당 1회 후방 홉(`LaunchCharacter`)
- **끌고간후 장판터지는**(DragFan): 부채꼴 줄기 7개 → 맞으면 끌려옴 → 1번 장판(좁게) → 2번(넓게). 아무도 안 맞아도 장판은 터짐. `ExitState`에서 `ReleasePull()`
- **두번긋고 도넛장판**(DonutSlash): 보스 Yaw를 매 틱 고정. 1·2번 반원 슬래시 고리(350~850, ±75도 → 정면 30도 겹침, 바로 뒤 30도 안전, 경직) → 3번 작은 도넛(넉다운) **터진 뒤에** 상승(Flying, Lerp) → 정점 대기 → 하강 시작에 4번 외곽 도넛(넉다운+매혹 1, 예고 ≈ `FallDuration`). 전부 RingCount 1. 1~3번 매혹 없음
- **전방향 하트발사**(HeartBurst): 2초 머리 위 하트 예고(보스 정지) → 2초간 0.3초마다 2~6개를 0~360 랜덤 각도로. 맞으면 데미지 + 3초 기절 + 발밑 오염 게이지(`AEchidnaPoopMarkActor`, 기절 끝난 뒤 시작, 다 차면 **비활성** 오염 장판)
- **백스탭 후 하트발사**: 백스텝 중 매 틱 플레이어 바라봄 → 착지 무렵 그 시점 방향 기준 부채꼴 4개(-30/-10/+10/+30, 양 끝이 경계). 기절 + 매혹(오염 게이지 아님)
- **되돌아오는 구체**(ReturningOrb): 조준 1 → 1.2초 뒤 재조준 2 → 백스텝 → 고정 조준 부채꼴 4, 총 6개 전부 귀환 시 종료(`MaxWaitDuration` 10초 안전장치)
- **정면 리본**(Ribbon): 2갈래 리본(데미지 0, 기절 + 매혹 — 겹쳐도 1스택: 먼저 걸린 기절이 신호) → 맞음: 보스 중심 원형 장판(넉다운) / 빗나감: 리본 끝(`RibbonEndLocation`, 실제 `TetherRange`)으로 보간 이동 → 재조준 2차 → 맞으면 원형 / 빗나가면 좌측 전방 호(`ArcYawOffset` 음수 = 좌측). `Ribbon|Override`는 `Activate()` 전에 대입
- **카운터**(Counter Pattern): 정지 + 플레이어 바라봄 → 2초 카운터 창 → 성공 시 그로기 끝까지 Running / 실패 시 종료(후속 공격 없음)

## 큰 패턴
- **똥장판**(정산 25·75, `PoopPattern`): 보스 정지, 카메라 1500, 오염 전부 활성. 플레이어 발밑 5초 게이지(`AEchidnaPoopMarkActor`, 다 차면 그 타일 **비활성** 오염 — 활성이면 즉시 매혹이 쌓였음) + 4초 뒤 추적 장판(`AEchidnaPoopBeamActor`, 직사각형 + 보스 원, 8도/초 추적 → 보스 쪽부터 게이지 차오름 → 원 먼저, 직사각형은 8칸 순차 폭발, 한 캐릭터 1회, 넉다운). `BeamLength` 4000(맵 끝과 끝). Exit에서 오염 비활성·카메라 복구
- **랜잡**(광폭화 460초 = 7분 40초, `RandomGrab`): 대사 → 3초 뒤 화면 연기 + 오염 활성 → 앞 장판 꽃이 다 나오면 플레이어 타일에 다음 파란 장판(`AEchidnaFlytrapZoneActor`, 같은 타일 중복 불가, 총 5개, 1초 차오름) → 파리지옥 꽃은 패턴 끝까지 남고 그 타일/이후 밟으면 먹힘(최대 HP × 0.9 Fixed + `SetHeldByPattern`). 연기 순간 작은 연꽃 10~15개(서로 다른 타일, 1~2초 뒤 핌 → 2초 퓨즈 → 반지름 110 폭발 5000 + 경직 + 매혹, FanZone 재사용·예고 메시 투명). 장판 단계 15초 상한. Exit에서 꽃 `Dismiss`, 붙잡힘 해제, 연기·오염 정리
- **그네**(광폭화 220초 = 3분 40초, `Swing`): 대사 3초 → 보스 사라짐 → 외곽 랜덤 타일에 아레나 중심 보고 등장 → 3초 뒤 그네 장판(`AEchidnaSwingZoneActor`, 반지름 900 안만 안전, 바깥 최대 HP × 10) → 폭발 직후 연기 + 오염 활성 + 사슬(`AEchidnaSwingChainActor`, 중심 대칭 타일 `(-q,-r)` 노란 테두리를 5초 안에 밟으면 파훼, 실패 시 매혹 3스택) + 나비 8마리(`AEchidnaButterflyActor`, 닿으면 10초 기절 후 소멸, 기절 중엔 통과). `ButterflyClass`가 네이티브로 할당돼 있음 — BP 값을 쓰려면 `BP_EchidnaButterfly`로
- **거울잇기**(정산 50·100, `MirrorLink`): 보스 사라짐 + 대사 + 시점 1700 → 파란 테두리 외곽 타일에 거울(`AEchidnaLinkMirrorActor`), 안쪽 타일에 보스. 5초 추적 후 플레이어 붙잡힘 + 빛 덩어리가 **타일 한 칸씩**: 1칸째 = 거울 정면(±30도) 이웃 타일에 플레이어가 있어야, 2칸째 = 플레이어 옆 칸에 보스가 있어야 성공. 실패 = 전 타일 빨간 점멸(`SetAllTilesDangerFlash`) + 최대 HP × 10. Exit에서 보스 보이게, 파란 테두리 다시 뽑기(직전 외곽 제외). 성공 보상 없음
- **거울 카운터**(217줄, `MirrorCounter`): 보스 사라짐, 카메라 1500 + 회전 오버라이드(Pitch -30, Yaw = 출발 변 바깥 법선 — 변은 진입 시 1회 결정, 4웨이브 같은 변). 시네마틱(`bPlayIntro`): 연출 카메라 줌인 → 맵 안쪽 연출 줄(`SetupDisplayRow`)이 돌면서 솟음 → 줌아웃 + 오른쪽부터 빠져나감 → 양쪽 유리 거울 기둥(장식) → 웨이브 시작
  - 웨이브(`AEchidnaMirrorWallActor`) 12초마다 4개: 거울 7개(폭 = 아레나 변 길이 - 양쪽 100 — 넓으면 끝 거울이 벽 너머로 가서 못 침), 랜덤 1개가 카운터 거울(좌우 반전 초상화 + 청백 발광). 거울 콜리전 QueryOnly·WorldDynamic·채널 전부 Ignore(토네이도 오브젝트 오버랩에만). 몸통 닿으면 1회 데미지 + 넉다운, 지나간 자리 불길(0.5초마다, 여러 줄 겹쳐도 `TryConsumeTickDamage("MirrorWallFire")`로 1회). 카운터 성공 → 쓰러짐 + 불길 꺼짐 + 시전자 카운터 스킬 쿨 초기화. 반대편 도착 = 실패, 거울·불길 같이 사라짐(`VanishRow`)
  - 패턴 동안 카운터 스킬 쿨타임 5초(`SetCounterSkillCooldownOverride`). **같은 프레임 `StartCooldown` 덮어쓰기 함정**: 돌풍은 Execute 안에서 판정 → 그 뒤 쿨타임 시작이라 `CounterResetFrame = GFrameCounter`로 같은 프레임은 건너뜀
  - 끝: `CleanupPattern()` → `bGroggyOnlyOnSuccess`(true)면 마지막 줄을 카운터쳤을 때만 5초 그로기(Running 유지), 잠금 해제는 ExitState. 실패 페널티·갈급 디버프 미구현

## 보스 맵 흐름 (컨트롤러 쪽은 `Source/LoA/CLAUDE.md`)
- 인트로: 페이드 인 → `ACameraActor`가 보스 정면 높은 곳 → 얼굴 앞 ease-in-out → 레터박스·타이틀 → 플레이어 카메라 블렌드 → `StartCombat()`. 연출 중 붙잡힘(폰이 늦게 빙의될 수 있어 매 틱), HUD 숨김, 뷰 타깃 매 틱 재설정, 카메라는 `SetLifeSpan(0.5)`로 정리
- 사망/클리어/중단 → 정비소 복귀. 레이드 보상은 `Source/LoA/Item/CLAUDE.md`
