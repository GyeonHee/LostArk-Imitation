#include "Item/AvatarTypes.h"
#include "Components/SkinnedMeshComponent.h"

// 소서리스 아바타 표 — /Game/LostArk/Character/Magician/Sorceress/Avatar/ 폴더(원본 의상 파츠 PSK 임포트)의 에셋 목록으로 만들었다.
// 파츠 하나 = 아바타 하나. 이름 = 부위 + 세트 번호(-변형). 세트 v1의 SK_HQ(통짜 메시)는 뺐다.
// Id는 저장 파일에 들어가니 바꾸지 말 것(새 아바타는 아래에 추가)

namespace
{
	struct FAvatarRow
	{
		const TCHAR* Id;
		const TCHAR* Name;
		EAvatarSlot Slot;
		const TCHAR* MeshPath;
		bool bHidesPants;
	};

	const FAvatarRow Avatar_Rows[] = {
		{ TEXT("Av_Head_Base1"), TEXT("머리 기본 1"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Body/v1/SK_helmet.SK_helmet"), false },
		{ TEXT("Av_Head_Base2"), TEXT("머리 기본 2"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Body/v2/SK_helmet.SK_helmet"), false },
		{ TEXT("Av_Head_Set1_1"), TEXT("머리 1-1"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v1/SK_helmet_v1.SK_helmet_v1"), false },
		{ TEXT("Av_Head_Set1_2"), TEXT("머리 1-2"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v1/SK_helmet_v2.SK_helmet_v2"), false },
		{ TEXT("Av_Head_Set1_3"), TEXT("머리 1-3"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v1/SK_helmet_v3.SK_helmet_v3"), false },
		{ TEXT("Av_Head_Set2_1"), TEXT("머리 2-1"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v2/SK_helmet_v1.SK_helmet_v1"), false },
		{ TEXT("Av_Head_Set2_2"), TEXT("머리 2-2"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v2/SK_helmet_v2.SK_helmet_v2"), false },
		{ TEXT("Av_Head_Set2_3"), TEXT("머리 2-3"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v2/SK_helmet_v3.SK_helmet_v3"), false },
		{ TEXT("Av_Head_Set3_1"), TEXT("머리 3-1"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v3/SK_helmet_v1.SK_helmet_v1"), false },
		{ TEXT("Av_Head_Set3_2"), TEXT("머리 3-2"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v3/SK_helmet_v2.SK_helmet_v2"), false },
		{ TEXT("Av_Head_Set4"), TEXT("머리 4"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v4/SK_helmet.SK_helmet"), false },
		{ TEXT("Av_Head_Set5_1"), TEXT("머리 5-1"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v5/SK_helmet_v1.SK_helmet_v1"), false },
		{ TEXT("Av_Head_Set5_2"), TEXT("머리 5-2"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v5/SK_helmet_v2.SK_helmet_v2"), false },
		{ TEXT("Av_Head_Set6_1"), TEXT("머리 6-1"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v6/SK_helmet_v1.SK_helmet_v1"), false },
		{ TEXT("Av_Head_Set6_2"), TEXT("머리 6-2"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v6/SK_helmet_v2.SK_helmet_v2"), false },
		{ TEXT("Av_Head_Set7_1"), TEXT("머리 7-1"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v7/SK_helmet_v1.SK_helmet_v1"), false },
		{ TEXT("Av_Head_Set7_2"), TEXT("머리 7-2"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v7/SK_helmet_v2.SK_helmet_v2"), false },
		{ TEXT("Av_Head_Set8_1"), TEXT("머리 8-1"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v8/SK_helmet_v1.SK_helmet_v1"), false },
		{ TEXT("Av_Head_Set8_2"), TEXT("머리 8-2"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v8/SK_helmet_v2.SK_helmet_v2"), false },
		{ TEXT("Av_Head_Set9"), TEXT("머리 9"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v9/SK_helmet.SK_helmet"), false },
		{ TEXT("Av_Head_Set10"), TEXT("머리 10"), EAvatarSlot::Head, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v10/SK_helmet.SK_helmet"), false },
		{ TEXT("Av_Shoulder_Set1"), TEXT("어깨 1"), EAvatarSlot::Shoulder, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v1/SK_shoulder.SK_shoulder"), false },
		{ TEXT("Av_Shoulder_Set2"), TEXT("어깨 2"), EAvatarSlot::Shoulder, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v2/SK_shoulder.SK_shoulder"), false },
		{ TEXT("Av_Shoulder_Set3_1"), TEXT("어깨 3-1"), EAvatarSlot::Shoulder, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v3/SK_shoulder_v1.SK_shoulder_v1"), false },
		{ TEXT("Av_Shoulder_Set3_2"), TEXT("어깨 3-2"), EAvatarSlot::Shoulder, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v3/SK_shoulder_v2.SK_shoulder_v2"), false },
		{ TEXT("Av_Shoulder_Set4"), TEXT("어깨 4"), EAvatarSlot::Shoulder, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v4/SK_shoulder.SK_shoulder"), false },
		{ TEXT("Av_Shoulder_Set5_1"), TEXT("어깨 5-1"), EAvatarSlot::Shoulder, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v5/SK_shoulder_v1.SK_shoulder_v1"), false },
		{ TEXT("Av_Shoulder_Set5_2"), TEXT("어깨 5-2"), EAvatarSlot::Shoulder, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v5/SK_shoulder_v2.SK_shoulder_v2"), false },
		{ TEXT("Av_Shoulder_Set6"), TEXT("어깨 6"), EAvatarSlot::Shoulder, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v6/SK_shoulder.SK_shoulder"), false },
		{ TEXT("Av_Shoulder_Set7"), TEXT("어깨 7"), EAvatarSlot::Shoulder, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v7/SK_shoulder.SK_shoulder"), false },
		{ TEXT("Av_Shoulder_Set8"), TEXT("어깨 8"), EAvatarSlot::Shoulder, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v8/SK_shoulder.SK_shoulder"), false },
		{ TEXT("Av_Chest_Base1"), TEXT("상의 기본 1"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Body/v1/SK_upper.SK_upper"), false },
		{ TEXT("Av_Chest_Base2"), TEXT("상의 기본 2"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Body/v2/SK_upper.SK_upper"), false },
		{ TEXT("Av_Chest_Set1_1"), TEXT("상의 1-1"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v1/SK_upper_v1.SK_upper_v1"), false },
		{ TEXT("Av_Chest_Set1_2"), TEXT("상의 1-2"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v1/SK_upper_v2.SK_upper_v2"), false },
		{ TEXT("Av_Chest_Set2_1"), TEXT("상의 2-1"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v2/SK_upper_v1.SK_upper_v1"), false },
		{ TEXT("Av_Chest_Set2_2"), TEXT("상의 2-2"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v2/SK_upper_v2.SK_upper_v2"), false },
		{ TEXT("Av_Chest_Set3_1"), TEXT("상의 3-1"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v3/SK_upper_v1.SK_upper_v1"), false },
		{ TEXT("Av_Chest_Set3_2"), TEXT("상의 3-2"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v3/SK_upper_v2.SK_upper_v2"), false },
		{ TEXT("Av_Chest_Set4"), TEXT("상의 4"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v4/SK_upper.SK_upper"), false },
		{ TEXT("Av_Chest_Set5_1"), TEXT("상의 5-1"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v5/SK_upper_v1.SK_upper_v1"), false },
		{ TEXT("Av_Chest_Set5_2"), TEXT("상의 5-2"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v5/SK_upper_v2.SK_upper_v2"), false },
		{ TEXT("Av_Chest_Set6"), TEXT("상의 6"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v6/SK_upper.SK_upper"), false },
		{ TEXT("Av_Chest_Set7"), TEXT("상의 7"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v7/SK_upper.SK_upper"), false },
		{ TEXT("Av_Chest_Set8"), TEXT("상의 8"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v8/SK_upper.SK_upper"), false },
		{ TEXT("Av_Chest_Set9"), TEXT("상의 9"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v9/SK_dress.SK_dress"), true },
		{ TEXT("Av_Chest_Set10"), TEXT("상의 10"), EAvatarSlot::Chest, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v10/SK_upper.SK_upper"), false },
		{ TEXT("Av_Gloves_Set1"), TEXT("장갑 1"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v1/SK_arm.SK_arm"), false },
		{ TEXT("Av_Gloves_Set2_1"), TEXT("장갑 2-1"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v2/SK_arm_v1.SK_arm_v1"), false },
		{ TEXT("Av_Gloves_Set2_2"), TEXT("장갑 2-2"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v2/SK_arm_v2.SK_arm_v2"), false },
		{ TEXT("Av_Gloves_Set3_1"), TEXT("장갑 3-1"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v3/SK_arm_v1.SK_arm_v1"), false },
		{ TEXT("Av_Gloves_Set3_2"), TEXT("장갑 3-2"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v3/SK_arm_v2.SK_arm_v2"), false },
		{ TEXT("Av_Gloves_Set4"), TEXT("장갑 4"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v4/SK_arm.SK_arm"), false },
		{ TEXT("Av_Gloves_Set5_1"), TEXT("장갑 5-1"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v5/SK_arm_v1.SK_arm_v1"), false },
		{ TEXT("Av_Gloves_Set5_2"), TEXT("장갑 5-2"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v5/SK_arm_v2.SK_arm_v2"), false },
		{ TEXT("Av_Gloves_Set5_3"), TEXT("장갑 5-3"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v5/SK_arm_v3.SK_arm_v3"), false },
		{ TEXT("Av_Gloves_Set6"), TEXT("장갑 6"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v6/SK_arm.SK_arm"), false },
		{ TEXT("Av_Gloves_Set7_1"), TEXT("장갑 7-1"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v7/SK_arm_v1.SK_arm_v1"), false },
		{ TEXT("Av_Gloves_Set7_2"), TEXT("장갑 7-2"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v7/SK_arm_v2.SK_arm_v2"), false },
		{ TEXT("Av_Gloves_Set8_1"), TEXT("장갑 8-1"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v8/SK_arm_v1.SK_arm_v1"), false },
		{ TEXT("Av_Gloves_Set8_2"), TEXT("장갑 8-2"), EAvatarSlot::Gloves, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v8/SK_arm_v2.SK_arm_v2"), false },
		{ TEXT("Av_Pants_Base1"), TEXT("하의 기본 1"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Body/v1/SK_lower.SK_lower"), false },
		{ TEXT("Av_Pants_Base2"), TEXT("하의 기본 2"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Body/v2/SK_lower.SK_lower"), false },
		{ TEXT("Av_Pants_Set1_1"), TEXT("하의 1-1"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v1/SK_lower_v1.SK_lower_v1"), false },
		{ TEXT("Av_Pants_Set1_2"), TEXT("하의 1-2"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v1/SK_lower_v2.SK_lower_v2"), false },
		{ TEXT("Av_Pants_Set1_3"), TEXT("하의 1-3"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v1/SK_lower_v3.SK_lower_v3"), false },
		{ TEXT("Av_Pants_Set2_1"), TEXT("하의 2-1"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v2/SK_lower_v1.SK_lower_v1"), false },
		{ TEXT("Av_Pants_Set2_2"), TEXT("하의 2-2"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v2/SK_lower_v2.SK_lower_v2"), false },
		{ TEXT("Av_Pants_Set3_1"), TEXT("하의 3-1"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v3/SK_lower_v1.SK_lower_v1"), false },
		{ TEXT("Av_Pants_Set3_2"), TEXT("하의 3-2"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v3/SK_lower_v2.SK_lower_v2"), false },
		{ TEXT("Av_Pants_Set3_3"), TEXT("하의 3-3"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v3/SK_lower_v3.SK_lower_v3"), false },
		{ TEXT("Av_Pants_Set4"), TEXT("하의 4"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v4/SK_lower.SK_lower"), false },
		{ TEXT("Av_Pants_Set5_1"), TEXT("하의 5-1"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v5/SK_lower_v1.SK_lower_v1"), false },
		{ TEXT("Av_Pants_Set5_2"), TEXT("하의 5-2"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v5/SK_lower_v2.SK_lower_v2"), false },
		{ TEXT("Av_Pants_Set6_1"), TEXT("하의 6-1"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v6/SK_lower_v1.SK_lower_v1"), false },
		{ TEXT("Av_Pants_Set6_2"), TEXT("하의 6-2"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v6/SK_lower_v2.SK_lower_v2"), false },
		{ TEXT("Av_Pants_Set7_1"), TEXT("하의 7-1"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v7/SK_lower_v1.SK_lower_v1"), false },
		{ TEXT("Av_Pants_Set7_2"), TEXT("하의 7-2"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v7/SK_lower_v2.SK_lower_v2"), false },
		{ TEXT("Av_Pants_Set7_3"), TEXT("하의 7-3"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v7/SK_lower_v3.SK_lower_v3"), false },
		{ TEXT("Av_Pants_Set8"), TEXT("하의 8"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v8/SK_lower.SK_lower"), false },
		{ TEXT("Av_Pants_Set10"), TEXT("하의 10"), EAvatarSlot::Pants, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Armor/v10/SK_lower.SK_lower"), false },
		{ TEXT("Av_Weapon_1"), TEXT("무기 1"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W1/SK_wp_wmem_10_sk.SK_wp_wmem_10_sk"), false },
		{ TEXT("Av_Weapon_2"), TEXT("무기 2"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W2/SK_wp_wmem_11_sk.SK_wp_wmem_11_sk"), false },
		{ TEXT("Av_Weapon_3"), TEXT("무기 3"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W3/SK_wp_wmem_06_sk.SK_wp_wmem_06_sk"), false },
		{ TEXT("Av_Weapon_4"), TEXT("무기 4"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W4/SK_wp_wmem_01_sk.SK_wp_wmem_01_sk"), false },
		{ TEXT("Av_Weapon_5"), TEXT("무기 5"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W5/SK_wp_wmem_07_sk.SK_wp_wmem_07_sk"), false },
		{ TEXT("Av_Weapon_6"), TEXT("무기 6"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W6/SK_wp_wmem_02_sk.SK_wp_wmem_02_sk"), false },
		{ TEXT("Av_Weapon_7_1"), TEXT("무기 7-1"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W7/SK_wp_wmem_08_high_sk.SK_wp_wmem_08_high_sk"), false },
		{ TEXT("Av_Weapon_7_2"), TEXT("무기 7-2"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W7/SK_wp_wmem_08_sk.SK_wp_wmem_08_sk"), false },
		{ TEXT("Av_Weapon_8"), TEXT("무기 8"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W8/SK_wp_wmem_03_sk.SK_wp_wmem_03_sk"), false },
		{ TEXT("Av_Weapon_9"), TEXT("무기 9"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W9/SK_wp_wmem_09_sk.SK_wp_wmem_09_sk"), false },
		{ TEXT("Av_Weapon_10_1"), TEXT("무기 10-1"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W10/SK_wp_wmem_04_1_sk.SK_wp_wmem_04_1_sk"), false },
		{ TEXT("Av_Weapon_10_2"), TEXT("무기 10-2"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W10/SK_wp_wmem_04_sk.SK_wp_wmem_04_sk"), false },
		{ TEXT("Av_Weapon_11"), TEXT("무기 11"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W11/SK_wp_wmem_05_sk.SK_wp_wmem_05_sk"), false },
		{ TEXT("Av_Weapon_12"), TEXT("무기 12"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W12/SK_wp_wmem_00_sk.SK_wp_wmem_00_sk"), false },
		{ TEXT("Av_Weapon_13_1"), TEXT("무기 13-1"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W13/SK_wp_wmem_tf01_00_sk.SK_wp_wmem_tf01_00_sk"), false },
		{ TEXT("Av_Weapon_13_2"), TEXT("무기 13-2"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W13/SK_wp_wmem_tf01_01_sk.SK_wp_wmem_tf01_01_sk"), false },
		{ TEXT("Av_Weapon_13_3"), TEXT("무기 13-3"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W13/SK_wp_wmem_tf01_02_sk.SK_wp_wmem_tf01_02_sk"), false },
		{ TEXT("Av_Weapon_14"), TEXT("무기 14"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W14/SK_wp_wmem_tf01_07_sk.SK_wp_wmem_tf01_07_sk"), false },
		{ TEXT("Av_Weapon_15_1"), TEXT("무기 15-1"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W15/SK_wp_wmem_tf01_08_sk.SK_wp_wmem_tf01_08_sk"), false },
		{ TEXT("Av_Weapon_15_2"), TEXT("무기 15-2"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W15/SK_wp_wmem_tf01_09_sk.SK_wp_wmem_tf01_09_sk"), false },
		{ TEXT("Av_Weapon_16_1"), TEXT("무기 16-1"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W16/SK_wp_wmem_tf01_03_sk.SK_wp_wmem_tf01_03_sk"), false },
		{ TEXT("Av_Weapon_16_2"), TEXT("무기 16-2"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W16/SK_wp_wmem_tf01_04_sk.SK_wp_wmem_tf01_04_sk"), false },
		{ TEXT("Av_Weapon_16_3"), TEXT("무기 16-3"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W16/SK_wp_wmem_tf01_05_sk.SK_wp_wmem_tf01_05_sk"), false },
		{ TEXT("Av_Weapon_17"), TEXT("무기 17"), EAvatarSlot::Weapon, TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Weapon/W17/SK_wp_wmem_tf01_06_sk.SK_wp_wmem_tf01_06_sk"), false },
	};

	// 칸이 비었을 때 모습 — 기본 옷(Body/v1)과 기본 지팡이. 어깨·장갑은 없음
	const TCHAR* Avatar_DefaultMeshes[] = {
		TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Body/v1/SK_helmet.SK_helmet"),
		nullptr,
		TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Body/v1/SK_upper.SK_upper"),
		nullptr,
		TEXT("/Game/LostArk/Character/Magician/Sorceress/Avatar/Body/v1/SK_lower.SK_lower"),
		TEXT("/Game/LostArk/Character/Magician/Sorceress/Model/Weapon/SK_Sorceress_Staff.SK_Sorceress_Staff"),
	};
	static_assert(UE_ARRAY_COUNT(Avatar_DefaultMeshes) == static_cast<int32>(EAvatarSlot::MAX), "칸마다 기본 메시");

	TArray<FLoAAvatarDef> BuildAvatarDefs()
	{
		TArray<FLoAAvatarDef> Defs;
		for (const FAvatarRow& Row : Avatar_Rows)
		{
			FLoAAvatarDef& Def = Defs.AddDefaulted_GetRef();
			Def.Id = Row.Id;
			Def.Name = FText::FromString(Row.Name);
			Def.Slot = Row.Slot;
			Def.MeshPath = FSoftObjectPath(Row.MeshPath);
			Def.bHidesPants = Row.bHidesPants;
		}
		return Defs;
	}
}

namespace LoAAvatars
{
	const TArray<FLoAAvatarDef>& All()
	{
		static const TArray<FLoAAvatarDef> Defs = BuildAvatarDefs();
		return Defs;
	}

	const FLoAAvatarDef* Find(FName Id)
	{
		if (Id.IsNone()) return nullptr;
		return All().FindByPredicate([Id](const FLoAAvatarDef& Def) { return Def.Id == Id; });
	}

	TArray<const FLoAAvatarDef*> ForSlot(EAvatarSlot Slot)
	{
		TArray<const FLoAAvatarDef*> Result;
		for (const FLoAAvatarDef& Def : All())
		{
			if (Def.Slot == Slot) Result.Add(&Def);
		}
		return Result;
	}

	FSoftObjectPath GetDefaultMesh(EAvatarSlot Slot)
	{
		const int32 Index = static_cast<int32>(Slot);
		return Avatar_DefaultMeshes[Index] ? FSoftObjectPath(Avatar_DefaultMeshes[Index]) : FSoftObjectPath();
	}

	void ShowOnlyBaseBodySections(USkinnedMeshComponent* Body)
	{
		if (!Body) return;
		// SK_Sorceress 슬롯 이름 — 얼굴(face_hiPol)·속눈썹·눈동자·머리카락만 남긴다
		static const FName BaseSlots[] = { TEXT("face_hiPol"), TEXT("lashes_hiP"), TEXT("pupill_hi_P"), TEXT("hair_mat_1") };
		const TArray<FName> SlotNames = Body->GetMaterialSlotNames();
		const int32 NumLODs = FMath::Max(1, Body->GetNumLODs());
		for (int32 Lod = 0; Lod < NumLODs; ++Lod)
		{
			for (int32 i = 0; i < SlotNames.Num(); ++i)
			{
				bool bKeep = false;
				for (const FName& Keep : BaseSlots) bKeep |= SlotNames[i] == Keep;
				Body->ShowMaterialSection(i, i, bKeep, Lod);
			}
		}
	}

	FText GetSlotName(EAvatarSlot Slot)
	{
		switch (Slot)
		{
		case EAvatarSlot::Head:     return NSLOCTEXT("Avatar", "Head", "머리");
		case EAvatarSlot::Shoulder: return NSLOCTEXT("Avatar", "Shoulder", "어깨");
		case EAvatarSlot::Chest:    return NSLOCTEXT("Avatar", "Chest", "상의");
		case EAvatarSlot::Gloves:   return NSLOCTEXT("Avatar", "Gloves", "장갑");
		case EAvatarSlot::Pants:    return NSLOCTEXT("Avatar", "Pants", "하의");
		case EAvatarSlot::Weapon:   return NSLOCTEXT("Avatar", "Weapon", "무기");
		default:                    return FText::GetEmpty();
		}
	}
}
