
// The Cleric's Serpent Staff -----------------------------------------------

class CWeapStaff : ClericWeapon
{
	Default
	{
		Weapon.SelectionOrder 1600;
		Weapon.AmmoUse1 1;
		Weapon.AmmoGive1 25;
		Weapon.KickBack 150;
		Weapon.YAdjust 10;
		Weapon.AmmoType1 "Mana1";
		Inventory.PickupMessage "$TXT_WEAPON_C2";
		Obituary "$OB_MPCWEAPSTAFFM";
		Tag "$TAG_CWEAPSTAFF";
	}

	States
	{
	Spawn:
		WCSS A -1;
		Stop;
	Select:
		CSSF C 1 A_Raise;
		Loop;
	Deselect:
		CSSF B 3;
		CSSF C 4;
		CSSF C 1 A_Lower;
		Wait;
	Ready:
		CSSF C 4;
		CSSF B 3 A_CStaffInitBlink;
		CSSF AAAAAAA 1 A_WeaponReady;
		CSSF A 1 A_CStaffCheckBlink;
		Goto Ready + 2;
	Fire:
		CSSF A 1 Offset (0, 45) A_CStaffCheck;
		CSSF J 1 Offset (0, 50) A_CStaffAttack;
		CSSF J 2 Offset (0, 50);
		CSSF J 2 Offset (0, 45);
		CSSF A 2 Offset (0, 40);
		CSSF A 2 Offset (0, 36);
		Goto Ready + 2;
	Blink:
		CSSF BBBCCCCCBBB 1 A_WeaponReady;
		Goto Ready + 2;
	Drain:
		CSSF K 10 Offset (0, 36);
		Goto Ready + 2;
	}
	
	//============================================================================
	//
	// A_CStaffCheck
	//
	//============================================================================

	action void A_CStaffCheck()
	{
		FTranslatedLineTarget t;

		if (player == null)
		{
			return;
		}
		Weapon weapon = player.ReadyWeapon;

		int damage = random[StaffCheck](20, 35);
		int max = player.mo.GetMaxHealth();
		for (int i = 0; i < 3; i++)
		{
			for (int j = 1; j >= -1; j -= 2)
			{
				double ang = angle + j*i*(45. / 16);
				double slope = AimLineAttack(ang, 1.5 * DEFMELEERANGE, t, 0., ALF_CHECK3D);
				if (t.linetarget)
				{
					LineAttack(ang, 1.5 * DEFMELEERANGE, slope, damage, 'Melee', "CStaffPuff", false, t);
					if (t.linetarget != null)
					{
						angle = t.angleFromSource;
						if (((t.linetarget.player && (!t.linetarget.IsTeammate(self) || level.teamdamage != 0)) || t.linetarget.bIsMonster)
							&& (!t.linetarget.bDormant && !t.linetarget.bInvulnerable))
						{
							int newLife = player.health + (damage >> 3);
							newLife = newLife > max ? max : newLife;
							if (newLife > player.health)
							{
								health = player.health = newLife;
							}
							if (weapon != null)
							{
								State newstate = weapon.FindState("Drain");
								if (newstate != null) player.SetPsprite(PSP_WEAPON, newstate);
							}
						}
						if (weapon != null)
						{
							weapon.DepleteAmmo(weapon.bAltFire, false);
						}
					}
					return;
				}
			}
		}
	}

	//============================================================================
	//
	// A_CStaffAttack
	//
	//============================================================================

	action void A_CStaffAttack()
	{
		if (player == null)
		{
			return;
		}

		Weapon weapon = player.ReadyWeapon;
		if (weapon != null)
		{
			if (!weapon.DepleteAmmo (weapon.bAltFire))
				return;
		}
		Actor mo = SpawnPlayerMissile ("CStaffMissile", angle - 3.0);
		if (mo)
		{
			mo.WeaveIndexXY = 32;
		}
		mo = SpawnPlayerMissile ("CStaffMissile", angle + 3.0);
		if (mo)
		{
			mo.WeaveIndexXY = 0;
		}
		A_StartSound ("ClericCStaffFire", CHAN_WEAPON);
	}

	//============================================================================
	//
	// A_CStaffInitBlink
	//
	//============================================================================

	action void A_CStaffInitBlink()
	{
		weaponspecial = (random[CStaffBlink]() >> 1) + 20;
	}

	//============================================================================
	//
	// A_CStaffCheckBlink
	//
	//============================================================================

	action void A_CStaffCheckBlink()
	{
		if (player && player.ReadyWeapon)
		{
			if (!--weaponspecial)
			{
				player.SetPsprite(PSP_WEAPON, player.ReadyWeapon.FindState ("Blink"));
				weaponspecial = (random[CStaffBlink]() + 50) >> 2;
			}
			else 
			{
				A_WeaponReady();
			}
		}
	}
}

// Serpent Staff Missile ----------------------------------------------------

class CStaffMissile : Actor
{
	Default
	{
		Speed 22;
		Radius 12;
		Height 10;
		Damage 5;
		RenderStyle "Add";
		Projectile;
		DeathSound "ClericCStaffExplode";
		Obituary "$OB_MPCWEAPSTAFFR";
	}
	States
	{
	Spawn:
		CSSF DDEE 1 Bright A_CStaffMissileSlither;
		Loop;
	Death:
		CSSF FG 4 Bright;
		CSSF HI 3 Bright;
		Stop;
	}
	
	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		// Cleric Serpent Staff does poison damage
		if (target.player)
		{
			target.player.PoisonPlayer (self, self.target, 20);
			damage >>= 1;
		}
		return damage;
	}
	
}

extend class Actor
{

	//============================================================================
	//
	// A_CStaffMissileSlither
	//
	//============================================================================

	void A_CStaffMissileSlither()
	{
		A_Weave(3, 0, 1., 0.);
	}

}

// Serpent Staff Puff -------------------------------------------------------

class CStaffPuff : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY
		+PUFFONACTORS
		RenderStyle "Translucent";
		Alpha 0.6;
		SeeSound "ClericCStaffHitThing";
	}
	States
	{
	Spawn:
		FHFX STUVW 4;
		Stop;
	}
}
