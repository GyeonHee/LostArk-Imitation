#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaFanZoneActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/**
 * 에키드나 "뒤로 빠지며 좌우장판" 짤패턴에 쓰이는 부채꼴(전방 확산) 장판 1개.
 * 1) 스폰 즉시 TelegraphDuration(기본 1초) 동안 전체 부채꼴(FanInnerRadius~FanRange)을
 *    TelegraphColor로 표시만 하고 데미지는 없음 (예고 단계).
 * 2) 예고가 끝나면 FanColor로 바뀌면서 안쪽부터 바깥쪽으로 RingCount개 구간(고리)을
 *    RingInterval 간격으로 한 칸씩 넓혀간다 (와이파이 아이콘처럼 계단식 확장) —
 *    각 구간이 새로 열릴 때마다 그 구간 안의 대상에게 즉시 1회 데미지.
 * 순서 배치(1→2, 좌/우 각도)와 보스가 판정 시작 순간 뒤로 홉하는 연출은
 * StateTree Task(FStateTreeTask_EchidnaRetreatFanPattern)가 HasStartedExploding()을 폴링해서 담당.
 * 부채꼴 모양은 엔진 기본 메시로 표현이 안 돼서 ProceduralMeshComponent로 런타임에 직접 생성한다.
 * VFX 없이 반투명/불투명 색상 메시(TelegraphColor/FanColor)만으로 표현 — 기본 머티리얼은 M_MirrorLaser
 * (Translucent+Unlit, "Base Color" 파라미터). BaseMaterial 참조 보러 ApplyMeshColor 주석 참고.
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaFanZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaFanZoneActor();

	// 스폰 직후 호출 — 예고 타이머 시작
	void Activate(float InDamage, AController* InInstigator);

	// 부채꼴 시각화 메시 — 런타임에 사다리꼴 조각들을 이어붙여 생성 (기본값: BasicShapeMaterial)
	UPROPERTY(VisibleAnywhere, Category = "Fan")
	TObjectPtr<UProceduralMeshComponent> FanMeshComp;

	// 부채꼴 전체 각도 (도) — 정면 기준 좌우 절반씩
	UPROPERTY(EditDefaultsOnly, Category = "Fan", meta = (ClampMin = "1.0", ClampMax = "360.0"))
	float FanAngle = 70.f;

	// 부채꼴 최대 사거리 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Fan")
	float FanRange = 900.f;

	// 부채꼴 안쪽(액터 위치에 가까운 쪽) 반지름 (cm) — 첫 구간이 여기서부터 시작
	UPROPERTY(EditDefaultsOnly, Category = "Fan")
	float FanInnerRadius = 60.f;

	// 예고(텔레그래프) 표시 시간 (초) — 이 동안은 전체 부채꼴만 보이고 데미지 없음
	UPROPERTY(EditDefaultsOnly, Category = "Fan")
	float TelegraphDuration = 1.0f;

	// FanInnerRadius~FanRange를 몇 구간(고리)으로 나눠 한 칸씩 넓힐지 — 와이파이 아이콘처럼 계단식 확장
	UPROPERTY(EditDefaultsOnly, Category = "Fan", meta = (ClampMin = "1"))
	int32 RingCount = 5;

	// 구간이 한 칸씩 넓어지는 간격 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Fan")
	float RingInterval = 0.15f;

	// 부채꼴 곡선을 몇 조각으로 나눌지 — 값이 클수록 매끄러움
	UPROPERTY(EditDefaultsOnly, Category = "Fan", meta = (ClampMin = "2"))
	int32 ArcSegments = 24;

	// 마지막 구간까지 다 넓어진 후 액터 정리까지 대기 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Fan")
	float LifeAfterExplode = 1.0f;

	// 예고(예상 범위) 중 색상 — 반투명 빨강
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FLinearColor TelegraphColor = FLinearColor(1.f, 0.f, 0.f);

	// 예고 중 불투명도 (0=완전투명, 1=불투명) — 예상 범위는 반투명하게
	UPROPERTY(EditDefaultsOnly, Category = "VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TelegraphOpacity = 0.35f;

	// 고리 확장(실제 판정) 중 색상 — 완전 불투명 빨강
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FLinearColor FanColor = FLinearColor(1.f, 0.f, 0.f);

	// TelegraphColor/FanColor를 넣을 Vector Parameter 이름 — 기본 머티리얼(M_MirrorLaser)의 파라미터명이 "Base Color"라 이게 기본값
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FName ColorParameterName = TEXT("Base Color");

	// MID를 만들 원본 머티리얼 — FanMeshComp->GetMaterial(0)에 의존하지 않고 이 값을 직접 사용함.
	// ProceduralMeshComponent는 CDO 시점엔 메시 섹션이 없어서 생성자에서 SetMaterial()로 지정해도
	// 에디터가 "실제 섹션 수만큼만" 머티리얼 슬롯을 유지하려다 이 값을 지워버리는 경우가 있어,
	// 이 프로퍼티에 직접 보관해뒀다가 ApplyMeshColor에서 매번 명시적으로 사용
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TObjectPtr<UMaterialInterface> BaseMaterial;

	// 실제 판정(고리 확장) 중 불투명도 — Color 파라미터의 Alpha로 전달됨. 실행 범위는 완전 불투명하게.
	// FanMeshComp 머티리얼의 Blend Mode가 Translucent이고 Alpha가 Opacity에 연결돼 있어야 실제로 투명/불투명이 적용됨
	UPROPERTY(EditDefaultsOnly, Category = "VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FanOpacity = 1.0f;

	// 판정(고리 확장)마다 넉다운을 적용할지 — "뒤로 빠지며 좌우장판"(패턴4) 기본값. 매혹 스택형 패턴은 꺼서 씀
	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	bool bApplyKnockdownOnHit = true;

	// 판정마다 매혹 게이지를 쌓을지 — "끌고간후 장판터지는" 패턴처럼 넉백 없이 매혹만 쌓는 패턴에 사용.
	// 두 장판이 겹치는 구간에 서 있으면 양쪽 다 판정되어 CharmGaugePerHit의 2배가 쌓임
	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	bool bApplyCharmGaugeOnHit = false;

	UPROPERTY(EditDefaultsOnly, Category = "Effect", meta = (EditCondition = "bApplyCharmGaugeOnHit"))
	int32 CharmGaugePerHit = 1;

	// StateTree Task가 "예고가 끝나고 실제 판정이 시작됐는지"(보스 후방 홉 타이밍) 폴링할 때 사용
	bool HasStartedExploding() const { return bStartedExploding; }

	// StateTree Task가 "전 구간이 다 넓어졌는지" 폴링할 때 사용
	bool IsFinished() const { return bExploded; }

	UFUNCTION(BlueprintImplementableEvent, Category = "VFX")
	void BP_OnExplode(FVector Origin, FVector Direction);

protected:
	virtual void BeginPlay() override;

private:
	float Damage = 0.f;
	TWeakObjectPtr<AController> InstigatorController;
	bool bExploded = false;
	bool bStartedExploding = false;
	int32 CurrentRing = 0;

	FTimerHandle TelegraphTimerHandle;
	FTimerHandle RingTimerHandle;

	void ApplyMeshColor(const FLinearColor& Color, float Opacity);
	void BeginRingExpansion();
	void RevealNextRing();
	void BuildFanMesh(float OuterRadius);
	bool IsActorInRing(const AActor* Actor, float InnerRadius, float OuterRadius) const;
	void ApplyRingDamage(float InnerRadius, float OuterRadius);
};
