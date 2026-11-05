#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageNumberActor.generated.h"

class UWidgetComponent;

/**
 * 피격 데미지 숫자 하나를 띄우는 월드 액터 — 맞은 위치에 스폰되어 위로 떠오르며 서서히 사라지고 소멸한다.
 *
 * 표시는 `UWidgetComponent`(WidgetSpace=Screen)로 한다 — 매혹 게이지(CharmGaugeWidgetComponent)와 같은
 * 방식이라 탑다운 카메라 각도와 무관하게 항상 화면 투영으로 일정한 크기로 보인다.
 *
 * **겹친 숫자들의 앞뒤 순서**는 `SetTranslucentSortPriority()`로 정한다 — 스폰할 때 증가하는 카운터를
 * 넘겨주면 나중에 맞은 숫자가 항상 앞에 온다(AEchidnaBoss::ReceiveDamage 참조).
 *
 * WidgetClass는 여기서 하드코딩하지 않고 BP에서 WBP_DamageNumber로 할당한다(다른 위젯들과 동일 컨벤션).
 */
UCLASS(Blueprintable)
class LOA_API ADamageNumberActor : public AActor
{
	GENERATED_BODY()

public:
	ADamageNumberActor();

	virtual void Tick(float DeltaTime) override;

	/** 숫자 하나를 띄운다 — `LoA.DamageNumber.Pool`이 켜져 있으면 풀에서 꺼내고, 꺼져 있으면 새로 스폰한다 */
	static ADamageNumberActor* Show(UWorld* World, TSubclassOf<ADamageNumberActor> Class, const FVector& Location, AActor* Owner);

	/** 풀을 켠 경우 레벨 로딩 중에 미리 만들어 둔다 */
	static void PrewarmPool(UWorld* World, TSubclassOf<ADamageNumberActor> Class, int32 Count, AActor* Owner);

	/** 스폰 직후 1회 호출 — 숫자를 채우고 겹침 순서를 정한다. SortPriority가 클수록 앞에 그려진다 */
	void Activate(float Damage, int32 SortPriority);

	/** 숫자 대신 글자로 띄우기 — 떠오르기·페이드·소멸은 숫자와 똑같다 */
	void ActivateLabel(const FText& Label, FLinearColor Color, float FontSizeScale, int32 SortPriority);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber")
	TObjectPtr<UWidgetComponent> DamageWidgetComponent;

	// 완전히 사라지기까지 걸리는 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DamageNumber")
	float Lifetime = 3.f;

	// 위로 떠오르는 속도 (cm/s)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DamageNumber")
	float FloatUpSpeed = 90.f;

	// 수명의 이 비율이 지난 뒤부터 페이드 시작 (0~1) — 0이면 스폰 즉시 흐려지기 시작해 읽기 어렵다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DamageNumber", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FadeStartRatio = 0.35f;

protected:
	virtual void BeginPlay() override;

private:
	friend class UDamageNumberPool;

	/** 풀에서 꺼낼 때 — 위치·시간·투명도를 처음 상태로 되돌리고 보이게 한다 */
	void BeginUse(const FVector& Location, AActor* NewOwner);

	/** 풀로 돌아갈 때 — 숨기고 Tick을 멈춘다 */
	void EndUse();

	float Elapsed = 0.f;

	// 풀 소속이면 수명이 끝나도 파괴하지 않고 풀로 돌아간다
	bool bPooled = false;
};
