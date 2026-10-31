#include "Item/AvatarComponent.h"
#include "LoACharacter.h"
#include "LoAGameInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

void UAvatarComponent::BeginPlay()
{
	Super::BeginPlay();

	Equipped.Init(NAME_None, LoAAvatars::SlotCount);
	if (const ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr)
	{
		for (int32 i = 0; i < LoAAvatars::SlotCount && i < GI->EquippedAvatars.Num(); ++i)
		{
			// 표에서 빠졌거나 칸이 바뀐 Id는 버린다
			const FLoAAvatarDef* Def = LoAAvatars::Find(GI->EquippedAvatars[i]);
			if (Def && static_cast<int32>(Def->Slot) == i) Equipped[i] = Def->Id;
		}
	}
	ApplyToOwner();
}

void UAvatarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveToGameInstance(false);
	Super::EndPlay(EndPlayReason);
}

FName UAvatarComponent::GetEquipped(EAvatarSlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	return Equipped.IsValidIndex(Index) ? Equipped[Index] : NAME_None;
}

void UAvatarComponent::Equip(FName AvatarId)
{
	const FLoAAvatarDef* Def = LoAAvatars::Find(AvatarId);
	if (!Def) return;
	const int32 Index = static_cast<int32>(Def->Slot);
	if (!Equipped.IsValidIndex(Index) || Equipped[Index] == AvatarId) return;
	Equipped[Index] = AvatarId;
	ApplyToOwner();
	SaveToGameInstance(true);
	OnAvatarChanged.Broadcast();
}

void UAvatarComponent::Unequip(EAvatarSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!Equipped.IsValidIndex(Index) || Equipped[Index].IsNone()) return;
	Equipped[Index] = NAME_None;
	ApplyToOwner();
	SaveToGameInstance(true);
	OnAvatarChanged.Broadcast();
}

USkeletalMesh* UAvatarComponent::ResolveMesh(EAvatarSlot Slot) const
{
	if (Slot == EAvatarSlot::Pants)
	{
		const FLoAAvatarDef* Chest = LoAAvatars::Find(GetEquipped(EAvatarSlot::Chest));
		if (Chest && Chest->bHidesPants) return nullptr;
	}
	const FLoAAvatarDef* Def = LoAAvatars::Find(GetEquipped(Slot));
	const FSoftObjectPath Path = Def ? Def->MeshPath : LoAAvatars::GetDefaultMesh(Slot);
	return Path.IsValid() ? Cast<USkeletalMesh>(Path.TryLoad()) : nullptr;
}

void UAvatarComponent::ApplyToOwner()
{
	ALoACharacter* Char = Cast<ALoACharacter>(GetOwner());
	USkeletalMeshComponent* Body = Char ? Char->GetMesh() : nullptr;
	if (!Body || !Body->GetSkeletalMeshAsset()) return;

	// 몸(통짜 모델) = 애니메이션 리더. 얼굴·눈·머리카락만 보이고 옷·맨살은 파츠가 맡는다
	LoAAvatars::ShowOnlyBaseBodySections(Body);

	PartComps.SetNum(LoAAvatars::SlotCount);
	for (int32 i = 0; i < LoAAvatars::SlotCount; ++i)
	{
		const EAvatarSlot Slot = static_cast<EAvatarSlot>(i);
		if (Slot == EAvatarSlot::Weapon) continue;

		USkeletalMeshComponent* Part = PartComps[i];
		if (!Part)
		{
			Part = NewObject<USkeletalMeshComponent>(Char, *FString::Printf(TEXT("AvatarPart_%d"), i));
			Part->SetupAttachment(Body);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->bUseBoundsFromLeaderPoseComponent = true;
			Part->RegisterComponent();
			PartComps[i] = Part;
		}
		USkeletalMesh* Mesh = ResolveMesh(Slot);
		Part->SetSkeletalMesh(Mesh);
		Part->SetLeaderPoseComponent(Body);
		Part->SetVisibility(Mesh != nullptr);
	}

	if (Char->WeaponMeshComp)
	{
		USkeletalMesh* Weapon = ResolveMesh(EAvatarSlot::Weapon);
		Char->WeaponMeshComp->SetSkeletalMesh(Weapon);
		Char->WeaponMeshComp->SetVisibility(Weapon != nullptr);
	}
}

void UAvatarComponent::SaveToGameInstance(bool bWriteDisk) const
{
	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (!GI || Equipped.Num() != LoAAvatars::SlotCount) return;
	GI->EquippedAvatars = Equipped;
	if (bWriteDisk) GI->SaveToDisk();
}
