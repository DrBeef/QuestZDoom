class Minotaur : Actor
{
	const MAULATORTICS = 25 * TICRATE;
	const MNTR_CHARGE_SPEED =13.;
	const MINOTAUR_LOOK_DIST = 16*54.;
	
	Default
	{
		Health 3000;
		Radius 28;
		Height 100;
		Mass 800;
		Speed 16;
		Damage 7;
		Painchance 25;
		Monster;
		+DROPOFF
		+FLOORCLIP
		+BOSS
		+NORADIUSDMG
		+DONTMORPH
		+NOTARGET
		+BOSSDEATH
		SeeSound "minotaur/sight";
		AttackSound "minotaur/attack1";
		PainSound "minotaur/pain";
		DeathSound "minotaur/death";
		ActiveSound "minotaur/active";
		DropItem "ArtiSuperHealth", 51;
		DropItem "PhoenixRodAmmo", 84, 10;
		Obituary "$OB_MINOTAUR";
		HitObituary "$OB_MINOTAURHIT";
		Tag "$FN_MINOTAUR";
	}

	States
	{
	Spawn:
		MNTR AB 10 A_MinotaurLook;
		Loop;
	Roam:
		MNTR ABCD 5 A_MinotaurRoam;
		Loop;
	See:
		MNTR ABCD 5 A_MinotaurChase;
		Loop;
	Melee:
		MNTR V 10 A_FaceTarget;
		MNTR W 7 A_FaceTarget;
		MNTR X 12 A_MinotaurAtk1;
		Goto See;
	Missile:
		MNTR V 10 A_MinotaurDecide;
		MNTR Y 4 A_FaceTarget;
		MNTR Z 9 A_MinotaurAtk2;
		Goto See;
	Hammer:
		MNTR V 10 A_FaceTarget;
		MNTR W 7 A_FaceTarget;
		MNTR X 12 A_MinotaurAtk3;
		Goto See;
	HammerLoop:
		MNTR X 12;
		Goto Hammer;
	Charge:
		MNTR U 2 A_MinotaurCharge;
		Loop;
	Pain:
		MNTR E 3;
		MNTR E 6 A_Pain;
		Goto See;
	Death:
		MNTR F 6 A_MinotaurDeath;
		MNTR G 5;
		MNTR H 6 A_Scream;
		MNTR I 5;
		MNTR J 6;
		MNTR K 5;
		MNTR L 6;
		MNTR M 5 A_NoBlocking;
		MNTR N 6;
		MNTR O 5;
		MNTR P 6;
		MNTR Q 5;
		MNTR R 6;
		MNTR S 5;
		MNTR T -1 A_BossDeath;
		Stop;
	FadeOut:
		MNTR E 6;
		MNTR E 2 A_Scream;
		MNTR E 5 A_SpawnItemEx("MinotaurSmokeExit");
		MNTR E 5;
		MNTR E 5 A_NoBlocking;
		MNTR E 5;
		MNTR E 5 A_SetTranslucent(0.66, 0);
		MNTR E 5 A_SetTranslucent(0.33, 0);
		MNTR E 10 A_BossDeath;
		Stop;
	}
	
	//---------------------------------------------------------------------------
	//
	// FUNC P_MinotaurSlam
	//
	//---------------------------------------------------------------------------

	void MinotaurSlam (Actor target)
	{
		double ang = AngleTo(target);
		double thrust = 16 + random[MinotaurSlam]() / 64.;
		target.VelFromAngle(ang, thrust);
		int damage = random[MinotaurSlam](1, 8) * (bSummonedMonster? 4 : 6);
		int newdam = target.DamageMobj (null, null, damage, 'Melee');
		target.TraceBleedAngle (newdam > 0 ? newdam : damage, ang, 0.);
		if (target.player)
		{
			target.reactiontime = random[MinotaurSlam](14, 21);
		}
	}

	
	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	override void Tick ()
	{
		Super.Tick ();
		
		// The unfriendly Minotaur (Heretic's) is invulnerable while charging
		if (!bSummonedMonster)
		{
			bInvulnerable = bSkullFly;
		}
	}

	override bool Slam (Actor thing)
	{
		// Slamming minotaurs shouldn't move non-creatures
		if (!thing.bIsMonster && !thing.player)
		{
			return false;
		}
		return Super.Slam (thing);
	}

	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		damage = Super.DoSpecialDamage (target, damage, damagetype);
		if (damage != -1 && bSkullFly)
		{ // Slam only when in charge mode
			MinotaurSlam (target);
			return -1;
		}
		return damage;
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_MinotaurAtk1
	//
	// Melee attack.
	//
	//----------------------------------------------------------------------------

	void A_MinotaurAtk1()
	{
		let targ = target;
		if (!targ)
		{
			return;
		}
		A_StartSound ("minotaur/melee", CHAN_WEAPON);
		if (CheckMeleeRange())
		{
			PlayerInfo player = targ.player;
			int damage = random[MinotaurAtk1](1, 8) * 4;
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			if (player != null && player.mo == targ)
			{ // Squish the player
				player.deltaviewheight = -16;
			}
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_MinotaurDecide
	//
	// Choose a missile attack.
	//
	//----------------------------------------------------------------------------

	void A_MinotaurDecide()
	{
		bool friendly = bSummonedMonster;

		if (!target)
		{
			return;
		}
		if (!friendly)
		{
			A_StartSound ("minotaur/sight", CHAN_WEAPON);
		}
		double dist = Distance2D(target);
		if (target.pos.z + target.height > pos.z
			&& target.pos.z + target.height < pos.z + height
			&& dist < (friendly ? 16*64. : 8*64.)
			&& dist > 1*64.
			&& random[MinotaurDecide]() < 150)
		{ // Charge attack
			// Don't call the state function right away
			SetStateLabel("Charge", true);
			bSkullFly = true;
			if (!friendly)
			{ // Heretic's Minotaur is invulnerable during charge attack
				bInvulnerable = true;
			}
			A_FaceTarget ();
			VelFromAngle(MNTR_CHARGE_SPEED);
			special1 = TICRATE/2; // Charge duration
		}
		else if (target.pos.z == target.floorz
			&& dist < 9*64.
			&& random[MinotaurDecide]() < (friendly ? 100 : 220))
		{ // Floor fire attack
			SetStateLabel("Hammer");
			special2 = 0;
		}
		else
		{ // Swing attack
			A_FaceTarget ();
			// Don't need to call P_SetMobjState because the current state
			// falls through to the swing attack
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_MinotaurCharge
	//
	//----------------------------------------------------------------------------

	void A_MinotaurCharge()
	{
		if (target == null)
		{
			return;
		}
		if (special1 > 0)
		{
			Class<Actor> type;

			if (gameinfo.gametype == GAME_Heretic)
			{
				type = "PhoenixPuff";
			}
			else
			{
				type = "PunchPuff";
			}
			Actor puff = Spawn (type, Pos, ALLOW_REPLACE);
			if (puff != null) puff.Vel.Z = 2;
			special1--;
		}
		else
		{
			bSkullFly = false;
			bInvulnerable = false;
			SetState (SeeState);
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_MinotaurAtk2
	//
	// Swing attack.
	//
	//----------------------------------------------------------------------------

	void A_MinotaurAtk2()
	{
		bool friendly = bSummonedMonster;

		let targ = target;
		if (targ == null)
		{
			return;
		}
		A_StartSound ("minotaur/attack2", CHAN_WEAPON);
		if (CheckMeleeRange())
		{
			int damage = random[MinotaurAtk2](1, 8) * (friendly ? 3 : 5);
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			return;
		}
		double z = pos.z + 40;
		Class<Actor> fx = "MinotaurFX1";
		Actor mo = SpawnMissileZ (z, targ, fx);
		if (mo != null)
		{
//			S_Sound (mo, CHAN_WEAPON, "minotaur/attack2", 1, ATTN_NORM);
			double vz = mo.Vel.Z;
			double ang = mo.angle;
			SpawnMissileAngleZ (z, fx, ang-(45./8), vz);
			SpawnMissileAngleZ (z, fx, ang+(45./8), vz);
			SpawnMissileAngleZ (z, fx, ang-(45./16), vz);
			SpawnMissileAngleZ (z, fx, ang+(45./16), vz);
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_MinotaurAtk3
	//
	// Floor fire attack.
	//
	//----------------------------------------------------------------------------

	void A_MinotaurAtk3()
	{
		bool friendly = bSummonedMonster;

		let targ = target;
		if (!targ)
		{
			return;
		}
		A_StartSound ("minotaur/attack3", CHAN_VOICE);
		if (CheckMeleeRange())
		{
			PlayerInfo player = targ.player;
			int damage = random[MinotaurAtk3](1, 8) * (friendly ? 3 : 5);
			int newdam = targ.DamageMobj (self, self, damage, 'Melee');
			targ.TraceBleed (newdam > 0 ? newdam : damage, self);
			if (player != null && player.mo == targ)
			{ // Squish the player
				player.deltaviewheight = -16;
			}
		}
		else
		{
			if (Floorclip > 0 && (Level.compatflags & COMPAT_MINOTAUR))
			{
				// only play the sound. 
				A_StartSound ("minotaur/fx2hit", CHAN_WEAPON);
			}
			else
			{
				Actor mo = SpawnMissile (target, "MinotaurFX2");
				if (mo != null)
				{
					mo.A_StartSound ("minotaur/attack1", CHAN_WEAPON);
				}
			}
		}
		if (random[MinotaurAtk3]() < 192 && special2 == 0)
		{
			SetStateLabel ("HammerLoop");
			special2 = 1;
		}
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_MinotaurDeath
	//
	//----------------------------------------------------------------------------

	void A_MinotaurDeath()
	{
		if (Wads.CheckNumForName ("MNTRF1", Wads.ns_sprites) < 0 &&
			Wads.CheckNumForName ("MNTRF0", Wads.ns_sprites) < 0)
			SetStateLabel("FadeOut");
	}


	//----------------------------------------------------------------------------
	//
	// A_MinotaurRoam
	//
	//----------------------------------------------------------------------------

	void A_MinotaurRoam()
	{
		// In case pain caused him to skip his fade in.
		A_SetRenderStyle(1, STYLE_Normal);

		let mf = MinotaurFriend(self);
		if (mf)
		{
			if (mf.StartTime >= 0 && (level.maptime - mf.StartTime) >= MAULATORTICS)
			{
				DamageMobj (null, null, TELEFRAG_DAMAGE, 'None');
				return;
			}
		}

		if (random[MinotaurRoam]() < 30)
			A_MinotaurLook();		// adjust to closest target

		if (random[MinotaurRoam]() < 6)
		{
			//Choose new direction
			movedir = random[MinotaurRoam](0, 7);
			FaceMovementDirection ();
		}
		if (!MonsterMove())
		{
			// Turn
			if (random[MinotaurRoam](0, 1))
				movedir = (movedir + 1) % 8;
			else
				movedir = (movedir + 7) % 8;
			FaceMovementDirection ();
		}
	}


	//----------------------------------------------------------------------------
	//
	//	PROC A_MinotaurLook
	//
	// Look for enemy of player
	//----------------------------------------------------------------------------

	void A_MinotaurLook()
	{
		if (!(self is "MinotaurFriend"))
		{
			A_Look();
			return;
		}

		Actor mo = null;
		PlayerInfo player;
		double dist;
		Actor master = tracer;

		target = null;
		if (deathmatch)					// Quick search for players
		{
			for (int i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i]) continue;
				player = players[i];
				mo = player.mo;
				if (mo == master) continue;
				if (mo.health <= 0) continue;
				dist = Distance2D(mo);
				if (dist > MINOTAUR_LOOK_DIST) continue;
				target = mo;
				break;
			}
		}

		if (!target)				// Near player monster search
		{
			if (master && (master.health > 0) && (master.player))
				mo = master.RoughMonsterSearch(20);
			else
				mo = RoughMonsterSearch(20);
			target = mo;
		}

		if (!target)				// Normal monster search
		{
			ThinkerIterator it = ThinkerIterator.Create("Actor");

			while ((mo = Actor(it.Next())) != null)
			{
				if (!mo.bIsMonster) continue;
				if (mo.health <= 0) continue;
				if (!mo.bShootable) continue;
				dist = Distance2D(mo);
				if (dist > MINOTAUR_LOOK_DIST) continue;
				if (mo == master || mo == self) continue;
				if (mo.bSummonedMonster && mo.tracer == master) continue;
				target = mo;
				break;			// Found actor to attack
			}
		}

		if (target)
		{
			SetState (SeeState, true);
		}
		else
		{
			SetStateLabel ("Roam", true);
		}
	}

	//----------------------------------------------------------------------------
	//
	//	PROC A_MinotaurChase
	//
	//----------------------------------------------------------------------------

	void A_MinotaurChase()
	{
		let mf = MinotaurFriend(self);
		if (!mf)
		{
			A_Chase();
			return;
		}


		// In case pain caused him to skip his fade in.
		A_SetRenderStyle(1, STYLE_Normal);

		if (mf.StartTime >= 0 && (level.maptime - mf.StartTime) >= MAULATORTICS)
		{
			DamageMobj (null, null, TELEFRAG_DAMAGE, 'None');
			return;
		}

		if (random[MinotaurChase]() < 30)
			A_MinotaurLook();		// adjust to closest target

		if (!target || (target.health <= 0) || !target.bShootable)
		{ // look for a new target
			SetIdle();
			return;
		}

		FaceMovementDirection ();
		reactiontime = 0;

		// Melee attack
		if (MeleeState && CheckMeleeRange ())
		{
			if (AttackSound)
			{
				A_StartSound (AttackSound, CHAN_WEAPON);
			}
			SetState (MeleeState);
			return;
		}

		// Missile attack
		if (MissileState && CheckMissileRange())
		{
			SetState (MissileState);
			return;
		}

		// chase towards target
		if (!MonsterMove ())
		{
			NewChaseDir ();
			FaceMovementDirection ();
		}

		// Active sound
		if (random[MinotaurChase]() < 6)
		{
			PlayActiveSound ();
		}
	}
}

class MinotaurFriend : Minotaur
{
	int StartTime;
	
	Default
	{
		Health 2500;
		-DROPOFF
		-BOSS
		-DONTMORPH
		+FRIENDLY
		+NOTARGETSWITCH
		+STAYMORPHED
		+TELESTOMP
		+SUMMONEDMONSTER
		RenderStyle "Translucent";
		Alpha 0.3333;
		DropItem "None";
	}
	States
	{
	Spawn:
		MNTR A 15;
		MNTR A 15 A_SetTranslucent(0.66, 0);
		MNTR A 3 A_SetTranslucent(1, 0);
		Goto Super::Spawn;
	Idle:
		Goto Super::Spawn;
	Death:
		Goto FadeOut;
	}
	
	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	override void BeginPlay ()
	{
		Super.BeginPlay ();
		StartTime = -1;
	}

	override void Die (Actor source, Actor inflictor, int dmgflags, Name MeansOfDeath)
	{
		Super.Die (source, inflictor, dmgflags, MeansOfDeath);

		if (tracer && tracer.health > 0 && tracer.player)
		{
			// Search thinker list for minotaur
			ThinkerIterator it = ThinkerIterator.Create("MinotaurFriend");
			MinotaurFriend mo;

			while ((mo = MinotaurFriend(it.Next())) != null)
			{
				if (mo.health <= 0) continue;
				// [RH] Minotaurs can't be morphed, so this isn't needed
				//if (!(mo.flags&MF_COUNTKILL)) continue;		// for morphed minotaurs
				if (mo.bCorpse) continue;
				if (mo.StartTime >= 0 && (level.maptime - StartTime) >= MAULATORTICS) continue;
				if (mo.tracer != null && mo.tracer.player == tracer.player) break;
			}

			if (mo == null)
			{
				Inventory power = tracer.FindInventory("PowerMinotaur");
				if (power != null)
				{
					power.Destroy ();
				}
			}
		}
	}

	
}

// Minotaur FX 1 ------------------------------------------------------------

class MinotaurFX1 : Actor
{
	Default
	{
		Radius 10;
		Height 6;
		Speed 20;
		FastSpeed 26;
		Damage 3;
		DamageType "Fire";
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		+ZDOOMTRANS
		RenderStyle "Add";
	}
	States
	{
	Spawn:
		FX12 AB 6 Bright;
		Loop;
	Death:
		FX12 CDEFGH 5 Bright;
		Stop;
	}
}


// Minotaur FX 2 ------------------------------------------------------------

class MinotaurFX2 : MinotaurFX1
{
	Default
	{
		Radius 5;
		Height 12;
		Speed 14;
		FastSpeed 20;
		Damage 4;
		+FLOORHUGGER
		ExplosionDamage 24;
		DeathSound "minotaur/fx2hit";
	}
	
	states
	{
	Spawn:
		FX13 A 2 Bright A_MntrFloorFire;
		Loop;
	Death:
		FX13 I 4 Bright A_Explode;
		FX13 JKLM 4 Bright;
		Stop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_MntrFloorFire
	//
	//----------------------------------------------------------------------------

	void A_MntrFloorFire()
	{
		SetZ(floorz);
		double x = Random2[MntrFloorFire]() / 64.;
		double y = Random2[MntrFloorFire]() / 64.;
		
		Actor mo = Spawn("MinotaurFX3", Vec2OffsetZ(x, y, floorz), ALLOW_REPLACE);
		if (mo != null)
		{
			mo.target = target;
			mo.Vel.X = MinVel; // Force block checking
			mo.CheckMissileSpawn (radius);
		}
	}
}

// Minotaur FX 3 ------------------------------------------------------------

class MinotaurFX3 : MinotaurFX2
{
	Default
	{
		Radius 8;
		Height 16;
		Speed 0;
		DeathSound "minotaur/fx3hit";
		ExplosionDamage 128;
	}
	States
	{
	Spawn:
		FX13 DC 4 Bright;
		FX13 BCDE 5 Bright;
		FX13 FGH 4 Bright;
		Stop;
	}
}

// Minotaur Smoke Exit ------------------------------------------------------

class MinotaurSmokeExit : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOTELEPORT
		RenderStyle "Translucent";
		Alpha 0.4;
	}
	States
	{
	Spawn:
		MNSM ABCDEFGHIJIHGFEDCBA 3;
		Stop;
	}
}

extend class Actor
{
	enum dirtype_t
	{
		DI_EAST,
		DI_NORTHEAST,
		DI_NORTH,
		DI_NORTHWEST,
		DI_WEST,
		DI_SOUTHWEST,
		DI_SOUTH,
		DI_SOUTHEAST,
		DI_NODIR,
		NUMDIRS
	};

	void FaceMovementDirection()
	{
		switch (movedir)
		{
		case DI_EAST:
			angle = 0.;
			break;
		case DI_NORTHEAST:
			angle = 45.;
			break;
		case DI_NORTH:
			angle = 90.;
			break;
		case DI_NORTHWEST:
			angle = 135.;
			break;
		case DI_WEST:
			angle = 180.;
			break;
		case DI_SOUTHWEST:
			angle = 225.;
			break;
		case DI_SOUTH:
			angle = 270.;
			break;
		case DI_SOUTHEAST:
			angle = 315.;
			break;
		}
	}
}
