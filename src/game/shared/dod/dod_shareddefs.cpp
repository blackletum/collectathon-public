//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "dod_shareddefs.h"
#include "weapon_dodbase.h"

//Voice commands
// SEALTODO
/*DodVoiceCommand_t g_VoiceCommands[] =
{
	//Voice command			// Command Name		//HS								//Allied subtitle					//german subtitle					//brit subtitle

	// Menu A
	{"voice_attack",		"Moveout",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_attack",			"#Voice_subtitle_moveout"},
	{"voice_hold",			"Hold",				PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_hold" },
	{"voice_left",			"FlankLeft",		PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_left" },
	{"voice_right",			"FlankRight",		PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_right" },
	{"voice_sticktogether",	"StickTogether",	PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_sticktogether" },
	{"voice_cover",			"CoveringFire",		PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_cover" },
	{"voice_usesmoke",		"UseSmoke",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_usesmoke" },
	{"voice_usegrens",		"UseGrenades",		PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_usegrens" },
	{"voice_ceasefire",		"CeaseFire",		PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_ceasefire" },
			
	// Menu B
	{"voice_yessir",		"YesSir",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_yessir" },
	{"voice_negative",		"Negative",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_negative" },
	{"voice_backup",		"BackUp",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_backup" },
	{"voice_fireinhole",	"FireInHole",		PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_fireinhole" },
	{"voice_grenade",		"Grenade",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_grenade" },
	{"voice_sniper",		"Sniper",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_sniper" },
	{"voice_niceshot",		"NiceShot",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_niceshot" },
	{"voice_thanks",		"Thanks",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_thanks" },
	{"voice_areaclear",		"Clear",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_areaclear" },
			
	// Menu C
	{"voice_dropweapons",	"DropWeapons",		PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_dropyourweapons" },
	{"voice_displace",		"Displace",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_displace" },
	{"voice_mgahead",		"MgAhead",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_mgahead" },
	{"voice_enemybehind",	"BehindUs",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_enemybehind" },
	{"voice_wegothim",		"WeGotHim",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_wegothim" },
	{"voice_moveupmg",		"MoveUpMg",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_moveupmg_30cal",	"#Voice_subtitle_moveupmg_mg",		"#Voice_subtitle_moveupmg_bren"},
	{"voice_needammo",		"NeedAmmo",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_needammo" },
	{"voice_usebazooka",	"UseRocket",		PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_usebazooka",		"#Voice_subtitle_usepschreck",		"#Voice_subtitle_usepiat"},
	{"voice_bazookaspotted","RocketSpotted",	PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_pschreckspotted",	"#Voice_subtitle_bazookaspotted",	"#Voice_subtitle_pschreckspotted"},

	// Voice commands that aren't in a menu
	{"voice_gogogo",		"Moveout",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_attack",			"#Voice_subtitle_moveout"},
	{"voice_medic",			"Medic",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_medic" },
	{"voice_coverflanks",	"CoverFlanks",		PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_coverflanks" },
	{"voice_tank",			"TankAhead",		PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_tigerahead",		"#Voice_subtitle_tankahead*" },
	{"voice_takeammo",		"TakeAmmo",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_takeammo" },
	{"voice_movewithtank",	"MoveWithTank",		PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_movewithtank" },
	{"voice_wtf",			"WhiskeyTangoFoxtrot",	PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_wtf" },
	{"voice_fireleft",		"TakingFireLeft",	PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_fireleft" },
	{"voice_fireright",		"TakingFireRight",	PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_fireright" },
	{"voice_mgahead",		"MgAhead",			PLAYERANIMEVENT_HANDSIGNAL,			"#Voice_subtitle_mgahead" },
	{"voice_enemyahead",	"EnemyAhead",		PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_enemyahead" },
	{"voice_fallback",		"FallBack",			PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_fallback" },
	
	// Must be last in the list
	{ NULL }
};

// Hand Signals
DodHandSignal_t g_HandSignals[] =
{
	//command					// anim event						//subtitle

	{"signal_sticktogether",	PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_sticktogether" },
	{"signal_fallback",			PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_fallback" },
	{"signal_no",				PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_negative" },
	{"signal_yes",				PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_yessir" },
	{"signal_sniper",			PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_sniper" },
	{"signal_backup",			PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_backup" },
	{"signal_enemyleft",		PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_fireleft" },
	{"signal_enemyright",		PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_fireright" },
	{"signal_grenade",			PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_grenade" },
	{"signal_flankleft",		PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_left" },
	{"signal_flankright",		PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_right" },
	{"signal_moveout",			PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_attack" },
	{"signal_areaclear",		PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_areaclear" },
	{"signal_coveringfire",		PLAYERANIMEVENT_HANDSIGNAL,		"#Voice_subtitle_cover" },
								
	// Must be last in the list
	{ NULL }
};*/

const char *m_pszHelmetModels[NUM_HELMETS] =
{
	"models/helmets/helmet_american.mdl",
	"models/helmets/helmet_german.mdl",
};

const char *g_pszDODHintMessages[] =
{
	"#Hint_spotted_a_friend",
	"#Hint_spotted_an_enemy",
	"#Hint_try_not_to_injure_teammates",
	"#Hint_careful_around_teammates",
	"#Hint_killing_enemies_is_good",
	"#Hint_touched_area_capture",
	"#Hint_touched_control_point",
	"#Hint_picked_up_object",
	"#Hint_mgs_fire_better_deployed",
	"#Hint_sandbag_area_touch",
	"#Hint_rocket_weapon_pickup",
	"#Hint_out_of_ammo",
	"#Hint_prone",
	"#Hint_low_stamina",
	"#Hint_area_requires_object",
	"#Hint_player_killed_wavetime", 
	"#Hint_mg_overheat",
	"#game_shoulder_rpg",
	"#Hint_pick_up_weapon",
	"#Hint_pick_up_grenade",
	"#Hint_death_cam",
	"#Hint_class_menu",
	"#Hint_use_2e_melee",
	"#Hint_use_zoom",
	"#Hint_use_iron_sights",
	"#Hint_use_semi_auto",
	"#Hint_use_sprint",
	"#Hint_use_deploy",
	"#Hint_use_prime",
	"#Hint_mg_deploy_usage",
	"#Dod_mg_reload",
	"#Hint_garand_reload",
	"#Hint_turn_off_hints",
	"#Hint_need_bomb_to_plant",
	"#Hint_bomb_planted",
	"#Hint_defuse_bomb",
	"#Hint_bomb_target",
	"#Hint_bomb_pickup",
	"#Hint_bomb_defuse_onground",
	"#Hint_bomb_plant_map",
	"#Hint_bomb_first_select",
};

const char *pszTeamAlliesClasses[] = 
{
	"us_garand",
	"us_tommy",
	"us_bar",
	"us_spring",
	"us_30cal",
	"us_bazooka",
	NULL
};

const char *pszTeamAxisClasses[] = 
{
	"axis_k98",
	"axis_mp40",
	"axis_mp44",
	"axis_k98s",
	"axis_mg42",
	"axis_pschreck",
	NULL
};

const char *pszWinPanelCategoryHeaders[] =
{
	"",
	"#winpanel_topbomb",
	"#winpanel_topcappers",
	"#winpanel_topdefenders",
	"#winpanel_kills"
};

const char *g_pszAchievementAwards[NUM_ACHIEVEMENT_AWARDS] =
{
	"",
	"DOD_KILLS_AS_RIFLEMAN",
	"DOD_KILLS_AS_ASSAULT",
	"DOD_KILLS_AS_SUPPORT",
	"DOD_KILLS_AS_SNIPER",
	"DOD_KILLS_AS_MG",
	"DOD_KILLS_AS_BAZOOKAGUY",
	"DOD_ALL_PACK_1",
};

const char *g_pszAchievementAwardMaterials_Allies[NUM_ACHIEVEMENT_AWARDS] =
{
	"sprites/player_icons/american",
	"sprites/player_icons/american_rifleman",
	"sprites/player_icons/american_assault",
	"sprites/player_icons/american_support",
	"sprites/player_icons/american_sniper",
	"sprites/player_icons/american_mg",
	"sprites/player_icons/american_rocket",
	"sprites/player_icons/american_hero",
};

const char *g_pszAchievementAwardMaterials_Axis[NUM_ACHIEVEMENT_AWARDS] =
{
	"sprites/player_icons/german",
	"sprites/player_icons/german_rifleman",
	"sprites/player_icons/german_assault",
	"sprites/player_icons/german_support",
	"sprites/player_icons/german_sniper",
	"sprites/player_icons/german_mg",
	"sprites/player_icons/german_rocket",
	"sprites/player_icons/german_hero",
};