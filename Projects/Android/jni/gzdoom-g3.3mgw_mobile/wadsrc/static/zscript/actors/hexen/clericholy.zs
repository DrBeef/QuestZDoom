
// Cleric Weapon Piece ------------------------------------------------------

class ClericWeaponPiece : WeaponPiece
{
	Default
	{
		Inventory.PickupSound "misc/w_pkup";
		Inventory.PickupMessage "$TXT_WRAITHVERGE_PIECE";
		Inventory.ForbiddenTo "FighterPlayer", "MagePlayer";
		WeaponPiece.Weapon "CWeapWraithverge";
		+FLOATBOB
	}
}

// Cleric Weapon Piece 1 ----------------------------------------------------

class CWeaponPiece1 : ClericWeaponPiece
{
	Default
	{
		WeaponPiece.Number 1;
	}
	States
	{
	Spawn:
		WCH1 A -1;
		Stop;
	}
}

// Cleric Weapon Piece 2 ----------------------------------------------------

class CWeaponPiece2 : ClericWeaponPiece
{
	Default
	{
		WeaponPiece.Number 2;
	}
	States
	{
	Spawn:
		WCH2 A -1;
		Stop;
	}
}

// Cleric Weapon Piece 3 ----------------------------------------------------

class CWeaponPiece3 : ClericWeaponPiece
{
	Default
	{
		WeaponPiece.Number 3;
	}
	States
	{
	Spawn:
		WCH3 A -1;
		Stop;
	}
}

// Wraithverge Drop ---------------------------------------------------------

class WraithvergeDrop : Actor
{
	States
	{
	Spawn:
		TNT1 A 1;
		TNT1 A 1 A_DropWeaponPieces("CWeaponPiece1", "CWeaponPiece2", "CWeaponPiece3");
		Stop;
	}
}

// Cleric's Wraithverge (Holy Symbol?) --------------------------------------

class CWeapWraithverge : ClericWeapon
{
	int CHolyCount;
	
	Default
	{
		Health 3;
		Weapon.SelectionOrder 3000;
		+WEAPON.PRIMARY_USES_BOTH;
		+Inventory.NoAttenPickupSound
		Weapon.AmmoUse1 18;
		Weapon.AmmoUse2 18;
		Weapon.AmmoGive1 20;
		Weapon.AmmoGive2 20;
		Weapon.KickBack 150;
		Weapon.AmmoType1 "Mana1";
		Weapon.AmmoType2 "Mana2";
		Inventory.PickupMessage "$TXT_WEAPON_C4";
		Tag "$TAG_CWEAPWRAITHVERGE";
		Inventory.PickupSound "WeaponBuild";
	}


	States
	{
	Spawn:
		TNT1 A -1;
		Stop;
	Ready:
		CHLY A 1 A_WeaponReady;
		Loop;
	Select:
		CHLY A 1 A_Raise;
		Loop;
	Deselect:
		CHLY A 1 A_Lower;
		Loop;
	Fire:
		CHLY AB 1 Bright Offset (0, 40);
		CHLY CD 2 Bright Offset (0, 43);
		CHLY E 2 Bright Offset (0, 45);
		CHLY F 6 Bright Offset (0, 48) A_CHolyAttack;
		CHLY GG 2 Bright Offset (0, 40) A_CHolyPalette;
		CHLY G 2 Offset (0, 36) A_CHolyPalette;
		Goto Ready;
	}

	override color GetBlend ()
	{
		if (paletteflash & PF_HEXENWEAPONS)
		{
			if (CHolyCount == 3)
				return Color(128, 70, 70, 70);
			else if (CHolyCount == 2)
				return Color(128, 100, 100, 100);
			else if (CHolyCount == 1)
				return Color(128, 130, 130, 130);
			else
				return Color(0, 0, 0, 0);
		}
		else
		{
			return Color(CHolyCount * 128 / 3, 131, 131, 131);
		}
	}

	//============================================================================
	//
	// A_CHolyAttack
	//
	//============================================================================

	action void A_CHolyAttack()
	{
		FTranslatedLineTarget t;

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
		Actor missile = SpawnPlayerMissile ("HolyMissile", angle, pLineTarget:t);
		if (missile != null && !t.unlinked)
		{
			missile.tracer = t.linetarget;
		}

		invoker.CHolyCount = 3;
		A_StartSound ("HolySymbolFire", CHAN_WEAPON);
	}

	//============================================================================
	//
	// A_CHolyPalette
	//
	//============================================================================

	action void A_CHolyPalette()
	{
		if (invoker.CHolyCount > 0) invoker.CHolyCount--;
	}
}

// Holy Missile -------------------------------------------------------------

class HolyMissile : Actor
{
	Default
	{
		Speed 30;
		Radius 15;
		Height 8;
		Damage 4;
		Projectile;
		-ACTIVATEIMPACT -ACTIVATEPCROSS
		+EXTREMEDEATH
	}

	States
	{
	Spawn:
		SPIR PPPP 3 Bright A_SpawnItemEx("HolyMissilePuff");
	Death:
		SPIR P 1 Bright A_CHolyAttack2;
		Stop;
	}
	
	//============================================================================
	//
	// A_CHolyAttack2 
	//
	// 	Spawns the spirits
	//============================================================================

	void A_CHolyAttack2()
	{
		for (int j = 0; j < 4; j++)
		{
			Actor mo = Spawn("HolySpirit", Pos, ALLOW_REPLACE);
			if (!mo)
			{
				continue;
			}
			switch (j)
			{ // float bob index

				case 0:
					mo.WeaveIndexZ = random[HolyAtk2](0, 7); // upper-left
					break;
				case 1:
					mo.WeaveIndexZ = random[HolyAtk2](32, 39); // upper-right
					break;
				case 2:
					mo.WeaveIndexXY = random[HolyAtk2](32, 39); // lower-left
					break;
				case 3:
					mo.WeaveIndexXY = random[HolyAtk2](32, 39);
					mo.WeaveIndexZ = random[HolyAtk2](32, 39);
					break;
			}
			mo.SetZ(pos.z);
			mo.angle = angle + 67.5 - 45.*j;
			mo.Thrust();
			mo.target = target;
			mo.args[0] = 10; // initial turn value
			mo.args[1] = 0; // initial look angle
			if (deathmatch)
			{ // Ghosts last slightly less longer in DeathMatch
				mo.health = 85;
			}
			if (tracer)
			{
				mo.tracer = tracer;
				mo.bNoClip = true;
				mo.bSkullFly = true;
				mo.bMissile = false;
			}
			HolyTail.SpawnSpiritTail (mo);
		}
	}
}

// Holy Missile Puff --------------------------------------------------------

class HolyMissilePuff : Actor
{
	Default
	{
		Radius 4;
		Height 8;
		+NOBLOCKMAP +NOGRAVITY +DROPOFF
		+NOTELEPORT
		RenderStyle "Translucent";
		Alpha 0.4;
	}
	States
	{
	Spawn:
		SPIR QRSTU 3;
		Stop;
	}
}

// Holy Puff ----------------------------------------------------------------

class HolyPuff : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY
		RenderStyle "Translucent";
		Alpha 0.6;
	}
	States
	{
	Spawn:
		SPIR KLMNO 3;
		Stop;
	}
}

// Holy Spirit --------------------------------------------------------------

class HolySpirit : Actor
{
	Default
	{
		Health 105;
		Speed 12;
		Radius 10;
		Height 6;
		Damage 3;
		Projectile;
		+RIPPER +SEEKERMISSILE
		+FOILINVUL +SKYEXPLODE +NOEXPLODEFLOOR +CANBLAST
		+EXTREMEDEATH +NOSHIELDREFLECT
		RenderStyle "Translucent";
		Alpha 0.4;
		DeathSound "SpiritDie";
		Obituary "$OB_MPCWEAPWRAITHVERGE";
	}

	States
	{
	Spawn:
		SPIR AAB 2 A_CHolySeek;
		SPIR B 2 A_CHolyCheckScream;
		Loop;
	Death:
		SPIR D 4;
		SPIR E 4 A_Scream;
		SPIR FGHI 4;
		Stop;
	}
	
	//============================================================================
	//
	//
	//
	//============================================================================

	override bool Slam(Actor thing)
	{
		if (thing.bShootable && thing != target)
		{
			if (multiplayer && !deathmatch && thing.player && target.player)
			{ // don't attack other co-op players
				return true;
			}
			if (thing.bReflective && (thing.player || thing.bBoss))
			{
				tracer = target;
				target = thing;
				return true;
			}
			if (thing.bIsMonster || thing.player)
			{
				tracer = thing;
			}
			if (random[SpiritSlam]() < 96)
			{
				int dam = 12;
				if (thing.player || thing.bBoss)
				{
					dam = 3;
					// ghost burns out faster when attacking players/bosses
					health -= 6;
				}
				thing.DamageMobj(self, target, dam, 'Melee');
				if (random[SpiritSlam]() < 128)
				{
					Spawn("HolyPuff", Pos, ALLOW_REPLACE);
					A_StartSound("SpiritAttack", CHAN_WEAPON);
					if (thing.bIsMonster && random[SpiritSlam]() < 128)
					{
						thing.Howl();
					}
				}
			}
			if (thing.health <= 0)
			{
				tracer = null;
			}
		}
		return true;
	}

	override bool SpecialBlastHandling (Actor source, double strength)
	{
		if (tracer == source)
		{
			tracer = target;
			target = source;
		}
		return true;
	}

	//============================================================================
	//
	// CHolyFindTarget
	//
	//============================================================================

	private void CHolyFindTarget ()
	{
		Actor target;

		if ( (target = RoughMonsterSearch (6, true)) )
		{
			tracer = target;
			bNoClip = true;
			bSkullFly = true;
			bMissile = false;
		}
	}

	//============================================================================
	//
	// CHolySeekerMissile
	//
	// 	 Similar to P_SeekerMissile, but seeks to a random Z on the target
	//============================================================================

	private void CHolySeekerMissile (double thresh, double turnMax)
	{
		Actor target = tracer;
		if (target == NULL)
		{
			return;
		}
		if (!target.bShootable || (!target.bIsMonster && !target.player))
		{ // Target died/target isn't a player or creature
			tracer = null;
			bNoClip = false;
			bSkullFly = false;
			bMissile = true;
			CHolyFindTarget();
			return;
		}
		double ang = deltaangle(angle, AngleTo(target));
		double delta = abs(ang);
		
		if (delta > thresh)
		{
			delta /= 2;
			if (delta > turnMax)
			{
				delta = turnMax;
			}
		}
		if (ang > 0)
		{ // Turn clockwise
			angle += delta;
		}
		else
		{ // Turn counter clockwise
			angle -= delta;
		}
		VelFromAngle();

		if (!(Level.maptime&15) 
			|| pos.z > target.pos.z + target.height
			|| pos.z + height < target.pos.z)
		{
			double newZ = target.pos.z + ((random[HolySeeker]()*target.Height) / 256.);
			double deltaZ = newZ - pos.z;
			if (abs(deltaZ) > 15)
			{
				if (deltaZ > 0)
				{
					deltaZ = 15;
				}
				else
				{
					deltaZ = -15;
				}
			}
			Vel.Z = deltaZ / DistanceBySpeed(target, Speed);
		}
	}

	//============================================================================
	//
	// A_CHolySeek
	//
	//============================================================================

	void A_CHolySeek()
	{
		health--;
		if (health <= 0)
		{
			Vel.X /= 4;
			Vel.Y /= 4;
			Vel.Z = 0;
			SetStateLabel ("Death");
			tics -= random[HolySeeker](0, 3);
			return;
		}
		if (tracer)
		{
			CHolySeekerMissile (args[0], args[0]*2.);
			if (!((Level.maptime+7)&15))
			{
				args[0] = 5+(random[HolySeeker]()/20);
			}
		}

		int xyspeed = random[HolySeeker](0, 4);
		int zspeed = random[HolySeeker](0, 4);
		A_Weave(xyspeed, zspeed, 4., 2.);
	}

	//============================================================================
	//
	// A_CHolyCheckScream
	//
	//============================================================================

	void A_CHolyCheckScream()
	{
		A_CHolySeek();
		if (random[HolyScream]() < 20)
		{
			A_StartSound ("SpiritActive", CHAN_VOICE);
		}
		if (!tracer)
		{
			CHolyFindTarget();
		}
	}
}

// Holy Tail ----------------------------------------------------------------

class HolyTail : Actor
{
	Default
	{
		Radius 1;
		Height 1;
		+NOBLOCKMAP +NOGRAVITY +DROPOFF +NOCLIP
		+NOTELEPORT
		RenderStyle "Translucent";
		Alpha 0.6;
	}

	States
	{
	Spawn:
		SPIR C 1 A_CHolyTail;
		Loop;
	TailTrail:
		SPIR D -1;
		Stop;
	}
	
	//============================================================================
	//
	// SpawnSpiritTail
	//
	//============================================================================

	static void SpawnSpiritTail (Actor spirit)
	{
		Actor tail = Spawn ("HolyTail", spirit.Pos, ALLOW_REPLACE);
		if (tail != null)
		{
			tail.target = spirit; // parent
			for (int i = 1; i < 3; i++)
			{
				Actor next = Spawn ("HolyTailTrail", spirit.Pos, ALLOW_REPLACE);
				tail.tracer = next;
				tail = next;
			}
			tail.tracer = null; // last tail bit
		}
	}

	//============================================================================
	//
	// CHolyTailFollow
	//
	//============================================================================

	private void CHolyTailFollow(double dist)
	{
		Actor mo = self;
		while (mo)
		{
			Actor child = mo.tracer;
			if (child)
			{
				double an = mo.AngleTo(child);
				double oldDistance = child.Distance2D(mo);
				if (child.TryMove(mo.Pos.XY + AngleToVector(an, dist), true))
				{
					double newDistance = child.Distance2D(mo) - 1;
					if (oldDistance < 1)
					{
						if (child.pos.z < mo.pos.z)
						{
							child.SetZ(mo.pos.z - dist);
						}
						else
						{
							child.SetZ(mo.pos.z + dist);
						}
					}
					else
					{
						child.SetZ(mo.pos.z + (newDistance * (child.pos.z - mo.pos.z) / oldDistance));
					}
				}
			}
			mo = child;
			dist -= 1;
		}
	}

	//============================================================================
	//
	// CHolyTailRemove
	//
	//============================================================================

	private void CHolyTailRemove ()
	{
		Actor mo = self;
		while (mo)
		{
			Actor next = mo.tracer;
			mo.Destroy ();
			mo = next;
		}
	}

	//============================================================================
	//
	// A_CHolyTail
	//
	//============================================================================

	void A_CHolyTail()
	{
		Actor parent = target;

		if (parent == null || parent.health <= 0)	// better check for health than current state - it's safer!
		{ // Ghost removed, so remove all tail parts
			CHolyTailRemove ();
			return;
		}
		else
		{
			if (TryMove(parent.Vec2Angle(14., parent.Angle, true), true))
			{
				SetZ(parent.pos.z - 5.);
			}
			CHolyTailFollow(10);
		}
	}
}

// Holy Tail Trail ---------------------------------------------------------

class HolyTailTrail : HolyTail
{
	States
	{
	Spawn:
		Goto TailTrail;
	}
}
