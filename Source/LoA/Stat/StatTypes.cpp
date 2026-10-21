#include "Stat/StatTypes.h"

int32 FCombatStatPoints::Get(ELoACombatStat Stat) const
{
	switch (Stat)
	{
	case ELoACombatStat::Crit:           return Crit;
	case ELoACombatStat::Specialization: return Specialization;
	case ELoACombatStat::Swiftness:      return Swiftness;
	case ELoACombatStat::Domination:     return Domination;
	case ELoACombatStat::Endurance:      return Endurance;
	case ELoACombatStat::Expertise:      return Expertise;
	default:                             return 0;
	}
}

void FCombatStatPoints::Add(ELoACombatStat Stat, int32 Amount)
{
	switch (Stat)
	{
	case ELoACombatStat::Crit:           Crit += Amount; break;
	case ELoACombatStat::Specialization: Specialization += Amount; break;
	case ELoACombatStat::Swiftness:      Swiftness += Amount; break;
	case ELoACombatStat::Domination:     Domination += Amount; break;
	case ELoACombatStat::Endurance:      Endurance += Amount; break;
	case ELoACombatStat::Expertise:      Expertise += Amount; break;
	default: break;
	}
}
