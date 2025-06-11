//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_shareddefs.h"
#include "KeyValues.h"
#include "takedamageinfo.h"
#include "tf_gamerules.h"
#include "filesystem.h"

const char *s_aFactionPath[FACTION_COUNT] =
{
	"tf_", // FACTION_TF
	"dod_", // FACTION_DOD
	"pc_", // FACTION_PC
	"cs_", // FACTION_CS
	"hl2_", // FACTION_HL2
	"hl1_" // FACTION_HL1
};

//-----------------------------------------------------------------------------
// Teams.
//-----------------------------------------------------------------------------
const char *g_aTeamNames[TF_TEAM_COUNT] =
{
	"Unassigned",
	"Spectator",
	"Red",
	"Blue"
};

color32 g_aTeamColors[TF_TEAM_COUNT] = 
{
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 255, 0, 0, 0 },
	{ 0, 0, 255, 0 }
};

//-----------------------------------------------------------------------------
// Classes.
//-----------------------------------------------------------------------------

const char *g_aPlayerClassNames[GAME_CLASS_COUNT] =
{
	"#TF_Class_Name_Undefined",
	"#TF_Class_Name_Scout",
	"#TF_Class_Name_Sniper",
	"#TF_Class_Name_Soldier",
	"#TF_Class_Name_Demoman",
	"#TF_Class_Name_Medic",
	"#TF_Class_Name_HWGuy",
	"#TF_Class_Name_Pyro",
	"#TF_Class_Name_Spy",
	"#TF_Class_Name_Engineer",
	"#TF_Random",
	"#class_allied_thompson",
	"#class_allied_heavy",
	"#class_allied_garand",
	"#class_allied_sniper",
	"#class_allied_bazooka",
	"#class_allied_mg",
	"#class_axis_mp40",
	"#class_axis_mp44",
	"#class_axis_kar98",
	"#class_axis_sniper",
	"#class_axis_pschreck",
	"#class_axis_mg42",
	"#PC_Class_Name_Aigis",
	"#CS_Class_Name_CT",
	"#CS_Class_Name_T",
	"#HL2_Class_Name_Rebel",
	"#HL2_Class_Name_Combine",
	"#HL2_Class_Name_Deathmatch", // TODO: Temp, merge all HL2 classes into one class
	"#HL1_Class_Name_Deathmatch",
};

const char *g_aPlayerClassNames_NonLocalized[GAME_CLASS_COUNT] =
{
	"Undefined",
	"Scout",
	"Sniper",
	"Soldier",
	"Demoman",
	"Medic",
	"Heavy",
	"Pyro",
	"Spy",
	"Engineer",
	"Random",
	"Assualt (Allies)",
	"Support (Allies)",
	"Rifleman (Allies)",
	"Sniper (Allies)",
	"Rocket (Allies)",
	"Machine Gunner (Allies)",
	"Assault (Axis)",
	"Support (Axis)",
	"Rifleman (Axis)",
	"Sniper (Axis)",
	"Rocket (Axis)",
	"Machine Gunner (Axis)",
	"Aigis",
	"Counter-Terrorist",
	"Terrorist",
	"Rebel",
	"Combine",
	"HL2DM", // TODO: Temp, merge all HL2 classes into one class
	"HL1DM",
};

const char *g_aRawPlayerClassNamesShort[GAME_CLASS_COUNT] =
{
	"undefined",
	"scout",
	"sniper",
	"soldier",
	"demo",	// short
	"medic",
	"heavy",// short
	"pyro",
	"spy",
	"engineer",
	"random",
	"tommy",
	"bar",
	"garand",
	"spring",
	"bazooka",
	"30cal",
	"mp40",
	"mp44",
	"k98",
	"k98s",
	"pschreck",
	"mg42",
	"aigis",
	"ct",
	"t",
	"rebel",
	"combine",
	"hl2dm",
	"hl1dm",
};

const char *g_aRawPlayerClassNames[GAME_CLASS_COUNT] =
{
	"undefined",
	"scout",
	"sniper",
	"soldier",
	"demoman",
	"medic",
	"heavyweapons",
	"pyro",
	"spy",
	"engineer",
	"random",
	"tommy",
	"bar",
	"garand",
	"spring",
	"bazooka",
	"30cal",
	"mp40",
	"mp44",
	"k98",
	"k98s",
	"pschreck",
	"mg42",
	"aigis",
	"counter-terrorist",
	"terrorist",
	"rebel",
	"combine",
	"hl2dm",
	"hl1dm",
};

const char *g_pszBreadModels[] = 
{
	"models/weapons/c_models/c_bread/c_bread_baguette.mdl",		// Spy
	"models/weapons/c_models/c_bread/c_bread_burnt.mdl",		// Pyro
	"models/weapons/c_models/c_bread/c_bread_cinnamon.mdl",		// Demo?
	"models/weapons/c_models/c_bread/c_bread_cornbread.mdl",	// Engineer
	"models/weapons/c_models/c_bread/c_bread_crumpet.mdl",		// Sniper?
	"models/weapons/c_models/c_bread/c_bread_plainloaf.mdl",	// Scout
	"models/weapons/c_models/c_bread/c_bread_pretzel.mdl",		// Medic
	"models/weapons/c_models/c_bread/c_bread_ration.mdl",		// Soldier
	"models/weapons/c_models/c_bread/c_bread_russianblack.mdl",	// Heavy?
};

int GetClassIndexFromString( const char *pClassName, int nLastClassIndex/*=TF_LAST_NORMAL_CLASS*/ )
{
	for ( int i = TF_FIRST_NORMAL_CLASS; i <= nLastClassIndex; ++i )
	{
		// compare first N characters to allow matching both "heavy" and "heavyweapons"
		int classnameLength = V_strlen( g_aPlayerClassNames_NonLocalized[i] );

		if ( V_strlen( pClassName ) < classnameLength )
			continue;

		if ( !V_strnicmp( g_aPlayerClassNames_NonLocalized[i], pClassName, classnameLength ) )
		{
			return i;
		}
	}

	return TF_CLASS_UNDEFINED;
}

int iRemapIndexToClass[TF_CLASS_MENU_BUTTONS] =
{
	0, // Undefined
		TF_CLASS_SCOUT,
		TF_CLASS_SOLDIER,
		TF_CLASS_PYRO,
		TF_CLASS_DEMOMAN,
		TF_CLASS_HEAVYWEAPONS,
		TF_CLASS_ENGINEER,
		TF_CLASS_MEDIC,
		TF_CLASS_SNIPER,
		TF_CLASS_SPY,
		TF_CLASS_RANDOM
};

int GetRemappedMenuIndexForClass( int iClass )
{
	int iIndex = 0;

	for ( int i = 0 ; i < TF_CLASS_MENU_BUTTONS ; i++ )
	{
		if ( iRemapIndexToClass[i] == iClass )
		{
			iIndex = i;
			break;
		}
	}

	return iIndex;
}

ETFCond condition_to_attribute_translation[]  =
{
	TF_COND_BURNING,					// 1 (1<<0)
	TF_COND_AIMING,						// 2 (1<<1)
	TF_COND_ZOOMED,						// 4 (1<<2)
	TF_COND_DISGUISING,					// 8 (...)
	TF_COND_DISGUISED,					// 16
	TF_COND_STEALTHED,					// 32
	TF_COND_INVULNERABLE,				// 64
	TF_COND_TELEPORTED,					// 128
	TF_COND_TAUNTING,					// 256
	TF_COND_INVULNERABLE_WEARINGOFF,	// 512
	TF_COND_STEALTHED_BLINK,			// 1024
	TF_COND_SELECTED_TO_TELEPORT,		// 2048
	TF_COND_CRITBOOSTED,				// 4096
	TF_COND_TMPDAMAGEBONUS,				// 8192
	TF_COND_FEIGN_DEATH,				// 16384
	TF_COND_PHASE,						// 32768
	TF_COND_STUNNED,					// 65536
	TF_COND_HEALTH_BUFF,				// 131072
	TF_COND_HEALTH_OVERHEALED,			// 262144
	TF_COND_URINE,						// 524288
	TF_COND_ENERGY_BUFF,				// 1048576

	TF_COND_LAST				// sentinel value checked against when iterating
};

ETFCond g_aDebuffConditions[] =
{
	TF_COND_BURNING,
	TF_COND_URINE,
	TF_COND_BLEEDING,
	TF_COND_MAD_MILK,
	TF_COND_LAST
};

bool ConditionExpiresFast( ETFCond eCond )
{
	return eCond == TF_COND_BURNING
		|| eCond == TF_COND_URINE
		|| eCond == TF_COND_BLEEDING
		|| eCond == TF_COND_MAD_MILK;
}

static const char *g_aConditionNames[] =
{
	"TF_COND_AIMING",		// Sniper aiming, Heavy minigun.
	"TF_COND_ZOOMED",
	"TF_COND_DISGUISING",
	"TF_COND_DISGUISED",
	"TF_COND_STEALTHED",		// Spy specific
	"TF_COND_INVULNERABLE",
	"TF_COND_TELEPORTED",
	"TF_COND_TAUNTING",
	"TF_COND_INVULNERABLE_WEARINGOFF",
	"TF_COND_STEALTHED_BLINK",
	"TF_COND_SELECTED_TO_TELEPORT",
	"TF_COND_CRITBOOSTED",	// DO NOT RE-USE THIS -- THIS IS FOR KRITZKRIEG AND REVENGE CRITS ONLY
	"TF_COND_TMPDAMAGEBONUS",
	"TF_COND_FEIGN_DEATH",
	"TF_COND_PHASE",
	"TF_COND_STUNNED",		// Any type of stun. Check iStunFlags for more info.
	"TF_COND_OFFENSEBUFF",
	"TF_COND_SHIELD_CHARGE",
	"TF_COND_DEMO_BUFF",
	"TF_COND_ENERGY_BUFF",
	"TF_COND_RADIUSHEAL",
	"TF_COND_HEALTH_BUFF",
	"TF_COND_BURNING",
	"TF_COND_HEALTH_OVERHEALED",
	"TF_COND_URINE",
	"TF_COND_BLEEDING",
	"TF_COND_DEFENSEBUFF",	// 35% defense! No crit damage.
	"TF_COND_MAD_MILK",
	"TF_COND_MEGAHEAL",
	"TF_COND_REGENONDAMAGEBUFF",
	"TF_COND_MARKEDFORDEATH",
	"TF_COND_NOHEALINGDAMAGEBUFF",
	"TF_COND_SPEED_BOOST",				// = 32
	"TF_COND_CRITBOOSTED_PUMPKIN",		// Brandon hates bits
	"TF_COND_CRITBOOSTED_USER_BUFF",
	"TF_COND_CRITBOOSTED_DEMO_CHARGE",
	"TF_COND_SODAPOPPER_HYPE",
	"TF_COND_CRITBOOSTED_FIRST_BLOOD",	// arena mode first blood
	"TF_COND_CRITBOOSTED_BONUS_TIME",
	"TF_COND_CRITBOOSTED_CTF_CAPTURE",
	"TF_COND_CRITBOOSTED_ON_KILL",		// =40. KGB, etc.
	"TF_COND_CANNOT_SWITCH_FROM_MELEE",
	"TF_COND_DEFENSEBUFF_NO_CRIT_BLOCK",	// 35% defense! Still damaged by crits.
	"TF_COND_REPROGRAMMED",				// Bots only
	"TF_COND_CRITBOOSTED_RAGE_BUFF",
	"TF_COND_DEFENSEBUFF_HIGH",			// 75% defense! Still damaged by crits.
	"TF_COND_SNIPERCHARGE_RAGE_BUFF",		// Sniper Rage - Charge time speed up
	"TF_COND_DISGUISE_WEARINGOFF",		// Applied for half-second post-disguise
	"TF_COND_MARKEDFORDEATH_SILENT",		// Sans sound
	"TF_COND_DISGUISED_AS_DISPENSER",
	"TF_COND_SAPPED",						// =50. Bots only
	"TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED",
	"TF_COND_INVULNERABLE_USER_BUFF",
	"TF_COND_HALLOWEEN_BOMB_HEAD",
	"TF_COND_HALLOWEEN_THRILLER",
	"TF_COND_RADIUSHEAL_ON_DAMAGE",
	"TF_COND_CRITBOOSTED_CARD_EFFECT",
	"TF_COND_INVULNERABLE_CARD_EFFECT",
	"TF_COND_MEDIGUN_UBER_BULLET_RESIST",
	"TF_COND_MEDIGUN_UBER_BLAST_RESIST",
	"TF_COND_MEDIGUN_UBER_FIRE_RESIST",		// =60
	"TF_COND_MEDIGUN_SMALL_BULLET_RESIST",
	"TF_COND_MEDIGUN_SMALL_BLAST_RESIST",
	"TF_COND_MEDIGUN_SMALL_FIRE_RESIST",
	"TF_COND_STEALTHED_USER_BUFF",			// Any class can have this
	"TF_COND_MEDIGUN_DEBUFF",
	"TF_COND_STEALTHED_USER_BUFF_FADING",
	"TF_COND_BULLET_IMMUNE",
	"TF_COND_BLAST_IMMUNE",
	"TF_COND_FIRE_IMMUNE",
	"TF_COND_PREVENT_DEATH",					// =70
	"TF_COND_MVM_BOT_STUN_RADIOWAVE", 		// Bots only
	"TF_COND_HALLOWEEN_SPEED_BOOST",
	"TF_COND_HALLOWEEN_QUICK_HEAL",
	"TF_COND_HALLOWEEN_GIANT",
	"TF_COND_HALLOWEEN_TINY",
	"TF_COND_HALLOWEEN_IN_HELL",
	"TF_COND_HALLOWEEN_GHOST_MODE",			// =77
	"TF_COND_MINICRITBOOSTED_ON_KILL",
	"TF_COND_OBSCURED_SMOKE",
	"TF_COND_PARACHUTE_DEPLOYED",				// =80
	"TF_COND_BLASTJUMPING",
	"TF_COND_HALLOWEEN_KART",
	"TF_COND_HALLOWEEN_KART_DASH",
	"TF_COND_BALLOON_HEAD",					// =84 larger head, lower-gravity-feeling jumps
	"TF_COND_MELEE_ONLY",						// =85 melee only
	"TF_COND_SWIMMING_CURSE",					// player movement become swimming movement
	"TF_COND_FREEZE_INPUT",					// freezes player input
	"TF_COND_HALLOWEEN_KART_CAGE",			// attach cage model to player while in kart
	"TF_COND_DONOTUSE_0",
	"TF_COND_RUNE_STRENGTH",
	"TF_COND_RUNE_HASTE",
	"TF_COND_RUNE_REGEN",
	"TF_COND_RUNE_RESIST",
	"TF_COND_RUNE_VAMPIRE",
	"TF_COND_RUNE_REFLECT",
	"TF_COND_RUNE_PRECISION",
	"TF_COND_RUNE_AGILITY",
	"TF_COND_GRAPPLINGHOOK",
	"TF_COND_GRAPPLINGHOOK_SAFEFALL",
	"TF_COND_GRAPPLINGHOOK_LATCHED",
	"TF_COND_GRAPPLINGHOOK_BLEEDING",
	"TF_COND_AFTERBURN_IMMUNE",
	"TF_COND_RUNE_KNOCKOUT",
	"TF_COND_RUNE_IMBALANCE",
	"TF_COND_CRITBOOSTED_RUNE_TEMP",
	"TF_COND_PASSTIME_INTERCEPTION",
	"TF_COND_SWIMMING_NO_EFFECTS",			// =107_DNOC_FT
	"TF_COND_PURGATORY",
	"TF_COND_RUNE_KING",
	"TF_COND_RUNE_PLAGUE",
	"TF_COND_RUNE_SUPERNOVA",
	"TF_COND_PLAGUE",
	"TF_COND_KING_BUFFED",
	"TF_COND_TEAM_GLOWS",					// used to show team glows to living players
	"TF_COND_KNOCKED_INTO_AIR",
	"TF_COND_COMPETITIVE_WINNER",
	"TF_COND_COMPETITIVE_LOSER",
	"TF_COND_HEALING_DEBUFF",
	"TF_COND_PASSTIME_PENALTY_DEBUFF",
	"TF_COND_GRAPPLED_TO_PLAYER",
	"TF_COND_GRAPPLED_BY_PLAYER",

	//
	// ADD NEW ITEMS HERE TO AVOID BREAKING DEMOS
	//

	// ******** Keep this block last! ********
	// Keep experimental conditions below and graduate out of it before shipping
#ifdef STAGING_ONLY
	"TF_COND_NO_COMBAT_SPEED_BOOST",		// STAGING_ENGY
	"TF_COND_TRANQ_SPY_BOOST",			// STAGING_SPY
	"TF_COND_TRANQ_MARKED",
//	"TF_COND_SPACE_GRAVITY",
//	"TF_COND_SELF_CONC",
	"TF_COND_ROCKETPACK",
	"TF_COND_STEALTHED_PHASE",  	
	"TF_COND_CLIP_OVERLOAD",
	"TF_COND_SPY_CLASS_STEAL",
#endif // STAGING_ONLY
};
COMPILE_TIME_ASSERT( ARRAYSIZE( g_aConditionNames ) == TF_COND_LAST );

const char *GetTFConditionName( ETFCond eCond )
{
	if ( ( eCond >= ARRAYSIZE( g_aConditionNames ) ) || ( eCond < 0 ) )
		return NULL;

	return g_aConditionNames[eCond];
}


ETFCond GetTFConditionFromName( const char *pszCondName )
{
	for( uint i=0; i<TF_COND_LAST; i++ )
	{ 
		ETFCond eCond = (ETFCond)i;
		if ( !V_stricmp( GetTFConditionName( eCond ), pszCondName ) ) 
			return eCond;
	} 

	Assert( !!"Invalid Condition Name" );
	return TF_COND_INVALID;
}

//-----------------------------------------------------------------------------
// Gametypes.
//-----------------------------------------------------------------------------
const char *g_aGameTypeNames[] =
{
	"Undefined",
	"#Gametype_CTF",
	"#Gametype_CP",
};

//-----------------------------------------------------------------------------
// Ammo.
//-----------------------------------------------------------------------------
const char *g_aAmmoNames[GAME_AMMO_COUNT] =
{
	"DUMMY AMMO",
	"TF_AMMO_PRIMARY",
	"TF_AMMO_SECONDARY",
	"TF_AMMO_METAL",
	"TF_AMMO_GRENADES1",
	"TF_AMMO_GRENADES2",
	"DOD_AMMO_SUBMG",
	"DOD_AMMO_ROCKET",
	"DOD_AMMO_COLT",
	"DOD_AMMO_P38",
	"DOD_AMMO_C96",
	"DOD_AMMO_WEBLEY",
	"DOD_AMMO_GARAND",
	"DOD_AMMO_K98",
	"DOD_AMMO_M1CARBINE",
	"DOD_AMMO_ENFIELD",
	"DOD_AMMO_SPRING",
	"DOD_AMMO_FG42",
	"DOD_AMMO_BREN",
	"DOD_AMMO_BAR",
	"DOD_AMMO_30CAL",
	"DOD_AMMO_MG34",
	"DOD_AMMO_MG42",
	"DOD_AMMO_HANDGRENADE",
	"DOD_AMMO_HANDGRENADE_EX",
	"DOD_AMMO_STICKGRENADE",
	"DOD_AMMO_STICKGRENADE_EX",
	"DOD_AMMO_SMOKEGRENADE_US",
	"DOD_AMMO_SMOKEGRENADE_GER",
	"DOD_AMMO_SMOKEGRENADE_US_LIVE",
	"DOD_AMMO_SMOKEGRENADE_GER_LIVE",
	"DOD_AMMO_RIFLEGRENADE_US",
	"DOD_AMMO_RIFLEGRENADE_GER",
	"DOD_AMMO_RIFLEGRENADE_US_LIVE",
	"DOD_AMMO_RIFLEGRENADE_GER_LIVE",
	"BULLET_PLAYER_50AE",
	"BULLET_PLAYER_762MM",
	"BULLET_PLAYER_556MM",
	"BULLET_PLAYER_556MM_BOX",
	"BULLET_PLAYER_338MAG",
	"BULLET_PLAYER_9MM",
	"BULLET_PLAYER_BUCKSHOT",
	"BULLET_PLAYER_45ACP",
	"BULLET_PLAYER_357SIG",
	"BULLET_PLAYER_57MM",
	"AMMO_TYPE_HEGRENADE",
	"AMMO_TYPE_FLASHBANG",
	"AMMO_TYPE_SMOKEGRENADE",
	"HL2_AMMO_AR2",
	"HL2_AMMO_AR2ALTFIRE",
	"HL2_AMMO_PISTOL",
	"HL2_AMMO_SMG1",
	"HL2_AMMO_357",
	"HL2_AMMO_XBOWBOLT",
	"HL2_AMMO_BUCKSHOT",
	"HL2_AMMO_RPGROUND",
	"HL2_AMMO_SMG1GRENADE",
	"HL2_AMMO_GRENADE",
	"HL2_AMMO_SLAM",
};

//-----------------------------------------------------------------------------
// Weapons.
//-----------------------------------------------------------------------------
const char *g_aWeaponNames[] =
{
	"TF_WEAPON_NONE",
	"TF_WEAPON_BAT",
	"TF_WEAPON_BOTTLE", 
	"TF_WEAPON_FIREAXE",
	"TF_WEAPON_CLUB",
	"TF_WEAPON_CROWBAR",
	"TF_WEAPON_KNIFE",
	"TF_WEAPON_FISTS",
	"TF_WEAPON_SHOVEL",
	"TF_WEAPON_WRENCH",
	"TF_WEAPON_BONESAW",
	"TF_WEAPON_SHOTGUN_PRIMARY",
	"TF_WEAPON_SHOTGUN_SOLDIER",
	"TF_WEAPON_SHOTGUN_HWG",
	"TF_WEAPON_SHOTGUN_PYRO",
	"TF_WEAPON_SCATTERGUN",
	"TF_WEAPON_SNIPERRIFLE",
	"TF_WEAPON_MINIGUN",
	"TF_WEAPON_SMG",
	"TF_WEAPON_SYRINGEGUN_MEDIC",
	"TF_WEAPON_TRANQ",
	"TF_WEAPON_ROCKETLAUNCHER",
	"TF_WEAPON_GRENADELAUNCHER",
	"TF_WEAPON_PIPEBOMBLAUNCHER",
	"TF_WEAPON_FLAMETHROWER",
	"TF_WEAPON_GRENADE_NORMAL",
	"TF_WEAPON_GRENADE_CONCUSSION",
	"TF_WEAPON_GRENADE_NAIL",
	"TF_WEAPON_GRENADE_MIRV",
	"TF_WEAPON_GRENADE_MIRV_DEMOMAN",
	"TF_WEAPON_GRENADE_NAPALM",
	"TF_WEAPON_GRENADE_GAS",
	"TF_WEAPON_GRENADE_EMP",
	"TF_WEAPON_GRENADE_CALTROP",
	"TF_WEAPON_GRENADE_PIPEBOMB",
	"TF_WEAPON_GRENADE_SMOKE_BOMB",
	"TF_WEAPON_GRENADE_HEAL",
	"TF_WEAPON_PISTOL",
	"TF_WEAPON_PISTOL_SCOUT",
	"TF_WEAPON_REVOLVER",
	"TF_WEAPON_NAILGUN",
	"TF_WEAPON_PDA",
	"TF_WEAPON_PDA_ENGINEER_BUILD",
	"TF_WEAPON_PDA_ENGINEER_DESTROY",
	"TF_WEAPON_PDA_SPY",
	"TF_WEAPON_BUILDER",
	"TF_WEAPON_MEDIGUN",
	"TF_WEAPON_GRENADE_MIRVBOMB",
	"TF_WEAPON_FLAMETHROWER_ROCKET",
	"TF_WEAPON_GRENADE_DEMOMAN",
	"TF_WEAPON_SENTRY_BULLET",
	"TF_WEAPON_SENTRY_ROCKET",
	"TF_WEAPON_DISPENSER",
	"TF_WEAPON_INVIS",
	"PC_WEAPON_AIGIS_ARMS",

	"TF_WEAPON_COUNT",

	// DOD Weapons Start
	//Melee
	"DOD_WEAPON_AMERKNIFE",
	"DOD_WEAPON_SPADE",

	//Pistols
	"DOD_WEAPON_COLT",
	"DOD_WEAPON_P38",
	"DOD_WEAPON_C96",

	//Rifles
	"DOD_WEAPON_GARAND",
	"DOD_WEAPON_M1CARBINE",
	"DOD_WEAPON_K98",

	//Sniper Rifles
	"DOD_WEAPON_SPRING",
	"DOD_WEAPON_K98_SCOPED",

	//SMG
	"DOD_WEAPON_THOMPSON",
	"DOD_WEAPON_MP40",
	"DOD_WEAPON_MP44",
	"DOD_WEAPON_BAR",

	//Machine guns
	"DOD_WEAPON_30CAL",
	"DOD_WEAPON_MG42",

	//Rocket weapons
	"DOD_WEAPON_BAZOOKA",
	"DOD_WEAPON_PSCHRECK",

	//Grenades
	"DOD_WEAPON_FRAG_US",
	"DOD_WEAPON_FRAG_GER",

	"DOD_WEAPON_FRAG_US_LIVE",
	"DOD_WEAPON_FRAG_GER_LIVE",

	"DOD_WEAPON_SMOKE_US",
	"DOD_WEAPON_SMOKE_GER",

	"DOD_WEAPON_RIFLEGREN_US",
	"DOD_WEAPON_RIFLEGREN_GER",

	"DOD_WEAPON_RIFLEGREN_US_LIVE",
	"DOD_WEAPON_RIFLEGREN_GER_LIVE",

	// not actually separate weapons, but defines used in stats recording
	"DOD_WEAPON_THOMPSON_PUNCH",
	"DOD_WEAPON_MP40_PUNCH",
	"DOD_WEAPON_GARAND_ZOOMED",

	"DOD_WEAPON_K98_ZOOMED",
	"DOD_WEAPON_SPRING_ZOOMED",
	"DOD_WEAPON_K98_SCOPED_ZOOMED",

	"DOD_WEAPON_30CAL_UNDEPLOYED",
	"DOD_WEAPON_MG42_UNDEPLOYED",

	"DOD_WEAPON_BAR_SEMIAUTO",
	"DOD_WEAPON_MP44_SEMIAUTO",
	"DOD_WEAPON_COUNT",
	// DOD Weapons End

	// CS Weapons Start
	"CS_WEAPON_P228",
	"CS_WEAPON_GLOCK",
	"CS_WEAPON_SCOUT",
	"CS_WEAPON_HEGRENADE",
	"CS_WEAPON_XM1014",
	"CS_WEAPON_C4",
	"CS_WEAPON_MAC10",
	"CS_WEAPON_AUG",
	"CS_WEAPON_SMOKEGRENADE",
	"CS_WEAPON_ELITE",
	"CS_WEAPON_FIVESEVEN",
	"CS_WEAPON_UMP45",
	"CS_WEAPON_SG550",

	"CS_WEAPON_GALIL",
	"CS_WEAPON_FAMAS",
	"CS_WEAPON_USP",
	"CS_WEAPON_AWP",
	"CS_WEAPON_MP5NAVY",
	"CS_WEAPON_M249",
	"CS_WEAPON_M3",
	"CS_WEAPON_M4A1",
	"CS_WEAPON_TMP",
	"CS_WEAPON_G3SG1",
	"CS_WEAPON_FLASHBANG",
	"CS_WEAPON_DEAGLE",
	"CS_WEAPON_SG552",
	"CS_WEAPON_AK47",
	"CS_WEAPON_KNIFE",
	"CS_WEAPON_P90",

	"CS_WEAPON_SHIELDGUN",	// BOTPORT: Is this still needed?

	"CS_WEAPON_KEVLAR",
	"CS_WEAPON_ASSAULTSUIT",
	"CS_WEAPON_NVG",

	"CS_WEAPON_COUNT",
	// CS Weapons End

	"HL2_WEAPON_PISTOL",
	"HL2_WEAPON_SMG1",
	"HL2_WEAPON_PHYSCANNON",

	"HL2_WEAPON_COUNT",
};
COMPILE_TIME_ASSERT( ARRAYSIZE( g_aWeaponNames ) == GAME_WEAPON_COUNT );

int g_aWeaponDamageTypes[] =
{
	DMG_GENERIC,	// TF_WEAPON_NONE
	DMG_CLUB,		// TF_WEAPON_BAT,
	DMG_CLUB,		// TF_WEAPON_BOTTLE, 
	DMG_CLUB,		// TF_WEAPON_FIREAXE,
	DMG_CLUB,		// TF_WEAPON_CLUB,
	DMG_CLUB,		// TF_WEAPON_CROWBAR,
	DMG_SLASH,		// TF_WEAPON_KNIFE,
	DMG_CLUB,		// TF_WEAPON_FISTS,
	DMG_CLUB,		// TF_WEAPON_SHOVEL,
	DMG_CLUB,		// TF_WEAPON_WRENCH,
	DMG_SLASH,		// TF_WEAPON_BONESAW,
	DMG_BUCKSHOT | DMG_USEDISTANCEMOD,	// TF_WEAPON_SHOTGUN_PRIMARY,
	DMG_BUCKSHOT | DMG_USEDISTANCEMOD,	// TF_WEAPON_SHOTGUN_SOLDIER,
	DMG_BUCKSHOT | DMG_USEDISTANCEMOD,	// TF_WEAPON_SHOTGUN_HWG,
	DMG_BUCKSHOT | DMG_USEDISTANCEMOD,	// TF_WEAPON_SHOTGUN_PYRO,
	DMG_BUCKSHOT | DMG_USEDISTANCEMOD,  // TF_WEAPON_SCATTERGUN,
	DMG_BULLET | DMG_USE_HITLOCATIONS,	// TF_WEAPON_SNIPERRIFLE,
	DMG_BULLET | DMG_USEDISTANCEMOD,		// TF_WEAPON_MINIGUN,
	DMG_BULLET | DMG_USEDISTANCEMOD,		// TF_WEAPON_SMG,
	DMG_BULLET | DMG_USEDISTANCEMOD | DMG_NOCLOSEDISTANCEMOD | DMG_PREVENT_PHYSICS_FORCE,		// TF_WEAPON_SYRINGEGUN_MEDIC,
	DMG_BULLET | DMG_USEDISTANCEMOD | DMG_PREVENT_PHYSICS_FORCE | DMG_PARALYZE,		// TF_WEAPON_TRANQ,
	DMG_BLAST | DMG_HALF_FALLOFF | DMG_USEDISTANCEMOD,		// TF_WEAPON_ROCKETLAUNCHER,
	DMG_BLAST | DMG_HALF_FALLOFF | DMG_USEDISTANCEMOD,		// TF_WEAPON_GRENADELAUNCHER,
	DMG_BLAST | DMG_HALF_FALLOFF,		// TF_WEAPON_PIPEBOMBLAUNCHER,
	DMG_IGNITE | DMG_PREVENT_PHYSICS_FORCE | DMG_PREVENT_PHYSICS_FORCE,		// TF_WEAPON_FLAMETHROWER,
	DMG_BLAST | DMG_HALF_FALLOFF,		// TF_WEAPON_GRENADE_NORMAL,
	DMG_SONIC | DMG_HALF_FALLOFF,		// TF_WEAPON_GRENADE_CONCUSSION,
	DMG_BULLET | DMG_HALF_FALLOFF,		// TF_WEAPON_GRENADE_NAIL,
	DMG_BLAST | DMG_HALF_FALLOFF,		// TF_WEAPON_GRENADE_MIRV,
	DMG_BLAST | DMG_HALF_FALLOFF,		// TF_WEAPON_GRENADE_MIRV_DEMOMAN,
	DMG_BURN | DMG_RADIUS_MAX,		// TF_WEAPON_GRENADE_NAPALM,
	DMG_POISON | DMG_HALF_FALLOFF,		// TF_WEAPON_GRENADE_GAS,
	DMG_BLAST | DMG_HALF_FALLOFF | DMG_PREVENT_PHYSICS_FORCE,		// TF_WEAPON_GRENADE_EMP,
	DMG_GENERIC,	// TF_WEAPON_GRENADE_CALTROP,
	DMG_BLAST | DMG_HALF_FALLOFF,		// TF_WEAPON_GRENADE_PIPEBOMB,
	DMG_GENERIC,	// TF_WEAPON_GRENADE_SMOKE_BOMB,
	DMG_GENERIC,	// TF_WEAPON_GRENADE_HEAL
	DMG_BULLET | DMG_USEDISTANCEMOD,		// TF_WEAPON_PISTOL,
	DMG_BULLET | DMG_USEDISTANCEMOD,		// TF_WEAPON_PISTOL_SCOUT,
	DMG_BULLET | DMG_USEDISTANCEMOD,		// TF_WEAPON_REVOLVER,
	DMG_BULLET | DMG_USEDISTANCEMOD | DMG_NOCLOSEDISTANCEMOD | DMG_PREVENT_PHYSICS_FORCE,		// TF_WEAPON_NAILGUN,
	DMG_BULLET,		// TF_WEAPON_PDA,
	DMG_BULLET,		// TF_WEAPON_PDA_ENGINEER_BUILD,
	DMG_BULLET,		// TF_WEAPON_PDA_ENGINEER_DESTROY,
	DMG_BULLET,		// TF_WEAPON_PDA_SPY,
	DMG_BULLET,		// TF_WEAPON_BUILDER
	DMG_BULLET,		// TF_WEAPON_MEDIGUN
	DMG_BLAST,		// TF_WEAPON_GRENADE_MIRVBOMB
	DMG_BLAST | DMG_IGNITE | DMG_RADIUS_MAX,		// TF_WEAPON_FLAMETHROWER_ROCKET
	DMG_BLAST | DMG_HALF_FALLOFF,					// TF_WEAPON_GRENADE_DEMOMAN
	DMG_GENERIC,	// TF_WEAPON_SENTRY_BULLET
	DMG_GENERIC,	// TF_WEAPON_SENTRY_ROCKET
	DMG_GENERIC,	// TF_WEAPON_DISPENSER
	DMG_GENERIC,	// TF_WEAPON_INVIS
	DMG_BULLET | DMG_USEDISTANCEMOD,		// PC_WEAPON_AIGIS_ARMS
};

const char *g_szSpecialDamageNames[] =
{
	"",
	"TF_DMG_CUSTOM_HEADSHOT",
	"TF_DMG_CUSTOM_BACKSTAB",
	"TF_DMG_CUSTOM_BURNING",
	"TF_DMG_WRENCH_FIX",
	"TF_DMG_CUSTOM_MINIGUN",
	"TF_DMG_CUSTOM_SUICIDE",
	"TF_DMG_CUSTOM_TAUNTATK_HADOUKEN",
	"TF_DMG_CUSTOM_BURNING_FLARE",
	"TF_DMG_CUSTOM_TAUNTATK_HIGH_NOON",
	"TF_DMG_CUSTOM_TAUNTATK_GRAND_SLAM",
	"TF_DMG_CUSTOM_PENETRATE_MY_TEAM",
	"TF_DMG_CUSTOM_PENETRATE_ALL_PLAYERS",
	"TF_DMG_CUSTOM_TAUNTATK_FENCING",
	"TF_DMG_CUSTOM_PENETRATE_NONBURNING_TEAMMATE",
	"TF_DMG_CUSTOM_TAUNTATK_ARROW_STAB",
	"TF_DMG_CUSTOM_TELEFRAG",
	"TF_DMG_CUSTOM_BURNING_ARROW",
	"TF_DMG_CUSTOM_FLYINGBURN",
	"TF_DMG_CUSTOM_PUMPKIN_BOMB",
	"TF_DMG_CUSTOM_DECAPITATION",
	"TF_DMG_CUSTOM_TAUNTATK_GRENADE",
	"TF_DMG_CUSTOM_BASEBALL",
	"TF_DMG_CUSTOM_CHARGE_IMPACT",
	"TF_DMG_CUSTOM_TAUNTATK_BARBARIAN_SWING",
	"TF_DMG_CUSTOM_AIR_STICKY_BURST",
	"TF_DMG_CUSTOM_DEFENSIVE_STICKY",
	"TF_DMG_CUSTOM_PICKAXE",
	"TF_DMG_CUSTOM_ROCKET_DIRECTHIT",
	"TF_DMG_CUSTOM_TAUNTATK_UBERSLICE",
	"TF_DMG_CUSTOM_PLAYER_SENTRY",
	"TF_DMG_CUSTOM_STANDARD_STICKY",
	"TF_DMG_CUSTOM_SHOTGUN_REVENGE_CRIT",
	"TF_DMG_CUSTOM_TAUNTATK_ENGINEER_GUITAR_SMASH",
	"TF_DMG_CUSTOM_BLEEDING",
	"TF_DMG_CUSTOM_GOLD_WRENCH",
	"TF_DMG_CUSTOM_CARRIED_BUILDING",
	"TF_DMG_CUSTOM_COMBO_PUNCH",
	"TF_DMG_CUSTOM_TAUNTATK_ENGINEER_ARM_KILL",
	"TF_DMG_CUSTOM_FISH_KILL",
	"TF_DMG_CUSTOM_TRIGGER_HURT",
	"TF_DMG_CUSTOM_DECAPITATION_BOSS",
	"TF_DMG_CUSTOM_STICKBOMB_EXPLOSION",
	"TF_DMG_CUSTOM_AEGIS_ROUND",
	"TF_DMG_CUSTOM_FLARE_EXPLOSION",
	"TF_DMG_CUSTOM_BOOTS_STOMP",
	"TF_DMG_CUSTOM_PLASMA",
	"TF_DMG_CUSTOM_PLASMA_CHARGED",
	"TF_DMG_CUSTOM_PLASMA_GIB",
	"TF_DMG_CUSTOM_PRACTICE_STICKY",
	"TF_DMG_CUSTOM_EYEBALL_ROCKET",
	"TF_DMG_CUSTOM_HEADSHOT_DECAPITATION",
	"TF_DMG_CUSTOM_TAUNTATK_ARMAGEDDON",
	"TF_DMG_CUSTOM_FLARE_PELLET",
	"TF_DMG_CUSTOM_CLEAVER",
	"TF_DMG_CUSTOM_CLEAVER_CRIT",
	"TF_DMG_CUSTOM_SAPPER_RECORDER_DEATH",
	"TF_DMG_CUSTOM_MERASMUS_PLAYER_BOMB",
	"TF_DMG_CUSTOM_MERASMUS_GRENADE",
	"TF_DMG_CUSTOM_MERASMUS_ZAP",
	"TF_DMG_CUSTOM_MERASMUS_DECAPITATION",
	"TF_DMG_CUSTOM_CANNONBALL_PUSH",
	"TF_DMG_CUSTOM_TAUNTATK_ALLCLASS_GUITAR_RIFF",
	"TF_DMG_CUSTOM_THROWABLE",
	"TF_DMG_CUSTOM_THROWABLE_KILL",
	"TF_DMG_CUSTOM_SPELL_TELEPORT",
	"TF_DMG_CUSTOM_SPELL_SKELETON",
	"TF_DMG_CUSTOM_SPELL_MIRV",
	"TF_DMG_CUSTOM_SPELL_METEOR",
	"TF_DMG_CUSTOM_SPELL_LIGHTNING",
	"TF_DMG_CUSTOM_SPELL_FIREBALL",
	"TF_DMG_CUSTOM_SPELL_MONOCULUS",
	"TF_DMG_CUSTOM_SPELL_BLASTJUMP",
	"TF_DMG_CUSTOM_SPELL_BATS",
	"TF_DMG_CUSTOM_SPELL_TINY",
	"TF_DMG_CUSTOM_KART",
	"TF_DMG_CUSTOM_GIANT_HAMMER",
	"TF_DMG_CUSTOM_RUNE_REFLECT",
	"TF_DMG_CUSTOM_SLAP_KILL",

	"DOD_DMG_CUSTOM_HEADSHOT",
	"DOD_DMG_CUSTOM_MELEE_SECONDARYATTACK",
	"DOD_DMG_CUSTOM_MELEE_FIST",
	"DOD_DMG_CUSTOM_MELEE_EDGE",
	"DOD_DMG_CUSTOM_MELEE_STRONGATTACK",
};
COMPILE_TIME_ASSERT( ARRAYSIZE( g_szSpecialDamageNames ) == TF_DMG_CUSTOM_END );

const char *GetCustomDamageName( ETFDmgCustom eDmgCustom )
{
	if ( ( eDmgCustom >= ARRAYSIZE( g_szSpecialDamageNames ) ) || ( eDmgCustom < 0 ) )
		return NULL;

	return g_szSpecialDamageNames[eDmgCustom];
}

ETFDmgCustom GetCustomDamageFromName( const char *pszCustomDmgName )
{
	for( uint i=0; i<TF_DMG_CUSTOM_END; i++ )
	{ 
		ETFDmgCustom eDmgCustom = (ETFDmgCustom)i;
		if ( !V_stricmp( GetCustomDamageName( eDmgCustom ), pszCustomDmgName ) ) 
			return eDmgCustom;
	} 

	Assert( !!"Invalid Custom Damage Name" );
	return TF_DMG_CUSTOM_NONE;
}

const char *g_szProjectileNames[] =
{
	"",
	"projectile_bullet",
	"projectile_rocket",
	"projectile_pipe",
	"projectile_pipe_remote",
	"projectile_syringe",
	"projectile_sentry_rocket",
};
COMPILE_TIME_ASSERT( ARRAYSIZE( g_szProjectileNames ) == TF_NUM_PROJECTILES );

// these map to the projectiles named in g_szProjectileNames
int g_iProjectileWeapons[] = 
{
	TF_WEAPON_NONE,
	TF_WEAPON_PISTOL,
	TF_WEAPON_ROCKETLAUNCHER,
	TF_WEAPON_PIPEBOMBLAUNCHER,
	TF_WEAPON_GRENADELAUNCHER,
	TF_WEAPON_SYRINGEGUN_MEDIC,
	TF_WEAPON_SENTRY_ROCKET,
};
COMPILE_TIME_ASSERT( ARRAYSIZE( g_szProjectileNames ) == ARRAYSIZE( g_iProjectileWeapons ) );

const char *g_pszHintMessages[] =
{
	"#Hint_spotted_a_friend",
	"#Hint_spotted_an_enemy",
	"#Hint_killing_enemies_is_good",
	"#Hint_out_of_ammo",
	"#Hint_turn_off_hints",
	"#Hint_pickup_ammo",
	"#Hint_Cannot_Teleport_With_Flag",
	"#Hint_Cannot_Cloak_With_Flag",
	"#Hint_Cannot_Disguise_With_Flag",
	"#Hint_Cannot_Attack_While_Cloaked",
	"#Hint_ClassMenu",

// Grenades
	"#Hint_gren_caltrops",
	"#Hint_gren_concussion",
	"#Hint_gren_emp",
	"#Hint_gren_gas",
	"#Hint_gren_mirv",
	"#Hint_gren_nail",
	"#Hint_gren_napalm",
	"#Hint_gren_normal",

// Altfires
	"#Hint_altfire_sniperrifle",
	"#Hint_altfire_flamethrower",
	"#Hint_altfire_grenadelauncher",
	"#Hint_altfire_pipebomblauncher",
	"#Hint_altfire_rotate_building",

// Soldier
	"#Hint_Soldier_rpg_reload",

// Engineer
	"#Hint_Engineer_use_wrench_onown",
	"#Hint_Engineer_use_wrench_onother",
	"#Hint_Engineer_use_wrench_onfriend",
	"#Hint_Engineer_build_sentrygun",
	"#Hint_Engineer_build_dispenser",
	"#Hint_Engineer_build_teleporters",
	"#Hint_Engineer_pickup_metal",
	"#Hint_Engineer_repair_object",
	"#Hint_Engineer_metal_to_upgrade",
	"#Hint_Engineer_upgrade_sentrygun",

	"#Hint_object_has_sapper",

	"#Hint_object_your_object_sapped",
	"#Hint_enemy_using_dispenser",
	"#Hint_enemy_using_tp_entrance",
	"#Hint_enemy_using_tp_exit",
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int GetWeaponId( const char *pszWeaponName )
{
	// if this doesn't match, you need to add missing weapons to the array
	COMPILE_TIME_ASSERT( GAME_WEAPON_COUNT == ARRAYSIZE( g_aWeaponNames ) );

	for ( int iWeapon = 0; iWeapon < ARRAYSIZE( g_aWeaponNames ); ++iWeapon )
	{
		if ( !Q_stricmp( pszWeaponName, g_aWeaponNames[iWeapon] ) )
			return iWeapon;
	}

	return TF_WEAPON_NONE;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *WeaponIdToAlias( int iWeapon )
{
	// if this doesn't match, you need to add missing weapons to the array
	COMPILE_TIME_ASSERT( GAME_WEAPON_COUNT == ARRAYSIZE( g_aWeaponNames ) );

	if ( ( iWeapon >= ARRAYSIZE( g_aWeaponNames ) ) || ( iWeapon < 0 ) )
		return NULL;

	return g_aWeaponNames[iWeapon];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *WeaponIdToClassname( int iWeapon )
{
	const char *pszAlias = WeaponIdToAlias( iWeapon );
	if ( pszAlias == NULL )
		return NULL;

	static char szClassname[128];
	V_strncpy( szClassname, pszAlias, sizeof( szClassname ) );
	V_strlower( szClassname );

	return szClassname;
}

int AliasToWeaponId( const char *alias )
{
	if ( alias )
	{
		for( int i=0; g_aWeaponNames[i] != NULL; ++i )
			if (!Q_stricmp( g_aWeaponNames[i], alias ))
				return i;
	}

	return TF_WEAPON_NONE;
}

#ifdef GAME_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int GetWeaponFromDamage( const CTakeDamageInfo &info )
{
	int iWeapon = TF_WEAPON_NONE;

	const char *killer_weapon_name = "";

	// Find the killer & the scorer
	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CBasePlayer *pScorer = TFGameRules()->GetDeathScorer( pKiller, pInflictor, NULL );

	// find the weapon the killer used

	if ( pScorer )	// Is the killer a client?
	{
		if ( pInflictor )
		{
			if ( pInflictor == pScorer )
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				if ( pScorer->GetActiveWeapon() )
				{
					killer_weapon_name = pScorer->GetActiveWeapon()->GetClassname();
				}
			}
			else
			{
				killer_weapon_name = STRING( pInflictor->m_iClassname );  // it's just that easy
			}
		}
	}
	else if ( pInflictor )
	{
		killer_weapon_name = STRING( pInflictor->m_iClassname );
	}

	if ( !Q_strnicmp( killer_weapon_name, "tf_projectile", 13 ) )
	{
		for( int i = 0; i < ARRAYSIZE( g_szProjectileNames ); i++ )
		{
			if ( !Q_stricmp( &killer_weapon_name[ 3 ], g_szProjectileNames[ i ] ) )
			{
				iWeapon = g_iProjectileWeapons[ i ];
				break;
			}
		}
	}
	else
	{
		int iLen = Q_strlen( killer_weapon_name );

		// strip off _projectile from projectiles shot from other projectiles
		if ( ( iLen < 256 ) && ( iLen > 11 ) && !Q_stricmp( &killer_weapon_name[ iLen - 11 ], "_projectile" ) )
		{
			char temp[ 256 ];
			V_strcpy_safe( temp, killer_weapon_name );
			temp[ iLen - 11 ] = 0;

			// set the weapon used
			iWeapon = GetWeaponId( temp );
		}
		else
		{
			// set the weapon used
			iWeapon = GetWeaponId( killer_weapon_name );
		}
	}

	AssertMsg( iWeapon >= 0 && iWeapon < TF_WEAPON_COUNT, "Referencing a weapon not in tf_shareddefs.h.  Check to make it's defined and it's mapped correctly in g_szProjectileNames and g_iProjectileWeapons." );
	return iWeapon;
}

#endif

// ------------------------------------------------------------------------------------------------ //
// CObjectInfo tables.
// ------------------------------------------------------------------------------------------------ //

CObjectInfo::CObjectInfo( const char *pObjectName )
{
	m_pObjectName = pObjectName;
	m_pClassName = NULL;
	m_flBuildTime = -9999;
	m_nMaxObjects = -9999;
	m_Cost = -9999;
	m_CostMultiplierPerInstance = -999;
	m_flUpgradeDuration = -999;
	m_UpgradeCost = -9999;
	m_MaxUpgradeLevel = -9999;
	m_pBuilderWeaponName = NULL;
	m_pBuilderPlacementString = NULL;
	m_SelectionSlot = -9999;
	m_SelectionPosition = -9999;
	m_bSolidToPlayerMovement = false;
	m_pIconActive = NULL;
	m_pIconInactive = NULL;
	m_pIconMenu = NULL;
	m_pViewModel = NULL;
	m_pPlayerModel = NULL;
	m_iDisplayPriority = 0;
	m_bVisibleInWeaponSelection = true;
	m_pExplodeSound = NULL;
	m_pUpgradeSound = NULL;
	m_pExplosionParticleEffect = NULL;
	m_bAutoSwitchTo = false;
	m_iBuildCount = 0;
	m_iNumAltModes = 0;
	m_bRequiresOwnBuilder = false;
}


CObjectInfo::~CObjectInfo()
{
	delete [] m_pClassName;
	delete [] m_pStatusName;
	delete [] m_pBuilderWeaponName;
	delete [] m_pBuilderPlacementString;
	delete [] m_pIconActive;
	delete [] m_pIconInactive;
	delete [] m_pIconMenu;
	delete [] m_pViewModel;
	delete [] m_pPlayerModel;
	delete [] m_pExplodeSound;
	delete [] m_pUpgradeSound;
	delete [] m_pExplosionParticleEffect;
}

CObjectInfo g_ObjectInfos[OBJ_LAST] =
{
	CObjectInfo( "OBJ_DISPENSER" ),
	CObjectInfo( "OBJ_TELEPORTER" ),
	CObjectInfo( "OBJ_SENTRYGUN" ),
	CObjectInfo( "OBJ_ATTACHMENT_SAPPER" ),
};
COMPILE_TIME_ASSERT( ARRAYSIZE( g_ObjectInfos ) == OBJ_LAST );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int GetBuildableId( const char *pszBuildableName )
{
	for ( int iBuildable = 0; iBuildable < OBJ_LAST; ++iBuildable )
	{
		if ( !Q_stricmp( pszBuildableName, g_ObjectInfos[iBuildable].m_pObjectName ) )
			return iBuildable;
	}

	return OBJ_LAST;
}

bool AreObjectInfosLoaded()
{
	return g_ObjectInfos[0].m_pClassName != NULL;
}

static void SpewFileInfo( IBaseFileSystem *pFileSystem, const char *resourceName, const char *pathID, KeyValues *pValues )
{
	bool bFileExists = pFileSystem->FileExists( resourceName, pathID );
	bool bFileWritable = pFileSystem->IsFileWritable( resourceName, pathID );
	unsigned int nSize = pFileSystem->Size( resourceName, pathID );

	Msg( "resourceName:%s pathID:%s bFileExists:%d size:%u writeable:%d\n", resourceName, pathID, bFileExists, nSize, bFileWritable );

	unsigned int filesize = ( unsigned int )-1;
	FileHandle_t f = filesystem->Open( resourceName, "rb", pathID );
	if ( f )
	{
		filesize = filesystem->Size( f );
		filesystem->Close( f );
	}

	Msg( " FileHandle_t:%p size:%u\n", f, filesize );

	IFileSystem *pFS = 	(IFileSystem *)filesystem;

	char cwd[ MAX_PATH ];
	cwd[ 0 ] = 0;
	pFS->GetCurrentDirectory( cwd, ARRAYSIZE( cwd ) );

	bool bAvailable = pFS->IsFileImmediatelyAvailable( resourceName );

	Msg( " IsFileImmediatelyAvailable:%d cwd:%s\n", bAvailable, cwd );

	pFS->PrintSearchPaths();

	if ( pValues )
	{
		Msg( "Keys:" );
		KeyValuesDumpAsDevMsg( pValues, 2, 0 );
	}
}

void LoadObjectInfos( IBaseFileSystem *pFileSystem )
{
	const char *pFilename = "scripts/objects.txt";

	// Make sure this stuff hasn't already been loaded.
	Assert( !AreObjectInfosLoaded() );

	KeyValues *pValues = new KeyValues( "Object descriptions" );
	if ( !pValues->LoadFromFile( pFileSystem, pFilename, "GAME" ) )
	{
		// Getting "Can't open scripts/objects.txt for object info." errors. Spew file information
		//  before the Error() call which should show up in the minidumps.
		SpewFileInfo( pFileSystem, pFilename, "GAME", NULL );

		Error( "Can't open %s for object info.", pFilename );
		pValues->deleteThis();
		return;
	}

	// Now read each class's information in.
	for ( int iObj=0; iObj < ARRAYSIZE( g_ObjectInfos ); iObj++ )
	{
		CObjectInfo *pInfo = &g_ObjectInfos[iObj];
		KeyValues *pSub = pValues->FindKey( pInfo->m_pObjectName );
		if ( !pSub )
		{
			// Getting "Missing section 'OBJ_DISPENSER' from scripts/objects.txt" errors.
			SpewFileInfo( pFileSystem, pFilename, "GAME", pValues );

			// It seems that folks have corrupt files when these errors are seen in http://minidump.
			// Does it make sense to call the below Steam API so it'll force a validation next startup time?
			// Need to verify it's real corruption and not someone dorking around with their objects.txt file...
			//
			// From Martin Otten: If you have a file on disc and you’re 100% sure it’s
			//  corrupt, call ISteamApps::MarkContentCorrupt( false ), before you shutdown
			//  the game. This will cause a content validation in Steam.

			Error( "Missing section '%s' from %s.", pInfo->m_pObjectName, pFilename );
			pValues->deleteThis();
			return;
		}

		// Read all the info in.
		if ( (pInfo->m_flBuildTime = pSub->GetFloat( "BuildTime", -999 )) == -999 ||
			(pInfo->m_nMaxObjects = pSub->GetInt( "MaxObjects", -999 )) == -999 ||
			(pInfo->m_Cost = pSub->GetInt( "Cost", -999 )) == -999 ||
			(pInfo->m_CostMultiplierPerInstance = pSub->GetFloat( "CostMultiplier", -999 )) == -999 ||
			(pInfo->m_flUpgradeDuration = pSub->GetFloat( "UpgradeDuration", -999 )) == -999 ||
			(pInfo->m_UpgradeCost = pSub->GetInt( "UpgradeCost", -999 )) == -999 ||
			(pInfo->m_MaxUpgradeLevel = pSub->GetInt( "MaxUpgradeLevel", -999 )) == -999 ||
			(pInfo->m_SelectionSlot = pSub->GetInt( "SelectionSlot", -999 )) == -999 ||
			(pInfo->m_iBuildCount = pSub->GetInt( "BuildCount", -999 )) == -999 ||
			(pInfo->m_SelectionPosition = pSub->GetInt( "SelectionPosition", -999 )) == -999 )
		{
			SpewFileInfo( pFileSystem, pFilename, "GAME", pValues );

			Error( "Missing data for object '%s' in %s.", pInfo->m_pObjectName, pFilename );
			pValues->deleteThis();
			return;
		}

		pInfo->m_pClassName = ReadAndAllocStringValue( pSub, "ClassName", pFilename );
		pInfo->m_pStatusName = ReadAndAllocStringValue( pSub, "StatusName", pFilename );
		pInfo->m_pBuilderWeaponName = ReadAndAllocStringValue( pSub, "BuilderWeaponName", pFilename );
		pInfo->m_pBuilderPlacementString = ReadAndAllocStringValue( pSub, "BuilderPlacementString", pFilename );
		pInfo->m_bSolidToPlayerMovement = pSub->GetInt( "SolidToPlayerMovement", 0 ) ? true : false;
		pInfo->m_pIconActive = ReadAndAllocStringValue( pSub, "IconActive", pFilename );
		pInfo->m_pIconInactive = ReadAndAllocStringValue( pSub, "IconInactive", pFilename );
		pInfo->m_pIconMenu = ReadAndAllocStringValue( pSub, "IconMenu", pFilename );
		pInfo->m_bUseItemInfo = ( pSub->GetInt( "UseItemInfo", 0 ) > 0 );
		pInfo->m_pViewModel = ReadAndAllocStringValue( pSub, "Viewmodel", pFilename );
		pInfo->m_pPlayerModel = ReadAndAllocStringValue( pSub, "Playermodel", pFilename );
		pInfo->m_iDisplayPriority = pSub->GetInt( "DisplayPriority", 0 );
		pInfo->m_pHudStatusIcon = ReadAndAllocStringValue( pSub, "HudStatusIcon", pFilename );
		pInfo->m_bVisibleInWeaponSelection = ( pSub->GetInt( "VisibleInWeaponSelection", 1 ) > 0 );
		pInfo->m_pExplodeSound = ReadAndAllocStringValue( pSub, "ExplodeSound", pFilename );
		pInfo->m_pUpgradeSound = ReadAndAllocStringValue( pSub, "UpgradeSound", pFilename );
		pInfo->m_pExplosionParticleEffect = ReadAndAllocStringValue( pSub, "ExplodeEffect", pFilename );
		pInfo->m_bAutoSwitchTo = ( pSub->GetInt( "autoswitchto", 0 ) > 0 );
		pInfo->m_iMetalToDropInGibs = pSub->GetInt( "MetalToDropInGibs", 0 );
		pInfo->m_bRequiresOwnBuilder = pSub->GetBool( "RequiresOwnBuilder", 0 );

		// Read the other alternate object modes.
		KeyValues *pAltModesKey = pSub->FindKey( "AltModes" );
		if ( pAltModesKey )
		{
			int iIndex = 0;
			while ( iIndex<OBJECT_MAX_MODES )
			{
				char buf[256];
				Q_snprintf( buf, sizeof(buf), "AltMode%d", iIndex );
				KeyValues *pCurrentModeKey = pAltModesKey->FindKey( buf );
				if ( !pCurrentModeKey )
					break;

				pInfo->m_AltModes[iIndex].pszStatusName = ReadAndAllocStringValue( pCurrentModeKey, "StatusName", pFilename );
				pInfo->m_AltModes[iIndex].pszModeName   = ReadAndAllocStringValue( pCurrentModeKey, "ModeName",   pFilename );
				pInfo->m_AltModes[iIndex].pszIconMenu   = ReadAndAllocStringValue( pCurrentModeKey, "IconMenu",   pFilename );

				iIndex++;
			}
			pInfo->m_iNumAltModes = iIndex-1;
		}

		// Alternate mode 0 always matches the defaults.
		pInfo->m_AltModes[0].pszStatusName = pInfo->m_pStatusName;
		pInfo->m_AltModes[0].pszIconMenu   = pInfo->m_pIconMenu;
	}

	pValues->deleteThis();
}


const CObjectInfo* GetObjectInfo( int iObject )
{
	Assert( iObject >= 0 && iObject < OBJ_LAST );
	Assert( AreObjectInfosLoaded() );
	return &g_ObjectInfos[iObject];
}

ConVar tf_cheapobjects( "tf_cheapobjects","0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Set to 1 and all objects will cost 0" );

//-----------------------------------------------------------------------------
// Purpose: Return the cost of another object of the specified type
//			If bLast is set, return the cost of the last built object of the specified type
// 
// Note: Used to contain logic from tf2 that multiple instances of the same object
//       cost different amounts. See tf2/game_shared/tf_shareddefs.cpp for details
//-----------------------------------------------------------------------------
int InternalCalculateObjectCost( int iObjectType )
{
	if ( tf_cheapobjects.GetInt() )
	{
		return 0;
	}

	int iCost = GetObjectInfo( iObjectType )->m_Cost;

	return iCost;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the cost to upgrade an object of a specific type
//-----------------------------------------------------------------------------
int	CalculateObjectUpgrade( int iObjectType, int iObjectLevel )
{
	// Max level?
	if ( iObjectLevel >= GetObjectInfo( iObjectType )->m_MaxUpgradeLevel )
		return 0;

	int iCost = GetObjectInfo( iObjectType )->m_UpgradeCost;
	for ( int i = 0; i < (iObjectLevel - 1); i++ )
	{
		iCost *= OBJECT_UPGRADE_COST_MULTIPLIER_PER_LEVEL;
	}

	return iCost;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified class is allowed to build the specified object type
//-----------------------------------------------------------------------------
bool ClassCanBuild( int iClass, int iObjectType )
{
	/*
	for ( int i = 0; i < OBJ_LAST; i++ )
	{
		// Hit the end?
		if ( g_TFClassInfos[iClass].m_pClassObjects[i] == OBJ_LAST )
			return false;

		// Found it?
		if ( g_TFClassInfos[iClass].m_pClassObjects[i] == iObjectType )
			return true;
	}

	return false;
	*/

	return ( iClass == TF_CLASS_ENGINEER );
}
