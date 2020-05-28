// The cleric ---------------------------------------------------------------

class ClericPlayer : PlayerPawn
{
	Default
	{
		Health 100;
		ReactionTime 0;
		PainChance 255;
		Radius 16;
		Height 64;
		Speed 1;
		+NOSKIN
		+NODAMAGETHRUST
		+PLAYERPAWN.NOTHRUSTWHENINVUL
		PainSound "PlayerClericPain";
		RadiusDamageFactor 0.25;
		Player.JumpZ 9;
		Player.Viewheight 48;
		Player.SpawnClass "Cleric";
		Player.DisplayName "Cleric";
		Player.SoundClass "cleric";
		Player.ScoreIcon "CLERFACE";
		Player.InvulnerabilityMode "Ghost";
		Player.HealRadiusType "Health";
		Player.Hexenarmor 10, 10, 25, 5, 20;
		Player.StartItem "CWeapMace";
		Player.Portrait "P_CWALK1";
		Player.WeaponSlot 1, "CWeapMace";
		Player.WeaponSlot 2, "CWeapStaff";
		Player.WeaponSlot 3, "CWeapFlame";
		Player.WeaponSlot 4, "CWeapWraithverge";
		Player.FlechetteType "ArtiPoisonBag1";
		
		Player.ColorRange 146, 163;
		Player.Colorset		0, "$TXT_COLOR_BLUE",		146, 163,    161;
		Player.ColorsetFile 1, "$TXT_COLOR_RED",		"TRANTBL7",  0xB3;
		Player.ColorsetFile 2, "$TXT_COLOR_GOLD",		"TRANTBL8",  0x8C;
		Player.ColorsetFile 3, "$TXT_COLOR_DULLGREEN",	"TRANTBL9",  0x41;
		Player.ColorsetFile 4, "$TXT_COLOR_GREEN",		"TRANTBLA",  0xC9;
		Player.ColorsetFile 5, "$TXT_COLOR_GRAY",		"TRANTBLB",  0x30;
		Player.ColorsetFile 6, "$TXT_COLOR_BROWN",		"TRANTBLC",  0x72;
		Player.ColorsetFile 7, "$TXT_COLOR_PURPLE",		"TRANTBLD",  0xEE;
	}
	
	States
	{
	Spawn:
		CLER A -1;
		Stop;
	See:
		CLER ABCD 4;
		Loop;
	Pain:
		CLER H 4;
		CLER H 4 A_Pain;
		Goto Spawn;
	Missile:
	Melee:
		CLER EFG 6;
		Goto Spawn;
	Death:
		CLER I 6;
		CLER J 6 A_PlayerScream;
		CLER KL 6;
		CLER M 6 A_NoBlocking;
		CLER NOP 6;
		CLER Q -1;
		Stop;		
	XDeath:
		CLER R 5 A_PlayerScream;
		CLER S 5;
		CLER T 5 A_NoBlocking;
		CLER UVWXYZ 5;
		CLER [ -1;
		Stop;
	Ice:
		CLER \ 5 A_FreezeDeath;
		CLER \ 1 A_FreezeDeathChunks;
		Wait;
	Burn:
		FDTH C 5 BRIGHT A_StartSound("*burndeath");
		FDTH D 4 BRIGHT;
		FDTH G 5 BRIGHT;
		FDTH H 4 BRIGHT A_PlayerScream;
		FDTH I 5 BRIGHT;
		FDTH J 4 BRIGHT;
		FDTH K 5 BRIGHT;
		FDTH L 4 BRIGHT;
		FDTH M 5 BRIGHT;
		FDTH N 4 BRIGHT;
		FDTH O 5 BRIGHT;
		FDTH P 4 BRIGHT;
		FDTH Q 5 BRIGHT;
		FDTH R 4 BRIGHT;
		FDTH S 5 BRIGHT A_NoBlocking;
		FDTH T 4 BRIGHT;
		FDTH U 5 BRIGHT;
		FDTH V 4 BRIGHT;
		ACLO E 35 A_CheckPlayerDone;
		Wait;
		ACLO E 8;
		Stop;
	}
}
		
