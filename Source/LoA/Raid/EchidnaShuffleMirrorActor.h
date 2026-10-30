#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CounterableInterface.h"
#include "EchidnaShuffleMirrorActor.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UWidgetComponent;

/**
 * 0줄 발악 "거울 섞기"의 거울 하나 — AEchidnaFinalPhaseActor가 맵 가운데 1개 + 외곽 8개를 스폰한다.
 * 위에서 내려다보는 카메라에서도 타원으로 보이도록 카메라를 향해 뒤로 눕혀 세운다(FaceDirection = 카메라 쪽).
 * 빨간 거울 여부는 진행자가 들고 있고, 이 액터는 보이는 것(빨간 발광·가라앉았다 올라오기)과 카운터 입력 전달만 한다.
 * 카운터는 돌풍 토네이도의 오브젝트 오버랩 → ICounterable — 맞았다는 사실만 OnCounterHit로 넘기고 정답 판정은 진행자가.
 * 비주얼은 거울 카운터 줄 거울(AEchidnaMirrorWallActor)과 같은 방식: 엔진 Cylinder + M_MirrorLaser + 상반신 초상화 PMC
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaShuffleMirrorActor : public AActor, public ICounterable
{
	GENERATED_BODY()

public:
	AEchidnaShuffleMirrorActor();

	virtual void Tick(float DeltaTime) override;

	/** 스폰 직후 한 번 — 크기·방향을 정하고 바닥에서 솟아오른다. bCounterable = 외곽 거울(가운데 거울은 장식).
	 *  bStaticPortrait = 평소 초상화를 띄울지 (가운데 거울은 비워 두고 PlayPortraitPass 때만 보인다) */
	void Setup(float Scale, const FVector& FaceDirection, bool bInCounterable, bool bStaticPortrait = true);

	/** 초상화가 거울 안을 가로질러 지나간다 — 화면 기준 왼쪽→오른쪽(bLeftToRight) 또는 반대.
	 *  거울 테두리(타원) 안만 보이게 매 틱 잘라 그리고, 진행 방향을 바라보는 텍스처를 쓴다 */
	void PlayPortraitPass(bool bLeftToRight, float Duration);
	bool IsPortraitPassing() const { return bPassing; }

	/** 빨간 발광 켜기/끄기 (정답 공개) */
	void SetRedGlow(bool bRed);

	/** 카운터 성공 표시 — 밝은 청백 발광 */
	void SetCounterGlow(bool bGlow);

	/** 카운터를 쳐야 하는 동안 — 파란 발광 (외곽 8개 전부) */
	void SetCounterReady(bool bReady);

	/** 거울 위 카운트다운("8s") — 음수면 숨김 */
	void SetCountdown(int32 Seconds);

	/** 바닥 아래로 가라앉았다가 다시 올라온다 (스왑 연출). 실제 위치는 그대로 */
	void PlayDip(float Duration);
	bool IsDipping() const { return AnimMode == EAnim::Dip; }

	/** 가라앉아 사라진 뒤 Destroy */
	void Vanish(float Duration);

	/** 진행자가 카운터를 받을 때만 켠다 — 꺼져 있으면 토네이도가 맞혀도 무시 */
	void SetAcceptCounter(bool bAccept) { bAcceptCounter = bAccept; }

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuffleMirrorCountered, AEchidnaShuffleMirrorActor* /*Mirror*/, AActor* /*Attacker*/);
	FOnShuffleMirrorCountered OnCounterHit;

	// ICounterable — 방향 조건 없음(어느 쪽에서 쳐도 됨)
	virtual bool TryCounterHit(AActor* Attacker, UPrimitiveComponent* HitComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShuffleMirror")
	TObjectPtr<USceneComponent> Root;

	// 거울 받침 — 바닥 높이, 카메라를 향해 기운다. 가라앉기 연출은 이걸 아래로 내린다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShuffleMirror")
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShuffleMirror")
	TObjectPtr<UStaticMeshComponent> MirrorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShuffleMirror")
	TObjectPtr<UProceduralMeshComponent> PortraitMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ShuffleMirror")
	TObjectPtr<UWidgetComponent> CountdownComp;

	// 카운트다운을 거울 꼭대기에서 얼마나 위에 띄울지 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Visual")
	float CountdownHeightOffset = 80.f;

	// Scale 1 기준 크기 (cm) — 거울 카운터 줄 거울과 같은 비율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Shape")
	float MirrorHeight = 270.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Shape")
	float MirrorWidth = 195.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Shape")
	float MirrorThickness = 20.f;

	// 카메라 쪽으로 눕히는 정도 (0 = 똑바로 섬, 1 = 카메라를 정면으로 봄). 거울 섞기는 비스듬한 시점이라 거의 세워 둔다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Shape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FaceCameraTilt = 0.25f;

	// 가라앉는 깊이 = 거울 키 + 이 여유 (cm) — 타일 아래로 완전히 숨게
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Anim")
	float SinkMargin = 60.f;

	// 스폰 때 솟아오르는 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Anim")
	float AppearDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Visual")
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Visual")
	FName ColorParameterName = TEXT("Base Color");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Visual")
	FLinearColor NormalColor = FLinearColor(0.85f, 0.75f, 0.95f, 0.95f);

	// HDR — 블룸으로 빨갛게 빛남
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Visual")
	FLinearColor RedColor = FLinearColor(9.f, 0.6f, 0.4f, 1.f);

	// 카운터 대기 — 거울 카운터 줄의 카운터 거울과 같은 청백
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Visual")
	FLinearColor CounterReadyColor = FLinearColor(1.5f, 3.5f, 9.f, 1.f);

	// 카운터 성공 — 대기보다 훨씬 밝게
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Visual")
	FLinearColor CounterColor = FLinearColor(5.f, 10.f, 20.f, 1.f);

	/** 초상화 머티리얼 — M_EchidnaMirrorPortrait(Texture "Portrait" × Vector "Tint") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Portrait")
	TObjectPtr<UMaterialInterface> PortraitMaterial;

	// 기본 초상화(fx_l_mirror_sden_01_cl) — 외곽 거울 평소 모습. 인물이 오른쪽에 치우쳐 있어 지나갈 땐 오른쪽→왼쪽
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Portrait")
	TObjectPtr<UTexture2D> PortraitTexture;

	// 01의 좌우 반전(fx_l_mirror_sden_02_cl) — 인물이 왼쪽에 치우쳐 있어 왼쪽→오른쪽으로 지나갈 때
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Portrait")
	TObjectPtr<UTexture2D> PortraitTextureFacingLeft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Portrait")
	FLinearColor NormalPortraitTint = FLinearColor(1.f, 1.f, 1.f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Portrait")
	FLinearColor RedPortraitTint = FLinearColor(3.f, 0.5f, 0.4f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ShuffleMirror|Portrait", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float PortraitInset = 0.88f;

private:
	enum class EAnim : uint8 { None, Appear, Dip, Vanish };

	void EnsurePortraitMID();
	/** 거울 타원을 "초상화 한 장 폭" 세로 띠(가운데 OffsetY)로 잘라 그린다. 띠 밖이면 비움. 정지 초상화는 OffsetY 0 */
	void BuildPortraitSection(float OffsetY);
	void ApplyColors();
	float GetSinkDepth() const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MirrorMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PortraitMID;

	EAnim AnimMode = EAnim::None;
	float AnimElapsed = 0.f;
	float AnimDuration = 0.f;

	float CurrentScale = 1.f;
	bool bPassing = false;
	bool bPassLeftToRight = true;
	float PassElapsed = 0.f;
	float PassDuration = 1.f;
	bool bCounterable = false;
	bool bAcceptCounter = false;
	bool bRed = false;
	bool bCounterGlow = false;
	bool bCounterReady = false;
};
