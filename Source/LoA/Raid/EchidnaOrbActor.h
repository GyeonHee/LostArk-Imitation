#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaOrbActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UMaterialInterface;

UENUM()
enum class EEchidnaOrbPhase : uint8
{
	Outgoing,	// 발사 방향으로 MaxRange까지 전진
	Returning,	// 왔던 경로를 되짚되 발사 지점을 관통해 반대편 -MaxRange까지 (이 구간 이동거리 = 2*MaxRange)
	Done		// 복귀 완료 — StateTree가 폴링으로 확인하고 소멸까지 기다린다
};

/**
 * 에키드나 "되돌아오는 구체(자야패턴)"에 쓰이는 큰 구체 1개.
 * 발사 방향으로 MaxRange까지 직선으로 나갔다가 **그 경로를 그대로 되짚어 발사 지점을 관통하고
 * 반대편 -MaxRange까지** 계속 간다. 즉 보스 앞뒤를 한 줄로 훑는다(+MaxRange → 0 → -MaxRange).
 * 발사 지점에서 멈추지 않으므로 보스 뒤에 있어도 안전하지 않다.
 * 레퍼런스의 "6개 구체 모두 되돌아오니 주의" — 나갈 때 피했어도 돌아올 때 다시 맞을 수 있다.
 *
 * 그래서 피격 기록(AlreadyHit)은 **왕복 구간마다 초기화**한다. 한 구체에 최대 두 번(나갈 때 1회,
 * 돌아올 때 1회) 맞을 수 있고, 한 구간 안에서는 같은 대상이 중복으로 맞지는 않는다.
 *
 * 비주얼은 엔진 기본 Sphere + M_MirrorLaser(색상만) — 거울 액터와 같은 컨벤션이라 VFX 에셋 없이도 바로 보인다.
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaOrbActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaOrbActor();

	virtual void Tick(float DeltaTime) override;

	/** 스폰 직후 1회 호출 — 이 방향으로 나갔다가 되돌아온다 */
	void Launch(const FVector& Direction, AController* InInstigator, float InDamage);

	/** StateTree Task가 스폰 직후(Launch 전) 호출 — **음수 인자는 "BP 기본값 유지"**를 뜻한다.
	 *  크기/판정 반지름은 BeginPlay에서 이미 한 번 적용된 뒤이므로 여기서 다시 적용해 덮어쓴다.
	 *  BP를 새로 파지 않고 패턴별로 구체 크기를 다르게 쓰고 싶을 때 사용 */
	void ApplyOverrides(float InVisualScale, float InCollisionRadius, float InSpeed, float InMaxRange);

	/** StateTree가 "왕복이 끝났는지" 폴링할 때 사용 */
	bool IsFinished() const { return Phase == EEchidnaOrbPhase::Done; }

	UPROPERTY(VisibleAnywhere, Category = "Orb")
	TObjectPtr<UStaticMeshComponent> OrbMeshComp;

	UPROPERTY(VisibleAnywhere, Category = "Orb")
	TObjectPtr<USphereComponent> CollisionComp;

	// 비행 속도 (cm/s)
	UPROPERTY(EditDefaultsOnly, Category = "Orb")
	float Speed = 700.f;

	// 이 거리까지 나간 뒤 되돌아온다 (cm). 복귀는 여기서 반대편 -MaxRange까지 이어지므로
	// 구체가 실제로 훑는 총 길이는 2*MaxRange다
	UPROPERTY(EditDefaultsOnly, Category = "Orb")
	float MaxRange = 1400.f;

	// 판정 반지름 (cm) — 큰 구체라 넉넉하게
	UPROPERTY(EditDefaultsOnly, Category = "Orb")
	float OrbCollisionRadius = 110.f;

	// 엔진 기본 Sphere(반지름 50cm) 기준 시각적 크기 배율 — BeginPlay에서 적용
	UPROPERTY(EditDefaultsOnly, Category = "Orb")
	float OrbVisualScale = 2.2f;

	// 맞으면 넉다운시킬지 — 큰 구체가 밀고 지나가는 느낌
	UPROPERTY(EditDefaultsOnly, Category = "Orb")
	bool bApplyKnockdownOnHit = true;

	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FLinearColor OrbColor = FLinearColor(0.85f, 0.25f, 1.f, 1.f);

	// M_MirrorLaser의 Vector Parameter 이름이 "Base Color"라 이게 기본값
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FName ColorParameterName = TEXT("Base Color");

	// 복귀 완료 후 소멸까지 대기 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Orb")
	float LifeAfterReturn = 0.2f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	EEchidnaOrbPhase Phase = EEchidnaOrbPhase::Outgoing;
	bool bLaunched = false;
	FVector FlyDirection = FVector::ForwardVector;
	float Damage = 0.f;
	float TraveledDistance = 0.f;
	TWeakObjectPtr<AController> InstigatorController;

	// 왕복 구간마다 초기화 — 나갈 때와 돌아올 때 각각 한 번씩 맞을 수 있게
	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> AlreadyHit;

	void ApplyHit(AActor* Target);
	void BeginReturn();
};
