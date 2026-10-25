# UI — 위젯

## 공통 규칙
- **새 UI는 C++ `BindWidget` 상속 WBP 또는 C++ 트리 위젯**으로 — MVVM은 바인딩을 에디터에서 손으로 걸어야 해서 MCP로 끝까지 못 만든다(MVVM은 `WBP_HUD`·`WBP_SkillTree`만)
- `BindWidget` 이름을 바꾸면 컴파일 에러, `BindWidgetOptional`은 없으면 그 기능만 안 보임
- **HUD 루트는 `HitTestInvisible`**(클릭 이동 게임). 창은 루트 `SelfHitTestInvisible` + 패널만 Visible
- **C++ 트리 위젯은 `RebuildWidget`에서 트리가 생긴다** — 값을 넣는 함수는 트리가 없으면 `TakeWidget()`부터(AddToViewport 전 글자가 조용히 버려진 전례 2건)
- **클릭을 먹는 C++ 위젯은 `NativeOnMouseButtonDoubleClick`도 Handled**(빠른 두 번째 클릭이 DoubleClick으로 와서 맵 이동으로 샜음)
- 위젯 클래스는 `BP_LoAPlayerController` Details에서 **유저가 직접 할당**(MCP CDO 쓰기는 PIE에서 null). 생성 지점마다 `[HUD]`/`[CastBar]` 같은 성공/실패 로그를 남길 것
- ViewModel 순서: Initialize → SetViewModel → AddToViewport
- ZOrder: 연기 -1 · HUD 0 · 미니맵/자원 띠 5 · 중단 버튼 6 · 대사 7 · 구역 알림 8 · 스킬트리·창 10 · 안내 11 · 드래그 아이콘 20 · 패배/클리어 30 · 툴팁 50
- 창 이동 공용 `FLoAWindowDrag`(`WindowDragHelper.h`) — 헤더를 끌어 이동, 화면 밖 클램프

## WBP_HUD (MVVM, `HUD_ViewModel`)
- 슬롯 위젯: 스킬 `Border_Q~F`(슬롯 0~7) / 아이템 `Border_F1,5~9` / 배틀아이템 `Border_1~4` / `Border_Dash`, `Border_GetUp`. **이름을 바꾸면 `LoAPlayerController.cpp` 상단 표도 바꿀 것**
- 쿨타임 UI는 재사용 함수가 아니라 **슬롯마다 Event Tick에 하드코딩된 노드 뭉치** — 새 슬롯은 대시 체인을 복사해 슬롯 번호만 바꾸는 게 기존 패턴. `RefreshingSlot`/`Slot Images`는 Q~F 아이콘 갱신용 별개 시스템(Construct의 Delay 뒤 채워짐 → BeginPlay 시점에 슬롯 이벤트를 쏘면 Accessed None)
- 대시/기상 칸은 HorizontalBox + Size To Content + Alignment 0.5 — **Collapsed**여야 자리가 빠져 남은 칸이 중앙. 칸 크기는 SizeBox로 고정(Auto면 텍스처 원본 크기)
- Construct의 Branch는 True/False가 모두 같은 Delay로 합류해야 함(True 미연결로 Q~F 아이콘 전체가 흰 박스였던 전례)
- 배틀아이템 칸은 `BuildBattleItemHUD()`가 `Border_N` 안을 C++로 바꿔 끼움(원형 쿨타임 `M_CooldownRadial` `CooldownRatio`)

## WBP_SkillTree (K키, MVVM `SkillTree_ViewModel`/`SkillTreeEntry_ViewModel`)
- 루트 Canvas **Visible**(입력 차단), `SkillTreeBG` 좌상단 앵커 + 명시적 Position, 마우스 드래그 이동
- On Mouse Move는 **항상 Handled**(Unhandled면 Enhanced Input이 K를 재평가해 토글이 두 번)
- 컨트롤러의 `SkillTree_ViewModel`·`HUDWidget`은 `BlueprintReadOnly` — OnDrop에서 HUD 슬롯을 참조할 때 씀
- 드랍은 HUD가 아니라 SkillTree의 OnDrop에서 처리(Z순서) — `SkillDragDropOperation`의 `SkillRowName`을 직접 참조(EntryViewModel 경유 시 null), `SkillSlotBorders` 히트 판정 → `AssignSkillToSlot`

## 보스 맵 위젯
- **`WBP_BossHP`**(`UBossHPWidget`): `HPBar`/`NextLineImage`/`LineText` BindWidget. 로아식 줄 바 — Percent = 현재 줄 잔량, 색 `LineColors[(현재줄-1) % N]`, 뒤 `NextLineImage` = 다음 줄 색(마지막 줄은 투명). **`HPBar` BackgroundImage 틴트 알파 0 필수**. 왼쪽 광폭화 타이머, 좌상단 `SettlementRoot`(초상화 + 정산 2칸, 정지 중 흑백 `T_EchidnaPortrait_Gray`). 무력화 게이지는 `BuildGroggyRow()`가 `BossHPRoot`에 C++로 붙임(**이름 바꾸면 안 붙음**). 레벨에 보스가 없으면 위젯을 안 만듦
- **`WBP_CastBar`**(`UCastBarWidget`): `GetActiveCastProgress()` 폴링. 컨트롤러 Tick에서 **`IsActionLocked()` 조기 return보다 앞**(뒤면 CC 중 바가 얼어붙음 — 광폭화 타이머 갱신도 같은 위치)
- **`ADamageNumberActor`** + `WBP_DamageNumber`: 월드 액터 + WidgetComponent(Screen). 지터, 카운터가 SortPriority(최신이 앞, `Counter!`는 +1000000), 페이드는 **안의 UserWidget `SetRenderOpacity`**(컴포넌트로는 Screen 스페이스에서 안 먹음). `ActivateLabel`로 글자 표시 재사용
- `UBossDialogueWidget`(대사창), `UScreenFogWidget`(연기 — 출처별 `ScreenFogSources`, `T_PinkFog`), `UCinematicOverlayWidget`(인트로), `UDefeatWidget`/`UClearWidget`(ZOrder 30, 클리어 루트 SelfHitTestInvisible), `UAbandonRaidWidget`(중단하기)

## 공용 HUD
- **`UMinimapWidget`**(C++만, 모든 맵): `NativePaint`에서 Slate 커스텀 버텍스로 직접 그림. 카메라 Yaw 기준. 보스 맵 = 타일·보스(숨김이면 숨김)·플레이어, 로비 = 바닥 사각형·구역·NPC(`MinimapIcon`, 비면 노란 점). `UUserWidget::Padding`과 이름이 겹쳐 `MapPadding`
- **`UResourceBarWidget`**(좌상단): 시간·실링·골드·운명의 파편, 인벤토리 델리게이트로만 갱신. 보스 맵에선 정산 게이지를 그 아래로 내림
- **`UCharmGaugeWidget`** + `WBP_CharmGauge`: 머리 위 WidgetComponent(Screen). 연꽃 120도 부채꼴 3조각(`/Game/UI/CharmGauge/T_CharmPetal_{1,2,3}_{Color,Gray}`) — Overlay에 흑백 3 → 컬러 3 겹침, `SetStacks(N)`. 이미지 크기를 원본으로 두면 화면을 덮음(작게 72x49)
- **`USkillTooltipWidget`**: HUD 스킬·대시·기상 칸, 스킬트리 줄(ListView → `GetListObjectFromEntry` → `GetRowName`)에 마우스 오버. 내용은 대상이 바뀔 때만 만들고 Tick엔 위치만(배치 전 `ForceLayoutPrepass` — 주기적으로 재생성하면 크기 계산 전 프레임에 겹쳐 깜빡임). 설명 `{D}` = 스펙 피해, 쿨타임은 기본값만

## 소지품 (`UInventoryWidget`, I키)
- 칸 위젯 100개는 한 번만 만들고 `OnSlotChanged(Index)`(INDEX_NONE = 전체)로 바뀐 칸만. 장비 칸 ▲▼ 비교 때문에 `OnEquipmentChanged` 구독
- 드래그 = UMG `UInventoryDragOperation`. 창 밖 놓기(`DragCancelled`)면 컨트롤러가 커서 아래 HUD 배틀 칸을 찾아 등록(HUD가 HitTestInvisible이라 UMG 드랍을 못 받음)
- 정렬 버튼, Ctrl+우클릭 잠금(파란 테두리, 아이템을 따라 움직임), 헤더 드래그 이동, 하단 띠 골드·실링 + 보석 합성 버튼(바탕 텍스처 3장 — Hover 빛은 그림)
- 칸 빛 훑기: `M_SlotShine`(UI 머티리얼, `Progress` 보간) — 마우스 올림·드래그 통과. `Width`를 바꾸면 cpp `InvSlot_ShineWidth`도. CVar `LoA.SlotShine`, `LoA.SlotShine.Duration`

## 캐릭터 정보 (P키, `UCharacterInfoWidget`)
- 탭 [장비]/[각인 `UEngravingPageWidget`]/[보석 `UGemPageWidget`]을 겹쳐 두고 보이는 것만. [상세정보] = 오른쪽 "전체 효과 정보"(실제 계산에 쓰는 값만 — 새 스탯 적용 시 `RefreshDetailPanel`에 줄 추가)
- 장신구 칸 왼쪽 ◆ 3개 = 연마 효과 등급색. 칸 위 글자는 안 띄움. 왼쪽 아래 [아크 패시브] 칸
- **3D 미리보기 `ACharacterPreviewActor`**(맵 위 Z 60000, SceneCapture → RT → UImage). **SceneCapture를 UI에 띄울 때 이 조합**: 자체 라이트 SpecularScale 0 + 라이팅 채널, 스카이라이트 ShowFlag 끔, **`SCS_FinalToneCurveHDR` + `RTF_RGBA16f`**(LDR 8비트는 톤매퍼 디더링으로 반짝임, SceneColorHDR은 검게 나옴), **TAA 켬**. 수동 노출 EV0이라 라이트가 매우 셈(15000/12500/12500). CVar `LoA.Preview.Light`/`LoA.Preview.Exposure`
- 공용 조각 `BuildUIHelpers.h`(원 = RoundedBox 브러시, ◆ 단계, 프리셋 줄, 각인 판). 버튼은 Border + 위치 판정
- 각인 효과 창 `UEngravingSetupWidget`·보석 합성 `UGemFuseWidget`·아크 패시브 `UArkPassiveWidget`: 규칙은 `Item/`·`Stat/` 문서. 합성 창은 `NativeTick`으로 소지품 패널 왼쪽에 붙어 따라감

## NPC 창 (규칙은 `Item/`, NPC는 `Lobby/` 문서)
- `UShopWidget`(구매/판매 탭, 묶음 팝업), `URefineWidget`(1200×700 — 작으면 버튼이 삐져나옴), `UAccessoryPolishWidget`, `UBraceletEnchantWidget`, `UStoneFacetWidget`, 왼쪽 목록 줄 공용 `UGearListRowWidget`
- 로비: `UZoneLabelWidget`(이름표), `UZoneNoticeWidget`(상단 알림), `NPCWorldWidgets`(이름표·[G] 대화하기)
