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

## 구현된 기능 (2026-06-22 기준)
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
- [x] 캐스팅 스킬 취소 시 설정 쿨타임 적용
- [x] 캐스팅 중 이동 입력 시 캐스팅 취소 + 쿨타임
- [x] 범위 발판 VFX (CircleShowTime — 원 표시 후 효과)
- [x] VFX 스케일 ExplosionRadius 기반 자동 비례 계산
- [x] 사거리 자동이동 시스템 (Cast: 홀드 필요 / Instant: 원프레스)
- [x] 이동 중 마우스 클릭 시 대기 스킬 취소 (쿨타임 없음)
- [x] ESkillInputType::Hold 추가, FSkillData::HoldMaxTime 필드 추가
- [ ] DT_Skills SkillName/Icon 데이터 입력 필요 (혹한의 부름·아이스 에로우 포함)
- [ ] BP_FrostCall / BP_IceArrow ZoneClass·VFX 에셋 할당 + CircleBaseRadius 실측
- [ ] 스킬 레벨에 따른 데미지 계수 연동
- [ ] AvailableSkillPoints UI 표시 연동
- [ ] NS_Inferno_Impact AddVelocity Z값 Niagara 에디터에서 조정 필요
- [ ] PER_Lava_Brutal 이미터 스케일 조정 (NS_Explosion_Impact 잔상 크기)

## 자주 쓰는 빌드 명령
```
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" LoA Win64 Development "C:\Users\User\Documents\Unreal Projects\LoA\LoA.uproject" -NoUBTMakefiles
```
