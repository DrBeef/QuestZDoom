// Chex Warrior

class ChexPlayer : DoomPlayer
{
	Default
	{
		player.displayname "Chex Warrior";
		player.crouchsprite "";
		player.colorrange 192, 207; //Not perfect, but its better than everyone being blue.
		player.startitem "MiniZorcher";
		player.startitem "Bootspoon";
		player.startitem "MiniZorchRecharge", 50;
		player.damagescreencolor "60 b0 58";
		player.WeaponSlot 1, "Bootspoon", "SuperBootspork";
		player.WeaponSlot 2, "MiniZorcher";
		player.WeaponSlot 3, "LargeZorcher", "SuperLargeZorcher";
		player.WeaponSlot 4, "RapidZorcher";
		player.WeaponSlot 5, "ZorchPropulsor";
		player.WeaponSlot 6, "PhasingZorcher";
		player.WeaponSlot 7, "LAZDevice";
		
		Player.Colorset 0, "$TXT_COLOR_LIGHTBLUE",	0xC0, 0xCF,  0xC2;
		Player.Colorset 1, "$TXT_COLOR_GREEN",		0x70, 0x7F,  0x72;
		Player.Colorset 2, "$TXT_COLOR_GRAY",		0x60, 0x6F,  0x62;
		Player.Colorset 3, "$TXT_COLOR_BROWN",		0x40, 0x4F,  0x42;
		Player.Colorset 4, "$TXT_COLOR_RED",		0x20, 0x2F,  0x22;
		Player.Colorset 5, "$TXT_COLOR_LIGHTGRAY",	0x58, 0x67,  0x5A;
		Player.Colorset 6, "$TXT_COLOR_LIGHTBROWN",	0x38, 0x47,  0x3A;
		Player.Colorset 7, "$TXT_COLOR_LIGHTRED",	0xB0, 0xBF,  0xB2;
	}
	
}
