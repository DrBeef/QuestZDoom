//===========================================================================
//
// Pink Demon
//
//===========================================================================
class Demon : Actor
{
	Default
	{
		Health 150;
		PainChance 180;
		Speed 10;
		Radius 30;
		Height 56;
		Mass 400;
		Monster;
		+FLOORCLIP
		SeeSound "demon/sight";
		AttackSound "demon/melee";
		PainSound "demon/pain";
		DeathSound "demon/death";
		ActiveSound "demon/active";
		Obituary "$OB_DEMONHIT";
		Tag "$FN_DEMON";
	}
	States
	{
	Spawn:
		SARG AB 10 A_Look;
		Loop;
	See:
		SARG AABBCCDD 2 Fast A_Chase;
		Loop;
	Melee:
		SARG EF 8 Fast A_FaceTarget;
		SARG G 8 Fast A_SargAttack;
		Goto See;
	Pain:
		SARG H 2 Fast;
		SARG H 2 Fast A_Pain;
		Goto See;
	Death:
		SARG I 8;
		SARG J 8 A_Scream;
		SARG K 4;
		SARG L 4 A_NoBlocking;
		SARG M 4;
		SARG N -1;
		Stop;
	Raise:
		SARG N 5;
		SARG MLKJI 5;
		Goto See;
	}
}

//===========================================================================
//
// Spectre
//
//===========================================================================
class Spectre : Demon
{
	Default
	{
		+SHADOW
		RenderStyle "OptFuzzy";
		Alpha 0.5;
		SeeSound "spectre/sight";
		AttackSound "spectre/melee";
		PainSound "spectre/pain";
		DeathSound "spectre/death";
		ActiveSound "spectre/active";
		Obituary "$OB_SPECTREHIT";
		Tag "$FN_SPECTRE";
	}
}


//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

extend class Actor
{
	void A_SargAttack()
	{
		let targ = target;
		if (targ && CheckMeleeRange())
		{
			int damage = random[pr_sargattack](1, 10) * 4;
			int newdam = targ.DamageMobj (self, self, damage, "Melee");
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
		}
	}
}
