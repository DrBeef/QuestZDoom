//===========================================================================
// Korax Variables
//	tracer		last teleport destination
//	special2	set if "below half" script not yet run
//
// Korax Scripts (reserved)
//	249		Tell scripts that we are below half health
//	250-254	Control scripts (254 is only used when less than half health)
//	255		Death script
//
// Korax TIDs (reserved)
//	245		Reserved for Korax himself
//  248		Initial teleport destination
//	249		Teleport destination
//	250-254	For use in respective control scripts
//	255		For use in death script (spawn spots)
//===========================================================================

class Korax : Actor
{
	const KORAX_ARM_EXTENSION_SHORT	= 40;
	const KORAX_ARM_EXTENSION_LONG	= 55;

	const KORAX_ARM1_HEIGHT			= 108;
	const KORAX_ARM2_HEIGHT			= 82;
	const KORAX_ARM3_HEIGHT			= 54;
	const KORAX_ARM4_HEIGHT			= 104;
	const KORAX_ARM5_HEIGHT			= 86;
	const KORAX_ARM6_HEIGHT			= 53;

	const KORAX_FIRST_TELEPORT_TID	= 248;
	const KORAX_TELEPORT_TID		= 249;

	const KORAX_DELTAANGLE			= 85;

	const KORAX_COMMAND_HEIGHT	= 120;
	const KORAX_COMMAND_OFFSET	= 27;

	const KORAX_SPIRIT_LIFETIME = 5*TICRATE/5;	// 5 seconds

	Default
	{
		Health 5000;
		Painchance 20;
		Speed 10;
		Radius 65;
		Height 115;
		Mass 2000;
		Damage 15;
		Monster;
		+BOSS
		+FLOORCLIP
		+TELESTOMP
		+DONTMORPH
		+NOTARGET
		+NOICEDEATH
		SeeSound "KoraxSight";
		AttackSound "KoraxAttack";
		PainSound "KoraxPain";
		DeathSound "KoraxDeath";
		ActiveSound "KoraxActive";
		Obituary "$OB_KORAX";
		Tag "$FN_KORAX";
	}

	States
	{
	Spawn:
		KORX A 5 A_Look;
		Loop;
	See:
		KORX AAA 3 A_KoraxChase;
		KORX B 3 A_Chase;
		KORX BBB 3 A_KoraxChase;
		KORX C 3 A_KoraxStep;
		KORX CCC 3 A_KoraxChase;
		KORX D 3 A_Chase;
		KORX DDD 3 A_KoraxChase;
		KORX A 3 A_KoraxStep;
		Loop;
	Pain:
		KORX H 5 A_Pain;
		KORX H 5;
		Goto See;
	Missile:
		KORX E 2 Bright A_FaceTarget;
		KORX E 5 Bright A_KoraxDecide;
		Wait;
	Death:
		KORX I 5;
		KORX J 5 A_FaceTarget;
		KORX K 5 A_Scream;
		KORX LMNOP 5;
		KORX Q 10;
		KORX R 5 A_KoraxBonePop;
		KORX S 5 A_NoBlocking;
		KORX TU 5;
		KORX V -1;
		Stop;
	Attack:
		KORX E 4 Bright A_FaceTarget;
		KORX F 8 Bright A_KoraxMissile;
		KORX E 8 Bright;
		Goto See;
	Command:
		KORX E 5 Bright A_FaceTarget;
		KORX W 10 Bright A_FaceTarget;
		KORX G 15 Bright A_KoraxCommand;
		KORX W 10 Bright;
		KORX E 5 Bright;
		Goto See;
	}
	
	
	void A_KoraxStep()
	{ 
		A_StartSound("KoraxStep"); 
		A_Chase(); 
	}	
	
	//============================================================================
	//
	// A_KoraxChase
	//
	//============================================================================


	void A_KoraxChase()
	{
		if ((!special2) && (health <= (SpawnHealth()/2)))
		{
			ActorIterator it = ActorIterator.Create(KORAX_FIRST_TELEPORT_TID);
			Actor spot = it.Next ();
			if (spot != null)
			{
				Teleport ((spot.pos.xy, ONFLOORZ), spot.angle, TELF_SOURCEFOG | TELF_DESTFOG);
			}
			ACS_Execute(249, 0);
			special2 = 1;	// Don't run again
			return;
		}

		if (target == null)
		{
			return;
		}
		if (random[KoraxChase]() < 30)
		{
			SetState (MissileState);
		}
		else if (random[KoraxChase]() < 30)
		{
			A_StartSound("KoraxActive", CHAN_VOICE, CHANF_DEFAULT, 1., ATTN_NONE);
		}

		// Teleport away
		if (health < (SpawnHealth() >> 1))
		{
			if (random[KoraxChase]() < 10)
			{
				ActorIterator it = ActorIterator.Create(KORAX_TELEPORT_TID);
				Actor spot;

				if (tracer != null)
				{	// Find the previous teleport destination
					do
					{
						spot = it.Next ();
					} while (spot != null && spot != tracer);
				}

				// Go to the next teleport destination
				spot = it.Next ();
				tracer = spot;
				if (spot)
				{
					Teleport ((spot.pos.xy, ONFLOORZ), spot.angle, TELF_SOURCEFOG | TELF_DESTFOG);
				}
			}
		}
	}

	//============================================================================
	//
	// A_KoraxDecide
	//
	//============================================================================

	void A_KoraxDecide()
	{
		if (random[KoraxDecide]() < 220)
		{
			SetStateLabel ("Attack");
		}
		else
		{
			SetStateLabel ("Command");
		}
	}

	//============================================================================
	//
	// A_KoraxBonePop
	//
	//============================================================================

	void A_KoraxBonePop()
	{
		// Spawn 6 spirits equalangularly
		for (int i = 0; i < 6; ++i)
		{
			Actor mo = SpawnMissileAngle ("KoraxSpirit", 60.*i, 5.);
			if (mo)
			{
				KSpiritInit (mo);
			}
		}
		ACS_Execute(255, 0);
	}

	//============================================================================
	//
	// KSpiritInit
	//
	//============================================================================

	private void KSpiritInit (Actor spirit)
	{
		spirit.health = KORAX_SPIRIT_LIFETIME;

		spirit.tracer = self;						// Swarm around korax
		spirit.WeaveIndexZ = random[Kspiritnit](32, 39);	// Float bob index
		spirit.args[0] = 10; 						// initial turn value
		spirit.args[1] = 0; 						// initial look angle

		// Spawn a tail for spirit
		HolyTail.SpawnSpiritTail (spirit);
	}

	//============================================================================
	//
	// A_KoraxMissile
	//
	//============================================================================

	void A_KoraxMissile()
	{
		if (!target)
		{
			return;
		}

		static const class<Actor> choices[] =
		{
			"WraithFX1", "Demon1FX1", "Demon2FX1", "FireDemonMissile", "CentaurFX", "SerpentFX"
		};
		static const sound sounds[] = 
		{
			"WraithMissileFire", "DemonMissileFire", "DemonMissileFire", "FireDemonAttack", "CentaurLeaderAttack", "SerpentLeaderAttack"
		};
		int type = random[KoraxMissile](0, 5);

		A_StartSound("KoraxAttack", CHAN_VOICE);

		// Fire all 6 missiles at once
		A_StartSound(sounds[type], CHAN_WEAPON, CHANF_DEFAULT, 1., ATTN_NONE);
		class<Actor> info = choices[type];
		for (int i = 0; i < 6; ++i)
		{
			KoraxFire(info, i);
		}
	}

	//============================================================================
	//
	// KoraxFire
	//
	// Arm projectiles
	//		arm positions numbered:
	//			1	top left
	//			2	middle left
	//			3	lower left
	//			4	top right
	//			5	middle right
	//			6	lower right
	//
	//============================================================================

	void KoraxFire (Class<Actor> type, int arm)
	{
		if (!target)
		{
			return;
		}

		static const int extension[] =
		{
			KORAX_ARM_EXTENSION_SHORT,
			KORAX_ARM_EXTENSION_LONG,
			KORAX_ARM_EXTENSION_LONG,
			KORAX_ARM_EXTENSION_SHORT,
			KORAX_ARM_EXTENSION_LONG,
			KORAX_ARM_EXTENSION_LONG
		};
		static const int armheight[] =
		{
			KORAX_ARM1_HEIGHT,
			KORAX_ARM2_HEIGHT,
			KORAX_ARM3_HEIGHT,
			KORAX_ARM4_HEIGHT,
			KORAX_ARM5_HEIGHT,
			KORAX_ARM6_HEIGHT
		};

		double ang = angle + (arm < 3 ? -KORAX_DELTAANGLE : KORAX_DELTAANGLE);
		Vector3 pos = Vec3Angle(extension[arm], ang, armheight[arm] - Floorclip);
		SpawnKoraxMissile (pos, target, type);
	}

	//============================================================================
	//
	// P_SpawnKoraxMissile
	//
	//============================================================================

	private void SpawnKoraxMissile (Vector3 pos, Actor dest, Class<Actor> type)
	{
		Actor th = Spawn (type, pos, ALLOW_REPLACE);
		if (th != null)
		{
			th.target = self; // Originator
			double an = th.AngleTo(dest);
			if (dest.bShadow)
			{ // Invisible target
				an += Random2[KoraxMissile]() * (45/256.);
			}
			th.angle = an;
			th.VelFromAngle();
			double dist = dest.DistanceBySpeed(th, th.Speed);
			th.Vel.Z = (dest.pos.z - pos.Z + 30) / dist;
			th.CheckMissileSpawn(radius);
		}
	}

	//============================================================================
	//
	// A_KoraxCommand
	//
	// Call action code scripts (250-254)
	//
	//============================================================================

	void A_KoraxCommand()
	{
		int numcommands;

		A_StartSound("KoraxCommand", CHAN_VOICE);

		// Shoot stream of lightning to ceiling
		double ang = angle - 90;
		Vector3 pos = Vec3Angle(KORAX_COMMAND_OFFSET, ang, KORAX_COMMAND_HEIGHT);
		Spawn("KoraxBolt", pos, ALLOW_REPLACE);

		if (health <= (SpawnHealth() >> 1))
		{
			numcommands = 4;
		}
		else
		{
			numcommands = 3;
		}

		ACS_Execute(250 + (random[KoraxCommand](0, numcommands)), 0);
	}
}

class KoraxSpirit : Actor
{
	Default
	{
		Speed 8;
		Projectile;
		+NOCLIP
		-ACTIVATEPCROSS
		-ACTIVATEIMPACT
		RenderStyle "Translucent";
		Alpha 0.4;
	}
	
	States
	{
	Spawn:
		SPIR AB 5 A_KSpiritRoam;
		Loop;
	Death:
		SPIR DEFGHI 5;
		Stop;
	}
	
	//============================================================================
	//
	// A_KSpiritSeeker
	//
	//============================================================================

	private void KSpiritSeeker (double thresh, double turnMax)
	{
		Actor target = tracer;
		if (target == null)
		{
			return;
		}
		double dir = deltaangle(angle, AngleTo(target));
		double delta = abs(dir);
		if (delta > thresh)
		{
			delta /= 2;
			if(delta > turnMax)
			{
				delta = turnMax;
			}
		}
		if(dir > 0)
		{ // Turn clockwise
			angle += delta;
		}
		else
		{ // Turn counter clockwise
			angle -= delta;
		}
		VelFromAngle();

		if (!(Level.maptime&15) 
			|| pos.z > target.pos.z + target.Default.Height
			|| pos.z + height < target.pos.z)
		{
			double newZ = target.pos.z + random[KoraxRoam]() * target.Default.Height / 256;
			double deltaZ = newZ - pos.z;

			if (abs(deltaZ) > 15)
			{
				if(deltaZ > 0)
				{
					deltaZ = 15;
				}
				else
				{
					deltaZ = -15;
				}
			}
			Vel.Z = deltaZ + DistanceBySpeed(target, Speed);
		}
	}

	//============================================================================
	//
	// A_KSpiritRoam
	//
	//============================================================================

	void A_KSpiritRoam()
	{
		if (health-- <= 0)
		{
			A_StartSound("SpiritDie", CHAN_VOICE);
			SetStateLabel ("Death");
		}
		else
		{
			if (tracer)
			{
				KSpiritSeeker(args[0], args[0] * 2.);
			}
			int xyspeed = random[KoraxRoam](0, 4);
			int zspeed = random[KoraxRoam](0, 4);
			A_Weave(xyspeed, zspeed, 4., 2.);

			if (random[KoraxRoam]() < 50)
			{
				A_StartSound("SpiritActive", CHAN_VOICE, CHANF_DEFAULT, 1., ATTN_NONE);
			}
		}
	}
}

class KoraxBolt : Actor
{
	const KORAX_BOLT_HEIGHT		= 48.;
	const KORAX_BOLT_LIFETIME		= 3;
	
	Default
	{
		Radius 15;
		Height 35;
		Projectile;
		-ACTIVATEPCROSS
		-ACTIVATEIMPACT
		RenderStyle "Add";
	}
	
	States
	{
	Spawn:
		MLFX I 2 Bright;
		MLFX J 2 Bright A_KBoltRaise;
		MLFX IJKLM 2 Bright A_KBolt;
		Stop;
	}
	
	//============================================================================
	//
	// A_KBolt
	//
	//============================================================================

	void A_KBolt()
	{
		// Countdown lifetime
		if (special1-- <= 0)
		{
			Destroy ();
		}
	}

	//============================================================================
	//
	// A_KBoltRaise
	//
	//============================================================================

	void A_KBoltRaise()
	{
		// Spawn a child upward
		double z = pos.z + KORAX_BOLT_HEIGHT;

		if ((z + KORAX_BOLT_HEIGHT) < ceilingz)
		{
			Actor mo = Spawn("KoraxBolt", (pos.xy, z), ALLOW_REPLACE);
			if (mo)
			{
				mo.special1 = KORAX_BOLT_LIFETIME;
			}
		}
	}
}
