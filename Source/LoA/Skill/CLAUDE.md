# Skill — 스킬 시스템

## 구조
- `USkillBase` → `USkillInstant` / `USkillCast` / `USkillCharge` / `USkillCombo` / Hold(SkillBase 직접 상속, 별도 클래스 없음)
- `FSkillData`(`DT_Skills` 행): SkillName, InputType, ManaCost, Cooldown, Icon, MaxSkillLevel, SkillPointCostPerLevel, DamageCoefficient, CastTime, ChargeMaxTime, HoldMaxTime, `bCanCounter`. **`SkillRowName`이 비면 DT 로드 실패 → 구조체 기본값이 조용히 남음**(기본공격이 계수 1.0으로 최강이던 전례)
- 슬롯: 0~7 = Q/W/E/R/A/S/D/F, 8 = 기본공격, 18 = 대시, 19 = 즉시 기상. 18·19는 인스턴스 없이 DT 쿨타임·아이콘만 쓰는 슬롯(`DashRowName`/`GetUpRowName` = `"InstantGetUp"`, 행 없으면 `GetUpCooldownFallback` 15초, **Cooldown 0이면 무한 재사용**)
- DT에 비(非)스킬 행을 추가하면 `USkillManagerComponent::IsSkillTreeRow()`에서도 뺄 것(스킬트리 목록에 뜸)

| 클래스 | 타입 | 액터 (BP) | 요점 |
|---|---|---|---|
| `SkillDoomsday` 종말의 날 | Cast | `ADoomsdayMeteor` (`BP_SkillMeteor`) | 메테오 낙하, 원 `NS_MeteorCircle` |
| `SkillCheonbeol` 천벌 | Cast | `ALightningStrikeActor` (`BP_LightningStrike`) | 번개 낙하, 원 `NS_LightningCircle` |
| `SkillExplosion` 익스플로전 | Cast | `AExplosionMeteorActor` (`BP_Explosion`) | 허리 높이에서 커서 방향 직선 비행 → `Land()` 폭발. `NS_Explosion_Projectile` / `NS_Explosion_Impact` |
| `SkillBlaze` 블레이즈 | Instant | `ABlazeProjectile` (`BP_SkillBlaze`) | 발사체 + 화상 틱 |
| `SkillInferno` 인페르노 | Instant | `AInfernoZoneActor` (`BP_SkillInferno`) | 원 `NS_InfernoCircle` 후 1회 폭발 `NS_Inferno_Impact`(Magma 복제, AddVelocity Z 양수로 위로 솟게) |
| `SkillIceArrow` 아이스 에로우 | Instant | `AIceArrowZoneActor` (`BP_IceArrow`) | 0.5초 × 4틱, 고드름 45도(`(-HorizDir + (0,0,-1)).Rotation()`), 수평 드리프트 보정 = 스폰 높이와 같은 값. `NS_IceArrowIcicle` |
| `SkillGust` 돌풍 | Instant | `AGustTornadoActor` (`BP_Gust`) | 커서 방향 회전 + 정지, 전방 박스. **카운터 스킬**(`DT_Skills` 행 `bCanCounter` 체크) — 오버랩 컴포넌트마다 `ICounterable` 호출 |
| `SkillFrostCall` 혹한의 부름 | Hold | `AFrostCallZoneActor` (`BP_FrostCall`) | 누르면 장판, 홀드 유지(`HoldMaxTime` 4초), 떼면 정지. 사거리 밖이면 이동 없이 경계에 스폰. 데미지 타이머와 VFX 타이머 독립. `NS_FrostCallCircle` / `NS_FrostCallIcicle` |

## 사거리 · 입력 동작
| 상황 | Cast | Instant | Hold |
|---|---|---|---|
| 사거리 안 누름 | 캐스팅 시작 | 즉시 발동 | 장판 스폰 |
| 사거리 안 뗌 | 취소(쿨타임 적용) | — | 장판 제거 |
| 사거리 밖 누름 | **홀드 중에만** 이동 | **원프레스** 자동이동 | 경계에 스폰 |
| 사거리 진입 | 멈추고 캐스팅 | 즉시 발동 | — |
| 이동 중 뗌 / 마우스 클릭 | 취소, 쿨타임 없음 | (뗌은 무시) 클릭 시 취소, 쿨타임 없음 | — |
- Cast: 사거리 밖 → `bMovingToRange=true`(bIsActive는 그대로) + `ForceMoveTo`, `OnKeyHeld`에서 진입 감지 → `Super::OnKeyDown`. 캐스팅 중 이동 입력 → `CancelActiveCastSkill()` → `ForceCancel` + `StartCooldown`, 범위이동 중이면 `CancelRangeMove`(쿨 없음)
- Instant: 사거리 밖 → `PendingRangeMoveSlot` 등록 → `TickComponent`가 `OnKeyHeld` 대신 호출 → 진입 시 `Execute` + `StartCooldown`. 마우스 클릭 → `CancelPendingRangeMove`. Cast/Charge는 등록 안 함
- 가상 함수: `IsMovingToRange()`, `CancelRangeMove()`, `ForceCancel()`, `GetCastProgress()`(Cast·Charge·Hold가 캐스트바에 표시 — Cast는 `bIsActive`일 때만)
- **스킬 키 = 이동 정지**: 컨트롤러 `OnSkillKeyDown`이 `HandleKeyDown`보다 **먼저** `CancelAutoMove()` + `bMoveHaltedByAttack`(순서가 반대면 사거리 자동이동이 지워짐). 스킬은 `IsSlotAssigned && !IsSlotOnCooldown`일 때만, 기본공격은 항상
- **커서 조준**: `GetHitResultUnderCursor`를 직접 쓰지 말 것 — 실패 시 Execute가 return해도 `TryActivateSkill`은 마나·쿨타임을 소모함. 방향 스킬은 `ALoAPlayerController::GetCursorAimLocation`, 장판 스킬은 `GetCursorGroundTarget`(바닥 못 맞히면 조준 방향 사거리 끝에 바로 시전)

## 스킬 잠금
- `IsSkillLocked()` = 어느 슬롯이든 Active/MovingToRange/PostDelay. 잠금 중 입력은 `QueuedSkillSlot` 1개(덮어쓰기) → `OnSkillLockReleased`에서 발동
- Instant·Cast·Charge 완료/취소 → `SkillPostDelay`(실제 값은 `BP_Sorceress` SkillManager **0.1초**, C++ 기본값과 다름) 후 해제. Hold 완료/취소·사거리 이동 취소 → 즉시. Hold 활성 중엔 다른 키 완전히 씹힘
- `SwapSkillSlots`: 시전/후딜 중 또는 둘 중 하나라도 쿨타임이면 거부
- 슬롯 등록(`AssignSkillToSlot`)은 정비소 안에서만(`SetSlotEditAllowed`). 복원은 제한·브로드캐스트 없는 `AssignSkillToSlotInternal`(HUD가 BeginPlay 시점엔 준비 안 됨)
- 배치 유지: `EndPlay`에서 `ULoAGameInstance`에 저장 → `BeginPlay`에서 복원

## 데미지 · 계수
- 데미지 = 공격력 × `DamageCoefficient` → `UStatComponent::CalcOutgoingDamage`(보석·각인·아크 패시브 스킬별 배율 포함)
- **계수 = 한 번 시전의 총 피해**. 다단히트는 틱 수로 나눌 것(혹한이 틱마다 계수를 통째로 줘서 딜 45%를 차지했던 버그). 블레이즈는 계수 ÷ (첫 타 + 틱 수)
- **기본공격은 BP 경로**: `BP_SorceressBasicAttack` → `BP_Sorceress.SpawnBasicAttack` → `ABP_BasicAttack`이 BeginPlay에서 `AttackPower × GetSlotSkillData(8).DamageCoefficient`
- 쿨타임은 실제 로아 소서리스 10레벨 값. Σ(계수 ÷ 쿨타임) ≈ 3.13 유지 → 이론 DPS 약 2,670만, 킬타임 3~4분

| 스킬 | 계수 | 쿨타임 | 목표 비중 |
|---|---|---|---|
| 기본공격 | 0.1 | 0.65 | - |
| 블레이즈 | 2.2 | 10 | 7% |
| 돌풍 | 0.45 | 14 | 1% |
| 인페르노 | 3.5 | 14 | 8% |
| 아이스 에로우 | 8.3 | 22 | 12% |
| 혹한의 부름 | 5.3 | 24 | 7% |
| 익스플로전 | 18.4 | 28 | 21% |
| 천벌 | 16.6 | 28 | 19% |
| 종말의 날 | 23.5 | 30 | 25% |
- 스킬 점유율 = Σ(시전 + 후딜) ÷ 쿨타임 ≈ 0.35(실제 로아 약 0.52). 빈 시간은 기본공격·회피가 채움 → 기본공격 계수를 올리면 "좌클릭 연타가 최적"이 됨
- 쿨감 = 1 - (1 - 신속·각인) × (1 - 작열 보석), 상한 80%(`GetTotalCooldownReductionOf`). 카운터 스킬 쿨 오버라이드는 Raid 문서의 거울 카운터 참조

## 범위 VFX 자동 스케일 (종말·천벌·인페르노)
- 캐스팅 완료 → `CircleShowTime` 동안 발판 원 → 효과. `AutoScale = ExplosionRadius / CircleBaseRadius`(Scale 1로 실측해 BP에 입력)
- **개별 이미터 파라미터만** 설정(`User.Scale_Circle` 등) — `Scale_All`과 같이 쓰면 제곱됨
