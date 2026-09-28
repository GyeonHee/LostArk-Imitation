#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LobbyZoneActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UWidgetComponent;
class UZoneNoticeWidget;

/**
 * 대기 지역(정비소·보스 입장)의 바닥 구역 공통 베이스.
 * 정N각형 테두리(바닥 링 + 낮은 빛나는 띠) + 옅은 바닥 채움을 ProceduralMesh로 그린다 — BorderSides가 크면 원, 8이면 팔각형.
 * 플레이어가 구역 안에 들어왔는지는 오버랩이 아니라 Tick에서 2D 다각형 판정으로 직접 검사한다
 * (콜리전 채널 설정이 필요 없고, 팔각형도 모양 그대로 정확히 판정된다).
 * 들어오고 나가는 순간 OnPlayerEnterZone/OnPlayerExitZone이 한 번씩 불린다.
 */
UCLASS(Abstract)
class LOA_API ALobbyZoneActor : public AActor
{
	GENERATED_BODY()

public:
	ALobbyZoneActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	// 꼭짓점까지의 반지름 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (ClampMin = "50"))
	float ZoneRadius = 300.f;

	// 변의 개수 — 원은 넉넉히(64), 팔각형은 8
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (ClampMin = "3", ClampMax = "128"))
	int32 BorderSides = 64;

	// 첫 꼭짓점 각도 (도) — 팔각형은 22.5면 변이 축에 나란해진다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	float BorderAngleOffset = 0.f;

	// 테두리 두께 (외곽선에서 안쪽으로, cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Visual")
	float BorderWidth = 22.f;

	// 외곽선을 따라 세우는 빛나는 띠의 높이 (cm, 0이면 바닥 링만)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Visual")
	float BorderWallHeight = 25.f;

	// 바닥에서 띄우는 높이 (Z-fighting 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Visual")
	float ZOffset = 2.f;

	// M_MirrorLaser(Translucent+Unlit) "Base Color"에 들어가므로 1보다 큰 값이 곧 발광(블룸) 세기, Alpha = 불투명도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Visual")
	FLinearColor BorderColor = FLinearColor(6.f, 4.f, 1.2f, 0.9f);

	// 테두리 안쪽 바닥 채움 — Alpha 0이면 안 그림
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Visual")
	FLinearColor FillColor = FLinearColor(1.f, 0.8f, 0.3f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Visual")
	FName ColorParameterName = TEXT("Base Color");

	// 테두리·채움 색을 주입할 원본 머티리얼 — 동적 머티리얼(MID)은 항상 이걸 부모로 만든다.
	// 메시 슬롯의 머티리얼을 부모로 삼으면 BP 컴파일(리인스턴싱) 뒤 옛 액터 소속 MID가 버려져 슬롯이 비고 회색 기본 머티리얼로 그려진다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Visual")
	TObjectPtr<UMaterialInterface> ZoneMaterial;

	// 테두리 밝기가 천천히 숨쉬듯 오르내리는 폭(0이면 고정)과 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Visual")
	float PulseAmount = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Visual")
	float PulseSpeed = 2.f;

	// 구역 위에 떠 있는 이름표 (비우면 숨김)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Label")
	FText LabelText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Label")
	FLinearColor LabelColor = FLinearColor(1.f, 0.85f, 0.45f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Label")
	float LabelHeight = 220.f;

	// 이 위치(월드)가 구역 안인가 — 수평(2D) 판정
	UFUNCTION(BlueprintPure, Category = "Zone")
	bool IsInsideZone(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Zone")
	bool IsPlayerInside() const { return bPlayerInside; }

protected:
	virtual void BeginPlay() override;

	// 플레이어 폰(0번) 기준 — ALoACharacter가 아니어도 불린다(GameMode가 틀어져도 입장은 되게). 캐릭터 기능이 필요하면 하위 클래스가 캐스트
	virtual void OnPlayerEnterZone(APawn* Player) {}
	virtual void OnPlayerExitZone(APawn* Player) {}
	// 구역 안에 있는 동안 매 틱
	virtual void TickPlayerInZone(APawn* Player, float DeltaSeconds) {}

	// 런타임에 테두리 색을 바꾼다(예: 입장 확정 시 초록) — 숨쉬기 연출은 이 색 기준으로 계속된다
	void SetCurrentBorderColor(const FLinearColor& NewColor);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 화면 상단 중앙 알림 띠 — 처음 부를 때 만들고, 이후엔 글자만 갈아 끼운다. Counter를 비우면 숫자 줄 숨김
	void ShowNotice(const FText& Title, const FText& Message, const FText& Counter = FText::GetEmpty());
	void HideNotice();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone")
	TObjectPtr<UProceduralMeshComponent> ZoneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone")
	TObjectPtr<UWidgetComponent> LabelComp;

private:
	void BuildZoneMesh();
	void ApplyColors(float BorderIntensity);

	UPROPERTY(Transient)
	TObjectPtr<UZoneNoticeWidget> NoticeWidget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BorderMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FillMID;

	// 실제로 칠하는 테두리 색 — 기본은 BorderColor
	FLinearColor CurrentBorderColor = FLinearColor::White;
	bool bUseCurrentBorderColor = false;

	bool bPlayerInside = false;
	float PulseTime = 0.f;
};
