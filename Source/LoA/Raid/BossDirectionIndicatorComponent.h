#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "BossDirectionIndicatorComponent.generated.h"

class UMaterialInterface;

/**
 * 보스 발밑에 앞/뒤 방향을 알려주는 호(arc) 표시 — 로스트아크의 정면/백어택 표시와 같은 역할.
 * 보스에 붙는 컴포넌트라 보스가 회전하면 표시도 같이 돌아간다(별도 갱신 코드 불필요).
 *
 * - **정면 호**: 가운데가 바깥으로 뾰족하게 튀어나온 모양. 스파이크를 별도 삼각형으로 붙이지 않고
 *   "바깥 반지름을 각도의 함수로 두는" 방식으로 만든다 — 중심 각도에서 멀어질수록 0으로 수렴하는
 *   선형 보간이라 호와 자연스럽게 이어지고 특수 처리가 필요 없다
 * - **후방 호**: 스파이크 없이 매끈한 고리 조각
 *
 * HexArena 벽 / 부채꼴 장판 / 하트 메시와 동일하게 ProceduralMeshComponent로 직접 지오메트리를 만든다.
 * 머티리얼은 같은 컨벤션(M_MirrorLaser, Vector Parameter 이름 "Base Color", Translucent+Unlit)을 따른다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LOA_API UBossDirectionIndicatorComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	// UProceduralMeshComponent에는 기본 생성자가 없어서 FObjectInitializer를 그대로 넘겨줘야 한다
	UBossDirectionIndicatorComponent(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

	// 에디터 뷰포트에서도 보이도록 등록 시점에 한 번 만든다 — BeginPlay에서만 만들면 PIE를 돌리기 전까진
	// 아무것도 안 보여서 위치/크기를 눈으로 맞출 수가 없다
	virtual void OnRegister() override;

#if WITH_EDITOR
	// Details에서 반지름/각도/색을 바꾸면 즉시 반영
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** 파라미터를 바꾼 뒤 모양을 다시 만든다 */
	UFUNCTION(BlueprintCallable, Category = "Indicator")
	void RebuildIndicator();

	// 호의 안쪽/바깥쪽 반지름 — 두 값의 차이가 띠의 두께.
	// **오너 캡슐의 로컬 단위다.** 에키드나 캡슐은 스케일이 2배라 여기 100을 넣으면 월드에서는 200cm가 된다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator")
	float InnerRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator")
	float OuterRadius = 120.f;

	// 정면 호가 덮는 각도 (도) — 보스 로컬 +X를 중심으로 좌우로 절반씩
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator")
	float FrontArcAngle = 120.f;

	// 후방 호가 덮는 각도 (도) — 보스 로컬 -X를 중심으로
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator")
	float BackArcAngle = 120.f;

	// 정면 호 가운데가 바깥으로 튀어나오는 길이 (반지름과 같은 로컬 단위). 0이면 후방과 같은 매끈한 호가 된다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator")
	float FrontSpikeLength = 30.f;

	// 스파이크가 퍼지는 폭 (도) — 중심에서 이 각도만큼 떨어지면 튀어나온 길이가 0이 된다. 작을수록 뾰족
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator")
	float SpikeHalfAngle = 14.f;

	// 호 하나를 몇 조각으로 나눌지 — 클수록 매끄럽고 스파이크도 또렷해진다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator", meta = (ClampMin = "4"))
	int32 ArcSegments = 48;

	// 지면에서 띄우는 높이 (cm) — 0이면 바닥과 Z파이팅이 난다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator")
	float IndicatorHeight = 6.f;

	// true면 BeginPlay에서 오너 캐릭터의 캡슐 절반 높이만큼 아래로 내려 발밑에 딱 붙인다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Indicator")
	bool bSnapToOwnerFeet = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	FLinearColor FrontColor = FLinearColor(1.f, 1.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	FLinearColor BackColor = FLinearColor(0.25f, 0.85f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IndicatorOpacity = 0.9f;

	// M_MirrorLaser의 Vector Parameter 이름이 "Base Color"라 이게 기본값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	FName ColorParameterName = TEXT("Base Color");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	/** 호 한 조각을 만들어 지정한 섹션에 넣는다. SpikeLength가 0이면 매끈한 호 */
	void BuildArcSection(int32 SectionIndex, float CenterYawDeg, float ArcAngleDeg,
		float SpikeLength, const FLinearColor& Color);

	/** 오너 캐릭터의 캡슐 절반 높이만큼 내려 발밑에 붙인다 (GetActorLocation이 캡슐 중심이라 필요) */
	void ApplyFeetOffset();
};
