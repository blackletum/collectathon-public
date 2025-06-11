//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "cbase.h"
#include "tf_gamerules.h"
#include "tf_player_shared.h"
#include "takedamageinfo.h"
#include "effect_dispatch_data.h"
#include "tf_item.h"
#include "entity_capture_flag.h"
#include "baseobject_shared.h"
#include "in_buttons.h"

#include "UtlSortVector.h"

// Weapons
#include "tf_weaponbase.h"
#include "tf_weapon_medigun.h"
#include "tf_weapon_pipebomblauncher.h"
#include "tf_weapon_sniperrifle.h"
#include "tf_weapon_wrench.h"
#include "tf_weapon_invis.h"

// DOD Includes
#include "ammodef.h"
#include "decals.h"
#include "weapon_dodbase.h"
#include "weapon_dodbasegrenade.h"
#include "weapon_dodbipodgun.h"
#include "weapon_dodbaserpg.h"
#include "weapon_dodsniper.h"
#include "dod_gamemovement.h"

// CSS
#include "datacache/imdlcache.h"
#ifdef CLIENT_DLL
#else
	#include "soundent.h"
#endif

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
#include "c_te_effect_dispatch.h"
#include "c_tf_fx.h"
#include "soundenvelope.h"
#include "c_tf_playerclass.h"
#include "iviewrender.h"
#include "prediction.h"
#include "c_tf_weapon_builder.h"
#include "c_func_capture_zone.h"
#include "dt_utlvector_recv.h"
#include "recvproxy.h"

#define CTFPlayerClass C_TFPlayerClass
#define CCaptureZone C_CaptureZone
#define CRecipientFilter C_RecipientFilter

// Server specific.
#else
#include "tf_player.h"
#include "te_effect_dispatch.h"
#include "tf_fx.h"
#include "util.h"
#include "tf_team.h"
#include "tf_gamestats.h"
#include "tf_playerclass.h"
#include "SpriteTrail.h"
#include "tf_weapon_builder.h"
#include "func_capture_zone.h"
#include "tf_obj_dispenser.h"
#include "dt_utlvector_send.h"
#include "tf_weapon_builder.h"

#include "sceneentity.h"
#endif

ConVar tf_scout_air_dash_count( "tf_scout_air_dash_count", "1", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

ConVar tf_spy_invis_time( "tf_spy_invis_time", "1.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Transition time in and out of spy invisibility", true, 0.1, true, 5.0 );
ConVar tf_spy_invis_unstealth_time( "tf_spy_invis_unstealth_time", "2.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Transition time in and out of spy invisibility", true, 0.1, true, 5.0 );

ConVar tf_spy_max_cloaked_speed( "tf_spy_max_cloaked_speed", "999", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );	// no cap
ConVar tf_max_health_boost( "tf_max_health_boost", "1.5", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Max health factor that players can be boosted to by healers.", true, 1.0, false, 0 );
ConVar tf_invuln_time( "tf_invuln_time", "1.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Time it takes for invulnerability to wear off." );
ConVar tf_player_movement_stun_time( "tf_player_movement_stun_time", "0.5", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );
extern ConVar tf_player_movement_restart_freeze;

ConVar tf_always_loser( "tf_always_loser", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Force loserstate to true." );

#ifdef GAME_DLL
ConVar tf_boost_drain_time( "tf_boost_drain_time", "15.0", FCVAR_DEVELOPMENTONLY, "Time is takes for a full health boost to drain away from a player.", true, 0.1, false, 0 );
ConVar tf_debug_bullets( "tf_debug_bullets", "0", FCVAR_DEVELOPMENTONLY, "Visualize bullet traces." );
ConVar tf_damage_events_track_for( "tf_damage_events_track_for", "30",  FCVAR_DEVELOPMENTONLY );
#endif

ConVar tf_useparticletracers( "tf_useparticletracers", "1", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Use particle tracers instead of old style ones." );
ConVar tf_spy_cloak_consume_rate( "tf_spy_cloak_consume_rate", "10.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "cloak to use per second while cloaked, from 100 max )" );	// 10 seconds of invis
ConVar tf_spy_cloak_regen_rate( "tf_spy_cloak_regen_rate", "3.3", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "cloak to regen per second, up to 100 max" );		// 30 seconds to full charge
ConVar tf_spy_cloak_no_attack_time( "tf_spy_cloak_no_attack_time", "2.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "time after uncloaking that the spy is prohibited from attacking" );

#if defined( _DEBUG ) || defined( STAGING_ONLY )
extern ConVar mp_developer;
#endif // _DEBUG || STAGING_ONLY

//ConVar tf_spy_stealth_blink_time( "tf_spy_stealth_blink_time", "0.3", FCVAR_DEVELOPMENTONLY, "time after being hit the spy blinks into view" );
//ConVar tf_spy_stealth_blink_scale( "tf_spy_stealth_blink_scale", "0.85", FCVAR_DEVELOPMENTONLY, "percentage visible scalar after being hit the spy blinks into view" );

ConVar tf_damage_disablespread( "tf_damage_disablespread", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Toggles the random damage spread applied to all player damage." );

ConVar tf_allow_taunt_switch( "tf_allow_taunt_switch", "0", FCVAR_REPLICATED, "0 - players are not allowed to switch weapons while taunting, 1 - players can switch weapons at the start of a taunt (old bug behavior), 2 - players can switch weapons at any time during a taunt." );

ConVar sv_showimpacts("sv_showimpacts", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Shows client (red) and server (blue) bullet impact point" );

ConVar tf_building_hauling( "tf_building_hauling", "1", FCVAR_NOTIFY | FCVAR_REPLICATED );

ConVar tf_deathanim_disable( "tf_deathanim_disable", "0", FCVAR_REPLICATED, "Disables death animations." );
ConVar tf_deathanim_headshot_disable( "tf_deathanim_headshot_disable", "0", FCVAR_REPLICATED, "Disables headshot death animations." );
ConVar tf_deathanim_backstab_disable( "tf_deathanim_backstab_disable", "0", FCVAR_REPLICATED, "Disables backstab death animations." );
ConVar tf_deathanim_burning_disable( "tf_deathanim_burning_disable", "1", FCVAR_REPLICATED, "Disables burning death animations." );

#define TF_SPY_STEALTH_BLINKTIME   0.3f
#define TF_SPY_STEALTH_BLINKSCALE  0.85f

#define TF_BUILDING_PICKUP_RANGE 150
#define TF_BUILDING_RESCUE_MIN_RANGE_SQ 62500  //250 * 250
#define TF_BUILDING_RESCUE_MAX_RANGE 5500

#define TF_PLAYER_CONDITION_CONTEXT	"TFPlayerConditionContext"

#define TF_SCREEN_OVERLAY_MATERIAL_BURNING		"effects/imcookin" 
#define TF_SCREEN_OVERLAY_MATERIAL_INVULN_RED	"effects/invuln_overlay_red" 
#define TF_SCREEN_OVERLAY_MATERIAL_INVULN_BLUE	"effects/invuln_overlay_blue" 

#define TF_SCREEN_OVERLAY_MATERIAL_MILK				"effects/milk_screen" 
#define TF_SCREEN_OVERLAY_MATERIAL_URINE			"effects/jarate_overlay" 
#define TF_SCREEN_OVERLAY_MATERIAL_BLEED			"effects/bleed_overlay" 
#define TF_SCREEN_OVERLAY_MATERIAL_STEALTH			"effects/stealth_overlay"
#define TF_SCREEN_OVERLAY_MATERIAL_SWIMMING_CURSE	"effects/jarate_overlay" 

#define TF_SCREEN_OVERLAY_MATERIAL_PHASE	"effects/dodge_overlay"

#define MAX_DAMAGE_EVENTS		128

float GetDensityFromMaterial( surfacedata_t *pSurfaceData )
{
	float flMaterialMod = 1.0f;

	Assert( pSurfaceData );

	// material mod is how many points of damage it costs to go through
	// 1 unit of the material

	switch( pSurfaceData->game.material )
	{
	//super soft
//	case CHAR_TEX_LEAVES:
//		flMaterialMod = 1.2f;
//		break;

	case CHAR_TEX_FLESH:
		flMaterialMod = 1.35f;
		break;

	//soft
//	case CHAR_TEX_STUCCO:
//	case CHAR_TEX_SNOW:
	case CHAR_TEX_GLASS:
	case CHAR_TEX_WOOD:
	case CHAR_TEX_TILE:
		flMaterialMod = 1.8f;
		break;

	//hard
//	case CHAR_TEX_SKY:
//	case CHAR_TEX_ROCK:
//	case CHAR_TEX_SAND:	
	case CHAR_TEX_CONCRETE:
	case CHAR_TEX_DIRT:		// "sand"
		flMaterialMod = 6.6f;
		break;

	//really hard
//	case CHAR_TEX_HEAVYMETAL:
	case CHAR_TEX_GRATE:
	case CHAR_TEX_METAL:
		flMaterialMod = 13.5f;
		break;

	case 'X':		// invisible collision material
		flMaterialMod = 0.1f;
		break;

	//medium
//	case CHAR_TEX_BRICK:
//	case CHAR_TEX_GRAVEL:
//	case CHAR_TEX_GRASS:
	default:

#ifndef CLIENT_DLL
		AssertMsg( 0, UTIL_VarArgs( "Material has unknown materialmod - '%c' \n", pSurfaceData->game.material ) );
#endif

		flMaterialMod = 5.0f;
		break;
	}

	Assert( flMaterialMod > 0 );

	return flMaterialMod;
}

static bool TraceToExit( const Vector &start,
						const Vector &dir,
						Vector &end,
						const float flStepSize,
						const float flMaxDistance )
{
	float flDistance = 0;
	Vector last = start;

	while ( flDistance < flMaxDistance )
	{
		flDistance += flStepSize;

		// no point in tracing past the max distance.
		// if this check fails, we save ourselves a traceline later
		if ( flDistance > flMaxDistance )
		{
			flDistance = flMaxDistance;
		}

		end = start + flDistance * dir; 

		// point contents fails to return proper contents inside a func_detail brush, eg the dod_flash 
		// stairs

		//int contents = UTIL_PointContents( end );

		trace_t tr;
		UTIL_TraceLine( end, end, MASK_SOLID | CONTENTS_HITBOX, NULL, &tr );

		//if ( (UTIL_PointContents ( end ) & MASK_SOLID) == 0 )

		if ( !tr.startsolid )
		{
			// found first free point
			return true;
		}
	}

	return false;
}

const char *g_pszBDayGibs[22] = 
{
	"models/effects/bday_gib01.mdl",
	"models/effects/bday_gib02.mdl",
	"models/effects/bday_gib03.mdl",
	"models/effects/bday_gib04.mdl",
	"models/player/gibs/gibs_balloon.mdl",
	"models/player/gibs/gibs_burger.mdl",
	"models/player/gibs/gibs_boot.mdl",
	"models/player/gibs/gibs_bolt.mdl",
	"models/player/gibs/gibs_can.mdl",
	"models/player/gibs/gibs_clock.mdl",
	"models/player/gibs/gibs_fish.mdl",
	"models/player/gibs/gibs_gear1.mdl",
	"models/player/gibs/gibs_gear2.mdl",
	"models/player/gibs/gibs_gear3.mdl",
	"models/player/gibs/gibs_gear4.mdl",
	"models/player/gibs/gibs_gear5.mdl",
	"models/player/gibs/gibs_hubcap.mdl",
	"models/player/gibs/gibs_licenseplate.mdl",
	"models/player/gibs/gibs_spring1.mdl",
	"models/player/gibs/gibs_spring2.mdl",
	"models/player/gibs/gibs_teeth.mdl",
	"models/player/gibs/gibs_tire.mdl"
};

ETFCond g_SoldierBuffAttributeIDToConditionMap[kSoldierBuffCount + 1] =
{
	TF_COND_LAST,				// dummy entry to deal with attribute value of "1" being the lowest value we store in the attribute itself
	TF_COND_OFFENSEBUFF,
	TF_COND_DEFENSEBUFF,
	TF_COND_REGENONDAMAGEBUFF,
	TF_COND_NOHEALINGDAMAGEBUFF,
	TF_COND_CRITBOOSTED_RAGE_BUFF,
	TF_COND_SNIPERCHARGE_RAGE_BUFF
};

//=============================================================================
//
// Tables.
//

// Client specific.
#ifdef CLIENT_DLL

EXTERN_RECV_TABLE(DT_TFPlayerConditionListExclusive);

BEGIN_RECV_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerSharedLocal )
	RecvPropInt( RECVINFO( m_nDesiredDisguiseTeam ) ),
	RecvPropInt( RECVINFO( m_nDesiredDisguiseClass ) ),
	RecvPropTime( RECVINFO( m_flStealthNoAttackExpire ) ),
	RecvPropTime( RECVINFO( m_flStealthNextChangeTime ) ),
	RecvPropBool( RECVINFO( m_bLastDisguisedAsOwnTeam ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominated ), RecvPropBool( RECVINFO( m_bPlayerDominated[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominatingMe ), RecvPropBool( RECVINFO( m_bPlayerDominatingMe[0] ) ) ),

	//DOD
	RecvPropFloat( RECVINFO( m_flDeployedYawLimitLeft ) ),
	RecvPropFloat( RECVINFO( m_flDeployedYawLimitRight ) ),
END_RECV_TABLE()

BEGIN_RECV_TABLE_NOBASE( condition_source_t, DT_TFPlayerConditionSource )
	//RecvPropInt( RECVINFO( m_nPreventedDamageFromCondition ) ),
	//RecvPropFloat( RECVINFO( m_flExpireTime ) ),
	RecvPropEHandle( RECVINFO( m_pProvider ) ),
	//RecvPropBool( RECVINFO( m_bPrevActive ) ),
END_RECV_TABLE()

BEGIN_RECV_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerShared )
	RecvPropInt( RECVINFO( m_nPlayerCond ) ),
	RecvPropInt( RECVINFO( m_bJumping) ),
	RecvPropInt( RECVINFO( m_nNumHealers ) ),
	RecvPropInt( RECVINFO( m_iCritMult) ),
	RecvPropInt( RECVINFO( m_iAirDash ) ),
	RecvPropInt( RECVINFO( m_nAirDucked ) ),
	RecvPropFloat( RECVINFO( m_flDuckTimer ) ),
	RecvPropInt( RECVINFO( m_nPlayerState ) ),
	RecvPropInt( RECVINFO( m_iDesiredPlayerClass ) ),
	RecvPropFloat( RECVINFO( m_flMovementStunTime ) ),
	RecvPropInt( RECVINFO( m_iMovementStunAmount ) ),
	RecvPropInt( RECVINFO( m_iMovementStunParity ) ),
	RecvPropEHandle( RECVINFO( m_hStunner ) ),
	RecvPropInt( RECVINFO( m_iStunFlags ) ),
	RecvPropInt( RECVINFO( m_iDisguiseBody ) ),
	RecvPropEHandle( RECVINFO( m_hCarriedObject ) ),
	RecvPropBool( RECVINFO( m_bCarryingObject ) ),
	RecvPropInt( RECVINFO( m_iSpawnRoomTouchCount ) ),
	// Spy.
	RecvPropTime( RECVINFO( m_flInvisChangeCompleteTime ) ),
	RecvPropInt( RECVINFO( m_nDisguiseTeam ) ),
	RecvPropInt( RECVINFO( m_nDisguiseClass ) ),
	RecvPropInt( RECVINFO( m_nDisguiseSkinOverride ) ),
	RecvPropInt( RECVINFO( m_nMaskClass ) ),
	RecvPropInt( RECVINFO( m_iDisguiseTargetIndex ) ),
	RecvPropInt( RECVINFO( m_iDisguiseHealth ) ),
	RecvPropEHandle( RECVINFO( m_hDisguiseWeapon ) ),
	RecvPropInt( RECVINFO( m_nTeamTeleporterUsed ) ),
	RecvPropFloat( RECVINFO( m_flCloakMeter ) ),
	// Local Data.
	RecvPropDataTable( "tfsharedlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_TFPlayerSharedLocal) ),
	RecvPropDataTable( RECVINFO_DT(m_ConditionList),0, &REFERENCE_RECV_TABLE(DT_TFPlayerConditionListExclusive) ),

	RecvPropInt( RECVINFO( m_nPlayerCondEx ) ),
	RecvPropInt( RECVINFO( m_nPlayerCondEx2 ) ),
	RecvPropInt( RECVINFO( m_nPlayerCondEx3 ) ),

	RecvPropInt( RECVINFO( m_iStunIndex ) ),

	RecvPropUtlVectorDataTable( m_ConditionData, TF_COND_LAST, DT_TFPlayerConditionSource ),

	// DOD
	RecvPropFloat( RECVINFO( m_flStamina ) ),
	RecvPropTime( RECVINFO( m_flSlowedUntilTime ) ),
	RecvPropBool( RECVINFO( m_bProne ) ),
	RecvPropBool( RECVINFO( m_bIsSprinting ) ),
	RecvPropTime( RECVINFO( m_flGoProneTime ) ),
	RecvPropTime( RECVINFO( m_flUnProneTime ) ),
	RecvPropTime( RECVINFO( m_flDeployChangeTime ) ),
	RecvPropFloat( RECVINFO( m_flDeployedHeight ) ),
	RecvPropBool( RECVINFO( m_bPlanting ) ),
	RecvPropBool( RECVINFO( m_bDefusing ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( CTFPlayerShared )
	DEFINE_PRED_FIELD( m_nPlayerState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nPlayerCond, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flCloakMeter, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bJumping, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iAirDash, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nAirDucked, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flDuckTimer, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flInvisChangeCompleteTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nDisguiseTeam, FIELD_INTEGER, FTYPEDESC_INSENDTABLE  ),
	DEFINE_PRED_FIELD( m_nDisguiseClass, FIELD_INTEGER, FTYPEDESC_INSENDTABLE  ),
	DEFINE_PRED_FIELD( m_nDisguiseSkinOverride, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nMaskClass, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nDesiredDisguiseTeam, FIELD_INTEGER, FTYPEDESC_INSENDTABLE  ),
	DEFINE_PRED_FIELD( m_nDesiredDisguiseClass, FIELD_INTEGER, FTYPEDESC_INSENDTABLE  ),
	DEFINE_PRED_FIELD( m_bLastDisguisedAsOwnTeam, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE  ),
	DEFINE_PRED_FIELD( m_nPlayerCondEx, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nPlayerCondEx2, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nPlayerCondEx3, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_FIELD( m_flDisguiseCompleteTime, FIELD_FLOAT ),

	//DOD
	DEFINE_PRED_FIELD( m_bProne, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flStamina, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bIsSprinting, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flGoProneTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flUnProneTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flDeployChangeTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flDeployedHeight, FIELD_FLOAT, FTYPEDESC_INSENDTABLE )
END_PREDICTION_DATA()

// Server specific.
#else

EXTERN_SEND_TABLE(DT_TFPlayerConditionListExclusive);
BEGIN_SEND_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerSharedLocal )
	SendPropInt( SENDINFO( m_nDesiredDisguiseTeam ), 3, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nDesiredDisguiseClass ), 4, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bLastDisguisedAsOwnTeam ) ),
	SendPropTime( SENDINFO( m_flStealthNoAttackExpire ) ),
	SendPropTime( SENDINFO( m_flStealthNextChangeTime ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_bPlayerDominated ), SendPropBool( SENDINFO_ARRAY( m_bPlayerDominated ) ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_bPlayerDominatingMe ), SendPropBool( SENDINFO_ARRAY( m_bPlayerDominatingMe ) ) ),

	// DOD
	SendPropFloat( SENDINFO( m_flDeployedYawLimitLeft ) ),
	SendPropFloat( SENDINFO( m_flDeployedYawLimitRight ) ),
END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE( condition_source_t, DT_TFPlayerConditionSource )
	//SendPropInt( SENDINFO( m_nPreventedDamageFromCondition ) ),
	//SendPropFloat( SENDINFO( m_flExpireTime ) ),
	SendPropEHandle( SENDINFO( m_pProvider ) ),
	//SendPropBool( SENDINFO( m_bPrevActive ) ),
END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerShared )
	SendPropInt( SENDINFO( m_nPlayerCond ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bJumping ), 1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
	SendPropInt( SENDINFO( m_nNumHealers ), 5, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
	SendPropInt( SENDINFO( m_iCritMult ), 8, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
	SendPropInt( SENDINFO( m_iAirDash ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nAirDucked ), 2, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flDuckTimer )  ),
	SendPropInt( SENDINFO( m_nPlayerState ), Q_log2( TF_STATE_COUNT )+1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDesiredPlayerClass ), Q_log2( GAME_CLASS_COUNT  )+1, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flMovementStunTime )  ),
	SendPropInt( SENDINFO( m_iMovementStunAmount ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iMovementStunParity ), MOVEMENTSTUN_PARITY_BITS, SPROP_UNSIGNED ),
	SendPropEHandle( SENDINFO( m_hStunner ) ),
	SendPropInt( SENDINFO( m_iStunFlags  ), 12, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDisguiseBody ) ),
	SendPropEHandle( SENDINFO( m_hCarriedObject ) ),
	SendPropBool( SENDINFO( m_bCarryingObject ) ),
	SendPropInt( SENDINFO( m_iSpawnRoomTouchCount ) ),
	// Spy
	SendPropTime( SENDINFO( m_flInvisChangeCompleteTime ) ),
	SendPropInt( SENDINFO( m_nDisguiseTeam ), 3, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nDisguiseClass ), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nDisguiseSkinOverride ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nMaskClass ), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDisguiseTargetIndex ), 7, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDisguiseHealth ), 10 ),
	SendPropEHandle( SENDINFO( m_hDisguiseWeapon ) ),
	SendPropInt( SENDINFO( m_nTeamTeleporterUsed ), 3, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flCloakMeter ), 16, SPROP_NOSCALE, 0.0, 100.0 ),
	// Local Data.
	SendPropDataTable( "tfsharedlocaldata", 0, &REFERENCE_SEND_TABLE( DT_TFPlayerSharedLocal ), SendProxy_SendLocalDataTable ),
	SendPropDataTable( SENDINFO_DT(m_ConditionList), &REFERENCE_SEND_TABLE(DT_TFPlayerConditionListExclusive) ),

	SendPropInt( SENDINFO( m_nPlayerCondEx ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nPlayerCondEx2 ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nPlayerCondEx3 ), -1, SPROP_VARINT | SPROP_UNSIGNED ),

	SendPropInt( SENDINFO( m_iStunIndex ), 8 ),

	SendPropUtlVectorDataTable( m_ConditionData, TF_COND_LAST, DT_TFPlayerConditionSource ),

	// DOD
	SendPropFloat( SENDINFO( m_flStamina ), 0, SPROP_NOSCALE | SPROP_CHANGES_OFTEN ),
	SendPropTime( SENDINFO( m_flSlowedUntilTime ) ),
	SendPropBool( SENDINFO( m_bProne ) ),
	SendPropBool( SENDINFO( m_bIsSprinting ) ),
	SendPropTime( SENDINFO( m_flGoProneTime ) ),
	SendPropTime( SENDINFO( m_flUnProneTime ) ),
	SendPropTime( SENDINFO( m_flDeployChangeTime ) ),
	SendPropTime( SENDINFO( m_flDeployedHeight ) ),
	SendPropBool( SENDINFO( m_bPlanting ) ),
	SendPropBool( SENDINFO( m_bDefusing ) ),
END_SEND_TABLE()

#endif


// --------------------------------------------------------------------------------------------------- //
// Shared CTFPlayer implementation.
// --------------------------------------------------------------------------------------------------- //

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::IsAllowedToTaunt( void )
{
	if ( !IsAlive() )
		return false;

	// Check to see if we can taunt again!
	if ( m_Shared.InCond( TF_COND_TAUNTING ) )
		return false;

	if ( m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
		return false;

	if ( m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) )
		return false;

	// Can't taunt while charging.
	if ( m_Shared.InCond( TF_COND_SHIELD_CHARGE ) )
		return false;

	if ( m_Shared.InCond( TF_COND_COMPETITIVE_LOSER ) )
		return false;

	if ( IsLerpingFOV() )
		return false;

	// Check for things that prevent taunting
	if ( ShouldStopTaunting() )
		return false;

	// Check to see if we are on the ground.
	if ( GetGroundEntity() == NULL && !m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
		return false;

	CPCWeaponBase *pActiveWeapon = m_Shared.GetActivePCWeapon();
	if ( pActiveWeapon )
	{
		//if ( !pActiveWeapon->OwnerCanTaunt() ) // SEALTODO??
		//	return false;

		// ignore taunt key if one of these if active weapon
		if ( pActiveWeapon->GetWeaponID() == TF_WEAPON_PDA_ENGINEER_BUILD 
			 ||	pActiveWeapon->GetWeaponID() == TF_WEAPON_PDA_ENGINEER_DESTROY )
			return false;
	}

	// can't taunt while carrying an object
	if ( m_Shared.IsCarryingObject() )
		return false;

	// Can't taunt if hooked into a player
	if ( m_Shared.InCond( TF_COND_GRAPPLED_TO_PLAYER ) )
		return false;

	if ( IsPlayerClass( TF_CLASS_SPY ) )
	{
		if ( m_Shared.IsStealthed() || m_Shared.InCond( TF_COND_STEALTHED_BLINK ) || 
			 m_Shared.InCond( TF_COND_DISGUISED ) || m_Shared.InCond( TF_COND_DISGUISING ) )
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldStopTaunting()
{
	// stop taunt if we're under water
	return GetWaterLevel() > WL_Waist;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SetTauntYaw( float flTauntYaw )
{
	m_flPrevTauntYaw = m_flTauntYaw;
	m_flTauntYaw = flTauntYaw;

	QAngle angle = GetLocalAngles();
	angle.y = flTauntYaw;
	SetLocalAngles( angle );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::StartBuildingObjectOfType( int iType, int iMode )
{
	// early out if we can't build this type of object
	if ( CanBuild( iType, iMode ) != CB_CAN_BUILD )
		return;

	// Does this type require a specific builder?
	CTFWeaponBuilder *pBuilder = CTFPlayerSharedUtils::GetBuilderForObjectType( this, iType );
	if ( pBuilder )
	{
#ifdef GAME_DLL
		pBuilder->SetSubType( iType );
		pBuilder->SetObjectMode( iMode );


		if ( GetActivePCWeapon() == pBuilder )
		{
			SetActiveWeapon( NULL );
		}	
#endif

		// try to switch to this weapon
		Weapon_Switch( pBuilder );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayerShared::GetSequenceForDeath( CBaseAnimating* pRagdoll, bool bBurning, int nCustomDeath )
{
	if ( !pRagdoll || tf_deathanim_disable.GetBool() )
		return -1;

	// SEALTODO
	//if ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() && !tf_mvm_allow_robot_deathanim.GetBool() )
	//{
	//	if ( m_pOuter && ( m_pOuter->GetTeamNumber() == TF_TEAM_PVE_INVADERS ) )
	//		return -1;
	//}

	int iDeathSeq = -1;
 	if ( bBurning && !tf_deathanim_burning_disable.GetBool() )
 	{
 		iDeathSeq = pRagdoll->LookupSequence( "primary_death_burning" );
 	}

	switch ( nCustomDeath )
	{
	case TF_DMG_CUSTOM_HEADSHOT_DECAPITATION:
	case TF_DMG_CUSTOM_TAUNTATK_BARBARIAN_SWING:
	case TF_DMG_CUSTOM_DECAPITATION:
	case TF_DMG_CUSTOM_HEADSHOT:
	case DOD_DMG_CUSTOM_HEADSHOT:
		if ( !tf_deathanim_headshot_disable.GetBool() )
			iDeathSeq = pRagdoll->LookupSequence( "primary_death_headshot" );
		break;
	case TF_DMG_CUSTOM_BACKSTAB:
	case DOD_DMG_CUSTOM_MELEE_STRONGATTACK:
		if ( !tf_deathanim_backstab_disable.GetBool() )
			iDeathSeq = pRagdoll->LookupSequence( "primary_death_backstab" );
		break;
	}

	return iDeathSeq;
}

// --------------------------------------------------------------------------------------------------- //
// CTFPlayerShared implementation.
// --------------------------------------------------------------------------------------------------- //

CTFPlayerShared::CTFPlayerShared()
{
	m_nPlayerState.Set( TF_STATE_WELCOME );
	m_bJumping = false;
	m_iAirDash = 0;
	m_nAirDucked = 0;
	m_flDuckTimer = 0.0f;
	m_flStealthNoAttackExpire = 0.0f;
	m_flStealthNextChangeTime = 0.0f;
	m_iCritMult = 0;
	m_flInvisibility = 0.0f;
	m_flPrevInvisibility = 0.f;
	m_flTmpDamageBonusAmount = 1.0f;

	m_fCloakConsumeRate = tf_spy_cloak_consume_rate.GetFloat();
	m_fCloakRegenRate = tf_spy_cloak_regen_rate.GetFloat();

	m_hStunner = NULL;
	m_iStunFlags = 0;

	m_bLastDisguisedAsOwnTeam = false;

	m_iSpawnRoomTouchCount = 0;

	m_nTeamTeleporterUsed = TEAM_UNASSIGNED;

	m_nForceConditions = 0;
	m_nForceConditionsEx = 0;
	m_nForceConditionsEx2 = 0;
	m_nForceConditionsEx3 = 0;

	m_flLastMovementStunChange = 0;
	m_bStunNeedsFadeOut = false;

	m_bCarryingObject = false;
	m_hCarriedObject = NULL;

	m_iStunIndex = -1;
	m_flLastNoMovementTime = -1.f;

	// make sure we have all conditions in the list
	m_ConditionData.EnsureCount( TF_COND_LAST );

	//DOD
	m_bProne = false;
	m_bForceProneChange = false;
	m_flNextProneCheck = 0;

	m_flSlowedUntilTime = 0;

	m_flUnProneTime = 0;
	m_flGoProneTime = 0;

	m_flDeployedHeight = STANDING_DEPLOY_HEIGHT;
	m_flDeployChangeTime = gpGlobals->curtime;

	m_flLastViewAnimationTime = gpGlobals->curtime;

	m_pViewOffsetAnim = NULL;
}

CTFPlayerShared::~CTFPlayerShared()
{
	// DOD
	if ( m_pViewOffsetAnim )
	{
		delete m_pViewOffsetAnim;
		m_pViewOffsetAnim = NULL;
	}
}

void CTFPlayerShared::Init( CTFPlayer *pPlayer )
{
	m_pOuter = pPlayer;

	m_flNextBurningSound = 0;

	m_iStunAnimState = STUN_ANIM_NONE;
	m_hStunner = NULL;

	SetJumping( false );
	SetAssist( NULL );

	Spawn();
}

void CTFPlayerShared::Spawn( void )
{
#ifdef GAME_DLL
	if ( m_bCarryingObject )
	{
		CBaseObject* pObj = GetCarriedObject();
		if ( pObj )
		{
			pObj->DetonateObject();
		}
	}

	m_bCarryingObject = false;
	m_hCarriedObject = NULL;

	m_iSpawnRoomTouchCount = 0;

	m_PlayerStuns.RemoveAll();
	m_iStunIndex = -1;
#else
	m_bSyncingConditions = false;
#endif

	// Reset our assist here incase something happens before we get killed
	// again that checks this (getting slapped with a fish)
	SetAssist( NULL );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template < typename tIntType >
class CConditionVars
{
public:
	CConditionVars( tIntType& nPlayerCond, tIntType& nPlayerCondEx, tIntType& nPlayerCondEx2, tIntType& nPlayerCondEx3, ETFCond eCond )
	{
		if ( eCond >= 96 )
		{
			Assert( eCond < 96 + 32 );
			m_pnCondVar = &nPlayerCondEx3;
			m_nCondBit = eCond - 96;
		}
		else if( eCond >= 64 )
		{
			Assert( eCond < (64 + 32) );
			m_pnCondVar = &nPlayerCondEx2;
			m_nCondBit = eCond - 64;
		}
		else if ( eCond >= 32 )
		{
			Assert( eCond < (32 + 32) );
			m_pnCondVar = &nPlayerCondEx;
			m_nCondBit = eCond - 32;
		}
		else
		{
			m_pnCondVar = &nPlayerCond;
			m_nCondBit = eCond;
		}
	}

	tIntType& CondVar() const
	{
		return *m_pnCondVar;
	}

	int CondBit() const
	{
		return 1 << m_nCondBit;
	}

private:
	tIntType *m_pnCondVar;
	int m_nCondBit;
};

//-----------------------------------------------------------------------------
// Purpose: Add a condition and duration
// duration of PERMANENT_CONDITION means infinite duration
//-----------------------------------------------------------------------------
void CTFPlayerShared::AddCond( ETFCond eCond, float flDuration /* = PERMANENT_CONDITION */, CBaseEntity *pProvider /*= NULL */)
{
	Assert( eCond >= 0 && eCond < TF_COND_LAST );
	Assert( eCond < m_ConditionData.Count() );

	// If we're dead, don't take on any new conditions
	if( !m_pOuter || !m_pOuter->IsAlive() )
	{
		return;
	}

#ifdef CLEINT_DLL
	if ( m_pOuter->IsDormant() )
	{
		return;
	}
#endif

	// Which bitfield are we tracking this condition variable in? Which bit within
	// that variable will we track it as?
	CConditionVars<int> cPlayerCond( m_nPlayerCond.m_Value, m_nPlayerCondEx.m_Value, m_nPlayerCondEx2.m_Value, m_nPlayerCondEx3.m_Value, eCond );

	// See if there is an object representation of the condition.
	bool bAddedToExternalConditionList = m_ConditionList.Add( eCond, flDuration, m_pOuter, pProvider );
	if ( !bAddedToExternalConditionList )
	{
		// Set the condition bit for this condition.
		cPlayerCond.CondVar() |= cPlayerCond.CondBit();

		// Flag for gamecode to query
		m_ConditionData[eCond].m_bPrevActive = ( m_ConditionData[eCond].m_flExpireTime != 0.f ) ? true : false;

		if ( flDuration != PERMANENT_CONDITION )
		{
			// if our current condition is permanent or we're trying to set a new
			// time that's less our current time remaining, use our current time instead
			if ( ( m_ConditionData[eCond].m_flExpireTime == PERMANENT_CONDITION ) || 
				 ( flDuration < m_ConditionData[eCond].m_flExpireTime ) )
			{
				flDuration = m_ConditionData[eCond].m_flExpireTime;
			}
		}

		m_ConditionData[eCond].m_flExpireTime = flDuration;
		m_ConditionData[eCond].m_pProvider = pProvider;
		m_ConditionData[ eCond ].m_nPreventedDamageFromCondition = 0;

		OnConditionAdded( eCond );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Forcibly remove a condition
//-----------------------------------------------------------------------------
void CTFPlayerShared::RemoveCond( ETFCond eCond, bool ignore_duration )
{
	Assert( eCond >= 0 && eCond < TF_COND_LAST );
	Assert( eCond < m_ConditionData.Count() );

	if ( !InCond( eCond ) )
		return;

	CConditionVars<int> cPlayerCond( m_nPlayerCond.m_Value, m_nPlayerCondEx.m_Value, m_nPlayerCondEx2.m_Value, m_nPlayerCondEx3.m_Value, eCond );

	// If this variable is handled by the condition list, abort before doing the
	// work for the condition flags.
	if ( m_ConditionList.Remove( eCond, ignore_duration ) )
		return;

	cPlayerCond.CondVar() &= ~cPlayerCond.CondBit();
	OnConditionRemoved( eCond );

	if ( m_ConditionData[ eCond ].m_nPreventedDamageFromCondition )
	{
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "damage_prevented" );
		if ( pEvent )
		{
			pEvent->SetInt( "preventor", m_ConditionData[eCond].m_pProvider ? m_ConditionData[eCond].m_pProvider->entindex() : m_pOuter->entindex() );
			pEvent->SetInt( "victim", m_pOuter->entindex() );
			pEvent->SetInt( "amount", m_ConditionData[ eCond ].m_nPreventedDamageFromCondition );
			pEvent->SetInt( "condition", eCond );

			gameeventmanager->FireEvent( pEvent, true );
		}

		m_ConditionData[ eCond ].m_nPreventedDamageFromCondition = 0;
	}

	m_ConditionData[eCond].m_flExpireTime = 0;
	m_ConditionData[eCond].m_pProvider = NULL;
	m_ConditionData[eCond].m_bPrevActive = false;

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayerShared::InCond( ETFCond eCond ) const
{
	Assert( eCond >= 0 && eCond < TF_COND_LAST );

	// Old condition system, only used for the first 32 conditions
	if ( eCond < 32 && m_ConditionList.InCond( eCond ) )
		return true;

	CConditionVars<const int> cPlayerCond( m_nPlayerCond.m_Value, m_nPlayerCondEx.m_Value, m_nPlayerCondEx2.m_Value, m_nPlayerCondEx3.m_Value, eCond );
	return (cPlayerCond.CondVar() & cPlayerCond.CondBit()) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: Return whether or not we were in this condition before.
//-----------------------------------------------------------------------------
bool CTFPlayerShared::WasInCond( ETFCond eCond ) const
{
	// I don't know if this actually works for conditions < 32, because we definitely cannot peak into m_ConditionList (back in time).
	// But others think that m_ConditionList is propogated into m_nOldConditions, so just check if you hit the assert. (And then remove the 
	// assert. And this comment).
	Assert( eCond >= 32 && eCond < TF_COND_LAST );

	CConditionVars<const int> cPlayerCond( m_nOldConditions, m_nOldConditionsEx, m_nOldConditionsEx2, m_nOldConditionsEx3, eCond );
	return (cPlayerCond.CondVar() & cPlayerCond.CondBit()) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: Set a bit to force this condition off and then back on next time we sync bits from the server. 
//-----------------------------------------------------------------------------
void CTFPlayerShared::ForceRecondNextSync( ETFCond eCond )
{
	// I don't know if this actually works for conditions < 32. We may need to set this bit in m_ConditionList, too.
	// Please check if you hit the assert. (And then remove the assert. And this comment).
	Assert(eCond >= 32 && eCond < TF_COND_LAST);

	CConditionVars<int> playerCond( m_nForceConditions, m_nForceConditionsEx, m_nForceConditionsEx2, m_nForceConditionsEx3, eCond );
	playerCond.CondVar() |= playerCond.CondBit();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CTFPlayerShared::GetConditionDuration( ETFCond eCond ) const
{
	Assert( eCond >= 0 && eCond < TF_COND_LAST );
	Assert( eCond < m_ConditionData.Count() );

	if ( InCond( eCond ) )
	{
		return m_ConditionData[eCond].m_flExpireTime;
	}
	
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the entity that provided the passed in condition
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayerShared::GetConditionProvider( ETFCond eCond ) const
{
	Assert( eCond >= 0 && eCond < TF_COND_LAST );
	Assert( eCond < m_ConditionData.Count() );

	CBaseEntity *pProvider = NULL;
	if ( InCond( eCond ) )
	{
		if ( eCond == TF_COND_CRITBOOSTED )
		{
			pProvider = m_ConditionList.GetProvider( eCond );
		}
		else
		{
			pProvider = m_ConditionData[eCond].m_pProvider;
		}
	}

	return pProvider;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the entity that applied this condition to us - for granting an assist when we die
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayerShared::GetConditionAssistFromVictim( void )
{
	// We only give an assist to one person.  That means this list is order
	// sensitive, so consider how "powerful" an effect is when adding it here.
	static const ETFCond nTrackedConditions[] = 
	{
		TF_COND_URINE,
		TF_COND_MAD_MILK,
		TF_COND_MARKEDFORDEATH,
	};

	CBaseEntity *pProvider = NULL;
	for ( int i = 0; i < ARRAYSIZE( nTrackedConditions ); i++ )
	{
		if ( InCond( nTrackedConditions[i] ) )
		{
			pProvider = GetConditionProvider( nTrackedConditions[i] );
			break;
		}
	}

	return pProvider;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the entity that applied this condition to us - for granting an assist when we kill someone
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayerShared::GetConditionAssistFromAttacker( void )
{
	// We only give an assist to one person.  That means this list is order
	// sensitive, so consider how "powerful" an effect is when adding it here.
	static const ETFCond nTrackedConditions[] = 
	{
		TF_COND_OFFENSEBUFF,			// Highest priority
		TF_COND_DEFENSEBUFF,
		TF_COND_REGENONDAMAGEBUFF,
		TF_COND_NOHEALINGDAMAGEBUFF,	// Lowest priority
	};

	CBaseEntity *pProvider = NULL;
	for ( int i = 0; i < ARRAYSIZE( nTrackedConditions ); i++ )
	{
		if ( InCond( nTrackedConditions[i] ) )
		{
			CBaseEntity* pPotentialProvider = GetConditionProvider( nTrackedConditions[i] );
			// Check to make sure we're not providing the condition to ourselves
			if( pPotentialProvider != m_pOuter )
			{
				pProvider = pPotentialProvider;
				break;
			}
		}
	}

	return pProvider;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::DebugPrintConditions( void )
{
#ifndef CLIENT_DLL
	const char *szDll = "Server";
#else
	const char *szDll = "Client";
#endif

	Msg( "( %s ) Conditions for player ( %d )\n", szDll, m_pOuter->entindex() );

	int i;
	int iNumFound = 0;
	for ( i=0;i<TF_COND_LAST;i++ )
	{
		if ( InCond( (ETFCond)i ) )
		{
			if ( m_ConditionData[i].m_flExpireTime == PERMANENT_CONDITION )
			{
				Msg( "( %s ) Condition %d - ( permanent cond )\n", szDll, i );
			}
			else
			{
				Msg( "( %s ) Condition %d - ( %.1f left )\n", szDll, i, m_ConditionData[i].m_flExpireTime );
			}

			iNumFound++;
		}
	}

	if ( iNumFound == 0 )
	{
		Msg( "( %s ) No active conditions\n", szDll );
	}
}

void CTFPlayerShared::InstantlySniperUnzoom( void )
{
	// Unzoom if we are a sniper zoomed!
	if ( m_pOuter->GetPlayerClass()->GetClassIndex() == TF_CLASS_SNIPER )
	{
		CPCWeaponBase *pWpn = m_pOuter->GetActivePCWeapon();

		if ( pWpn && WeaponID_IsSniperRifle( pWpn->GetWeaponID() ) )
		{
			CTFSniperRifle *pRifle = static_cast<CTFSniperRifle*>( pWpn );
			if ( pRifle->IsZoomed() )
			{
				// Let the rifle clean up conditions and state
				pRifle->ToggleZoom();
				// Slam the FOV right now
				m_pOuter->SetFOV( m_pOuter, 0, 0.0f );
			}
		}
	}
}

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnPreDataChanged( void )
{
	m_ConditionList.OnPreDataChanged();

	m_nOldConditions = m_nPlayerCond;
	m_nOldConditionsEx = m_nPlayerCondEx;
	m_nOldConditionsEx2 = m_nPlayerCondEx2;
	m_nOldConditionsEx3 = m_nPlayerCondEx3;
	m_nOldDisguiseClass = GetDisguiseClass();
	m_nOldDisguiseTeam = GetDisguiseTeam();
	m_iOldMovementStunParity = m_iMovementStunParity;

	InvisibilityThink();
	ConditionThink();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnDataChanged( void )
{
	m_ConditionList.OnDataChanged( m_pOuter );

	if ( m_iOldMovementStunParity != m_iMovementStunParity )
	{
		m_flStunFade = gpGlobals->curtime + m_flMovementStunTime;
		m_flStunEnd  = m_flStunFade;
		if ( IsControlStunned() && (m_iStunAnimState == STUN_ANIM_NONE) )
		{
			m_flStunEnd += CONTROL_STUN_ANIM_TIME;
		}

		UpdateLegacyStunSystem();
	}

	// Update conditions from last network change
	SyncConditions( m_nOldConditions, m_nPlayerCond, m_nForceConditions, 0 );
	SyncConditions( m_nOldConditionsEx, m_nPlayerCondEx, m_nForceConditionsEx, 32 );
	SyncConditions( m_nOldConditionsEx2, m_nPlayerCondEx2, m_nForceConditionsEx2, 64 );
	SyncConditions( m_nOldConditionsEx3, m_nPlayerCondEx3, m_nForceConditionsEx3, 96 );

	// Make sure these items are present
	m_nPlayerCond		|= m_nForceConditions;
	m_nPlayerCondEx		|= m_nForceConditionsEx;
	m_nPlayerCondEx2	|= m_nForceConditionsEx2;
	m_nPlayerCondEx3	|= m_nForceConditionsEx3;

	// Clear our force bits now that we've used them.
	m_nForceConditions = 0;
	m_nForceConditionsEx = 0;
	m_nForceConditionsEx2 = 0;
	m_nForceConditionsEx3 = 0;

	if ( m_nOldDisguiseClass != GetDisguiseClass() || m_nOldDisguiseTeam != GetDisguiseTeam() )
	{
		OnDisguiseChanged();
	}

	if ( m_hDisguiseWeapon )
	{
		m_hDisguiseWeapon->UpdateVisibility();
//		m_hDisguiseWeapon->UpdateParticleSystems();
	}

	// Only hide TF weapons
	CTFWeaponBase *pWeapon = dynamic_cast<CTFWeaponBase*>( GetActivePCWeapon() );
	if ( ( IsLoser() || InCond( TF_COND_COMPETITIVE_LOSER ) ) && pWeapon && !pWeapon->IsEffectActive( EF_NODRAW ) )
	{
		pWeapon->SetWeaponVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: check the newly networked conditions for changes
//-----------------------------------------------------------------------------
void CTFPlayerShared::SyncConditions( int nPreviousConditions, int nNewConditions, int nForceConditions, int nBaseCondBit )
{
	if ( nPreviousConditions == nNewConditions )
		return;

	int nCondChanged = nNewConditions ^ nPreviousConditions;
	int nCondAdded = nCondChanged & nNewConditions;
	int nCondRemoved = nCondChanged & nPreviousConditions;
	m_bSyncingConditions = true;

	for ( int i=0;i<32;i++ )
	{
		const int testBit = 1<<i;
		if ( nForceConditions & testBit )
		{
			if ( nPreviousConditions & testBit )
			{
				OnConditionRemoved((ETFCond)(nBaseCondBit + i));
			}
			OnConditionAdded((ETFCond)(nBaseCondBit + i));
		}
		else
		{
			if ( nCondAdded & testBit )
			{
				OnConditionAdded( (ETFCond)(nBaseCondBit + i) );
			}
			else if ( nCondRemoved & testBit )
			{
				OnConditionRemoved( (ETFCond)(nBaseCondBit + i) );
			}
		}
	}
	m_bSyncingConditions = false;
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Remove any conditions affecting players
//-----------------------------------------------------------------------------
void CTFPlayerShared::RemoveAllCond()
{
	m_ConditionList.RemoveAll();

	int i;
	for ( i=0;i<TF_COND_LAST;i++ )
	{
		if ( InCond( (ETFCond)i ) )
		{
			RemoveCond( (ETFCond)i );
		}
	}

	// Now remove all the rest
	m_nPlayerCond = 0;
	m_nPlayerCondEx = 0;
	m_nPlayerCondEx2 = 0;
	m_nPlayerCondEx3 = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Called on both client and server. Server when we add the bit,
// and client when it recieves the new cond bits and finds one added
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnConditionAdded( ETFCond eCond )
{
	switch( eCond )
	{
	case TF_COND_HEALTH_BUFF:
#ifdef GAME_DLL
		m_flHealFraction = 0;
		m_flDisguiseHealFraction = 0;

		m_flHealedPerSecondTimer = gpGlobals->curtime + 1.0f;
#endif
		break;

	case TF_COND_STEALTHED:
	case TF_COND_STEALTHED_USER_BUFF:
		OnAddStealthed();
		break;

	case TF_COND_INVULNERABLE:
	case TF_COND_INVULNERABLE_USER_BUFF:
	case TF_COND_INVULNERABLE_CARD_EFFECT:
		OnAddInvulnerable();
		break;

	case TF_COND_TELEPORTED:
		OnAddTeleported();
		break;

	case TF_COND_BURNING:
		OnAddBurning();
		break;

	case TF_COND_CRITBOOSTED:
		Assert( !"TF_COND_CRITBOOSTED should be handled by the condition list!" );
		break;

	// First blood falls through on purpose.
	//case TF_COND_CRITBOOSTED_FIRST_BLOOD:
	//	SetFirstBloodBoosted( true ); // SEALTODO
	case TF_COND_CRITBOOSTED_PUMPKIN:
	case TF_COND_CRITBOOSTED_USER_BUFF:
	case TF_COND_CRITBOOSTED_BONUS_TIME:
	case TF_COND_CRITBOOSTED_CTF_CAPTURE:
	case TF_COND_CRITBOOSTED_ON_KILL:
	case TF_COND_CRITBOOSTED_RAGE_BUFF:
	case TF_COND_SNIPERCHARGE_RAGE_BUFF:
	case TF_COND_CRITBOOSTED_CARD_EFFECT:
	case TF_COND_CRITBOOSTED_RUNE_TEMP:
		OnAddCritBoost();
		break;

	case TF_COND_DISGUISING:
		OnAddDisguising();
		break;

	case TF_COND_DISGUISED:
		OnAddDisguised();
		break;

	case TF_COND_TAUNTING:
		{
			CPCWeaponBase *pWpn = m_pOuter->GetActivePCWeapon();
			if ( pWpn )
			{
				// cancel any reload in progress.
				pWpn->AbortReload();
			}
		}
		break;

	case TF_COND_STUNNED:
		OnAddStunned();
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called on both client and server. Server when we remove the bit,
// and client when it recieves the new cond bits and finds one removed
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnConditionRemoved( ETFCond eCond )
{
	switch( eCond )
	{
	case TF_COND_ZOOMED:
		OnRemoveZoomed();
		break;

	case TF_COND_BURNING:
		OnRemoveBurning();
		break;

	case TF_COND_CRITBOOSTED:
		Assert( !"TF_COND_CRITBOOSTED should be handled by the condition list!" );
		break;

	// First blood falls through on purpose.
	//case TF_COND_CRITBOOSTED_FIRST_BLOOD:
	//	SetFirstBloodBoosted( false ); // SEALTODO
	case TF_COND_CRITBOOSTED_PUMPKIN:
	case TF_COND_CRITBOOSTED_USER_BUFF:
	case TF_COND_CRITBOOSTED_BONUS_TIME:
	case TF_COND_CRITBOOSTED_CTF_CAPTURE:
	case TF_COND_CRITBOOSTED_ON_KILL:
	case TF_COND_CRITBOOSTED_RAGE_BUFF:
	case TF_COND_SNIPERCHARGE_RAGE_BUFF:
	case TF_COND_CRITBOOSTED_CARD_EFFECT:
	case TF_COND_CRITBOOSTED_RUNE_TEMP:
		OnRemoveCritBoost();
		break;

	case TF_COND_HEALTH_BUFF:
#ifdef GAME_DLL
		m_flHealFraction = 0;
		m_flDisguiseHealFraction = 0;
#endif
		break;

	case TF_COND_STEALTHED:
	case TF_COND_STEALTHED_USER_BUFF:
		OnRemoveStealthed();
		break;

	case TF_COND_DISGUISED:
		OnRemoveDisguised();
		break;

	case TF_COND_DISGUISING:
		OnRemoveDisguising();
		break;

	case TF_COND_INVULNERABLE:
	case TF_COND_INVULNERABLE_USER_BUFF:
	case TF_COND_INVULNERABLE_CARD_EFFECT:
		OnRemoveInvulnerable();
		break;

	case TF_COND_TELEPORTED:
		OnRemoveTeleported();
		break;

	case TF_COND_STUNNED:
		OnRemoveStunned();
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the overheal bonus the specified healer is capable of buffing to
//-----------------------------------------------------------------------------
int CTFPlayerShared::GetMaxBuffedHealth( bool bIgnoreAttributes /*= false*/, bool bIgnoreHealthOverMax /*= false*/ )
{
	// Find the healer we have who's providing the most overheal
	float flBoostMax = m_pOuter->GetMaxHealthForBuffing() * tf_max_health_boost.GetFloat();
#ifdef GAME_DLL
	if ( !bIgnoreAttributes )
	{
		for ( int i = 0; i < m_aHealers.Count(); i++ )
		{
			float flOverheal = m_pOuter->GetMaxHealthForBuffing() * m_aHealers[i].flOverhealBonus;
			if ( flOverheal > flBoostMax )
			{
				flBoostMax = flOverheal;
			}
		}
	}
#endif

	int iRoundDown = floor( flBoostMax / 5 );
	iRoundDown = iRoundDown * 5;

	if ( !bIgnoreHealthOverMax )
	{
		// Don't allow overheal total to be less than the buffable + unbuffable max health or the current health
		int nBoostMin = MAX( m_pOuter->GetMaxHealth(), m_pOuter->GetHealth() );
		if ( iRoundDown < nBoostMin )
		{
			iRoundDown = nBoostMin;
		}
	}

	return iRoundDown;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayerShared::GetDisguiseMaxBuffedHealth( bool bIgnoreAttributes /*= false*/, bool bIgnoreHealthOverMax /*= false*/ )
{
	// Find the healer we have who's providing the most overheal
	float flBoostMax = GetDisguiseMaxHealth() * tf_max_health_boost.GetFloat();
#ifdef GAME_DLL
	if ( !bIgnoreAttributes )
	{
		for ( int i = 0; i < m_aHealers.Count(); i++ )
		{
			float flOverheal = GetDisguiseMaxHealth() * m_aHealers[i].flOverhealBonus;
			if ( flOverheal > flBoostMax )
			{
				flBoostMax = flOverheal;
			}
		}
	}
#endif

	int iRoundDown = floor( flBoostMax / 5 );
	iRoundDown = iRoundDown * 5;

	if ( !bIgnoreHealthOverMax )
	{
		// Don't allow overheal total to be less than the buffable + unbuffable max health or the current health
		int nBoostMin = MAX(GetDisguiseMaxHealth(), GetDisguiseHealth() );
		if ( iRoundDown < nBoostMin )
		{
			iRoundDown = nBoostMin;
		}
	}

	return iRoundDown;
}

//-----------------------------------------------------------------------------
bool ShouldRemoveConditionOnTimeout( ETFCond eCond )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Runs SERVER SIDE only Condition Think
// If a player needs something to be updated no matter what do it here (invul, etc).
//-----------------------------------------------------------------------------
void CTFPlayerShared::ConditionGameRulesThink( void )
{
#ifdef GAME_DLL
	m_ConditionList.ServerThink();

	if ( m_flNextCritUpdate < gpGlobals->curtime )
	{
		UpdateCritMult();
		m_flNextCritUpdate = gpGlobals->curtime + 0.5;
	}

	for ( int i=0; i < TF_COND_LAST; ++i )
	{
		// if we're in this condition and it's not already being handled by the condition list
		if ( InCond( (ETFCond)i ) && ((i >= 32) || !m_ConditionList.InCond( (ETFCond)i )) )
		{
			// Ignore permanent conditions
			if ( m_ConditionData[i].m_flExpireTime != PERMANENT_CONDITION )
			{
				float flReduction = gpGlobals->frametime;

				// If we're being healed, we reduce bad conditions faster
				if ( ConditionExpiresFast( (ETFCond)i) && m_aHealers.Count() > 0 )
				{
					if ( i == TF_COND_URINE )
					{
						flReduction += (m_aHealers.Count() * flReduction);
					}
					else
					{
						flReduction += (m_aHealers.Count() * flReduction * 4);
					}
				}

				m_ConditionData[i].m_flExpireTime = MAX( m_ConditionData[i].m_flExpireTime - flReduction, 0 );

				if ( m_ConditionData[i].m_flExpireTime == 0 )
				{
					RemoveCond( (ETFCond)i );
				}
			}
			else
			{
#if !defined( DEBUG )
				// Prevent hacked usercommand exploits
				if ( m_pOuter->GetTimeSinceLastUserCommand() > 5.f || m_pOuter->GetTimeSinceLastThink() > 5.f )
				{
					ETFCond eCond = (ETFCond)i;

					if ( ShouldRemoveConditionOnTimeout( eCond ) )
					{
						RemoveCond( eCond );

						// Reset active weapon to prevent stale-state bugs
						CPCWeaponBase *pTFWeapon = m_pOuter->GetActivePCWeapon();
						if ( pTFWeapon )
						{
							pTFWeapon->WeaponReset();
						}

						m_pOuter->TeamFortress_SetSpeed();
					}
				}
#endif
			}
		}
	}

	// Our health will only decay ( from being medic buffed ) if we are not being healed by a medic
	// Dispensers can give us the TF_COND_HEALTH_BUFF, but will not maintain or give us health above 100%s
	bool bDecayHealth = true;
	bool bDecayDisguiseHealth = true;

	// If we're being healed, heal ourselves
	if ( InCond( TF_COND_HEALTH_BUFF ) )
	{
		// Heal faster if we haven't been in combat for a while
		float flTimeSinceDamage = gpGlobals->curtime - m_pOuter->GetLastDamageReceivedTime();
		float flScale = RemapValClamped( flTimeSinceDamage, 10, 15, 1.0, 3.0 );
		float flAttribModScale = 1.0;

		float flCurOverheal = (float)m_pOuter->GetHealth() / (float)m_pOuter->GetMaxHealth();

		if ( flCurOverheal > 1.0f )
		{
			// If they're over their max health the overheal calculation is relative to the max buffable amount scale
			float flMaxHealthForBuffing = m_pOuter->GetMaxHealthForBuffing();
			float flBuffableRangeHealth = m_pOuter->GetHealth() - ( m_pOuter->GetMaxHealth() - flMaxHealthForBuffing );
			flCurOverheal = flBuffableRangeHealth / flMaxHealthForBuffing;
		}

		float flCurDisguiseOverheal = ( GetDisguiseMaxHealth() != 0 ) ? ( (float)GetDisguiseHealth() / (float)GetDisguiseMaxHealth() ) : ( flCurOverheal );

		float fTotalHealAmount = 0.0f;
		for ( int i = 0; i < m_aHealers.Count(); i++ )
		{
			Assert( m_aHealers[i].pHealer );

			float flPerHealerAttribModScale = 1.f;

			bool bHealDisguise = InCond( TF_COND_DISGUISED );
			bool bHealActual = true;

			// dispensers heal cloak
			if ( m_aHealers[i].bDispenserHeal )
			{
				AddToSpyCloakMeter( gpGlobals->frametime * m_aHealers[i].flAmount );	
			}

			// Don't heal over the healer's overheal bonus
			if ( flCurOverheal >= m_aHealers[i].flOverhealBonus )
			{
				bHealActual = false;
			}

			// Same overheal check, but for fake health
			if ( InCond( TF_COND_DISGUISED ) && flCurDisguiseOverheal >= m_aHealers[i].flOverhealBonus )
			{
				// Fake over-heal
				bHealDisguise = false;
			}

//			CTFPlayer *pTFHealer = ToTFPlayer( m_aHealers[i].pHealer );
			if ( !bHealActual && !bHealDisguise )
			{
				continue;
			}

			// Being healed by a medigun, don't decay our health
			if ( bHealActual )
			{
				bDecayHealth = false;
			}

			if ( bHealDisguise )
			{
				bDecayDisguiseHealth = false;
			}

			// What we multiply the heal amount by (can be changed by conditions or items).
			float flHealAmountMult = 1.0f;

			// Quick-Fix uber
			if ( InCond( TF_COND_MEGAHEAL ) )
			{
				flHealAmountMult = 3.0f;
			}

			flScale *= flHealAmountMult;

			// Dispensers heal at a constant rate
			if ( m_aHealers[i].bDispenserHeal )
			{
				// Dispensers heal at a slower rate, but ignore flScale
				if ( bHealActual )
				{
					float flDispenserFraction = gpGlobals->frametime * m_aHealers[i].flAmount * flAttribModScale;
					m_flHealFraction += flDispenserFraction;

					// track how much this healer has actually done so far
					m_aHealers[i].flHealAccum += clamp( flDispenserFraction, 0.f, (float) GetMaxBuffedHealth() - m_pOuter->GetHealth() );
				}
				if ( bHealDisguise )
				{
					m_flDisguiseHealFraction += gpGlobals->frametime * m_aHealers[i].flAmount * flAttribModScale;
				}
			}
			else	// player heals are affected by the last damage time
			{
				if ( bHealActual )
				{
					// Scale this if needed
					m_flHealFraction += gpGlobals->frametime * m_aHealers[i].flAmount * flScale * flAttribModScale * flPerHealerAttribModScale;
				}
				if ( bHealDisguise )
				{
					m_flDisguiseHealFraction += gpGlobals->frametime * m_aHealers[i].flAmount * flScale * flAttribModScale * flPerHealerAttribModScale;
				}
			}

			fTotalHealAmount += m_aHealers[i].flAmount;

			// Keep our decay multiplier uptodate
			if ( m_flBestOverhealDecayMult == -1 || m_aHealers[i].flOverhealDecayMult < m_flBestOverhealDecayMult )
			{
				m_flBestOverhealDecayMult = m_aHealers[i].flOverhealDecayMult;
			}
		}

		if ( InCond( TF_COND_HEALING_DEBUFF ) )
		{
			m_flHealFraction *= 0.75f;
		}

		int nHealthToAdd = (int)m_flHealFraction;
		int nDisguiseHealthToAdd = (int)m_flDisguiseHealFraction;
		if ( nHealthToAdd > 0 || nDisguiseHealthToAdd > 0 )
		{
			if ( nHealthToAdd > 0 )
			{
				m_flHealFraction -= nHealthToAdd;
			}

			if ( nDisguiseHealthToAdd > 0 )
			{
				m_flDisguiseHealFraction -= nDisguiseHealthToAdd;
			}

			int iBoostMax = GetMaxBuffedHealth();

			if ( InCond( TF_COND_DISGUISED ) )
			{
				// Separate cap for disguised health
				int nFakeHealthToAdd = clamp( nDisguiseHealthToAdd, 0, GetDisguiseMaxBuffedHealth() - m_iDisguiseHealth );
				m_iDisguiseHealth += nFakeHealthToAdd;
			}

			// Track health prior to healing
			int nPrevHealth = m_pOuter->GetHealth();
			
			// Cap it to the max we'll boost a player's health
			nHealthToAdd = clamp( nHealthToAdd, 0, iBoostMax - m_pOuter->GetHealth() );
			
			m_pOuter->TakeHealth( nHealthToAdd, DMG_IGNORE_MAXHEALTH | DMG_IGNORE_DEBUFFS );
			
			m_pOuter->AdjustDrownDmg( -1.0 * nHealthToAdd ); // subtract this from the drowndmg in case they're drowning and being healed at the same time

			// split up total healing based on the amount each healer contributes
			if ( fTotalHealAmount > 0 )
			{
				for ( int i = 0; i < m_aHealers.Count(); i++ )
				{
					Assert( m_aHealers[i].pHealScorer );
					Assert( m_aHealers[i].pHealer );
					if ( m_aHealers[i].pHealScorer.IsValid() && m_aHealers[i].pHealer.IsValid() )
					{
						CBaseEntity *pHealer = m_aHealers[i].pHealer;
						float flHealAmount = nHealthToAdd * ( m_aHealers[i].flAmount / fTotalHealAmount );

						if ( pHealer && IsAlly( pHealer ) )
						{
							CTFPlayer *pHealScorer = ToTFPlayer( m_aHealers[i].pHealScorer );
							if ( pHealScorer )
							{	
								// Don't report healing when we're close to the buff cap and haven't taken damage recently.
								// This avoids sending bogus heal stats while maintaining our max overheal.  Ideally we
								// wouldn't decay in this scenario, but that would be a risky change.
								if ( iBoostMax - nPrevHealth > 1 || gpGlobals->curtime - m_pOuter->GetLastDamageReceivedTime() <= 1.f )
								{
									CTF_GameStats.Event_PlayerHealedOther( pHealScorer, flHealAmount );
								}

								// Add this to the one-second-healing counter
								m_aHealers[i].flHealedLastSecond += flHealAmount;

								// If it's been one second, or we know healing beyond this point will be overheal, generate an event
								if ( ( m_flHealedPerSecondTimer <= gpGlobals->curtime || m_pOuter->GetHealth() >= m_pOuter->GetMaxHealth() ) 
									   && m_aHealers[i].flHealedLastSecond > 1 )
								{
									// Make sure this isn't pure overheal
									if ( m_pOuter->GetHealth() - m_aHealers[i].flHealedLastSecond < m_pOuter->GetMaxHealth() )
									{
										float flOverHeal = m_pOuter->GetHealth() - m_pOuter->GetMaxHealth();
										if ( flOverHeal > 0 )
										{
											m_aHealers[i].flHealedLastSecond -= flOverHeal;
										}

										// TEST THIS
										// Give the medic some uber if it is from their (AoE heal) which has no overheal
										if ( m_aHealers[i].flOverhealBonus <= 1.0f )
										{
											// Give a litte bit of uber based on actual healing
											// Give them a little bit of Uber
											CWeaponMedigun *pMedigun = static_cast<CWeaponMedigun *>( pHealScorer->Weapon_OwnsThisID( TF_WEAPON_MEDIGUN ) );
											if ( pMedigun )
											{
												// On Mediguns, per frame, the amount of uber added is based on 
												// Default heal rate is 24per second, we scale based on that and frametime
												pMedigun->AddCharge( ( m_aHealers[i].flHealedLastSecond / 24.0f ) * gpGlobals->frametime * 0.33f );
											}
										}

										IGameEvent * event = gameeventmanager->CreateEvent( "player_healed" );
										if ( event )
										{
											// HLTV event priority, not transmitted
											event->SetInt( "priority", 1 );	

											// Healed by another player.
											event->SetInt( "patient", m_pOuter->GetUserID() );
											event->SetInt( "healer", pHealScorer->GetUserID() );
											event->SetInt( "amount", m_aHealers[i].flHealedLastSecond );
											gameeventmanager->FireEvent( event );
										}
									}

									m_aHealers[i].flHealedLastSecond = 0;
									m_flHealedPerSecondTimer = gpGlobals->curtime + 1.0f;
								}
							}
						}
						else
						{
							CTF_GameStats.Event_PlayerLeachedHealth( m_pOuter, m_aHealers[i].bDispenserHeal, flHealAmount );
						}
					}
				}
			}
		}

		if ( InCond( TF_COND_BURNING ) )
		{
			// Reduce the duration of this burn 
			float flReduction = 2;	 // ( flReduction + 1 ) x faster reduction
			m_flFlameRemoveTime -= flReduction * gpGlobals->frametime;
		}
		//if ( InCond( TF_COND_BLEEDING ) )
		//{
		//	// Reduce the duration of this bleeding 
		//	float flReduction = 2;	 // ( flReduction + 1 ) x faster reduction
		//	FOR_EACH_VEC( m_PlayerBleeds, i )
		//	{
		//		m_PlayerBleeds[i].flBleedingRemoveTime -= flReduction * gpGlobals->frametime;
		//	}
		//}
	}

	if ( !InCond( TF_COND_HEALTH_OVERHEALED ) && m_pOuter->GetHealth() > m_pOuter->GetMaxHealth() )
	{
		AddCond( TF_COND_HEALTH_OVERHEALED, PERMANENT_CONDITION );
	}
	else if ( InCond( TF_COND_HEALTH_OVERHEALED ) && m_pOuter->GetHealth() <= m_pOuter->GetMaxHealth() )
	{
		RemoveCond( TF_COND_HEALTH_OVERHEALED );
	}

	if ( bDecayHealth )
	{
		float flOverheal = GetMaxBuffedHealth( false, true );

		// If we're not being buffed, our health drains back to our max
		if ( m_pOuter->GetHealth() > m_pOuter->GetMaxHealth() )
		{
			// Items exist that get us over max health, without ever being healed, in which case our m_flBestOverhealDecayMult will still be -1.
			float flDrainMult = (m_flBestOverhealDecayMult == -1) ? 1.0 : m_flBestOverhealDecayMult;
			float flBoostMaxAmount = flOverheal - m_pOuter->GetMaxHealth();
			float flDrain = flBoostMaxAmount / (tf_boost_drain_time.GetFloat() * flDrainMult);
			m_flHealFraction += (gpGlobals->frametime * flDrain);

			int nHealthToDrain = (int)m_flHealFraction;
			if ( nHealthToDrain > 0 )
			{
				m_flHealFraction -= nHealthToDrain;

				// Manually subtract the health so we don't generate pain sounds / etc
				m_pOuter->m_iHealth -= nHealthToDrain;
			}
		}
		else if ( m_flBestOverhealDecayMult != -1 )
		{
			m_flBestOverhealDecayMult = -1;
		}

	}

	if ( bDecayDisguiseHealth )
	{
		float flOverheal = GetDisguiseMaxBuffedHealth( false, true );

		if ( InCond( TF_COND_DISGUISED ) && (GetDisguiseHealth() > GetDisguiseMaxHealth()) )
		{
			// Items exist that get us over max health, without ever being healed, in which case our m_flBestOverhealDecayMult will still be -1.
			float flDrainMult = (m_flBestOverhealDecayMult == -1) ? 1.0 : m_flBestOverhealDecayMult;
			float flBoostMaxAmount = flOverheal - GetDisguiseMaxHealth();
			float flDrain = (flBoostMaxAmount / tf_boost_drain_time.GetFloat()) * flDrainMult;
			m_flDisguiseHealFraction += (gpGlobals->frametime * flDrain);

			int nHealthToDrain = (int)m_flDisguiseHealFraction;
			if ( nHealthToDrain > 0 )
			{
				m_flDisguiseHealFraction -= nHealthToDrain;

				// Reduce our fake disguised health by roughly the same amount
				m_iDisguiseHealth -= nHealthToDrain;
			}
		}
	}

	// Taunt
	if ( InCond( TF_COND_TAUNTING ) )
	{
		if ( m_pOuter->IsAllowedToRemoveTaunt() && gpGlobals->curtime > m_pOuter->GetTauntRemoveTime() )
		{
			RemoveCond( TF_COND_TAUNTING );
		}
	}

	if ( InCond( TF_COND_BURNING ) && ( m_pOuter->m_flPowerPlayTime < gpGlobals->curtime ) )
	{
		// If we're underwater, put the fire out
		if ( gpGlobals->curtime > m_flFlameRemoveTime || m_pOuter->GetWaterLevel() >= WL_Waist )
		{
			RemoveCond( TF_COND_BURNING );
		}
		else if ( ( gpGlobals->curtime >= m_flFlameBurnTime ) && ( TF_CLASS_PYRO != m_pOuter->GetPlayerClass()->GetClassIndex() ) )
		{
			// Burn the player (if not pyro, who does not take persistent burning damage)
			CTakeDamageInfo info( m_hBurnAttacker, m_hBurnAttacker, TF_BURNING_DMG, DMG_BURN | DMG_PREVENT_PHYSICS_FORCE, TF_DMG_CUSTOM_BURNING );
			m_pOuter->TakeDamage( info );
			m_flFlameBurnTime = gpGlobals->curtime + TF_BURNING_FREQUENCY;
		}

		if ( m_flNextBurningSound < gpGlobals->curtime )
		{
			m_pOuter->SpeakConceptIfAllowed( MP_CONCEPT_ONFIRE );
			m_flNextBurningSound = gpGlobals->curtime + 2.5;
		}
	}

	if ( InCond( TF_COND_DISGUISING ) )
	{
		if ( gpGlobals->curtime > m_flDisguiseCompleteTime )
		{
			CompleteDisguise();
		}
	}

	// Stops the drain hack.
	if ( m_pOuter->IsPlayerClass( TF_CLASS_MEDIC ) )
	{
		CWeaponMedigun *pWeapon = ( CWeaponMedigun* )m_pOuter->Weapon_OwnsThisID( TF_WEAPON_MEDIGUN );
		if ( pWeapon && pWeapon->IsReleasingCharge() )
		{
			pWeapon->DrainCharge();
		}
	}

	if ( InCond( TF_COND_INVULNERABLE )  )
	{
		bool bRemoveInvul = false;

		if ( ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN ) && ( TFGameRules()->GetWinningTeam() != m_pOuter->GetTeamNumber() ) )
		{
			bRemoveInvul = true;
		}
		
		if ( m_flInvulnerableOffTime )
		{
			if ( gpGlobals->curtime > m_flInvulnerableOffTime )
			{
				bRemoveInvul = true;
			}
		}

		if ( bRemoveInvul == true )
		{
			m_flInvulnerableOffTime = 0;
			RemoveCond( TF_COND_INVULNERABLE_WEARINGOFF );
			RemoveCond( TF_COND_INVULNERABLE );
		}
	}

	if ( InCond( TF_COND_STEALTHED_BLINK ) )
	{
		float flBlinkTime = TF_SPY_STEALTH_BLINKTIME;
		if ( flBlinkTime < ( gpGlobals->curtime - m_flLastStealthExposeTime ) )
		{
			RemoveCond( TF_COND_STEALTHED_BLINK );
		}
	}

	if ( !InCond( TF_COND_DISGUISED ) )
	{
		// Remove our disguise weapon if we are ever not disguised and we have one.
		RemoveDisguiseWeapon();

		// also clear the disguise weapon list
		m_pOuter->ClearDisguiseWeaponList();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Do CLIENT/SERVER SHARED condition thinks.
//-----------------------------------------------------------------------------
void CTFPlayerShared::ConditionThink( void )
{
	// Client Only Updates Meters for Local Only
#ifdef CLIENT_DLL
	if ( m_pOuter->IsLocalPlayer() )
#endif
	{
		UpdateCloakMeter();
	}

	m_ConditionList.Think();

	if ( InCond( TF_COND_STUNNED ) )
	{
#ifdef GAME_DLL
		if ( IsControlStunned() )
		{
			m_pOuter->SetAbsAngles( m_pOuter->m_angTauntCamera );
			m_pOuter->SetLocalAngles( m_pOuter->m_angTauntCamera );
		}
#endif
		if ( GetActiveStunInfo() && gpGlobals->curtime > GetActiveStunInfo()->flExpireTime )
		{
#ifdef GAME_DLL	
			m_PlayerStuns.Remove( m_iStunIndex );
			m_iStunIndex = -1;

			// Apply our next stun
			if ( m_PlayerStuns.Count() )
			{
				int iStrongestIdx = 0;
				for ( int i = 1; i < m_PlayerStuns.Count(); i++ )
				{
					if ( m_PlayerStuns[i].flStunAmount > m_PlayerStuns[iStrongestIdx].flStunAmount )
					{
						iStrongestIdx = i;
					}
				}
				m_iStunIndex = iStrongestIdx;

				AddCond( TF_COND_STUNNED, -1.f, m_PlayerStuns[m_iStunIndex].hPlayer );
				m_iMovementStunParity = ( m_iMovementStunParity + 1 ) & ( ( 1 << MOVEMENTSTUN_PARITY_BITS ) - 1 ); 

				Assert( GetActiveStunInfo() );
			}
			else
			{
				RemoveCond( TF_COND_STUNNED );
			}
#endif // GAME_DLL

			UpdateLegacyStunSystem();
		}
		else if ( IsControlStunned() && GetActiveStunInfo() && ( gpGlobals->curtime > GetActiveStunInfo()->flStartFadeTime ) )
		{
			// Control stuns have a final anim to play.
			ControlStunFading();
		}

#ifdef CLIENT_DLL
		// turn off stun effect that gets turned on when incomplete stun msg is received on the client
		if ( GetActiveStunInfo() && GetActiveStunInfo()->iStunFlags & TF_STUN_NO_EFFECTS )
		{
			if ( m_pOuter->m_pStunnedEffect )
			{
				// Remove stun stars if they are still around.
				// They might be if we died, etc.
				m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pStunnedEffect );
				m_pOuter->m_pStunnedEffect = NULL;
			}
		}
#endif
	}

	CheckDisguiseTimer();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::CheckDisguiseTimer( void )
{
	if ( InCond( TF_COND_DISGUISING ) && GetDisguiseCompleteTime() > 0 )
	{
		if ( gpGlobals->curtime > GetDisguiseCompleteTime() )
		{
			CompleteDisguise();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveZoomed( void )
{
#ifdef GAME_DLL
	m_pOuter->SetFOV( m_pOuter, 0, 0.1f );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddDisguising( void )
{
#ifdef CLIENT_DLL
	if ( m_pOuter->GetPredictable() && ( !prediction->IsFirstTimePredicted() || m_bSyncingConditions ) )
		return;

	if ( m_pOuter->m_pDisguisingEffect )
	{
//		m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pDisguisingEffect );
	}

	if ( !m_pOuter->IsLocalPlayer() && ( !IsStealthed() || !m_pOuter->IsEnemyPlayer() ) )
	{
		const char *pEffectName = ( m_pOuter->GetTeamNumber() == TF_TEAM_RED ) ? "spy_start_disguise_red" : "spy_start_disguise_blue";
		m_pOuter->m_pDisguisingEffect = m_pOuter->ParticleProp()->Create( pEffectName, PATTACH_ABSORIGIN_FOLLOW );
		m_pOuter->m_flDisguiseEffectStartTime = gpGlobals->curtime;
	}

	m_pOuter->EmitSound( "Player.Spy_Disguise" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: set up effects for when player finished disguising
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddDisguised( void )
{
#ifdef CLIENT_DLL
	if ( m_pOuter->m_pDisguisingEffect )
	{
		// turn off disguising particles
//		m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pDisguisingEffect );
		m_pOuter->m_pDisguisingEffect = NULL;
	}
	m_pOuter->m_flDisguiseEndEffectStartTime = gpGlobals->curtime;

	UpdateCritBoostEffect( kCritBoost_ForceRefresh );

	m_pOuter->UpdateSpyStateChange();
#endif
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Return the team that the spy is displayed as.
// Not disguised: His own team
// Disguised: The team he is disguised as
//-----------------------------------------------------------------------------
int CTFPlayerShared::GetDisplayedTeam( void ) const
{
	int iVisibleTeam = m_pOuter->GetTeamNumber();
	// if this player is disguised and on the other team, use disguise team
	if ( InCond( TF_COND_DISGUISED ) && m_pOuter->IsEnemyPlayer() )
	{
		iVisibleTeam = GetDisguiseTeam();
	}

	return iVisibleTeam;
}

//-----------------------------------------------------------------------------
// Purpose: start, end, and changing disguise classes
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnDisguiseChanged( void )
{
	m_pOuter->UpdateSpyStateChange();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddInvulnerable( void )
{
#ifdef CLIENT_DLL

	if ( m_pOuter->IsLocalPlayer() )
	{
		const char *pEffectName = NULL;

		switch( m_pOuter->GetTeamNumber() )
		{
		case TF_TEAM_BLUE:
		default:
			pEffectName = TF_SCREEN_OVERLAY_MATERIAL_INVULN_BLUE;
			break;
		case TF_TEAM_RED:
			pEffectName =  TF_SCREEN_OVERLAY_MATERIAL_INVULN_RED;
			break;
		}

		IMaterial *pMaterial = materials->FindMaterial( pEffectName, TEXTURE_GROUP_CLIENT_EFFECTS, false );
		if ( !IsErrorMaterial( pMaterial ) )
		{
			view->SetScreenOverlayMaterial( pMaterial );
		}
	}
#else
	// remove any persistent damaging conditions
	if ( InCond( TF_COND_BURNING ) )
	{
		RemoveCond( TF_COND_BURNING );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveInvulnerable( void )
{
#ifdef CLIENT_DLL
	if ( m_pOuter->IsLocalPlayer() )
	{
		// only remove the overlay if it is an invuln material 
		IMaterial *pMaterial = view->GetScreenOverlayMaterial();

		if ( pMaterial &&
				( FStrEq( pMaterial->GetName(), TF_SCREEN_OVERLAY_MATERIAL_INVULN_BLUE ) || 
				  FStrEq( pMaterial->GetName(), TF_SCREEN_OVERLAY_MATERIAL_INVULN_RED ) ) )
		{
			view->SetScreenOverlayMaterial( NULL );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddTeleported( void )
{
#ifdef CLIENT_DLL
	m_pOuter->OnAddTeleported();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveTeleported( void )
{
#ifdef CLIENT_DLL
	m_pOuter->OnRemoveTeleported();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddTaunting( void )
{
	CPCWeaponBase *pWpn = m_pOuter->GetActivePCWeapon();
	if ( pWpn )
	{
		// cancel any reload in progress.
		pWpn->AbortReload();
	}

	// Unzoom if we are a sniper zoomed!
	InstantlySniperUnzoom();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveTaunting( void )
{
#ifdef GAME_DLL
	m_pOuter->StopTaunt();

	if ( IsControlStunned() )
	{
		m_pOuter->SetAbsAngles( m_pOuter->m_angTauntCamera );
		m_pOuter->SetLocalAngles( m_pOuter->m_angTauntCamera );
	}
#endif // GAME_DLL

	m_pOuter->GetGameAnimStateType()->ResetGestureSlot( GESTURE_SLOT_VCD );

	// when we stop taunting, make sure active weapon is visible
	if ( m_pOuter->GetActiveWeapon() )
	{
		m_pOuter->GetActiveWeapon()->SetWeaponVisible( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::Burn( CTFPlayer *pAttacker, float flBurningTime /*=-1*/ )
{
#ifdef CLIENT_DLL

#else
	// Don't bother igniting players who have just been killed by the fire damage.
	if ( !m_pOuter->IsAlive() )
		return;

	//Don't ignite if I'm in phase mode.
	if ( InCond( TF_COND_PHASE ) || InCond( TF_COND_PASSTIME_INTERCEPTION ) )
		return;

	// pyros don't burn persistently or take persistent burning damage, but we show brief burn effect so attacker can tell they hit
	bool bVictimIsPyro = ( TF_CLASS_PYRO ==  m_pOuter->GetPlayerClass()->GetClassIndex() );

	if ( !InCond( TF_COND_BURNING ) )
	{
		// Start burning
		AddCond( TF_COND_BURNING, -1.f, pAttacker );
		m_flFlameBurnTime = gpGlobals->curtime;	//asap
		// let the attacker know he burned me
		if ( pAttacker && !bVictimIsPyro )
		{
			pAttacker->OnBurnOther( m_pOuter );
		}
	}

	int nAfterburnImmunity = 0;

	if ( InCond( TF_COND_AFTERBURN_IMMUNE ) )
	{
		nAfterburnImmunity = 1;
		m_flFlameRemoveTime = 0;
	}

	float flFlameLife;
	if ( bVictimIsPyro || nAfterburnImmunity )
	{
		flFlameLife = TF_BURNING_FLAME_LIFE_PYRO;
	}
	else if ( flBurningTime > 0 )
	{
		flFlameLife = flBurningTime;
	}
	else
	{
		flFlameLife = TF_BURNING_FLAME_LIFE;
	}
	
	//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flFlameLife, mult_wpn_burntime );

	float flBurnEnd = gpGlobals->curtime + flFlameLife;
	if ( flBurnEnd > m_flFlameRemoveTime )
	{
		m_flFlameRemoveTime = flBurnEnd;
	}

	m_hBurnAttacker = pAttacker;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveBurning( void )
{
#ifdef CLIENT_DLL
	m_pOuter->StopBurningSound();

	if ( m_pOuter->m_pBurningEffect )
	{
		m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pBurningEffect );
		m_pOuter->m_pBurningEffect = NULL;
	}

	if ( m_pOuter->IsLocalPlayer() )
	{
		view->SetScreenOverlayMaterial( NULL );
	}

	m_pOuter->m_flBurnEffectStartTime = 0;
	m_pOuter->m_flBurnEffectEndTime = 0;
#else
	m_hBurnAttacker = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveCritBoost( void )
{
#ifdef CLIENT_DLL
	UpdateCritBoostEffect();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveTmpDamageBonus( void )
{
	m_flTmpDamageBonusAmount = 1.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddStealthed( void )
{
#ifdef CLIENT_DLL
	if ( m_pOuter->GetPredictable() && ( !prediction->IsFirstTimePredicted() || m_bSyncingConditions ) )
		return;

	if ( !InCond( TF_COND_FEIGN_DEATH ) )
	{
		m_pOuter->EmitSound( "Player.Spy_Cloak" );
	}
	m_pOuter->RemoveAllDecals();
	UpdateCritBoostEffect();
#endif

	bool bSetInvisChangeTime = true;
#ifdef CLIENT_DLL
	if ( !m_pOuter->IsLocalPlayer() )
	{
		// We only clientside predict changetime for the local player
		bSetInvisChangeTime = false;
	}

	if ( InCond( TF_COND_STEALTHED_USER_BUFF ) && m_pOuter->IsLocalPlayer() )
	{
		IMaterial *pMaterial = materials->FindMaterial( TF_SCREEN_OVERLAY_MATERIAL_STEALTH, TEXTURE_GROUP_CLIENT_EFFECTS, false );
		if ( !IsErrorMaterial( pMaterial ) )
		{
			view->SetScreenOverlayMaterial( pMaterial );
		}
	}
#endif

	if ( bSetInvisChangeTime )
	{
		if ( !InCond( TF_COND_FEIGN_DEATH ) && !InCond( TF_COND_STEALTHED_USER_BUFF ) )
		{
			float flInvisTime = tf_spy_invis_time.GetFloat();
			m_flInvisChangeCompleteTime = gpGlobals->curtime + flInvisTime;
		}
		else
		{
			m_flInvisChangeCompleteTime = gpGlobals->curtime; // Stealth immediately if we are in feign death.
		}
	}

	// set our offhand weapon to be the invis weapon, but only for the spy's stealth
	if ( InCond( TF_COND_STEALTHED ) )
	{
		for (int i = 0; i < m_pOuter->WeaponCount(); i++) 
		{
			CTFWeaponInvis *pWpn = (CTFWeaponInvis *) m_pOuter->GetWeapon(i);
			if ( !pWpn )
				continue;

			if ( pWpn->GetWeaponID() != TF_WEAPON_INVIS )
				continue;

			// try to switch to this weapon
			m_pOuter->SetOffHandWeapon( pWpn );
			break;
		}
	}

	m_pOuter->TeamFortress_SetSpeed();

#ifdef CLIENT_DLL
	m_pOuter->UpdateSpyStateChange();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveStealthed( void )
{
#ifdef CLIENT_DLL
	if ( !m_bSyncingConditions )
		return;

	m_pOuter->EmitSound( "Player.Spy_UnCloak" );
	UpdateCritBoostEffect( kCritBoost_ForceRefresh );

	if ( m_pOuter->IsLocalPlayer() && !InCond( TF_COND_STEALTHED_USER_BUFF_FADING ) )
	{
		IMaterial *pMaterial = view->GetScreenOverlayMaterial();
		if ( pMaterial && FStrEq( pMaterial->GetName(), TF_SCREEN_OVERLAY_MATERIAL_STEALTH ) )
		{
			view->SetScreenOverlayMaterial( NULL );
		}
	}
#endif

	// End feign death if we leave stealth for some reason.
	if ( InCond( TF_COND_FEIGN_DEATH ) )
	{
		RemoveCond( TF_COND_FEIGN_DEATH );
	}

	m_pOuter->HolsterOffHandWeapon();

	m_pOuter->TeamFortress_SetSpeed();

#ifdef CLIENT_DLL
	m_pOuter->UpdateSpyStateChange();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveDisguising( void )
{
#ifdef CLIENT_DLL
	if ( m_pOuter->m_pDisguisingEffect )
	{
		m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pDisguisingEffect );
		m_pOuter->m_pDisguisingEffect = NULL;
	}
#else
	m_nDesiredDisguiseTeam = TF_SPY_UNDEFINED;

	// Do not reset this value, we use the last desired disguise class for the
	// 'lastdisguise' command

	//m_nDesiredDisguiseClass = TF_CLASS_UNDEFINED;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveDisguised( void )
{
#ifdef CLIENT_DLL
	if ( m_pOuter->GetPredictable() && ( !prediction->IsFirstTimePredicted() || m_bSyncingConditions ) )
		return;

	// if local player is on the other team, reset the model of this player
	CTFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !m_pOuter->InSameTeam( pLocalPlayer ) )
	{
		TFPlayerClassData_t *pData = GetPlayerClassData( TF_CLASS_SPY );
		int iIndex = modelinfo->GetModelIndex( pData->GetModelName() );

		m_pOuter->SetModelIndex( iIndex );
	}

	m_pOuter->EmitSound( "Player.Spy_Disguise" );

	// They may have called for medic and created a visible medic bubble
	m_pOuter->ParticleProp()->StopParticlesNamed( "speech_mediccall", true );

	UpdateCritBoostEffect( kCritBoost_ForceRefresh );
	m_pOuter->UpdateSpyStateChange();
#else
	m_nDisguiseTeam  = TF_SPY_UNDEFINED;
	m_nDisguiseClass.Set( TF_CLASS_UNDEFINED );
	m_nDisguiseSkinOverride = 0;
	m_hDisguiseTarget.Set( NULL );
	m_iDisguiseTargetIndex = TF_DISGUISE_TARGET_INDEX_NONE;
	m_iDisguiseHealth = 0;
	SetDisguiseBody( 0 );
	m_iDisguiseAmmo = 0;

	// Update the player model and skin.
	m_pOuter->UpdateModel();

	m_pOuter->ClearExpression();

	m_pOuter->ClearDisguiseWeaponList();

	RemoveDisguiseWeapon();
#endif
	m_pOuter->TeamFortress_SetSpeed();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddBurning( void )
{
#ifdef CLIENT_DLL
	// Start the burning effect
	if ( !m_pOuter->m_pBurningEffect )
	{
		const char *pEffectName = ( m_pOuter->GetTeamNumber() == TF_TEAM_RED ) ? "burningplayer_red" : "burningplayer_blue";
		m_pOuter->m_pBurningEffect = m_pOuter->ParticleProp()->Create( pEffectName, PATTACH_ABSORIGIN_FOLLOW );

		m_pOuter->m_flBurnEffectStartTime = gpGlobals->curtime;
		m_pOuter->m_flBurnEffectEndTime = gpGlobals->curtime + TF_BURNING_FLAME_LIFE;
	}
	// set the burning screen overlay
	if ( m_pOuter->IsLocalPlayer() )
	{
		IMaterial *pMaterial = materials->FindMaterial( "effects/imcookin", TEXTURE_GROUP_CLIENT_EFFECTS, false );
		if ( !IsErrorMaterial( pMaterial ) )
		{
			view->SetScreenOverlayMaterial( pMaterial );
		}
	}
#endif

	/*
#ifdef GAME_DLL
	
	if ( player == robin || player == cook )
	{
		CSingleUserRecipientFilter filter( m_pOuter );
		TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_SPECIAL );
	}

#endif
	*/

	// play a fire-starting sound
	m_pOuter->EmitSound( "Fire.Engulf" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddStunned( void )
{
	if ( IsControlStunned() || IsLoserStateStunned() )
	{
#ifdef CLIENT_DLL
		if ( GetActiveStunInfo() )
		{
			if ( !m_pOuter->m_pStunnedEffect && !( GetActiveStunInfo()->iStunFlags & TF_STUN_NO_EFFECTS ) )
			{
				if ( ( GetActiveStunInfo()->iStunFlags & TF_STUN_BY_TRIGGER ) || InCond( TF_COND_HALLOWEEN_BOMB_HEAD ) )
				{
					const char* pEffectName = "yikes_fx";
					m_pOuter->m_pStunnedEffect = m_pOuter->ParticleProp()->Create( pEffectName, PATTACH_POINT_FOLLOW, "head" );
				}
				else
				{
					const char* pEffectName = "conc_stars";
					m_pOuter->m_pStunnedEffect = m_pOuter->ParticleProp()->Create( pEffectName, PATTACH_POINT_FOLLOW, "head" );
				}
			}
		}
#endif

		// Notify our weapon that we have been stunned.
		CPCWeaponBase* pWpn = m_pOuter->GetActivePCWeapon();
		if ( pWpn )
		{
			CTFWeaponBase *pTFWeapon = dynamic_cast<CTFWeaponBase*>( pWpn );
			if ( pTFWeapon )
				pTFWeapon->OnControlStunned();
		}

		m_pOuter->TeamFortress_SetSpeed();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveStunned( void )
{
	m_iStunFlags = 0;
	m_hStunner = NULL;

#ifdef CLIENT_DLL
	if ( m_pOuter->m_pStunnedEffect )
	{
		// Remove stun stars if they are still around.
		// They might be if we died, etc.
		m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pStunnedEffect );
		m_pOuter->m_pStunnedEffect = NULL;
	}
#else
	m_iStunIndex = -1;
	m_PlayerStuns.RemoveAll();
#endif

	m_pOuter->TeamFortress_SetSpeed();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::ControlStunFading( void )
{
#ifdef CLIENT_DLL
	if ( m_pOuter->m_pStunnedEffect )
	{
		// Remove stun stars early...
		m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pStunnedEffect );
		m_pOuter->m_pStunnedEffect = NULL;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::SetStunExpireTime( float flTime )
{
#ifdef GAME_DLL
	if ( GetActiveStunInfo() ) 
	{
		GetActiveStunInfo()->flExpireTime = flTime;
	}
#else
	m_flStunEnd = flTime;
#endif

	UpdateLegacyStunSystem();
}

//-----------------------------------------------------------------------------
// Purpose: Mirror stun info to the old system for networking
//-----------------------------------------------------------------------------
void CTFPlayerShared::UpdateLegacyStunSystem( void )
{
	// What a mess.
#ifdef GAME_DLL
	stun_struct_t *pStun = GetActiveStunInfo();
	if ( pStun )
	{
		m_hStunner = pStun->hPlayer;
		m_flStunFade = gpGlobals->curtime + pStun->flDuration;
		m_flMovementStunTime = pStun->flDuration; 
		m_flStunEnd = pStun->flExpireTime;
		if ( pStun->iStunFlags & TF_STUN_CONTROLS )
		{
			m_flStunEnd = pStun->flExpireTime;
		}
		m_iMovementStunAmount = pStun->flStunAmount; 
		m_iStunFlags = pStun->iStunFlags;

		m_iMovementStunParity = ( m_iMovementStunParity + 1 ) & ( ( 1 << MOVEMENTSTUN_PARITY_BITS ) - 1 ); 
	}
#else
	m_ActiveStunInfo.hPlayer = m_hStunner;
	m_ActiveStunInfo.flDuration = m_flMovementStunTime;
	m_ActiveStunInfo.flExpireTime = m_flStunEnd;
	m_ActiveStunInfo.flStartFadeTime = m_flStunEnd;
	m_ActiveStunInfo.flStunAmount = m_iMovementStunAmount;
	m_ActiveStunInfo.iStunFlags = m_iStunFlags;
#endif // GAME_DLL
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
stun_struct_t *CTFPlayerShared::GetActiveStunInfo( void ) const
{
#ifdef GAME_DLL
	return ( m_PlayerStuns.IsValidIndex( m_iStunIndex ) ) ? const_cast<stun_struct_t*>( &m_PlayerStuns[m_iStunIndex] ) : NULL;
#else
	return ( m_iStunIndex >= 0 ) ? const_cast<stun_struct_t*>( &m_ActiveStunInfo ) : NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFPlayer *CTFPlayerShared::GetStunner( void )
{ 
	return GetActiveStunInfo() ? GetActiveStunInfo()->hPlayer : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddCritBoost( void )
{
#ifdef CLIENT_DLL
	UpdateCritBoostEffect();
#endif
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::UpdateCritBoostEffect( ECritBoostUpdateType eUpdateType )
{
	bool bShouldDisplayCritBoostEffect = IsCritBoosted()
									  || InCond( TF_COND_ENERGY_BUFF )
									  //|| IsHypeBuffed()
									  || InCond( TF_COND_SNIPERCHARGE_RAGE_BUFF );

	CTFWeaponBase* pWeapon = dynamic_cast<CTFWeaponBase*>( m_pOuter->GetActivePCWeapon() );
	if ( pWeapon )
	{
		bShouldDisplayCritBoostEffect &= pWeapon->CanBeCritBoosted();
	}

	// Never show crit boost effects when stealthed
	bShouldDisplayCritBoostEffect &= !IsStealthed();

	// Never show crit boost effects when disguised unless we're the local player (so crits show on our viewmodel)
	if ( !m_pOuter->IsLocalPlayer() )
	{
		bShouldDisplayCritBoostEffect &= !InCond( TF_COND_DISGUISED );
	}

	// Remove our current crit-boosted effect if we're forcing a refresh (in which case we'll
	// regenerate an effect below) or if we aren't supposed to have an effect active.
	if ( eUpdateType == kCritBoost_ForceRefresh || !bShouldDisplayCritBoostEffect )
	{
		if ( m_pOuter->m_pCritBoostEffect )
		{
			Assert( m_pOuter->m_pCritBoostEffect->IsValid() );
			if ( m_pOuter->m_pCritBoostEffect->GetOwner() )
			{
				m_pOuter->m_pCritBoostEffect->GetOwner()->ParticleProp()->StopEmissionAndDestroyImmediately( m_pOuter->m_pCritBoostEffect );
			}
			else
			{
				m_pOuter->m_pCritBoostEffect->StopEmission();
			}

			m_pOuter->m_pCritBoostEffect = NULL;
		}

#ifdef CLIENT_DLL
		if ( m_pCritBoostSoundLoop )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( m_pCritBoostSoundLoop );
			m_pCritBoostSoundLoop = NULL;
		}
#endif
	}

	// Should we have an active crit effect?
	if ( bShouldDisplayCritBoostEffect )
	{
		CBaseEntity *pWeapon = NULL;
		// Use GetRenderedWeaponModel() instead?
		if ( m_pOuter->IsLocalPlayer() )
		{
			pWeapon = m_pOuter->GetViewModel(0);
		}
		else
		{
			// is this player an enemy?
			if ( m_pOuter->GetTeamNumber() != GetLocalPlayerTeam() )
			{
				// are they a cloaked spy? or disguised as someone who almost assuredly isn't also critboosted?
				if ( IsStealthed() || InCond( TF_COND_STEALTHED_BLINK ) || InCond( TF_COND_DISGUISED ) )
					return;
			}

			pWeapon = m_pOuter->GetActiveWeapon();
		}

		if ( pWeapon )
		{
			if ( !m_pOuter->m_pCritBoostEffect )
			{
				if ( InCond( TF_COND_DISGUISED ) && !m_pOuter->IsLocalPlayer() && m_pOuter->GetTeamNumber() != GetLocalPlayerTeam() )
				{
					const char *pEffectName = ( GetDisguiseTeam() == TF_TEAM_RED ) ? "critgun_weaponmodel_red" : "critgun_weaponmodel_blu";
					m_pOuter->m_pCritBoostEffect = pWeapon->ParticleProp()->Create( pEffectName, PATTACH_ABSORIGIN_FOLLOW );
				}
				else
				{
					const char *pEffectName = ( m_pOuter->GetTeamNumber() == TF_TEAM_RED ) ? "critgun_weaponmodel_red" : "critgun_weaponmodel_blu";
					m_pOuter->m_pCritBoostEffect = pWeapon->ParticleProp()->Create( pEffectName, PATTACH_ABSORIGIN_FOLLOW );
				}

				if ( m_pOuter->IsLocalPlayer() )
				{
					if ( m_pOuter->m_pCritBoostEffect )
					{
						ClientLeafSystem()->SetRenderGroup( m_pOuter->m_pCritBoostEffect->RenderHandle(), RENDER_GROUP_VIEW_MODEL_TRANSLUCENT );
					}
				}
			}
			else
			{
				m_pOuter->m_pCritBoostEffect->StartEmission();
			}

			Assert( m_pOuter->m_pCritBoostEffect->IsValid() );
		}

#ifdef CLIENT_DLL
		if ( m_pOuter->GetActivePCWeapon() && !m_pCritBoostSoundLoop )
		{
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			CLocalPlayerFilter filter;
			m_pCritBoostSoundLoop = controller.SoundCreate( filter, m_pOuter->entindex(), "Weapon_General.CritPower" );	
			controller.Play( m_pCritBoostSoundLoop, 1.0, 100 );
		}
#endif
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayerShared::GetStealthNoAttackExpireTime( void )
{
	return m_flStealthNoAttackExpire;
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether this player is dominating the specified other player
//-----------------------------------------------------------------------------
void CTFPlayerShared::SetPlayerDominated( CTFPlayer *pPlayer, bool bDominated )
{
	int iPlayerIndex = pPlayer->entindex();
	m_bPlayerDominated.Set( iPlayerIndex, bDominated );
	pPlayer->m_Shared.SetPlayerDominatingMe( m_pOuter, bDominated );
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether this player is being dominated by the other player
//-----------------------------------------------------------------------------
void CTFPlayerShared::SetPlayerDominatingMe( CTFPlayer *pPlayer, bool bDominated )
{
	int iPlayerIndex = pPlayer->entindex();
	m_bPlayerDominatingMe.Set( iPlayerIndex, bDominated );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether this player is dominating the specified other player
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsPlayerDominated( int iPlayerIndex )
{
#ifdef CLIENT_DLL
	// On the client, we only have data for the local player.
	// As a result, it's only valid to ask for dominations related to the local player
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pLocalPlayer )
		return false;

	Assert( m_pOuter->IsLocalPlayer() || pLocalPlayer->entindex() == iPlayerIndex );

	if ( m_pOuter->IsLocalPlayer() )
		return m_bPlayerDominated.Get( iPlayerIndex );

	return pLocalPlayer->m_Shared.IsPlayerDominatingMe( m_pOuter->entindex() );
#else
	// Server has all the data.
	return m_bPlayerDominated.Get( iPlayerIndex );
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsPlayerDominatingMe( int iPlayerIndex )
{
	return m_bPlayerDominatingMe.Get( iPlayerIndex );
}

//-----------------------------------------------------------------------------
// Purpose: True if the given player is a spy disguised as our team.
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsSpyDisguisedAsMyTeam( CTFPlayer *pPlayer )
{
	if ( !pPlayer )
		return false;

	if ( pPlayer->IsPlayerClass( TF_CLASS_SPY ) &&
		pPlayer->GetTeamNumber() != m_pOuter->GetTeamNumber() &&
		pPlayer->m_Shared.GetDisguiseTeam() == m_pOuter->GetTeamNumber() )
	{
		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::NoteLastDamageTime( int nDamage )
{
	// we took damage
	if ( ( nDamage > 5 || InCond( TF_COND_BLEEDING ) ) && !InCond( TF_COND_FEIGN_DEATH ) && InCond( TF_COND_STEALTHED ) )
	{
		m_flLastStealthExposeTime = gpGlobals->curtime;
		AddCond( TF_COND_STEALTHED_BLINK );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnSpyTouchedByEnemy( void )
{
	if ( !InCond( TF_COND_FEIGN_DEATH ) && InCond( TF_COND_STEALTHED ) )
	{
		m_flLastStealthExposeTime = gpGlobals->curtime;
		AddCond( TF_COND_STEALTHED_BLINK );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsEnteringOrExitingFullyInvisible( void )
{
	return ( ( GetPercentInvisiblePrevious() != 1.f && GetPercentInvisible() == 1.f ) || 
			 ( GetPercentInvisiblePrevious() == 1.f && GetPercentInvisible() != 1.f ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::FadeInvis( float fAdditionalRateScale )
{
	ETFCond nExpiringCondition = TF_COND_LAST;
	if ( InCond( TF_COND_STEALTHED ) )
	{
		nExpiringCondition = TF_COND_STEALTHED;
		RemoveCond( TF_COND_STEALTHED );
	}
	else if ( InCond( TF_COND_STEALTHED_USER_BUFF ) )
	{
		nExpiringCondition = TF_COND_STEALTHED_USER_BUFF;
		RemoveCond( TF_COND_STEALTHED_USER_BUFF );
	}

#ifdef GAME_DLL
	// inform the bots
	// SEALTODO
	//CTFWeaponInvis *pInvis = dynamic_cast< CTFWeaponInvis * >( m_pOuter->Weapon_OwnsThisID( TF_WEAPON_INVIS ) );
	//if ( pInvis )
	//{
	//	TheNextBots().OnWeaponFired( m_pOuter, pInvis );
	//}
#endif

	// If present, give our invisibility weapon a chance to override our decloak
	// rate scale.
	float flDecloakRateScale = 0.0f;

	// This comes from script, so sanity check the result.
	if ( flDecloakRateScale <= 0.0f )
	{
		flDecloakRateScale = 1.0f;
	}

	float flInvisFadeTime = fAdditionalRateScale
						  * (tf_spy_invis_unstealth_time.GetFloat() * flDecloakRateScale);

	if ( flInvisFadeTime < 0.15 )
	{
		// this was a force respawn, they can attack whenever
	}
	else if ( ( nExpiringCondition != TF_COND_STEALTHED_USER_BUFF ) && !InCond( TF_COND_STEALTHED_USER_BUFF_FADING ) )
	{
		// next attack in some time
		m_flStealthNoAttackExpire = gpGlobals->curtime + (tf_spy_cloak_no_attack_time.GetFloat() * flDecloakRateScale * fAdditionalRateScale);
	}

	m_flInvisChangeCompleteTime = gpGlobals->curtime + flInvisFadeTime;
}

//-----------------------------------------------------------------------------
// Purpose: Approach our desired level of invisibility
//-----------------------------------------------------------------------------
void CTFPlayerShared::InvisibilityThink( void )
{
	if ( m_pOuter->GetPlayerClass()->GetClassIndex() != TF_CLASS_SPY && InCond( TF_COND_STEALTHED ) )
	{
		// Shouldn't happen, but it's a safety net
		m_flInvisibility = 0.0f;
		if ( InCond( TF_COND_STEALTHED ) )
		{
			RemoveCond( TF_COND_STEALTHED );
		}
		return;
	}

	float flTargetInvis = 0.0f;
	float flTargetInvisScale = 1.0f;
	if ( InCond( TF_COND_STEALTHED_BLINK ) || InCond( TF_COND_URINE ) )
	{
		// We were bumped into or hit for some damage.
		flTargetInvisScale = TF_SPY_STEALTH_BLINKSCALE;
	}

	// Go invisible or appear.
	if ( m_flInvisChangeCompleteTime > gpGlobals->curtime )
	{
		if ( IsStealthed() )
		{
			flTargetInvis = 1.0f - ( ( m_flInvisChangeCompleteTime - gpGlobals->curtime ) );
		}
		else
		{
			flTargetInvis = ( ( m_flInvisChangeCompleteTime - gpGlobals->curtime ) * 0.5f );
		}
	}
	else
	{
		if ( IsStealthed() )
		{
			flTargetInvis = 1.0f;
			m_flLastNoMovementTime = -1.f;
		}
		else
		{
			flTargetInvis = 0.0f;
		}
	}

	flTargetInvis *= flTargetInvisScale;
	m_flPrevInvisibility = m_flInvisibility;
	m_flInvisibility = clamp( flTargetInvis, 0.0f, 1.0f );
}


//-----------------------------------------------------------------------------
// Purpose: How invisible is the player [0..1]
//-----------------------------------------------------------------------------
float CTFPlayerShared::GetPercentInvisible( void ) const
{
	return m_flInvisibility;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsCritBoosted( void ) const
{
	bool bAllWeaponCritActive = ( InCond( TF_COND_CRITBOOSTED ) || 
								  InCond( TF_COND_CRITBOOSTED_PUMPKIN ) || 
								  InCond( TF_COND_CRITBOOSTED_USER_BUFF ) || 
#ifdef CLIENT_DLL
								  InCond( TF_COND_CRITBOOSTED_DEMO_CHARGE ) || 
#endif
								  InCond( TF_COND_CRITBOOSTED_FIRST_BLOOD ) || 
								  InCond( TF_COND_CRITBOOSTED_BONUS_TIME ) || 
								  InCond( TF_COND_CRITBOOSTED_CTF_CAPTURE ) || 
								  InCond( TF_COND_CRITBOOSTED_ON_KILL ) ||
								  InCond( TF_COND_CRITBOOSTED_CARD_EFFECT ) ||
								  InCond( TF_COND_CRITBOOSTED_RUNE_TEMP ) );

	if ( bAllWeaponCritActive )
		return true;


	CTFWeaponBase *pWeapon = dynamic_cast< CTFWeaponBase* >( m_pOuter->GetActiveWeapon() );
	if ( pWeapon )
	{
		if ( InCond( TF_COND_CRITBOOSTED_RAGE_BUFF ) && pWeapon->GetPCWpnData().m_ExtraTFWeaponData.m_iWeaponType == TF_WPN_TYPE_PRIMARY )
		{
			// Only primary weapon can be crit boosted by pyro rage
			return true;
		}

		float flCritHealthPercent = 1.0f;

		if ( flCritHealthPercent < 1.0f && m_pOuter->HealthFraction() < flCritHealthPercent )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsInvulnerable( void ) const
{
	bool bInvuln = InCond( TF_COND_INVULNERABLE ) || 
				   InCond( TF_COND_INVULNERABLE_USER_BUFF ) || 
				   InCond( TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED ) ||
				   InCond( TF_COND_INVULNERABLE_CARD_EFFECT );

	return bInvuln;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsStealthed( void ) const
{
	return ( InCond( TF_COND_STEALTHED ) || InCond( TF_COND_STEALTHED_USER_BUFF ) || InCond( TF_COND_STEALTHED_USER_BUFF_FADING ) );
}

bool CTFPlayerShared::CanBeDebuffed( void ) const
{
	if ( IsInvulnerable() )
		return false;

	if ( InCond( TF_COND_PHASE ) || InCond( TF_COND_PASSTIME_INTERCEPTION ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Start the process of disguising
//-----------------------------------------------------------------------------
void CTFPlayerShared::Disguise( int nTeam, int nClass, CTFPlayer* pDesiredTarget )
{
	int nRealTeam = m_pOuter->GetTeamNumber();
	int nRealClass = m_pOuter->GetPlayerClass()->GetClassIndex();

	Assert ( ( nClass >= TF_CLASS_SCOUT ) && ( nClass <= TF_CLASS_ENGINEER ) );

	// we're not a spy
	if ( nRealClass != TF_CLASS_SPY )
	{
		return;
	}

	if ( InCond( TF_COND_TAUNTING ) )
	{
		// not allowed to disguise while taunting
		return;
	}

	// we're not disguising as anything but ourselves (so reset everything)
	if ( nRealTeam == nTeam && nRealClass == nClass )
	{
		RemoveDisguise();
		return;
	}

	// Ignore disguise of the same type, unless we're using 'Your Eternal Reward'
	if ( nTeam == m_nDisguiseTeam && nClass == m_nDisguiseClass )
	{
#ifdef GAME_DLL
		DetermineDisguiseWeapon( false );
#endif
		return;
	}

	// invalid team
	if ( nTeam <= TEAM_SPECTATOR || nTeam >= TF_TEAM_COUNT )
	{
		return;
	}

	// invalid class
	if ( nClass <= TF_CLASS_UNDEFINED || nClass >= TF_CLASS_COUNT_ALL )
	{
		return;
	}

	// are we already in the middle of disguising as this class? 
	// (the lastdisguise key might get pushed multiple times before the disguise is complete)
	if ( InCond( TF_COND_DISGUISING ) )
	{
		if ( nTeam == m_nDesiredDisguiseTeam && nClass == m_nDesiredDisguiseClass )
		{
			return;
		}
	}

	m_hDesiredDisguiseTarget.Set( pDesiredTarget );
	m_nDesiredDisguiseClass = nClass;
	m_nDesiredDisguiseTeam = nTeam;

	m_bLastDisguisedAsOwnTeam = ( m_nDesiredDisguiseTeam == m_pOuter->GetTeamNumber() );

	AddCond( TF_COND_DISGUISING );

	// Start the think to complete our disguise
	float flTimeToDisguise = TF_TIME_TO_DISGUISE;

	// Quick disguise if you already disguised
	if ( InCond( TF_COND_DISGUISED ) )
	{
		flTimeToDisguise = TF_TIME_TO_QUICK_DISGUISE;
	}

	if ( pDesiredTarget )
	{
		flTimeToDisguise = 0;
	}
	m_flDisguiseCompleteTime = gpGlobals->curtime + flTimeToDisguise;
}

//-----------------------------------------------------------------------------
// Purpose: Set our target with a player we've found to emulate
//-----------------------------------------------------------------------------
#ifndef CLIENT_DLL
void CTFPlayerShared::FindDisguiseTarget( void )
{
	if ( m_hDesiredDisguiseTarget )
	{
		m_hDisguiseTarget.Set( m_hDesiredDisguiseTarget.Get() );
		m_hDesiredDisguiseTarget.Set( NULL );
	}
	else
	{
		m_hDisguiseTarget = m_pOuter->TeamFortress_GetDisguiseTarget( m_nDisguiseTeam, m_nDisguiseClass );
	}

	if ( m_hDisguiseTarget )
	{
		m_iDisguiseTargetIndex.Set( m_hDisguiseTarget.Get()->entindex() );
		Assert( m_iDisguiseTargetIndex >= 1 && m_iDisguiseTargetIndex <= MAX_PLAYERS );
	}
	else
	{
		m_iDisguiseTargetIndex.Set( TF_DISGUISE_TARGET_INDEX_NONE );
	}

	m_pOuter->CreateDisguiseWeaponList( ToTFPlayer( m_hDisguiseTarget.Get() ) );
}

#endif

int CTFPlayerShared::GetDisguiseTeam( void ) const
{
	return InCond( TF_COND_DISGUISED_AS_DISPENSER ) ? (int)( ( m_pOuter->GetTeamNumber() == TF_TEAM_RED ) ? TF_TEAM_BLUE : TF_TEAM_RED ) : m_nDisguiseTeam;
}

//-----------------------------------------------------------------------------
// Purpose: Complete our disguise
//-----------------------------------------------------------------------------
void CTFPlayerShared::CompleteDisguise( void )
{
	AddCond( TF_COND_DISGUISED );

	m_nDisguiseClass = m_nDesiredDisguiseClass;
	m_nDisguiseTeam = m_nDesiredDisguiseTeam;

	if ( m_nDisguiseClass == TF_CLASS_SPY )
	{
		m_nMaskClass = rand()%9+1;
	}

	RemoveCond( TF_COND_DISGUISING );

#ifdef GAME_DLL
	// Update the player model and skin.
	m_pOuter->UpdateModel();
	m_pOuter->ClearExpression();

	FindDisguiseTarget();

	if ( GetDisguiseTarget() )
	{
		m_iDisguiseHealth = GetDisguiseTarget()->GetHealth();
		if ( m_iDisguiseHealth <= 0 || !GetDisguiseTarget()->IsAlive() )
		{
			// If we disguised as an enemy who is currently dead, just set us to full health.
			m_iDisguiseHealth = GetDisguiseMaxHealth();
		}
	}
	else
	{
		int iMaxHealth = m_pOuter->GetMaxHealth();
		m_iDisguiseHealth = (int)random->RandomInt( iMaxHealth / 2, iMaxHealth );
	}

	// In Medieval mode, don't force primary weapon because most classes just have melee weapons
	DetermineDisguiseWeapon( !TFGameRules()->IsInMedievalMode() );
#endif

	m_pOuter->TeamFortress_SetSpeed();

	m_flDisguiseCompleteTime = 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::SetDisguiseHealth( int iDisguiseHealth )
{
	m_iDisguiseHealth = iDisguiseHealth;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayerShared::GetDisguiseMaxHealth( void )
{
	TFPlayerClassData_t *pClass = GetPlayerClassData( GetDisguiseClass() );
	if ( pClass )
	{
		return pClass->m_nMaxHealth;
	}
	else
	{
		return m_pOuter->GetMaxHealth();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::RemoveDisguise( void )
{
	if ( GetDisguiseTeam() != m_pOuter->GetTeamNumber() )
	{
		if ( InCond( TF_COND_TELEPORTED ) )
		{
			RemoveCond( TF_COND_TELEPORTED );
		}
	}

	RemoveCond( TF_COND_DISGUISED );
	RemoveCond( TF_COND_DISGUISING );

	AddCond( TF_COND_DISGUISE_WEARINGOFF, 0.5f );
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::DetermineDisguiseWeapon( bool bForcePrimary )
{
	Assert( m_pOuter->GetPlayerClass()->GetClassIndex() == TF_CLASS_SPY );

	const char* strDisguiseWeapon = NULL;

	CTFPlayer *pDisguiseTarget = ToTFPlayer( m_hDisguiseTarget.Get() );
	TFPlayerClassData_t *pData = GetPlayerClassData( m_nDisguiseClass );
	if ( pDisguiseTarget && (pDisguiseTarget->GetPlayerClass()->GetClassIndex() != m_nDisguiseClass) )
	{
		pDisguiseTarget = NULL;
	}

	// Determine which slot we have active.
	int iCurrentSlot = 0;
	if ( m_pOuter->GetActivePCWeapon() && !bForcePrimary )
	{
		iCurrentSlot = m_pOuter->GetActivePCWeapon()->GetSlot();
		if ( (iCurrentSlot == 3) && // Cig Case, so they are using the menu not a key bind to disguise.
			m_pOuter->GetLastWeapon() )
		{
			iCurrentSlot = m_pOuter->GetLastWeapon()->GetSlot();
		}
	}

	CTFWeaponBase *pItemWeapon = NULL;
	if ( pDisguiseTarget )
	{
		CTFWeaponBase *pLastDisguiseWeapon = m_hDisguiseWeapon;
		CTFWeaponBase *pFirstValidWeapon = NULL;
		// Cycle through the target's weapons and see if we have a match.
		// Note that it's possible the disguise target doesn't have a weapon in the slot we want,
		// for example if they have replaced it with an unlockable that isn't a weapon (wearable).
		for ( int i=0; i<m_pOuter->m_hDisguiseWeaponList.Count(); ++i )
		{
			CTFWeaponBase *pWeapon = m_pOuter->m_hDisguiseWeaponList[i];

			if ( !pWeapon )
				continue;

			if ( !pFirstValidWeapon )
			{
				pFirstValidWeapon = pWeapon;
			}

			// skip passtime gun
			//if ( pWeapon->GetWeaponID() == TF_WEAPON_PASSTIME_GUN )
			//{
			//	continue;
			//}

			if ( pWeapon->GetSlot() == iCurrentSlot )
			{
				pItemWeapon = pWeapon;
				break;
			}
		}

		if ( !pItemWeapon )
		{
			if ( pLastDisguiseWeapon )
			{
				pItemWeapon = pLastDisguiseWeapon;
			}
			else if ( pFirstValidWeapon )
			{
				pItemWeapon = pFirstValidWeapon;
			}
		}

		if ( pItemWeapon )
		{
			strDisguiseWeapon = pItemWeapon->GetClassname();
		}
	}

	if ( !pItemWeapon && pData )
	{
		// We have not found our item yet, so cycle through the class's default weapons
		// to find a match.
		for ( int i=0; i<TF_PLAYER_WEAPON_COUNT; ++i )
		{
			if ( pData->m_aWeapons[i] == TF_WEAPON_NONE )
				continue;
			const char *pWpnName = WeaponIdToAlias( pData->m_aWeapons[i] );
			WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( pWpnName );
			Assert( hWpnInfo != GetInvalidWeaponInfoHandle() );
			CTFWeaponInfo *pWeaponInfo = dynamic_cast<CTFWeaponInfo*>( GetFileWeaponInfoFromHandle( hWpnInfo ) );
			if ( pWeaponInfo->iSlot == iCurrentSlot )
			{
				strDisguiseWeapon = pWeaponInfo->szClassName;
			}
		}
	}

	if ( strDisguiseWeapon )
	{
		// Remove the old disguise weapon, if any.
		RemoveDisguiseWeapon();

		// We may need a sub-type if we're a builder. Otherwise we'll always appear as a engineer's workbox.
		int iSubType = 0;
		if ( Q_strcmp( strDisguiseWeapon, "tf_weapon_builder" ) == 0 )
		{
			return; // Temporary.
		}

		m_hDisguiseWeapon.Set( dynamic_cast<CTFWeaponBase*>( m_pOuter->GiveNamedItem( strDisguiseWeapon, iSubType ) ) );
		if ( m_hDisguiseWeapon )
		{
			m_hDisguiseWeapon->SetTouch( NULL );// no touch
			m_hDisguiseWeapon->SetOwner( dynamic_cast<CBaseCombatCharacter*>(m_pOuter) );
			m_hDisguiseWeapon->SetOwnerEntity( m_pOuter );
			m_hDisguiseWeapon->SetParent( m_pOuter );
			m_hDisguiseWeapon->FollowEntity( m_pOuter, true );
			m_hDisguiseWeapon->m_iState = WEAPON_IS_ACTIVE;
			m_hDisguiseWeapon->m_bDisguiseWeapon = true;
			m_hDisguiseWeapon->SetContextThink( &CTFWeaponBase::DisguiseWeaponThink, gpGlobals->curtime + 0.5, "DisguiseWeaponThink" );


			// Ammo/clip state is displayed to attached medics
			m_iDisguiseAmmo = 0;
			if ( !m_hDisguiseWeapon->IsMeleeWeapon() )
			{
				// Use the player we're disguised as if possible
				if ( pDisguiseTarget )
				{
					CPCWeaponBase *pWeapon = pDisguiseTarget->GetActivePCWeapon();
					if ( pWeapon && pWeapon->GetWeaponID() == m_hDisguiseWeapon->GetWeaponID() )
					{
						m_iDisguiseAmmo = pWeapon->UsesClipsForAmmo1() ? 
										  pWeapon->Clip1() : 
										  pDisguiseTarget->GetAmmoCount( pWeapon->GetPrimaryAmmoType() );
					}
				}

				// Otherwise display a faked ammo count
				if ( !m_iDisguiseAmmo )
				{
					int nMaxCount = m_hDisguiseWeapon->UsesClipsForAmmo1() ? 
									m_hDisguiseWeapon->GetMaxClip1() : 
									m_pOuter->GetMaxAmmo( m_hDisguiseWeapon->GetPrimaryAmmoType(), m_nDisguiseClass );
				
					m_iDisguiseAmmo = (int)random->RandomInt( 1, nMaxCount );
				}
			}
		}
	}
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::ProcessDisguiseImpulse( CTFPlayer *pPlayer )
{
	// Get the player owning the weapon.
	if ( !pPlayer )
		return;

	if ( pPlayer->GetImpulse() > 200 )
	{ 
		char szImpulse[6];
		Q_snprintf( szImpulse, sizeof( szImpulse ), "%d", pPlayer->GetImpulse() );

		char szTeam[3];
		Q_snprintf( szTeam, sizeof( szTeam ), "%c", szImpulse[1] );

		char szClass[3];
		Q_snprintf( szClass, sizeof( szClass ), "%c", szImpulse[2] );

		if ( pPlayer->CanDisguise() )
		{
			// intercepting the team value and reassigning what gets passed into Disguise()
			// because the team numbers in the client menu don't match the #define values for the teams
			pPlayer->m_Shared.Disguise( Q_atoi( szTeam ), Q_atoi( szClass ) );

			// Switch from the PDA to our previous weapon
			if ( GetActivePCWeapon() && GetActivePCWeapon()->GetWeaponID() == TF_WEAPON_PDA_SPY )
			{
				pPlayer->SelectLastItem();
			}
		}
	}
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: Heal players.
// pPlayer is person who healed us
//-----------------------------------------------------------------------------
void CTFPlayerShared::Heal( CBaseEntity *pHealer, float flAmount, float flOverhealBonus, float flOverhealDecayMult, bool bDispenserHeal /* = false */, CTFPlayer *pHealScorer /* = NULL */ )
{
	// If already healing, stop healing
	float flHealAccum = 0;
	if ( FindHealerIndex(pHealer) != m_aHealers.InvalidIndex() )
	{
		flHealAccum = StopHealing( pHealer );
	}

	healers_t newHealer;
	newHealer.pHealer = pHealer;
	newHealer.flAmount = flAmount;
	newHealer.flHealAccum = flHealAccum;
	newHealer.iKillsWhileBeingHealed = 0;
	newHealer.flOverhealBonus = flOverhealBonus;
	newHealer.flOverhealDecayMult = flOverhealDecayMult;
	newHealer.bDispenserHeal = bDispenserHeal;
	newHealer.flHealedLastSecond = 0;

	if ( pHealScorer )
	{
		newHealer.pHealScorer = pHealScorer;
	}
	else
	{
		//Assert( pHealer->IsPlayer() );
		newHealer.pHealScorer = pHealer;
	}

	m_aHealers.AddToTail( newHealer );

	AddCond( TF_COND_HEALTH_BUFF, PERMANENT_CONDITION, pHealer );

	RecalculateInvuln();

	m_nNumHealers = m_aHealers.Count();

	if ( pHealer && pHealer->IsPlayer() )
	{
		CTFPlayer *pPlayer = ToTFPlayer( pHealer );
		Assert(pPlayer);
		pPlayer->m_AchievementData.AddTargetToHistory( m_pOuter );
		pPlayer->TeamFortress_SetSpeed();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Heal players.
// pPlayer is person who healed us
//-----------------------------------------------------------------------------
float CTFPlayerShared::StopHealing( CBaseEntity *pHealer )
{
	int iIndex = FindHealerIndex(pHealer);
	if ( iIndex == m_aHealers.InvalidIndex() )
		return 0;

	float flHealingDone = 0.f;

	if ( iIndex != m_aHealers.InvalidIndex() )
	{
		flHealingDone = m_aHealers[iIndex].flHealAccum;
		m_aHealers.Remove( iIndex );
	}

	if ( !m_aHealers.Count() )
	{
		RemoveCond( TF_COND_HEALTH_BUFF );
	}

	RecalculateInvuln();

	m_nNumHealers = m_aHealers.Count();

	return flHealingDone;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsProvidingInvuln( CTFPlayer *pPlayer )
{
	if ( !pPlayer->IsPlayerClass(TF_CLASS_MEDIC) )
		return false;

	CWeaponMedigun *pMedigun = dynamic_cast<CWeaponMedigun*>( pPlayer->GetActivePCWeapon() );
	if ( pMedigun && pMedigun->IsReleasingCharge() )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::RecalculateInvuln( bool bInstantRemove )
{
	bool bShouldBeInvuln = false;

	if ( m_pOuter->m_flPowerPlayTime > gpGlobals->curtime )
	{
		bShouldBeInvuln = true;
	}

	// If we're not carrying the flag, and we're being healed by a medic 
	// who's generating invuln, then we should get invuln.
	if ( !m_pOuter->HasTheFlag() )
	{
		if ( IsProvidingInvuln( m_pOuter ) )
		{
			bShouldBeInvuln = true;
		}
		else
		{
			for ( int i = 0; i < m_aHealers.Count(); i++ )
			{
				if ( !m_aHealers[i].pHealer )
					continue;

				CTFPlayer *pPlayer = ToTFPlayer( m_aHealers[i].pHealer );
				if ( !pPlayer )
					continue;

				if ( IsProvidingInvuln( pPlayer ) )
				{
					bShouldBeInvuln = true;
					break;
				}
			}
		}
	}

	SetInvulnerable( bShouldBeInvuln, bInstantRemove );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::SetInvulnerable( bool bState, bool bInstant )
{
	bool bCurrentState = InCond( TF_COND_INVULNERABLE );
	if ( bCurrentState == bState )
	{
		if ( bState && m_flInvulnerableOffTime )
		{
			m_flInvulnerableOffTime = 0;
			RemoveCond( TF_COND_INVULNERABLE_WEARINGOFF );
		}
		return;
	}

	if ( bState )
	{
		Assert( !m_pOuter->HasTheFlag() );

		if ( m_flInvulnerableOffTime )
		{
			m_pOuter->StopSound( "TFPlayer.InvulnerableOff" );

			m_flInvulnerableOffTime = 0;
			RemoveCond( TF_COND_INVULNERABLE_WEARINGOFF );
		}

		// Invulnerable turning on
		AddCond( TF_COND_INVULNERABLE );

		// remove any persistent damaging conditions
		if ( InCond( TF_COND_BURNING ) )
		{
			RemoveCond( TF_COND_BURNING );
		}

		CSingleUserRecipientFilter filter( m_pOuter );
		m_pOuter->EmitSound( filter, m_pOuter->entindex(), "TFPlayer.InvulnerableOn" );
	}
	else
	{
		if ( !m_flInvulnerableOffTime )
		{
			CSingleUserRecipientFilter filter( m_pOuter );
			m_pOuter->EmitSound( filter, m_pOuter->entindex(), "TFPlayer.InvulnerableOff" );
		}

		if ( bInstant )
		{
			m_flInvulnerableOffTime = 0;
			RemoveCond( TF_COND_INVULNERABLE );
			RemoveCond( TF_COND_INVULNERABLE_WEARINGOFF );
		}
		else
		{
			// We're already in the process of turning it off
			if ( m_flInvulnerableOffTime )
				return;

			AddCond( TF_COND_INVULNERABLE_WEARINGOFF );
			m_flInvulnerableOffTime = gpGlobals->curtime + tf_invuln_time.GetFloat();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::AddTmpDamageBonus( float flBonus, float flExpiration )
{
	AddCond( TF_COND_TMPDAMAGEBONUS, flExpiration );
	m_flTmpDamageBonusAmount += flBonus;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFPlayerShared::FindHealerIndex( CBaseEntity *pHealer )
{
	for ( int i = 0; i < m_aHealers.Count(); i++ )
	{
		if ( m_aHealers[i].pHealer == pHealer )
			return i;
	}

	return m_aHealers.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayerShared::GetHealerByIndex( int index )
{
	int iNumHealers = m_aHealers.Count();

	if ( index < 0 || index >= iNumHealers )
		return NULL;

	return m_aHealers[index].pHealer;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayerShared::HealerIsDispenser( int index )
{
	int iNumHealers = m_aHealers.Count();

	if ( index < 0 || index >= iNumHealers )
		return false;

	return m_aHealers[index].bDispenserHeal;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the first healer in the healer array.  Note that this
//		is an arbitrary healer.
//-----------------------------------------------------------------------------
EHANDLE CTFPlayerShared::GetFirstHealer()
{
	if ( m_aHealers.Count() > 0 )
		return m_aHealers.Head().pHealer;

	return NULL;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Get all of our conditions in a nice CBitVec
//-----------------------------------------------------------------------------
void CTFPlayerShared::GetConditionsBits( CBitVec< TF_COND_LAST >& vbConditions ) const
{
	vbConditions.Set( 0u, (uint32)m_nPlayerCond );
	vbConditions.Set( 1u, (uint32)m_nPlayerCondEx );
	vbConditions.Set( 2u, (uint32)m_nPlayerCondEx2 );
	vbConditions.Set( 3u, (uint32)m_nPlayerCondEx3 );
	//COMPILE_TIME_ASSERT( 32 + 32 + 32 + 32 > TF_COND_LAST );
}

//-----------------------------------------------------------------------------
// Purpose: Team check.
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsAlly( CBaseEntity *pEntity )
{
	return ( pEntity->GetTeamNumber() == m_pOuter->GetTeamNumber() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayerShared::GetDesiredPlayerClassIndex( void )
{
	return m_iDesiredPlayerClass;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::SetJumping( bool bJumping )
{
	m_bJumping = bJumping;
}

void CTFPlayerShared::SetAirDash( int iAirDash )
{
	m_iAirDash = iAirDash;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayerShared::GetCritMult( void )
{
	float flRemapCritMul = RemapValClamped( m_iCritMult, 0, 255, 1.0, 4.0 );
/*#ifdef CLIENT_DLL
	Msg("CLIENT: Crit mult %.2f - %d\n",flRemapCritMul, m_iCritMult);
#else
	Msg("SERVER: Crit mult %.2f - %d\n", flRemapCritMul, m_iCritMult );
#endif*/

	return flRemapCritMul;
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::UpdateCritMult( void )
{
	const float flMinMult = 1.0;
	const float flMaxMult = TF_DAMAGE_CRITMOD_MAXMULT;

	if ( m_DamageEvents.Count() == 0 )
	{
		m_iCritMult = RemapValClamped( flMinMult, 1.0, 4.0, 0, 255 );
		return;
	}

	//Msg( "Crit mult update for %s\n", m_pOuter->GetPlayerName() );
	//Msg( "   Entries: %d\n", m_DamageEvents.Count() );

	// Go through the damage multipliers and remove expired ones, while summing damage of the others
	float flTotalDamage = 0;
	for ( int i = m_DamageEvents.Count() - 1; i >= 0; i-- )
	{
		float flDelta = gpGlobals->curtime - m_DamageEvents[i].flTime;
		if ( flDelta > tf_damage_events_track_for.GetFloat() )
		{
			//Msg( "      Discarded (%d: time %.2f, now %.2f)\n", i, m_DamageEvents[i].flTime, gpGlobals->curtime );
			m_DamageEvents.Remove(i);
			continue;
		}

		// Ignore damage we've just done. We do this so that we have time to get those damage events
		// to the client in time for using them in prediction in this code.
		if ( flDelta < TF_DAMAGE_CRITMOD_MINTIME )
		{
			//Msg( "      Ignored (%d: time %.2f, now %.2f)\n", i, m_DamageEvents[i].flTime, gpGlobals->curtime );
			continue;
		}

		if ( flDelta > TF_DAMAGE_CRITMOD_MAXTIME )
			continue;

		//Msg( "      Added %.2f (%d: time %.2f, now %.2f)\n", m_DamageEvents[i].flDamage, i, m_DamageEvents[i].flTime, gpGlobals->curtime );

		flTotalDamage += m_DamageEvents[i].flDamage * m_DamageEvents[i].flDamageCritScaleMultiplier;
	}

	float flMult = RemapValClamped( flTotalDamage, 0, TF_DAMAGE_CRITMOD_DAMAGE, flMinMult, flMaxMult );

//	Msg( "   TotalDamage: %.2f   -> Mult %.2f\n", flTotalDamage, flMult );

	m_iCritMult = (int)RemapValClamped( flMult, flMinMult, flMaxMult, 0, 255 );
}

#define CRIT_DAMAGE_TIME		0.1f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::RecordDamageEvent( const CTakeDamageInfo &info, bool bKill, int nVictimPrevHealth )
{
	if ( m_DamageEvents.Count() >= MAX_DAMAGE_EVENTS )
	{
		// Remove the oldest event
		m_DamageEvents.Remove( m_DamageEvents.Count()-1 );
	}

	// Don't count critical damage toward the critical multiplier.
	float flDamage = info.GetDamage() - info.GetDamageBonus();

	float flDamageCriticalScale = info.GetDamageType() & DMG_DONT_COUNT_DAMAGE_TOWARDS_CRIT_RATE
								? 0.0f
								: 1.0f;

	// cap the damage at our current health amount since it's going to kill us
	if ( bKill && flDamage > nVictimPrevHealth )
	{
		flDamage = nVictimPrevHealth;
	}

	// Don't allow explosions to stack up damage toward the critical modifier.
	bool bOverride = false;
	if ( info.GetDamageType() & DMG_BLAST )
	{
		int nDamageCount = m_DamageEvents.Count();
		for ( int iDamage = 0; iDamage < nDamageCount; ++iDamage )
		{
			// Was the older event I am checking against an explosion as well?
			if ( m_DamageEvents[iDamage].nDamageType & DMG_BLAST )
			{
				// Did it happen very recently?
				if ( ( gpGlobals->curtime - m_DamageEvents[iDamage].flTime ) < CRIT_DAMAGE_TIME )
				{
					if ( bKill )
					{
						m_DamageEvents[iDamage].nKills++;
					}

					// Take the max damage done in the time frame.
					if ( flDamage > m_DamageEvents[iDamage].flDamage )
					{
						m_DamageEvents[iDamage].flDamage = flDamage;
						m_DamageEvents[iDamage].flDamageCritScaleMultiplier = flDamageCriticalScale;
						m_DamageEvents[iDamage].flTime = gpGlobals->curtime;
						m_DamageEvents[iDamage].nDamageType = info.GetDamageType();

//						Msg( "Update Damage Event: D:%f, T:%f\n", m_DamageEvents[iDamage].flDamage, m_DamageEvents[iDamage].flTime );
					}

					bOverride = true;
				}
			}
		}
	}

	// We overrode a value, don't add this to the list.
	if ( bOverride )
		return;

	int iIndex = m_DamageEvents.AddToTail();
	m_DamageEvents[iIndex].flDamage = flDamage;
	m_DamageEvents[iIndex].flDamageCritScaleMultiplier = flDamageCriticalScale;
	m_DamageEvents[iIndex].nDamageType = info.GetDamageType();
	m_DamageEvents[iIndex].flTime = gpGlobals->curtime;
	m_DamageEvents[iIndex].nKills = bKill;

//	Msg( "Damage Event: D:%f, T:%f\n", m_DamageEvents[iIndex].flDamage, m_DamageEvents[iIndex].flTime );

	if ( TFGameRules()->IsMannVsMachineMode() && m_pOuter->IsPlayerClass( TF_CLASS_SNIPER ) )
	{
		int nKillCount = 0;
		int nDamageCount = m_DamageEvents.Count();
		for ( int iDamage = 0; iDamage < nDamageCount; ++iDamage )
		{
			// Did it happen very recently?
			if ( ( gpGlobals->curtime - m_DamageEvents[iDamage].flTime ) < CRIT_DAMAGE_TIME )
			{
				nKillCount += m_DamageEvents[iDamage].nKills;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::AddTempCritBonus( float flAmount )
{
	if ( m_DamageEvents.Count() >= MAX_DAMAGE_EVENTS )
	{
		// Remove the oldest event
		m_DamageEvents.Remove( m_DamageEvents.Count()-1 );
	}

	int iIndex = m_DamageEvents.AddToTail();
	m_DamageEvents[iIndex].flDamage = RemapValClamped( flAmount, 0, 1, 0, TF_DAMAGE_CRITMOD_DAMAGE ) / (TF_DAMAGE_CRITMOD_MAXMULT - 1.0);
	m_DamageEvents[iIndex].flDamageCritScaleMultiplier = 1.0f;
	m_DamageEvents[iIndex].nDamageType = DMG_GENERIC;
	m_DamageEvents[iIndex].flTime = gpGlobals->curtime;
	m_DamageEvents[iIndex].nKills = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFPlayerShared::GetNumKillsInTime( float flTime )
{
	if ( tf_damage_events_track_for.GetFloat() < flTime )
	{
		Warning("Player asking for damage events for time %.0f, but tf_damage_events_track_for is only tracking events for %.0f\n", flTime, tf_damage_events_track_for.GetFloat() );
	}

	int iKills = 0;
	for ( int i = m_DamageEvents.Count() - 1; i >= 0; i-- )
	{
		float flDelta = gpGlobals->curtime - m_DamageEvents[i].flTime;
		if ( flDelta < flTime )
		{
			iKills += m_DamageEvents[i].nKills;
		}
	}

	return iKills;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayerShared::AddToSpyCloakMeter( float val, bool bForce )
{
	CTFWeaponInvis *pWpn = (CTFWeaponInvis *) m_pOuter->Weapon_OwnsThisID( TF_WEAPON_INVIS );
	if ( !pWpn )
		return false;

	bool bResult = ( val > 0 && m_flCloakMeter < 100.0f );

	m_flCloakMeter = clamp( m_flCloakMeter + val, 0.0f, 100.0f );

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: Stun & Snare Application
//-----------------------------------------------------------------------------
void CTFPlayerShared::StunPlayer( float flTime, float flReductionAmount, int iStunFlags, CTFPlayer* pAttacker )
{
	// Insanity prevention
	if ( ( m_PlayerStuns.Count() + 1 ) >= 250 )
		return;

	if ( InCond( TF_COND_PHASE ) || InCond( TF_COND_PASSTIME_INTERCEPTION ) )
		return;

	if ( InCond( TF_COND_MEGAHEAL ) )
		return;

	if ( InCond( TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED ) && !InCond( TF_COND_MVM_BOT_STUN_RADIOWAVE ) )
		return;

	if ( pAttacker && TFGameRules() && TFGameRules()->IsTruceActive() && pAttacker->IsTruceValidForEnt() )
	{
		if ( ( pAttacker->GetTeamNumber() == TF_TEAM_RED ) || ( pAttacker->GetTeamNumber() == TF_TEAM_BLUE ) )
			return;
	}

	float flRemapAmount = RemapValClamped( flReductionAmount, 0.0, 1.0, 0, 255 );

	int iOldStunFlags = GetStunFlags();

	// Already stunned
	bool bStomp = false;
	if ( InCond( TF_COND_STUNNED ) )
	{
		if ( GetActiveStunInfo() )
		{
			// Is it stronger than the active?
			if ( flRemapAmount > GetActiveStunInfo()->flStunAmount || iStunFlags & TF_STUN_CONTROLS || iStunFlags & TF_STUN_LOSER_STATE )
			{
				bStomp = true;
			}
			// It's weaker.  Would it expire before the active?
			else if ( gpGlobals->curtime + flTime < GetActiveStunInfo()->flExpireTime )
			{
				// Ignore
				return;
			}
		}
	}
	else if ( GetActiveStunInfo() )
	{
		// Something yanked our TF_COND_STUNNED in an unexpected way
		if ( !HushAsserts() )
			Assert( !"Something yanked out TF_COND_STUNNED." );
		m_PlayerStuns.RemoveAll();
		return;
	}

	// Add it to the stack
	stun_struct_t stunEvent = 
	{
		pAttacker,						// hPlayer
		flTime,							// flDuration
		gpGlobals->curtime + flTime,	// flExpireTime
		gpGlobals->curtime + flTime,	// flStartFadeTime
		flRemapAmount,					// flStunAmount
		iStunFlags						// iStunFlags
	};

	// Should this become the active stun?
	if ( bStomp || !GetActiveStunInfo() )
	{
		// If stomping, see if the stun we're replacing has a stronger slow.
		// This can happen when stuns use TF_STUN_CONTROLS or TF_STUN_LOSER_STATE.
		float flOldStun = GetActiveStunInfo() ? GetActiveStunInfo()->flStunAmount : 0.f;

		m_iStunIndex = m_PlayerStuns.AddToTail( stunEvent );

		if ( flOldStun > flRemapAmount )
		{
			GetActiveStunInfo()->flStunAmount = flOldStun;
		}
	}
	else
	{
		// Done for now
		m_PlayerStuns.AddToTail( stunEvent );
		return;
	}

	// Add in extra time when TF_STUN_CONTROLS
	if ( GetActiveStunInfo()->iStunFlags & TF_STUN_CONTROLS )
	{
		if ( !InCond( TF_COND_HALLOWEEN_KART ) )
		{
			GetActiveStunInfo()->flExpireTime += CONTROL_STUN_ANIM_TIME;
		}
	}

	GetActiveStunInfo()->flStartFadeTime = gpGlobals->curtime + GetActiveStunInfo()->flDuration;
	
	// Update old system for networking
	UpdateLegacyStunSystem();
	
	if ( GetActiveStunInfo()->iStunFlags & TF_STUN_CONTROLS || GetActiveStunInfo()->iStunFlags & TF_STUN_LOSER_STATE )
	{
		m_pOuter->m_angTauntCamera = m_pOuter->EyeAngles();
		m_pOuter->SpeakConceptIfAllowed( MP_CONCEPT_STUNNED );
		if ( pAttacker )
		{
			pAttacker->SpeakConceptIfAllowed( MP_CONCEPT_STUNNED_TARGET );
		}
	}

	if ( !( GetActiveStunInfo()->iStunFlags & TF_STUN_NO_EFFECTS ) )
	{
		m_pOuter->StunSound( pAttacker, GetActiveStunInfo()->iStunFlags, iOldStunFlags );
	}

	// Event for achievements.
	IGameEvent *event = gameeventmanager->CreateEvent( "player_stunned" );
	if ( event )
	{
		if ( pAttacker )
		{
			event->SetInt( "stunner", pAttacker->GetUserID() );
		}
		event->SetInt( "victim", m_pOuter->GetUserID() );
		event->SetBool( "victim_capping", m_pOuter->IsCapturingPoint() );
		event->SetBool( "big_stun", ( GetActiveStunInfo()->iStunFlags & TF_STUN_SPECIAL_SOUND ) != 0 );
		gameeventmanager->FireEvent( event );
	}

	// Clear off all taunts, expressions, and scenes.
	if ( ( GetActiveStunInfo()->iStunFlags & TF_STUN_CONTROLS) == TF_STUN_CONTROLS || ( GetActiveStunInfo()->iStunFlags & TF_STUN_LOSER_STATE) == TF_STUN_LOSER_STATE )
	{
		m_pOuter->StopTaunt();

		m_pOuter->ClearExpression();
		m_pOuter->ClearWeaponFireScene();
	}

	AddCond( TF_COND_STUNNED, -1.f, pAttacker );
}
#endif // GAME_DLL

//-----------------------------------------------------------------------------
// Purpose: Returns the intensity of the current stun effect, if we have the type of stun indicated.
//-----------------------------------------------------------------------------
float CTFPlayerShared::GetAmountStunned( int iStunFlags )
{
	if ( GetActiveStunInfo() )
	{
		if ( InCond( TF_COND_STUNNED ) && ( iStunFlags & GetActiveStunInfo()->iStunFlags ) && ( GetActiveStunInfo()->flExpireTime > gpGlobals->curtime ) )
			return MIN( MAX( GetActiveStunInfo()->flStunAmount, 0 ), 255 ) * ( 1.f/255.f );
	}

	return 0.f;
}

//-----------------------------------------------------------------------------
// Purpose: Indicates that our controls are stunned.
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsControlStunned( void )
{
	if ( GetActiveStunInfo() )
	{
		if ( InCond( TF_COND_STUNNED ) && ( m_iStunFlags & TF_STUN_CONTROLS ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Indicates that our controls are stunned.
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsLoserStateStunned( void ) const
{
	if ( GetActiveStunInfo() )
	{
		if ( InCond( TF_COND_STUNNED ) && ( m_iStunFlags & TF_STUN_LOSER_STATE ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Indicates that our movement is slowed, but our controls are still free.
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsSnared( void )
{
	if ( InCond( TF_COND_STUNNED ) && !IsControlStunned() )
		return true;
	else
		return false;
}

struct penetrated_target_list
{
	CBaseEntity *pTarget;
	float flDistanceFraction;
};

//-----------------------------------------------------------------------------
class CBulletPenetrateEnum : public IEntityEnumerator
{
public:
	CBulletPenetrateEnum( Ray_t *pRay, CBaseEntity *pShooter, int nCustomDamageType, bool bIgnoreTeammates = true )
	{
		m_pRay = pRay;
		m_pShooter = pShooter;
		m_nCustomDamageType = nCustomDamageType;
		m_bIgnoreTeammates = bIgnoreTeammates;
	}

	// We need to sort the penetrated targets into order, with the closest target first
	class PenetratedTargetLess
	{
	public:
		bool Less( const penetrated_target_list &src1, const penetrated_target_list &src2, void *pCtx )
		{
			return src1.flDistanceFraction < src2.flDistanceFraction;
		}
	};

	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		trace_t tr;

		CBaseEntity *pEnt = static_cast<CBaseEntity*>(pHandleEntity);

		// Ignore collisions with the shooter
		if ( pEnt == m_pShooter )
			return true;

		if ( pEnt->IsCombatCharacter() || pEnt->IsBaseObject() )
		{
			if ( m_bIgnoreTeammates && pEnt->GetTeam() == m_pShooter->GetTeam() )
				return true;

			enginetrace->ClipRayToEntity( *m_pRay, MASK_SOLID | CONTENTS_HITBOX, pHandleEntity, &tr );

			if (tr.fraction < 1.0f)
			{
				penetrated_target_list newEntry;
				newEntry.pTarget = pEnt;
				newEntry.flDistanceFraction = tr.fraction;
				m_Targets.Insert( newEntry );
				return true;
			}
		}

		return true;
	}

public:
	Ray_t		*m_pRay;
	int			 m_nCustomDamageType;
	CBaseEntity	*m_pShooter;
	bool		 m_bIgnoreTeammates;
	CUtlSortVector<penetrated_target_list, PenetratedTargetLess> m_Targets;
};


CTargetOnlyFilter::CTargetOnlyFilter( CBaseEntity *pShooter, CBaseEntity *pTarget ) 
	: CTraceFilterSimple( pShooter, COLLISION_GROUP_NONE )
{
	m_pShooter = pShooter;
	m_pTarget = pTarget;
}

bool CTargetOnlyFilter::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	CBaseEntity *pEnt = static_cast<CBaseEntity*>(pHandleEntity);

	if ( pEnt && pEnt == m_pTarget ) 
		return true;
	else if ( !pEnt || pEnt != m_pTarget )
	{
		// If we hit a solid piece of the world, we're done.
		if ( pEnt->IsBSPModel() && pEnt->IsSolid() )
			return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
		return false;
	}
	else
		return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}

#include "tf_gamemovement.h"

void CTFPlayer::GameMovementInit( void )
{
	if ( GetGameMovementType() )
	{
		delete GetGameMovementType();
		SetGameMovementType( nullptr );
	}

	CGameMovement *GameMovement = NULL;

	if ( IsDODClass() )
	{
		GameMovement = new CDODGameMovement( this );
		SetMovementTypeReference( FACTION_DOD );
	}
	else
	{
		GameMovement = new CTFGameMovement( this );
		SetMovementTypeReference( FACTION_TF );
	}

	if ( !GameMovement )
		return;

	SetGameMovementType( GameMovement );
}

bool CTFPlayer::DoesMovementTypeMatch()
{
	if ( IsDODClass() )
		return GetMovementTypeReference() == FACTION_DOD;
	else
		return GetMovementTypeReference() == FACTION_TF;
}

#include "dod_playeranimstate.h"

void CTFPlayer::GameAnimStateInit( void )
{
	if ( GetGameAnimStateType() )
	{
		delete GetGameAnimStateType();
		SetGameAnimStateType( nullptr );
	}

	CMultiPlayerAnimState *AnimState = NULL;

	if ( IsDODClass() )
	{
		AnimState = CreateDODPlayerAnimState( this );
		SetGameAnimStateTypeReference( FACTION_DOD );
	}
	else
	{
		AnimState = CreateTFPlayerAnimState( this );
		SetGameAnimStateTypeReference( FACTION_TF );
	}

	if ( !AnimState )
		return;

	SetGameAnimStateType( AnimState );
}

bool CTFPlayer::DoesAnimStateTypeMatch()
{
	if ( IsDODClass() )
		return GetGameAnimStateTypeReference() == FACTION_DOD;
	else
		return GetGameAnimStateTypeReference() == FACTION_TF;
}

//=============================================================================
//
// Shared player code that isn't CTFPlayerShared
//

//-----------------------------------------------------------------------------
// Purpose:
//   Input: info
//          bDoEffects - effects (blood, etc.) should only happen client-side.
//-----------------------------------------------------------------------------
void CTFPlayer::FireBullet( CTFWeaponBase *pWpn, const FireBulletsInfo_t &info, bool bDoEffects, int nDamageType, int nCustomDamageType /*= TF_DMG_CUSTOM_NONE*/ )
{
	// Fire a bullet (ignoring the shooter).
	Vector vecStart = info.m_vecSrc;
	Vector vecEnd = vecStart + info.m_vecDirShooting * info.m_flDistance;
	trace_t trace;
	UTIL_TraceLine( vecStart, vecEnd, ( MASK_SOLID | CONTENTS_HITBOX ), this, COLLISION_GROUP_NONE, &trace );

#ifdef GAME_DLL
	if ( tf_debug_bullets.GetBool() )
	{
		NDebugOverlay::Line( vecStart, trace.endpos, 0,255,0, true, 30 );
	}
#endif

	if( trace.fraction < 1.0 )
	{
		// Verify we have an entity at the point of impact.
		Assert( trace.m_pEnt );

		if( bDoEffects )
		{
			// If shot starts out of water and ends in water
			if ( !( enginetrace->GetPointContents( trace.startpos ) & ( CONTENTS_WATER | CONTENTS_SLIME ) ) &&
				( enginetrace->GetPointContents( trace.endpos ) & ( CONTENTS_WATER | CONTENTS_SLIME ) ) )
			{	
				// Water impact effects.
				ImpactWaterTrace( trace, vecStart );
			}
			else
			{
				// Regular impact effects.

				// don't decal your teammates or objects on your team
				if ( trace.m_pEnt->GetTeamNumber() != GetTeamNumber() )
				{
					UTIL_ImpactTrace( &trace, nDamageType );
				}
			}

#ifdef CLIENT_DLL
			static int	tracerCount;
			if ( ( ( info.m_iTracerFreq != 0 ) && ( tracerCount++ % info.m_iTracerFreq ) == 0 ) )
			{
				// if this is a local player, start at attachment on view model
				// else start on attachment on weapon model
				int iUseAttachment = TRACER_DONT_USE_ATTACHMENT;
				int iAttachment = 1;

				{
					C_BaseCombatWeapon *pWeapon = GetActiveWeapon();

					if ( pWeapon )
					{
						iAttachment = pWeapon->LookupAttachment( "muzzle" );
					}
				}


				C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

				bool bInToolRecordingMode = clienttools->IsInRecordingMode();

				// If we're using a viewmodel, override vecStart with the muzzle of that - just for the visual effect, not gameplay.
				if ( ( pLocalPlayer != NULL ) && !pLocalPlayer->ShouldDrawThisPlayer() && !bInToolRecordingMode && pWpn )
				{
					C_BaseAnimating *pAttachEnt = pWpn->GetAppropriateWorldOrViewModel();
					if ( pAttachEnt != NULL )
					{
						pAttachEnt->GetAttachment( iAttachment, vecStart );
					}
				}
				else if ( !IsDormant() )
				{
					// fill in with third person weapon model index
					C_BaseCombatWeapon *pWeapon = GetActiveWeapon();

					if( pWeapon )
					{
						int nModelIndex = pWeapon->GetModelIndex();
						int nWorldModelIndex = pWeapon->GetWorldModelIndex();
						if ( bInToolRecordingMode && nModelIndex != nWorldModelIndex )
						{
							pWeapon->SetModelIndex( nWorldModelIndex );
						}

						pWeapon->GetAttachment( iAttachment, vecStart );

						if ( bInToolRecordingMode && nModelIndex != nWorldModelIndex )
						{
							pWeapon->SetModelIndex( nModelIndex );
						}
					}
				}

				if ( tf_useparticletracers.GetBool() )
				{
					const char *pszTracerEffect = GetTracerType();
					if ( pszTracerEffect && pszTracerEffect[0] )
					{
						char szTracerEffect[128];
						if ( nDamageType & DMG_CRITICAL )
						{
							Q_snprintf( szTracerEffect, sizeof(szTracerEffect), "%s_crit", pszTracerEffect );
							pszTracerEffect = szTracerEffect;
						}

						UTIL_ParticleTracer( pszTracerEffect, vecStart, trace.endpos, entindex(), iUseAttachment, true );
					}
				}
				else
				{
					UTIL_Tracer( vecStart, trace.endpos, entindex(), iUseAttachment, 5000, true, GetTracerType() );
				}
			}
#endif
		}

		// Server specific.
#ifndef CLIENT_DLL
		// See what material we hit.
		CTakeDamageInfo dmgInfo( this, info.m_pAttacker, GetActiveWeapon(), info.m_flDamage, nDamageType );
		dmgInfo.SetDamageCustom( nCustomDamageType );
		CalculateBulletDamageForce( &dmgInfo, info.m_iAmmoType, info.m_vecDirShooting, trace.endpos, 1.0 );	//MATTTODO bullet forces
		trace.m_pEnt->DispatchTraceAttack( dmgInfo, info.m_vecDirShooting, &trace );
#endif
	}
}

void CTFPlayer::DODFireBullets( const FireBulletsInfo_t &info )
{
	trace_t			tr;								
	trace_t			reverseTr;						//Used to find exit points
	static int		iMaxPenetrations	= 6;
	int				iPenetrations		= 0;
	float			flDamage			= info.m_flDamage;		//Remaining damage in the bullet
	Vector			vecSrc				= info.m_vecSrc;
	Vector			vecEnd				= vecSrc + info.m_vecDirShooting * info.m_flDistance;

	static int		iTraceMask = ( ( MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_HITBOX | CONTENTS_PRONE_HELPER ) & ~CONTENTS_GRATE );
	 
	CBaseEntity		*pLastHitEntity		= this;	// start with us so we don't trace ourselves
		
	int iDamageType = GetAmmoDef()->DamageType( info.m_iAmmoType );
	int iCollisionGroup = COLLISION_GROUP_NONE;

#ifdef GAME_DLL
	int iNumHeadshots = 0;
#endif

	while ( flDamage > 0 && iPenetrations < iMaxPenetrations )
	{
		//DevMsg( 2, "penetration: %d, starting dmg: %.1f\n", iPenetrations, flDamage );

		CBaseEntity *pPreviousHit = pLastHitEntity;

		// skip the shooter always
		CTraceFilterSkipTwoEntities ignoreShooterAndPrevious( this, pPreviousHit, iCollisionGroup );
		UTIL_TraceLine( vecSrc, vecEnd, iTraceMask, &ignoreShooterAndPrevious, &tr );

		const float rayExtension = 40.0f;
		UTIL_ClipTraceToPlayers( vecSrc, vecEnd + info.m_vecDirShooting * rayExtension, iTraceMask, &ignoreShooterAndPrevious, &tr );

		if ( tr.fraction == 1.0f )
			break; // we didn't hit anything, stop tracing shoot

		// New hitbox code that uses hitbox groups instead of trying to trace
		// through the player
		if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
		{
			switch( tr.hitgroup )
			{
#ifdef GAME_DLL
			case HITGROUP_HEAD:
				{
					if ( tr.m_pEnt->GetTeamNumber() != GetTeamNumber() )
					{
						iNumHeadshots++;
					}
				}
				break;
#endif

			case HITGROUP_LEFTARM:
			case HITGROUP_RIGHTARM:
				{
					//DevMsg( 2, "Hit arms, tracing against alt hitboxes.. \n" );

					CTFPlayer *pPlayer = ToTFPlayer( tr.m_pEnt );

					// Only DOD characters have this secondary hitbox set
					if ( pPlayer->IsDODClass() )
					{
						// set hitbox set to "dod_no_arms"
						pPlayer->SetHitboxSet( 1 );

						trace_t newTr;

						// re-fire the trace
						UTIL_TraceLine( vecSrc, vecEnd, iTraceMask, &ignoreShooterAndPrevious, &newTr );

						// if we hit the same player in the chest
						if ( tr.m_pEnt == newTr.m_pEnt )
						{
							//DevMsg( 2, ".. and we hit the chest.\n" );

							Assert( tr.hitgroup != newTr.hitgroup );	// If we hit this, hitbox sets are broken

							// use that damage instead
							tr = newTr;
						}

						// set hitboxes back to "dod"
						pPlayer->SetHitboxSet( 0 );
					}
				}
				break;

			default:
				break;
			}			
		}
			
		pLastHitEntity = tr.m_pEnt;

		if ( sv_showimpacts.GetBool() )
		{
#ifdef CLIENT_DLL
			// draw red client impact markers
			debugoverlay->AddBoxOverlay( tr.endpos, Vector(-1,-1,-1), Vector(1,1,1), QAngle(0,0,0), 255, 0, 0, 127, 4 );

			if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
			{
				C_BasePlayer *player = ToBasePlayer( tr.m_pEnt );
				player->DrawClientHitboxes( 4, true );
			}
#else
			// draw blue server impact markers
			NDebugOverlay::Box( tr.endpos, Vector(-1,-1,-1), Vector(1,1,1), 0,0,255,127, 4 );

			if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
			{
				CBasePlayer *player = ToBasePlayer( tr.m_pEnt );
				player->DrawServerHitboxes( 4, true );
			}
#endif
		}

#ifdef CLIENT_DLL
		// See if the bullet ended up underwater + started out of the water
		if ( enginetrace->GetPointContents( tr.endpos ) & (CONTENTS_WATER|CONTENTS_SLIME) )
		{	
			trace_t waterTrace;
			UTIL_TraceLine( vecSrc, tr.endpos, (MASK_SHOT|CONTENTS_WATER|CONTENTS_SLIME), this, iCollisionGroup, &waterTrace );
			
			if( waterTrace.allsolid != 1 )
			{
				CEffectData	data;
 				data.m_vOrigin = waterTrace.endpos;
				data.m_vNormal = waterTrace.plane.normal;
				data.m_flScale = random->RandomFloat( 8, 12 );

				if ( waterTrace.contents & CONTENTS_SLIME )
				{
					data.m_fFlags |= FX_WATER_IN_SLIME;
				}

				// Use TF varient since it looks better
				DispatchEffect( "tf_gunshotsplash", data );
			}
		}
		else
		{
			//Do Regular hit effects

			// Don't decal nodraw surfaces
			if ( !( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) ) )
			{
				CBaseEntity *pEntity = tr.m_pEnt;
				if ( !( !friendlyfire.GetBool() && pEntity && pEntity->GetTeamNumber() == GetTeamNumber() ) )
				{
					UTIL_ImpactTrace( &tr, iDamageType );
				}
			}
		}
#endif

		// Get surface where the bullet entered ( if it had different surfaces on enter and exit )
		surfacedata_t *pSurfaceData = physprops->GetSurfaceData( tr.surface.surfaceProps );
		Assert( pSurfaceData );
		
		float flMaterialMod = GetDensityFromMaterial(pSurfaceData);

		if ( iDamageType & DMG_MACHINEGUN )
		{
			flMaterialMod *= 0.65;
		}

		// try to penetrate object
		Vector penetrationEnd;
		float flMaxDistance = flDamage / flMaterialMod;

#ifndef CLIENT_DLL
		ClearMultiDamage();

		float flActualDamage = flDamage;

		CTakeDamageInfo dmgInfo( info.m_pAttacker, info.m_pAttacker, flActualDamage, iDamageType );
		CalculateBulletDamageForce( &dmgInfo, info.m_iAmmoType, info.m_vecDirShooting, tr.endpos );
		tr.m_pEnt->DispatchTraceAttack( dmgInfo, info.m_vecDirShooting, &tr );

		DevMsg( 2, "Giving damage ( %.1f ) to entity of type %s\n", flActualDamage, tr.m_pEnt->GetClassname() );

		TraceAttackToTriggers( dmgInfo, tr.startpos, tr.endpos, info.m_vecDirShooting );
#endif

		int stepsize = 16;

		// displacement always stops the bullet
		if ( tr.IsDispSurface() )
		{
			DevMsg( 2, "bullet was stopped by displacement\n" );
			ApplyMultiDamage();
			break;
		}

		// trace through the solid to find the exit point and how much material we went through
		if ( !TraceToExit( tr.endpos, info.m_vecDirShooting, penetrationEnd, stepsize, flMaxDistance ) )
		{
			DevMsg( 2, "bullet was stopped\n" );
			ApplyMultiDamage();
			break;
		}

		// find exact penetration exit
		CTraceFilterSimple ignoreShooter( this, iCollisionGroup );
		UTIL_TraceLine( penetrationEnd, tr.endpos, iTraceMask, &ignoreShooter, &reverseTr );

		// Now we can apply the damage, after we have traced the entity
		// so it doesn't break or die before we have a change to test against it
#ifndef CLIENT_DLL
		ApplyMultiDamage();
#endif

		// Continue looking for the exit point
		if( reverseTr.m_pEnt != tr.m_pEnt && reverseTr.m_pEnt != NULL )
		{
			// something was blocking, trace again
			CTraceFilterSkipTwoEntities ignoreShooterAndBlocker( this, reverseTr.m_pEnt, iCollisionGroup );
			UTIL_TraceLine( penetrationEnd, tr.endpos, iTraceMask, &ignoreShooterAndBlocker, &reverseTr );
		}

		if ( sv_showimpacts.GetBool() )
		{
			debugoverlay->AddLineOverlay( penetrationEnd, reverseTr.endpos, 255, 0, 0, true, 3.0 );
		}

		// penetration was successful

#ifdef CLIENT_DLL
		// bullet did penetrate object, exit Decal
		if ( !( reverseTr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) ) )
		{
			CBaseEntity *pEntity = reverseTr.m_pEnt;
			if ( !( !friendlyfire.GetBool() && pEntity && pEntity->GetTeamNumber() == GetTeamNumber() ) )
			{
				UTIL_ImpactTrace( &reverseTr, iDamageType );
			}
		}
#endif

		//setup new start end parameters for successive trace

		// New start point is our last exit point
		vecSrc = reverseTr.endpos + /* 1.0 * */ info.m_vecDirShooting;

		// Reduce bullet damage by material and distanced travelled through that material
		// if it is < 0 we won't go through the loop again
		float flTraceDistance = VectorLength( reverseTr.endpos - tr.endpos );
		
		flDamage -= flMaterialMod * flTraceDistance;

		if( flDamage > 0 )
		{
			DevMsg( 2, "Completed penetration, new damage is %.1f\n", flDamage );
		}
		else
		{
			DevMsg( 2, "bullet was stopped\n" );
		}

		iPenetrations++;
	}
}

ConVar sv_showplayerhitboxes( "sv_showplayerhitboxes", "0", FCVAR_REPLICATED, "Show lag compensated hitboxes for the specified player index whenever a player fires." );
#define	CS_MASK_SHOOT (MASK_SOLID|CONTENTS_DEBRIS)

void CTFPlayer::GetBulletTypeParameters(
	int iBulletType,
	float &fPenetrationPower,
	float &flPenetrationDistance )
{
	//MIKETODO: make ammo types come from a script file.
	if ( IsAmmoType( iBulletType, g_aAmmoNames[BULLET_PLAYER_50AE] ) )
	{
		fPenetrationPower = 30;
		flPenetrationDistance = 1000.0;
	}
	else if ( IsAmmoType( iBulletType, g_aAmmoNames[BULLET_PLAYER_762MM] ) )
	{
		fPenetrationPower = 39;
		flPenetrationDistance = 5000.0;
	}
	else if ( IsAmmoType( iBulletType, g_aAmmoNames[BULLET_PLAYER_556MM] ) ||
			  IsAmmoType( iBulletType, g_aAmmoNames[BULLET_PLAYER_556MM_BOX] ) )
	{
		fPenetrationPower = 35;
		flPenetrationDistance = 4000.0;
	}
	else if ( IsAmmoType( iBulletType, g_aAmmoNames[BULLET_PLAYER_338MAG] ) )
	{
		fPenetrationPower = 45;
		flPenetrationDistance = 8000.0;
	}
	else if ( IsAmmoType( iBulletType, g_aAmmoNames[BULLET_PLAYER_9MM] ) )
	{
		fPenetrationPower = 21;
		flPenetrationDistance = 800.0;
	}
	else if ( IsAmmoType( iBulletType, g_aAmmoNames[BULLET_PLAYER_BUCKSHOT] ) )
	{
		fPenetrationPower = 0;
		flPenetrationDistance = 0.0;
	}
	else if ( IsAmmoType( iBulletType, g_aAmmoNames[BULLET_PLAYER_45ACP] ) )
	{
		fPenetrationPower = 15;
		flPenetrationDistance = 500.0;
	}
	else if ( IsAmmoType( iBulletType, g_aAmmoNames[BULLET_PLAYER_357SIG] ) )
	{
		fPenetrationPower = 25;
		flPenetrationDistance = 800.0;
	}
	else if ( IsAmmoType( iBulletType, g_aAmmoNames[BULLET_PLAYER_57MM] ) )
	{
		fPenetrationPower = 30;
		flPenetrationDistance = 2000.0;
	}
	else
	{
		// What kind of ammo is this?
		Assert( false );
		fPenetrationPower = 0;
		flPenetrationDistance = 0.0;
	}
}

static void GetMaterialParameters( int iMaterial, float &flPenetrationModifier, float &flDamageModifier )
{
	switch ( iMaterial )
	{
		case CHAR_TEX_METAL :
			flPenetrationModifier = 0.5;  // If we hit metal, reduce the thickness of the brush we can't penetrate
			flDamageModifier = 0.3;
			break;
		case CHAR_TEX_DIRT :
			flPenetrationModifier = 0.5;
			flDamageModifier = 0.3;
			break;
		case CHAR_TEX_CONCRETE :
			flPenetrationModifier = 0.4;
			flDamageModifier = 0.25;
			break;
		case CHAR_TEX_GRATE	:
			flPenetrationModifier = 1.0;
			flDamageModifier = 0.99;
			break;
		case CHAR_TEX_VENT :
			flPenetrationModifier = 0.5;
			flDamageModifier = 0.45;
			break;
		case CHAR_TEX_TILE :
			flPenetrationModifier = 0.65;
			flDamageModifier = 0.3;
			break;
		case CHAR_TEX_COMPUTER :
			flPenetrationModifier = 0.4;
			flDamageModifier = 0.45;
			break;
		case CHAR_TEX_WOOD :
			flPenetrationModifier = 1.0;
			flDamageModifier = 0.6;
			break;
		default :
			flPenetrationModifier = 1.0;
			flDamageModifier = 0.5;
			break;
	}

	Assert( flPenetrationModifier > 0 );
	Assert( flDamageModifier < 1.0f ); // Less than 1.0f for avoiding infinite loops
}

inline void UTIL_TraceLineIgnoreTwoEntities( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask,
					 const IHandleEntity *ignore, const IHandleEntity *ignore2, int collisionGroup, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd );
	CTraceFilterSkipTwoEntities traceFilter( ignore, ignore2, collisionGroup );
	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
	if( r_visualizetraces.GetBool() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, -1.0f );
	}
}

void CTFPlayer::CSFireBullet(
	Vector vecSrc,	// shooting postion
	const QAngle &shootAngles,  //shooting angle
	float flDistance, // max distance
	int iPenetration, // how many obstacles can be penetrated
	int iBulletType, // ammo type
	int iDamage, // base damage
	float flRangeModifier, // damage range modifier
	CBaseEntity *pevAttacker, // shooter
	bool bDoEffects,
	float xSpread, float ySpread
	)
{
	float fCurrentDamage = iDamage;   // damage of the bullet at it's current trajectory
	float flCurrentDistance = 0.0;  //distance that the bullet has traveled so far

	Vector vecDirShooting, vecRight, vecUp;
	AngleVectors( shootAngles, &vecDirShooting, &vecRight, &vecUp );

	// MIKETODO: put all the ammo parameters into a script file and allow for CS-specific params.
	float flPenetrationPower = 0;		// thickness of a wall that this bullet can penetrate
	float flPenetrationDistance = 0;	// distance at which the bullet is capable of penetrating a wall
	float flDamageModifier = 0.5;		// default modification of bullets power after they go through a wall.
	float flPenetrationModifier = 1.f;

	GetBulletTypeParameters( iBulletType, flPenetrationPower, flPenetrationDistance );


	if ( !pevAttacker )
		pevAttacker = this;  // the default attacker is ourselves

	// add the spray
	Vector vecDir = vecDirShooting + xSpread * vecRight + ySpread * vecUp;

	VectorNormalize( vecDir );

	//Adrian: visualize server/client player positions
	//This is used to show where the lag compesator thinks the player should be at.
#if 0
	for ( int k = 1; k <= gpGlobals->maxClients; k++ )
	{
		CBasePlayer *clientClass = (CBasePlayer *)CBaseEntity::Instance( k );

		if ( clientClass == NULL )
			 continue;

		if ( k == entindex() )
			 continue;

#ifdef CLIENT_DLL
		debugoverlay->AddBoxOverlay( clientClass->GetAbsOrigin(), clientClass->WorldAlignMins(), clientClass->WorldAlignMaxs(), QAngle( 0, 0, 0), 255,0,0,127, 4 );
#else
		NDebugOverlay::Box( clientClass->GetAbsOrigin(), clientClass->WorldAlignMins(), clientClass->WorldAlignMaxs(), 0,0,255,127, 4 );
#endif

	}

#endif


//=============================================================================
// HPE_BEGIN:
//=============================================================================

#ifndef CLIENT_DLL
	// [menglish] Increment the shots fired for this player
	// SEALTODO
	//CCS_GameStats.Event_ShotFired( this, GetActiveWeapon() );
#endif

//=============================================================================
// HPE_END
//=============================================================================

	bool bFirstHit = true;

	CBasePlayer *lastPlayerHit = NULL;

	if( sv_showplayerhitboxes.GetInt() > 0 )
	{
		CBasePlayer *lagPlayer = UTIL_PlayerByIndex( sv_showplayerhitboxes.GetInt() );
		if( lagPlayer )
		{
#ifdef CLIENT_DLL
			lagPlayer->DrawClientHitboxes(4, true);
#else
			lagPlayer->DrawServerHitboxes(4, true);
#endif
		}
	}

	MDLCACHE_CRITICAL_SECTION();
	while ( fCurrentDamage > 0 )
	{
		Vector vecEnd = vecSrc + vecDir * flDistance;

		trace_t tr; // main enter bullet trace

		UTIL_TraceLineIgnoreTwoEntities( vecSrc, vecEnd, CS_MASK_SHOOT|CONTENTS_HITBOX, this, lastPlayerHit, COLLISION_GROUP_NONE, &tr );
		{
			CTraceFilterSkipTwoEntities filter( this, lastPlayerHit, COLLISION_GROUP_NONE );

			// Check for player hitboxes extending outside their collision bounds
			const float rayExtension = 40.0f;
			UTIL_ClipTraceToPlayers( vecSrc, vecEnd + vecDir * rayExtension, CS_MASK_SHOOT|CONTENTS_HITBOX, &filter, &tr );
		}

		lastPlayerHit = ToBasePlayer(tr.m_pEnt);

		if ( tr.fraction == 1.0f )
			break; // we didn't hit anything, stop tracing shoot

#ifdef _DEBUG
		if ( bFirstHit )
			AddBulletStat( gpGlobals->realtime, VectorLength( vecSrc-tr.endpos), tr.endpos );
#endif

		bFirstHit = false;

#ifndef CLIENT_DLL
		//
		// Propogate a bullet impact event
		// @todo Add this for shotgun pellets (which dont go thru here)
		//
		IGameEvent * event = gameeventmanager->CreateEvent( "bullet_impact" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetFloat( "x", tr.endpos.x );
			event->SetFloat( "y", tr.endpos.y );
			event->SetFloat( "z", tr.endpos.z );
			gameeventmanager->FireEvent( event );
		}
#endif

		/************* MATERIAL DETECTION ***********/
		surfacedata_t *pSurfaceData = physprops->GetSurfaceData( tr.surface.surfaceProps );
		int iEnterMaterial = pSurfaceData->game.material;

		GetMaterialParameters( iEnterMaterial, flPenetrationModifier, flDamageModifier );



#pragma warning( push )
#pragma warning( disable : 4800)
		bool hitGrate = tr.contents & CONTENTS_GRATE;
#pragma warning( pop )

		// since some railings in de_inferno are CONTENTS_GRATE but CHAR_TEX_CONCRETE, we'll trust the
		// CONTENTS_GRATE and use a high damage modifier.
		if ( hitGrate )
		{
			// If we're a concrete grate (TOOLS/TOOLSINVISIBLE texture) allow more penetrating power.
			flPenetrationModifier = 1.0f;
			flDamageModifier = 0.99f;
		}

#ifdef CLIENT_DLL
		if ( sv_showimpacts.GetInt() == 1 || sv_showimpacts.GetInt() == 2 )
		{
			// draw red client impact markers
			debugoverlay->AddBoxOverlay( tr.endpos, Vector(-2,-2,-2), Vector(2,2,2), QAngle( 0, 0, 0), 255,0,0,127, 4 );

			if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
			{
				C_BasePlayer *player = ToBasePlayer( tr.m_pEnt );
				player->DrawClientHitboxes( 4, true );
			}
		}
#else
		if ( sv_showimpacts.GetInt() == 1 || sv_showimpacts.GetInt() == 3 )
		{
			// draw blue server impact markers
			NDebugOverlay::Box( tr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,0,255,127, 4 );

			if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
			{
				CBasePlayer *player = ToBasePlayer( tr.m_pEnt );
				player->DrawServerHitboxes( 4, true );
			}
		}
#endif

		//calculate the damage based on the distance the bullet travelled.
		flCurrentDistance += tr.fraction * flDistance;
		fCurrentDamage *= pow (flRangeModifier, (flCurrentDistance / 500));

		// check if we reach penetration distance, no more penetrations after that
		if (flCurrentDistance > flPenetrationDistance && iPenetration > 0)
			iPenetration = 0;

#ifndef CLIENT_DLL
		// This just keeps track of sounds for AIs (it doesn't play anything).
		CSoundEnt::InsertSound( SOUND_BULLET_IMPACT, tr.endpos, 400, 0.2f, this );
#endif

		int iDamageType = DMG_BULLET | DMG_NEVERGIB;

		if( bDoEffects )
		{
			// See if the bullet ended up underwater + started out of the water
			if ( enginetrace->GetPointContents( tr.endpos ) & (CONTENTS_WATER|CONTENTS_SLIME) )
			{
				trace_t waterTrace;
				UTIL_TraceLine( vecSrc, tr.endpos, (MASK_SHOT|CONTENTS_WATER|CONTENTS_SLIME), this, COLLISION_GROUP_NONE, &waterTrace );

				if( waterTrace.allsolid != 1 )
				{
					CEffectData	data;
 					data.m_vOrigin = waterTrace.endpos;
					data.m_vNormal = waterTrace.plane.normal;
					data.m_flScale = random->RandomFloat( 8, 12 );

					if ( waterTrace.contents & CONTENTS_SLIME )
					{
						data.m_fFlags |= FX_WATER_IN_SLIME;
					}

					DispatchEffect( "gunshotsplash", data );
				}
			}
			else
			{
				//Do Regular hit effects

				// Don't decal nodraw surfaces
				if ( !( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) ) )
				{
					CBaseEntity *pEntity = tr.m_pEnt;
					if ( !( !friendlyfire.GetBool() && pEntity && pEntity->GetTeamNumber() == GetTeamNumber() ) )
					{
						UTIL_ImpactTrace( &tr, iDamageType );
					}
				}
			}
		} // bDoEffects

		// add damage to entity that we hit

#ifndef CLIENT_DLL
		ClearMultiDamage();

		//=============================================================================
		// HPE_BEGIN:
		// [pfreese] Check if enemy players were killed by this bullet, and if so,
		// add them to the iPenetrationKills count
		//=============================================================================
		
		CBaseEntity *pEntity = tr.m_pEnt;

		CTakeDamageInfo info( pevAttacker, pevAttacker, fCurrentDamage, iDamageType );
		CalculateBulletDamageForce( &info, iBulletType, vecDir, tr.endpos );
		pEntity->DispatchTraceAttack( info, vecDir, &tr );

		TraceAttackToTriggers( info, tr.startpos, tr.endpos, vecDir );

		ApplyMultiDamage();
		
		//=============================================================================
		// HPE_END
		//=============================================================================

#endif

		// check if bullet can penetrate another entity
		if ( iPenetration == 0 && !hitGrate )
			break; // no, stop

		// If we hit a grate with iPenetration == 0, stop on the next thing we hit
		if ( iPenetration < 0 )
			break;

		Vector penetrationEnd;

		// try to penetrate object, maximum penetration is 128 inch
		if ( !TraceToExit( tr.endpos, vecDir, penetrationEnd, 24, 128 ) )
			break;

		// find exact penetration exit
		trace_t exitTr;
		UTIL_TraceLine( penetrationEnd, tr.endpos, CS_MASK_SHOOT|CONTENTS_HITBOX, NULL, &exitTr );

		if( exitTr.m_pEnt != tr.m_pEnt && exitTr.m_pEnt != NULL )
		{
			// something was blocking, trace again
			UTIL_TraceLine( penetrationEnd, tr.endpos, CS_MASK_SHOOT|CONTENTS_HITBOX, exitTr.m_pEnt, COLLISION_GROUP_NONE, &exitTr );
		}

		// get material at exit point
		pSurfaceData = physprops->GetSurfaceData( exitTr.surface.surfaceProps );
		int iExitMaterial = pSurfaceData->game.material;

		hitGrate = hitGrate && ( exitTr.contents & CONTENTS_GRATE );

		// if enter & exit point is wood or metal we assume this is
		// a hollow crate or barrel and give a penetration bonus
		if ( iEnterMaterial == iExitMaterial )
		{
			if( iExitMaterial == CHAR_TEX_WOOD ||
				iExitMaterial == CHAR_TEX_METAL )
			{
				flPenetrationModifier *= 2;
			}
		}

		float flTraceDistance = VectorLength( exitTr.endpos - tr.endpos );

		// check if bullet has enough power to penetrate this distance for this material
		if ( flTraceDistance > ( flPenetrationPower * flPenetrationModifier ) )
			break; // bullet hasn't enough power to penetrate this distance

		// penetration was successful

		// bullet did penetrate object, exit Decal
		if ( bDoEffects )
		{
			UTIL_ImpactTrace( &exitTr, iDamageType );
		}

		//setup new start end parameters for successive trace

		flPenetrationPower -= flTraceDistance / flPenetrationModifier;
		flCurrentDistance += flTraceDistance;

		// NDebugOverlay::Box( exitTr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,255,0,127, 8 );

		vecSrc = exitTr.endpos;
		flDistance = (flDistance - flCurrentDistance) * 0.5;

		// reduce damage power each time we hit something other than a grate
		fCurrentDamage *= flDamageModifier;

		// reduce penetration counter
		iPenetration--;
	}
}

#ifdef CLIENT_DLL
static ConVar tf_impactwatertimeenable( "tf_impactwatertimeenable", "0", FCVAR_CHEAT, "Draw impact debris effects." );
static ConVar tf_impactwatertime( "tf_impactwatertime", "1.0f", FCVAR_CHEAT, "Draw impact debris effects." );
#endif

//-----------------------------------------------------------------------------
// Purpose: Trace from the shooter to the point of impact (another player,
//          world, etc.), but this time take into account water/slime surfaces.
//   Input: trace - initial trace from player to point of impact
//          vecStart - starting point of the trace 
//-----------------------------------------------------------------------------
void CTFPlayer::ImpactWaterTrace( trace_t &trace, const Vector &vecStart )
{
#ifdef CLIENT_DLL
	if ( tf_impactwatertimeenable.GetBool() )
	{
		if ( m_flWaterImpactTime > gpGlobals->curtime )
			return;
	}
#endif 

	trace_t traceWater;
	UTIL_TraceLine( vecStart, trace.endpos, ( MASK_SHOT | CONTENTS_WATER | CONTENTS_SLIME ), 
		this, COLLISION_GROUP_NONE, &traceWater );
	if( traceWater.fraction < 1.0f )
	{
		CEffectData	data;
		data.m_vOrigin = traceWater.endpos;
		data.m_vNormal = traceWater.plane.normal;
		data.m_flScale = random->RandomFloat( 8, 12 );
		if ( traceWater.contents & CONTENTS_SLIME )
		{
			data.m_fFlags |= FX_WATER_IN_SLIME;
		}

		const char *pszEffectName = "tf_gunshotsplash";
		CPCWeaponBase *pWeapon = GetActivePCWeapon();
		if ( pWeapon && ( TF_WEAPON_MINIGUN == pWeapon->GetWeaponID() ) )
		{
			// for the minigun, use a different, cheaper splash effect because it can create so many of them
			pszEffectName = "tf_gunshotsplash_minigun";
		}		
		DispatchEffect( pszEffectName, data );

#ifdef CLIENT_DLL
		if ( tf_impactwatertimeenable.GetBool() )
		{
			m_flWaterImpactTime = gpGlobals->curtime + tf_impactwatertime.GetFloat();
		}
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPCWeaponBase *CTFPlayer::GetActivePCWeapon( void ) const
{
	CBaseCombatWeapon *pRet = GetActiveWeapon();
	if ( pRet )
	{
		switch ( pRet->GetWeaponTypeReference() )
		{
			case WEAPON_TF:
			{
				Assert( dynamic_cast< CTFWeaponBase* >( pRet ) != NULL );
				return dynamic_cast< CTFWeaponBase * >( pRet );
				break;
			}
			case WEAPON_DOD:
			{
				Assert( dynamic_cast< CWeaponDODBase* >( pRet ) != NULL );
				return dynamic_cast< CWeaponDODBase * >( pRet );
				break;
			}
			case WEAPON_CS:
			{
				Assert( dynamic_cast< CWeaponCSBase* >( pRet ) != NULL );
				return dynamic_cast< CWeaponCSBase * >( pRet );
				break;
			}
			default:
			{
				Assert( dynamic_cast< CPCWeaponBase* >( pRet ) != NULL );
				return dynamic_cast< CPCWeaponBase * >( pRet );
				break;
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPCWeaponBase*CTFPlayerShared::GetActivePCWeapon() const
{
	return m_pOuter->GetActivePCWeapon();
}

//-----------------------------------------------------------------------------
// Purpose: How much build resource ( metal ) does this player have
//-----------------------------------------------------------------------------
int CTFPlayer::GetBuildResources( void )
{
	return GetAmmoCount( TF_AMMO_METAL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::CanPlayerMove() const
{
	bool bFreezeOnRestart = tf_player_movement_restart_freeze.GetBool();
	if ( bFreezeOnRestart )
	{
#if defined( _DEBUG ) || defined( STAGING_ONLY )
		if ( mp_developer.GetBool() )
			bFreezeOnRestart = false;
#endif // _DEBUG || STAGING_ONLY
	}

	bool bInRoundRestart = TFGameRules() && TFGameRules()->InRoundRestart();

	bool bNoMovement = bInRoundRestart && bFreezeOnRestart;

	return !bNoMovement;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::TeamFortress_SetSpeed()
{
	int playerclass = GetPlayerClass()->GetClassIndex();
	float maxfbspeed;

	// SEALTODO, unify this with "CDODGameMovement::SetPlayerSpeed"
	if ( IsDODClass() )
		return;

	// Spectators can move while in Classic Observer mode
	if ( IsObserver() )
	{
		if ( GetObserverMode() == OBS_MODE_ROAMING )
			SetMaxSpeed( GetPlayerClassData( TF_CLASS_SCOUT )->m_flMaxSpeed );
		else
			SetMaxSpeed( 0 );
		return;
	}

	// Check for any reason why they can't move at all
	if ( playerclass == TF_CLASS_UNDEFINED || !CanPlayerMove() )
	{
		SetAbsVelocity( vec3_origin );
		SetMaxSpeed( 1 );
		return;
	}

	// First, get their max class speed
	maxfbspeed = GetPlayerClassData( playerclass )->m_flMaxSpeed;

	bool bAllowSlowing = m_Shared.InCond( TF_COND_HALLOWEEN_BOMB_HEAD ) ? false : true;

	// Slow us down if we're disguised as a slower class
	// unless we're cloaked..
	if ( m_Shared.InCond( TF_COND_DISGUISED ) && !m_Shared.IsStealthed() )
	{
		float flMaxDisguiseSpeed = GetPlayerClassData( m_Shared.GetDisguiseClass() )->m_flMaxSpeed;
		maxfbspeed = MIN( flMaxDisguiseSpeed, maxfbspeed );
	}

	// Second, see if any flags are slowing them down
	if ( HasItem() && GetItem()->GetItemID() == TF_ITEM_CAPTURE_FLAG )
	{
		CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag*>( GetItem() );

		if ( pFlag )
		{
			if ( pFlag->GetType() == TF_FLAGTYPE_ATTACK_DEFEND || pFlag->GetType() == TF_FLAGTYPE_TERRITORY_CONTROL )
			{
				maxfbspeed *= 0.5;
			}
		}
	}

	// if they're a sniper, and they're aiming, their speed must be 80 or less
	if ( m_Shared.InCond( TF_COND_AIMING ) )
	{
		// Pyro's move faster while firing their flamethrower
		if ( playerclass == TF_CLASS_PYRO )
		{
			if (maxfbspeed > 200)
				maxfbspeed = 200;
		}
		else
		{
			if (maxfbspeed > 80)
				maxfbspeed = 80;
		}
	}

	if ( m_Shared.InCond( TF_COND_STEALTHED ) )
	{
		if (maxfbspeed > tf_spy_max_cloaked_speed.GetFloat() )
		{
			maxfbspeed = tf_spy_max_cloaked_speed.GetFloat();
		}
	}

	// if we're in bonus time because a team has won, give the winners 110% speed and the losers 90% speed
	if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN )
	{
		int iWinner = TFGameRules()->GetWinningTeam();
		
		if ( iWinner != TEAM_UNASSIGNED )
		{
			if ( iWinner == GetTeamNumber() )
			{
				maxfbspeed *= 1.1f;
			}
			else
			{
				maxfbspeed *= 0.9f;
			}
		}
	}

	bool bCarryPenalty = true;

	if ( m_Shared.IsCarryingObject() && bCarryPenalty && bAllowSlowing )
	{
		maxfbspeed *= 0.90f;
	}

	if ( m_Shared.IsLoserStateStunned() && bAllowSlowing )
	{
		// Yikes is not as slow, terrible gotcha
		if ( m_Shared.GetActiveStunInfo()->iStunFlags & TF_STUN_BY_TRIGGER ) 
		{
			maxfbspeed *= 0.75f;
		}
		else
		{
			maxfbspeed *= 0.5f;
		}
	}

	// Set the speed
	SetMaxSpeed( maxfbspeed );

	if ( maxfbspeed <= 0.0f )
	{
		SetAbsVelocity( vec3_origin );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayer::HasItem( void ) const
{
	return ( m_hItem != NULL );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::SetItem( CTFItem *pItem )
{
	m_hItem = pItem;

#ifndef CLIENT_DLL
	if ( pItem )
	{
		AddGlowEffect();
	}
	else
	{
		RemoveGlowEffect();
	}

	if ( pItem && pItem->GetItemID() == TF_ITEM_CAPTURE_FLAG )
	{
		RemoveInvisibility();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFItem	*CTFPlayer::GetItem( void ) const
{
	return m_hItem;
}

//-----------------------------------------------------------------------------
// Purpose: Is the player carrying the flag?
//-----------------------------------------------------------------------------
bool CTFPlayer::HasTheFlag( ETFFlagType exceptionTypes[], int nNumExceptions ) const
{
	if ( HasItem() && GetItem()->GetItemID() == TF_ITEM_CAPTURE_FLAG )
	{
		CCaptureFlag* pFlag = static_cast< CCaptureFlag* >( GetItem() );

		for( int i=0; i < nNumExceptions; ++i )
		{
			if ( exceptionTypes[ i ] == pFlag->GetType() )
				return false;
		}

		return true;
	}

	return false;
}

bool CTFPlayer::IsAllowedToPickUpFlag( void ) const
{
	int iCannotPickUpIntelligence = 0;
	if ( iCannotPickUpIntelligence )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCaptureZone *CTFPlayer::GetCaptureZoneStandingOn( void )
{
	touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
	if ( root )
	{
		for ( touchlink_t *link = root->nextLink; link != root; link = link->nextLink )
		{
			CBaseEntity *pTouch = link->entityTouched;
			if ( pTouch && pTouch->IsSolidFlagSet( FSOLID_TRIGGER ) && pTouch->IsBSPModel() )
			{
				CCaptureZone *pAreaTrigger = dynamic_cast< CCaptureZone* >(pTouch);
				if ( pAreaTrigger )
				{
					return pAreaTrigger;
				}
			}
		}
	}

	return NULL;
}

CCaptureZone *CTFPlayer::GetClosestCaptureZone( void )
{
	CCaptureZone *pCaptureZone = NULL;
	float flClosestDistance = FLT_MAX;

	for ( int i=0; i<ICaptureZoneAutoList::AutoList().Count(); ++i )
	{
		CCaptureZone *pTempCaptureZone = static_cast< CCaptureZone* >( ICaptureZoneAutoList::AutoList()[i] );
		if ( !pTempCaptureZone->IsDisabled() && pTempCaptureZone->GetTeamNumber() == GetTeamNumber() )
		{
			float fCurrentDistance = GetAbsOrigin().DistTo( pTempCaptureZone->WorldSpaceCenter() );
			if ( flClosestDistance > fCurrentDistance )
			{
				pCaptureZone = pTempCaptureZone;
				flClosestDistance = fCurrentDistance;
			}
		}
	}

	return pCaptureZone;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this player's allowed to build another one of the specified object
//-----------------------------------------------------------------------------
int CTFPlayer::CanBuild( int iObjectType, int iObjectMode )
{
	if ( iObjectType < 0 || iObjectType >= OBJ_LAST )
		return CB_UNKNOWN_OBJECT;

	const CObjectInfo *pInfo = GetObjectInfo( iObjectType );
	if ( pInfo && ((iObjectMode > pInfo->m_iNumAltModes) || (iObjectMode < 0)) )
		return CB_CANNOT_BUILD;

	// Does this type require a specific builder?
	bool bHasSubType = false;
	CTFWeaponBuilder *pBuilder = CTFPlayerSharedUtils::GetBuilderForObjectType( this, iObjectType );
	if ( pBuilder )
	{
		bHasSubType = true;
	}

	if ( TFGameRules() )
	{
		if ( TFGameRules()->IsTruceActive() && ( iObjectType == OBJ_ATTACHMENT_SAPPER ) )
			return CB_CANNOT_BUILD;
	}

#ifndef CLIENT_DLL
	CTFPlayerClass *pCls = GetPlayerClass();

	if ( !bHasSubType && pCls && pCls->CanBuildObject( iObjectType ) == false )
	{
		return CB_CANNOT_BUILD;
	}
#endif

	// We can redeploy the object if we are carrying it.
	CBaseObject* pObjType = GetObjectOfType( iObjectType, iObjectMode );
	if ( pObjType && pObjType->IsCarried() )
	{
		return CB_CAN_BUILD;
	}

	// Allow MVM engineer bots to have multiple sentries.  Currently they only need this so
	// they can appear to be carrying a new building when advancing their nest rather than
	// transporting an existing building.
	//if( TFGameRules() && TFGameRules()->IsMannVsMachineMode() && IsBot() )
	//{
	//	return CB_CAN_BUILD;
	//}

	// Make sure we haven't hit maximum number
	int iObjectCount = GetNumObjects( iObjectType, iObjectMode );
	if ( iObjectCount >= GetObjectInfo( iObjectType )->m_nMaxObjects && GetObjectInfo( iObjectType )->m_nMaxObjects != -1)
	{
		return CB_LIMIT_REACHED;
	}

	// Find out how much the object should cost
	int iCost = m_Shared.CalculateObjectCost( this, iObjectType );

	// Make sure we have enough resources
	if ( GetBuildResources() < iCost )
	{
		return CB_NEED_RESOURCES;
	}

	return CB_CAN_BUILD;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsAiming( void )
{
	if ( !m_pOuter )
		return false;

	bool bAiming = InCond( TF_COND_AIMING ) && !m_pOuter->IsPlayerClass( TF_CLASS_SOLDIER );
	if ( m_pOuter->IsPlayerClass( TF_CLASS_SNIPER ) && m_pOuter->GetActivePCWeapon() )
	{
		bAiming = InCond( TF_COND_ZOOMED );
	}

	return bAiming;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of objects of the specified type that this player has
//-----------------------------------------------------------------------------
int CTFPlayer::GetNumObjects( int iObjectType, int iObjectMode /*= 0*/ )
{
	int iCount = 0;
	for (int i = 0; i < GetObjectCount(); i++)
	{
		if ( !GetObject(i) )
			continue;

		if ( GetObject(i)->IsDisposableBuilding() )
			continue;

		if ( GetObject(i)->GetType() == iObjectType && 
			( GetObject(i)->GetObjectMode() == iObjectMode || iObjectMode == BUILDING_MODE_ANY ) )
		{
			iCount++;
		}
	}

	return iCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayerShared::CalculateObjectCost( CTFPlayer* pBuilder, int iObjectType )
{
	int nCost = InternalCalculateObjectCost( iObjectType );

	if ( iObjectType == OBJ_TELEPORTER )
	{
		float flCostMod = 1.f;
		if ( flCostMod != 1.f )
		{
			nCost *= flCostMod;
		}
	}
	
	return nCost;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::HealthKitPickupEffects( int iHealthGiven /*= 0*/ )
{
	// Healthkits also contain a fire blanket.
	if ( InCond( TF_COND_BURNING ) )
	{
		RemoveCond( TF_COND_BURNING );		
	}

	// Spawns a number on the player's health bar in the HUD, and also
	// spawns a "+" particle over their head for enemies to see
	if ( iHealthGiven && !IsStealthed() && m_pOuter )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "player_healonhit" );
		if ( event )
		{
			event->SetInt( "amount", iHealthGiven );
			event->SetInt( "entindex", m_pOuter->entindex() );
			gameeventmanager->FireEvent( event ); 
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsLoser( void )
{
	if ( !m_pOuter->IsTF2Class() )
		return false;

	if ( tf_always_loser.GetBool() )
		return true;

	if ( !TFGameRules() )
		return false;

	if ( TFGameRules()->State_Get() != GR_STATE_TEAM_WIN )
		return false;

	bool bLoser = TFGameRules()->GetWinningTeam() != m_pOuter->GetTeamNumber();

	int iClass = m_pOuter->GetPlayerClass()->GetClassIndex();

	// don't reveal disguised spies
	if ( bLoser && iClass == TF_CLASS_SPY )
	{
		if ( InCond( TF_COND_DISGUISED ) && GetDisguiseTeam() == TFGameRules()->GetWinningTeam() )
		{
			bLoser = false;
		}
	}

	return bLoser;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::UpdateCloakMeter( void )
{
	if ( !m_pOuter->IsPlayerClass( TF_CLASS_SPY ) )
		return;

	if ( InCond( TF_COND_STEALTHED ) )
	{
		// Classic cloak: drain at a fixed rate.
		m_flCloakMeter -= gpGlobals->frametime * m_fCloakConsumeRate;

		if ( m_flCloakMeter <= 0.0f )	
		{
			FadeInvis( 1.0f );
		}

		// Update Debuffs
		// Decrease duration if cloaked
#ifdef GAME_DLL
		// staging_spy
		float flReduction = gpGlobals->frametime * 0.75f;
		for ( int i = 0; g_aDebuffConditions[i] != TF_COND_LAST; i++ )
		{
			if ( InCond( g_aDebuffConditions[i] ) )
			{
				if ( m_ConditionData[g_aDebuffConditions[i]].m_flExpireTime != PERMANENT_CONDITION )
				{			
					m_ConditionData[g_aDebuffConditions[i]].m_flExpireTime = MAX( m_ConditionData[g_aDebuffConditions[i]].m_flExpireTime - flReduction, 0 );
				}
				// Burning and Bleeding and extra timers
				if ( g_aDebuffConditions[i] == TF_COND_BURNING )
				{
					// Reduce the duration of this burn 
					m_flFlameRemoveTime -= flReduction;
				}
				//else if ( g_aDebuffConditions[i] == TF_COND_BLEEDING )
				//{
				//	// Reduce the duration of this bleeding 
				//	FOR_EACH_VEC( m_PlayerBleeds, i )
				//	{
				//		m_PlayerBleeds[i].flBleedingRemoveTime -= flReduction;
				//	}
				//}
			}
		}
#endif
	} 
	else
	{
		m_flCloakMeter += gpGlobals->frametime * m_fCloakRegenRate;

		if ( m_flCloakMeter >= 100.0f )
		{
			m_flCloakMeter = 100.0f;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTraceFilterIgnoreTeammatesAndTeamObjects::ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
{
	CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );

	if ( pEntity->GetTeamNumber() == m_iIgnoreTeam )
	{
		return false;
	}

	CTFPlayer *pPlayer = dynamic_cast<CTFPlayer*>( pEntity );
	if ( pPlayer )
	{
		if ( pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) && pPlayer->m_Shared.GetDisguiseTeam() == m_iIgnoreTeam )
			return false;

		if ( pPlayer->m_Shared.IsStealthed() )
			return false;
	}

	return BaseClass::ShouldHitEntity( pServerEntity, contentsMask );
}

void CTFPlayerShared::SetCarriedObject( CBaseObject* pObj )
{
	m_bCarryingObject = (pObj != NULL);
	m_hCarriedObject.Set( pObj ); 
#ifdef GAME_DLL
	if ( m_pOuter )
		m_pOuter->TeamFortress_SetSpeed(); 
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ItemPostFrame()
{
	if ( m_hOffHandWeapon.Get() && m_hOffHandWeapon->IsWeaponVisible() )
	{
		if ( gpGlobals->curtime < m_flNextAttack )
		{
			m_hOffHandWeapon->ItemBusyFrame();
		}
		else
		{
#if defined( CLIENT_DLL )
			// Not predicting this weapon
			if ( m_hOffHandWeapon->IsPredicted() )
#endif
			{
				m_hOffHandWeapon->ItemPostFrame( );
			}
		}
	}

	BaseClass::ItemPostFrame();
}

void CTFPlayer::SetOffHandWeapon( CTFWeaponBase *pWeapon )
{
	m_hOffHandWeapon = pWeapon;
	if ( m_hOffHandWeapon.Get() )
	{
		m_hOffHandWeapon->Deploy();
	}
}

// Set to NULL at the end of the holster?
void CTFPlayer::HolsterOffHandWeapon( void )
{
	if ( m_hOffHandWeapon.Get() )
	{
		m_hOffHandWeapon->Holster();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we should record our last weapon when switching between the two specified weapons
//-----------------------------------------------------------------------------
bool CTFPlayer::Weapon_ShouldSetLast( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon )
{
	// if the weapon doesn't want to be auto-switched to, don't!	
	CTFWeaponBase *pTFWeapon = dynamic_cast< CTFWeaponBase * >( pOldWeapon );
	
	if ( pTFWeapon && pTFWeapon->AllowsAutoSwitchTo() == false )
	{
		return false;
	}

	return BaseClass::Weapon_ShouldSetLast( pOldWeapon, pNewWeapon );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayer::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex )
{
	// Ghosts cant switch weapons!
	if ( m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) )
	{
		return false;
	}

	// set last weapon before we switch to a new weapon to make sure that we can get the correct last weapon in Deploy/Holster
	// This should be done in CBasePlayer::Weapon_Switch, but we don't want to break other games
	CBaseCombatWeapon *pPreviousLastWeapon = GetLastWeapon();
	CBaseCombatWeapon *pPreviousActiveWeapon = GetActiveWeapon();

	// always set last for Weapon_Switch code to get attribute from the correct last item
	Weapon_SetLast( GetActiveWeapon() );

	bool bSwitched = BaseClass::Weapon_Switch( pWeapon, viewmodelindex );

	if ( bSwitched )
	{
		GetGameAnimStateType()->ResetGestureSlot( GESTURE_SLOT_ATTACK_AND_RELOAD );

		// valid last weapon
		if ( Weapon_ShouldSetLast( pPreviousActiveWeapon, pWeapon ) )
		{
			Weapon_SetLast( pPreviousActiveWeapon );
			SetSecondaryLastWeapon( pPreviousLastWeapon );
		}
		// previous active weapon is not valid to be last weapon, but the new active weapon is
		else if ( Weapon_ShouldSetLast( pWeapon, pPreviousLastWeapon ) )
		{
			if ( pWeapon != GetSecondaryLastWeapon() )
			{
				Weapon_SetLast( GetSecondaryLastWeapon() );
				SetSecondaryLastWeapon( pPreviousLastWeapon );
			}
			else
			{
				// new active weapon is the same as the secondary last weapon, leave the last weapon alone
				Weapon_SetLast( pPreviousLastWeapon );
			}
		}
		// both previous and new active weapons are not not valid for last weapon
		else
		{
			Weapon_SetLast( pPreviousLastWeapon );
		}
	}
	else
	{
		// restore to the previous last weapon if we failed to switch to a new weapon
		Weapon_SetLast( pPreviousLastWeapon );
	}

#ifdef CLIENT_DLL
	m_Shared.UpdateCritBoostEffect();
#endif

	return bSwitched;
}

//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayer::GetEntityForLoadoutSlot( int iLoadoutSlot )
{
//	CBaseEntity *pEntity = NULL;

//	int iClass = GetPlayerClass()->GetClassIndex();
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( GetWeapon(i) )
		{
			//CEconItemView *pEconItemView = GetWeapon(i)->GetAttributeContainer()->GetItem();
			//if ( !pEconItemView )
			//	continue;

			//CTFItemDefinition *pItemDef = pEconItemView->GetStaticData();
			//if ( !pItemDef )
			//	continue;

			//if ( pItemDef->GetLoadoutSlot( iClass ) == iLoadoutSlot )
			//	return GetWeapon(i);

			// We use weapon types to determine loadout slots
			CTFWeaponBase *pWeapon = dynamic_cast< CTFWeaponBase* >( GetWeapon( i ) );
			if ( pWeapon->GetPCWpnData().m_ExtraTFWeaponData.m_iWeaponType == iLoadoutSlot )
				return GetWeapon(i);
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::GetStepSoundVelocities( float *velwalk, float *velrun )
{
	float flMaxSpeed = MaxSpeed();

	if ( ( GetFlags() & FL_DUCKING ) || ( GetMoveType() == MOVETYPE_LADDER ) )
	{
		if ( m_Shared.IsLoser() )
		{
			*velwalk = 0;
			*velrun = 0;		
		}
		else
		{
			*velwalk = flMaxSpeed * 0.25;
			*velrun = flMaxSpeed * 0.3;		
		}	
	}
	else
	{
		*velwalk = flMaxSpeed * 0.3;
		*velrun = flMaxSpeed * 0.8;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SetStepSoundTime( stepsoundtimes_t iStepSoundTime, bool bWalking )
{
	float flMaxSpeed = MaxSpeed();

	switch ( iStepSoundTime )
	{
	case STEPSOUNDTIME_NORMAL:
	case STEPSOUNDTIME_WATER_FOOT:
		m_flStepSoundTime = RemapValClamped( flMaxSpeed, 200, 450, 400, 200 );
		if ( bWalking )
		{
			m_flStepSoundTime += 100;
		}
		break;

	case STEPSOUNDTIME_ON_LADDER:
		m_flStepSoundTime = 350;
		break;

	case STEPSOUNDTIME_WATER_KNEE:
		m_flStepSoundTime = RemapValClamped( flMaxSpeed, 200, 450, 600, 400 );
		break;

	default:
		Assert(0);
		break;
	}

	if ( ( GetFlags() & FL_DUCKING) || ( GetMoveType() == MOVETYPE_LADDER ) )
	{
		m_flStepSoundTime += 100;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::CanAttack( int iCanAttackFlags )
{
	CTFGameRules *pRules = TFGameRules();

	Assert( pRules );

	if ( ( m_Shared.GetStealthNoAttackExpireTime() > gpGlobals->curtime && !m_Shared.InCond( TF_COND_STEALTHED_USER_BUFF ) ) || m_Shared.InCond( TF_COND_STEALTHED ) )
	{
		if ( !( iCanAttackFlags & TF_CAN_ATTACK_FLAG_GRAPPLINGHOOK ) )
		{
#ifdef CLIENT_DLL
			HintMessage( HINT_CANNOT_ATTACK_WHILE_CLOAKED, true, true );
#endif
			return false;
		}
	}

	if ( IsTaunting() && !( iCanAttackFlags & TF_CAN_ATTACK_FLAG_TAUNT ) )
		return false;

	if ( m_Shared.InCond( TF_COND_PHASE ) == true )
		return false;

	if ( ( pRules->State_Get() == GR_STATE_TEAM_WIN ) && ( pRules->GetWinningTeam() != GetTeamNumber() ) )
	{
		return false;
	}

	if ( m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
	{
		return false;
	}

	// DOD
	if ( IsDODClass() )
	{
		if ( IsSprinting() ) 
			return false;

		if ( GetMoveType() == MOVETYPE_LADDER )
			return false;

		if ( m_Shared.IsJumping() )
			return false;
	}

	// SEALTODO
	//if ( m_Shared.IsDefusing() )
	//	return false;

	// cannot attack while prone moving. except if you have a bazooka
	if ( m_Shared.IsProne() && GetAbsVelocity().LengthSqr() > 1 )
	{
		return false;
	}

	if( m_Shared.IsGoingProne() || m_Shared.IsGettingUpFromProne() )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Weapons can call this on secondary attack and it will link to the class
// ability
//-----------------------------------------------------------------------------
bool CTFPlayer::DoClassSpecialSkill( void )
{
	if ( !IsAlive() )
		return false;

	if ( m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
		return false;

	bool bDoSkill = false;

	switch( GetPlayerClass()->GetClassIndex() )
	{
	case TF_CLASS_SPY:
		{
			if ( !m_Shared.InCond( TF_COND_TAUNTING ) )
			{
				if ( m_Shared.m_flStealthNextChangeTime <= gpGlobals->curtime )
				{
					// Feign death if we have the right equipment mod.
					CTFWeaponInvis* pInvisWatch = static_cast<CTFWeaponInvis*>( Weapon_OwnsThisID( TF_WEAPON_INVIS ) );
					if ( pInvisWatch )
					{
						pInvisWatch->ActivateInvisibilityWatch();
					}
				}
			}
		}
		break;

	case TF_CLASS_DEMOMAN:
		{
			CTFPipebombLauncher *pPipebombLauncher = static_cast<CTFPipebombLauncher*>( Weapon_OwnsThisID( TF_WEAPON_PIPEBOMBLAUNCHER ) );

			if ( pPipebombLauncher )
			{
				pPipebombLauncher->SecondaryAttack();
			}
		}
		bDoSkill = true;
		break;
	case TF_CLASS_ENGINEER:
//		if ( !m_Shared.HasPasstimeBall() )
		{
			bDoSkill = TryToPickupBuilding();
		}
		break;
	default:
		break;
	}

	return bDoSkill;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::CanPickupBuilding( CBaseObject *pPickupObject )
{
	if ( !pPickupObject )
		return false;

	if ( pPickupObject->IsBuilding() )
		return false;

	if ( pPickupObject->IsUpgrading() )
		return false;

	if ( pPickupObject->HasSapper() )
		return false;

	if ( pPickupObject->IsPlasmaDisabled() )
		return false;

	// If we were recently carried & placed we may still be upgrading up to our old level.
	if ( pPickupObject->GetUpgradeLevel() != pPickupObject->GetHighestUpgradeLevel() )
		return false;

	if ( m_Shared.IsCarryingObject() )
		return false;

	if ( /*m_Shared.IsLoserStateStunned() ||*/ m_Shared.IsControlStunned())
		return false;

	if ( m_Shared.IsLoser() )
		return false;

	if ( TFGameRules()->State_Get() != GR_STATE_RND_RUNNING && TFGameRules()->State_Get() != GR_STATE_STALEMATE && TFGameRules()->State_Get() != GR_STATE_BETWEEN_RNDS )
		return false;

	// don't allow to pick up building while grappling hook
	if ( m_Shared.InCond( TF_COND_GRAPPLINGHOOK ) )
		return false;

	// There's ammo in the clip... no switching away!
	if ( GetActivePCWeapon() && GetActivePCWeapon()->AutoFiresFullClip() && GetActivePCWeapon()->Clip1() > 0 )
		return false;

	if ( !tf_building_hauling.GetBool() )
		return false;

	// Check it's within range
	int nPickUpRangeSq = TF_BUILDING_PICKUP_RANGE * TF_BUILDING_PICKUP_RANGE;
	int iIncreasedRangeCost = 0;
	int nSqrDist = (EyePosition() - pPickupObject->GetAbsOrigin()).LengthSqr();

	// Extra range only works with primary weapon
//	CPCWeaponBase * pWeapon = GetActivePCWeapon();
	if ( iIncreasedRangeCost != 0 )
	{
		// False on deadzone
		if ( nSqrDist > nPickUpRangeSq && nSqrDist < TF_BUILDING_RESCUE_MIN_RANGE_SQ )
			return false;
		if ( nSqrDist >= TF_BUILDING_RESCUE_MIN_RANGE_SQ && GetAmmoCount( TF_AMMO_METAL ) < iIncreasedRangeCost )
			return false;
		return true;
	}
	else if ( nSqrDist > nPickUpRangeSq )
		return false;

	if ( TFGameRules()->IsInTraining() )
	{
		ConVarRef training_can_pickup_sentry( "training_can_pickup_sentry" );
		ConVarRef training_can_pickup_dispenser( "training_can_pickup_dispenser" );
		ConVarRef training_can_pickup_tele_entrance( "training_can_pickup_tele_entrance" );
		ConVarRef training_can_pickup_tele_exit( "training_can_pickup_tele_exit" );
		switch ( pPickupObject->GetType() )
		{
		case OBJ_DISPENSER:
			return training_can_pickup_dispenser.GetBool();
		case OBJ_TELEPORTER:
			return pPickupObject->GetObjectMode() == MODE_TELEPORTER_ENTRANCE ? training_can_pickup_tele_entrance.GetBool() : training_can_pickup_tele_exit.GetBool();
		case OBJ_SENTRYGUN:
			return training_can_pickup_sentry.GetBool();
		} // switch
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::TryToPickupBuilding()
{
	if ( m_Shared.IsCarryingObject() )
		return false;

	if ( m_Shared.InCond( TF_COND_TAUNTING ) )
		return false;

	if ( m_Shared.InCond( TF_COND_HALLOWEEN_TINY ) )
		return false;

	if ( m_Shared.InCond( TF_COND_MELEE_ONLY ) )
		return false;

	if ( m_Shared.InCond( TF_COND_SWIMMING_CURSE ) )
		return false;

	if ( !tf_building_hauling.GetBool() )
		return false;

#ifdef GAME_DLL
	int iCannotPickUpBuildings = 0;
	if ( iCannotPickUpBuildings )
	{
		return false;
	}
#endif

	// Check to see if a building we own is in front of us.
	Vector vecForward;
	AngleVectors( EyeAngles(), &vecForward, NULL, NULL );

	int iPickUpRange = TF_BUILDING_PICKUP_RANGE;
	int iIncreasedRangeCost = 0;
//	CPCWeaponBase * pWeapon = GetActivePCWeapon();
	if ( iIncreasedRangeCost != 0 )
	{
		iPickUpRange = TF_BUILDING_RESCUE_MAX_RANGE;
	}
	
	// Create a ray a see if any of my objects touch it
	Ray_t ray;
	ray.Init( EyePosition(), EyePosition() + vecForward * iPickUpRange );

	CBulletPenetrateEnum ePickupPenetrate( &ray, this, TF_DMG_CUSTOM_PENETRATE_ALL_PLAYERS, false );
	enginetrace->EnumerateEntities( ray, false, &ePickupPenetrate );

	CBaseObject *pPickupObject = NULL;
	float flCurrDistanceSq = iPickUpRange * iPickUpRange;

	for ( int i=0; i<GetObjectCount(); i++ )
	{
		CBaseObject	*pObj = GetObject(i);
		if ( !pObj )
			continue;

		float flDistToObjSq = ( pObj->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
		if ( flDistToObjSq > flCurrDistanceSq )
			continue;		

		FOR_EACH_VEC( ePickupPenetrate.m_Targets, iTarget )
		{
			if ( ePickupPenetrate.m_Targets[iTarget].pTarget == pObj )
			{
				CTargetOnlyFilter penetrateFilter( this, pObj );
				trace_t pTraceToUse;
				UTIL_TraceLine( EyePosition(), EyePosition() + vecForward * iPickUpRange, ( MASK_SOLID | CONTENTS_HITBOX ), &penetrateFilter, &pTraceToUse );
				if ( pTraceToUse.m_pEnt == pObj )
				{
					pPickupObject = pObj;
					flCurrDistanceSq = flDistToObjSq;
					break;
				}
			}
			if ( ePickupPenetrate.m_Targets[iTarget].pTarget->IsWorld() )
			{
				break;
			}
		}
	}

	if ( !CanPickupBuilding(pPickupObject) )
	{
		if ( pPickupObject )
		{
			CSingleUserRecipientFilter filter( this );
			EmitSound( filter, entindex(), "Player.UseDeny", NULL, 0.0f );
		}

		return false;
	}

#ifdef CLIENT_DLL

	return (bool) pPickupObject;

#elif GAME_DLL

	if ( pPickupObject )
	{
		// remove rage for long range
		if ( iIncreasedRangeCost )
		{
			int nSqrDist = (EyePosition() - pPickupObject->GetAbsOrigin()).LengthSqr();
			if ( nSqrDist > TF_BUILDING_RESCUE_MIN_RANGE_SQ )
			{
				RemoveAmmo( iIncreasedRangeCost, TF_AMMO_METAL );

				// Particles
				// Spawn a railgun
				Vector origin = pPickupObject->GetAbsOrigin();
				CPVSFilter filter( origin );

				const char *pRailParticleName = GetTeamNumber() == TF_TEAM_BLUE ? "dxhr_sniper_rail_blue" : "dxhr_sniper_rail_red";
				const char *pTeleParticleName = GetTeamNumber() == TF_TEAM_BLUE ? "teleported_blue" : "teleported_red";

				TE_TFParticleEffect( filter, 0.0, pTeleParticleName, origin, vec3_angle );

				te_tf_particle_effects_control_point_t controlPoint = { PATTACH_WORLDORIGIN, pPickupObject->GetAbsOrigin() + Vector(0,0,32) };
				TE_TFParticleEffectComplex( filter, 0.0f, pRailParticleName, GetAbsOrigin() + Vector(0,0,32), QAngle( 0, 0, 0 ), NULL, &controlPoint );

				// Play Sounds
				pPickupObject->EmitSound( "Building_Teleporter.Send" );
				EmitSound( "Building_Teleporter.Receive" );
			}
		}

		pPickupObject->MakeCarriedObject( this );

		CTFWeaponBuilder *pBuilder = dynamic_cast<CTFWeaponBuilder*>(Weapon_OwnsThisID( TF_WEAPON_BUILDER ));
		if ( pBuilder )
		{
			if ( GetActivePCWeapon() == pBuilder )
				SetActiveWeapon( NULL );

			Weapon_Switch( pBuilder );
			pBuilder->m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		}

		SpeakConceptIfAllowed( MP_CONCEPT_PICKUP_BUILDING, pPickupObject->GetResponseRulesModifier() );

		m_flCommentOnCarrying = gpGlobals->curtime + random->RandomFloat( 6.f, 12.f );
		return true;
	}
	else
	{
		return false;
	}


#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::CanGoInvisible( void )
{
	if ( HasItem() && GetItem()->GetItemID() == TF_ITEM_CAPTURE_FLAG )
	{
		HintMessage( HINT_CANNOT_CLOAK_WITH_FLAG );
		return false;
	}

	CTFGameRules *pRules = TFGameRules();

	Assert( pRules );

	if ( ( pRules->State_Get() == GR_STATE_TEAM_WIN ) && ( pRules->GetWinningTeam() != GetTeamNumber() ) )
	{
		return false;
	}

	return true;
}

//ConVar testclassviewheight( "testclassviewheight", "0", FCVAR_DEVELOPMENTONLY );
//Vector vecTestViewHeight(0,0,0);

//-----------------------------------------------------------------------------
// Purpose: Return class-specific standing eye height
//-----------------------------------------------------------------------------
const Vector& CTFPlayer::GetClassEyeHeight( void )
{
	CTFPlayerClass *pClass = GetPlayerClass();

	if ( !pClass )
		return VEC_VIEW;

	//if ( testclassviewheight.GetFloat() > 0 )
	//{
	//	vecTestViewHeight.z = test.GetFloat();
	//	return vecTestViewHeight;
	//}

	int iClassIndex = pClass->GetClassIndex();

	// SEALTODO Check this
	if ( iClassIndex < TF_FIRST_NORMAL_CLASS || iClassIndex > TF_LAST_NORMAL_CLASS )
		return VEC_VIEW;

	return g_TFClassViewVectors[pClass->GetClassIndex()];
}

//-----------------------------------------------------------------------------
// Purpose: Remove disguise
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveDisguise( void )
{
	// remove quickly
	if ( m_Shared.InCond( TF_COND_DISGUISED ) || m_Shared.InCond( TF_COND_DISGUISING ) )
	{
		m_Shared.RemoveDisguise();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::RemoveDisguiseWeapon( void )
{
#ifdef GAME_DLL
	if ( m_hDisguiseWeapon )
	{
		m_hDisguiseWeapon->Drop( Vector(0,0,0) );
		m_hDisguiseWeapon = NULL;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::CanDisguise( void )
{
	if ( !IsAlive() )
		return false;

	if ( GetPlayerClass()->GetClassIndex() != TF_CLASS_SPY )
		return false;

	if ( HasItem() && GetItem()->GetItemID() == TF_ITEM_CAPTURE_FLAG )
	{
		HintMessage( HINT_CANNOT_DISGUISE_WITH_FLAG );
		return false;
	}

	if ( !Weapon_GetWeaponByType( TF_WPN_TYPE_PDA ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayer::MedicGetHealTarget( void )
{
	if ( IsPlayerClass( TF_CLASS_MEDIC ) )
	{
		CWeaponMedigun *pWeapon = dynamic_cast <CWeaponMedigun*>( GetActiveWeapon() );

		if ( pWeapon )
			return pWeapon->GetHealTarget();
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayer::MedicGetChargeLevel( CTFWeaponBase **pRetMedigun )
{
	if ( IsPlayerClass( TF_CLASS_MEDIC ) )
	{
		CTFWeaponBase *pWpn = ( CTFWeaponBase *)Weapon_OwnsThisID( TF_WEAPON_MEDIGUN );

		if ( pWpn == NULL )
			return 0;

		CWeaponMedigun *pMedigun = dynamic_cast <CWeaponMedigun*>( pWpn );

		if ( pRetMedigun )
		{
			*pRetMedigun = pMedigun;
		}

		if ( pMedigun )
			return pMedigun->GetChargeLevel();
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayer::GetNumActivePipebombs( void )
{
	if ( IsPlayerClass( TF_CLASS_DEMOMAN ) )
	{
		CTFPipebombLauncher *pWeapon = dynamic_cast < CTFPipebombLauncher*>( Weapon_OwnsThisID( TF_WEAPON_PIPEBOMBLAUNCHER ) );

		if ( pWeapon )
		{
			return pWeapon->GetPipeBombCount();
		}
	}

	return 0;
}

CTFWeaponBase *CTFPlayer::Weapon_OwnsThisID( int iWeaponID )
{
	for (int i = 0;i < WeaponCount(); i++) 
	{
		CTFWeaponBase *pWpn = ( CTFWeaponBase *)GetWeapon( i );

		if ( pWpn == NULL )
			continue;

		if ( pWpn->GetWeaponID() == iWeaponID )
		{
			return pWpn;
		}
	}

	return NULL;
}

CTFWeaponBase *CTFPlayer::Weapon_GetWeaponByType( int iType )
{
	for (int i = 0;i < WeaponCount(); i++) 
	{
		CTFWeaponBase *pWpn = ( CTFWeaponBase *)GetWeapon( i );

		if ( pWpn == NULL )
			continue;

		int iWeaponRole = pWpn->GetPCWpnData().m_ExtraTFWeaponData.m_iWeaponType;

		if ( iWeaponRole == iType )
		{
			return pWpn;
		}
	}

	return NULL;

}

bool CTFPlayer::Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon )
{
	switch ( GetClassType() )
	{
		case FACTION_DOD:
			return Weapon_CanSwitchTo_DOD( pWeapon );
		case FACTION_CS:
			return Weapon_CanSwitchTo_CS( pWeapon );
		default:
			return Weapon_CanSwitchTo_TF( pWeapon );
	}
}

bool CTFPlayer::Weapon_CanSwitchTo_TF( CBaseCombatWeapon *pWeapon )
{
	// TF behavior
	bool bCanSwitch = BaseClass::Weapon_CanSwitchTo( pWeapon );

	if ( bCanSwitch )
	{
		if ( GetActivePCWeapon() )
		{
			// There's ammo in the clip while auto firing... no switching away!
			if ( GetActivePCWeapon()->AutoFiresFullClip() && GetActivePCWeapon()->Clip1() > 0 )
				return false;
		}

		if ( m_Shared.IsCarryingObject() && (GetPlayerClass()->GetClassIndex() == TF_CLASS_ENGINEER) )
		{
			CTFWeaponBase *pTFWeapon = dynamic_cast<CTFWeaponBase*>( pWeapon );
			if ( pTFWeapon && (pTFWeapon->GetWeaponID() != TF_WEAPON_BUILDER) )
			{
				return false;
			}
		}

		// prevents script exploits, like switching to the minigun while eating a sandvich
		if ( IsTaunting() && tf_allow_taunt_switch.GetInt() == 0 )
		{
			return false;
		}
	}

	return bCanSwitch;
}

bool CTFPlayer::Weapon_CanSwitchTo_DOD( CBaseCombatWeapon *pWeapon )
{
#if !defined( CLIENT_DLL )
	IServerVehicle *pVehicle = GetVehicle();
#else
	IClientVehicle *pVehicle = GetVehicle();
#endif	

	if ( pVehicle && !UsingStandardWeaponsInVehicle() )
		return false;

	if ( !pWeapon->CanDeploy() )
		return false;
	
	if ( GetActiveWeapon() )
	{
		// Seal: This check doesn't actually work in DOD...
		if ( !GetActiveWeapon()->CanHolster() )
			return false;

		// ...However we still don't allow players to holster primed grenades
		CWeaponDODBaseGrenade *pGrenade = dynamic_cast<CWeaponDODBaseGrenade*>( GetActiveWeapon() );
		if ( pGrenade && !pGrenade->CanHolster() )
			return false;
	}

	return true;
}

bool CTFPlayer::Weapon_CanSwitchTo_CS( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon->CanDeploy() )
		return false;

	if ( GetActiveWeapon() )
	{
		if ( !GetActiveWeapon()->CanHolster() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Gives the player an opportunity to abort a double jump.
//-----------------------------------------------------------------------------
bool CTFPlayer::CanAirDash( void ) const
{	
	if ( m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
		return false;

	if ( m_Shared.InCond( TF_COND_HALLOWEEN_SPEED_BOOST ) )
		return true;

	if ( m_Shared.InCond( TF_COND_SODAPOPPER_HYPE ) )
	{
		if ( m_Shared.GetAirDash() < 5 )
			return true;
		else
 			return false;
	}

	int iDashCount = (IsPlayerClass(TF_CLASS_SCOUT)) ? tf_scout_air_dash_count.GetInt() : 0;

	if ( m_Shared.GetAirDash() >= iDashCount ) 
		return false;

	int iNoAirDash = 0;
	//CALL_ATTRIB_HOOK_INT( iNoAirDash, set_scout_doublejump_disabled );
	if ( 1 == iNoAirDash )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFWeaponBuilder *CTFPlayerSharedUtils::GetBuilderForObjectType( CTFPlayer *pTFPlayer, int iObjectType )
{
	const int OBJ_ANY = -1;

	if ( !pTFPlayer )
		return NULL;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CTFWeaponBuilder *pBuilder = dynamic_cast< CTFWeaponBuilder* >( pTFPlayer->GetWeapon( i ) );
		if ( !pBuilder )
			continue;

		// Any builder will do - return first
		if ( iObjectType == OBJ_ANY )
			return pBuilder;

		// Requires a specific builder for this type
		if ( pBuilder->CanBuildObjectType( iObjectType ) )
			return pBuilder;
	}

	return NULL;
}

//////////////////////////////// Day Of Defeat ////////////////////////////////
void CTFPlayer::SharedSpawn()
{	
	BaseClass::SharedSpawn();

	m_flMinNextStepSoundTime = gpGlobals->curtime;

	m_bPlayingProneMoveSound = false;
}

bool CTFPlayerShared::IsDucking( void ) const
{
	return !!( m_pOuter->GetFlags() & FL_DUCKING );
}

bool CTFPlayerShared::IsProne() const
{
	return m_bProne;
}

bool CTFPlayerShared::IsGettingUpFromProne() const
{
	return ( m_flUnProneTime > 0 );
}

bool CTFPlayerShared::IsGoingProne() const
{
	return ( m_flGoProneTime > 0 );
}

void CTFPlayerShared::SetProne( bool bProne, bool bNoAnimation /* = false */ )
{
	m_bProne = bProne;

	if ( bNoAnimation )
	{
		m_flGoProneTime = 0;
		m_flUnProneTime = 0;

		// cancel the view animation!
		m_bForceProneChange = true;
	}

	if ( !bProne /*&& IsSniperZoomed()*/ )	// forceunzoom for going prone is in StartGoingProne
	{
		ForceUnzoom();
	}
}

void CTFPlayerShared::SetSprinting( bool bSprinting )
{
	if ( bSprinting && !m_bIsSprinting )
	{
		StartSprinting();

		// only one penalty per key press
		if ( m_bGaveSprintPenalty == false )
		{
			m_flStamina -= INITIAL_SPRINT_STAMINA_PENALTY;
			m_bGaveSprintPenalty = true;
		}
	}
	else if ( !bSprinting && m_bIsSprinting )
	{
		StopSprinting();
	}
}

bool CTFPlayer::IsSprinting( void )
{
	float flVelSqr = GetAbsVelocity().LengthSqr();

	return m_Shared.m_bIsSprinting && ( flVelSqr > 0.5f );
}

// this is reset when we let go of the sprint key
void CTFPlayerShared::ResetSprintPenalty( void )
{
	m_bGaveSprintPenalty = false;
}

void CTFPlayerShared::StartSprinting( void )
{
	m_bIsSprinting = true;

#ifndef CLIENT_DLL
	m_pOuter->RemoveHintTimer( HINT_USE_SPRINT );
#endif
}

void CTFPlayerShared::StopSprinting( void )
{
	m_bIsSprinting = false;
}

void CTFPlayerShared::ForceUnzoom( void )
{
	CDODSniperWeapon *pWeapon = dynamic_cast<CDODSniperWeapon*>( m_pOuter->GetActivePCWeapon() );
	if( pWeapon && ( pWeapon->GetPCWpnData().m_DODWeaponData.m_WeaponType & WPN_MASK_GUN ) )
		pWeapon->ZoomOut();
}

bool CTFPlayerShared::IsBazookaDeployed( void ) const
{
	CDODBaseRocketWeapon *pWeapon = dynamic_cast<CDODBaseRocketWeapon*>( m_pOuter->GetActivePCWeapon() );
	if( pWeapon && pWeapon->GetPCWpnData().m_DODWeaponData.m_WeaponType == WPN_TYPE_BAZOOKA )
		return pWeapon->IsDeployed() && !pWeapon->m_bInReload;

	return false;
}

bool CTFPlayerShared::IsBazookaOnlyDeployed( void ) const
{
	CDODBaseRocketWeapon *pWeapon = dynamic_cast<CDODBaseRocketWeapon*>( m_pOuter->GetActivePCWeapon() );
	if( pWeapon && pWeapon->GetPCWpnData().m_DODWeaponData.m_WeaponType == WPN_TYPE_BAZOOKA )
		return pWeapon->IsDeployed();

	return false;
}

bool CTFPlayerShared::IsSniperZoomed( void ) const
{
	CWeaponDODBaseGun *pWeapon = dynamic_cast<CWeaponDODBaseGun*>( m_pOuter->GetActivePCWeapon() );
	if( pWeapon && ( pWeapon->GetPCWpnData().m_DODWeaponData.m_WeaponType & WPN_MASK_GUN ) )
		return pWeapon->IsSniperZoomed();

	return false;
}

bool CTFPlayerShared::IsInMGDeploy( void ) const
{
	CDODBipodWeapon *pWeapon = dynamic_cast<CDODBipodWeapon*>( m_pOuter->GetActivePCWeapon() );
	if( pWeapon && pWeapon->GetPCWpnData().m_DODWeaponData.m_WeaponType == WPN_TYPE_MG )
		return pWeapon->IsDeployed();

	return false;
}

bool CTFPlayerShared::IsProneDeployed( void ) const
{
	return ( IsProne() && IsInMGDeploy() );
}

bool CTFPlayerShared::IsSandbagDeployed( void ) const
{
	return ( !IsProne() && IsInMGDeploy() );
}

void CTFPlayerShared::SetStamina( float flStamina )
{
	m_flStamina = clamp( flStamina, 0, 100 );
}

void CTFPlayerShared::SetDeployed( bool bDeployed, float flHeight /* = -1 */ )
{
	if( gpGlobals->curtime - m_flDeployChangeTime < 0.2 )
	{
		Assert(0);
	}

	m_flDeployChangeTime = gpGlobals->curtime;
	m_vecDeployedAngles = m_pOuter->EyeAngles();

	if( flHeight > 0 )
	{
		m_flDeployedHeight = flHeight;
	}
	else
	{
		m_flDeployedHeight = m_pOuter->GetViewOffset()[2];
	}
}

QAngle CTFPlayerShared::GetDeployedAngles( void ) const
{
	return m_vecDeployedAngles;
}

void CTFPlayerShared::SetDeployedYawLimits( float flLeftYaw, float flRightYaw )
{
	m_flDeployedYawLimitLeft = flLeftYaw;
	m_flDeployedYawLimitRight = -flRightYaw;

	m_vecDeployedAngles = m_pOuter->EyeAngles();
}

void CTFPlayerShared::ClampDeployedAngles( QAngle *vecTestAngles )
{
	Assert( vecTestAngles );

	// Clamp Pitch
	vecTestAngles->x = clamp( vecTestAngles->x, MAX_DEPLOY_PITCH, MIN_DEPLOY_PITCH );

	// Clamp Yaw - do a bit more work as yaw will wrap around and cause problems
	float flDeployedYawCenter = GetDeployedAngles().y;

	float flDelta = AngleNormalize( vecTestAngles->y - flDeployedYawCenter );

	if( flDelta < m_flDeployedYawLimitRight )
	{
		vecTestAngles->y = flDeployedYawCenter + m_flDeployedYawLimitRight;
	}
	else if( flDelta > m_flDeployedYawLimitLeft )
	{
		vecTestAngles->y = flDeployedYawCenter + m_flDeployedYawLimitLeft;
	}

	/*
	Msg( "delta %.1f ( left %.1f, right %.1f ) ( %.1f -> %.1f )\n",
		flDelta,
		flDeployedYawCenter + m_flDeployedYawLimitLeft,
		flDeployedYawCenter + m_flDeployedYawLimitRight,
		before,
		vecTestAngles->y );
		*/

}

float CTFPlayerShared::GetDeployedHeight( void ) const
{
	return m_flDeployedHeight;
}

float CTFPlayerShared::GetSlowedTime( void ) const
{
	return m_flSlowedUntilTime;
}

void CTFPlayerShared::SetSlowedTime( float t )
{
	m_flSlowedUntilTime = gpGlobals->curtime + t;
}

void CTFPlayerShared::StartGoingProne( void )
{
	// make the prone sound
	CPASFilter filter( m_pOuter->GetAbsOrigin() );
	filter.UsePredictionRules();
	m_pOuter->EmitSound( filter, m_pOuter->entindex(), "DODPlayer.GoProne" );

	// slow to prone speed
	m_flGoProneTime = gpGlobals->curtime + TIME_TO_PRONE;

	m_flUnProneTime = 0.0f;	//reset

	if ( IsSniperZoomed() )
		ForceUnzoom();
}

void CTFPlayerShared::StandUpFromProne( void )
{	
	// make the prone sound
	CPASFilter filter( m_pOuter->GetAbsOrigin() );
	filter.UsePredictionRules();
	m_pOuter->EmitSound( filter, m_pOuter->entindex(), "DODPlayer.UnProne" );

	// speed up to target speed
	m_flUnProneTime = gpGlobals->curtime + TIME_TO_PRONE;

	m_flGoProneTime = 0.0f;	//reset 
}

bool CTFPlayerShared::CanChangePosition( void )
{
	if ( IsInMGDeploy() )
		return false;

	if ( IsGettingUpFromProne() )
		return false;

	if ( IsGoingProne() )
		return false;

	return true;
}

void CTFPlayer::UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity )
{
	if ( !IsDODClass() )
		return BaseClass::UpdateStepSound( psurface, vecOrigin, vecVelocity );

	Vector knee;
	Vector feet;
	float height;
	int	fLadder;

	if ( m_flStepSoundTime > 0 )
	{
		m_flStepSoundTime -= 1000.0f * gpGlobals->frametime;
		if ( m_flStepSoundTime < 0 )
		{
			m_flStepSoundTime = 0;
		}
	}

	if ( m_flStepSoundTime > 0 )
		return;

	if ( GetFlags() & (FL_FROZEN|FL_ATCONTROLS))
		return;

	if ( GetMoveType() == MOVETYPE_NOCLIP || GetMoveType() == MOVETYPE_OBSERVER )
		return;

	if ( !sv_footsteps.GetFloat() )
		return;

	float speed = VectorLength( vecVelocity );
	float groundspeed = Vector2DLength( vecVelocity.AsVector2D() );

	// determine if we are on a ladder
	fLadder = ( GetMoveType() == MOVETYPE_LADDER );

	float flDuck;

	if ( ( GetFlags() & FL_DUCKING) || fLadder )
	{
		flDuck = 100;
	}
	else
	{
		flDuck = 0;
	}

	static float flMinProneSpeed = 10.0f;
	static float flMinSpeed = 70.0f;
	static float flRunSpeed = 110.0f;

	bool onground = ( GetFlags() & FL_ONGROUND );
	bool movingalongground = ( groundspeed > 0.0f );
	bool moving_fast_enough =  ( speed >= flMinSpeed );

	// always play a step sound if we are moving faster than 

	// To hear step sounds you must be either on a ladder or moving along the ground AND
	// You must be moving fast enough

	CheckProneMoveSound( groundspeed, onground );

	if ( !moving_fast_enough || !(fLadder || ( onground && movingalongground )) )
	{
		return;
	}

	bool bWalking = ( speed < flRunSpeed );		// or ducking!

	VectorCopy( vecOrigin, knee );
	VectorCopy( vecOrigin, feet );

	height = GetPlayerMaxs()[ 2 ] - GetPlayerMins()[ 2 ];

	knee[2] = vecOrigin[2] + 0.2 * height;

	float flVol;

	// find out what we're stepping in or on...
	if ( fLadder )
	{
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "ladder" ) );
		flVol = 1.0;
		m_flStepSoundTime = 350;
	}
	else if ( enginetrace->GetPointContents( knee ) & MASK_WATER )
	{
		static int iSkipStep = 0;

		if ( iSkipStep == 0 )
		{
			iSkipStep++;
			return;
		}

		if ( iSkipStep++ == 3 )
		{
			iSkipStep = 0;
		}
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "wade" ) );
		flVol = 0.65;
		m_flStepSoundTime = 600;
	}
	else if ( enginetrace->GetPointContents( feet ) & MASK_WATER )
	{
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "water" ) );
		flVol = bWalking ? 0.2 : 0.5;
		m_flStepSoundTime = bWalking ? 400 : 300;		
	}
	else
	{
		if ( !psurface )
			return;

		if ( bWalking )
		{
			m_flStepSoundTime = 400;
		}
		else
		{
			if ( speed > 200 )
			{
				int speeddiff = PLAYER_SPEED_SPRINT - PLAYER_SPEED_RUN;
				int diff = speed - PLAYER_SPEED_RUN;

				float percent = (float)diff / (float)speeddiff;

				m_flStepSoundTime = 300.0f - 30.0f * percent;
			}
			else 
			{
				m_flStepSoundTime = 400;
			}
		}

		switch ( psurface->game.material )
		{
		default:
		case CHAR_TEX_CONCRETE:						
			flVol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_METAL:	
			flVol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_DIRT:
			flVol = bWalking ? 0.25 : 0.55;
			break;

		case CHAR_TEX_VENT:	
			flVol = bWalking ? 0.4 : 0.7;
			break;

		case CHAR_TEX_GRATE:
			flVol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_TILE:	
			flVol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_SLOSH:
			flVol = bWalking ? 0.2 : 0.5;
			break;
		}
	}

	m_flStepSoundTime += flDuck; // slower step time if ducking

	if ( GetFlags() & FL_DUCKING )
	{
		flVol *= 0.65;
	}

	// protect us from prediction errors a little bit
	if ( m_flMinNextStepSoundTime > gpGlobals->curtime )
	{
		return;
	}

	m_flMinNextStepSoundTime = gpGlobals->curtime + 0.1f;	

	PlayStepSound( feet, psurface, flVol, false );
}

void CTFPlayer::CheckProneMoveSound( int groundspeed, bool onground )
{
#ifdef CLIENT_DLL
	bool bShouldPlay = (groundspeed > 10) && (onground == true) && m_Shared.IsProne() && IsAlive();

	if ( m_bPlayingProneMoveSound && !bShouldPlay )
	{
		StopSound( "DODPlayer.MoveProne" );
		m_bPlayingProneMoveSound= false;
	}
	else if ( !m_bPlayingProneMoveSound && bShouldPlay )
	{
		CRecipientFilter filter;
		filter.AddRecipientsByPAS( WorldSpaceCenter() );
		EmitSound( filter, entindex(), "DODPlayer.MoveProne" );

		m_bPlayingProneMoveSound = true;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : step - 
//			fvol - 
//			force - force sound to play
//-----------------------------------------------------------------------------
void CTFPlayer::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force )
{
	if ( !IsDODClass() )
		return BaseClass::PlayStepSound( vecOrigin, psurface, fvol, force );

	if ( gpGlobals->maxClients > 1 && !sv_footsteps.GetFloat() )
		return;

#if defined( CLIENT_DLL )
	// during prediction play footstep sounds only once
	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;
#endif

	if ( !psurface )
		return;

	unsigned short stepSoundName = m_Local.m_nStepside ? psurface->sounds.stepleft : psurface->sounds.stepright;
	m_Local.m_nStepside = !m_Local.m_nStepside;

	if ( !stepSoundName )
		return;

	IPhysicsSurfaceProps *physprops = MoveHelper( )->GetSurfaceProps();
	const char *pSoundName = physprops->GetString( stepSoundName );
	CSoundParameters params;

	// we don't always know the model, so go by team
	char *pModelNameForGender = DOD_PLAYERMODEL_AXIS_RIFLEMAN;

	if( GetTeamNumber() == TF_TEAM_RED )
		pModelNameForGender = DOD_PLAYERMODEL_US_RIFLEMAN;

	if ( !CBaseEntity::GetParametersForSound( pSoundName, params, pModelNameForGender ) )
		return;	

	CRecipientFilter filter;
	filter.AddRecipientsByPAS( vecOrigin );

#ifndef CLIENT_DLL
	// im MP, server removed all players in origins PVS, these players 
	// generate the footsteps clientside
	if ( gpGlobals->maxClients > 1 )
		filter.RemoveRecipientsByPVS( vecOrigin );
#endif

	EmitSound_t ep;
	ep.m_nChannel = params.channel;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = fvol;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;

	EmitSound( filter, entindex(), ep );
}

Activity CTFPlayer::TranslateActivity( Activity baseAct, bool *pRequired /* = NULL */ )
{
	// SEALTODO: this was commented out, cant remember why
	//if ( !IsDODClass() )
	//	return BaseClass::TranslateActivity( baseAct, pRequired );

	Activity translated = baseAct;

	if ( GetActiveWeapon() )
	{
		translated = GetActiveWeapon()->ActivityOverride( baseAct, pRequired );
	}
	else if (pRequired)
	{
		*pRequired = false;
	}

	return translated;
}

void CTFPlayerShared::SetLastViewAnimTime( float flTime )
{
	m_flLastViewAnimationTime = flTime;
}

float CTFPlayerShared::GetLastViewAnimTime( void )
{
	return m_flLastViewAnimationTime;
}

void CTFPlayerShared::ResetViewOffsetAnimation( void )
{
    if ( m_pViewOffsetAnim )
	{
		//cancel it!
		m_pViewOffsetAnim->Reset();
	}
}

void CTFPlayerShared::ViewOffsetAnimation( Vector vecDest, float flTime, ViewAnimationType type )
{
	if ( !m_pViewOffsetAnim )
	{
		m_pViewOffsetAnim =  CViewOffsetAnimation::CreateViewOffsetAnim( m_pOuter );
	}

	Assert( m_pViewOffsetAnim );

	if ( m_pViewOffsetAnim )
	{
		m_pViewOffsetAnim->StartAnimation( m_pOuter->GetViewOffset(), vecDest, flTime, type );
	}
}

void CTFPlayerShared::ViewAnimThink( void )
{
	// Check for the flag that will reset our view animations
	// when the player respawns
	if ( m_bForceProneChange )
	{
		ResetViewOffsetAnimation();

		if ( m_pOuter->IsDODClass() )
			m_pOuter->SetViewOffset( VEC_VIEW_SCALED_DOD( m_pOuter ) );

		m_bForceProneChange = false;
	}

	if ( m_pViewOffsetAnim )
	{
		m_pViewOffsetAnim->Think();
	}
}

void CTFPlayerShared::ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	Vector org = m_pOuter->GetAbsOrigin();

	if ( IsProne() )
	{
		static Vector vecProneMin(-44, -44, 0 );
		static Vector vecProneMax(44, 44, 24 );

		VectorAdd( vecProneMin, org, *pVecWorldMins );
		VectorAdd( vecProneMax, org, *pVecWorldMaxs );
	}
	else
	{
		static Vector vecMin(-32, -32, 0 );
		static Vector vecMax(32, 32, 72 );

		VectorAdd( vecMin, org, *pVecWorldMins );
		VectorAdd( vecMax, org, *pVecWorldMaxs );
	}
}

CWeaponDODBase*CTFPlayer::DODWeapon_OwnsThisID( int iWeaponID )
{
	for (int i = 0;i < WeaponCount(); i++) 
	{
		CWeaponDODBase *pWpn = ( CWeaponDODBase *)GetWeapon( i );

		if ( pWpn == NULL )
			continue;

		if ( pWpn->GetWeaponID() == iWeaponID )
		{
			return pWpn;
		}
	}

	return NULL;
}

CWeaponDODBase *CTFPlayer::DODWeapon_GetWeaponByType( int iType )
{
	for (int i = 0;i < WeaponCount(); i++) 
	{
		CWeaponDODBase *pWpn = ( CWeaponDODBase *)GetWeapon( i );

		if ( pWpn == NULL )
			continue;

		int iWeaponRole = pWpn->GetPCWpnData().m_DODWeaponData.m_WeaponType;

		if ( iWeaponRole == iType )
		{
			return pWpn;
		}
	}

	return NULL;

}

CWeaponCSBase *CTFPlayer::CSWeapon_OwnsThisID( int iWeaponID )
{
	for (int i = 0;i < WeaponCount(); i++) 
	{
		CWeaponCSBase *pWpn = ( CWeaponCSBase *)GetWeapon( i );

		if ( pWpn == NULL )
			continue;

		if ( pWpn->GetWeaponID() == iWeaponID )
		{
			return pWpn;
		}
	}

	return NULL;
}