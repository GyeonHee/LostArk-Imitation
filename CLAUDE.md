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
| `SkillManagerComponent` | 스킬슬롯 관리, 쿨타임, 스킬트리 레벨 |
| `HUD_ViewModel` | MVVM — HP/MP 바 |
| `SkillTree_ViewModel` | MVVM — 스킬트리 창 전체 |
| `SkillTreeEntry_ViewModel` | MVVM — 스킬트리 행(Row) 단위 |

### 스킬 시스템
- `SkillBase` → `SkillInstant` / `SkillCast` / `SkillCharge` / `SkillCombo`
- `FSkillData` (DataTable 행): SkillName, InputType, ManaCost, Cooldown, Icon, MaxSkillLevel, SkillPointCostPerLevel 등
- `DT_Skills` — 스킬 데이터 테이블

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

## 구현된 기능 (2026-06-11 기준)
- [x] 마우스 클릭 이동
- [x] 대시 (스페이스바)
- [x] 스킬 시스템 (즉발/캐스팅/차지/콤보)
- [x] HP/MP UI (MVVM)
- [x] 스킬트리 창 (K키, MVVM, +/-버튼으로 레벨업)
- [x] 스킬트리 — 드래그앤드랍으로 HUD 슬롯 배정
- [x] 스킬트리 창 마우스 드래그 이동
- [x] 스킬트리 UI 위에서 게임 입력 차단
- [ ] DT_Skills SkillName/Icon/SkillTypeLabel 데이터 입력 필요
- [ ] 스킬 레벨에 따른 데미지 계수 연동
- [ ] AvailableSkillPoints UI 표시 연동

## 자주 쓰는 빌드 명령
```
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" LoA Win64 Development "C:\Users\User\Documents\Unreal Projects\LoA\LoA.uproject" -NoUBTMakefiles
```
