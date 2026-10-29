#include "Raid/EchidnaBarrierActor.h"
#include "Components/BoxComponent.h"

AEchidnaBarrierActor::AEchidnaBarrierActor()
{
	PrimaryActorTick.bCanEverTick = false;

	BoxComp = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComp"));
	SetRootComponent(BoxComp);
	BoxComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BoxComp->SetCollisionObjectType(ECC_WorldStatic);
	BoxComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BoxComp->SetGenerateOverlapEvents(false);
	BoxComp->SetHiddenInGame(true);
	BoxComp->SetCanEverAffectNavigation(false);
}

void AEchidnaBarrierActor::SetHalfExtent(const FVector& HalfExtent)
{
	BoxComp->SetBoxExtent(HalfExtent, true);
}
