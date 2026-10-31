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
| `SkillFrostCall` 혹한의 부름 | Hold | `AFrostCallZoneActor` (`BP_FrostCall`) | 누르면 장판, 홀드 유지(`HoldMaxTime` 4초), 떼면 정지. 시전 중 몸만 `FloatHeight` 60cm 띄움(`ALoACharacter::SetVisualLift` — 원본 애니메이션은 b_root가 10cm만 떠서 바닥에 붙어 보였음, 캡슐·판정은 그대로). 사거리 밖이면 이동 없이 경계에 스폰. 데미지 타이머와 VFX 타이머 독립. `NS_FrostCallCircle` / `NS_FrostCallIcicle` |

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

## 캐릭터 애니메이션 (원본 소서리스 모델일 때만 — 마네킹이면 무시)
- 짝 표 = `SkillBase.cpp`의 `SkillAnim_Table`(키 = 네이티브 스킬 클래스 이름, 기본공격만 BP라 `SkillRowName` "BasicAttack" — 종말의 날 BP의 행 이름은 DT와 다른 "Meteor"라 행 이름으로 못 찾음). **유저가 PIE에서 보고 정한 짝**:
  종말의 날 `sk_meteor_01`→`02` · 천벌 `sk_thunderbolt_01`→`02` · 익스플로전 `sk_greatfire_01`→`02` · 블레이즈 `sk_firewall` · 인페르노 `sk_flamestrike` · 아이스 에로우 `sk_icearrow` · 돌풍 `sk_windstrike` · 혹한의 부름 `sk_flashfrost_01`→`02` 반복→뗄 때 `03` · 기본공격 `att_battle_1_01`/`02` 번갈아. 대시 `sk_movingillusion`, Z·X는 `ASorceress`(`sk_arcanereinforce`·`sk_arcaneshift`)
- **타이밍 = 애니메이션 기준(유저 원칙: 애니메이션이 있는 스킬은 효과를 애니메이션에 맞춘다)**. 발동 애니메이션을 처음부터 틀고 효과는 발사 순간까지 미룬다(`SkillAnim_Table`의 `ExecuteDelay[Release별]` — 원본 PSA에서 손·지팡이가 가장 빠른 프레임): 블레이즈 0.30 · 인페르노 0.60 · 아이스 에로우 1.20 · 돌풍 0.47 · 기본공격 0.07/0.27초. 캐스팅 스킬의 `_02`(종말의 날·천벌·익스플로전)는 발사가 첫 0.1초라 0
  - 구조: `USkillInstant::OnKeyDown` → `BeginSkillExecute`(방향·애니메이션, 지연 있으면 타이머 후 `Execute` 재호출 → `RunDeferredExecute`). OnKeyDown을 따로 둔 스킬(아이스 에로우·인페르노 — 사거리 이동)은 `Execute_Implementation` 첫 줄 `if (!BeginSkillExecute(Owner, &PendingTarget)) return;`. 미뤄진 발동은 `bLastExecuteDeferred` — 커서 대신 누를 때 돌아선 정면(블레이즈·돌풍). 대기 중 같은 스킬이 또 나가면 앞의 것을 즉시 발동(기본공격 연타). `ForceCancel`(CC·사망)이면 취소. 애니메이션이 없으면(마네킹) 지연 없음
  - 익스플로전 투사체는 `CircleShowTime` 0(점프와 동시 발사). 캐스팅 `_01`은 재생 속도 = 길이 ÷ (CastTime ÷ 신속 캐스팅 배율)이라 캐스팅 바가 찰 때 끝남
- **기본공격 VFX = `Sorceress/VFX/NS_SorceressBasicAttackMist`**(`Mixed_Magic_VFX_Pack/VFX/NS_Ice_Mist` 복사 — 원본은 제자리에서 퍼지는 연기라 소스 이미터 `Sparks_Way` AddVelocity를 로컬 +X 800~1300으로 줘 앞으로 "후!" 뿜게, 색 보라·은하, 수명 0.5~1.1초·방출 0.2초로 짧게). `SkillAnim_Table`의 `ReleaseVFX` 열 → 발사 순간(`ExecuteDelay` 뒤) `SpawnReleaseVFX`가 캡슐 중심 + 정면 60·위 20에 정면 방향으로 스폰. 판정은 그대로 `ABP_BasicAttack`(투사체 500cm/s, 구 30) — 그 Niagara(`NS_Magma_Shot_Projectile`)는 **`ULoADamageLibrary::TagDamageSource`(투사체 BeginPlay가 부름)에서 소서리스 기본공격이면 끔** — MCP로 컴포넌트 에셋을 None으로 써도 저장이 안 되고 되돌아갔음. 입김은 진하게(밝기 2배·알파 2.5·연기 크기 1.3배). **판정도 입김에 맞춤**(같은 곳): 투사체 속도 900·수명 = 4.2m ÷ 900, 구 반지름 75(원래 500cm/s × 1.25초 = 6.25m, 반지름 30). 입김 사거리는 `Sparks_Way` 속도 550~750 × 수명 0.45~0.55초 ≈ 3~4m, Z ±10(위로 치솟던 건 `Sparks`의 AddVelocityFromPoint 355 → 120, 구 150 → 50). **끝에 높이 솟던 건 `Smoke_Last`의 Scale Sprite Size**(곡선 0.75→5 × ScaleCurve 1.5 = 끝에 7.5배 — 카메라를 향한 큰 판이라 비스듬한 탑다운에선 위로 솟아 보임) → ScaleCurve 0.4, 기본 크기 원본(300/250), Camera Offset 끔. **`Sparks_Way`·`Sparks`의 Curl Noise Force는 끔** — 시드가 고정된 월드 노이즈 장이라 늘 같은 쪽(왼쪽·위)으로 휘었고, 세기 곡선 10000→1000이 입김 속도(550~750)보다 훨씬 커서 길어지고 솟았음
- 블레이즈 VFX `/Game/Free_Magic/VFX_Niagara/NS_Blaze`: 불길을 끌고 가는 `REF` 이미터의 Collision(WorldDynamic 레이 트레이스)·KillParticles를 **껐음** — 켜져 있으면 앞에 보스·액터가 있을 때 REF가 죽어 불길이 중간에 끊겼다
- 재생 지점: 캐스팅 `USkillCast::OnKeyDown`(캐스팅 애니메이션 반복) → 완료 시 발동 애니메이션, 취소(`OnKeyUp`)·`ForceCancel`이면 끊음 / 즉발은 각 스킬 `Execute_Implementation` 첫 줄(사거리 이동 뒤 발동도 잡힘) + 기본공격용 `USkillInstant::OnKeyDown` — 같은 프레임 두 번은 무시 / 홀드는 누를 때 시작, 장판이 꺼질 때 끊음. 발동 애니메이션은 움직이면 바로 끊긴다(`bStopOnMove`)
- **선딜 = 무조건 실행(유저가 로아에서 확인)**: 즉발 스킬은 효과가 나올 때(`ExecuteDelay`)까지 스킬 잠금 — `ReleaseSkillLock`이 후딜(0.1초)과 `GetMaxPendingExecuteRemaining()` 중 긴 쪽으로 잠금. 그사이 누른 스킬은 `QueuedSkillSlot`(덮어쓰기 1개)에 들어갔다가 효과 직후 나감(스킬별 선딜이 달라 잠금도 스킬별). 선딜 중엔 대시·점멸·마력 방출도 불가(`IsExecutePending`). 대기 중 CC·사망이면 대기 입력 버림. 효과가 나온 뒤(후딜)는 다른 스킬·대시로 끊을 수 있음
- **즉발 스킬 발동 애니메이션 동안 클릭 이동 금지**(`BeginSkillExecute` → `ALoACharacter::LockMoveForSkillAnim(남은 애니메이션 길이)`, 컨트롤러 이동 입력 3곳이 `IsSkillAnimMoveLocked` 확인) — 스킬을 쓰고 바로 다른 쪽을 클릭하면 캐릭터가 돌아서 미뤄진 효과(블레이즈·돌풍 = 액터 정면)가 그쪽으로 나갔음. 다른 스킬·대시는 됨, 기본공격은 제외, `ForceCancel`이면 풀림
- **방향**: 캐스팅·홀드는 시작 때, 즉발·기본공격은 발동 때 커서 쪽으로 즉시 회전(`FaceCursor` — 마네킹이어도). 캐스팅 완료 때는 다시 안 돎(시작 방향 유지), 사거리 이동 뒤 발동하는 인페르노·아이스 에로우는 커서 대신 `PendingTarget` 쪽. 블레이즈·돌풍은 원래 Execute에서 스스로 회전
- 점화(아이덴티티) 변형 `*_identity1`, 트라이포드 변형 `*_custom_N`은 아직 안 씀. 쿡: `Model/Anims` 폴더 `DirectoriesToAlwaysCook`

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
- 쿨감 = 1 - (1 - 신속·각인) × (1 - 작열 보석), 상한 80%(`GetTotalCooldownReductionOf`). 점화(마력 해방 발동)는 `ReduceRemainingSkillCooldowns` — 슬롯 0~7의 남은 쿨만. 카운터 스킬 쿨 오버라이드는 Raid 문서의 거울 카운터 참조

## 범위 VFX 자동 스케일 (종말·천벌·인페르노)
- 캐스팅 완료 → `CircleShowTime` 동안 발판 원 → 효과. `AutoScale = ExplosionRadius / CircleBaseRadius`(Scale 1로 실측해 BP에 입력)
- **개별 이미터 파라미터만** 설정(`User.Scale_Circle` 등) — `Scale_All`과 같이 쓰면 제곱됨
