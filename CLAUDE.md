# LoA 프로젝트 — Claude 컨텍스트

## 프로젝트 개요
- **장르**: 탑다운 ARPG (로스트아크 모작)
- **엔진**: Unreal Engine 5.8
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
- `WBP_BossHP` (`/Game/LostArk/UI/`) — 보스 HP 바. **MVVM이 아니라 `UBossHPWidget`(C++) 상속 + `BindWidget`** 방식
  - MVVM은 ViewModel 바인딩을 에디터에서 손으로 걸어야 해서 MCP로 끝까지 못 만든다. `UCharmGaugeWidget`과 동일하게 C++ Setter를 호출하는 방식이면 위젯 BP까지 MCP로 생성 가능 — 새 UI를 추가할 땐 이 쪽이 작업이 빠름
  - 트리: `RootCanvas` → `BossHPRoot`(VerticalBox, 상단 중앙 앵커 1010x70) → `BossNameText`(고정 라벨) + `BarRow`(HorizontalBox) → [`EnrageBox`(VerticalBox: `EnrageLabelText` + `EnrageTimeText`, 광폭화 타이머) + `BarOverlay`(Fill) → **`NextLineImage` → `HPBar` → `LineText`**] (Overlay는 나중 자식이 위에 그려지므로 이 순서가 곧 뒤→앞 순서)
  - `EnrageLabelText`/`EnrageTimeText`는 `BindWidgetOptional` — 없어도 컴파일은 되고 타이머만 안 보인다
  - 화면 좌상단(`RootCanvas` 직속 `SettlementRoot`, 앵커 0,0 / 위치 20,20)에 **초상화 + 2칸 정산 게이지**: `PortraitImage`(`T_EchidnaPortrait` 128x128 — 레퍼런스 스크린샷에서 잘라낸 저해상도 임시 이미지, 좋은 원본이 생기면 교체) → `SettlementBarSize`(120x10) → `SettlementBarRow` → `SettlementBar1`/`SettlementBar2` + `SettlementText`(%). 세 개 다 `BindWidgetOptional`
  - `HPBar`/`NextLineImage`/`LineText` 세 개가 C++ `BindWidget` 대상이라 **이름을 바꾸면 컴파일 에러**가 난다. `BossNameText`는 C++이 모르는 순수 디자이너 라벨
  - `LineText` 표기: `현재체력 / 최대체력    남은줄수` (예: `4,746,719,168 / 4,746,719,168    285`). 체력이 int32 범위를 넘으므로 `FText::AsNumber(int64)`로 자릿수 구분
  - **줄마다 색이 바뀌는 로아식 바**: `HPBar`의 Percent는 전체 HP가 아니라 **현재 줄 안의 잔량**이고, 색은 `LineColors[(현재줄-1) % Num]`. 뒤에 깔린 `NextLineImage`는 **한 줄 아래**의 색이라, 현재 줄이 비어갈수록 다음 줄 색이 드러난다. 마지막 1줄에서는 `GetLineColor(0)`이 투명을 반환해 빈 칸이 보임
  - 이게 성립하려면 **`HPBar`의 `WidgetStyle.BackgroundImage` 틴트 알파가 0**이어야 한다(기본값은 흰색 불투명이라 뒤를 가림). `NextLineImage`는 텍스처 없이도 확실히 그려지도록 브러시를 `RoundedBox`(cornerRadii 0 = 단색 사각형)로 설정해둠
  - `LineColors`는 `EditDefaultsOnly`라 WBP Class Defaults에서 색/개수 조절 가능 (기본 5색 순환)
- `WBP_CastBar` (`/Game/LostArk/UI/`) — 캐스팅/차지 진행바. `UCastBarWidget`(C++) 상속 + `BindWidget` (보스 HP와 동일 방식)
  - 트리: `RootCanvas` → `CastBarRoot`(Overlay, 하단 중앙 앵커 400x28, 바닥에서 200px 위) → `CastBar`(ProgressBar) + `CastTimeText`(TextBlock, 바 우측 하단에 작게 얹힘, 11pt + 그림자)
  - 진행도 조회 경로: `USkillBase::GetCastProgress()`(virtual, 기본 false) → `USkillCast`/`USkillCharge`가 오버라이드 → `USkillManagerComponent::GetActiveCastProgress()`가 슬롯을 훑어 진행 중인 첫 스킬 반환 → `ALoAPlayerController::UpdateCastBar()`가 Tick에서 폴링
  - **Cast는 `bIsActive`일 때만 true를 반환** — 사거리 밖에서 이동 중(`bMovingToRange`)일 땐 아직 캐스팅이 시작된 게 아니라 바가 뜨지 않는다
  - `UpdateCastBar()`는 Tick의 **`IsActionLocked()` 조기 return보다 앞**에 있어야 한다. 뒤에 두면 캐스팅 중 기절/넉다운을 맞았을 때 그 아래가 실행되지 않아 바가 화면에 얼어붙은 채 남는다
  - Hold 스킬(혹한의 부름)도 `ElapsedTime`을 누적하므로 `GetCastProgress` 오버라이드만 추가하면 같은 바에 표시 가능 (현재는 미적용)
- `ADamageNumberActor` + `WBP_DamageNumber` (`/Game/LostArk/UI/`) — 보스 피격 데미지 숫자. **HUD 위젯이 아니라 월드 액터**다
  - `AEchidnaBoss::ReceiveDamage`마다 `SpawnDamageNumber()`가 보스 몸통(`GetActorLocation()` = 캡슐 중심) + `DamageNumberHeight`에 스폰하고, `DamageNumberJitter` 범위로 흩뿌려 연타 시 숫자가 완전히 겹치지 않게 한다
  - 표시는 `UWidgetComponent`(**WidgetSpace=Screen**) — 매혹 게이지와 같은 이유로 탑다운 카메라 각도와 무관하게 항상 일정 크기로 보인다
  - **겹친 숫자의 앞뒤 순서**: 보스가 계속 증가하는 카운터를 `Activate()`에 넘기고 액터가 `SetTranslucentSortPriority()`에 넣는다 → 나중에 맞은 숫자가 항상 앞
  - 떠오르기·페이드·소멸은 전부 액터의 `Tick`이 담당(위젯은 숫자만 채움). `FadeStartRatio`(기본 0.35)까지는 완전 불투명하게 두고 그 뒤 `Lifetime`(3초)까지 선형으로 사라진다 — 스폰 즉시 흐려지면 읽을 수가 없어서
  - 페이드는 `WidgetComponent`가 아니라 **안에 든 `UserWidget`의 `SetRenderOpacity()`**를 건드려야 Screen 스페이스에서도 먹는다
  - **버그였던 것 (가끔 데미지 0이 뜸)**: `AEchidnaBoss::TakeDamage`가 `Super::TakeDamage`의 **반환값**을 `ReceiveDamage`에 넘기고 있었다. `AActor::InternalTakeRadialDamage`는 `ComponentHits`의 가장 가까운 충돌 지점까지의 거리로 배율을 구하는데, 오버랩은 잡혔어도 그 지점 계산이 실패하면 거리가 `UE_MAX_FLT`로 남아 배율이 0이 되고 `ApplyRadialDamage`는 `MinimumDamage=0`이라 결과가 통째로 0이 된다(`bDoFullDamage=true`는 감쇠 곡선만 없앨 뿐 이 경로를 막지 못함). **숫자만 0인 게 아니라 HP도 실제로 안 깎이고 있었다.** `ALoACharacter::TakeDamage`처럼 들어온 원본 `DamageAmount`를 그대로 쓰도록 통일해서 해결
  - **데미지 계열 액터를 새로 만들 때 규칙**: 이 프로젝트는 반경 감쇠를 쓰지 않으므로(`ApplyRadialDamage`의 `bDoFullDamage`가 전부 `true`) `TakeDamage` 오버라이드에서는 **`Super`의 반환값이 아니라 인자로 들어온 `DamageAmount`를 적용할 것**
- `UMinimapWidget` (`UI/MinimapWidget.h/.cpp`, 2026-09-24) — 화면 우상단 미니맵. **WBP 없이 C++ 클래스로 바로 생성**(`ALoAPlayerController::BeginPlay`, 레벨에 `AHexArena`가 있을 때만) → 에디터 할당 불필요
  - 그림은 전부 `NativePaint`에서 Slate 커스텀 버텍스(`FSlateDrawElement::MakeCustomVerts` + `WhiteBrush` 리소스 핸들)로 직접 그림: 배경 박스 → 아레나 타일 육각형(전부 같은 색 — 오염 장판 등 타일 상태는 일부러 표시 안 함) → 보스 빨간 마름모 → 플레이어 초록 화살표(바라보는 방향)
  - 방향은 **카메라 Yaw 기준**(화면 위 = 미니맵 위). 보스가 숨김(`IsHidden`) 상태면 보스 아이콘도 숨김 — 그네·거울잇기에서 사라졌다 나타나는 위치가 바로 보임
  - `AddToViewport(5)` — 연기(-1)·HUD(0)보다 위, 스킬트리(10)보다 아래. 크기/여백/색은 `UMinimapWidget` 기본값(`MapSize` 220 등)
  - `UUserWidget`에 이미 `Padding` 멤버가 있어서 같은 이름 UPROPERTY는 UHT 에러(shadowing) — 그래서 `MapPadding`
- **HUD 위젯은 전부 루트 Visibility를 `HitTestInvisible`로 둘 것** — 클릭 이동 게임이라 바가 마우스 입력을 먹으면 그 영역을 클릭해도 캐릭터가 안 움직인다
- ⚠️ **`BP_LoAPlayerController`의 위젯 클래스 프로퍼티는 반드시 에디터 Details 패널에서 직접 할당할 것.**
  MCP(`ObjectTools.set_properties`)로 CDO(`Default__BP_LoAPlayerController_C`)에 써서 저장하면 **에디터 조회로는 값이 보이고 `save_assets`도 true를 반환하는데, PIE 런타임에서는 null이다.** Live Coding 리인스턴싱을 거치면서 날아가는 것으로 보임.
  실제로 `BossHPWidgetClass`/`CastBarWidgetClass`를 MCP로 넣었더니 두 위젯 다 PIE에서 아예 생성되지 않았고(`[CastBar] 생성 건너뜀 — CastBarWidgetClass:미설정`), 유저가 에디터에서 직접 넣은 `HUDWidgetClass`/`SkillTreeWidgetClass`만 정상 동작했다.
  **교훈: MCP CDO 쓰기는 에디터 조회로 검증하면 안 되고 PIE 로그로 검증해야 한다.** 위젯 생성 지점마다 성공/실패 `UE_LOG`를 남겨두면 이런 걸 바로 잡을 수 있다(`[HUD]`/`[CastBar]` 로그가 그 예)
  - 갱신 경로: `AEchidnaBoss::OnHPChanged`(ReceiveDamage마다, BeginPlay에서도 1회) → `ALoAPlayerController::OnBossHPChanged` → `UBossHPWidget::SetBossHP()`
  - 컨트롤러가 BeginPlay에서 `UGameplayStatics::GetActorOfClass`로 보스를 찾아 구독한다 — **레벨에 `AEchidnaBoss`가 없으면 위젯 자체를 만들지 않음**

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
- **Instant 완료** → `SkillPostDelay` 후 잠금 해제 → 큐 발동 (실제 적용값은 `BP_Sorceress`의 SkillManager 컴포넌트에 **0.1s**로 설정돼 있음 — C++ 기본값과 다르니 점유율 계산 시 주의)
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
- **보이지 않는 충돌벽 `BarrierMesh`** (2026-09-24): 벽 안쪽 면을 바닥부터 벽 위 `InvisibleBarrierHeight`(1500cm)까지 세운 PMC. 보스 백스텝(RetreatFan 홉·BackstepHeart·ReturningOrb)이나 넉다운으로 뜬 캐릭터가 **벽 윗면에 올라가 못 움직이던 버그** 수정용
  - **Pawn 채널만 Block, 나머지 Ignore** — 벽(`WallMeshes`)처럼 BlockAll이면 Visibility도 막아서 카메라 쪽 가장자리에서 마우스 클릭 이동 트레이스를 가로챈다. 그래서 별도 컴포넌트로 분리함

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

### 레이드 시작 배치 — 비활성 오염 장판 8칸 + 파란 테두리 타일 2칸 (2026-09-23)
- `AHexArena::BeginPlay` → 타일 스폰 → **다음 틱**에 `SetupInitialLayout()` (플레이어 폰이 스폰된 뒤에 돌아야 폰이 선 타일을 뺄 수 있어서 한 틱 미룸)
- **오염 장판 8칸**(`InitialPoopTileCount`)은 **비활성 상태로 깔린다 — 밟아도 매혹·데미지 없음**, 살짝 핑크빛. 나중에 특정 패턴이 `AHexArena::SetAllPoopTilesActive(true)`(또는 타일별 `SetPoopActive`)로 켜면 **빨간색 + 빨간 테두리**가 되고 그때부터 밟으면 매혹 스택 + 데미지
  - 폰(플레이어·보스)이 서 있는 타일만 제외(**파란 테두리 타일도 오염될 수 있음** — 비활성 오염이면 핑크 바닥 + 파란 테두리, 활성화되면 빨간 테두리가 우선), **시작부터 꽃이 피는 배치는 건너뜀**(후보를 넣었을 때 어떤 Normal 타일이 완전히 둘러싸이면 스킵)
  - 타일은 타입과 무관하게 **위에 서 있는 캐릭터를 항상 기억**한다 → 서 있는 도중에 활성화돼도 즉시 틱 시작(`RefreshPoopTicking`)
  - `SetTileType`으로 PoopZone이 아니게 되면 활성 플래그도 꺼짐. 꽃 개화 판정은 활성 여부와 무관하게 PoopZone 타입 기준
- **파란 테두리 2칸**(반정산 패턴용 정보): 외곽 링에서 랜덤 1칸 + 거기서 헥스 거리 `MarkerTileDistance`(2)인 **안쪽(외곽 아닌) 타일** 1칸. SideCount=4면 어느 외곽 타일에서 출발해도 후보가 3~4개 있음(검증). `MarkerTileCoords[0]`=외곽, `[1]`=안쪽
- 테두리는 **타일 타입이 아니라 별도 표시** — `HighlightMesh`(PMC)가 헥스 외곽선을 따라 바닥 링 + 낮은 띠를 그림. 우선순위: **활성 오염(빨강 `ActivePoopBorderColor`) > 파란 테두리(`HighlightColor`) > 숨김** (`RefreshBorder`). 타일 액터가 TileYaw(30도) 돌아 있어서 로컬 꼭짓점은 0/60/120...도, 꼭짓점 반지름 = 내접원 반지름 × 2/√3. 높이는 `TileMesh->Bounds` 꼭대기 기준
- 테두리 색은 `M_MirrorLaser`(Translucent+Unlit) `"Base Color"`에 HDR 값 → 1보다 큰 값이 블룸으로 빛남
- **타일 머티리얼**: `M_HexTileTinted`(= `M_Gemini` 복제 + `Tint` Vector로 텍스처 곱 → Base Color, `× Glow` Scalar → Emissive). `MI_HexTile_PoopInactive`(진한 핑크 틴트 — 처음 값(1,0.72,0.82)은 너무 옅어서 안 보였음, `ABP_HexTile.PoopMaterial`) / `MI_HexTile_PoopActive`(빨강 틴트 + 약한 발광, `ActivePoopMaterial` — C++ 생성자에서 기본값으로 박음). 색 조절은 두 MI의 `Tint`/`Glow`
- `WorldToTileCoord()`(월드 → q,r, 큐브 반올림), `GetHexDistance()`는 패턴에서 "플레이어가 어느 타일에 있나"·거리 판정에 사용

### 큰 꽃 (Flower 타일) — 2026-09-23
- 오염 장판에 완전히 둘러싸인 타일이 Flower가 되면(`NotifyTileTypeChanged`) **바닥은 항상 `PoopMaterial`(비활성 오염과 같은 핑크)** — 예전 `FlowerMaterial`(불 텍스처 `MI_Rock_Inst_7`)은 BP에서 MCP로 비워도 PIE에서 옛 값이 남아 불이 계속 나와서 **필드 자체를 삭제**함 + **큰 꽃**(`AEchidnaBigFlowerActor`)이 타일 위에 핌
- `AEchidnaBigFlowerActor`: 에셋 없이 PMC로 꽃잎 3겹(바깥 7장 → 안쪽 5장, 바깥일수록 크고 눕고 안쪽일수록 작고 섬, 색은 바깥 붉은 분홍 → 안쪽 살구/크림) + 꽃술 돔 + 바닥 잎 `LeafCount`(4)장(초록). `BloomDuration` 동안 ease-out-back으로 피고, 이후 천천히 회전·상하로 흔들림. 순수 비주얼 — 모양/색은 Class Defaults. 타일이 `RefreshFlower()`로 스폰/제거하고 `EndPlay`에서 정리(붙은 액터는 자동으로 안 사라져서)
- **매혹 오라**: `AHexArena::TickFlowerAura()`(1초 루핑 타이머, 항상 돎) — 꽃 타일과의 헥스 거리 `FlowerAuraRange`(1) 이하 타일(꽃 타일 자신 포함)에 서 있으면 `FlowerCharmAmount`(1) 매혹 스택. 꽃이 여러 개여도 1초에 한 번만. Flower 타일은 PoopZone이 아니라 오염 틱 데미지는 없음

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
  - `TrackingRotationSpeed` 기본값 60 → 100 → 80 → 50 → 15 → **8**도/초 (C++ 기본값 + `BP_EchidnaMirror` 둘 다). 걸어서 피할 수 있으려면 각속도×MaxRange(3000) < 걷기 600cm/s → 약 11도/초 이하
- 완료 후 `LifeAfterBeam` 뒤 소멸
- **8거울의 개인 유도레이저**(`FStateTreeTask_EchidnaEightMirrorPattern`): 패턴 시작 **`GuidedSpawnDelay`(2초) 뒤 보스 위치**에서 스폰돼 플레이어를 쫓아감 (2026-09-24 — 예전엔 시작 즉시 보스 오른쪽 500cm에 스폰돼서 그 자리 플레이어가 피할 틈 없이 맞았음. 이때 `GuidedHoverHeight` 필드를 삭제함)

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

## 보스 앞/뒤 방향 표시 (`Source/LoA/Raid/BossDirectionIndicatorComponent.h/.cpp`) — 2026-09-22

- 로스트아크의 정면/백어택 표시와 같은 역할 — 보스 발밑에 호(arc) 두 개를 그려 앞뒤를 알려준다
- **`UProceduralMeshComponent` 서브클래스**라 `AEchidnaBoss` 캡슐(루트)에 붙이기만 하면 **보스가 회전할 때 같이 돈다** — 매 틱 방향을 갱신하는 코드가 전혀 없음
- **정면 호(섹션 0, 로컬 +X)**: 가운데가 바깥으로 뾰족하게 튀어나옴 / **후방 호(섹션 1, 로컬 -X)**: 매끈한 고리 조각
- **뾰족한 부분은 별도 삼각형이 아니다** — `OuterRadius`를 각도의 함수로 두고, 중심에서 `SpikeHalfAngle`만큼 떨어지면 0이 되는 선형 보간(`Spike = SpikeLength * max(0, 1 - |각도차|/SpikeHalfAngle)`)으로 만든다. 호와 자연스럽게 이어지고 특수 케이스 처리가 없음. `FrontSpikeLength=0`이면 후방과 같은 매끈한 호가 됨
- **발밑 정렬**: `bSnapToOwnerFeet`이 BeginPlay에서 오너 캐릭터의 캡슐 절반 높이만큼 내린다. 안 하면 보스 허리 높이에 떠 보임(`GetActorLocation`이 캡슐 중심인 것과 같은 함정)
- 지오메트리/머티리얼 컨벤션은 HexArena 벽·부채꼴 장판·하트 메시와 동일 (`CreateMeshSection` + `M_MirrorLaser`의 `"Base Color"` Vector Parameter에 MID로 색 주입, 양면 감김)
- 반지름·각도·스파이크 길이/폭·색상·불투명도 전부 컴포넌트 Details에서 조절 가능

## 에키드나 보스 짤패턴 — 끌고간후 장판터지는 (`Source/LoA/Raid/EchidnaTetherActor.h/.cpp`, `EchidnaBossStateTreeUtility.h/.cpp`)

### 패턴 개요 (레퍼런스 이미지 "1. 끌고간후 장판터지는 패턴")
- 1단계: 보스 정면 기준 부채꼴로 줄기(`AEchidnaTetherActor`) `TetherCount`(기본 7)개를 동시에 뻗음 → `SnapDelay` 뒤 범위 안 대상을 1회 판정
- 2단계: **맞은 대상이 멈췄다가 보스 앞까지 끌려온 뒤에** 1번 장판(좁게) → 2번 장판(넓게) 순서로 터짐
- **아무도 안 맞아도 1번·2번 장판은 순서대로 터진다** (2026-09-24 변경 — 예전엔 `AnyTetherHit` false면 장판 없이 끝났음, 지금은 로그만 남김)

### 끌어당기기 = "잠깐 멈춤 → 강제 드래그" 2단계 (`ALoACharacter::ApplyPull`) — 2026-09-22
- **버그였던 것**: 예전엔 `ApplyPull`이 보스 방향으로 `LaunchCharacter` 임펄스 **한 번**을 주는 게 전부였다. 마찰·지형·현재 속도에 따라 도달 거리가 들쭉날쭉해서 "끌려간다"가 아니라 "살짝 밀린다"에 가까웠고, 멈추는 연출도 없었음
- 현재 동작: ①맞는 즉시 캐스팅/사거리이동을 끊고 `StopMovementImmediately()`로 **`PullHoldDuration`(기본 0.4초) 동안 제자리에 묶임** → ②`PullSpeedCmS`(StateTree의 `PullStrength`를 속도 cm/s로 재해석)로 목표 지점까지 **Tick에서 위치를 직접 보간**해 끌고 감 → ③도착하거나 `PullMaxDragTime`(기본 1.5초)이 지나면 해제
- 임펄스가 아니라 `AddActorWorldOffset(..., bSweep=true)`로 직접 옮기므로 **항상 보스 앞까지 확실히 도달**한다(벽은 스윕이 막아줌). `PullStopDistance`(기본 200cm)만큼 앞에서 멈춰 보스와 겹치지 않게 함
- **끌려간 뒤에도 패턴이 끝날 때까지 계속 묶여 있다** — 도착 시 `FinishPullDrag()`가 드래그만 멈추고 `bIsPulled`는 유지한다. 해제는 `ReleasePull()`이 담당하고, `FStateTreeTask_EchidnaDragFanPattern::ExitState`에서 호출된다(Succeeded/Failed 구분 없이 불리므로 패턴이 중간에 강제 전이돼도 반드시 풀림)
- **안전장치**: `PullMaxHoldTime`(기본 8초) 타이머가 `ReleasePull()`을 강제 호출한다. 이게 없으면 패턴이 비정상 종료될 때 플레이어가 영구히 못 움직인다 — 패턴 전체 길이보다 넉넉하게 잡을 것
- `bIsPulled`가 `IsActionLocked()`에 포함되어 멈춤·드래그·속박 내내 조작 불가. 연출 훅은 `OnPullVisualChanged(bool)` (경직/기절과 동일 패턴)
- **`PullStrength`의 의미가 바뀌었다** — 임펄스 세기가 아니라 **끌려가는 속도(cm/s)**. StateTree 인스턴스 데이터 레이아웃을 건드리면 배치된 Task가 Live Coding에서 크래시 나므로 필드를 지우지 않고 의미만 재해석한 것(`GuidedHoverHeight`와 같은 선례)

### 타이밍 — 끌려오는 중에 장판이 터지지 않도록
- `AEchidnaTetherActor::IsFinished()`는 예전엔 판정 즉시 true였는데, 그러면 아직 끌려오는 중에 다음 단계 장판이 터진다
- 이제 누군가 맞았으면 `PullResolveDelay`(기본 1.2초) 뒤 `FinishSnap()`에서야 `bSnapped=true`가 된다. **`ALoACharacter`의 `PullHoldDuration` + 실제 드래그 시간보다 넉넉히 잡을 것** (기본값 기준 0.4 + 드래그 ≒ 1.2초)
- 소멸(`SetLifeSpan`)도 `FinishSnap`에서야 예약한다 — 그 전에 사라지면 `AreTethersFinished`가 null을 "완료"로 세고 `DidHit()` 정보도 같이 날아가 "아무도 안 맞음"으로 오판한다

## 에키드나 보스 짤패턴 — 두번긋고 도넛장판 (`Source/LoA/Raid/EchidnaBossStateTreeUtility.h/.cpp`) — 2026-09-21

### 패턴 개요 (레퍼런스 이미지 "5. 두번긋고 도넛장판")
- 순서: **1번(우측 대각 슬래시)** → **2번(좌측 대각 슬래시)** → **3번(작은 도넛)이 터지는 동시에 보스가 하늘로 상승, 정점에서 대기** → **하강하며 4번(외곽 도넛, "원테두리만")이 예고→폭발** (`FStateTreeTask_EchidnaDonutSlashPattern`)
- 1~3번은 매혹 게이지 없음, **4번만 매혹 1스택** — 레퍼런스의 "1~3은 매혹스택 X" 그대로
- 새 액터 클래스 없이 기존 `AEchidnaFanZoneActor`(뒤로 빠지며 좌우장판·끌고간후 장판터지는에서 이미 쓰던 부채꼴/고리 액터)를 4번 다 재사용 — 1·2번은 **`SlashInnerRadius`(기본 350cm)~`SlashRange`(기본 850cm)의 얇은 부채꼴 고리(annulus)**로(레퍼런스처럼 중심이 아니라 호(arc) 끝부분만 타격, `SlashInnerRadius=0`이면 중심에서 뻗는 일반 부채꼴로 되돌아감), 3·4번은 **`FanAngle=360` 오버라이드로 원형 고리(도넛)**를 만듦 (`IsActorInRing`의 각도 판정이 반각 180도라 360도 부채꼴 = 완전한 원이 되는 걸 그대로 활용, 별도 원형 메시 코드 불필요). 범위·예고시간(`SlashTelegraphDuration` 등)은 플레이 피드백 기준으로 여러 차례 재조정됨 — 정확한 현재값은 헤더의 기본값 참고
- **보스는 패턴 시작~4번 종료까지 한 방향만 바라봄**: `RetreatFanPattern`/`DragFanPattern`은 슬래시마다 그 공격 방향으로 보스를 다시 회전시키지만("보스가 쏘는 방향을 쳐다봄" 패턴), 이 패턴은 반대로 **보스 모델이 절대 돌지 않아야** 함(레퍼런스 요구사항) — `Tick()` 맨 앞에서 매 프레임 `Boss->SetActorRotation(FRotator(0, BaseAimRotation.Yaw, 0))`을 강제 재적용해 어떤 이유로든(엔진 내부 로직 등) 회전이 흐트러지는 것을 방지. 슬래시 1·2번의 공격 자체는 `Slash1/2YawOffset`만큼 서로 다른 방향으로 나가지만(장판 메시 회전), 보스 자신의 Yaw는 그대로 고정됨
- **1·2번 각도 범위 — 넓은 반원 + 정면 겹침**: `SlashFanAngle=180`(절반씩 좌우로 90도) + `Slash1YawOffset=75`/`Slash2YawOffset=-75` 조합으로, 1번은 정면 기준 [-15도(살짝 왼쪽), 165도(우측 뒷편)]를, 2번은 [-165도(좌측 뒷편), 15도(살짝 오른쪽)]를 덮음 — 정면 [-15,15] 30도 구간은 1·2 둘 다 맞아 레퍼런스의 "가운데 진하게 겹치는" 부분과 일치, 정반대(보스 바로 뒤) 근처 30도는 둘 다 안 닿는 좁은 안전지대로 남음

### 이펙트 클래스 분리 — SlashZoneClass(1·2) / FanZoneClass(3) / OuterDonutClass(4번), 3종류
- **1·2번(슬래시)에 맞으면 살짝 경직(스태거)만, 3·4번(도넛)에 맞으면 넘어짐(넉다운)** — 정도가 다른 CC라 클래스를 3개로 분리:
  `SlashZoneClass`(경직 O `bApplyStaggerOnHit=true`/넉다운 X/매혹 X) / `FanZoneClass`(3번, 넉다운 O `bApplyKnockdownOnHit=true`/매혹 X) / `OuterDonutClass`(4번, 넉다운 O **+** 매혹 O `bApplyCharmGaugeOnHit=true` 둘 다 켬)
- `AEchidnaFanZoneActor::bApplyKnockdownOnHit`/`bApplyStaggerOnHit`/`bApplyCharmGaugeOnHit`는 스폰 시점에 오버라이드하지 않고 **BP 클래스 기본값**을 그대로 따름 (DragFan 패턴과 동일한 컨벤션)
- `SpawnZone()`이 매번 `FanAngle`/`FanInnerRadius`/`FanRange`를 명시적으로 덮어씀 (조건부 아님 — 슬래시/작은도넛/외곽도넛마다 값이 다 다르므로 항상 설정). `TelegraphDuration`은 선택적 오버라이드(음수면 무시, BP 기본값 유지)지만 **`RingCount`는 이 Task의 모든 호출부에서 항상 1을 강제로 넘김** — 아래 "전부 단발 판정" 참조

### "경직"(스태거) — 넉다운과 별개의 약한 히트리액션 (`ALoACharacter::ApplyStagger`, 2026-09-21)
- 넉다운(`ApplyKnockdown`)은 캐릭터를 띄우고 던져서 눕히는 강한 CC라 1·2번 슬래시 같은 잦은 히트에 매번 걸면 과함 — `bIsStaggered`/`StaggerDuration`(기본 0.3초) 필드와 `ApplyStagger()`/`IsStaggered()`를 새로 추가해 **캐릭터를 띄우거나 쓰러뜨리지 않고 짧게만 행동불능**으로 만듦
- `ApplyStagger()`: 이미 넉다운 중이면 무시(더 강한 상태), 아니면 `SkillManager->CancelActiveCastSkill()`/`CancelPendingRangeMove()`로 캐스팅만 끊고 `StopMovementImmediately()`로 제자리에 세운 뒤 `StaggerDuration` 타이머로 자동 해제(`EndStagger`) — 재히트하면 넉다운의 정착 타이머와 동일하게 갱신되어 계속 경직 유지
- `ALoACharacter::IsActionLocked()` = `bIsKnockedDown || bIsStaggered` 신설 — `LoAPlayerController`의 입력 차단 지점(Tick의 자동이동, `OnInputStarted`, `OnSetDestinationTriggered`, `OnSkillKeyDown/Held`)을 전부 `IsKnockedDown()` 단독 체크에서 이걸로 교체. `OnDashInput`만 예외: 넉다운 중엔 스페이스바=즉시기상이라 그 분기 유지하고, 그 아래에 `IsStaggered()`면 그냥 씹히도록 별도 return 추가
- 애니메이션 훅은 `OnStaggerVisualChanged(bool)` (BlueprintImplementableEvent) — 넉다운의 `OnKnockdownVisualChanged`/`OnKnockdownSettled`와 동일한 패턴이지만 경직은 "눕는 단계" 자체가 없어서 이벤트 1개로 충분
- `AEchidnaFanZoneActor::ApplyRingDamage`에서 `bApplyKnockdownOnHit`가 꺼져 있고 `bApplyStaggerOnHit`가 켜져 있을 때만 `ApplyStagger()` 호출 (`else if`로 넉다운이 항상 우선하도록 배치, 둘 다 켜는 건 권장 안 함)

### 보스 상승/하강 — LaunchCharacter 대신 MovementMode 직접 제어
- 넉백류 패턴(뒤로 빠지며 좌우장판의 `HopBackward` 등)은 `LaunchCharacter`로 물리에 맡기지만, 이 패턴은 **정점 대기 시간·착지 타이밍이 정확해야 해서** 물리 예측 대신 스크립트로 직접 제어
- **순서 고정 — 예고 → 폭발 → (그제서야) 상승**: `InnerDonutTelegraph` 단계에서는 3번 장판만 스폰해두고 보스는 그대로 지상에 둠(Flying 전환 없음) — `InnerDonut->IsFinished()`(예고+단발판정 완료)가 true가 되는 순간에만 `Rising` 단계로 넘어가 `CharacterMovementComponent->SetMovementMode(MOVE_Flying)` + `StopMovementImmediately()`를 호출함. **버그였던 것**: 예전엔 3번 스폰과 상승 시작을 같은 프레임에 같이 처리해서 "예고 보여주면서 이미 공중에 떠 있다가 뜬 채로 터지는" 것처럼 보였음(레퍼런스 요구사항 위반) — 두 단계(`InnerDonutTelegraph`/`Rising`)를 분리해서 해결
- `Rising` 단계 진입 후 Tick에서 매 프레임 `GroundActorZ + Lerp(0, RiseHeight, alpha)`로 `SetActorLocation` 직접 호출 (Flying이라 중력이 안 걸리고 바닥 스냅도 안 일어남) — `RiseDuration` 동안 올라간 뒤 `ApexHoldDuration`만큼 `RiseHeight`에서 순수 연출용으로 고정 대기(이 시점엔 3번은 이미 다 터진 뒤라 도넛 완료 여부를 더 체크할 필요 없음)
- 대기가 끝나면 외곽 도넛(4번)을 먼저 스폰(하강 시작과 동시에 예고가 보이게)한 뒤 `FallDuration` 동안 역방향으로 Lerp — 하강 완료(`FallAlpha>=1`) 시점에 `EndRiseFall()`로 `MOVE_Walking` 복구 + Z를 `GroundActorZ`로 정확히 스냅. 착지 타이밍과 4번 폭발 타이밍을 맞추려면 `OuterDonutTelegraphDuration`을 `FallDuration`과 비슷하게 맞춰둘 것
- **안전 복구**: 패턴이 도중에 끊겨도(다른 State로 강제 전이 등) 보스가 공중에 뜬 채로 남지 않도록 `ExitState`에서도 `MovementMode == MOVE_Flying`이면 `EndRiseFall()` 호출

### 1~4번 전부 "예고 후 계단식 확장"이 아니라 "예고 후 단발 판정" (2026-09-21 수정)
- **버그였던 것**: `AEchidnaFanZoneActor`의 RingCount 계단식 확장(안→밖으로 훑으며 여러 번 판정)은 "뒤로 빠지며 좌우장판" 전용으로 만든 연출인데, 이 패턴도 같은 액터를 재사용하면서 BP 기본 RingCount(보통 5)를 그대로 물려받아 1~4번 전부 점진적으로 여러 번 맞는 것처럼 보였음 — 이 패턴은 "예고 후 딱 한 번만" 때리는 게 목표라 안 맞았음
- `SpawnZone()`의 4번 호출부(Slash1/Slash2/InnerDonut/OuterDonut) 전부에서 `RingCountOverride`에 항상 **1**을 명시적으로 넘기도록 변경 — BP의 RingCount 기본값이 몇이든 무시되고 예고(`TelegraphDuration`) 후 한 번에 전체 판정이 끝남
- 예고시간은 존마다 별도 필드로 오버라이드: 슬래시(1·2)는 `SlashTelegraphDuration`, 작은 도넛(3)은 `InnerDonutTelegraphDuration`(예고→폭발이 끝나야 상승이 시작되므로 `RiseDuration`과의 선후관계는 더 이상 신경쓸 필요 없음 — 완전히 순차적), 외곽 도넛(4)은 `OuterDonutTelegraphDuration`(`FallDuration`과 맞춰둠) — 전부 음수면 BP 기본 `TelegraphDuration`을 그대로 쓰지만 이 Task는 항상 명시적인 값을 넘김

### "에어본"은 별도 CC 시스템이 아니라 기존 넉다운 재사용 — 캐릭터도 "뜨고 → 떨어지고 → 넘어짐" 순서로 진행됨
- 레퍼런스의 "3번 내부 에어본"은 플레이어 전용 CC가 아니라 **보스 자신이 하늘로 뜨는 연출** + 3번 판정에 맞은 플레이어는 기존 `ALoACharacter::ApplyKnockdown()`(넉다운 시스템 섹션 참조)을 그대로 재사용
- `ApplyKnockdown()`은 이미 "공격 반대 방향+위로 `LaunchCharacter`(뜸) → `KnockdownHopSettleTime`(기본 0.4초) 뒤 강제로 바닥까지 스냅(떨어짐) → 그 순간부터 `KnockdownDuration`(기본 3초) 동안 누운 포즈(넘어짐)" 순서로 진행되므로, "에어본이니 캐릭터도 올라가면서 떨어진 후 넘어지는 판정이 나야" 하는 요구사항을 그대로 만족함 — 별도 구현 불필요, `FanZoneClass`의 `bApplyKnockdownOnHit=true` 설정만으로 충분
- 이 프로젝트엔 아직 별도의 "띄우기(launch/juggle)" CC가 구현되어 있지 않아서 기존 넉다운으로 대체한 것 — 나중에 진짜 보스 상승 높이/시간에 맞춰 캐릭터가 더 오래 공중에 떠 있어야 한다면 `KnockdownHopStrength`/`KnockdownHopUpwardStrength`/`KnockdownHopSettleTime`을 패턴별로 오버라이드할 수 있게 `ApplyKnockdown`을 확장해야 함(현재는 전역 값)

### StateTree 배치 — BP 3종 분리 완료, 필드 재할당 남음 (2026-09-21)
- 4거울/뒤로빠지며좌우장판과 동일한 패턴: `SmallPatternRotation` 안에 `Echidna Donut Slash Pattern` Task 하나만 넣고 On State Completed → `Cooldown` 연결 — 이미 배치됨
- **버그였던 것**: Task의 `SlashZoneClass`/`FanZoneClass`/`OuterDonutClass` 세 필드가 전부 `BP_EhidnaFanZone`(기존 넉다운 전용 BP, 이름에 오타 있음 — "Echidna"가 아니라 "Ehidna") 하나만 가리키고 있어서, 1·2번 슬래시도 3·4번과 똑같이 넉다운이 걸렸음(경직 아님) — Unreal MCP로 직접 Task 인스턴스 데이터를 읽어서 확인
- Unreal MCP로 새 BP 2개를 생성해 기본값까지 설정·컴파일·저장 완료: `BP_EchidnaFanZone_Stagger`(`bApplyKnockdownOnHit=false`/`bApplyStaggerOnHit=true`, 1·2번용) / `BP_EchidnaFanZone_KnockdownCharm`(`bApplyKnockdownOnHit=true`/`bApplyCharmGaugeOnHit=true`, 4번용). 3번(`FanZoneClass`)은 기존 `BP_EhidnaFanZone`(넉다운 O/경직·매혹 X) 그대로 재사용
- **StateTree 노드의 클래스 참조 자체를 MCP로 재할당하는 API는 없어서(읽기 전용 툴만 제공)** `SlashZoneClass`→`BP_EchidnaFanZone_Stagger`, `OuterDonutClass`→`BP_EchidnaFanZone_KnockdownCharm` 재할당은 에디터에서 직접 드롭다운으로 바꿔야 함 (`FanZoneClass`는 그대로 유지)

## 에키드나 보스 짤패턴 — 전방향 하트발사 (`Source/LoA/Raid/EchidnaHeartActor.h/.cpp`, `EchidnaBossStateTreeUtility.h/.cpp`) — 2026-09-21

### 패턴 개요 (레퍼런스 이미지 "7. 전방향 하트발사" 단순화 버전)
- 레퍼런스 원본은 "①머리위 하트 표시 → ②전방향 하트발사 → ③피격자 중 랜덤 1명 잡기+무력패턴(딜타임) → ④잡힌 사람 씨앗 생성" 4단계지만, 이번 구현은 ①·②만 하고 ③·④(잡기/씨앗)는 제외한 단순화 버전 — 대신 맞은 사람에게 스턴+매혹을 적용
- 흐름(`FStateTreeTask_EchidnaHeartBurstPattern`): 패턴 시작 → `TelegraphDuration`(기본 2초) 동안 보스 머리 위에 하트 마커 표시, 보스는 완전히 가만히 있음(이동/회전 명령 없음) → 예고가 끝나면 `FireDuration`(기본 2초) 동안 `FireInterval`(기본 0.3초)마다 한 웨이브씩 하트를 발사 → `FireDuration`이 다 지나면 (날아가는 하트가 남아있어도) 바로 Succeeded
- **8방향 고정이 아니라 완전 전방향**: 처음엔 8방향(45도 간격)을 Fisher-Yates로 섞어 그중 일부만 쏘는 방식이었으나, "8방향이 아니라 전방향으로" 요청에 따라 각 하트의 각도를 `FMath::FRandRange(0.f, 360.f)`로 매번 완전히 새로 뽑는 방식으로 교체 — 이제 방향에 정해진 슬롯 자체가 없음. 웨이브당 개수만 `[MinHeartsPerWave, MaxHeartsPerWave]`(기본 2~6, 더 이상 8개로 상한 없음) 범위에서 랜덤으로 정함 — 매번 개수도 방향도 전부 달라짐
- 맞으면: 데미지 + `HeartStunDuration`(기본 3초) 완전 기절(`ALoACharacter::ApplyStun`) + **발밑에 오염 장판 게이지**(`AEchidnaPoopMarkActor` — 똥장판 패턴의 그 5초 원형 게이지, 다 차면 서 있는 타일이 **비활성** 오염 장판)
  - 변경(2026-09-23): 예전엔 매혹 1스택이었는데 오염 장판 생성으로 바꿈. Task가 스폰한 하트마다 `SetSpawnPoopMarkOnHit(true)` — **백스탭 하트발사는 같은 하트 액터지만 이걸 안 켜서 여전히 매혹 스택**
  - 이미 기절 중이면(직전 하트에 맞음) 게이지를 또 붙이지 않고, 그 플레이어에게 진행 중인 게이지가 있어도 중복 생성 안 함
  - **게이지는 하트 기절(3초)이 풀린 뒤에 시작**(`Activate(..., bWaitForStunEnd=true)` — 기절 중엔 숨긴 채 대기)

### AEchidnaHeartActor — 예고 마커와 발사체를 하나의 클래스로 겸용
- `Launch()`를 호출하기 전까지는 `bLaunched=false`라 Tick에서 이동하지 않고 제자리에 가만히 떠 있음 — 이 성질을 그대로 이용해서 **같은 클래스를 예고 마커(Launch 안 함)와 실제 발사체(Launch 함) 양쪽에 재사용**함(새 클래스 하나 안 만들어도 됨)
- 이동은 `Tick`에서 `AddActorWorldOffset(FlyDirection * Speed * DeltaTime, true)`로 직선 이동, `USphereComponent`(기본 반지름 40cm, `ECC_Pawn`만 Overlap)로 캐릭터 감지 — 맞으면 데미지+스턴+매혹 적용 후 즉시 `Destroy()`, `MaxRange`(기본 2000cm)에 도달하면 아무도 안 맞아도 소멸
- `bHasHit` 플래그로 한 번만 판정 (여러 캐릭터가 있어도 첫 번째로 겹친 대상 하나만 맞음 — 관통 안 함)
- **크기 노출(2026-09-21)**: 처음엔 `HeartVisualScale`(메시 스케일)/`HeartCollisionRadius`(판정 반지름)가 생성자에 하드코딩돼 있어서 에디터에서 조절 불가능했음 — `EditDefaultsOnly` 프로퍼티로 빼서 `BeginPlay`에서 적용하도록 변경(생성자에서 바로 적용하면 BP 서브클래스의 오버라이드가 반영 안 됨 — CDO 생성 순서상 BP 오버라이드는 C++ 생성자 실행 "이후"에 적용되므로, 프로퍼티를 실제로 사용하는 시점(BeginPlay)에서 읽어야 함). `BP_EchidnaHeart`(`/Game/LostArk/Raid/Echidna/Pattern/`)를 Unreal MCP로 생성해서 Class Defaults에 노출해둠

### 비주얼 — 하트 모양 시행착오: 텍스처 스프라이트 → 3D 압출 솔리드 메시로 최종 정착 (2026-09-21)
- **1차 시도(폐기): 평면 스프라이트**. 엔진 기본 `Sphere`+`M_MirrorLaser`(핑크색 구체) → "하트 모양으로" 요청에 따라 PIL로 하트 실루엣 텍스처(`T_HeartShape`)를 만들고 `M_HeartSprite`(Translucent+Unlit, 텍스처 알파를 `MP_Opacity`로 연결) 머티리얼을 Unreal MCP `MaterialTools`로 그래프까지 구성, `HeartMeshComp`를 `/Engine/BasicShapes/Plane`으로 교체하는 방식으로 구현했었음
- 이 스프라이트 방식에서 연달아 겪은 문제들(전부 결국 폐기된 접근이라 세부 삽질 과정은 생략) — ①BP 컴포넌트의 머티리얼/회전 오버라이드가 C++ 기본값 변경 이후에도 예전 값에 얼어붙어 있는 문제가 반복 발생 ②`ST_Echidna`의 `Echidna Heart Burst Pattern` Task가 애초에 `BP_EchidnaHeart`가 아니라 **네이티브 클래스(`/Script/LoA.EchidnaHeartActor`) 자체**를 `Heart Class`로 물고 있어서 BP에 넣은 수정이 실제 스폰에는 반영되지 않았음 ③네이티브 클래스의 CDO를 `ObjectTools.set_properties`로 런타임 패치해도 Live Coding 재컴파일 등의 시점에 조용히 예전 값으로 되돌아가는 현상까지 겹쳐서, "평면+텍스처+알파+회전 정렬"이라는 조합 자체가 이 프로젝트의 MCP 워크플로우와 상성이 나빴음
- **2차 시도(최종): ProceduralMeshComponent로 실제 압출된 3D 솔리드 메시**. "3D/VFX처럼 입체적으로 안 되면 구체로 대체" 요청에 따라, 텍스처/알파/회전 정렬 문제 자체를 없애기 위해 HexArena 벽·부채꼴 장판과 동일한 "PMC로 직접 지오메트리 생성" 방식으로 전환:
  - `HeartMeshComp` 타입을 `UStaticMeshComponent` → `UProceduralMeshComponent`로 교체
  - 파라메트릭 하트 커브(`X=16sin³(t)`, `Y=13cos(t)-5cos(2t)-2cos(3t)-cos(4t)`)로 하트 윤곽선을 `HeartSegments`(기본 32)개 점으로 계산 — 원점 기준 스타컨벡스(star-convex) 도형이라 앞/뒷면은 원점에서 팬(fan) 삼각분할, 옆면(`HeartThickness`, 기본 20cm 두께)은 변마다 쿼드 하나씩 압출(HexArena `AddQuad`와 동일한 "양면 감김" 패턴으로 노멀 계산 없이도 항상 보이게 함)
  - **빌드 에러였던 것**: `Boundary`가 `TArray<FVector2D>`인데 옆면 노멀 계산에서 `EdgeDir`를 `FVector`(3D)로 잘못 선언해서 `C2440` 타입 변환 에러 — `FVector2D`로 수정
  - Opaque 솔리드라 반투명/알파 마스킹이 통째로 필요 없어짐 — 머티리얼도 다시 기존 `M_MirrorLaser`(색상 하나만 있는 Unlit 컨벤션)로 되돌림, 텍스처 관련 에셋(`T_HeartShape`, `M_HeartSprite`)은 이제 안 씀(정리는 안 했으니 필요시 삭제해도 무방)
  - `BeginPlay`에서 `HeartVisualScale`(전체 스케일) 적용 후 `BuildHeartMesh()` 호출 — 다른 컴포넌트 프로퍼티들과 같은 "EditDefaultsOnly는 생성자가 아니라 BeginPlay에서 적용" 패턴 유지
- **3차 수정 — "누워있는" 원반을 세우고 비행 중 회전 추가 (2026-09-21)**: 처음 압출했을 때는 하트 곡선을 로컬 X-Y 평면에 놓고 Z를 두께로 압출해서(`(LocalX,LocalY)=(-CurveY,CurveX)`, 뾰족한 끝이 +X를 향함) 탑다운 카메라에서 보면 "바닥에 눕혀진 원반"처럼 보였음 — "세워주고 날아가면서 천천히 회전"하도록 좌표축을 다시 바꿈: 이제 하트 곡선을 로컬 **Y(좌우)-Z(상하) 평면**에 그대로 놓고(`Boundary=(CurveX,CurveY)`, 부호 반전 없음 — 커브 자체가 뾰족한 부분이 Y최솟값이라 그대로 두면 아래(-Z)를 향함) 두께는 로컬 **X(전후) 방향**으로 압출 — 세워진 카드처럼 정면(+X)을 보는 얇은 판이 됨
  - `Tick()`에서 `bLaunched`(실제 발사된 뒤)일 때만 `HeartMeshComp->AddLocalRotation(FRotator(0,HeartSpinSpeed*DeltaTime,0))`으로 로컬 Z축(세로) 기준 회전 — 액터 자체가 아니라 메시 컴포넌트만 도는 것이라 충돌 판정(`CollisionComp`, 구체라 회전 무관)에는 영향 없음. `HeartSpinSpeed`(기본 60도/초) EditDefaultsOnly로 노출. 예고 마커(Launch 전)는 이 분기 자체가 안 타서 계속 세워진 채 고정됨
- **컴파일 관련 중요 주의사항**: `HeartMeshComp`의 **컴포넌트 타입 자체를 바꾼 변경**(UStaticMeshComponent→UProceduralMeshComponent)이 포함되어 있어서 지금까지의 함수 본문 수정과는 차원이 다름 — Live Coding(Ctrl+Alt+F11)이 UPROPERTY 타입 변경까지 안정적으로 핫패치한다는 보장이 없으므로, **에디터를 완전히 닫고 풀 빌드하는 걸 권장**. 어중간하게 Live Coding으로 넘어가면 이전 여러 번처럼 "고쳤다고 했는데 그대로"인 상태가 재발할 위험이 큼

### "기절"(Stun) — 경직·넉다운과 별개의 세 번째 CC 등급 (`ALoACharacter::ApplyStun`, 2026-09-21)
- 이 프로젝트의 CC 3종 비교: **넉다운**(캐릭터를 띄우고 던져서 눕힘, 3초+즉시기상) > **기절**(캐릭터를 안 띄우고 제자리에 완전히 얼어붙음, Duration은 호출마다 다름 — 예: 하트발사 3초) > **경직**(기절과 동일한 방식이지만 훨씬 짧음, 고정 0.3초)
- `ApplyStun(float Duration)`은 `ApplyStagger()`와 구현이 거의 동일(캐스팅 취소+`StopMovementImmediately`+재히트 시 타이머 갱신, 넉다운 중이면 무시)하지만 **Duration을 고정 필드가 아니라 매번 인자로 받음** — 패턴마다 기절 시간이 다르기 때문(경직은 모든 패턴이 공통으로 짧게 쓰므로 고정 필드 `StaggerDuration` 유지)
- `ALoACharacter::IsActionLocked()` = `bIsKnockedDown || bIsStaggered || bIsStunned`로 확장 — 이미 이 함수를 참조하던 `LoAPlayerController`의 모든 입력 차단 지점이 코드 변경 없이 자동으로 기절도 함께 차단함
- 애니메이션 훅은 `OnStunVisualChanged(bool)` (BlueprintImplementableEvent), 경직과 마찬가지로 "눕는 단계" 없이 이벤트 1개로 충분

### StateTree 배치 (미완 — 아래 "할일" 참조)
- `SmallPatternRotation` 안에 `Echidna Heart Burst Pattern` Task 하나만 넣고 On State Completed → `Cooldown` 연결 (다른 패턴들과 동일 컨벤션)
- `Boss`/`AIController` 컨텍스트 바인딩, `HeartClass`에 `AEchidnaHeartActor` BP 서브클래스 할당 필요
- **주의**: 새 C++ 클래스(`AEchidnaHeartActor`)와 새 StateTree Task/Enum 타입을 추가한 변경이라 Live Coding(Ctrl+Alt+F11)이 새 UCLASS/USTRUCT 리플렉션 타입을 못 잡아낼 수 있음 — 에디터에서 새 Task가 노드 목록에 안 뜨면 에디터를 완전히 닫고 풀 빌드해야 함

## 에키드나 보스 짤패턴 — 백스탭 후 하트발사 (`EchidnaBossStateTreeUtility.h/.cpp`) — 2026-09-22

### 패턴 개요 (레퍼런스 이미지 "8. 백스탭 후 하트발사")
- 보스가 **플레이어를 바라본 채 뒤로 크게 튕겨나가고**, 착지할 즈음 정면으로 하트 4개를 부채꼴로 발사 (`FStateTreeTask_EchidnaBackstepHeartPattern`)
- Phase: `Backstep`(튕겨나가는 중, 매 틱 플레이어를 다시 바라봄) → `Fire`(하트 발사 후 `FireLingerDuration` 대기) → Succeeded
- **새 액터 클래스 없음** — "전방향 하트발사"의 `AEchidnaHeartActor`를 그대로 재사용한다(맞으면 데미지 + 기절 + 매혹 1스택)
- 레퍼런스의 "상태이상은 정화로 해제가능"은 이 프로젝트에 정화 시스템이 없어 **기존 기절(`ApplyStun`)로 대체**함

### 조준 — 백스텝 중에는 계속 추적, 발사 순간에 고정
- `FacePlayer()`가 플레이어 방향을 구해 `Boss->SetActorRotation()`으로 보스를 즉시 돌리고 그 Rotation을 반환한다. `Backstep` 단계에서 **매 틱** 호출되므로 뒤로 밀려나는 동안에도 정면이 계속 플레이어를 향한다(레퍼런스 그림의 "물러나면서 앞으로 쏘는" 모양)
- 발사 직전 시점의 값이 `BaseAimRotation`으로 남아 부채꼴 기준선이 되고, `FireFan()`은 이 기준선 ± 오프셋만 쓴다 — RetreatFan과 달리 패턴 **시작** 시점이 아니라 **발사** 시점에 고정되는 게 차이점
- 보스가 `bOrientRotationToMovement=false`(EchidnaBoss 생성자)라 `LaunchCharacter`로 밀려나도 이동 방향으로 자동 회전하지 않는다 — 이게 없으면 뒤로 밀리는 방향을 쳐다보게 됨

### 부채꼴 각도 분배
- `FireFan()`: 하트 i의 Yaw 오프셋 = `Lerp(-FanSpreadAngle/2, +FanSpreadAngle/2, i/(Count-1))` — 개수와 무관하게 **양 끝이 항상 부채꼴 경계**에 오고 나머지는 균등 분포. `HeartCount=1`이면 정면 하나만
- 기본값 `HeartCount=4`, `FanSpreadAngle=60도` → -30 / -10 / +10 / +30도
- **발사 높이**: `GetActorLocation()`은 캡슐 중심이라 그대로 쓰면 하트가 떠 보인다. 캡슐 절반 높이를 빼 발밑으로 내린 뒤 `HeartFireHeight`(기본 100cm, 플레이어 캡슐 중심 높이와 비슷)를 더한다 — 안 맞추면 플레이어 위를 그냥 지나감(전방향 하트발사에서 이미 겪은 문제)

### 백스텝 — LaunchCharacter + 낙사 방지
- `Backstep()`은 `FacePlayer` 직후 호출되므로 "정면의 반대"가 곧 플레이어 반대 방향
- 착지 예상 지점(`BackstepCheckDistance` 뒤)에 수직 라인트레이스(`ECC_Visibility`)로 바닥이 없으면 **`LaunchCharacter` 자체를 건너뛴다** — 맵 끝자락에서도 떨어지지 않고 제자리에서 발사만 함 (RetreatFan의 `HopBackward`와 동일한 안전장치)
- `BackstepDuration`(기본 0.6초)은 "튕겨나가서 착지할 때쯤 발사"되도록 맞춘 값 — `BackstepStrength`/`BackstepUpwardStrength`를 바꾸면 체공 시간이 달라지므로 같이 조정할 것

### StateTree 배치 (미완)
- `SmallPatternRotation` 안에 `Echidna Backstep Heart Pattern` Task 하나만 넣고 On State Completed → `Cooldown` 연결
- `Boss`/`AIController` 컨텍스트 바인딩 + `HeartClass`에 **`BP_EchidnaHeart`** 할당 필요 (네이티브 `EchidnaHeartActor`가 아니라 BP를 넣을 것 — 전방향 하트발사에서 이걸 잘못 물려 한참 헤맸음)
- **새 Task/Enum 타입 추가라 Live Coding으로는 노드 목록에 안 뜰 수 있음** — 에디터를 닫고 풀 빌드할 것

## 에키드나 보스 짤패턴 — 되돌아오는 구체(자야패턴) (`Source/LoA/Raid/EchidnaOrbActor.h/.cpp`, `EchidnaBossStateTreeUtility.h/.cpp`) — 2026-09-22

### 패턴 개요 (레퍼런스 이미지 "10. 되돌아오는 구체(자야패턴)")
- 큰 구체를 전방으로 던지는데 **한 번 나간 구체는 그 경로를 그대로 되짚어 돌아온다** — 나갈 때 피했어도 돌아올 때 다시 맞을 수 있다("6개 구체 모두 되돌아오니 주의")
- 순서(`FStateTreeTask_EchidnaReturningOrbPattern`): 플레이어 조준 → 1번째 → `ThrowInterval`(1.2초) 뒤 **다시 조준해서** 2번째 → 백스텝 → 착지 무렵 정면 부채꼴 `FanOrbCount`(4)개 → 총 6개가 전부 돌아오면 Succeeded
- **조준 방식이 앞뒤로 다르다**: 앞의 2개는 발사 시점마다 다시 조준해 플레이어를 쫓아가고, 부채꼴 4개는 발사 직전 `BaseAimRotation`으로 한 번 고정해 각도 간격을 일정하게 유지 (RetreatFan·BackstepHeart와 같은 방침)

### AEchidnaOrbActor — Outgoing → Returning 2단계
- `Outgoing`: `FlyDirection`으로 `MaxRange`(기본 1400cm)까지 전진 → `Returning` → `Done`(`IsFinished()`가 true, StateTree가 폴링)
- **복귀는 발사 지점에서 멈추지 않는다.** `+MaxRange → 0(발사 지점) → -MaxRange`로 관통해서 반대편까지 가므로, 복귀 구간의 이동 거리는 `MaxRange`가 아니라 **`2*MaxRange`**이고 구체가 훑는 총 길이는 보스 앞뒤를 합쳐 `2*MaxRange`다. **보스 뒤에 서 있어도 안전하지 않다**
- **피격 기록(`AlreadyHit`)은 왕복 구간마다 초기화**한다(`BeginReturn`) — 한 구체에 최대 두 번(나갈 때 1회, 돌아올 때 1회) 맞을 수 있고 한 구간 안에서는 중복 피격이 없다
- **하트와 달리 맞아도 소멸하지 않는다** — 계속 날아가 되돌아와야 하므로. 이 차이 때문에 `bHasHit` 단일 플래그가 아니라 `TSet` 기록 방식을 쓴다
- 비주얼은 엔진 기본 Sphere + `M_MirrorLaser`(색상만) — 거울 액터와 같은 컨벤션이라 VFX 에셋 없이도 보인다
- ⚠️ **크기/판정 반지름은 컴포넌트가 아니라 Class Defaults에서 바꿀 것.** `BeginPlay`가 `OrbVisualScale`/`OrbCollisionRadius` 값으로 `OrbMeshComp->SetRelativeScale3D()`·`CollisionComp->SetSphereRadius()`를 **강제로 덮어쓴다.** BP 컴포넌트 트리에서 `CollisionComp`의 Sphere Radius나 `OrbMeshComp`의 Scale을 직접 만지면 BeginPlay가 즉시 되돌려놔서 "오버라이드가 안 먹는" 것처럼 보인다 (하트·거울 액터도 같은 구조)
- **StateTree에서도 조절 가능**: Task의 `Orb|Override` 카테고리 4개(`OrbCollisionRadiusOverride`/`OrbVisualScaleOverride`/`OrbSpeedOverride`/`OrbMaxRangeOverride`)를 `AEchidnaOrbActor::ApplyOverrides()`가 스폰 직후 적용한다. **음수면 BP 값을 그대로 쓴다.** BeginPlay 뒤에 호출되므로 BP 기본값을 확실히 덮어쓴다
- 발사 높이는 Task가 캡슐 절반 높이를 빼 발밑으로 내린 뒤 `OrbSpawnHeight`(100cm)를 더한다 — 안 맞추면 플레이어 위를 그냥 지나간다(하트발사에서 이미 겪은 문제)

### 안전장치
- 착지 예상 지점에 바닥이 없으면 백스텝을 건너뛰고 제자리에서 쏜다(낙사 방지 — RetreatFan과 동일)
- `MaxWaitDuration`(기본 10초)이 지나면 아직 안 돌아온 구체가 있어도 패턴을 끝낸다 — 구체가 지형에 끼면 State가 영영 안 끝나기 때문

### StateTree 배치 (미완)
- `SmallPatternRotation` 안에 `Echidna Returning Orb Pattern` Task 하나만 넣고 On State Completed → `Cooldown` 연결
- `Boss`/`AIController` 바인딩 + `OrbClass`에 **`BP_EchidnaOrb`**(`/Game/LostArk/Raid/Echidna/Pattern/`) 할당 — 네이티브 클래스가 아니라 BP를 넣을 것

## 에키드나 보스 짤패턴 — 정면 리본 공격 (`EchidnaBossStateTreeUtility.h/.cpp`, `EchidnaTetherActor.h/.cpp`) — 2026-09-22

### 패턴 개요 (레퍼런스 이미지 "11. 정면 리본 공격")
```
정면 2갈래 리본
├─ 맞음   → 기절 3초 + 매혹 1스택 → 보스 중심 원형 장판(넉다운) → 종료
└─ 빗나감 → 리본 끝자락으로 이동 → 플레이어 재조준 → 2차 리본
             ├─ 맞음   → 원형 장판 → 종료
             └─ 빗나감 → 좌측 전방 호(arc) 내려치기 → 종료
```
- **피격 여부로 갈라지는 분기 패턴** — 다른 짤패턴들이 정해진 순서를 그대로 진행하는 것과 다르다. `AnyRibbonHit()` 결과로 다음 Phase가 결정된다

### 새 액터 없이 기존 둘을 재사용
- **리본 = `AEchidnaTetherActor`**. 끌기 전용이던 액터에 피격 효과 스위치(`bApplyPullOnHit`/`bApplyStunOnHit`/`StunDuration`/`CharmGaugeAmount`)를 추가해 BP 설정만으로 두 패턴이 갈라지게 했다

  | | `BP_EchidnaTether`(끌고간후) | `BP_EchidnaRibbon`(리본) |
  |---|---|---|
  | `bApplyPullOnHit` | true | **false** |
  | `bApplyStunOnHit` | false | **true** (3초) |
  | `CharmGaugeAmount` | 0 | **1** |
  | `TetherRange` | 1200 | 1600 |

- **끌기를 안 쓰면 `PullResolveDelay`를 기다리지 않는다** (`PerformSnap`의 `!bDidHit || !bApplyPullOnHit` 조건) — 안 그러면 리본에 맞고도 1.2초를 멍하니 기다린다
- **원형 장판 / 호 = `AEchidnaFanZoneActor`**. 원형은 `FanAngle=360`(도넛과 같은 수법), 호는 `ArcInnerRadius`를 준 얇은 고리(두번긋고 도넛장판의 슬래시와 같은 방식). 둘 다 `RingCount=1`로 강제해 "예고 후 단발 판정"만 나오게 함
- **리본 자체는 데미지 0** — 기절+매혹만 주고 데미지는 뒤이어 터지는 원형 장판이 담당한다
- **버그였던 것 (겹친 구간에서 매혹 2스택)**: 2갈래는 서로를 모르는 별개 액터라 각자 `AddCharmGauge(1)`을 불렀고, 둘 다 맞으면 한 번에 2스택이 쌓였다. **먼저 맞은 갈래가 걸어둔 기절을 신호로 삼아** 중복을 막는다(`bAlreadyStunned`면 매혹 건너뜀) — 전방향 하트발사의 "기절 중 매혹 중복 축적 방지"와 같은 방식. 두 갈래의 `PerformSnap`은 같은 `SnapDelay` 타이머라 같은 틱에 순차 실행되므로 이 검사가 성립한다. **`ApplyStun()` 호출 전에 검사해야 한다** — 부르는 순간 `bIsStunned`가 true가 되어 판정이 무의미해짐

### 리본 끝자락 이동 — NavMesh가 아니라 직접 보간
- `MoveToLocation`은 도착 시점이 들쭉날쭉해 다음 리본 타이밍이 어긋나고 NavMesh 유무에도 의존한다. `MoveDuration` 동안 `SetActorLocation(Lerp(...), bSweep=true)`로 직접 옮긴다(두번긋고 도넛장판의 상승/하강과 같은 방침)
- 목표 지점은 스폰된 리본 액터의 `TetherRange`를 실제로 읽어 계산한다(`RibbonEndLocation`) — 길이를 바꾸면 이동 거리가 자동으로 따라온다
- **길이/폭은 StateTree에서도 조절 가능**: Task의 `Ribbon|Override` 2개(`RibbonRangeOverride`/`RibbonHalfWidthOverride`, 음수면 BP 값 유지)를 `SpawnRibbons()`가 **`Activate()` 호출 전에** 덮어쓴다. `Activate()`가 이 값으로 표시 메시와 판정 박스를 만들기 때문에 순서가 중요하다 (Orb는 `BeginPlay`가 값을 적용해서 별도 `ApplyOverrides()`가 필요했지만, Tether는 `Activate()`가 읽으므로 스폰 직후 대입만으로 충분)
- 높이는 이동 전 Z를 유지한다(지면 높낮이는 무시)

### StateTree 배치 (미완)
- `SmallPatternRotation` 안에 `Echidna Ribbon Pattern` Task 하나만 넣고 On State Completed → `Cooldown` 연결
- `RibbonClass` = **`BP_EchidnaRibbon`**, `CircleZoneClass` = **`BP_EhidnaFanZone`**(넉다운 O), `ArcZoneClass`는 원하는 CC의 FanZone BP
- `ArcYawOffset`은 **음수가 좌측**(기본 -60)

## 광폭화 시스템 (`Raid/EchidnaBoss.h/.cpp`, `UI/BossHPWidget`) — 2026-09-23

- 레이드 시작(보스 `BeginPlay`)부터 `EnrageTimeLimit`(기본 540초 = 9분)이 지나면 `AEchidnaBoss::Enrage()` → **속도 x2(`EnrageSpeedMultiplier`), 플레이어가 받는 데미지 x2(`EnrageDamageMultiplier`)**
- 광폭화 전에 보스를 잡으면 남은 시간이 그 값에서 멈춘다(`FrozenEnrageRemaining`)
- **속도 = `CustomTimeDilation`**: 보스 · 보스 AI 컨트롤러 · 광폭화 중 스폰된 패턴 액터 전부에 건다
  - 컴포넌트 틱은 소유 액터의 `CustomTimeDilation`을 따른다(엔진 `FActorComponentTickFunction::ExecuteTickHelper`). 그래서 보스에 걸면 **캐릭터 무브먼트·애니메이션·LaunchCharacter 궤적**이, 컨트롤러에 걸면 **StateTreeAIComponent의 Task Tick 누적 시간·Cooldown Wait·PathFollowing**이 전부 같이 빨라진다 — 패턴 Task 코드를 하나도 안 고쳐도 되는 이유
  - 패턴 액터(FanZone/Mirror/Tether/Heart/Orb)는 `BeginPlay`에서 `CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this)`를 설정 → Tick 기반 이동/추적은 자동으로 빨라짐
  - ⚠️ **월드 타이머(`SetTimer`)는 `CustomTimeDilation`을 따르지 않는다.** 그래서 패턴 액터의 `SetTimer`는 시간을 `/ CustomTimeDilation`으로 나눠서 건다. **새 패턴 액터를 만들 때도 이 두 가지(BeginPlay 설정 + 타이머 나누기)를 반드시 따를 것**
  - 예외: `AEchidnaTetherActor::PullResolveDelay`는 안 나눈다 — 플레이어가 끌려오는 시간(플레이어 쪽 타이밍, 광폭화 무관)을 기다리는 값이라, 줄이면 끌려오는 도중에 장판이 터진다
  - `SetLifeSpan`(소멸 대기)도 안 나눔 — 연출상 영향 없음
  - 광폭화 **순간 이미 진행 중이던** 패턴 액터는 원래 속도로 끝난다(다음 패턴부터 2배속). 보스·AI는 즉시 2배속
- **데미지 = `ALoACharacter::ReceiveDamage` 한 곳**에서 `GetEnrageDamageMultiplier()`를 곱한다. 플레이어를 때리는 건 전부 보스 쪽(패턴·똥장판)이라 여기서 일괄 처리 — 똥장판 틱 데미지도 2배가 됨
- `GetEnrageTimeScale`/`GetEnrageDamageMultiplier`는 static — 월드의 모든 `AEchidnaBoss`를 훑어 광폭화 중인 보스의 배율을 반환(보스가 여럿이어도 동작)
- UI: `ALoAPlayerController::UpdateEnrageTimer()`(Tick, `UpdateCastBar`와 같이 `IsActionLocked` 조기 return보다 앞) → `UBossHPWidget::SetEnrageTime()`. 표기는 `광폭화까지 / 00:08:56`(초 올림), 60초 이하·광폭화 후엔 `EnrageWarningColor`(빨강), 광폭화 후 라벨은 `광폭화`. 표시 초가 바뀔 때만 텍스트를 갱신

## 정산 게이지 (`Raid/EchidnaBoss.h/.cpp`, `EchidnaBossStateTreeUtility`, `UI/BossHPWidget`) — 2026-09-23

- 보스 `SettlementGauge` 0~100%. UI는 2칸(1칸 = 50%). **25% / 50%(반정산) / 75% / 100%(풀정산)**에 패턴 — 25%와 75%는 같은 패턴. **패턴 내용은 아직 미정(유저가 나중에 알려주기로 함)**, 지금은 발동 틀만 있음
- **발동 지점과 패턴**: 25% = 똥장판, 50% = 거울잇기(반정산), 75% = 똥장판, 100% = 거울잇기(풀정산) → 100% 거울잇기가 끝나면(`ExitState`) **게이지 0 + 발동 지점 초기화**로 다음 바퀴
- **발동 판정은 이름이 아니라 숫자** (2026-09-24 재설계): 보스가 `SettlementThresholds`({25,50,75,100})와 사용한 지점 `ConsumedSettlementThresholds`를 추적. `GetNextSettlementThreshold()` = 게이지가 넘었고 아직 안 쓴 지점 중 **가장 낮은 것** → 게이지가 한 번에 25·50을 넘어도 25부터 차례로. 정산 패턴 Task(똥장판·거울잇기)는 진입 시 `ConsumeNextSettlementThreshold()`로 소모
  - 이유: 같은 패턴이 두 지점(25/75, 50/100)에서 나와야 하는데, 이름 방식은 조건·Task 이름 불일치로 무한 반복 버그가 두 번 났었음
- 상승 규칙 (전부 `BP_Echidna`의 Settlement 카테고리):
  - 자연 상승: **1.4~1.8초(평균 1.6초)마다 1%** — 매혹 없이 레이드 시작 40초(광폭화까지 8분 20초)쯤 25%. 예전 4~5초는 너무 느렸음
  - 매혹 스택 1 증가마다 +2~3%, 매혹 3스택 도달 시 추가 +10~15% (`ALoACharacter::AddCharmGauge` → `AEchidnaBoss::NotifyCharmStackGained()`)
  - 자연 상승만 기준 대략: 25% 0:40 / 50% 1:20 / 75% 2:00 / 100% 2:40 (패턴 진행 중에도 게이지는 계속 오름). 매혹이 쌓일수록 당겨짐
- 보스가 죽으면 자연 상승 정지. `Reset Settlement Gauge` Task는 이제 보통 필요 없음(거울잇기 100%가 자동 초기화)
- **큰 패턴(시간·정산) 진행 중엔 게이지 정지** (2026-09-24): `AEchidnaBoss::IsSettlementPaused()` = `IsTimedPatternActive()` → `AddSettlementGauge`가 무시(자연 상승·매혹 보너스 전부). 패턴이 끝나면(`ClearActiveTimedPattern`) 다음 자연 상승부터 재개
- **정지 중 UI 흑백**: `UBossHPWidget::SetSettlementPaused()`(컨트롤러 Tick이 매 프레임 넘김, 바뀔 때만 적용) — 초상화는 `T_EchidnaPortrait_Gray`(PIL로 만든 흑백본)로 브러시 텍스처 교체, 게이지 2칸·% 글자는 `SettlementPausedColor`(회색). 원래 모습은 처음 정지할 때 WBP 값을 캡처해 두었다가 되돌림(색 하드코딩 안 함). `PortraitImage`도 `BindWidgetOptional`
- UI 갱신: `ALoAPlayerController::UpdateEnrageTimer()`(Tick)가 `UBossHPWidget::SetSettlementGauge()`도 같이 호출

## 에키드나 정산 패턴 — 게이지 25%·75% "똥장판" (처음엔 광폭화 8분 20초 시간 패턴으로 잘못 만들었음 — 그 시간대에 25%가 차서 착각) (`EchidnaPoopMarkActor`, `EchidnaPoopBeamActor`, `EchidnaBossStateTreeUtility`) — 2026-09-23

### 흐름 (`FStateTreeTask_EchidnaPoopPattern`)
1. 진입: 보스 정지(패턴 내내 매 틱 `StopMovementImmediately`), **카메라 줌아웃**(`ALoACharacter::SetCameraZoomOverride(CameraArmLength=1500)`), **비활성 오염 장판 전부 활성화**(`AHexArena::SetAllPoopTilesActive(true)` → 빨강 + 빨간 테두리)
2. 플레이어 발밑에 **5초 원형 게이지**(`AEchidnaPoopMarkActor`, 플레이어를 매 틱 따라감) — 다 차면 그 순간 밟고 있는 타일을 **비활성 오염 장판**(핑크)으로(이미 오염이면 변화 없음). 처음엔 활성으로 만들었는데 서 있던 플레이어에게 생성 즉시 매혹 스택이 쌓여서 비활성으로 바꿈
3. `BeamSpawnDelay`(4초) 뒤 보스 발밑에서 **추적 장판**(`AEchidnaPoopBeamActor`) — 직사각형(보스→플레이어) + 보스 중심 원
   - `TrackDuration`(3초) 동안 `TrackingRotationSpeed`(**8도/초** — 90→45→20→8. 장판이 플레이어를 휩쓰는 속도 = 각속도×거리라, 걷기 600cm/s로 맵 끝 4000cm에서도 피하려면 ω < 약 8.6도/초) 제한으로 플레이어를 따라 회전, 안쪽이 **보스 쪽부터 게이지처럼 차오름**(직사각형은 길이, 원은 반지름이 같은 비율) → 언제 터지는지 보임
   - 꽉 차면 **원이 먼저 터지고**, 직사각형은 `ExplosionSegmentCount`(8)칸으로 나뉘어 **보스 쪽부터 `ExplosionSegmentInterval`(0.07초) 간격으로 순차 폭발**(블레이즈처럼 앞으로 뻗어나감)
   - 한 캐릭터는 원+직사각형 통틀어 **한 번만** 맞음(원 안이면 `CircleDamage`, 아니면 `BeamDamage`), `bApplyKnockdownOnHit`(기본 true)
4. 게이지·장판 둘 다 끝나면 Succeeded. `ExitState`에서 카메라 복구 + 오염 장판 비활성화 + 중간에 끊겼으면 남은 액터 정리
- `BeamLength` 기본 4000 — 2400이었을 땐 보스와 플레이어가 맵 끝과 끝에 있으면 안 닿았음(타일 중심 간 최대 3180cm)
- 범위 조절: Beam BP 값(`BeamLength`/`BeamHalfWidth`/`CircleRadius`) 또는 Task의 `Pattern|Override`(음수 = BP 값 유지)
- `MarkClass`/`BeamClass`는 **비워두면 네이티브 클래스로 스폰** — BP 할당을 깜빡해 패턴이 조용히 실패하던 전례(하트 등) 때문에 폴백을 둠
- 두 액터 다 Tick 기반 + `BeginPlay`에서 `GetEnrageTimeScale()` → 광폭화 규칙 준수(월드 타이머 안 씀)
- **패턴이 끝나면(중간에 끊겨도) `ExitState`에서 오염 장판을 전부 다시 비활성(핑크)으로** — 활성(빨강)은 패턴 진행 중에만

### 트리거 — `Boss Settlement Gauge Reached` 조건 25 / 75 (예전 `Boss Enrage Time Reached` 500은 폐기)
- Root에 On Tick Transition 2개(GaugePercent 25, 75) → 둘 다 PoopPattern State. Task가 진입 시 발동 지점 소모
- (이하 예전 시간 조건 시절 기록)
- Task가 **진입 즉시 스스로** `MarkPatternTriggered` — `Mark Boss Pattern Triggered` Task를 같은 State에 나란히 두면 그게 즉시 Succeeded를 반환해 State가 바로 끝날 수 있어서
- ⚠️ Enter Condition은 **State 선택 시점에만** 검사된다 → 짤패턴 도중에 8:20이 되면 그 패턴 + Cooldown이 끝난 뒤에 시작(몇 초 늦을 수 있음). 정확히 맞추려면 `SmallPatternRotation`에 `On Tick` 전이 + 같은 조건을 추가해야 함

### StateTree 배치 (에디터 수동 — MCP는 StateTree 쓰기 불가)
- 루트 아래 `MirrorCounter`와 같은 레벨, **`SmallPatternRotation`보다 위**에 새 State `PoopPattern` 추가
- Enter Condition: `Boss Enrage Time Reached` (Boss 바인딩, RemainingSeconds 500, PatternName `PoopPattern`)
- Task: `Echidna Poop Pattern` 하나만 (Boss / AIController 바인딩, PatternName `PoopPattern`)
- Transition: On State Succeeded → `SmallPatternRotation` (MirrorCounter와 동일)

### 카메라 줌 (`ALoACharacter`)
- `SetCameraZoomOverride(ArmLength)` / `ClearCameraZoomOverride()` — Tick에서 `CameraBoom->TargetArmLength`를 `FInterpTo(CameraZoomInterpSpeed=2.5)`로 보간. 기본 길이는 **BeginPlay 시점 값**을 기억(BP에서 바꾼 값도 존중)

## 에키드나 시간 패턴 — 광폭화 7분 40초 "랜잡" (`EchidnaFlytrapZoneActor`, `UI/ScreenFogWidget`, `EchidnaBossStateTreeUtility`) — 2026-09-23

### 흐름 (`FStateTreeTask_EchidnaRandomGrabPattern`)
1. 진입: 보스 정지(매 틱), **비활성 오염 장판 전부 활성화**, 진입 즉시 `MarkPatternTriggered` + `SetActiveTimedPattern`
2. `FogDelay`(3초) 뒤 **화면 전체 핑크 연기**, 그로부터 `FirstTrapDelay`(1초) 뒤부터 장판 판정 시작
3. **앞 장판의 꽃이 다 나오면(`IsTrapShown`) 그 순간 플레이어가 서 있는 타일에 다음 파란 장판**(`AEchidnaFlytrapZoneActor`). 한 번에 하나씩, **같은 타일엔 중복 불가**(이미 꽃이 있는 타일에 서 있으면 다른 타일로 옮길 때까지 대기). 총 `RoundCount`(5)개
   - 변천사: "꽃 나온 뒤 0.2초 텀" → "체류 0.5초면 동시에 여러 개" → "꽃 나온 뒤 + 체류 0.5초" → **"꽃 나온 뒤 바로"**(현재, 체류 조건 `DwellTime` 삭제)
4. 파란 장판이 `FillDuration`(1초) 동안 꽉 차면 파리지옥 — **꽃은 패턴이 끝날 때까지 남는다.** 장판이 꽉 찰 때 그 타일에 있거나, **이후 꽃 타일을 밟으면** 먹힘
   - 먹힘 = **최대 HP × `EatDamageRatio`(0.9)** 데미지 + **패턴이 끝날 때까지 붙잡힘**(`ALoACharacter::SetHeldByPattern(true)` → `IsActionLocked`). 이미 붙잡힌 사람은 다시 먹지 않음(데미지 중복 방지)
5. 마무리 조건: 더 깔 게 없음(5개 다 깔았거나 / 플레이어가 붙잡혔거나 / 장판 단계 `MaxRoundsDuration` 15초 초과 — 계속 움직여서 안 깔리는 경우 대비) **그리고** 깔린 것들이 전부 꽃까지 나옴 → `EndDelay`(1.5초) 후 Succeeded
6. `ExitState`(중간에 끊겨도): **꽃 전부 `Dismiss()`(가라앉아 사라짐)**, **붙잡힌 플레이어 해제**, 연기 걷힘, 오염 장판 비활성, 시간 패턴 잠금 해제

### "붙잡힘" 상태 (`ALoACharacter::bIsHeld`)
- 기절(`ApplyStun`)과 같은 방식(캐스팅·사거리 이동 취소 + 제자리 정지)이지만 **시간 제한이 없다** — 건 쪽이 반드시 풀어야 함(랜잡은 `ExitState`에서 월드의 모든 `ALoACharacter`를 해제). 연출 훅 `OnHeldVisualChanged(bool)`

### ⚠️ PatternName 불일치로 패턴이 무한 반복되던 문제 (RenGrab 2026-09-23, MirrorLink 2026-09-24 — 같은 실수 반복)
- 조건에는 State 이름(`RenGrab`, `MirrorLink`)을, Task는 기본값(`RandomGrabPattern`, `Settlement50`)을 쓰는 실수가 반복됨 → 조건이 영원히 "미발동" → 끝날 때마다 다시 시작
- **코드로 해결**: 큰 패턴 Task(똥장판·랜잡·거울잇기)는 공용 `MarkBigPatternTriggered()`로 **Task의 PatternName과 자기 State 이름(`Context.GetStateFromHandle(Context.GetCurrentlyProcessedState())->Name`) 둘 다** 발동 표시 → 조건에 둘 중 뭘 적어도 맞음. 새 큰 패턴 Task도 이 함수를 쓸 것
- 그 외 조건 PatternName이 둘 다와 다르면 여전히 반복됨 — `재진입` 경고 로그로 확인

### (이전 기록) PatternName은 조건과 Task가 반드시 같아야 한다 (2026-09-23 버그)
- Root On Tick 조건이 `RenGrab`, Task가 `RandomGrabPattern`으로 표시해서 조건이 영원히 true → **매 틱 State 재진입 → 패턴이 매 프레임 처음부터 다시 시작**(로그에 "패턴 시작"이 프레임마다 찍힘) → 연기·장판·파리지옥이 하나도 안 나오고 끝나지도 않았음
- **짤패턴과 겹치지 않는다**: Root On Tick 전이는 원래 진행 중인 짤패턴 State를 즉시 끊는데, 짤패턴이 이미 스폰한 거울·장판·하트 등은 독립 액터라 계속 살아서 시간 패턴과 겹쳤음. 그래서 `Boss Enrage Time Reached`가 `Context.GetActiveStateNames()`에 `WaitWhileStatesActive`(기본 `SmallPatternRotation`)가 있으면 false → **짤패턴이 끝나 Patrol(쿨다운)로 넘어가는 순간 시작**. StateTree 수정 불필요(새 필드는 기본값으로 로드됨). 짤패턴 부모 State 이름을 바꾸면 이 목록도 바꿀 것
- 코드 안전장치: 시간 패턴 Task가 진입 시 `AEchidnaBoss::SetActiveTimedPattern()`, Exit에서 해제. `Boss Enrage Time Reached`는 **시간 패턴 진행 중이면 무조건 false** → 이름이 틀려도 매 틱 재시작은 안 함(단, 끝난 뒤 다시 발동은 여전히 되므로 이름은 맞춰야 함). 시간 패턴끼리 서로 끊지도 않음
- 재진입 시 Task가 `이미 발동한 패턴('...')에 재진입` 경고 로그를 남김 — 이 로그가 보이면 이름 불일치

### StateTree 배치 (에디터 수동) — 똥장판 패턴과 동일
- Root 자식으로 State `RandomGrab` 추가, Task `Echidna Random Grab Pattern` 하나(Boss/AIController 바인딩)
- **Root에 On Tick Transition → RandomGrab**, Condition `Boss Enrage Time Reached`(RemainingSeconds **460** = 7분 40초, PatternName `RandomGrabPattern`)
- RandomGrab에 On State Completed → Patrol
- Enter Condition은 넣지 않는다(똥장판 패턴에서 두 곳 값이 어긋나 안 나왔던 전례 — Root Transition 하나로 충분)

## 에키드나 시간 패턴 — 광폭화 3분 40초 "그네" (`EchidnaSwingZoneActor`, `EchidnaSwingChainActor`, `EchidnaButterflyActor`, `EchidnaBossStateTreeUtility`) — 2026-09-24

### 흐름 (`FStateTreeTask_EchidnaSwingPattern`)
1. 진입: 발동 표시(`MarkBigPatternTriggered`, PatternName `SwingPattern`), 보스 **사라짐** → `VanishDuration`(1초) 뒤 **외곽 링 랜덤 타일**에 아레나 중심을 보고 등장, 패턴 내내 그 자리 고정
2. 등장 `ZoneDelay`(3초) 뒤 보스 발밑에 **그네 장판**(`AEchidnaSwingZoneActor`) — 보스 중심 `SafeRadius`(900cm) 원 안만 안전, 바깥은 거리 제한 없이 전부 **최대 HP × `ZoneDamageRatio`(10) 즉사급**. `TelegraphDuration`(3초) 동안 안전 원 경계에서 바깥으로 차오르다 폭발
3. 폭발 직후 동시에:
   - **화면 핑크 연기**(랜잡과 같은 `SetScreenFog`) + **비활성 오염 장판 전부 활성화**(`SetAllPoopTilesActive(true)` → 빨강, 밟으면 매혹·데미지). `ExitState`에서 다시 비활성
   - **보스↔플레이어 사슬**(`AEchidnaSwingChainActor`, 엔진 Cylinder를 두 끝점 사이로 매 틱 늘림) — 보스 **반대편 타일 = 아레나 중심 대칭 `(q,r)→(-q,-r)`**에 노란 테두리(`SetLinkHighlighted`). `ChainDuration`(5초) 안에 그 타일을 밟으면 끊김(파훼), 못 끊으면 `AddCharmGauge(MaxCharmGauge)`로 **매혹 3스택과 같은 매혹**. 남은 시간이 줄수록 사슬이 빨개짐
   - **나비** `ButterflyCount`(8)마리(`AEchidnaButterflyActor`) — 맵 안쪽(외곽 링 제외) 타일, 플레이어 주변 `ButterflySafeDistance`(1칸) 제외하고 스폰. `Speed`(140cm/s)로 랜덤 비행, `WanderInterval`마다 ±`WanderAngle` 방향 전환, 아레나 밖으로 나가려 하면 중심 쪽으로 튼다. 닿으면 **`ButterflyStunDuration`(10초) 기절** 후 나비 소멸(이미 기절 중이면 통과 — 기절이 끝없이 갱신되지 않게)
4. 사슬 결과가 나면 `EndDelay`(2초) 뒤 Succeeded. `ExitState`(끊겨도): 보스 보이게, 연기 걷힘, 사슬(EndPlay에서 노란 테두리도 끔)·장판·나비 제거, 시간 패턴 잠금 해제
- 나비 BP: `/Game/LostArk/Raid/Echidna/Pattern/BP_EchidnaButterfly` (2026-09-24 생성) — BP 값을 바꿨으면 Task의 `ButterflyClass`에 할당해야 반영됨
- 세 액터 다 비워두면 네이티브 클래스로 스폰, 광폭화 규칙(BeginPlay `CustomTimeDilation` + Tick 기반) 준수
- 비주얼은 전부 에셋 없이 `M_MirrorLaser` 색 주입(장판 PMC 고리, 사슬 Cylinder, 나비 PMC 날개 — 날개 폭 Y 스케일을 흔들어 날갯짓)
- 파훼 성공 보상(무력화 등)은 아직 없음 — 결과만 로그(`[SwingChain] 사슬 끊음 — 파훼`)

### StateTree 배치 (에디터 수동) — 랜잡과 동일
- Root 자식으로 State `Swing`, Task `Echidna Swing Pattern` 하나(Boss/AIController 바인딩)
- **Root에 On Tick Transition → Swing**, Condition `Boss Enrage Time Reached`(RemainingSeconds **220** = 3분 40초, PatternName `SwingPattern` 또는 State 이름 `Swing`)
- Swing에 On State Completed → Patrol. Enter Condition은 넣지 말 것
- 테스트할 땐 RemainingSeconds를 크게(예: 530) 하면 레이드 시작 10초 뒤 바로 나옴

## 에키드나 정산 패턴 — 반정산(50%)·풀정산(100%) "거울잇기" (둘 다 같은 패턴, 100% 끝나면 게이지 0) (`EchidnaLinkMirrorActor`, `EchidnaBossStateTreeUtility`) — 2026-09-24

### 흐름 (`FStateTreeTask_EchidnaMirrorLinkPattern`)
1. 진입: 발동 표시(`Settlement50`) + `SetActiveTimedPattern`(시간 패턴과 서로 안 끊게), 보스 **사라짐**(`SetActorHiddenInGame` + 충돌 끔)
2. `VanishDuration`(1초) 뒤 **레이드 시작 때 깔린 파란 테두리 2칸**에 동시 등장 — 거울은 외곽(`MarkerTileCoords[0]`), 보스는 안쪽(`[1]`) 타일 윗면에 거울을 바라보고 선다(패턴 내내 매 틱 그 자리 고정)
3. 거울(`AEchidnaLinkMirrorActor`): `TrackDuration`(5초) 동안 노란 빛줄기로 플레이어 추적(`TrackingRotationSpeed` 45도/초, 거울 원반도 같이 회전) → 끝나면 **플레이어 강제 정지**(`SetHeldByPattern(true)`) + 거울 정면으로 빛 덩어리 직진
4. 빛 덩어리가 플레이어에 닿으면(2D 거리 ≤ `OrbHitRadius` + 캡슐 반지름) 플레이어 타일 **노란 테두리**(`AHexTile::SetLinkHighlighted`) → 그 타일과 보스 타일이 **헥스 거리 1**이면 보스에게 날아가 **성공**
5. **실패**: 플레이어에 못 닿고 `MaxTravelDistance`(5000) 초과(맵 밖), 또는 플레이어는 맞았지만 옆 칸에 보스가 없음(유저가 명시 안 해서 실패로 처리) → **전 타일 빨간 점멸**(`AHexArena::SetAllTilesDangerFlash` — 바닥 `ActivePoopMaterial` + 빨간 테두리, 타일 타입은 안 바뀜) + 모든 플레이어에게 **최대 HP × `FailDamageRatio`(10)** → `FailFlashDuration`(2초) 뒤 점멸 해제
6. `EndDelay`(1초) 후 Succeeded. `ExitState`(끊겨도): **보스 다시 보이게**, 붙잡힘 해제, 노란 테두리·점멸 해제, 거울 제거, 잠금 해제
- 성공했을 때의 보상(무력화 등)은 아직 없음 — 결과만 로그(`[LinkMirror] ... 성공`)
- 테두리 우선순위: 위험 점멸·활성 오염(빨강) > 거울잇기(노랑) > 파란 테두리

### 트리거 — `Boss Settlement Gauge Reached` 조건 50 / 100
- `GetNextSettlementThreshold() == GaugePercent` && **다른 큰 패턴(시간/정산) 진행 중 아님** && **짤패턴(`WaitWhileStatesActive` = `SmallPatternRotation`) 진행 중 아님**. PatternName 필드는 삭제됨(%만 적으면 됨)

### StateTree 배치 (에디터 수동) — 시간 패턴과 동일
- Root 자식으로 State `MirrorLink`, Task `Echidna Mirror Link Pattern` 하나(Boss/AIController 바인딩, PatternName `Settlement50`)
- **Root에 On Tick Transition 2개 → MirrorLink**, Condition `Boss Settlement Gauge Reached` GaugePercent **50**, **100** — Enter Condition은 넣지 말 것
- MirrorLink에 On State Completed → Patrol
- 테스트: `BP_Echidna`의 `SettlementNaturalAmount`를 크게(예: 20) 하면 금방 50%

## 매혹 게이지 스택 시스템 (`Source/LoA/LoACharacter.h/.cpp`, `LoAPlayerController.h/.cpp`, `UI/CharmGaugeWidget.h/.cpp`) — 2026-09-21

### 개요 — 기존 0~10 누적 게이지를 3스택 + 스택 단위 감소 방식으로 전면 재설계
- **버그였던 것(설계 미비)**: 기존 `CharmGauge`는 그냥 0~`MaxCharmGauge`(10) 사이를 클램프하며 누적만 되는 값이었고, 스택이 다 찼을 때의 디버프도 자연 감소도 전혀 구현되어 있지 않았음
- 현재 설계: `MaxCharmGauge=3`(스택), `CharmGaugeStackDuration=30초`(**1스택이 빠지는 간격**), `CharmedDuration=5초`(매혹 상태 지속시간)
- **스택 감소는 1개씩** (`DecayCharmGauge`, 루핑 타이머): 2스택이면 30초 뒤 1스택, 다시 30초 뒤 0스택. `AddCharmGauge()`로 재히트하면 이 타이머가 처음부터 다시 갱신됨
  - **버그였던 것**: 예전엔 `ClearCharmGauge()`가 스택이 몇 개든 30초 뒤 한 번에 전부 0으로 밀어버렸음(`SetTimer(..., false)` 단발 + 전체 초기화). 루핑 타이머 + 1씩 감소로 교체
- **매혹 상태는 `CharmedDuration`(5초)만 지속**: 3스택 도달 → `bIsCharmed=true` + `OnCharmedChanged(true)` 브로드캐스트 + 스택 감소 타이머 정지 + 5초 타이머 시작 → `EndCharm()`이 `bIsCharmed=false` 브로드캐스트하고 **스택을 0으로 통째 초기화**
  - 매혹 중 `AddCharmGauge()`는 **맨 앞에서 그냥 return** — 재히트로 5초가 연장되면 "매혹은 5초만"이 깨지고, 스택도 이미 최대라 할 일이 없음
- `FOnCharmedChanged` 델리게이트는 3스택에 "도달하는 그 순간"과 `EndCharm()`에서만 브로드캐스트되므로 중복 호출이 없음

### 매혹 상태 — "조종 불가 + 무작위 이동/스킬 사용" (`ALoAPlayerController::OnPlayerCharmedChanged`)
- 넉다운/경직/기절과 달리 **완전히 멈추는 게 아니라 캐릭터가 제멋대로 움직이고 스킬을 씀** — 그래서 `IsActionLocked()`(Tick 맨 위에서 이동 처리 자체를 건너뛰는 조건)에는 **일부러 안 넣음**. 매혹 중에도 Tick의 `bAutoMoving` 처리 경로는 정상 작동해야 무작위 이동이 먹히기 때문
- 대신 `OnInputStarted`/`OnSetDestinationTriggered`/`OnSkillKeyDown`/`OnSkillKeyHeld`/`OnDashInput` 등 **실제 플레이어 입력이 들어오는 지점마다** `Char->IsCharmed()`를 개별 체크해서 진짜 입력만 씹음 — "이동 처리 자체는 살아있어야 하지만 플레이어가 그 이동을 지시할 순 없어야 한다"는 요구사항을 이렇게 분리해서 만족시킴
- `OnPlayerCharmedChanged(true)`: 그 즉시 무작위 행동 1회 실행 + `CharmActionInterval`(기본 0.8초)마다 반복하는 타이머 시작. `false`: 타이머 2개 정지, 붙잡고 있던 스킬 슬롯 있으면 떼기
- `PerformRandomCharmAction()` — 매 틱 순서대로:
  1. **스킬을 붙잡고 있으면(`CharmActiveSkillSlot >= 0`) 그대로 return.** 여기서 매 틱 이동 목표를 덮어쓰면 사거리 밖 Cast 스킬의 `ForceMoveTo`와 싸워서 영원히 사거리에 못 들어간다 — 붙잡는 동안은 스킬이 이동을 주도하게 둔다
  2. 실제 클릭 이동과 동일한 매커니즘(`bAutoMoving`+`CachedDestination`)으로 `CharmWanderRadius`(기본 400cm) 안 무작위 지점을 목표로 설정
  3. 슬롯 0~7 중 **`IsSlotAssigned() && !IsSlotOnCooldown()`인 것만 모아서** 그 중 하나를 고름 → `SkillManager->HandleKeyDown()` 직접 호출 (컨트롤러의 `OnSkillKeyDown` 래퍼는 `IsCharmed()`면 막아버리므로 매혹 스스로의 행동은 SkillManager를 직접 호출해야 함)
  4. `GetSlotSkillData()`로 타입을 보고 **실제 발동에 필요한 만큼** 붙잡음 — Cast는 `CastTime+0.3`, Charge는 `ChargeMaxTime+0.3`, Hold는 `HoldMaxTime`의 50~100%, Instant는 0.1초
- **버그였던 것**: 예전엔 `FMath::RandRange(0,7)`로 쿨타임을 안 보고 뽑고 0.15~0.6초만 붙잡았음. 스킬 쿨타임이 1~3초이던 시절엔 대충 맞았지만 실제 로아 쿨타임(10~30초)으로 바꾼 뒤로는 ①뽑은 슬롯이 대부분 쿨 중이라 불발되고 ②어쩌다 Cast를 뽑아도 0.6초 만에 떼버려서 캐스팅이 취소만 되고 쿨타임만 날아감 → 매혹이 거의 무해해졌다. "쓸 수 있는 것만 고르고, 나갈 만큼 붙잡는다"로 고침
- 매혹 중엔 실제 키 입력이 전부 차단되므로 `Tick`에서 `CharmActiveSkillSlot`에 대해 `HandleKeyHeld()`를 대신 흘려준다 — 사거리 밖 Cast의 진입 판정과 Hold 스킬의 지속 누적이 이 호출에 의존
- 마나 부족 등으로 실제 발동에 실패해도 그냥 조용히 무시됨(SkillManager 자체 검증에 맡김)

### 머리 위 UI — 연꽃 배경 제거 + 120도 부채꼴(파이) 3등분, 흑백/컬러 겹침 (`UCharmGaugeWidget`, 텍스처/위젯 전부 Unreal MCP로 직접 작업)
- **배경 제거**: numpy 없이 순수 PIL로 처리 — 원본 사진은 배경(똥장판 등 어두운 잎/암전)이 전부 어둡고(V 0~0.2) 꽃만 밝아서(V 0.5~1.0) 값(Value) 채널 기준 스무스스텝(0.25~0.45 사이 페더링) 알파를 만들고 `GaussianBlur(1.2)`로 경계를 살짝 부드럽게 처리 — 별도 세그멘테이션 모델(rembg 등 미설치) 없이도 히스토그램이 두 덩어리로 확실히 갈려서(중간값 픽셀이 거의 없음) 깔끔하게 분리됨(`flower_cutout.png`로 결과 확인)
- **분할 방식 변경**: 처음엔 세로 3등분(단순 좌/중/우 크롭)이었으나 "세로 3등분이 아니라 부채꼴로" 요청에 따라 이미지 중심 기준 **120도씩 3개 부채꼴**(위쪽, 좌하단, 우하단 — Mercedes 로고/삼분원 느낌)로 재마스킹 — 각도는 `atan2(cy-y, x-cx)`로 계산한 뒤 경계(30°/150°/270°) 기준으로 알파를 0으로 깎아서 조각별 PNG 6장(컬러/흑백 × 3조각) 생성. 3조각을 겹치면 원래 꽃 전체가 다시 만들어짐
- Unreal MCP `TextureTools.import_file`로 `/Game/UI/CharmGauge/T_CharmPetal_{1,2,3}_{Color,Gray}` 6장 텍스처 임포트(재작업 시 같은 이름 임포트는 실패해서 `AssetTools.delete`로 지우고 재임포트하는 방식 사용 — import_file은 덮어쓰기가 아니라 새 에셋 생성만 지원)
- `UCharmGaugeWidget`(UUserWidget 서브클래스): `ColorImage1/2/3` 3개를 `BindWidget`으로 요구. **위젯 트리는 처음의 가로 배치(HorizontalBox+Segment별 Overlay)에서 부채꼴 방식에 맞춰 단일 `RootOverlay`(Overlay) 안에 6개 Image(Gray1/2/3 먼저, Color1/2/3 나중 — Overlay는 나중에 추가한 자식이 위에 그려짐)를 전부 같은 위치에 겹쳐 배치하는 구조로 재구성**(부채꼴이라 조각들이 서로 다른 위치가 아니라 같은 캔버스 안에서 서로 다른 영역만 차지하기 때문). `SetStacks(N)`이 컬러 이미지 N개만 보이게 하고, N=0이면 위젯 전체를 `Collapsed`
- **크기 버그였던 것**: 처음 만들 때 Image의 `brush.imageSize`를 원본 텍스처 크기(213x436, 나중엔 639x436) 그대로 뒀는데, `WidgetComponent`가 `bDrawAtDesiredSize=true`라 그 크기 그대로 화면에 그려져서 화면 대부분을 뒤덮어버림 — `imageSize`를 72x49(원본 비율 유지한 작은 배지 크기)로, `WidgetComponent`의 `DrawSize`도 같은 값으로 축소해서 해결. `WidgetSpace=Screen`이라 탑다운 카메라 각도와 무관하게 항상 화면 투영으로 작게 보임
- `ALoACharacter`에 `CharmGaugeWidgetComponent`(머리 위 Z+160 오프셋) 신설 — `BeginPlay`에서 `OnCharmGaugeChanged`를 구독해 `HandleCharmGaugeChanged`가 위젯을 `UCharmGaugeWidget`으로 캐스팅해 `SetStacks()` 호출
- **완료됨(2026-09-21, Unreal MCP로 전부 직접 작업, 수동 할 일 없음)**: `WBP_CharmGauge` 생성·컴파일·저장 + 실제 사용 플레이어 캐릭터 BP(`/Game/LostArk/Character/Magician/Sorceress/BP_Sorceress` — `CharacterData=DA_Sorceress`가 할당된 쪽이 실제 사용 캐릭터, `BP_LostArkCharacter`는 미사용 상태였음)의 `CharmGaugeWidgetComponent.WidgetClass`를 `WBP_CharmGauge_C`로 할당까지 전부 완료, PIE에서 바로 확인 가능

### 기절 중 매혹 중복 축적 방지 (`AEchidnaHeartActor::ApplyHit`, 2026-09-21)
- 하트에 맞아 기절(`StunDuration`, 기본 3초)해서 아무것도 못 하는 동안 또 다른 하트에 맞아도 매혹 게이지가 추가로 쌓이면 안 됨 — 무적 시간이 없어서 기절 상태에서도 다른 하트의 콜리전에 계속 맞을 수 있기 때문
- `ApplyHit()`에서 `Character->ApplyStun()`을 호출하기 **전에** 먼저 `Character->IsStunned()`로 이미 기절 중인지 체크 — 호출 순서가 중요함(`ApplyStun()`을 먼저 부르면 그 즉시 `bIsStunned`가 true로 바뀌어버려서 판정이 무의미해짐). 이미 기절 중이면 `ApplyStun()`으로 기절 시간만 갱신하고 `AddCharmGauge()`는 건너뜀

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

## 데미지/HP 밸런스 (2026-09-22)

### 기준값 (2026-09-23 — 실제 에키드나 싱글모드 수치로 교체)
| 항목 | 값 | 실제 저장 위치 |
|---|---|---|
| 플레이어 최대 HP | 100,000 | `DA_Sorceress.MaxHP` |
| 플레이어 AttackPower | 7,911,200 | `DA_Sorceress.AttackPower` |
| 보스 최대 HP | 4,746,719,168 (에키드나 **싱글모드** 실제값) | C++ 기본값 (`BP_Echidna` CDO·레벨 인스턴스 모두 오버라이드를 지워 C++을 따르게 함) |
| 보스 체력 줄 | 285 (싱글모드) | `BP_Echidna` CDO |
| 1줄당 HP | 약 16,655,155 | — |
| 거울 카운터 발동 줄 | 210 | `BP_Echidna` CDO `BigPatternThresholds` |

⚠️ `BigPatternThresholds`의 `TriggerLine`은 **반드시 `TotalLines`보다 작아야 한다.** 285→210으로 줄일 때 트리거가 210에 그대로 남아 있어서 풀피에서 대형 패턴이 즉시 발동하던 버그가 있었음.

⚠️ **보스 HP는 `double`이다.** 40억대라 float(유효숫자 약 7자리)로는 512 단위로 뭉개지고, `int32`(최대 약 21억)로 반올림하면 오버플로한다. HP 표기도 `FString::FormatAsNumber`(int32 전용) 대신 `FText::AsNumber(int64)`를 쓴다. 보스 HP를 다루는 코드를 새로 짤 때 float/int32로 받지 말 것.

스킬 데미지 = `AttackPower × DamageCoefficient`. 킬타임 조절은 **`AttackPower` 하나만** 만지면 전체가 비례해 움직인다. 7,911,200 = 예전 35,000(보스 2100만 기준) × (4,746,719,168 ÷ 21,000,000) — 계수·쿨타임은 그대로 두고 킬타임이 유지되도록 환산한 값.

### 스킬 계수/쿨타임 (`DT_Skills`) — 쿨타임은 실제 로아 소서리스 10레벨 값 그대로
| 스킬 | 계수 | 데미지 | 쿨타임 |
|---|---|---|---|
| 기본공격 | 0.1 | 791,120 | 0.65s |
| 블레이즈 | 2.0 | 15,822,400 | 10s |
| 돌풍 | 4.0 | 31,644,800 | 14s |
| 인페르노 | 6.0 | 47,467,200 | 14s |
| 아이스 에로우 | 5.0 | 39,556,000 | 22s |
| 혹한의 부름 | 8.0 | 63,289,600 | 24s |
| 익스플로전 | 9.0 | 71,200,800 | 28s |
| 천벌 | 14.0 | 110,756,800 | 28s |
| 종말의 날 | 25.0 | 197,780,000 | 30s |

- **기본공격 데미지 경로**: `BP_SorceressBasicAttack`(SkillInstant) → `BP_Sorceress.SpawnBasicAttack` → 발사체 `ABP_BasicAttack`이 BeginPlay에서 `DamageAmount = AttackPower × SkillManager.GetSlotSkillData(8).DamageCoefficient`로 계산. **2026-09-24 이전엔 계수를 안 곱하고 `AttackPower` 그대로(= 계수 1.0) 넣고 있어서 DT 계수를 바꿔도 데미지가 안 바뀌었음** — 다른 스킬처럼 C++ `Execute`가 아니라 BP 발사체가 데미지를 정하는 구조라 놓치기 쉬움

예상 킬타임: 이론 DPS 약 2,670만 → 완벽 플레이 **3분**, 패턴 회피 포함 실전 **약 4분**

### "스킬 점유율" — 로테이션이 비는지 판단하는 기준
스킬마다 `(시전시간 + SkillPostDelay) ÷ 쿨타임`을 구해 전부 더한 값.
- 이 프로젝트 현재 합계 **약 0.35** → 전체 시간의 65%는 스킬을 못 씀
- 실제 로아 소서리스도 **약 0.52**로 100%가 아님 — 빈 시간을 기본공격·이동·패턴 회피가 채우는 게 정상이고, 그게 "정갈하게 흘러가는" 상태
- **그래서 기본공격 계수가 민감함**: 올리면 "좌클릭 연타가 최적"이 되어버린다. 0.35는 기본공격이 전체 딜의 6~7%를 차지하도록 역산한 값

### 보스 패턴 데미지 (플레이어 100,000 기준)
| 패턴 | 저장 위치 | 값 | 비율 |
|---|---|---|---|
| 4거울 레이저 (1틱) | `ST_Echidna` → `4Mirror` | 4,000 | 4% (4틱 풀히트 16%) |
| 8거울 레이저 (1틱) | `ST_Echidna` → `8Mirror` | 3,000 | 3% |
| 개인 유도레이저 | `ST_Echidna` → `8Mirror` GuidedDamage | 5,000 | 5% |
| 뒤로 빠지며 좌우장판 | `ST_Echidna` → `RetreatFan` | 12,000 | 12% |
| 끌고간후 장판 | `ST_Echidna` → `DragFan` | 15,000 | 15% |
| 두번긋고 도넛장판 | `ST_Echidna` → `DonutSlash` | 15,000 | 15% |
| 전방향 하트발사 | `ST_Echidna` → `HeartBurst` HeartDamage | 10,000 | 10% (+3초 기절) |
| 똥장판 틱 | `ABP_HexTile` CDO `PoopTickDamage` | 2,000/초 | 2%/초 |

설계 의도: 짤패턴 **5~8대 맞으면 사망** (로아 짤패턴 체감과 동일)

### ⚠️ C++ 기본값은 전부 에셋에 덮어써진다
밸런스 수치를 바꿀 때 **C++ 헤더만 고치면 아무 일도 일어나지 않는다.** 실제로 동작하는 값은 전부 에셋에 직렬화되어 있음:

| C++ 기본값 | 실제로 읽히는 곳 |
|---|---|
| `CharacterDataAsset.h` MaxHP/AttackPower | `DA_Sorceress` (`PostInitializeComponents`가 덮어씀) |
| `EchidnaBoss.h` TotalLines | `BP_Echidna` CDO **+ 레벨 배치 인스턴스의 자체 오버라이드** (MaxHP는 2026-09-23에 오버라이드를 지워 C++ 기본값을 따름) |
| `HexTile.h` PoopTickDamage | `ABP_HexTile` CDO (`AHexArena.TileClass`로 지정돼 있음) |
| `EchidnaBossStateTreeUtility.h` 각 Task의 `Damage` | `ST_Echidna`의 Task 인스턴스 데이터 |
| `FSkillData` 구조체 기본값 | `DT_Skills` 행 — 단 `SkillRowName`이 비어 있으면 DT 로드에 실패하고 **구조체 기본값이 조용히 그대로 남음** (기본공격이 이 상태로 계수 1.0 = 모든 스킬보다 강했던 적 있음) |

- **레벨에 배치된 액터는 CDO를 바꿔도 자체 오버라이드가 우선**한다. 프로퍼티 우클릭 → Reset to Default로 오버라이드를 지워야 CDO를 따름
- **StateTree Task 인스턴스 값은 Unreal MCP로 쓸 수 없다** (읽기 전용 툴만 제공) — 에디터에서 직접 입력해야 함. 읽기는 `StateTreeTools.get_root_states`/`get_children`/`get_tasks`로 가능

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
- [x] 매혹 게이지 3스택 시스템 + 3스택 도달 시 조종 불가(무작위 이동/스킬 사용) + 머리 위 UI(연꽃 3등분 흑백/컬러) — 위 "매혹 게이지 스택 시스템" 섹션 참조. C++ 구현 + WBP_CharmGauge 위젯 블루프린트 + BP_Sorceress 컴포넌트 할당까지 전부 완료(PIE 테스트만 남음)
- [x] 똥장판에 둘러싸인 타일 자동 Flower 전환 (AHexArena::NotifyTileTypeChanged)
- [x] 에키드나 보스 짤패턴 "4거울" — 대각 4방향 거울 동시 스폰, 추적(장판 따라옴) → 발사(레이저 고정) 2단계, 데미지+넉백 독립 틱
- [x] 보스 쿨다운 중 패트롤 (FStateTreeTask_EchidnaPatrol)
- [x] 짤패턴 로테이션 Cooldown 분리 (3초 텀 후 다음 패턴)
- [x] 에키드나 보스 짤패턴 "뒤로 빠지며 좌우장판" — 플레이어 반대로 후퇴하며 정면 부채꼴 장판(예고→폭발) 2회 순차 발동 (C++ 구현 완료, StateTree `ST_Echidna` 에디터 배치는 미완 — 아래 참조)
- [x] 넉다운 시스템 — 뒤로 튕겨나가며 쓰러짐, 3초 자동/스페이스바 즉시(15초 쿨타임) 기상, fanzone·거울 레이저에 연결 (자세한 내용은 위 "넉다운 시스템" 섹션)
- [x] 즉시 기상 쿨타임 UI — SkillManagerComponent 슬롯 19로 대시(슬롯18)와 동일하게 통합, WBP_HUD에 Border_GetUp 추가
- [x] 거울 레이저 타이밍 재조정 — FiringDuration 3초→1초, 데미지 4틱, 추적 회전속도 60→50도/초, 별도 넉백 시스템 제거(넉다운으로 통합)
- [x] 에키드나 보스 짤패턴 "두번긋고 도넛장판" — 우/좌 대각 슬래시 2회(경직) → 작은 도넛(넉다운) 터지며 보스 상승·정점대기 → 하강하며 외곽 도넛(넉다운+매혹 1스택) 예고→폭발 (C++ 구현 완료, StateTree `ST_Echidna` 에디터 배치는 미완 — 위 섹션 참조)
- [x] 경직(스태거) 시스템 — `ALoACharacter::ApplyStagger()`, 넉다운보다 약하게 짧은 시간만 행동불능(캐릭터를 띄우지 않음), 두번긋고 도넛장판의 슬래시(1·2번)에 연결
- [x] 기절(스턴) 시스템 — `ALoACharacter::ApplyStun(Duration)`, 경직과 같은 방식이지만 지속시간을 호출마다 지정, 전방향 하트발사에 연결
- [x] 에키드나 보스 짤패턴 "전방향 하트발사"(단순화판, 잡기/씨앗 제외) — 2초 예고(보스 정지) → 2초간 0.3초 간격으로 완전 전방향(0~360도 랜덤) 랜덤 개수 하트 발사, 피격 시 데미지+3초 기절+매혹 1스택 (C++ 구현 완료, StateTree `ST_Echidna` 에디터 배치는 미완 — 위 섹션 참조)
- [x] 광폭화 시스템 — 9분 후 보스·패턴 2배속 + 받는 데미지 2배, 보스 HP 바 왼쪽에 남은 시간 표시 (위 "광폭화 시스템" 섹션)
- [x] 정산 게이지 — 자연 상승 + 매혹 스택/3스택 보너스, 좌상단 초상화 아래 2칸 게이지, StateTree 조건/리셋 Task (위 "정산 게이지" 섹션)
- [ ] 정산 패턴(25/75%, 50% 반정산, 100% 풀정산) 내용 구현 — 유저가 패턴 설명 주기로 함
- [x] 레이드 시작 배치 — 비활성 오염 장판 8칸(핑크) + 파란 테두리 타일 2칸(외곽 1 + 안쪽 1, 2칸 간격), 활성화 시 빨강+빨간 테두리 (위 "레이드 시작 배치" 섹션)
- [x] 광폭화 8분 20초 "똥장판" 패턴 C++ 구현 (위 섹션) — **`ST_Echidna`에 `PoopPattern` State 배치는 에디터에서 수동으로 해야 함**
- [x] 광폭화 7분 40초 "랜잡" 패턴 C++ 구현 (위 섹션) — **`ST_Echidna`에 `RandomGrab` State + Root On Tick Transition 배치는 에디터에서 수동**
- [x] 정산 패턴 전부 — 25/75 똥장판, 50/100 거울잇기 (발동 지점 숫자 추적 방식)
- [x] 시간 패턴 "그네" (광폭화 3분 40초) C++ 구현 (위 섹션) — **`ST_Echidna`에 `Swing` State + Root On Tick Transition 배치는 에디터에서 수동**
- [ ] Border_GetUp UI 최종 위치/스타일 다듬기
- [x] DT_Skills `InstantGetUp` 행 — Cooldown 15초 + Icon 채워짐 확인 완료 (2026-09-22)
- [x] StateTree `ST_Echidna`에 `Echidna Retreat Fan Pattern` Task 배치 완료 (`RetreatFan` State)
- [x] 데미지/HP 밸런스 1차 세팅 완료 — 위 "데미지/HP 밸런스" 섹션 참조 (보스 2,100만/210줄, AttackPower 35,000, 스킬 쿨타임을 실제 로아 소서리스 값으로 교체, 기본공격 행 신설)
- [x] 보스 HP UI — 로아식 줄별 색상 바(현재 줄 잔량 + 뒤에 다음 줄 색), `UBossHPWidget` + `WBP_BossHP` 생성/컴파일/저장, `BP_LoAPlayerController.BossHPWidgetClass` 할당까지 완료 (PIE 육안 확인만 남음)
- [x] 캐스팅/차지/홀딩 진행바 UI — `UCastBarWidget` + `WBP_CastBar` 생성/컴파일/저장 완료. **`CastBarWidgetClass`/`BossHPWidgetClass`는 에디터에서 직접 할당 필요** (MCP CDO 쓰기가 런타임에 반영 안 됨 — 위 UI 섹션 경고 참조)
- [x] 에키드나 보스 짤패턴 "백스탭 후 하트발사" — 플레이어를 바라본 채 후방 백스텝 → 착지 무렵 정면 부채꼴로 하트 4개 발사, 피격 시 데미지+기절+매혹 1스택 (C++ 구현 완료, StateTree `ST_Echidna` 배치는 미완 — 위 섹션 참조)
- [x] 에키드나 보스 짤패턴 "정면 리본 공격" — 2갈래 리본 → 피격 시 기절+매혹+원형장판(넉다운) / 빗나가면 끝자락 이동 후 재시도 / 둘 다 빗나가면 좌측 전방 호. C++ + `BP_EchidnaRibbon` 생성 완료, StateTree 배치는 미완 — 위 섹션 참조
- [x] 에키드나 보스 짤패턴 "되돌아오는 구체(자야패턴)" — 플레이어 조준 2회 → 백스텝 → 부채꼴 4개, 총 6개가 전부 왔던 경로로 되돌아옴(돌아올 때 재피격 가능). C++ + `BP_EchidnaOrb` 생성 완료, StateTree 배치는 미완 — 위 섹션 참조
- [x] 보스 피격 데미지 폰트 — `ADamageNumberActor`(월드 액터) + `WBP_DamageNumber`/`BP_DamageNumber` 생성·연결 완료. 3초 페이드, 최신 타격이 앞(`TranslucentSortPriority`)
- [x] 보스 앞/뒤 방향 표시 — `UBossDirectionIndicatorComponent`, 정면은 가운데 뾰족한 호/후방은 매끈한 호
- [x] 끌어당기기(`ApplyPull`) 2단계 재구현 — 멈춤 → 강제 드래그 → 패턴 끝날 때까지 속박
- [ ] `ST_Echidna`의 `Echidna Donut Slash Pattern` Task에서 `SlashZoneClass`→`BP_EchidnaFanZone_Stagger`, `OuterDonutClass`→`BP_EchidnaFanZone_KnockdownCharm`으로 재할당 필요 (Task 배치·`FanZoneClass`·Boss/AIController 바인딩은 이미 완료, BP 2개도 이미 생성·설정 완료 — 드롭다운 재할당만 남음)
- [x] StateTree `ST_Echidna`에 `Echidna Heart Burst Pattern` Task는 이미 배치됨 — **다만 `Heart Class`가 `BP_EchidnaHeart`가 아니라 네이티브 `EchidnaHeartActor`로 잘못 바인딩되어 있음, 반드시 `Heart Class`를 `/Game/LostArk/Raid/Echidna/Pattern/BP_EchidnaHeart`로 바꿔야 함** (StateTree 필드 재할당은 MCP로 못 하는 부분이라 에디터에서 직접 드롭다운 변경 필요)
- [ ] `HeartMeshComp`를 `UProceduralMeshComponent`로 바꾼 뒤 `BP_EchidnaHeart`를 다시 열어서 컴파일 에러/경고 없는지 확인 필요 — 예전(Plane 스프라이트 시절)에 이 컴포넌트에 걸어둔 Material/Rotation 오버라이드들은 컴포넌트 타입 자체가 바뀌면서 무효화됐을 가능성이 있음(에디터가 자동으로 정리하거나, 혹은 에러를 띄울 수 있음) — 컴파일 후 한 번은 반드시 직접 열어서 확인할 것
- [ ] 위 항목 컴파일은 **Live Coding이 아니라 에디터를 완전히 닫고 하는 풀 빌드를 권장** — `HeartMeshComp`의 UPROPERTY 타입 자체가 바뀐 변경이라 Live Coding 핫패치로는 불안정할 수 있음
- [ ] BP_EchidnaFanZone 서브클래스 생성 + 부채꼴 전용 커스텀 머티리얼(Color Vector Parameter + Translucent) 할당 — VFX는 안 씀, 색상만으로 표현
- [ ] SM_HexTile 머티리얼 슬롯 분리 (윗면 MI_Rock_Inst_5, 옆면 어두운 색)
- [ ] BP_HexArena에서 WallMaterial 재할당 (기존 WallMesh 프로퍼티가 프로시저럴 메시 전환으로 제거됨)
- [ ] BP_HexTile 서브클래스 생성 + NormalMesh/PoopMesh/FlowerMesh 할당 (NormalMesh는 기존 HISM 메시와 동일하게)
- [ ] 매혹 게이지 머리 위 UI PIE 실제 확인 필요 — 위치/크기(`CharmGaugeWidgetComponent` Z+160, DrawSize 150x60)가 적절한지, 3등분 이미지가 의도대로 보이는지 육안 확인 안 함
- [x] 큰 꽃 비주얼 + 주변 1칸 매혹 오라 (위 "큰 꽃" 섹션) — 똥장판 자체 VFX는 여전히 없음(머티리얼 색만)
- [ ] DT_Skills SkillName/Icon 데이터 입력 필요 (혹한의 부름·아이스 에로우·돌풍 포함)
- [ ] BP_FrostCall / BP_IceArrow / BP_Gust ZoneClass·VFX 에셋 할당
- [ ] 스킬 레벨에 따른 데미지 계수 연동
- [ ] AvailableSkillPoints UI 표시 연동
- [ ] NS_Inferno_Impact AddVelocity Z값 Niagara 에디터에서 조정 필요
- [ ] PER_Lava_Brutal 이미터 스케일 조정 (NS_Explosion_Impact 잔상 크기)

## 자주 쓰는 빌드 명령
- `LoA.Build.cs`에 **`SlateCore`** 의존성 추가함(2026-09-24) — `FSlateBrush` 등 SlateCore 타입을 멤버로 들고 있으면 LNK2019(`FSlateBrush::FSlateBrush`)가 난다
```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" LoAEditor Win64 Development "C:\Users\User\Documents\Unreal Projects\LoA\LoA.uproject" -NoUBTMakefiles
```
