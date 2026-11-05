// Fill out your copyright notice in the Description page of Project Settings.


#include "Skill/SkillInstant.h"
#include "Engine/Engine.h"

void USkillInstant::OnKeyDown(AActor* Owner)
{
	if (!IsValid(Owner)) return;
	// 애니메이션을 처음부터 틀고 효과는 발사 순간에(SkillAnim_Table ExecuteDelay)
	if (BeginSkillExecute(Owner, nullptr))
	{
		SpawnReleaseVFX(Owner);
		Execute(Owner);
	}
}
