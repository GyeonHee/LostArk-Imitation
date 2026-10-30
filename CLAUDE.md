# LoA 프로젝트 — Claude 컨텍스트

## 개요
- 탑다운 ARPG (로스트아크 모작), **Unreal Engine 5.8**, C++ 우선 + Blueprint 혼용
- 이동: 마우스 클릭 이동, NavMesh 없이 `RequestDirectMove` (보스 Patrol만 NavMesh 사용)
- 흐름: `Lv_EchidnaLobby`(대기 지역 — GameDefaultMap·EditorStartupMap) → `Echidna2-1`(에키드나 2관문, `AHexArena` 37타일) → 클리어/사망/중단 시 정비소로 복귀
- 캐릭터: 소서리스(`BP_Sorceress` + `DA_Sorceress`), 보스: `BP_Echidna` + StateTree `ST_Echidna`
- 수치는 최대한 실제 로아 값 — 출처는 Maxroll 게임 데이터 덤프(`Source/LoA/Stat/CLAUDE.md`)
- 계획서: `Docs/에키드나_2관문_구현계획서.html`, `Docs/장비_스탯_강화_구현계획서.md` / 테스트 명령·CVar·아이템 ID: `Docs/테스트_명령어.txt`(새로 만들면 갱신)

## 상세 문서 — 폴더별 CLAUDE.md (그 폴더 파일을 읽으면 자동으로 읽힘)
| 파일 | 내용 |
|---|---|
| `Source/LoA/CLAUDE.md` | 캐릭터(CC·매혹·사망), 컨트롤러(입력·HUD 클릭 차단·레벨 이동·사망/클리어), 저장, 데미지/HP 밸런스 |
| `Source/LoA/Skill/CLAUDE.md` | 스킬 클래스, 사거리 자동이동, 스킬 잠금, 계수·쿨타임 표 |
| `Source/LoA/Raid/CLAUDE.md` | 아레나·타일, 보스(광폭화·정산·카운터·그로기), StateTree 규칙, 패턴 전부 |
| `Source/LoA/UI/CLAUDE.md` | 위젯 규칙, HUD·스킬트리·보스 HP·소지품·P창·미니맵 등 |
| `Source/LoA/Item/CLAUDE.md` | 소지품·배틀아이템·장비·재련·보상·장신구 연마·팔찌/스톤·보석 |
| `Source/LoA/Stat/CLAUDE.md` | 스탯 공식·방어력·K값·각인·아크 패시브 |
| `Source/LoA/Lobby/CLAUDE.md` | 대기 지역, 정비소·입장 구역, NPC |
- **BP·에셋·MCP만 만지는 작업이라도 관련 폴더 문서를 먼저 읽을 것**

## 문서 관리 규칙
- 새 기능·버그 교훈은 **해당 폴더 CLAUDE.md**에 쓴다. 이 파일에는 프로젝트 전체에 걸친 규칙만
- 쓰는 것: 지금의 동작 · 의도 · 함정. 쓰지 않는 것: 변천사·폐기된 방식(교훈 한 줄만, 경위는 커밋 메시지), 코드에 보이는 기본값 나열(값의 이유가 중요할 때만), 날짜
- 폴더 문서가 한 화면을 크게 넘기면 오래된 기록부터 압축

## 주요 클래스
| 클래스 | 역할 |
|---|---|
| `ALoAPlayerController` | 이동, 스킬 입력, UI 생성·토글, NPC 메뉴, 인트로/사망/클리어, 레벨 이동 |
| `ALoACharacter` | HP/MP, CC, 매혹, 카메라 오버라이드, 컴포넌트 보유 |
| `ASorceress` / `USorceressIdentityComponent` | 소서리스(`BP_Sorceress` 부모), 아이덴티티(신비한 마력·마력 방출 Z·점멸 X) |
| `USkillManagerComponent` | 슬롯(0~7 Q~F, 8 기본공격, 18 대시, 19 즉시 기상), 쿨타임, 잠금, 자동이동 틱 |
| `UStatComponent` | 전투 스탯·수정치·데미지 계산 |
| `UInventoryComponent` / `UEquipmentComponent` / `UGemComponent` / `UEngravingComponent` / `UArkPassiveComponent` / `UBattleItemComponent` | 성장 시스템 |
| `AHexArena` / `AHexTile` | 아레나·개별 타일 |
| `AEchidnaBoss` / `AEchidnaBossAIController` | 보스 / StateTree AI |
| `EchidnaBossStateTreeUtility` | 모든 패턴 Task·조건 |
| `ULoAGameInstance` / `ULoASaveGame` | 레벨 간 유지 + 디스크 저장(`Saved/SaveGames/LoA.sav`) |
- 입력: `IMC_LoADefault`(이동/클릭), `SkillMappingContext`(스킬 키). 일부 키(1~4, I 등)는 C++ 직접 바인딩

## 전역 규칙 · 함정 (반드시 지킬 것)

### 값과 에셋
- **C++ 기본값은 에셋에 직렬화된 값이 덮어쓴다** — 밸런스 수치는 실제로 읽히는 곳을 고칠 것:
  `DA_Sorceress`(캐릭터 스탯) · `BP_Echidna` CDO + 레벨 배치 인스턴스(보스) · `ABP_HexTile` CDO(타일) · `ST_Echidna` Task 인스턴스(패턴 데미지·타이밍) · `DT_Skills`(스킬)
- 레벨 배치 액터는 자체 오버라이드가 CDO보다 우선 → Reset to Default
- **MCP로 CDO에 쓴 값은 에디터 조회·`save_assets` true여도 PIE에서 null일 수 있다**(위젯 클래스 할당 전례) → 위젯 클래스 등 컨트롤러 프로퍼티는 유저가 Details에서 직접. **MCP 쓰기는 PIE 로그로 검증**
- **StateTree는 MCP로 읽기만 가능** — 배치·Task 값은 유저가 에디터에서. 할 일 목록에 남길 것
- MCP 요령: `ObjectTools.set_properties`가 위 CDO 문제의 원인 / `TextureTools.import_file`은 덮어쓰기가 안 됨(`AssetTools.delete` 후 재임포트) / Niagara `AddModule`은 지정 모듈 **뒤**에 넣음 / World Partition 레벨 저장은 `save_assets([])`
- `EditDefaultsOnly` 값을 컴포넌트에 적용하는 건 생성자가 아니라 `BeginPlay`에서(BP 오버라이드 반영)
- 저장 파일에 들어가는 enum(`EEquipSlot` 등)은 **뒤에만 추가**. 기존 저장 파일에 새 지급이 필요하면 별도 1회 플래그
- **`Content/LocalOnly/`는 `.gitignore`**(GitHub LFS 무료 한도 약 10GB, 이미 8GB대) — 원본에서 통째로 가져온 안 쓰는 애셋(소서리스 의상·헤어·무기·공용 애니메이션·음성)을 둔다. 쓰게 되면 에디터에서 `LostArk/` 아래로 **옮긴 뒤** 참조·푸시(LocalOnly 애셋을 참조하면 다른 PC 클론에서 빠짐)
- 원본 게임 추출물(PSK/PSA)은 `Tools/Blender/`의 스크립트로 FBX 변환 → MCP 임포트. 함정·절차는 `Source/LoA/Raid/CLAUDE.md`(에키드나 애니메이션) 참고. Blender를 `--factory-startup`으로 돌리면 꺼진 확장으로 보고 PSK/PSA 애드온 의존 패키지를 지워 버림 — 쓰지 말 것

### 코드
- `TakeDamage` 오버라이드에서는 **`Super` 반환값이 아니라 인자 `DamageAmount`** 를 적용(반경 감쇠를 안 씀 — `ApplyRadialDamage`는 전부 `bDoFullDamage=true`인데도 Super가 0을 돌려준 버그). "최대 HP × N" 기믹은 `ULoATrueDamageType`(Fixed)
- 보스 HP는 `double`, 표기는 `FText::AsNumber(int64)`
- `GetActorLocation()` = 캡슐 **중심** — 바닥에 놓을 땐 캡슐 절반 높이를 뺄 것
- 패턴 액터: 광폭화 `CustomTimeDilation` + 타이머 나누기(`Raid/CLAUDE.md` 맨 위 공통 규칙)
- 커서 스킬은 `GetHitResultUnderCursor` 대신 `GetCursorAimLocation`/`GetCursorGroundTarget`
- UI: HUD 루트 HitTestInvisible, C++ 트리 위젯은 트리 생성 보장 후 값 넣기, 클릭 먹는 위젯은 DoubleClick도 처리, BeginPlay 시점에 HUD 바인딩 이벤트 쏘지 않기(`UI/CLAUDE.md`)
- PMC + MID를 `OnConstruction`에서 만드는 액터는 원본 머티리얼을 별도 프로퍼티로 들고 그걸 부모로 MID 생성
- **유니티 빌드**: 익명 네임스페이스 헬퍼도 파일 간 이름이 겹치면 C2084 → 파일 고유 접두어(`GetPoopBeamCapsuleRadius` 식)
- 모듈 의존성: `ProceduralMeshComponent`(.uproject 플러그인 + Build.cs), `SlateCore`(`FSlateBrush` 등을 멤버로 들면 LNK2019) — 둘 다 추가돼 있음

### 빌드 · Live Coding · 패키징
```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" LoAEditor Win64 Development "C:\Users\User\Documents\Unreal Projects\LoA\LoA.uproject" -NoUBTMakefiles
```
- Bash에선 경로 공백 때문에 실패 → PowerShell에서 `&`로
- **새 UCLASS/USTRUCT/UENUM·StateTree Task 추가, UPROPERTY 타입 변경, StateTree 인스턴스 데이터 레이아웃 변경은 Live Coding 말고 에디터 닫고 풀 빌드**
- 패키징: 레벨·코드 전용 에셋은 소프트 참조라 쿡이 못 따라감 → `DefaultGame.ini`의 `+MapsToCook`(새 레벨 추가 시), `+DirectoriesToAlwaysCook`(`/Game/LostArk/UI`, 패턴 VFX 폴더, Free_Magic VFX 등). 점검: `Source/LoA`의 `TEXT("/Game/...")` 경로를 `Saved/Cooked/Windows/LoA/Metadata/ReferencedSet.txt`와 대조
- 시작 시 "GameFeatureData" Asset Manager 에러는 MCP `AllToolsets` 플러그인 때문 — `DefaultGame.ini`에 스캔 항목을 넣어 해결함

## 남은 할 일
- [ ] `BP_EchidnaHeart` 다시 열어 컴파일 확인(HeartMeshComp가 PMC로 바뀌어 옛 오버라이드가 남았을 수 있음)
- [ ] 애니메이션 BP 연결: 사망 `OnDeathVisualChanged`, 보스 처치 `OnDefeatedVisual`, 각종 CC `On*VisualChanged`
- [ ] PIE 육안 확인: 매혹 게이지 머리 위 UI 위치·크기, `Border_GetUp` 위치/스타일
- [ ] K(6.09) 허수아비 `LoADpsTest`로 실측 보정 — 소서리스 아이덴티티(마력 해방·점화) 추가로 DPS가 크게 오름(추정 ×1.5~1.8), 실측 후 `DA_Sorceress`에 반영
- [ ] 0줄 발악 페이즈(`Raid/CLAUDE.md`): 진입 연출은 자리표시. 거울 섞기·저스트 가드·격돌 QTE PIE 확인(카메라·장판 반지름·판정 오차·연타 난이도)
- [ ] 미구현: 그로기 중 받는 피해 증가, 잡화상점, 로아 각인 영웅·전설 단계 / 나중에: 스킬 레벨 → 데미지 계수 연동, 스킬 포인트 UI
- 하지 않기로 함: 파훼 성공 보상(그네·거울잇기), 거울 카운터 실패 디버프(뒤따르는 불길 피해로 충분)
- [ ] 밸런스 미결: 신속 몰빵 시 이동속도·쿨감이 커서 회피 패턴이 쉬워질 수 있음, 보석 Lv5→10 성장 속도
- [ ] VFX 다듬기: `NS_Inferno_Impact` AddVelocity Z, `PER_Lava_Brutal` 스케일, 거울 레이저 `LaserVFXWidth`
