#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "LoADamageTypes.generated.h"

/** 플레이어가 받는 피해 종류 — 로아처럼 물리/마법 방어력이 따로 깎는다. Fixed = 방어력 무시(최대 HP 비율 기믹·즉사) */
UENUM(BlueprintType)
enum class ELoADamageKind : uint8
{
	Physical UMETA(DisplayName="물리"),
	Magical  UMETA(DisplayName="마법"),
	Fixed    UMETA(DisplayName="고정(방어 무시)"),
};

/** ApplyDamage의 DamageTypeClass로 넘기면 물리 피해. 아무것도 안 넘기면(UDamageType) 마법 — 에키드나 패턴은 전부 마법 */
UCLASS()
class LOA_API ULoAPhysicalDamageType : public UDamageType
{
	GENERATED_BODY()
};

/** 방어력·패턴 배율을 거치지 않는 피해 — "최대 HP × N" 기믹(파리지옥·그네·거울잇기 실패)용 */
UCLASS()
class LOA_API ULoATrueDamageType : public UDamageType
{
	GENERATED_BODY()
};
