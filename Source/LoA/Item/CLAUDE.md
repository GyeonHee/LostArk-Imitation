# Item — 소지품 · 장비 · 성장 재료 · 보상

## 공통
- 아이템 정의 = C++ 표 `ItemRegistry.cpp`(`LoAItems::Find`, 플라이웨이트 해시맵). 늘어나면 DataTable로 옮길 것. 아이콘 `/Game/LostArk/UI/Item/T_*`
- 공식 아이콘 찾는 법: Maxroll `items.json`의 `icon`(예 `Acc_217`) → `https://cdn-lostark.game.onstove.com/efui_iconatlas/acc/acc_217.png`. 스마일게이트 저작물 — 개인 학습용, 외부 배포 금지
- **저장 순서가 어긋나면 아이템이 사라지거나 늘어난다** — 장비·보석·재련 등 소지품을 건드리는 변경은 `UEquipmentComponent::SaveNow()`(소지품 → 장착 GUID → 디스크)로 즉시 저장. 판매 목록이 있으면 저장 생략
- **저장용 enum은 뒤에만 추가**: `EEquipSlot`(장착 GUID 배열 순서), `EPolishEffect`, `EStatModifierTarget` 등
- 기존 저장 파일에 새 시작 아이템을 주려면 별도 플래그(`bHasGold`, `bHasStarterAccessories` 등)로 한 번만. 초기화 = `Saved/SaveGames/LoA.sav` 삭제

## 소지품 `UInventoryComponent`
- 칸 = 크기 100 고정 배열(인덱스 = 화면 위치), 칸은 `ItemId`·`Count`·`bLocked`만
- 정렬: 잠긴 칸은 제자리, 나머지를 종류별로 합쳐 (카테고리 → SortOrder → 이름) 순으로 MaxStack 단위로 채움
- `MoveOrSwap`(같은 아이템이면 합침), `CanAddItems`(복사본에 채워 봄), 장비 인스턴스는 `EquipmentId`
- MaxStack을 줄인 아이템(보석 1)은 `EnsureLoaded`가 겹친 칸을 나누고, 모자라면 대기 보상으로
- 화폐(칸 없음): **골드** = 재련·성장 외 비용·더보기·보석 상자 / **실링** = 상점·장비 성장·팔찌 부여·스톤 세공. `SaveVersion` 1 이하 파일은 실링을 200만으로 낮춤
- 판매 예약 `SellReserve`(최대 12줄): 올리면 칸에서 실제로 빠짐, 비우기·상점 닫기·`EndPlay`(저장 전)에 되돌림. 장비는 줄마다 인스턴스 1개, 판매 시 인스턴스 삭제

## 배틀아이템 `UBattleItemComponent` (키 1~4)
- 처음엔 빈 칸, 소지품에서 드래그 등록(`bIsBattleItem`만, 최대 `BattleEquipMax` 개를 꺼냄, 다른 칸의 같은 아이템은 비움), HUD 칸을 소지품 위로 끌면 해제
- 다 써도 칸에 남음(빨간 틴트). **쿨타임은 아이템 종류별**(빼고 끼워 초기화 방지). 정비소에서 `TopUpFromInventory`
- 사용 VFX `NS_Heal`을 캡슐 **발밑**에 붙임(`bLocalSpace` true). Free_Magic 이펙트를 복제할 땐 `ShapeLocation` Rotation Mode가 `Axis Angle`이면 원기둥이 눕는다 → `Default`. 힘 모듈은 `SolveForcesAndVelocity`보다 앞(MCP `AddModule`은 지정 모듈 **뒤**에 넣음)

## 장비 · 재련 (`EquipmentComponent`, `RefineLibrary`)
- 재련 장비 6부위(`GearSlotCount`) + 장신구 5 + 팔찌·스톤 = 11 → `IsAccessorySlot`. 귀걸이·반지는 1번 칸 정의, 2번도 허용(빈 칸 우선)
- 장비 스탯 = `LoAItems::GetGearStats(부위, 단계)` — 운명의 업화 0~25단계 실제 표(공식은 `Stat/` 문서)
- 재련 `LoARefine`: 확률표 `Refine_Tiers`, 실패 보정 기본×10%씩(최대 2배), 숨결 구간별 개수·1개당 확률(무기 = 용암의 숨결, 방어구 = 빙하의 숨결 — ItemId `"RefineBreath"`는 호환용), 장인의 기운 = 실패마다 확률×46.51%. 재료 무기 파괴석·방어구 수호석 + 돌파석 + 골드, 무기 1.5배
- **장비 성장**: 단계마다 재련 전 [성장](운명의 파편 + 실링) → `bGrowthDone`, 성공하면 false. 성장 전 재련 거부
- [장비 재련 N회](2~10): 성공하거나 재료가 떨어질 때까지, 시도마다 차감·저장
- 테스트: `LoARefineSim 25 10000`, `LoASetRefine All 15`

## 레이드 보상 · 경제 (`RaidReward`)
- `LoARaidReward::RollEchidna`: 수호석·파괴석·돌파석·골드·실링·운명의 파편 + 확률 숨결·장신구·보석·각인서·연마 파편·스톤·팔찌. 약 80회 클리어로 전 부위 +25가 되도록 역산
- 첫 클리어 ×3(`bFirstClearRewarded`, 테스트 명령 쓴 판 제외), [더보기] 1,500골드 = 화폐 제외 재료 +50%
- 지급: `PendingRewards`에 넣고 **즉시 디스크 저장** → 소지품에 넣고 `SaveNow`. 못 넣은 건 로비에서 2초마다 재시도. 실패·중단은 보상 없음

## 장신구 연마 (`AccessoryPolish`)
- 수치는 Maxroll `itemRandom` 5009/5109/5209. 유물 최대 3단계, 단계마다 효과 1개(종류 균등, 중복 없음), 등급 하 63 · 중 30 · 상 7
- **기본 효과(주 스탯·체력·깨달음 포인트)는 연마 단계로 읽는다**(`GetAccessoryStats(부위, PolishLevel)`) — 예전 `AccessoryTier`는 미사용
- 효과 없는 종류(파티·서포터·상태이상 등)는 표시 생략. 적용은 장비 수정치(`AdditiveDamage`, `WeaponPowerPercent`, `AttackPowerFlat`, `MaxHPFlat` 등)
- 비용 영롱한 보석 파편 + 골드(2·3단계는 추정). 착용 장신구 시작 세팅은 `bAccessoryPolishPresetV1`로 한 번 3단계 중·중
- 장신구에는 **전투 특성이 없다**(로아 T4) — 전투 특성은 아크 패시브 진화에서만. 예전 `StatRolls`는 불러올 때 지움

## 팔찌 · 어빌리티 스톤 (`BraceletStone`)
- 팔찌 `LoABracelet`(Maxroll 213300023#5): 고정 효과 2 + 부여 3칸, 부여 4회. 기본·전투 특성·특수 효과(하·중·상 6:3:1). 부여·변환 24,000실링, 재변환 60,000. 첫 부여는 바로 적용, 이후는 [효과 선택] 창에서 기존/신규 선택. 잠금 = 줄 오른쪽 자물쇠. 조건부 효과는 상시 최대 중첩으로 계산
- 스톤 `LoAStone`: 증가 각인 2줄 + 감소 1줄, 줄마다 10칸, 확률 75%에서 성공 -10 / 실패 +10(25~75, 공유). 1칸 1,925실링. 세공 보너스(성공 14회부터 체력, 레벨 합 5 → 공격력 1.5%). 자동 세공은 안 깎은 스톤만, DP로 줄 선택
- **스톤 각인은 장착한 각인에만** 더해짐(`GetStoneLevel` → `Eng_StoneRows`)
- 시작 팔찌·77돌은 `bHasStarterBraceletStone`으로 한 번

## 보석 (`GemComponent`)
- 아이템 `Gem_Dmg_01~10`(겁화)·`Gem_Cd_01~10`(작열), **칸마다 1개**(MaxStack 1). 스킬은 칸에 끼울 때 고름(스킬당 겁화 1 + 작열 1)
- 겁화 8% → 44%, 작열 6% → 24%, 보석마다 기본 공격력 0.05~1.2%(Lv2부터)
- 합성: 같은 레벨 3개 → 다음 레벨(종류는 재료 중 랜덤), 재료는 소지품에서 빠져 `FuseInputs`(저장됨, 다음 BeginPlay에 돌려줌). 목표 레벨 버튼은 누르는 순간 한 번 채움
- 보석 상자 `GemBox_Lv1`(1레벨 10개, 골드 1,000), 우클릭으로 열기
- 시작 Lv7 11개(겁화 6 · 작열 5). 변경은 보스 맵에서 금지(`IsBuildChangeAllowed`)
