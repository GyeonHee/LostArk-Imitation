# Stat — 스탯 공식 · 각인 · 아크 패시브

## 원칙
- 수치는 최대한 실제 로아 값. 출처는 Maxroll 게임 데이터 덤프 `https://assets-ng.maxroll.gg/laplanner/game/stats.json` — 커뮤니티 글보다 먼저 볼 것
  - `itemLevel["아이템ID#레벨"]`: stat ID 55 물방 / 56 마방 / 3·4·5 힘·민첩·지능 / 6 체력 / 151 무기 공격력. 업화 ID `11159000` 무기, `111590X1~X5` 방어구(X = 1 힘 · 2 민첩 · 3 지능)
  - `classes[205]`(소서리스) hpCon 2.1 / defCoefficient 0.95, `statLevelScaling["70"]`, `statConversion`
- **`DA_Sorceress` 값이 에셋에 저장돼 있다** — C++ 기본값만 바꾸면 반영 안 됨(MCP로 DA 저장)
- 수정치는 `UStatComponent`에 출처별("Equipment", "Gem", "Engraving", "EngravingBuff", "ArkPassive", "ArkPassiveBuff")로 등록, 표시는 `OnStatsChanged`로만 갱신

## 공식
- 표시 공격력 = √(주 스탯 × 무기 공격력 / 6), 실제 = 표시 × **K(`AttackPowerScale`)**. 맨몸 주 스탯 477, 무기 0
- **K = 6.09**(현재). 시작 세트 기대 DPS가 목표(허수아비 약 2,670만)가 되도록 시스템을 추가할 때마다 역산 — 내역은 `Data/CharacterDataAsset.h` 주석. 추정값이므로 `LoADpsTest`(허수아비)로 확인할 것. 시작 장비·특성을 바꾸면 다시 역산
- 최대 생명력 = (`BaseVitality` 40,895 + 장비 체력) × 2.1 → 시작 약 19만. 장비를 바꿔도 현재 HP는 그대로(최대치만, 줄면 자름). 스폰 때만 최대로 채움(`bHPSettled`)
- 방어력 = 장비 방어력 × 0.95 × (1 + 인내/1,222.55), 피해 감소 = 방어력 / (방어력 + 6,500)
- 피해 종류(`ELoADamageKind`): DamageType이 `ULoAPhysicalDamageType` → 물리, `ULoATrueDamageType` → **Fixed(방어·배율 무시)**, 그 외 → 마법(에키드나 패턴 전부). UENUM 값에 `True`는 UHT가 거부
- 보스 패턴은 `AEchidnaBoss::IncomingDamageScale`(11.356)을 곱한 뒤 방어력으로 깎음 → 시작 세트가 예전 HP 10만 시절과 같은 비율로 맞음. **"최대 HP × N" 기믹은 Fixed를 쓸 것**
- 전투 특성(로아 70레벨, 포인트당 1%): 치명 27.944 / 신속 46.5731(쿨감)·58.2211(속도) / 제압 14.0053 / 인내 12.2255. 특화는 자체 설계(캐스팅·차지 +0.05%). 상한 속도 +40%, 쿨감 80%
- 광폭화 피해 ×2는 `ALoACharacter::ReceiveDamage`에서 곱함

## 각인 (`EngravingComponent`, `BuildRegistry.cpp` `Eng_Rows`)
- 로아 T4 전투 각인 43종, 유물 0~4단계(5권마다 ◆, 0/20권). 시스템이 없는 13종은 `bHasEffect=false`(장착은 됨)
- 조건부 효과는 판정 시점에: 대상 HP(`ResolveIncomingHit`의 `TargetHPRatio`), 내 HP·마나 소모·차지 등은 `CalcOutgoingDamage`, 피격 쪽은 `ReceiveDamage`(굳은 의지·불굴·여신의 가호·위기 모면 — Fixed 즉사는 못 막음·긴급구조)
- 중첩(아드레날린 등)은 `USkillManagerComponent::OnSkillUsed`(슬롯 0~7)마다
- 각인서 `EngBook_<Id>`, 소지품 우클릭으로 읽음. 장착 변경은 각인 효과 창에서 [저장]해야 적용
- 저장 플래그 `bEngravingBooksV2`(예전 1권 = 배움 규칙 변환)

## 아크 패시브 (`ArkPassiveComponent`, `ArkPassiveRegistry.cpp`)
- 노드 = Maxroll `arkPassives`(진화 group 0, 깨달음 1·도약 2 class 205). `.` 키 또는 P창
- 포인트는 장착 장비에서 계산(`ComputeBudget`): 진화 = 방어구당 20 + 레벨 70 / 깨달음 = 장신구(등급·부위·연마 단계 표) + 레벨 / 도약 = 팔찌 + 레벨×2. BeginPlay·다음 틱·`OnEquipmentChanged`마다(컴포넌트 BeginPlay 순서가 보장 안 됨)
- 규칙: 진화 티어 상한 40/30/20/20/30 + 아래 티어를 채워야 다음, 깨달음 연계 + 점화/환류 택1, 도약 1티어 40 사용 후 2티어. `Validate`, `TryChange`
- **포인트가 모자라면 배치는 남기고 (티어, 열) 순서로 들어가는 만큼만 적용**(`EffectiveLevels`) — 통째로 끄는 규칙 때문에 치명·특화가 0이 된 버그 전례
- 효과 없는 노드는 `bHasEffect=false`(아이덴티티·각성기 없음 → 점화 계열·도약 대부분). 점멸 = 대시
- 시작 = 추천 세팅(점화), `bHasStarterArkPassive`/`bArkPassiveBudgetV2`
