# LoA 프로젝트 — Claude 컨텍스트

## 프로젝트 개요
- **장르**: 탑다운 ARPG (로스트아크 모작)
- **엔진**: Unreal Engine 5.7
- **이동 방식**: 마우스 클릭 이동 (NavMesh 없이 직접 RequestDirectMove)
- **언어**: C++ + Blueprint 혼용, C++ 로직 우선

## 핵심 아키텍처

### 입력 시스템
- `IMC_LoADefault` — 기본 이동/클릭 매핑
- `SkillMappingContext` — 스킬 키 (Q/W/E/R/A/S/D/F)
- 스킬 슬롯: 0=Q, 1=W, 2=E, 3=R, 4=A, 5=S, 6=D, 7=F, 8=기본공격, 18=대시, 19=즉시 기상 (SkillManagerComponent 슬롯 시스템에 얹혀있을 뿐 실제 스킬은 아님 — 아래 "넉다운 시스템" 참조)

### 주요 클래스
| 파일 | 역할 |
|---|---|
| `LoAPlayerController` | 이동, 대시억제, 스킬입력, UI토글 |
| `LoACharacter` | HP/MP스탯, SkillManager 보유 |
| `SkillManagerComponent` | 스킬슬롯 관리, 쿨타임, 스킬트리 레벨, 자동이동 틱 |
| `HUD_ViewModel` | MVVM — HP/MP 바 |
| `SkillTree_ViewModel` | MVVM — 스킬트리 창 전체 |
| `SkillTreeEntry_ViewModel` | MVVM — 스킬트리 행(Row) 단위 |
| `AHexArena` | 에키드나 2관문 육각형 타일 아레나 + 외곽 벽 |
| `AEchidnaBoss` | 에키드나 보스 — HP-줄 조회, 대형 패턴 발동 추적 |
| `AEchidnaBossAIController` | 에키드나 보스 AI — StateTreeAIComponent 보유 |
| `AEchidnaMirrorActor` | 에키드나 "4거울" 짤패턴 — 추적 장판 + 레이저 발사 |
| `AEchidnaFanZoneActor` | 에키드나 "뒤로 빠지며 좌우장판" 짤패턴 — 예고 후 고리 단위 확장 부채꼴 장판 |

### 스킬 시스템
- `SkillBase` → `SkillInstant` / `SkillCast` / `SkillCharge` / `SkillCombo` / (Hold는 SkillBase 직접 상속)
- `FSkillData` (DataTable 행): SkillName, InputType, ManaCost, Cooldown, Icon, MaxSkillLevel, SkillPointCostPerLevel, DamageCoefficient, HoldMaxTime 등
- `DT_Skills` — 스킬 데이터 테이블

#### 구현된 스킬 클래스
| 클래스 | 타입 | 액터 | 설명 |
|---|---|---|---|
| `SkillDoomsday` | SkillCast | `ADoomsdayMeteor` (`BP_SkillMeteor`) | 종말의 날 — 메테오 낙하 |
| `SkillCheonbeol` | SkillCast | `ALightningStrikeActor` (`BP_LightningStrike`) | 천벌 — 번개 낙하 |
| `SkillBlaze` | SkillInstant | `ABlazeProjectile` (`BP_SkillBlaze`) | 블레이즈 — 발사체 화염 |
| `SkillExplosion` | SkillCast | `AExplosionMeteorActor` (`BP_Explosion`) | 익스플로전 — 캐릭터 앞에서 커서 방향 직선 메테오 |
| `SkillInferno` | SkillInstant | `AInfernoZoneActor` (`BP_SkillInferno`) | 인페르노 — 커서 위치 장판 + 위로 솟구치는 폭발 |
| `SkillFrostCall` | SkillBase (Hold) | `AFrostCallZoneActor` (`BP_FrostCall`) | 혹한의 부름 — 커서 위치 장판 홀딩, 틱 데미지 + 고드름 낙하 |
| `SkillIceArrow` | SkillInstant | `AIceArrowZoneActor` (`BP_IceArrow`) | 아이스 에로우 — 커서 위치 원형 범위, 45도 고드름 낙하 4틱 |
| `SkillGust` | SkillInstant | `AGustTornadoActor` (`BP_Gust`) | 돌풍 — 커서 방향 회전 후 캐릭터 앞 토네이도 존, 틱 데미지 |

#### SkillCast 공통 동작 패턴 (2026-06-16 기준)
- **사거리 초과 시**: `bMovingToRange=true` (bIsActive는 건드리지 않음) → `ForceMoveTo(PendingCastTarget)`
- **OnKeyHeld에서 거리 체크**: 사거리 진입 시 `bMovingToRange=false`, `CancelMovement()`, `Super::OnKeyDown()` 호출 (캐스팅 타이머 시작)
- **OnKeyUp 이동 중 취소**: `bMovingToRange=false`, `CancelMovement()`, return — `bIsActive=false`이므로 HandleKeyUp도 쿨타임 미발동
- **캐스팅 완료/취소 쿨타임**: `bIsActive=true` 상태에서 HandleKeyUp → Cast 타입이면 StartCooldown
- **캐스팅 중 이동 입력**: `OnInputStarted()` → `CancelActiveCastSkill()` → `ForceCancel()` + `StartCooldown()`
- **이동 중 마우스 클릭**: `OnInputStarted()` → `CancelActiveCastSkill()` 에서 `IsMovingToRange()`도 체크하여 `CancelRangeMove()` 호출 (쿨타임 없음)
- `ForceCancel()`: `SkillBase`에 virtual 선언, bIsActive=false + ElapsedTime=0 리셋
- `CancelActiveCastSkill()`: 활성 Cast → ForceCancel+StartCooldown / 범위이동 중 Cast → CancelRangeMove(쿨타임 없음)

#### SkillInstant 사거리 자동이동 패턴 (2026-06-16 기준)
- **사거리 초과 시**: `bMovingToRange=true` → `ForceMoveTo(PendingTarget)` → `SkillManagerComponent::PendingRangeMoveSlot` 등록
- **SkillManagerComponent 틱**: `PendingRangeMoveSlot` 슬롯의 `OnKeyHeld` 호출 → 사거리 진입 감지 → `Execute()` 직접 호출 → `StartCooldown()`
- **원프레스**: 키를 뗀 뒤에도 틱이 계속 처리하므로 한 번만 눌러도 자동이동 후 발동
- **이동 중 마우스 클릭**: `OnInputStarted()` → `CancelPendingRangeMove()` → `CancelRangeMove()` 호출, 슬롯 클리어 (쿨타임 없음)
- **OnKeyUp 처리**: `bMovingToRange=true`면 그냥 return (취소 안 함 — 틱이 처리)

#### SkillBase 가상 함수 (사거리 자동이동 관련)
- `virtual bool IsMovingToRange() const` — 자동이동 중 여부, 각 스킬에서 `return bMovingToRange` 오버라이드
- `virtual void CancelRangeMove(AActor* Owner)` — 이동 취소 (bMovingToRange=false + CancelMovement), 쿨타임 없음

#### SkillManagerComponent 자동이동 필드
- `int32 PendingRangeMoveSlot = -1` — 원프레스 대기 중인 Instant 슬롯 (-1=없음)
- `TickComponent` — PendingRangeMoveSlot 있으면 OnKeyHeld 호출, 도달 시 StartCooldown + 슬롯 클리어
- `CancelPendingRangeMove()` — 슬롯 취소 (쿨타임 없음), OnInputStarted에서 호출
- Cast/Charge 타입은 PendingRangeMoveSlot 미등록 (HandleKeyHeld 경로로만 처리)

#### 범위 VFX 자동 스케일 (DoomsdayMeteor · LightningStrikeActor · InfernoZoneActor)
- **패턴**: 캐스팅 완료 → `CircleShowTime`초 동안 발판 원 VFX → 효과 발동
- **스케일 공식**: `AutoScale = ExplosionRadius / CircleBaseRadius`
- `CircleBaseRadius`: 에디터에서 Scale=1.0으로 재생해 실측 후 BP에서 설정
- Niagara 파라미터는 **개별 이미터 파라미터만** 설정 (Scale_All과 동시 설정 시 제곱되어 너무 커짐)
- 메테오 원 (`NS_MeteorCircle`) 파라미터: `User.Scale_Circle`, `User.Scale_Mesh1`, `User.Scale_Ray`, `User.Scale_Sparks1`, `User.Scale_Sparks2`
- 천벌 원 (`NS_LightningCircle`) 파라미터: `User.Scale_Circle`, `User.Scale_Smoke`, `User.Scale_Sparks1`, `User.Scale_Spiral1`
- 인페르노 원 (`NS_InfernoCircle`) 파라미터: 동일 패턴, CircleBaseRadius BP에서 별도 측정 필요

#### AExplosionMeteorActor (익스플로전)
- 캐릭터 허리 위치(`CharWaistLoc`)에서 커서 방향으로 `SpawnForwardOffset`만큼 앞에 스폰
- `SetActorRotation((Target-Spawn).GetSafeNormal().Rotation())` — VFX 방향 정렬
- 발판 원 VFX → `CircleShowTime` 후 `StartFlying()`: `MeteorVFXComponent` 활성화 + Tick 시작
- `Tick`: 직선이동, `ArrivalThreshold` 이내 도달 시 `Land()` 호출
- `Land()`: 비행 VFX 종료 → `ExplosionVFXSystem` 착탄 위치에 별도 스폰 → `ApplyRadialDamage`
- VFX 구성: `NS_Explosion_Projectile`(비행) + `NS_Explosion_Impact`(폭발) — Mixed_Magic_VFX_Pack/Sperate_VFX

#### AInfernoZoneActor (인페르노)
- 스폰 즉시 `Activate()`: 원 VFX 표시 + `CircleShowTime` 타이머 → `Explode()` 1회 호출
- `Explode()`: `ExplosionVFXSystem`(`NS_Inferno_Impact`) 스폰 + `ApplyRadialDamage`
- `NS_Inferno_Impact`: `NS_Magma_Shot_Impact` 복제 후 AddVelocity Z값을 양수로 수정하여 위로 솟구치게 변경 (Niagara 에디터 작업)

#### SkillFrostCall / AFrostCallZoneActor (혹한의 부름) — 2026-06-22
- `USkillBase` 직접 상속 (Hold 타입, 별도 SkillHold 클래스 없음)
- **키 누름**: 커서 위치에 `AFrostCallZoneActor` 스폰 (`bIsActive=true`, 즉시 1틱 데미지)
- **키 홀드**: `ElapsedTime` 누적 → `HoldMaxTime`(기본 4s) 초과 시 자동 종료
- **키 해제 / ForceCancel**: `StopZone()` → 타이머 클리어 + `LifeAfterStop` 후 Destroy
- **사거리 초과**: 자동이동 없음 — 사거리 경계로 스폰 위치 클램프 (`MaxCastRange = 800cm`)
- `AFrostCallZoneActor`: 데미지 타이머(`DamageTickInterval=0.5s`, `MaxDamageTicks=9`)와 VFX 타이머(`VFXTickInterval=0.2s`, `IciclesPerTick=3`) **독립 동작**
- VFX: `NS_FrostCallCircle`(루핑 원 장판) + `NS_FrostCallIcicle`(고드름 `IcicleSpawnHeight=400cm`에서 낙하)
- `ZoneRadius = 400cm`, `ApplyRadialDamage`

#### SkillIceArrow / AIceArrowZoneActor (아이스 에로우) — 2026-06-22
- `USkillInstant` 상속, 인페르노와 동일한 **원프레스 자동이동 패턴**
- **사거리 안**: `OnKeyDown`에서 즉시 `Execute()`
- **사거리 밖**: `bMovingToRange=true`, `ForceMoveTo()` → `PendingRangeMoveSlot` 등록 → 틱에서 진입 감지 후 발동
- **고드름 각도**: 캐릭터→커서 수평 방향 + 수직 동일 크기 = 정확히 45도 하향
  ```cpp
  FRotator IcicleRot = (-HorizDir + FVector(0,0,-1)).GetSafeNormal().Rotation();
  ```
- **드리프트 보정**: `IcicleHorizontalDrift=500cm` — Niagara 수평 속도로 인한 착지점 오차 상쇄 (IcicleSpawnHeight와 동일값이 45도 기준 정확한 보정)
- `AIceArrowZoneActor`: `TickInterval=0.5s`, `MaxTicks=4` (총 1.5초), `ZoneRadius=350cm`
- 틱당 데미지 = 총 데미지 / MaxTicks (Execute에서 CDO로 MaxTicks 읽어 계산)
- VFX: `NS_IceArrowIcicle` (`IcicleSpawnHeight=500cm`, `IciclesPerTick=5`)

### UI 시스템 (MVVM)
- `WBP_HUD` — HP/MP 바, HUD_ViewModel 바인딩
- `WBP_SkillTree` — K키로 토글, SkillTree_ViewModel 바인딩
- `WBP_SkillTreeRow` — ListView 행, IUserObjectListEntry 구현
- `USkillDragDropOperation` — 스킬 드래그앤드랍

## Blueprint 주의사항
- `BP_LoAPlayerController` EventGraph의 이동 관련 BP 노드 모두 제거됨 (C++와 충돌)
- ViewModel은 PlayerController BeginPlay에서 생성 — 순서: **Initialize → SetViewModel → AddToViewport**
- `SkillTree_ViewModel`은 `BlueprintReadOnly`로 컨트롤러에서 직접 접근 가능
- `HUDWidget`은 `BlueprintReadOnly`로 컨트롤러에서 직접 접근 가능 — WBP_SkillTree OnDrop에서 참조

## WBP_SkillTree 드래그 관련 구조 (2026-06-11)
- 루트 Canvas Panel Visibility: **Visible** (전체 화면 히트 테스트 활성화, 마우스 입력 차단용)
- `SkillTreeBG` (Border): Canvas Panel 직속 자식, 앵커 **좌상단(0,0)**, 명시적 Position 설정
- **On Mouse Button Down**: bOverPanel 체크(`AbsoluteToLocal(GetCachedGeometry(SkillTreeBG))`) → 좌클릭이면 `bIsDragging=true`, `DragMouseStart=AbsoluteToLocal(My Geometry, screenPos)`, `DragWidgetStart=Slot→GetPosition`
- **On Mouse Move**: `CurrentLocal=AbsoluteToLocal(My Geometry, screenPos)` → `SetPosition(DragWidgetStart + CurrentLocal - DragMouseStart)` → **항상 Handled 반환** (Unhandled 반환 시 Enhanced Input이 K키 재평가하여 토글 이중 발동)
- **On Mouse Button Up**: `bIsDragging=false` → Unhandled
- **OnDrop**: WBP_HUD가 아닌 WBP_SkillTree에서 처리 (Z-order상 SkillTree가 위에 있어 HUD OnDrop 미발동)
  - Cast to SkillDragDropOperation → SkillRowName 직접 참조 (EntryViewModel 경유 시 null)
  - SkillSlotBorders 배열 순회 → AbsoluteToLocal+GetLocalSize로 히트 판정 → AssignSkillToSlot

## 스킬 타입별 사거리 동작 (2026-06-22 기준)
| 상황 | Cast 스킬 (종말·천벌·익스플로전) | Instant 스킬 (인페르노·아이스에로우) | Hold 스킬 (혹한의 부름) |
|---|---|---|---|
| 사거리 안, 키 누름 | 캐스팅 타이머 시작 | 즉시 발동 | 즉시 장판 스폰 |
| 사거리 안, 키 홀드 | 타이머 채우는 중 | — | 틱 데미지 지속 |
| 사거리 안, 키 해제 | 취소 (설정 쿨타임) | — | 장판 즉시 제거 |
| 사거리 밖, 키 누름 | 홀드 중에만 사거리 방향 이동 | 한 번만 눌러도 자동이동 | 경계에 장판 스폰 (이동 없음) |
| 사거리 밖, 사거리 진입 | 이동 멈추고 캐스팅 시작 | 즉시 발동 | — |
| 사거리 밖, 키 해제 | 이동 취소, 쿨타임 없음 | 이동 취소, 쿨타임 없음 | — |
| 이동 중 마우스 클릭 | 취소, 쿨타임 없음 | 취소, 쿨타임 없음 | — |

## 스킬 잠금 시스템 (2026-06-23 기준)
- `IsSkillLocked()` — 어느 슬롯이든 IsActive/IsMovingToRange이거나 PostDelay 중이면 잠금
- `QueuedSkillSlot` — 잠금 중 입력된 다음 스킬 슬롯 1개 저장 (덮어쓰기)
- `ReleaseSkillLock(bWithPostDelay)` — 잠금 해제 + PostDelay 타이머 예약
- `OnSkillLockReleased()` — PostDelay 완료 후 큐된 슬롯 자동 발동
- **Instant 완료** → `SkillPostDelay`(기본 0.3s) 후 잠금 해제 → 큐 발동
- **Cast/Charge 완료·취소** → PostDelay 후 잠금 해제 → 큐 발동
- **Hold 완료·취소** → 즉시 잠금 해제 (PostDelay 없음)
- **Hold 활성 중** → 다른 스킬 키 완전 씹힘 (큐 등록조차 안 됨)
- **사거리 이동 취소** → 즉시 잠금 해제 + 쿨타임 없음

## 레이드 맵 — AHexArena (`Source/LoA/Raid/HexArena.h/.cpp`) — 2026-07-04

### 구조
- `UHierarchicalInstancedStaticMeshComponent` (HexMeshes) — 타일 HISC, 단일 드로우콜
- `UProceduralMeshComponent` (WallMeshes) — 외곽 벽, 미터(miter) 접합 사다리꼴 지오메트리 실시간 생성
- `OnConstruction` — 에디터에서 파라미터 변경 시 실시간 리빌드
- 플러그인/모듈 의존성 추가: `.uproject`에 `ProceduralMeshComponent` 플러그인, `LoA.Build.cs`에 동일 모듈

### 타일 그리드 수학
- **좌표계**: Pointy-top Axial (q, r)
- **World 변환**: `X = D*(q + r*0.5)`, `Y = D*√3/2*r`, `D = TileSpacing + HexGap`
- **루프 범위**: q ∈ [-R, R], r ∈ [max(-R,-q-R), min(R,-q+R)], R = SideCount-1
- **총 타일 수**: 3R²+3R+1 (SideCount=4 → R=3 → 37개)
- `TileYaw=30°` — Modeling Mode 기본 Flat-top 메시를 Pointy-top으로 회전

### 외곽 벽 (타일 변 단위, 미터 접합 사다리꼴) — 2026-07-04
- 6방향 이웃 벡터 `GDQ[6], GDR[6]` + 외향 법선 각도 `GEdgeAngle[6]`
- **노출 변 판정**: `IsValidTile(q+dq, r+dr, R)` 불만족 시 해당 방향에 벽 배치
- SideCount=4 기준 외곽 노출 변 **24개** (코너 타일 6×2 + 일반 경계 타일 12×1)
- **문제였던 것**: 이전엔 HISM 박스 인스턴싱(균일 스케일)이라 바깥쪽 모서리가 직각으로 남아 인접 벽끼리 안 맞물림
- **해결**: 각 노출 변의 실제 타일 꼭짓점(`RHex = TileSpacing/√3`, 중심 기준 코너 각도 = `GEdgeAngle[d]±30°`)을 구하고,
  인접한 변들이 만나는 꼭짓점을 허용오차(`WeldTolerance = clamp(HexGap*2, 1, 50)`) 내에서 용접(weld)
- **미터 오프셋**: 한 꼭짓점에 모이는 변들의 외향 노멀 합 → 이등분 방향(`MiterDir`), 거리 배율 `1/cos(반각)`(`MiterFactor`)
  → `InnerPoint = Vertex + MiterDir*WallOffset*MiterFactor`, `OuterPoint = Vertex + MiterDir*(WallOffset+WallThickness)*MiterFactor`
  → 안쪽 변 길이는 그대로, 바깥쪽 변이 자동으로 늘어나 인접 조각과 꼭짓점을 정확히 공유 (사다리꼴)
- **메시**: 변 하나당 Top/Outer/Inner 3개 쿼드 (`AddQuad` 헬퍼가 지정한 Normal 기준으로 winding 자동 보정), 닫힌 루프라 이음매(end cap) 불필요
- **콜리전**: `bUseComplexAsSimpleCollision = true`

### BP_HexArena 파라미터
| 카테고리 | 파라미터 | 기본값 | 설명 |
|---|---|---|---|
| Hex Grid | SideCount | 4 | 한 변의 타일 수 |
| Hex Grid | TileSpacing | 520 | 중심간 거리 cm (flat-to-flat과 일치) |
| Hex Grid | HexGap | 10 | 타일 사이 틈 cm |
| Hex Grid | TileYaw | 30 | 타일 회전 (Flat→Pointy top) |
| Hex Tile | TileClass | `AHexTile` | BeginPlay에 스폰할 개별 타일 클래스 (BP_HexTile로 서브클래싱해 메시 할당) |
| Hex Wall | WallMaterial | — | 벽 머티리얼 (기존 WallMesh 스태틱메시 프로퍼티 대체, BP에서 재할당 필요) |
| Hex Wall | WallHeight | 400 | 벽 높이 cm |
| Hex Wall | WallThickness | 30 | 벽 두께 cm |
| Hex Wall | WallOffset | 0 | 외곽선 기준 추가 오프셋 cm |

### 레벨
- `Echidna2-1.umap` — 에키드나 2관문 메인 레벨, BP_HexArena 배치됨

## 개별 타일 관리 — AHexTile (`Source/LoA/Raid/HexTile.h/.cpp`) — 2026-07-07

### 왜 필요했나
- 기존 `HexMeshes`(HISM)는 타일들이 인스턴스 트랜스폼 배열일 뿐이라 개별 오버랩 이벤트/상태를 가질 수 없음
- 요구사항: 특정 타일(똥장판) 위에 서 있으면 매혹 게이지 누적 + 데미지, 똥장판에 둘러싸인 타일엔 거대한 꽃이 피어야 함 → 타일 단위의 콜리전 이벤트와 상태(enum)가 필요

### 설계 — "에디터는 HISM, 플레이는 개별 액터"
- `OnConstruction`(에디터/디자인 타임)은 그대로 `HexMeshes`로 프리뷰 — 슬라이더 조작마다 액터를 스폰/파괴하면 에디터가 무거워지고 아웃라이너가 지저분해지므로 여기선 손대지 않음
- `BeginPlay`(런타임)에서만 `HexMeshes`를 `SetVisibility(false)` + 콜리전 끄고, 좌표(q,r)마다 실제 `AHexTile` 액터를 스폰해 게임플레이를 이 액터들이 전담
- `EndPlay`에서 스폰된 타일 전부 `Destroy()` (PIE 종료/레벨 전환 시 정리)

### AHexTile 구조
- `TileMesh` (`UStaticMeshComponent`, 루트) — 콜리전 `BlockAll` 유지 (걷는 바닥 역할, 기존 HISM과 동일)
- `OverlapBox` (`UBoxComponent`) — `OverlapAllDynamic`, 캐릭터가 타일 위에 "서 있는지"만 감지 (Block 없음)
- `Coord` (`FIntPoint`) — 이 타일의 (q,r), `OwnerArena`가 이웃 조회할 때 사용
- `TileType` (`EHexTileType`: Normal/PoopZone/Flower) — `SetTileType()`으로 전환. 메시는 고정(`TileMesh` 컴포넌트에 SM_HexTile 직접 할당), 타입별로 `NormalMaterial`/`PoopMaterial`/`FlowerMaterial` 중 하나만 `TileMesh->SetMaterial(0, ...)`로 교체 (BP_HexTile 기본값에서 3개 머티리얼 할당 필요)
- `OnTileTypeChanged(NewType)` — `BlueprintImplementableEvent`, 꽃 개화·똥장판 이펙트 등 연출은 BP_HexTile에서 구현
- **PoopZone 틱 효과**: `HandleBeginOverlap`에서 캐릭터 감지 시 즉시 1틱 + `PoopTickInterval`(기본 1s) 반복 타이머 시작 → `ApplyPoopTick()`에서 `Character->AddCharmGauge(PoopCharmGaugePerTick)` + `Character->ReceiveDamage(PoopTickDamage)`, `HandleEndOverlap`에서 타이머 클리어 (혹한의 부름과 동일한 "독립 타이머 틱" 패턴)

### AHexArena 쪽 변경
- `TileClass` (`TSubclassOf<AHexTile>`) — BP에서 BP_HexTile 등 서브클래스 지정 가능, 비어 있으면 `AHexTile` 기본 클래스 사용
- `TileMap` (`TMap<FIntPoint, TObjectPtr<AHexTile>>`) — 좌표→타일 액터 매핑, `SpawnGameplayTiles()`가 채움
- `ComputeTileLocalTransform(q, r)` — 기존 `RebuildGrid()` 안에 있던 좌표 계산식을 분리해 `SpawnGameplayTiles()`와 공유 (HISM 프리뷰와 실제 스폰 위치가 항상 일치하도록)
- `GetTile(Coord)` — 좌표로 타일 액터 조회, `BlueprintCallable`
- `NotifyTileTypeChanged(ChangedCoord)` — `AHexTile::SetTileType()`이 호출. 바뀐 타일의 6방향 이웃마다 "그 이웃이 실제로 가진 이웃(그리드 밖 제외) 전부가 PoopZone인가"를 검사해서, 조건을 만족하는 Normal 타일을 자동으로 Flower로 전환 (기존 `GDQ/GDR` 이웃 벡터 재사용) — 외곽/코너 타일은 실제 이웃이 3~5개뿐이라 그 개수만큼만 만족해도 개화, 모든 타일이 개화 가능

### 사용 흐름 (스킬/기믹 쪽에서 호출할 때)
1. 보스 기믹이 특정 좌표를 똥장판으로 만들고 싶으면 `Arena->GetTile(FIntPoint(q, r))->SetTileType(EHexTileType::PoopZone)` 호출
2. 캐릭터가 그 타일 위를 지나가면 `OverlapBox`가 감지 → 매혹 게이지 누적 + 데미지 자동 진행
3. 여러 개의 똥장판이 한 타일을 완전히 둘러싸면 `NotifyTileTypeChanged`가 자동으로 그 타일을 Flower로 전환 (별도 호출 불필요)

### ALoACharacter 추가 (`LoACharacter.h/.cpp`)
- `CharmGauge`/`MaxCharmGauge` (기본 0/10), `OnCharmGaugeChanged` 델리게이트 — HP/MP와 동일한 패턴
- `AddCharmGauge(int32 Amount)` — 클램프 누적 + 델리게이트 브로드캐스트 (게이지 가득 찼을 때의 디버프 효과는 아직 미구현)

## 에키드나 보스 짤패턴 — 4거울 (`Source/LoA/Raid/EchidnaMirrorActor.h/.cpp`, `EchidnaBossStateTreeUtility.h/.cpp`) — 2026-07-19

### 패턴 개요
- 짤패턴 State 진입 시 보스 정면 기준 대각 4방향(45/135/225/315도)에 거울 4개를 동시 스폰 (`FStateTreeTask_EchidnaFourMirrorPattern`)
- 각 거울(`AEchidnaMirrorActor`)은 스폰 직후부터 독립적으로 Tracking → Firing 2단계를 자체 진행, StateTree는 전부 `IsFinished()`(레이저까지 끝남) 될 때까지 Running 유지

### AEchidnaMirrorActor 2단계 동작
- **Tracking (기본 3초, `TrackingDuration`)**: 거울에서 플레이어 방향/거리로 뻗은 직사각형 장판(`ZoneMeshComp`)이 실시간으로 따라옴
  - 회전은 `FMath::RInterpConstantTo`로 `TrackingRotationSpeed`(기본 60도/초) 각속도 제한 — 즉시 스냅 안 하게 해서 대시 같은 순간이동에도 즉시 안 꺾임
  - 장판 길이는 매 틱 플레이어까지 실제 거리로 갱신 (`MaxRange`로 클램프)
- **Firing (기본 1초, `FiringDuration`)**: 방향 고정, 장판 길이가 `MaxRange`까지 고정 연장 → 레이저로 전환
  - 데미지: `LaserDamageTickInterval`(기본 1/3초)마다 반복 판정 — 맞는 순간 즉시 1틱 + 그 뒤 3틱 = 1초간 총 4틱. `MaxDamageTicks`는 `floor(FiringDuration/Interval)+1`로 계산 (아래 "넉다운 시스템" 참조, 예전엔 `round()`라 "즉시 1틱+반복" 구조와 안 맞았음)
  - 판정: `GetActorsInBeamBox()`로 박스 오버랩
  - 별도 넉백 시스템(`KnockbackTickInterval`/`KnockbackStrength` 등)은 **제거됨** — 대신 데미지 틱마다 `ALoACharacter::ApplyKnockdown()` 호출로 통합 (아래 "넉다운 시스템" 참조)
  - `TrackingRotationSpeed` 기본값 60 → 100 → 80 → **50**도/초로 재조정 (플레이어 피드백 기준)
- 완료 후 `LifeAfterBeam` 뒤 소멸

### 비주얼 — 별도 에셋 없이도 즉시 보이게
- `MirrorMeshComp`(엔진 Sphere) / `ZoneMeshComp`(엔진 Plane) — 생성자에서 `/Engine/BasicShapes/*` + `BasicShapeMaterial`을 기본으로 박아둠 (VFX 미할당이어도 스폰만 되면 무조건 보임)
- 색상: `TrackingColor`(노랑)/`FiringColor`(빨강)를 `UMaterialInstanceDynamic`으로 `ColorParameterName`(기본 `"Color"`) Vector Parameter에 주입 — **머티리얼에 해당 이름의 파라미터가 있어야 실제로 색이 바뀜** (BasicShapeMaterial은 파라미터 없어서 안 바뀜, `M_MirrorLaser` 같은 커스텀 머티리얼 필요)
- 반투명: `ZoneOpacity`(기본 0.35)를 Color의 Alpha로 전달 — **머티리얼 Blend Mode가 Translucent이고 Alpha가 Opacity 핀에 연결돼 있어야** 실제로 투명해짐 (Opaque면 Alpha 무시됨, UE 기본 제약)
- `M_MirrorLaser` (`Content/Free_Magic/Demo/LevelPrototyping/Materials/`) — Vector Parameter 이름이 `"Color"`가 아니라 **`"Base Color"`**(공백 포함)라 `ColorParameterName`을 이거에 맞춰 BP에서 재설정함

### 쿨다운 & 패턴 로테이션 (StateTree `ST_Echidna`)
- `SmallPatternRotation`(패턴 후보들) 밖에 형제 State로 `Cooldown`(`Wait Random Duration` Min=3/Max=3)을 둠 — 안에 넣으면 In Order 선택 시 패턴보다 먼저 뽑힐 수 있어서 반드시 밖에 둬야 함
- 각 패턴 State는 Task가 자체 완료 조건을 가진 것 **하나만** 남기고 (`Echidna Four Mirror Pattern` 등), On State Completed → `Cooldown`으로 연결
- **버그였던 것**: 패턴 State 안에 `Wait Random Duration`을 공격 Task와 나란히(병렬) 넣고 Tasks 완료 정책이 "Any"였던 탓에, Wait(2~4초)가 레이저(6초)보다 먼저 끝나버려서 레이저 끝나기 전에 다음 패턴으로 넘어감 → Wait를 패턴 State에서 제거하고 `Cooldown`으로 분리해서 해결
- `FStateTreeTask_EchidnaPatrol` — Cooldown 중 보스가 제자리에 멈춰있지 않도록 `PatrolRadius`(기본 600cm) 안 무작위 지점으로 `AIController->MoveToLocation()` 이동. **레벨에 Nav Mesh Bounds Volume 필요** (없으면 MoveToLocation 실패, 조용히 안 움직임)
- Debug Text 같은 "완료를 리턴 안 하는" Task 하나만 State에 남기면 그 State는 영원히 안 끝남 (Cooldown 전이가 아예 안 됨) — 짤패턴 자리를 임시로 비워둘 땐 Wait 계열처럼 실제로 완료되는 Task를 최소 하나는 남겨야 함

## 에키드나 보스 짤패턴 — 뒤로 빠지며 좌우장판 (`Source/LoA/Raid/EchidnaFanZoneActor.h/.cpp`, `EchidnaBossStateTreeUtility.h/.cpp`) — 2026-08-05

### 패턴 개요 (레퍼런스 이미지 "4. 뒤로 빠지며 좌우장판")
- 보스는 제자리에 멈춘 채로 정면(플레이어 방향) 기준 **왼쪽으로 비스듬한 부채꼴(1)**을 먼저 터뜨리고,
  이어서 **오른쪽으로 비스듬한 부채꼴(2)**을 터뜨린다 — 총 2회 순서대로 (`FStateTreeTask_EchidnaRetreatFanPattern`)
- 각 장판(`AEchidnaFanZoneActor`)은 예고(Telegraph) 후 고리 단위로 확장하는 자체 완결형 액터 — 4거울과 동일하게 Task는 스폰만 담당, `IsFinished()` 폴링으로 완료 판정
- 순서: 장판1(왼쪽) 예고+확장 → **판정이 시작되는 순간** 보스 후방 홉 → 장판2(오른쪽) 예고+확장 → 그 순간에도 후방 홉 → Succeeded (한쪽만 끝나면 안 되고 **둘 다 순차 완료**해야 State 종료)
- 1번(왼쪽)과 2번(오른쪽)의 각도 차이가 부채꼴 절반각보다 작아서 **가운데(보스 정면)가 항상 겹침** — 레퍼런스의 "겹치는부분 주의" 경고와 동일, 그 구간에 서 있으면 1·2 두 번 다 맞을 수 있음

### AEchidnaFanZoneActor 동작 — 예고 후 와이파이 아이콘처럼 계단식 확장 (2026-08-06)
- **변천사**: (1) 예고 후 한 번에 폭발 → (2) 한 번에 터지는 느낌이 싫다고 해서 예고 없이 고리 단위 계단식 확장으로 재설계 → (3) 그래도 예고 단계가 있어야 한다고 해서 "예고(전체 부채꼴 표시) + 고리 단위 계단식 확장" 두 단계를 합친 현재 구조로 정착
- **1) 예고 단계**: `Activate()` 호출 즉시 `BuildFanMesh(FanRange)`로 전체 부채꼴(`FanInnerRadius`~`FanRange`)을 `TelegraphColor`로 표시만 함, 데미지 없음. `TelegraphDuration`(기본 1초) 뒤 `BeginRingExpansion()`으로 전환
- **2) 고리 확장 단계**: `ApplyMeshColor(FanColor)`로 색을 바꾸고 `RevealNextRing()` 1회 호출 → `FanInnerRadius`~`FanRange`를 `RingCount`(기본 5)등분한 첫 고리로 메시가 다시 줄어들었다가, `RingInterval`(기본 0.15초) 반복 타이머로 한 칸씩 다시 넓어짐 — 매번 `BuildFanMesh(NewOuterRadius)`로 그 시점까지 누적된 고리(0~N)를 다시 그림 (고리마다 별도 섹션이 아니라 매번 섹션 0을 통째로 재생성, 개수가 적어 성능 문제 없음). 예고 때 이미 전체가 넓게 표시돼 있다가 확장 단계 시작하면서 다시 작게 오므라들었다 넓어지는 모양이라, 결과적으로 "위험 범위를 미리 보여준 뒤 그 안에서 실제 타격이 안→밖으로 훑고 지나가는" 느낌이 됨
- 데미지는 고리 단위로 1회만: `ApplyRingDamage(PrevOuterRadius, NewOuterRadius)`가 해당 구간(annulus) 안에 있는 대상만 판정 — 제자리에 서 있으면 자기 위치에 해당하는 고리가 열릴 때 딱 한 번만 맞음 (이미 지나간 안쪽 고리가 다시 판정하지 않음, 예고 단계에서는 아예 판정 자체가 없음)
- 방향은 **스폰 시점의 플레이어 방향 + 좌/우 각도 오프셋**으로 고정 — 거울 패턴과 동일하게 Task의 `SpawnFan()`에서 보스 자체 회전이 아니라 플레이어 위치로 직접 재계산한 Rotation을 스폰 인자로 넘김 (1번은 왼쪽, 2번은 오른쪽으로 틀어서 씀 — 자세한 각도 계산은 아래 `FStateTreeTask_EchidnaRetreatFanPattern` 항목 참조)
- 완료 판정: `IsFinished()` = `bExploded` — 마지막 고리까지 다 넓어진 시점에 true (그 뒤 `LifeAfterExplode` 동안의 소멸 대기는 기다리지 않고 StateTree는 바로 다음 단계로 넘어감)

### 비주얼 — 부채꼴은 ProceduralMeshComponent로 직접 생성, VFX 없이 색상만
- 부채꼴 모양은 엔진 기본 메시로 표현이 안 돼서(Cone을 눕혀 스케일로 흉내냈던 첫 버전은 탑다운 카메라에서 옆면이 둥글게 보여 폐기) `FanMeshComp`(`UProceduralMeshComponent`)로 런타임에 `CreateMeshSection()`으로 직접 지오메트리 생성 — `HexArena` 벽과 동일한 방식(모듈 의존성도 이미 있음)
- `ArcSegments`(기본 24)개의 사다리꼴 쿼드를 이어붙여 부채꼴을 구성 — 조각 하나당 로컬 +X(정면) 기준 각도 A/B에서 `FanInnerRadius`~(그 시점) 바깥 반지름 사이 안쪽변/바깥변 4점으로 쿼드 생성. 쿼드 생성은 `HexArena.cpp`의 `AddQuad` 헬퍼와 동일한 패턴(로컬 anonymous namespace `AddFanQuad`) — 감김 방향을 앞/뒤 양쪽 다 추가해서 PMC의 front-face 방향에 상관없이 항상 양면이 보이게 함
- `FanInnerRadius`(기본 60cm) — 0이면 뾰족한 삼각형, 0보다 크면 안쪽이 잘린 사다리꼴 형태 (레퍼런스 이미지의 "살짝 사다리꼴" 모양). 첫 고리도 이 반지름부터 시작하므로 데미지 판정도 동일하게 안쪽 한계선으로 적용됨
- **VFX 없이 색상 하나로만 표현** — 예고는 `TelegraphColor`, 고리 확장은 `FanColor`를 `ColorParameterName`/`FanOpacity`와 함께 MID로 주입 (`ApplyMeshColor()`). Niagara는 전부 제거함. 커스텀 머티리얼 쓸 경우 `ColorParameterName`에 해당 이름의 Vector Parameter + Translucent Blend Mode 필요 (거울 패턴과 동일한 MID 주입 방식)

### FStateTreeTask_EchidnaRetreatFanPattern — 제자리 캐스팅 + 판정 순간 후방 홉 (2026-08-06 재설계)
- **변천사**: 처음엔 "AIController->MoveToLocation()으로 걸어서 후퇴 → 도착하면 캐스팅"을 캐스팅마다 반복하는 구조였는데, 걸어서 물러나는 느낌이 아니라 "멈춰서 캐스팅하다가 터지는 순간 점프하듯 뒤로 홉"하는 느낌을 원해서 나브메시 이동을 걷어내고 `LaunchCharacter` 기반으로 교체
- `Phase`(Casting1→Casting2→Done) 상태머신만 남음 — Retreating 단계 자체가 없어짐, `AIController`는 State 진입 시 잔여 이동을 멈추는 용도(`StopMovement()`)로만 남아있고 더 이상 이동 명령에 쓰이지 않음
- **좌/우 각도**: `SpawnFan(InstanceData, YawOffsetDeg)`가 `InstanceData.BaseAimRotation`(기준 조준 방향)에 추가 회전을 더함 — 1번은 `-FanYawOffset`(보스 기준 왼쪽), 2번은 `+FanYawOffset`(오른쪽). `FanYawOffset`(기본 32도)이 `AEchidnaFanZoneActor::FanAngle` 절반(기본 70/2=35도)보다 작아야 겹침 — 겹치는 폭은 `FanAngle - 2*FanYawOffset`(현재 70-64=6도)이라 값이 커질수록 겹치는 부분이 줄어듦 (100/35 → 70/18 → 70/28 → 70/32 순으로 레퍼런스에 맞춰 계속 좁힘)
- **주의**: `ST_Echidna`에서 `Fan Yaw Offset`을 한 번이라도 에디터에서 직접 입력했다면 그 값이 인스턴스 오버라이드로 저장돼 코드 기본값을 바꿔도 반영되지 않음 — PIE에서 안 바뀌면 Task Details에서 이 필드에 노란 오버라이드 화살표가 떠 있는지 확인하고 우클릭 → Reset to Default 하거나 값을 직접 갱신해야 함
- **기준 조준 방향은 패턴 시작 시 1회만 고정** (2026-08-06): 처음엔 `SpawnFan()`이 호출될 때마다 그 시점의 플레이어 위치로 매번 다시 조준했는데, 그러면 1번(왼쪽)과 2번(오른쪽) 사이에 플레이어가 움직일 경우 두 장판의 기준선이 서로 달라져서 "가운데가 겹치는" 디자인이 깨짐 — `ComputeAimRotation(Boss)`(플레이어 방향, 없으면 보스 현재 회전)를 `EnterState`에서 딱 한 번 호출해 `InstanceData.BaseAimRotation`에 저장해두고, 1번/2번 `SpawnFan()` 모두 이 고정값 + YawOffsetDeg만 사용. 장판 스폰 **위치**는 여전히 매번 그 시점 보스의 실제 위치(홉으로 물러난 후 위치)를 쓰고, 오직 **방향**만 고정됨
- **보스가 쏘는 방향을 쳐다봄**: `SpawnFan()`에서 최종 Rotation을 계산한 직후 `Boss->SetActorRotation(FRotator(0, SpawnRotation.Yaw, 0))`로 보스 자신도 같은 Yaw로 즉시 회전시킴 — 이전엔 장판만 방향을 잡고 보스 모델은 계속 이전 방향에 고정돼 있던 버그
- **후방 홉 타이밍**: Tick에서 매 프레임 `CurrentFan->HasStartedExploding()`을 폴링하다가, 처음 true가 되는 순간(예고가 끝나고 고리 확장이 막 시작된 시점 = "터지는 순간") `HopBackward()`를 **장판 하나당 딱 1번만** 실행 (`bHoppedForCurrentCast` 플래그로 중복 방지)
- `HopBackward()`는 "플레이어 반대 방향"(장판 방향 계산과 동일한 방식)으로 `Boss->LaunchCharacter(AwayFromPlayer * HopBackStrength + Up * HopUpwardStrength, true, true)` 호출 — 순간이동이 아니라 실제 물리 launch라서 자연스럽게 포물선을 그리며 착지함 (거울 패턴의 `LaunchCharacter` 넉백과 동일한 함수, 대상만 플레이어→보스 자신)
- **낙사 방지**: 홉을 실제로 실행하기 전에 `HasGroundBelow()`로 착지 예상 지점(`보스 위치 + AwayFromPlayer * HopCheckDistance`, 기본 350cm)에서 수직 라인트레이스(`ECC_Visibility`, 다른 스킬들의 커서 지면 판정과 동일 채널)를 쏴서 바닥이 없으면 `LaunchCharacter` 자체를 호출하지 않고 조용히 스킵 — 맵 끝자락에서 패턴이 나가도 그 자리에 멈춰있을 뿐 떨어지지 않음. `HopCheckDistance`는 실제 물리 이동 거리를 정확히 예측하는 값이 아니라 대략적인 안전 판정용 근사치
- StateTree 배치는 4거울과 동일한 패턴: `SmallPatternRotation` 안에 이 Task 하나만 넣고 On State Completed → `Cooldown`으로 연결 (Wait를 나란히 넣지 말 것 — 4거울 문서의 "버그였던 것" 참조)
- `FanZoneClass` 미할당 시 EnterState에서 바로 Failed (경고 로그로 원인 표시) — `AIController`는 이제 필수 아님(없어도 패턴 자체는 동작, 다만 잔여 이동을 못 멈춤)
- **버그였던 것**: `SpawnFan()`에서 스폰 위치로 `Boss->GetActorLocation()`을 그대로 썼더니 장판이 공중에 떠 보임 — 이 값은 캡슐 **중심** 좌표(지면에서 캡슐 절반 높이만큼 위)라서, `GetCapsuleComponent()->GetScaledCapsuleHalfHeight()`만큼 Z를 빼서 발밑(지면) 높이로 보정 후 스폰

## 넉다운 시스템 (`Source/LoA/LoACharacter.h/.cpp`, `Skill/SkillManagerComponent.h/.cpp`) — 2026-08-06

### 개요
- 특정 패턴에 맞으면 뒤로 튕겨나가며 쓰러지고, 3초 뒤 자동 기상하거나 스페이스바로 즉시 기상(15초 쿨타임) 가능
- 현재 fanzone(`AEchidnaFanZoneActor::ApplyRingDamage`)과 거울 레이저(`AEchidnaMirrorActor::ApplyLaserDamageTick`)에 연결됨 — 다른 패턴에서 걸고 싶으면 `Character->ApplyKnockdown(SourceLocation)` 호출하면 됨

### ALoACharacter 상태 흐름
- `ApplyKnockdown(SourceLocation)` — `SourceLocation` 반대 방향으로 `LaunchCharacter`, `KnockdownHopSettleTime`(기본 0.4초) 타이머 시작. **착지 여부(MovementMode) 폴링 방식은 폐기** — 여러 공격원(거울 4개 등)이 같은 프레임에 겹쳐 `LaunchCharacter`를 호출하면 엔진 내부 `PendingLaunchVelocity`가 서로 덮어써지고 `bForceNextFloorCheck` 때문에 Falling→Walking이 같은 프레임에 즉시 왕복돼버려서 폴링으로는 착지를 감지 못했음(항상 즉시 눕는 것처럼 오판) → 대신 맞을 때마다 정착 타이머를 새로 시작하는 방식으로 교체, 연속 타격 동안은 계속 "공중에 뜬 채" 유지됨
- 생성자의 `bConstrainToPlane=true`(top-down 클릭이동을 위한 Z축 고정)가 `LaunchCharacter`의 수직 임펄스를 매 틱 평면으로 눌러버려서 뜨자마자 즉시 착지 판정이 나던 버그 있었음 — `ApplyKnockdown` 중엔 임시로 `false`, 정착 시(`SettleKnockdown()`) 다시 `true`로 복구
- `GetUpFromKnockdown()` — 자동 기상(3초 타이머)/즉시 기상 공통 진입점
- `TryInstantGetUp()` — 완전히 누운 뒤(공중에 뜬 상태 `bKnockdownAirborne`가 아닐 때)만 성공. 성공 시 캐릭터 함수가 아니라 **`SkillManager->TriggerCooldown(19)`를 호출** (아래 참조)
- 컨트롤러 쪽: `IsKnockedDown()`인 동안 이동/스킬 입력 전부 차단, 스페이스바(`OnDashInput`)는 넉다운 중엔 대시 대신 `TryInstantGetUp()` 호출

### 즉시 기상 쿨타임 = SkillManagerComponent 슬롯 19
- 처음엔 `ALoACharacter`에 자체 쿨타임 필드(`InstantGetUpCooldown`, `LastInstantGetUpTime`)를 뒀었으나, 대시(슬롯18)와 완전히 동일한 UX/쿨타임 UI를 원해서 **`SkillManagerComponent`로 이관** — 대시와 같은 "인스턴스 없이 DT 쿨타임/아이콘만 쓰는 슬롯" 패턴 재사용
- `GetUpSlotIndex=19`, `GetUpRowName`(기본 `"InstantGetUp"`), `GetUpCooldownFallback`(15초) — `DT_Skills`에 해당 이름 행이 없으면 폴백값으로 동작, 있으면 그 행의 Cooldown/Icon 사용
- `GetSlotIcon`/`GetSlotSkillData`에 `DashSlotIndex`와 동일한 패턴으로 `GetUpSlotIndex` 분기 추가
- **DT_Skills 연동 시 주의**: `FindRow`는 행 이름이 `GetUpRowName` 프로퍼티 값과 **정확히 일치**해야 함 (한글 행 이름 vs 영문 기본값 불일치로 한참 헤맴). 그리고 **Cooldown 컬럼이 비어있으면(0) 쿨타임이 사실상 즉시 끝나버려서 무한 재사용 가능** — 반드시 값 채울 것

### WBP_HUD — 대시 UI를 그대로 복제해서 기상기 UI 구성 (Blueprint, C++ 아님)
- `Border_Dash`/`Img_Dash`/`CooldownImg_Dash`/`CooldownTxt_Dash` 구조를 통째로 복제해 `Border_GetUp` 등 생성, Event Tick의 대시 전용 하드코딩 체인(쿨타임 텍스트/Border 표시/원형 머티리얼 채움)도 슬롯번호 18→19, 타겟 위젯 Dash→GetUp으로 바꿔 복제
- 이 프로젝트의 스킬 슬롯 쿨타임 UI는 **재사용 함수가 아니라 슬롯마다 Event Tick에 하드코딩된 노드 뭉치**임 (`RefreshingSlot`/`Slot Images` 배열은 드래그앤드랍 아이콘 갱신용 별개 시스템, Q~F 전용) — 새 슬롯 UI 추가할 땐 기존 슬롯(대시)의 Tick 체인을 통째로 복사해서 슬롯번호만 바꾸는 게 이 코드베이스의 기존 패턴
- **HorizontalBox 자동 중앙정렬**: 부모 Canvas 슬롯에 `Size To Content` + `Alignment(0.5,0.5)`를 걸면, 자식(Border_Dash/Border_GetUp) 중 Visibility가 **Collapsed**인 것은 레이아웃에서 완전히 빠지므로 1개만 보일 때 자동으로 정중앙에 옴 (Hidden은 자리를 계속 차지하니 안 됨). 자식 크기는 각각 `SizeBox`로 Width/Height Override 고정 — Auto로 두면 아이콘 텍스처 원본 해상도 그대로 desired size로 잡혀서 화면을 뒤덮을 만큼 커짐
- **버그였던 것 (Q~F 아이콘이 전부 흰 박스로 나옴)**: Construct 그래프에서 기상기 아이콘 초기화 `Branch`(아이콘 유효성 체크)의 **True 쪽 체인 끝이 `Delay` 노드로 연결이 안 되어 있어서**, DT_Skills에 아이콘이 있어 True로 빠지는 경우 그 뒤에 있는 Q~F 아이콘 로딩 루프 전체가 실행되지 않음 (False만 연결해뒀던 게 원인 — True/False 둘 다 결국 같은 `Delay`로 합류하도록 고쳐야 함). Tick 이벤트 안에서는 **브레이크포인트가 정상 작동 안 할 때가 있어서**(에디터가 멈춘 채 진행 안 됨) True/False 양쪽에 각각 다른 문구 찍는 `Print String`으로 대체 디버깅

## 구현된 기능 (2026-07-03 기준)
- [x] 마우스 클릭 이동
- [x] 대시 (스페이스바)
- [x] 스킬 시스템 (즉발/캐스팅/차지/콤보/홀딩)
- [x] HP/MP UI (MVVM)
- [x] 스킬트리 창 (K키, MVVM, +/-버튼으로 레벨업)
- [x] 스킬트리 — 드래그앤드랍으로 HUD 슬롯 배정
- [x] 스킬트리 창 마우스 드래그 이동
- [x] 스킬트리 UI 위에서 게임 입력 차단
- [x] 종말의 날 (SkillDoomsday) — 캐스팅 스킬, 사거리 자동이동 후 메테오 낙하
- [x] 천벌 (SkillCheonbeol) — 캐스팅 스킬, 사거리 자동이동 후 번개 낙하
- [x] 블레이즈 (SkillBlaze) — 즉발 발사체 스킬
- [x] 익스플로전 (SkillExplosion) — 캐스팅 스킬, 캐릭터 앞 직선 메테오 발사 + 범위 폭발
- [x] 인페르노 (SkillInferno) — 즉발 스킬, 커서 위치 장판 + 위로 솟구치는 폭발
- [x] 혹한의 부름 (SkillFrostCall) — 홀딩 스킬, 커서 위치 장판 유지 + 틱 데미지 + 고드름 낙하 VFX
- [x] 아이스 에로우 (SkillIceArrow) — 즉발 스킬, 원프레스 자동이동, 45도 고드름 4틱 낙하
- [x] 돌풍 (SkillGust) — 즉발 스킬, 커서 방향 회전 + 이동 정지, 캐릭터 앞 전방 박스 콜리전 토네이도 존 1회 데미지
- [x] 스킬 잠금 시스템 — 시전 중 다른 스킬 입력 차단 + 큐 등록, Hold 활성 중 완전 씹힘, PostDelay 후 자동 발동
- [x] 캐스팅 스킬 취소 시 설정 쿨타임 적용
- [x] 캐스팅 중 이동 입력 시 캐스팅 취소 + 쿨타임
- [x] 범위 발판 VFX (CircleShowTime — 원 표시 후 효과)
- [x] VFX 스케일 ExplosionRadius 기반 자동 비례 계산
- [x] 사거리 자동이동 시스템 (Cast: 홀드 필요 / Instant: 원프레스)
- [x] 이동 중 마우스 클릭 시 대기 스킬 취소 (쿨타임 없음)
- [x] ESkillInputType::Hold 추가, FSkillData::HoldMaxTime 필드 추가
- [x] AHexArena — 37타일 육각형 아레나 (HISC, Axial 좌표계, OnConstruction 실시간 리빌드)
- [x] AHexArena 외곽 벽 — 타일 변 단위 24개 벽 조각, 미터 접합 사다리꼴 프로시저럴 메시로 인접 조각과 완전히 맞물림
- [x] AHexTile — 개별 타일 액터 (BeginPlay 스폰), 타입별 오버랩 이벤트 + 비주얼 전환 (Normal/PoopZone/Flower)
- [x] 매혹 게이지 (CharmGauge) — ALoACharacter, PoopZone 틱마다 누적 + 데미지
- [x] 똥장판에 둘러싸인 타일 자동 Flower 전환 (AHexArena::NotifyTileTypeChanged)
- [x] 에키드나 보스 짤패턴 "4거울" — 대각 4방향 거울 동시 스폰, 추적(장판 따라옴) → 발사(레이저 고정) 2단계, 데미지+넉백 독립 틱
- [x] 보스 쿨다운 중 패트롤 (FStateTreeTask_EchidnaPatrol)
- [x] 짤패턴 로테이션 Cooldown 분리 (3초 텀 후 다음 패턴)
- [x] 에키드나 보스 짤패턴 "뒤로 빠지며 좌우장판" — 플레이어 반대로 후퇴하며 정면 부채꼴 장판(예고→폭발) 2회 순차 발동 (C++ 구현 완료, StateTree `ST_Echidna` 에디터 배치는 미완 — 아래 참조)
- [x] 넉다운 시스템 — 뒤로 튕겨나가며 쓰러짐, 3초 자동/스페이스바 즉시(15초 쿨타임) 기상, fanzone·거울 레이저에 연결 (자세한 내용은 위 "넉다운 시스템" 섹션)
- [x] 즉시 기상 쿨타임 UI — SkillManagerComponent 슬롯 19로 대시(슬롯18)와 동일하게 통합, WBP_HUD에 Border_GetUp 추가
- [x] 거울 레이저 타이밍 재조정 — FiringDuration 3초→1초, 데미지 4틱, 추적 회전속도 60→50도/초, 별도 넉백 시스템 제거(넉다운으로 통합)
- [ ] Border_GetUp UI 최종 위치/스타일 다듬기
- [ ] DT_Skills에 `InstantGetUp` 행 Cooldown/Icon 값 채워졌는지 재확인 (비어있으면 쿨타임 무력화됨)
- [ ] StateTree `ST_Echidna`에 `Echidna Retreat Fan Pattern` Task 배치 필요 — `SmallPatternRotation` 안에 4거울과 나란히 추가, `FanZoneClass`(BP_EchidnaFanZone 등)/Boss/AIController 바인딩, On State Completed → `Cooldown` 연결
- [ ] BP_EchidnaFanZone 서브클래스 생성 + 부채꼴 전용 커스텀 머티리얼(Color Vector Parameter + Translucent) 할당 — VFX는 안 씀, 색상만으로 표현
- [ ] SM_HexTile 머티리얼 슬롯 분리 (윗면 MI_Rock_Inst_5, 옆면 어두운 색)
- [ ] BP_HexArena에서 WallMaterial 재할당 (기존 WallMesh 프로퍼티가 프로시저럴 메시 전환으로 제거됨)
- [ ] BP_HexTile 서브클래스 생성 + NormalMesh/PoopMesh/FlowerMesh 할당 (NormalMesh는 기존 HISM 메시와 동일하게)
- [ ] 매혹 게이지 가득 찼을 때의 디버프 효과 미구현 (현재는 누적만 됨)
- [ ] 꽃 개화/똥장판 VFX 연출 (AHexTile::OnTileTypeChanged BlueprintImplementableEvent에서 구현 필요)
- [ ] DT_Skills SkillName/Icon 데이터 입력 필요 (혹한의 부름·아이스 에로우·돌풍 포함)
- [ ] BP_FrostCall / BP_IceArrow / BP_Gust ZoneClass·VFX 에셋 할당
- [ ] 스킬 레벨에 따른 데미지 계수 연동
- [ ] AvailableSkillPoints UI 표시 연동
- [ ] NS_Inferno_Impact AddVelocity Z값 Niagara 에디터에서 조정 필요
- [ ] PER_Lava_Brutal 이미터 스케일 조정 (NS_Explosion_Impact 잔상 크기)

## 자주 쓰는 빌드 명령
```
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" LoA Win64 Development "C:\Users\User\Documents\Unreal Projects\LoA\LoA.uproject" -NoUBTMakefiles
```
