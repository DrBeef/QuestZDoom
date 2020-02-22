
// Zombie -------------------------------------------------------------------

class Zombie : StrifeHumanoid
{
	Default
	{
		Health 31;
		Radius 20;
		Height 56;
		PainChance 0;
		+SOLID
		+SHOOTABLE
		+FLOORCLIP
		+CANPASS
		+CANPUSHWALLS
		+ACTIVATEMCROSS
		MinMissileChance 150;
		MaxStepHeight 16;
		MaxDropOffHeight 32;
		Translation 0;
		DeathSound "zombie/death";
		CrushPainSound "misc/pcrush";
	}
	States
	{
	Spawn:
		PEAS A 5 A_CheckTerrain;
		Loop;
	Pain:
		AGRD A 5 A_CheckTerrain;
		Loop;
	Death:
		GIBS M 5 A_TossGib;
		GIBS N 5 A_XScream;
		GIBS O 5 A_NoBlocking;
		GIBS PQRST 4 A_TossGib;
		GIBS U 5;
		GIBS V -1;
		Stop;
	}
}


// Zombie Spawner -----------------------------------------------------------

class ZombieSpawner : Actor
{
	Default
	{
		Health 20;
		+SHOOTABLE
		+NOSECTOR
		RenderStyle "None";
		ActiveSound "zombie/spawner";	// Does Strife use this somewhere else?
	}
	States
	{
	Spawn:
		TNT1 A 175 A_SpawnItemEx("Zombie");
		Loop;
	}
}

