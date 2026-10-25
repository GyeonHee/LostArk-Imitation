#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IdentityGaugeWidget.generated.h"

class UImage;
class USizeBox;
class UTexture2D;
class USorceressIdentityComponent;

/**
 * 소서리스 아이덴티티 — 로아처럼 스킬 칸(Q~F)과 아이템 칸 사이 하단 가운데의 장식 틀.
 * 유저 캡처를 두 층으로 나눔 — T_IdentityArt(틀·Z·X 아이콘, 게이지 바 안쪽은 비움) / T_IdentityGlow(푸른 불빛).
 * 불빛은 M_IdentityGlow(UI 머티리얼: 노이즈로 UV를 흔들고 밝기를 일렁임)로 틀 아래에 깔고, 세기는 게이지에 따라
 * (가득 차면 맥동, 해방 중엔 흰빛). 나머지는 NativePaint로 위에 그린다:
 *  - 게이지 채움 (평소 파랑 / 100% 밝은 파랑 / 강화·해방 중엔 남은 시간, 해방은 흰빛)
 *  - 해방 중 Z 테두리를 반시계로 도는 마주 본 빛 두 줄기
 *  - 환류(Z 사용 불가)면 Z 어둡게 + 자물쇠, 쓸 수 없는 상태면 Z·X 어둡게, 점멸 쿨타임이면 X에 남은 초
 * 그림 좌표는 틀 텍스처 픽셀 기준(FrameSize)이고 Scale을 곱해 그린다. 소서리스가 아니면 숨는다
 * CVar: LoA.Identity.OffsetY(화면 아래에서 띄울 거리), LoA.Identity.Scale
 */
UCLASS()
class LOA_API UIdentityGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Identity")
	TSoftObjectPtr<UTexture2D> FrameTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Identity/T_IdentityArt.T_IdentityArt")));

	/** 불빛 머티리얼 — 파라미터 Strength(세기)·Tint(색), 텍스처 GlowTex = T_IdentityGlow */
	UPROPERTY(EditDefaultsOnly, Category = "Identity")
	TSoftObjectPtr<class UMaterialInterface> GlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Identity/M_IdentityGlow.M_IdentityGlow")));

	UPROPERTY(EditDefaultsOnly, Category = "Identity")
	TSoftObjectPtr<UTexture2D> LockTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Icon_Lock.T_Icon_Lock")));

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	UPROPERTY(Transient) TObjectPtr<USizeBox> Panel;
	UPROPERTY(Transient) TObjectPtr<UImage> FrameImage;
	UPROPERTY(Transient) TObjectPtr<UImage> GlowImage;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> GlowMID;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LoadedLock;

	FSlateBrush LockBrush;
	float ShownOffsetY = -1.f;
	float ShownScale = -1.f;
	float RingAngle = 0.f;   // 해방 빛 회전 (도) — 반시계라 줄어든다
	float GlowStrength = 0.f;  // 지금 불빛 세기 (목표로 부드럽게)
	float FullPulseTime = 0.f;

	USorceressIdentityComponent* FindIdentity() const;
	float GetScale() const;
	float GetOffsetY() const;
};
