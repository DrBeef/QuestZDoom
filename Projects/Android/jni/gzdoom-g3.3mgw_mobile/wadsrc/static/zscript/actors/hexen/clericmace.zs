
// The Cleric's Mace --------------------------------------------------------

class CWeapMace : ClericWeapon
{
	Default
	{
		Weapon.SelectionOrder 3500;
		Weapon.KickBack 150;
		Weapon.YAdjust -8;
		+BLOODSPLATTER
		Obituary "$OB_MPCWEAPMACE";
		Tag "$TAG_CWEAPMACE";
	}

	States
	{
	Select:
		CMCE A 1 A_Raise;
		Loop;
	Deselect:
		CMCE A 1 A_Lower;
		Loop;
	Ready:
		CMCE A 1 A_WeaponReady;
		Loop;
	Fire:
		CMCE B 2 Offset (60, 20);
		CMCE B 1 Offset (30, 33);
		CMCE B 2 Offset (8, 45);
		CMCE C 1 Offset (8, 45);
		CMCE D 1 Offset (8, 45);
		CMCE E 1 Offset (8, 45);
		CMCE E 1 Offset (-11, 58) A_CMaceAttack;
		CMCE F 1 Offset (8, 45);
		CMCE F 2 Offset (-8, 74);
		CMCE F 1 Offset (-20, 96);
		CMCE F 8 Offset (-33, 160);
		CMCE A 2 Offset (8, 75) A_ReFire;
		CMCE A 1 Offset (8, 65);
		CMCE A 2 Offset (8, 60);
		CMCE A 1 Offset (8, 55);
		CMCE A 2 Offset (8, 50);
		CMCE A 1 Offset (8, 45);
		Goto Ready;
	}
	
	//===========================================================================
	//
	// A_CMaceAttack
	//
	//===========================================================================

	action void A_CMaceAttack()
	{
		FTranslatedLineTarget t;

		if (player == null)
		{
			return;
		}

		int damage = random[MaceAtk](25, 40);
		for (int i = 0; i < 16; i++)
		{
			for (int j = 1; j >= -1; j -= 2)
			{
				double ang = angle + j*i*(45. / 16);
				double slope = AimLineAttack(ang, 2 * DEFMELEERANGE, t, 0., ALF_CHECK3D);
				if (t.linetarget)
				{
					LineAttack(ang, 2 * DEFMELEERANGE, slope, damage, 'Melee', "HammerPuff", true, t);
					if (t.linetarget != null)
					{
						AdjustPlayerAngle(t);
						return;
					}
				}
			}
		}
		// didn't find any creatures, so try to strike any walls
		weaponspecial = 0;

		double slope = AimLineAttack (angle, DEFMELEERANGE, null, 0., ALF_CHECK3D);
		LineAttack (angle, DEFMELEERANGE, slope, damage, 'Melee', "HammerPuff");
	}
}
