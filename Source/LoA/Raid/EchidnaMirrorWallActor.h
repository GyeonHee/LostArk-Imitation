#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CounterableInterface.h"
#include "EchidnaMirrorWallActor.generated.h"

class AEchidnaBoss;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UNiagaraSystem;

UENUM()
enum class EEchidnaMirrorWallPhase : uint8
{
	Idle,
	Advancing,  // 줄이 반대편으로 전진, 지나간 자리에 불길
	Falling,    // 카운터 성공 — 거울들이 쓰러지는 중 (불길은 남아 있음)
	Done        // 거울이 사라짐 — 불길도 같이 꺼짐
};

/**
 * 거울 카운터(217줄) 한 웨이브 — 맵 외곽에서 거울 MirrorCount(7)개가 일렬로 서서 반대편 외곽까지 전진한다.
 *
 * - 그중 하나(랜덤)가 **카운터 거울**: 청백색으로 빛나고, 정면에서 [카운터 가능] 스킬(돌풍)로 맞히면
 *   줄 전체가 뒤로 쓰러지며 멈춘다(ICounterable). 나머지 거울은 맞아도 아무 일 없음
 * - 거울이 **지나간 자리엔 불길**이 남아(줄 전체 폭 × 시작선~현재 줄) FireTickInterval마다 틱 데미지.
 *   불길은 거울과 수명이 같다 — 거울이 사라지는 순간(쓰러진 뒤 / 반대편 도착) 같이 꺼진다. 빨리 카운터칠수록 불길이 좁다
 * - 줄 폭은 Task가 아레나 한 변(타일 SideCount칸) 길이에 맞춰 넘겨준다 — 끝 거울이 벽 너머로 밀려 칠 수 없게 되는 걸 막기 위해
 * - 전진하는 줄에 닿으면 1회 데미지 + 넉다운(줄 진행 방향으로 밀림)
 *
 * 액터 위치 = 시작선 중심(지면), 액터 정면(+X) = 진행 방향. 거울 줄은 RowRoot를 +X로 밀어 옮기고,
 * 불길·판정은 전부 액터 로컬 좌표(X = 진행 거리, Y = 줄 좌우)로 계산한다.
 * 비주얼은 에셋 없이 엔진 Cylinder(얇게 세운 타원 거울) + M_MirrorLaser 색 주입.
 * 광폭화 규칙 준수 — BeginPlay에서 CustomTimeDilation, 진행은 전부 Tick 기반(월드 타이머 안 씀).
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaMirrorWallActor : public AActor, public ICounterable
{
	GENERATED_BODY()

public:
	AEchidnaMirrorWallActor();

	virtual void Tick(float DeltaTime) override;

	/** 스폰 직후 1회. TravelDistance = 시작선에서 반대편 끝까지, FireStartDistance = 불길이 시작되는 거리(맵 밖 구간 제외).
	 *  RowWidth > 0이면 양 끝 거울의 바깥 가장자리가 이 폭 안에 들어오도록 MirrorSpacing을 다시 계산한다 */
	void Activate(AEchidnaBoss* InBoss, float InTravelDistance, float InFireStartDistance, AController* InInstigator, float RowWidth = -1.f);

	// ICounterable — 카운터 거울을 정면에서 맞았을 때만 성공
	virtual bool TryCounterHit(AActor* Attacker, UPrimitiveComponent* HitComponent) override;

	bool IsCountered() const { return bCountered; }

	// 줄이 멈춤(카운터 성공 또는 반대편 도착) — 다음 웨이브 판단용
	bool HasStopped() const { return Phase == EEchidnaMirrorWallPhase::Falling || Phase == EEchidnaMirrorWallPhase::Done; }

	// 거울·불길 다 사라짐 — 패턴 종료 판단용
	bool IsFinished() const { return Phase == EEchidnaMirrorWallPhase::Done; }

	// ── 연출 전용 줄 (판정·불길·카운터 없음) — 거울 카운터 시네마틱의 줄, 패턴 중 양쪽 세로 거울 기둥 ──

	/** 판정 없는 장식 줄을 만든다. Count/Spacing이 0 이하면 기본값(MirrorCount, RowWidth에 맞춘 간격).
	 *  bHiddenUntilAppear면 PlayAppear 전까지 땅속에 숨겨둔다 */
	void SetupDisplayRow(float RowWidth, int32 Count = -1, float Spacing = -1.f, bool bHiddenUntilAppear = true, bool bGlass = false);

	/** 거울이 하나씩(인덱스 순서, Stagger 간격) 땅에서 돌면서 솟아오른다 */
	void PlayAppear(float Stagger, float RiseDuration, float SpinTurns);

	/** 정면(+X)에서 볼 때 오른쪽(로컬 -Y)부터 하나씩 오른쪽으로 미끄러지며 작아져 사라진다 */
	void PlayExit(float Stagger, float SlideDuration, float SlideDistance);

	// 등장/퇴장 애니메이션이 아직 진행 중인지
	bool IsDisplayAnimating() const { return DisplayAnim != EDisplayAnim::None; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MirrorWall")
	TObjectPtr<USceneComponent> RowRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MirrorWall")
	TObjectPtr<UProceduralMeshComponent> FireMesh;

	// ── 모양 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Shape", meta = (ClampMin = "1"))
	int32 MirrorCount = 7;

	// 거울 중심 간 좌우 간격 (cm) — Task가 RowWidth를 넘기면(기본) 그 폭에 맞춰 덮어쓴다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Shape")
	float MirrorSpacing = 300.f;

	// 레퍼런스(로아 거울 카운터)처럼 거울끼리 거의 맞닿는 크기 — 변 길이 약 1920cm에 7개면 간격 약 277이라 폭 260이면 틈 약 17cm.
	// 키는 폭의 약 1.5배(세로로 긴 타원)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Shape")
	float MirrorHeight = 380.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Shape")
	float MirrorWidth = 260.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Shape")
	float MirrorThickness = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Shape")
	TObjectPtr<UStaticMesh> MirrorMesh;

	// ── 이동 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Timing")
	float MoveSpeed = 350.f;

	// 카운터 성공 후 거울이 완전히 눕기까지 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Timing")
	float FallDuration = 0.6f;

	// 쓰러진 거울이 사라지기까지 누워 있는 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Timing")
	float FallenLingerDuration = 1.5f;

	// ── 거울 몸통 판정 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Hit")
	float MirrorHitDamage = 10000.f;

	// 줄 앞뒤로 이 두께 안에 들어오면 거울에 닿은 것 (cm, 캡슐 반지름 별도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Hit")
	float MirrorHitThickness = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Hit")
	bool bKnockdownOnMirrorHit = true;

	/** 카운터 정면 판정 반각(도) — 카운터 거울 정면(+X)과 "거울→시전자" 사이 각도. 보스 HeadAttackHalfAngle과 같은 의미 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Hit", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float CounterHalfAngle = 60.f;

	// ── 불길 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Fire")
	float FireTickInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Fire")
	float FireTickDamage = 3000.f;

	// 불길 연출 — 부채꼴 패턴과 같은 Niagara(기본 NS_EchidnaRetreatFan, 1초짜리)를 FireVFXRefreshInterval마다 불길 영역 전체에
	// 다시 깔아서 계속 타오르게 한다. 거울이 쓰러지거나 사라지면(또는 패턴이 끝나면) 더 안 깔아서 꺼진다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Fire")
	TObjectPtr<UNiagaraSystem> FireVFX;

	// 이펙트 수명(약 1초)보다 짧아야 끊기지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Fire", meta = (ClampMin = "0.1"))
	float FireVFXRefreshInterval = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Fire", meta = (ClampMin = "30.0"))
	float FireVFXSpacing = 320.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Fire", meta = (ClampMin = "0.05"))
	float FireVFXScale = 0.6f;

	// 한 번 깔 때 최대 개수 — 넘으면 간격을 넓힘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Fire", meta = (ClampMin = "1"))
	int32 FireVFXMaxCount = 50;

	// 예전 불길 장판 메시(FireColor)도 같이 보여줄지 — FireVFX가 비면 자동으로 메시
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Fire")
	bool bShowFireMesh = false;

	// ── 비주얼 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Visual")
	FLinearColor NormalMirrorColor = FLinearColor(0.85f, 0.75f, 0.95f, 0.95f);

	// HDR(1 초과) — 블룸으로 청백색 발광
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Visual")
	FLinearColor CounterMirrorColor = FLinearColor(1.5f, 3.5f, 9.f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Visual")
	FLinearColor FireColor = FLinearColor(6.f, 1.4f, 0.25f, 0.5f);

	// 상반신 없는 투명 유리 거울 색 (SetupDisplayRow의 bGlass — 패턴 중 양쪽 세로 기둥). Alpha = 불투명도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Visual")
	FLinearColor GlassMirrorColor = FLinearColor(0.75f, 0.8f, 1.f, 0.22f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Visual")
	FName ColorParameterName = TEXT("Base Color");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Visual")
	TObjectPtr<UMaterialInterface> BaseMaterial;

	// ── 거울 속 에키드나 상반신 ──
	/** 초상화 머티리얼 — 기본 M_EchidnaMirrorPortrait(Unlit, Texture Parameter "Portrait" × Vector Parameter "Tint" → Emissive) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Portrait")
	TObjectPtr<UMaterialInterface> PortraitMaterial;

	// 일반 거울 — fx_l_mirror_sden_01_cl(오른쪽을 봄)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Portrait")
	TObjectPtr<UTexture2D> NormalPortraitTexture;

	// 카운터 거울 — fx_l_mirror_sden_02_cl(01의 좌우 반전, 반대 방향을 봄)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Portrait")
	TObjectPtr<UTexture2D> CounterPortraitTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Portrait")
	FLinearColor NormalPortraitTint = FLinearColor(1.f, 1.f, 1.f, 1.f);

	// 카운터 거울은 초상화도 청백색으로 빛나게 (HDR)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Portrait")
	FLinearColor CounterPortraitTint = FLinearColor(0.8f, 1.4f, 3.f, 1.f);

	// 거울 테두리 안쪽으로 얼마나 들어와서 그릴지 (1 = 거울 크기 그대로)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Portrait", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float PortraitInset = 0.88f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Portrait")
	FName PortraitTextureParameterName = TEXT("Portrait");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorWall|Portrait")
	FName PortraitTintParameterName = TEXT("Tint");

protected:
	virtual void BeginPlay() override;

private:
	EEchidnaMirrorWallPhase Phase = EEchidnaMirrorWallPhase::Idle;

	enum class EDisplayAnim : uint8 { None, Appear, Exit };
	EDisplayAnim DisplayAnim = EDisplayAnim::None;
	float DisplayElapsed = 0.f;
	float DisplayStagger = 0.f;
	float DisplayDuration = 0.f;
	float DisplaySpinTurns = 0.f;
	float DisplaySlideDistance = 0.f;
	void TickDisplayAnim(float DeltaTime);

	float FireVFXElapsed = 0.f;

	// true면 BuildMirrors가 초상화 없이 GlassMirrorColor로 만든다
	bool bGlassMirrors = false;
	bool ShouldShowFireMesh() const { return bShowFireMesh || !FireVFX; }

	TWeakObjectPtr<AEchidnaBoss> Boss;
	TWeakObjectPtr<AController> InstigatorController;

	float TravelDistance = 0.f;
	float FireStartDistance = 0.f;

	// 시작선에서 줄까지 진행 거리 (액터 로컬 X)
	float RowOffset = 0.f;

	float PhaseElapsed = 0.f;
	float FireTickElapsed = 0.f;

	bool bCountered = false;
	int32 CounterMirrorIndex = 0;

	// 거울마다: 쓰러질 때 도는 받침(지면 높이) + 실제 메시
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> MirrorPivots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> MirrorMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> PortraitMeshes;

	// 거울 앞면에 붙는 타원 초상화 (받침 로컬 좌표)
	UProceduralMeshComponent* CreatePortrait(USceneComponent* Pivot, bool bCounterMirror);

	// 거울 몸통에 이미 맞은 캐릭터 (웨이브당 1회)
	TSet<TWeakObjectPtr<AActor>> MirrorHitActors;

	float GetRowHalfWidth() const;
	float GetMirrorLocalY(int32 Index) const;

	void BuildMirrors();
	void TickAdvancing(float DeltaTime);
	void TickFire(float DeltaTime);
	void UpdateFireMesh();
	void StopRow();

	// 거울 숨김 + 불길 끔 → Done
	void VanishRow();
};
