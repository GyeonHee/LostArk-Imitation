#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LoADamageLibrary.generated.h"

/**
 * 플레이어가 주는 데미지 계산을 한 곳에 모은다.
 *  - 시전 시점: CalcOutgoingDamage — 공격력 × 계수 × 피해 버킷 (스킬 액터를 스폰할 때 1번)
 *  - 피격 시점: ResolveIncomingHit — 치명·제압. ApplyRadialDamage가 범위 안 모두에 같은 값을 넘기므로
 *    타수마다 따로 굴려야 하는 것들은 맞는 쪽(보스 TakeDamage)에서 판정한다
 * 백/헤드어택 보너스는 없다 — 로아에서도 [백어택]/[헤드어택] 태그가 붙은 스킬만 받는데, 소서리스는 타대 클래스라
 * 그런 스킬이 없다. 방향 클래스를 추가하면 FSkillData에 방향 태그를 두고 여기서 판정할 것
 * 스탯은 공격자의 UStatComponent에서 읽는다. 컴포넌트가 없으면 예전 스킬 코드의 폴백(공격력 100)을 그대로 쓴다.
 */
UCLASS()
class LOA_API ULoADamageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 시전 시점 데미지 = 공격력 × 계수 × 특화(캐스팅·차지 스킬만) × (1 + 합연산) × 곱연산.
	 * 특화 대상은 SkillRowName으로 공격자의 DT_Skills 행을 찾아 입력 타입으로 판단한다.
	 * @param Attacker      스탯을 가진 액터(캐릭터). 컨트롤러를 넘기면 그 폰을 쓴다
	 * @param Coefficient   DT_Skills의 DamageCoefficient (블레이즈 화상처럼 따로 정한 배율도 가능)
	 * @param SkillRowName  어느 스킬인지 — 로그용, 이후 스킬별 보석·특화 판정에 쓴다
	 */
	UFUNCTION(BlueprintCallable, Category="Damage", meta=(DefaultToSelf="Attacker"))
	static float CalcOutgoingDamage(const AActor* Attacker, float Coefficient, FName SkillRowName);

	/**
	 * 피격 시점 보정 — 맞을 때마다 치명을 새로 굴리고, 대상이 그로기면 제압 배율을 곱한다.
	 * 공격자 스탯이 없으면(패턴 등 플레이어가 아닌 데미지) 그대로 돌려준다.
	 * @param Instigator     ApplyDamage의 EventInstigator(컨트롤러) 또는 공격자 액터
	 * @param bTargetGroggy  맞는 대상이 무력화(그로기) 상태인지
	 * @param bOutCritical   치명타가 터졌는지 — 데미지 숫자 표시에 쓴다
	 */
	UFUNCTION(BlueprintCallable, Category="Damage")
	static float ResolveIncomingHit(float BaseDamage, const AActor* Instigator, bool bTargetGroggy, bool& bOutCritical, float TargetHPRatio = 1.f);

	/**
	 * 스킬이 스폰한 데미지 액터(메테오·장판·발사체 등)에 어느 스킬인지 표시해 둔다.
	 * 피격 쪽은 DamageCauser에서 이 표시를 읽어 데미지 미터에 스킬별로 집계한다. 액터 Tags에 "LoASkill.<행 이름>"으로 넣는다
	 */
	UFUNCTION(BlueprintCallable, Category="Damage")
	static void TagDamageSource(AActor* DamageSource, FName SkillRowName);

	/** TagDamageSource로 붙인 스킬 행 이름 (없으면 NAME_None) */
	static FName GetDamageSourceSkill(const AActor* DamageSource);

	/**
	 * 피격 결과를 공격한 플레이어의 데미지 미터에 기록한다 — 보스·허수아비의 TakeDamage가 ResolveIncomingHit 다음에 부른다.
	 * @param bTrainingTarget  허수아비면 true — 미터가 5초 무피격 시 세션을 끝낸다(보스는 처치·실패 때까지 한 세션)
	 */
	static void ReportDamageDealt(const AActor* Instigator, const AActor* Target, const AActor* DamageCauser,
		float FinalDamage, bool bCritical, bool bTrainingTarget);

	/**
	 * 아이덴티티 게이지 — 보스·허수아비 TakeDamage가 치명 전 피해(DamageAmount)로 부른다.
	 * 이 타격의 계수 = 피해 ÷ (그 스킬 계수 1일 때 피해)를 공격자의 아이덴티티 컴포넌트에 넘긴다(소서리스만)
	 */
	static void NotifyIdentityHit(const AActor* Instigator, const AActor* DamageCauser, float BaseDamage);
};
