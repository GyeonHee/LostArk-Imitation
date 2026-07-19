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
- 스킬 슬롯: 0=Q, 1=W, 2=E, 3=R, 4=A, 5=S, 6=D, 7=F, 8=기본공격, 18=대시

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
- **Firing (기본 3초, `FiringDuration`)**: 방향 고정, 장판 길이가 `MaxRange`까지 고정 연장 → 레이저로 전환
  - 데미지: `LaserDamageTickInterval`(기본 0.5s)마다 반복 판정
  - 넉백: 데미지 틱과 **별도 타이머**로 `KnockbackTickInterval`(기본 1s)마다 `LaunchCharacter(빔 진행방향*KnockbackStrength + 위로*KnockbackUpwardStrength)` — 혹한의 부름의 "독립 타이머 틱" 패턴과 동일
  - 판정: `GetActorsInBeamBox()`로 박스 오버랩 공통화 (데미지/넉백 둘 다 재사용)
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
- [ ] **넉백 판정 수정 필요** — 현재 구현 확인 중, 다음 작업 우선순위
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
