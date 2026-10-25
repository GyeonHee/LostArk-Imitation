# Lobby — 대기 지역 · NPC

## 레벨 `Lv_EchidnaLobby` (World Partition)
- TopDown 템플릿 복제. **복제 레벨은 World Settings GameMode부터 확인** — `BP_TopDownGameMode`가 따라와 HUD·입력·구역 판정이 다 안 됐던 전례(→ `BP_LostArkGameMode`)
- MCP로 WP 레벨에 새 액터를 놓으면 `save_actor`/경로 지정 `save_assets`가 실패 → `save_assets([])`
- 배치: PlayerStart (-1300,0) → 정비소 (-300,0) → 입장 구역 (1300,0). +Y 벽(Y 1720, Yaw -90) 왼쪽부터 리엔(팔찌, -1600) · 칼도르(재련, -800) · 상점(0) · 페메르(연마, 800) · 어비르(스톤, 1600). 카메라 Yaw 0이라 화면 오른쪽 = +Y
- MCP 테스트: `StartPIE`의 `startTransform`으로 구역 안 스폰, PIE 중 `find_actors`는 PIE 월드 액터

## 구역 `ALobbyZoneActor` (Abstract)
- 정N각형 테두리 + 바닥 채움을 PMC로(`BorderSides` 64 = 원, 8 = 팔각형). 진입은 Tick의 2D 정다각형 판정(콜리전 불필요), 대상 = 0번 플레이어 폰
- **MID는 항상 원본 `ZoneMaterial`을 부모로, 슬롯 MID는 이 액터 소속일 때만 재사용** — PMC+MID를 OnConstruction에서 만드는 BP 액터의 공통 함정(MID의 MID / 리인스턴싱 후 회색 불투명)
- **`ARepairStationActor`**(BP_RepairStation): 진입 시 HP·MP 최대 + 전 슬롯 쿨타임 초기화 + 배틀아이템 보충, 안에서만 스킬 슬롯 등록 허용
- **`ARaidEntranceActor`**(BP_RaidEntrance): 3초 카운트다운(나가면 취소), 아이템 레벨 `RequiredItemLevel`(1620) 미만이면 안내만(매 틱 재확인). 0초 → 붙잡힘 + 페이드 이동, 이후 취소 불가
- 레벨 이동 사이 유지 = `ULoAGameInstance`(스킬 배치·소지품·장비 등). 사망·클리어·중단 복귀 시 `bReturnToRepairStation`으로 정비소 위치에 스폰

## NPC `AInteractableNPCActor` (상속 구조)
- 공통: 마네킹(`SKM_Quinn_Simple` + `MM_Idle`), 이름표, [G] 대화하기, 발광(`M_NPCHighlight` 오버레이), `InteractRadius`, 메뉴 카메라(`MenuCameraArmLength`/`MenuCameraPitch` — 옛 이름 `ShopCamera*`는 CoreRedirects), `MinimapIcon`
- NPC마다 다른 건 "어떤 창을 여는가"뿐 → 컨트롤러 `OpenNPCMenu`/`CloseNPCMenu`가 `Cast`로 분기. **새 NPC 종류는 두 함수에 분기 하나씩**
- 열기: 거리 안에서 G 또는 NPC 우클릭(이동 버튼을 `OnInputStarted`에서 가로챔). 메뉴 중 붙잡힘·HUD 숨김·K 차단·모든 클릭 소비
- **상점** `AShopNPCActor`(BP_ShopNPC): 구매(우클릭 1개 / Shift+우클릭 묶음, 칸당 99) · 판매(메뉴 중 소지품 우클릭 → 판매 목록 `SellReserve`). 구매 시 실링·소지품 공간 확인. 실링·골드 가격 혼합(`bGoldPrice`)
- **재련** `ARefineNPCActor`(칼도르, BP 없음): 재련 창 + 확인 팝업(숨결 개수) + 성장 팝업 + 성공 창. 규칙은 `Item/` 문서
- **장신구 연마** `APolishNPCActor`(페메르): 소지품에서 연마가 안 끝난 장신구만 목록
- **팔찌 효과 부여** `ABraceletNPCActor`(리엔, 이름 임시) / **스톤 세공** `AStoneNPCActor`(어비르): 목록은 소지품(착용 제외)에서 손볼 게 남은 것만(`LoAGearList::CollectWorkable/CommitChange`)
- 미니맵 아이콘 `/Game/LostArk/UI/Minimap/T_MinimapIcon_*NPC`(유저 캡처에서 잘라냄)
