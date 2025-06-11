//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: The TF Game rules 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "tf_gamerules.h"
#include "ammodef.h"
#include "KeyValues.h"
#include "tf_weaponbase.h"
#include "time.h"
#include "weapon_dodbase.h"
#include "dod_shareddefs.h"

#include "effect_dispatch_data.h"
#ifdef CLIENT_DLL
	#include <game/client/iviewport.h>
	#include "c_tf_player.h"
	#include "c_tf_objective_resource.h"
	#include <filesystem.h>
	#include "c_tf_team.h"
	#include "dt_utlvector_recv.h"
#else
	#include "basemultiplayerplayer.h"
	#include "voice_gamemgr.h"
	#include "items.h"
	#include "team.h"
	#include "tf_bot_temp.h"
	#include "tf_player.h"
	#include "tf_team.h"
	#include "player_resource.h"
	#include "entity_tfstart.h"
	#include "filesystem.h"
	#include "tf_obj.h"
	#include "tf_objective_resource.h"
	#include "tf_player_resource.h"
	#include "team_control_point_master.h"
	#include "playerclass_info_parse.h"
	#include "team_control_point_master.h"
	#include "coordsize.h"
	#include "entity_healthkit.h"
	#include "tf_gamestats.h"
	#include "entity_capture_flag.h"
	#include "tf_player_resource.h"
	#include "tf_obj_sentrygun.h"
	#include "tier0/icommandline.h"
	#include "activitylist.h"
	#include "AI_ResponseSystem.h"
	#include "hl2orange.spa.h"
	#include "hltvdirector.h"

	#include "tf_weapon_grenade_pipebomb.h"
	#include "tf_weaponbase_melee.h"
	
	#include "dod_baserocket.h"
	#include "dod_basegrenade.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ITEM_RESPAWN_TIME	10.0f
#define MASK_RADIUS_DAMAGE  ( MASK_SHOT & ~( CONTENTS_HITBOX ) )

// Halloween 2013 VO defines for plr_hightower_event
#define HELLTOWER_TIMER_INTERVAL	( 60 + RandomInt( -30, 30 )	)
#define HELLTOWER_RARE_LINE_CHANCE	0.15	// 15%
#define HELLTOWER_MISC_CHANCE		0.50	// 50%

enum
{
	BIRTHDAY_RECALCULATE,
	BIRTHDAY_OFF,
	BIRTHDAY_ON,
};

static int g_TauntCamAchievements[] = 
{
	0,		// TF_CLASS_UNDEFINED

	0,		// TF_CLASS_SCOUT,	
	0,		// TF_CLASS_SNIPER,
	0,		// TF_CLASS_SOLDIER,
	0,		// TF_CLASS_DEMOMAN,
	0,		// TF_CLASS_MEDIC,
	0,		// TF_CLASS_HEAVYWEAPONS,
	0,		// TF_CLASS_PYRO,
	0,		// TF_CLASS_SPY,
	0,		// TF_CLASS_ENGINEER,
};

extern ConVar mp_capstyle;
extern ConVar sv_turbophysics;

#ifdef GAME_DLL
extern ConVar tf_debug_damage;
extern ConVar tf_damage_range;
extern ConVar tf_damage_disablespread;

extern ConVar mp_autoteambalance;

ConVar tf_stealth_damage_reduction( "tf_stealth_damage_reduction", "0.8", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

ConVar tf_weapon_criticals_distance_falloff( "tf_weapon_criticals_distance_falloff", "0", FCVAR_CHEAT, "Critical weapon damage will take distance into account." );
ConVar tf_weapon_minicrits_distance_falloff( "tf_weapon_minicrits_distance_falloff", "0", FCVAR_CHEAT, "Mini-crit weapon damage will take distance into account." );
#endif

ConVar tf_caplinear( "tf_caplinear", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "If set to 1, teams must capture control points linearly." );
ConVar tf_stalematechangeclasstime( "tf_stalematechangeclasstime", "20", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Amount of time that players are allowed to change class in stalemates." );
ConVar tf_birthday( "tf_birthday", "0", FCVAR_NOTIFY | FCVAR_REPLICATED );

ConVar tf_player_movement_restart_freeze( "tf_player_movement_restart_freeze", "1", FCVAR_REPLICATED, "When set, prevent player movement during round restart" );

ConVar tf_sticky_radius_ramp_time( "tf_sticky_radius_ramp_time", "2.0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_REPLICATED, "Amount of time to get full radius after arming" );
ConVar tf_sticky_airdet_radius( "tf_sticky_airdet_radius", "0.85", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_REPLICATED, "Radius Scale if detonated in the air" );

#ifdef GAME_DLL
// TF overrides the default value of this convar
ConVar mp_waitingforplayers_time( "mp_waitingforplayers_time", (IsX360()?"15":"30"), FCVAR_GAMEDLL | FCVAR_DEVELOPMENTONLY, "WaitingForPlayers time length in seconds" );
ConVar tf_gravetalk( "tf_gravetalk", "1", FCVAR_NOTIFY, "Allows living players to hear dead players using text/voice chat." );
ConVar tf_spectalk( "tf_spectalk", "1", FCVAR_NOTIFY, "Allows living players to hear spectators using text chat." );

ConVar tf_ctf_bonus_time ( "tf_ctf_bonus_time", "10", FCVAR_NOTIFY, "Length of team crit time for CTF capture." );
extern ConVar tf_flag_caps_per_round;

void ValidateCapturesPerRound( IConVar *pConVar, const char *oldValue, float flOldValue )
{
	ConVarRef var( pConVar );

	if ( var.GetInt() <= 0 )
	{
		// reset the flag captures being played in the current round
		int nTeamCount = TFTeamMgr()->GetTeamCount();
		for ( int iTeam = FIRST_GAME_TEAM; iTeam < nTeamCount; ++iTeam )
		{
			CTFTeam *pTeam = GetGlobalTFTeam( iTeam );
			if ( !pTeam )
				continue;

			pTeam->SetFlagCaptures( 0 );
		}
	}
}
#endif

ConVar tf_flag_caps_per_round( "tf_flag_caps_per_round", "3", FCVAR_REPLICATED, "Number of captures per round on CTF and PASS Time maps. Set to 0 to disable.", true, 0, false, 0
#ifdef GAME_DLL
							  , ValidateCapturesPerRound
#endif
							  );

ConVar tf_spec_xray( "tf_spec_xray", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Allows spectators to see player glows. 1 = same team, 2 = both teams" );
ConVar tf_spawn_glows_duration( "tf_spawn_glows_duration", "10", FCVAR_NOTIFY | FCVAR_REPLICATED, "How long should teammates glow after respawning\n" );

extern ConVar tf_flag_return_on_touch;
extern ConVar tf_flag_return_time_credit_factor;

/**
 * Player hull & eye position for standing, ducking, etc.  This version has a taller
 * player height, but goldsrc-compatible collision bounds.
 */
static CViewVectors g_TFViewVectors(
	Vector( 0, 0, 72 ),		//VEC_VIEW (m_vView) eye position
							
	Vector(-24, -24, 0 ),	//VEC_HULL_MIN (m_vHullMin) hull min
	Vector( 24,  24, 82 ),	//VEC_HULL_MAX (m_vHullMax) hull max
												
	Vector(-24, -24, 0 ),	//VEC_DUCK_HULL_MIN (m_vDuckHullMin) duck hull min
	Vector( 24,  24, 55 ),	//VEC_DUCK_HULL_MAX	(m_vDuckHullMax) duck hull max
	Vector( 0, 0, 45 ),		//VEC_DUCK_VIEW		(m_vDuckView) duck view
												
	Vector( -10, -10, -10 ),	//VEC_OBS_HULL_MIN	(m_vObsHullMin) observer hull min
	Vector(  10,  10,  10 ),	//VEC_OBS_HULL_MAX	(m_vObsHullMax) observer hull max
												
	Vector( 0, 0, 14 )		//VEC_DEAD_VIEWHEIGHT (m_vDeadViewHeight) dead view height
);							

Vector g_TFClassViewVectors[10] =
{
	Vector( 0, 0, 72 ),		// TF_CLASS_UNDEFINED

	Vector( 0, 0, 65 ),		// TF_CLASS_SCOUT,			// TF_FIRST_NORMAL_CLASS
	Vector( 0, 0, 75 ),		// TF_CLASS_SNIPER,
	Vector( 0, 0, 68 ),		// TF_CLASS_SOLDIER,
	Vector( 0, 0, 68 ),		// TF_CLASS_DEMOMAN,
	Vector( 0, 0, 75 ),		// TF_CLASS_MEDIC,
	Vector( 0, 0, 75 ),		// TF_CLASS_HEAVYWEAPONS,
	Vector( 0, 0, 68 ),		// TF_CLASS_PYRO,
	Vector( 0, 0, 75 ),		// TF_CLASS_SPY,
	Vector( 0, 0, 68 ),		// TF_CLASS_ENGINEER,		// TF_LAST_NORMAL_CLASS
};

static CDODViewVectors g_DODViewVectors(

	Vector( 0, 0, 58 ),			//VEC_VIEW (m_vView) 
								
	Vector(-16, -16, 0 ),		//VEC_HULL_MIN (m_vHullMin)
	Vector( 16,  16,  72 ),		//VEC_HULL_MAX (m_vHullMax)
													
	Vector(-16, -16, 0 ),		//VEC_DUCK_HULL_MIN (m_vDuckHullMin)
	Vector( 16,  16, 45 ),		//VEC_DUCK_HULL_MAX	(m_vDuckHullMax)
	Vector( 0, 0, 34 ),			//VEC_DUCK_VIEW		(m_vDuckView)
													
	Vector(-10, -10, -10 ),		//VEC_OBS_HULL_MIN	(m_vObsHullMin)
	Vector( 10,  10,  10 ),		//VEC_OBS_HULL_MAX	(m_vObsHullMax)
													
	Vector( 0, 0, 14 ),			//VEC_DEAD_VIEWHEIGHT (m_vDeadViewHeight)
								
	Vector(-16, -16, 0 ),		//VEC_PRONE_HULL_MIN (m_vProneHullMin)
	Vector( 16,  16, 24 )		//VEC_PRONE_HULL_MAX (m_vProneHullMax)
);

const CViewVectors *CTFGameRules::GetViewVectors() const
{
	CViewVectors *ViewVector = &g_TFViewVectors;
	return ViewVector;
}

const CDODViewVectors *CTFGameRules::GetDODViewVectors() const
{
	return &g_DODViewVectors;
}

REGISTER_GAMERULES_CLASS( CTFGameRules );

BEGIN_NETWORK_TABLE_NOBASE( CTFGameRules, DT_TFGameRules )
#ifdef CLIENT_DLL

	RecvPropInt( RECVINFO( m_nGameType ) ),
	RecvPropString( RECVINFO( m_pszTeamGoalStringRed ) ),
	RecvPropString( RECVINFO( m_pszTeamGoalStringBlue ) ),

#else

	SendPropInt( SENDINFO( m_nGameType ), 3, SPROP_UNSIGNED ),
	SendPropString( SENDINFO( m_pszTeamGoalStringRed ) ),
	SendPropString( SENDINFO( m_pszTeamGoalStringBlue ) ),

#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_gamerules, CTFGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( TFGameRulesProxy, DT_TFGameRulesProxy )

#ifdef CLIENT_DLL
	void RecvProxy_TFGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CTFGameRules *pRules = TFGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CTFGameRulesProxy, DT_TFGameRulesProxy )
		RecvPropDataTable( "tf_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_TFGameRules ), RecvProxy_TFGameRules )
	END_RECV_TABLE()
#else
	void *SendProxy_TFGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CTFGameRules *pRules = TFGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CTFGameRulesProxy, DT_TFGameRulesProxy )
		SendPropDataTable( "tf_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_TFGameRules ), SendProxy_TFGameRules )
	END_SEND_TABLE()
#endif

#ifdef GAME_DLL
BEGIN_DATADESC( CTFGameRulesProxy )
	// Inputs.
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetRedTeamRespawnWaveTime", InputSetRedTeamRespawnWaveTime ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetBlueTeamRespawnWaveTime", InputSetBlueTeamRespawnWaveTime ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "AddRedTeamRespawnWaveTime", InputAddRedTeamRespawnWaveTime ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "AddBlueTeamRespawnWaveTime", InputAddBlueTeamRespawnWaveTime ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetRedTeamGoalString", InputSetRedTeamGoalString ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetBlueTeamGoalString", InputSetBlueTeamGoalString ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetRedTeamRole", InputSetRedTeamRole ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetBlueTeamRole", InputSetBlueTeamRole ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetRedTeamRespawnWaveTime( inputdata_t &inputdata )
{
	TFGameRules()->SetTeamRespawnWaveTime( TF_TEAM_RED, inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetBlueTeamRespawnWaveTime( inputdata_t &inputdata )
{
	TFGameRules()->SetTeamRespawnWaveTime( TF_TEAM_BLUE, inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputAddRedTeamRespawnWaveTime( inputdata_t &inputdata )
{
	TFGameRules()->AddTeamRespawnWaveTime( TF_TEAM_RED, inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputAddBlueTeamRespawnWaveTime( inputdata_t &inputdata )
{
	TFGameRules()->AddTeamRespawnWaveTime( TF_TEAM_BLUE, inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetRedTeamGoalString( inputdata_t &inputdata )
{
	TFGameRules()->SetTeamGoalString( TF_TEAM_RED, inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetBlueTeamGoalString( inputdata_t &inputdata )
{
	TFGameRules()->SetTeamGoalString( TF_TEAM_BLUE, inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetRedTeamRole( inputdata_t &inputdata )
{
	CTFTeam *pTeam = TFTeamMgr()->GetTeam( TF_TEAM_RED );
	if ( pTeam )
	{
		pTeam->SetRole( inputdata.value.Int() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::InputSetBlueTeamRole( inputdata_t &inputdata )
{
	CTFTeam *pTeam = TFTeamMgr()->GetTeam( TF_TEAM_BLUE );
	if ( pTeam )
	{
		pTeam->SetRole( inputdata.value.Int() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRulesProxy::Activate()
{
	TFGameRules()->Activate();

	BaseClass::Activate();
}
#endif

// (We clamp ammo ourselves elsewhere).
ConVar ammo_max( "ammo_max", "5000", FCVAR_REPLICATED );

#ifndef CLIENT_DLL
ConVar sk_plr_dmg_grenade( "sk_plr_dmg_grenade","0");		// Very lame that the base code needs this defined
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFGameRules::Damage_IsTimeBased( int iDmgType )
{
	// Damage types that are time-based.
	return ( ( iDmgType & ( DMG_PARALYZE | DMG_NERVEGAS | DMG_DROWNRECOVER ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFGameRules::Damage_ShowOnHUD( int iDmgType )
{
	// Damage types that have client HUD art.
	return ( ( iDmgType & ( DMG_DROWN | DMG_BURN | DMG_NERVEGAS | DMG_SHOCK ) ) != 0 );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFGameRules::Damage_ShouldNotBleed( int iDmgType )
{
	// Should always bleed currently.
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFGameRules::Damage_GetTimeBased( void )
{
	int iDamage = ( DMG_PARALYZE | DMG_NERVEGAS | DMG_DROWNRECOVER );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFGameRules::Damage_GetShowOnHud( void )
{
	int iDamage = ( DMG_DROWN | DMG_BURN | DMG_NERVEGAS | DMG_SHOCK );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFGameRules::Damage_GetShouldNotBleed( void )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFGameRules::CTFGameRules()
{
#ifdef GAME_DLL
	// Create teams.
	TFTeamMgr()->Init();

	ResetMapTime();

	// Create the team managers
//	for ( int i = 0; i < ARRAYSIZE( teamnames ); i++ )
//	{
//		CTeam *pTeam = static_cast<CTeam*>(CreateEntityByName( "tf_team" ));
//		pTeam->Init( sTeamNames[i], i );
//
//		g_Teams.AddToTail( pTeam );
//	}

	m_flIntermissionEndTime = 0.0f;
	m_flNextPeriodicThink = 0.0f;

	ListenForGameEvent( "teamplay_point_captured" );
	ListenForGameEvent( "teamplay_capture_blocked" );	
	ListenForGameEvent( "teamplay_round_win" );
	ListenForGameEvent( "teamplay_flag_event" );

	Q_memset( m_vecPlayerPositions,0, sizeof(m_vecPlayerPositions) );

	m_iPrevRoundState = -1;
	m_iCurrentRoundState = -1;
	m_iCurrentMiniRoundMask = 0;
	m_flTimerMayExpireAt = -1.0f;

	// Lets execute a map specific cfg file
	// ** execute this after server.cfg!
	char szCommand[32];
	Q_snprintf( szCommand, sizeof( szCommand ), "exec %s.cfg\n", STRING( gpGlobals->mapname ) );
	engine->ServerCommand( szCommand );

#else // GAME_DLL

	ListenForGameEvent( "game_newmap" );
	
#endif

	// Initialize the game type
	m_nGameType.Set( TF_GAMETYPE_UNDEFINED );

	// Initialize the classes here.
	InitPlayerClasses();

	// Set turbo physics on.  Do it here for now.
	sv_turbophysics.SetValue( 1 );

	// Initialize the team manager here, etc...

	// If you hit these asserts its because you added or removed a weapon type 
	// and didn't also add or remove the weapon name or damage type from the
	// arrays defined in tf_shareddefs.cpp
	COMPILE_TIME_ASSERT( TF_WEAPON_COUNT == ARRAYSIZE( g_aWeaponDamageTypes ) );
	COMPILE_TIME_ASSERT( GAME_WEAPON_COUNT == ARRAYSIZE( g_aWeaponNames ) );

	m_iPreviousRoundWinners = TEAM_UNASSIGNED;
	m_iBirthdayMode = BIRTHDAY_RECALCULATE;

	m_pszTeamGoalStringRed.GetForModify()[0] = '\0';
	m_pszTeamGoalStringBlue.GetForModify()[0] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::FlagsMayBeCapped( void )
{
	if ( State_Get() != GR_STATE_TEAM_WIN )
		return true;

	return false;
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: remove all projectiles in the world
//-----------------------------------------------------------------------------
void CTFGameRules::RemoveAllProjectiles()
{
	for ( int i=0; i<IBaseProjectileAutoList::AutoList().Count(); ++i )
	{
		UTIL_Remove( static_cast< CBaseProjectile* >( IBaseProjectileAutoList::AutoList()[i] ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: remove all buildings in the world
//-----------------------------------------------------------------------------
void CTFGameRules::RemoveAllBuildings( bool bExplodeBuildings /*= false*/ )
{
	for ( int i=0; i<IBaseObjectAutoList::AutoList().Count(); ++i )
	{
		CBaseObject *pObj = static_cast< CBaseObject* >( IBaseObjectAutoList::AutoList()[i] );
		if ( !pObj->IsMapPlaced() )
		{
			// this is separate from the object_destroyed event, which does
			// not get sent when we remove the objects from the world
			IGameEvent *event = gameeventmanager->CreateEvent( "object_removed" );	
			if ( event )
			{
				CTFPlayer *pOwner = pObj->GetOwner();
				event->SetInt( "userid", pOwner ? pOwner->GetUserID() : -1 ); // user ID of the object owner
				event->SetInt( "objecttype", pObj->GetType() ); // type of object removed
				event->SetInt( "index", pObj->entindex() ); // index of the object removed
				gameeventmanager->FireEvent( event );
			}

			if ( bExplodeBuildings )
			{
				pObj->DetonateObject();
			}
			else
			{
				// This fixes a bug in Raid mode where we could spawn where our sentry was but 
				// we didn't get the weapons because they couldn't trace to us in FVisible
				pObj->SetSolid( SOLID_NONE );
				UTIL_Remove( pObj );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: remove all sentries ammo
//-----------------------------------------------------------------------------
void CTFGameRules::RemoveAllSentriesAmmo()
{
	for ( int i=0; i<IBaseObjectAutoList::AutoList().Count(); ++i )
	{
		CBaseObject *pObj = static_cast< CBaseObject* >( IBaseObjectAutoList::AutoList()[i] );
		if ( pObj->GetType() == OBJ_SENTRYGUN )
		{
			CObjectSentrygun *pSentry = assert_cast< CObjectSentrygun* >( pObj );
			pSentry->RemoveAllAmmo();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes all projectiles and buildings from world
//-----------------------------------------------------------------------------
void CTFGameRules::RemoveAllProjectilesAndBuildings( bool bExplodeBuildings /*= false*/ )
{
	RemoveAllProjectiles();
	RemoveAllBuildings( bExplodeBuildings );
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether we should allow mp_timelimit to trigger a map change
//-----------------------------------------------------------------------------
bool CTFGameRules::CanChangelevelBecauseOfTimeLimit( void )
{
	CTeamControlPointMaster *pMaster = g_hControlPointMasters.Count() ? g_hControlPointMasters[0] : NULL;

	// we only want to deny a map change triggered by mp_timelimit if we're not forcing a map reset,
	// we're playing mini-rounds, and the master says we need to play all of them before changing (for maps like Dustbowl)
	if ( !m_bForceMapReset && pMaster && pMaster->PlayingMiniRounds() && pMaster->ShouldPlayAllControlPointRounds() )
	{
		if ( pMaster->NumPlayableControlPointRounds() > 0 )
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::CanGoToStalemate( void )
{
	// In CTF, don't go to stalemate if one of the flags isn't at home
	if ( m_nGameType == TF_GAMETYPE_CTF )
	{
		CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag*> ( gEntList.FindEntityByClassname( NULL, "item_teamflag" ) );
		while( pFlag )
		{
			if ( pFlag->IsDropped() || pFlag->IsStolen() )
				return false;

			pFlag = dynamic_cast<CCaptureFlag*> ( gEntList.FindEntityByClassname( pFlag, "item_teamflag" ) );
		}

		// check that one team hasn't won by capping
		if ( CheckCapsPerRound() )
			return false;
	}

	return BaseClass::CanGoToStalemate();
}

// Classnames of entities that are preserved across round restarts
static const char *s_PreserveEnts[] =
{
	"tf_gamerules",
	"tf_team_manager",
	"tf_player_manager",
	"tf_team",
	"tf_objective_resource",
	"keyframe_rope",
	"move_rope",
	"tf_viewmodel",
	"", // END Marker
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::Activate()
{
	m_iBirthdayMode = BIRTHDAY_RECALCULATE;

	m_nGameType.Set( TF_GAMETYPE_UNDEFINED );

	CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag*> ( gEntList.FindEntityByClassname( NULL, "item_teamflag" ) );
	if ( pFlag )
	{
		m_nGameType.Set( TF_GAMETYPE_CTF );
	}

	if ( g_hControlPointMasters.Count() )
	{
		m_nGameType.Set( TF_GAMETYPE_CP );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	bool bRetVal = true;

	if ( ( State_Get() == GR_STATE_TEAM_WIN ) && pVictim )
	{
		if ( pVictim->GetTeamNumber() == GetWinningTeam() )
		{
			CBaseTrigger *pTrigger = dynamic_cast< CBaseTrigger *>( info.GetInflictor() );

			// we don't want players on the winning team to be
			// hurt by team-specific trigger_hurt entities during the bonus time
			if ( pTrigger && pTrigger->UsesFilter() )
			{
				bRetVal = false;
			}
		}
	}

	return bRetVal;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::SetTeamGoalString( int iTeam, const char *pszGoal )
{
	if ( iTeam == TF_TEAM_RED )
	{
		if ( !pszGoal || !pszGoal[0] )
		{
			m_pszTeamGoalStringRed.GetForModify()[0] = '\0';
		}
		else
		{
			if ( Q_stricmp( m_pszTeamGoalStringRed.Get(), pszGoal ) )
			{
				Q_strncpy( m_pszTeamGoalStringRed.GetForModify(), pszGoal, MAX_TEAMGOAL_STRING );
			}
		}
	}
	else if ( iTeam == TF_TEAM_BLUE )
	{
		if ( !pszGoal || !pszGoal[0] )
		{
			m_pszTeamGoalStringBlue.GetForModify()[0] = '\0';
		}
		else
		{
			if ( Q_stricmp( m_pszTeamGoalStringBlue.Get(), pszGoal ) )
			{
				Q_strncpy( m_pszTeamGoalStringBlue.GetForModify(), pszGoal, MAX_TEAMGOAL_STRING );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::RoundCleanupShouldIgnore( CBaseEntity *pEnt )
{
	if ( FindInList( s_PreserveEnts, pEnt->GetClassname() ) )
		return true;

	//There has got to be a better way of doing this.
	if ( Q_strstr( pEnt->GetClassname(), "tf_weapon_" ) 
		|| Q_strstr( pEnt->GetClassname(), "dod_weapon_" ) )
		return true;

	return BaseClass::RoundCleanupShouldIgnore( pEnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::ShouldCreateEntity( const char *pszClassName )
{
	if ( FindInList( s_PreserveEnts, pszClassName ) )
		return false;

	return BaseClass::ShouldCreateEntity( pszClassName );
}

void CTFGameRules::CleanUpMap( void )
{
	BaseClass::CleanUpMap();

	if ( HLTVDirector() )
	{
		HLTVDirector()->BuildCameraList();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------ob
void CTFGameRules::RecalculateControlPointState( void )
{
	Assert( ObjectiveResource() );

	if ( !g_hControlPointMasters.Count() )
		return;

	if ( g_pObjectiveResource && g_pObjectiveResource->PlayingMiniRounds() )
		return;

	for ( int iTeam = LAST_SHARED_TEAM+1; iTeam < GetNumberOfTeams(); iTeam++ )
	{
		int iFarthestPoint = GetFarthestOwnedControlPoint( iTeam, true );
		if ( iFarthestPoint == -1 )
			continue;

		// Now enable all spawn points for that spawn point
		CBaseEntity *pSpot = gEntList.FindEntityByClassname( NULL, "info_player_teamspawn" );
		while( pSpot )
		{
			CTFTeamSpawn *pTFSpawn = assert_cast<CTFTeamSpawn*>(pSpot);
			if ( pTFSpawn->GetControlPoint() )
			{
				if ( pTFSpawn->GetTeamNumber() == iTeam )
				{
					if ( pTFSpawn->GetControlPoint()->GetPointIndex() == iFarthestPoint )
					{
						pTFSpawn->SetDisabled( false );
					}
					else
					{
						pTFSpawn->SetDisabled( true );
					}
				}
			}

			pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_teamspawn" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when a new round is being initialized
//-----------------------------------------------------------------------------
void CTFGameRules::SetupOnRoundStart( void )
{
	for ( int i = 0; i < MAX_TEAMS; i++ )
	{
		ObjectiveResource()->SetBaseCP( -1, i );
	}

	for ( int i = 0; i < TF_TEAM_COUNT; i++ )
	{
		m_iNumCaps[i] = 0;
	}

	// Let all entities know that a new round is starting
	CBaseEntity *pEnt = gEntList.FirstEnt();
	while( pEnt )
	{
		variant_t emptyVariant;
		pEnt->AcceptInput( "RoundSpawn", NULL, NULL, emptyVariant, 0 );

		pEnt = gEntList.NextEnt( pEnt );
	}

	// All entities have been spawned, now activate them
	m_areHealthAndAmmoVectorsReady = false;
	m_ammoVector.RemoveAll();
	m_healthVector.RemoveAll();

	pEnt = gEntList.FirstEnt();
	while( pEnt )
	{
		variant_t emptyVariant;
		pEnt->AcceptInput( "RoundActivate", NULL, NULL, emptyVariant, 0 );

		pEnt = gEntList.NextEnt( pEnt );
	}

	if ( g_pObjectiveResource && !g_pObjectiveResource->PlayingMiniRounds() )
	{
		// Find all the control points with associated spawnpoints
		memset( m_bControlSpawnsPerTeam, 0, sizeof(bool) * MAX_TEAMS * MAX_CONTROL_POINTS );
		CBaseEntity *pSpot = gEntList.FindEntityByClassname( NULL, "info_player_teamspawn" );
		while( pSpot )
		{
			CTFTeamSpawn *pTFSpawn = assert_cast<CTFTeamSpawn*>(pSpot);
			if ( pTFSpawn->GetControlPoint() )
			{
				m_bControlSpawnsPerTeam[ pTFSpawn->GetTeamNumber() ][ pTFSpawn->GetControlPoint()->GetPointIndex() ] = true;
				pTFSpawn->SetDisabled( true );
			}

			pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_teamspawn" );
		}

		RecalculateControlPointState();

		SetRoundOverlayDetails();
	}
#ifdef GAME_DLL
	m_szMostRecentCappers[0] = 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Called when a new round is off and running
//-----------------------------------------------------------------------------
void CTFGameRules::SetupOnRoundRunning( void )
{
	// Let out control point masters know that the round has started
	for ( int i = 0; i < g_hControlPointMasters.Count(); i++ )
	{
		variant_t emptyVariant;
		if ( g_hControlPointMasters[i] )
		{
			g_hControlPointMasters[i]->AcceptInput( "RoundStart", NULL, NULL, emptyVariant, 0 );
		}
	}

	// Reset player speeds after preround lock
	CTFPlayer *pPlayer;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );

		if ( !pPlayer )
			continue;

		pPlayer->TeamFortress_SetSpeed();
		pPlayer->SpeakConceptIfAllowed( MP_CONCEPT_ROUND_START );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called before a new round is started (so the previous round can end)
//-----------------------------------------------------------------------------
void CTFGameRules::PreviousRoundEnd( void )
{
	// before we enter a new round, fire the "end output" for the previous round
	if ( g_hControlPointMasters.Count() && g_hControlPointMasters[0] )
	{
		g_hControlPointMasters[0]->FireRoundEndOutput();
	}

	m_iPreviousRoundWinners = GetWinningTeam();
}

//-----------------------------------------------------------------------------
// Purpose: Called when a round has entered stalemate mode (timer has run out)
//-----------------------------------------------------------------------------
void CTFGameRules::SetupOnStalemateStart( void )
{
	// Remove everyone's objects
	RemoveAllProjectilesAndBuildings();

	// Respawn all the players
	RespawnPlayers( true );

	// Disable all the active health packs in the world
	m_hDisabledHealthKits.Purge();
	CHealthKit *pHealthPack = gEntList.NextEntByClass( (CHealthKit *)NULL );
	while ( pHealthPack )
	{
		if ( !pHealthPack->IsDisabled() )
		{
			pHealthPack->SetDisabled( true );
			m_hDisabledHealthKits.AddToTail( pHealthPack );
		}
		pHealthPack = gEntList.NextEntByClass( pHealthPack );
	}

	CTFPlayer *pPlayer;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );

		if ( !pPlayer )
			continue;

		pPlayer->SpeakConceptIfAllowed( MP_CONCEPT_SUDDENDEATH_START );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::SetupOnStalemateEnd( void )
{
	// Reenable all the health packs we disabled
	for ( int i = 0; i < m_hDisabledHealthKits.Count(); i++ )
	{
		if ( m_hDisabledHealthKits[i] )
		{
			m_hDisabledHealthKits[i]->SetDisabled( false );
		}
	}

	m_hDisabledHealthKits.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::InitTeams( void )
{
	BaseClass::InitTeams();

	// clear the player class data
	ResetFilePlayerClassInfoDatabase();
}

class CTraceFilterIgnoreTeammatesWithException : public CTraceFilterSimple
{
	DECLARE_CLASS( CTraceFilterIgnoreTeammatesWithException, CTraceFilterSimple );
public:
	CTraceFilterIgnoreTeammatesWithException( const IHandleEntity *passentity, int collisionGroup, int iIgnoreTeam, const IHandleEntity *pExceptionEntity )
		: CTraceFilterSimple( passentity, collisionGroup ), m_iIgnoreTeam( iIgnoreTeam )
	{
		m_pExceptionEntity = pExceptionEntity;
	}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );

		if ( pServerEntity == m_pExceptionEntity )
			return true;

		if ( pEntity->IsPlayer() && pEntity->GetTeamNumber() == m_iIgnoreTeam )
		{
			return false;
		}

		return true;
	}

	int m_iIgnoreTeam;
	const IHandleEntity *m_pExceptionEntity;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::RadiusDamage( CTFRadiusDamageInfo &info )
{
	float flRadSqr = (info.flRadius * info.flRadius);

	int iDamageEnemies = 0;
	int nDamageDealt = 0;
	// Some weapons pass a radius of 0, since their only goal is to give blast jumping ability
	if ( info.flRadius > 0 )
	{
		// Find all the entities in the radius, and attempt to damage them.
		CBaseEntity *pEntity = NULL;
		for ( CEntitySphereQuery sphere( info.vecSrc, info.flRadius ); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
		{
			// Skip the attacker, if we have a RJ radius set. We'll do it post.
			if ( info.flRJRadius && pEntity == info.dmgInfo->GetAttacker() )
				continue;

			// CEntitySphereQuery actually does a box test. So we need to make sure the distance is less than the radius first.
			Vector vecPos;
			pEntity->CollisionProp()->CalcNearestPoint( info.vecSrc, &vecPos );
			if ( (info.vecSrc - vecPos).LengthSqr() > flRadSqr )
				continue;

			int iDamageToEntity = info.ApplyToEntity( pEntity );
			if ( iDamageToEntity )
			{
				// Keep track of any enemies we damaged
				if ( pEntity->IsPlayer() && !pEntity->InSameTeam( info.dmgInfo->GetAttacker() ) )
				{
					nDamageDealt+= iDamageToEntity;
					iDamageEnemies++;
				}
			}
		}
	}

	// If we have a set RJ radius, use it to affect the inflictor. This way Rocket Launchers 
	// with modified damage/radius perform like the base rocket launcher when it comes to RJing.
	if ( info.flRJRadius && info.dmgInfo->GetAttacker() )
	{
		// Set our radius & damage to the base RL
		info.flRadius = info.flRJRadius;
		CTFWeaponBase *pWeapon = dynamic_cast<CTFWeaponBase *>(info.dmgInfo->GetWeapon());
		if ( pWeapon )
		{
			float flBaseDamage = pWeapon->GetPCWpnData().GetWeaponData( TF_WEAPON_PRIMARY_MODE ).m_nDamage;
			info.dmgInfo->SetDamage( flBaseDamage );
			info.dmgInfo->CopyDamageToBaseDamage();
			info.dmgInfo->SetDamagedOtherPlayers( iDamageEnemies );
		}

		// Apply ourselves to our attacker, if we're within range
		Vector vecPos;
		info.dmgInfo->GetAttacker()->CollisionProp()->CalcNearestPoint( info.vecSrc, &vecPos );
		if ( (info.vecSrc - vecPos).LengthSqr() <= (info.flRadius * info.flRadius) )
		{
			info.ApplyToEntity( info.dmgInfo->GetAttacker() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the damage falloff over distance
//-----------------------------------------------------------------------------
void CTFRadiusDamageInfo::CalculateFalloff( void )
{
	if ( dmgInfo->GetDamageType() & DMG_RADIUS_MAX )
		flFalloff = 0.0;
	else if ( dmgInfo->GetDamageType() & DMG_HALF_FALLOFF )
		flFalloff = 0.5;
	else if ( flRadius )
		flFalloff = dmgInfo->GetDamage() / flRadius;
	else
		flFalloff = 1.0;

	CBaseEntity *pWeapon = dmgInfo->GetWeapon();
	if ( pWeapon != NULL )
	{
		float flFalloffMod = 1.f;
		if ( flFalloffMod != 1.f )
		{
			flFalloff += flFalloffMod;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Attempt to apply the radius damage to the specified entity
//-----------------------------------------------------------------------------
int CTFRadiusDamageInfo::ApplyToEntity( CBaseEntity *pEntity )
{
	if ( pEntity == pEntityIgnore || pEntity->m_takedamage == DAMAGE_NO )
		return 0;

	trace_t	tr;
	CBaseEntity *pInflictor = dmgInfo->GetInflictor();

	// Check that the explosion can 'see' this entity.
	Vector vecSpot = pEntity->BodyTarget( vecSrc, false );
	CTraceFilterIgnorePlayers filterPlayers( pInflictor, COLLISION_GROUP_PROJECTILE );
	CTraceFilterIgnoreFriendlyCombatItems filterCombatItems( pInflictor, COLLISION_GROUP_PROJECTILE, pInflictor->GetTeamNumber() );
	CTraceFilterChain filter( &filterPlayers, &filterCombatItems );

	UTIL_TraceLine( vecSrc, vecSpot, MASK_RADIUS_DAMAGE, &filter, &tr );
	if ( tr.startsolid && tr.m_pEnt )
	{
		// Return when inside an enemy combat shield and tracing against a player of that team ("absorbed")
		if ( tr.m_pEnt->IsCombatItem() && pEntity->InSameTeam( tr.m_pEnt ) && ( pEntity != tr.m_pEnt ) )
			return 0;

		filterPlayers.SetPassEntity( tr.m_pEnt );
		CTraceFilterChain filterSelf( &filterPlayers, &filterCombatItems );
		UTIL_TraceLine( vecSrc, vecSpot, MASK_RADIUS_DAMAGE, &filterSelf, &tr );
	}

	// If we don't trace the whole way to the target, and we didn't hit the target entity, we're blocked
	if ( tr.fraction != 1.0 && tr.m_pEnt != pEntity )
		return 0;

	// Adjust the damage - apply falloff.
	float flAdjustedDamage = 0.0f;
	float flDistanceToEntity;

	// Rockets store the ent they hit as the enemy and have already dealt full damage to them by this time
	if ( pInflictor && ( pEntity == pInflictor->GetEnemy() ) )
	{
		// Full damage, we hit this entity directly
		flDistanceToEntity = 0;
	}
	else if ( pEntity->IsPlayer() )
	{
		// Use whichever is closer, absorigin or worldspacecenter
		float flToWorldSpaceCenter = ( vecSrc - pEntity->WorldSpaceCenter() ).Length();
		float flToOrigin = ( vecSrc - pEntity->GetAbsOrigin() ).Length();

		flDistanceToEntity = MIN( flToWorldSpaceCenter, flToOrigin );
	}
	else
	{
		flDistanceToEntity = ( vecSrc - tr.endpos ).Length();
	}

	flAdjustedDamage = RemapValClamped( flDistanceToEntity, 0, flRadius, dmgInfo->GetDamage(), dmgInfo->GetDamage() * flFalloff );

	CTFWeaponBase *pWeapon = dynamic_cast<CTFWeaponBase *>(dmgInfo->GetWeapon());
	
	// Grenades & Pipebombs do less damage to ourselves.
	if ( pEntity == dmgInfo->GetAttacker() && pWeapon )
	{
		switch( pWeapon->GetWeaponID() )
		{
			case TF_WEAPON_PIPEBOMBLAUNCHER :
			case TF_WEAPON_GRENADELAUNCHER :
				flAdjustedDamage *= 0.75f;
				break;
		}
	}

	// If we end up doing 0 damage, exit now.
	if ( flAdjustedDamage <= 0 )
		return 0;

	// the explosion can 'see' this entity, so hurt them!
	if (tr.startsolid)
	{
		// if we're stuck inside them, fixup the position and distance
		tr.endpos = vecSrc;
		tr.fraction = 0.0;
	}

	CTakeDamageInfo adjustedInfo = *dmgInfo;
	adjustedInfo.SetDamage( flAdjustedDamage );

	Vector dir = vecSpot - vecSrc;
	VectorNormalize( dir );

	// If we don't have a damage force, manufacture one
	if ( adjustedInfo.GetDamagePosition() == vec3_origin || adjustedInfo.GetDamageForce() == vec3_origin )
	{
		CalculateExplosiveDamageForce( &adjustedInfo, dir, vecSrc );
	}
	else
	{
		// Assume the force passed in is the maximum force. Decay it based on falloff.
		float flForce = adjustedInfo.GetDamageForce().Length() * flFalloff;
		adjustedInfo.SetDamageForce( dir * flForce );
		adjustedInfo.SetDamagePosition( vecSrc );
	}

	adjustedInfo.ScaleDamageForce( m_flForceScale );

	int nDamageTaken = 0;
	if ( tr.fraction != 1.0 && pEntity == tr.m_pEnt )
	{
		ClearMultiDamage( );
		pEntity->DispatchTraceAttack( adjustedInfo, dir, &tr );
		ApplyMultiDamage();
	}
	else
	{
		nDamageTaken = pEntity->TakeDamage( adjustedInfo );
	}

	// Now hit all triggers along the way that respond to damage.
	pEntity->TraceAttackToTriggers( adjustedInfo, vecSrc, tr.endpos, dir );
	return nDamageTaken;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//			&vecSrcIn - 
//			flRadius - 
//			iClassIgnore - 
//			*pEntityIgnore - 
//-----------------------------------------------------------------------------
void CTFGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore )
{
	// This shouldn't be used. Call the version above that takes a CTFRadiusDamageInfo pointer.
	Assert(0);

	CTakeDamageInfo dmgInfo = info;
	Vector vecSrc = vecSrcIn;
	CTFRadiusDamageInfo radiusinfo( &dmgInfo, vecSrc, flRadius, pEntityIgnore );
	RadiusDamage(radiusinfo);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::ApplyOnDamageModifyRules( CTakeDamageInfo &info, CBaseEntity *pVictimBaseEntity, bool bAllowDamage )
{
	info.SetDamageForForceCalc( info.GetDamage() );
	bool bDebug = tf_debug_damage.GetBool();

	CTFPlayer *pVictim = ToTFPlayer( pVictimBaseEntity );
	CBaseEntity *pAttacker = info.GetAttacker();
	CTFPlayer *pTFAttacker = ToTFPlayer( pAttacker );

	// damage may not come from a weapon (ie: Bosses, etc)
	// The existing code below already checked for NULL pWeapon, anyways
	CTFWeaponBase *pWeapon = dynamic_cast< CTFWeaponBase * >( info.GetWeapon() );

	bool bShowDisguisedCrit = false;
	bool bAllSeeCrit = false;
	EAttackBonusEffects_t eBonusEffect = kBonusEffect_None;

	if ( pVictim )
	{
		pVictim->SetSeeCrit( false, false, false );
		pVictim->SetAttackBonusEffect( kBonusEffect_None );
	}

	int bitsDamage = info.GetDamageType();

	// Damage type was already crit (Flares / headshot)
	if ( bitsDamage & DMG_CRITICAL )
	{
		info.SetCritType( CTakeDamageInfo::CRIT_FULL );
	}

	// First figure out whether this is going to be a full forced crit for some specific reason. It's
	// important that we do this before figuring out whether we're going to be a minicrit or not.

	// Allow attributes to force critical hits on players with specific conditions
	if ( pVictim )
	{
		// Crit against players that have these conditions
		int iCritDamageTypes = 0;
		//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iCritDamageTypes, or_crit_vs_playercond );

		if ( iCritDamageTypes )
		{
			// iCritDamageTypes is an or'd list of types. We need to pull each bit out and
			// then test against what that bit in the items_master file maps to.
			for ( int i = 0; condition_to_attribute_translation[i] != TF_COND_LAST; i++ )
			{
				if ( iCritDamageTypes & ( 1 << i ) )
				{
					if ( pVictim->m_Shared.InCond( condition_to_attribute_translation[ i ] ) )
					{
						bitsDamage |= DMG_CRITICAL;
						info.AddDamageType( DMG_CRITICAL );
						info.SetCritType( CTakeDamageInfo::CRIT_FULL );

						if ( condition_to_attribute_translation[i] == TF_COND_DISGUISED || 
							 condition_to_attribute_translation[i] == TF_COND_DISGUISING )
						{
							// if our attribute specifically crits disguised enemies we need to show it on the client
							bShowDisguisedCrit = true;
						}
						break;
					}
				}
			}
		}

		int iCritVsWet = 0;
		//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iCritVsWet, crit_vs_wet_players );
		if ( iCritVsWet )
		{
			float flWaterExitTime = pVictim->GetWaterExitTime();

			if ( pVictim->m_Shared.InCond( TF_COND_URINE ) ||
				 pVictim->m_Shared.InCond( TF_COND_MAD_MILK ) ||
			   ( pVictim->GetWaterLevel() > WL_NotInWater ) ||
			   ( ( flWaterExitTime > 0 ) && ( gpGlobals->curtime - flWaterExitTime < 5.0f ) ) ) // or they exited the water in the last few seconds
			{
				bitsDamage |= DMG_CRITICAL;
				info.AddDamageType( DMG_CRITICAL );
				info.SetCritType( CTakeDamageInfo::CRIT_FULL );
			}
		}
 
		// Crit against players that don't have these conditions
		int iCritDamageNotTypes = 0;
		//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iCritDamageNotTypes, or_crit_vs_not_playercond );

		if ( iCritDamageNotTypes )
		{
			// iCritDamageTypes is an or'd list of types. We need to pull each bit out and
			// then test against what that bit in the items_master file maps to.
			for ( int i = 0; condition_to_attribute_translation[i] != TF_COND_LAST; i++ )
			{
				if ( iCritDamageNotTypes & ( 1 << i ) )
				{
					if ( !pVictim->m_Shared.InCond( condition_to_attribute_translation[ i ] ) )
					{
						bitsDamage |= DMG_CRITICAL;
						info.AddDamageType( DMG_CRITICAL );
						info.SetCritType( CTakeDamageInfo::CRIT_FULL );

						if ( condition_to_attribute_translation[ i ] == TF_COND_DISGUISED || 
							 condition_to_attribute_translation[ i ] == TF_COND_DISGUISING )
						{
							// if our attribute specifically crits disguised enemies we need to show it on the client
							bShowDisguisedCrit = true;
						}
						break;
					}
				}
			}
		}

		// Crit burning behind
		int iCritBurning = 0;
		//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iCritBurning, axtinguisher_properties );
		if ( iCritBurning && pVictim->m_Shared.InCond( TF_COND_BURNING ) )
		{
			// Full crit in back, mini in front
			Vector toEnt = pVictim->GetAbsOrigin() - pTFAttacker->GetAbsOrigin();
			{
				Vector entForward;
				AngleVectors( pVictim->EyeAngles(), &entForward );
				toEnt.z = 0;
				toEnt.NormalizeInPlace();

				if ( DotProduct( toEnt, entForward ) > 0.0f )	// 90 degrees from center (total of 180)
				{
					bitsDamage |= DMG_CRITICAL;
					info.AddDamageType( DMG_CRITICAL );
					info.SetCritType( CTakeDamageInfo::CRIT_FULL );
				}
				else
				{
					bAllSeeCrit = true;
					info.SetCritType( CTakeDamageInfo::CRIT_MINI );
					eBonusEffect = kBonusEffect_MiniCrit;
				}
			}
		}

		// Mini Crit and consume burning condition
		int iMiniCritBurning = 0;
		//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iMiniCritBurning, attack_minicrits_and_consumes_burning );
		if ( iMiniCritBurning && pVictim->m_Shared.InCond( TF_COND_BURNING ) )
		{
			bAllSeeCrit = true;
			info.SetCritType( CTakeDamageInfo::CRIT_MINI );
			eBonusEffect = kBonusEffect_MiniCrit;
			// Remove burning from the victim, also give a speed boost on kill; see CTFPlayer::Event_KilledOther().
			pVictim->m_Shared.RemoveCond( TF_COND_BURNING );
		}
	}

	int iCritWhileAirborne = 0;
	//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iCritWhileAirborne, crit_while_airborne );
	if ( iCritWhileAirborne && pTFAttacker )
	{
		if ( pTFAttacker->InAirDueToExplosion() )
		{
			bitsDamage |= DMG_CRITICAL;
			info.AddDamageType( DMG_CRITICAL );
			info.SetCritType( CTakeDamageInfo::CRIT_FULL );
		}
	}
	
	// For awarding assist damage stat later
	ETFCond eDamageBonusCond = TF_COND_LAST;

	// Some forms of damage override long range damage falloff
	bool bIgnoreLongRangeDmgEffects = false;

	// Figure out if it's a minicrit or not
	// But we never minicrit ourselves.
	if ( pAttacker != pVictimBaseEntity )
	{
		if ( info.GetCritType() == CTakeDamageInfo::CRIT_NONE )
		{
			CBaseEntity *pInflictor = info.GetInflictor();
			CTFGrenadePipebombProjectile *pBaseGrenade = dynamic_cast< CTFGrenadePipebombProjectile* >( pInflictor );
			CTFBaseRocket *pBaseRocket = dynamic_cast< CTFBaseRocket* >( pInflictor );

			if ( pVictim && ( pVictim->m_Shared.InCond( TF_COND_URINE ) ||
			                  pVictim->m_Shared.InCond( TF_COND_MARKEDFORDEATH ) ||
			                  pVictim->m_Shared.InCond( TF_COND_MARKEDFORDEATH_SILENT ) ||
			                  pVictim->m_Shared.InCond( TF_COND_PASSTIME_PENALTY_DEBUFF ) ) )
			{
				bAllSeeCrit = true;
				info.SetCritType( CTakeDamageInfo::CRIT_MINI );
				eBonusEffect = kBonusEffect_MiniCrit;

				if ( !pVictim->m_Shared.InCond( TF_COND_MARKEDFORDEATH_SILENT ) )
				{
					eDamageBonusCond = pVictim->m_Shared.InCond( TF_COND_URINE ) ? TF_COND_URINE : TF_COND_MARKEDFORDEATH;
				}
			}
			else if ( pTFAttacker && ( pTFAttacker->m_Shared.InCond( TF_COND_OFFENSEBUFF ) || pTFAttacker->m_Shared.InCond( TF_COND_NOHEALINGDAMAGEBUFF ) ) )
			{
				// Attackers buffed by the soldier do mini-crits.
				bAllSeeCrit = true;
				info.SetCritType( CTakeDamageInfo::CRIT_MINI );
				eBonusEffect = kBonusEffect_MiniCrit;

				if ( pTFAttacker->m_Shared.InCond( TF_COND_OFFENSEBUFF ) )
				{
					eDamageBonusCond = TF_COND_OFFENSEBUFF;
				}
			}
			else if ( pTFAttacker && (bitsDamage & DMG_RADIUS_MAX) && pWeapon && ( (pWeapon->GetWeaponID() == TF_WEAPON_BOTTLE)|| (pWeapon->GetWeaponID() == TF_WEAPON_WRENCH) ) )
			{
				// First sword or bottle attack after a charge is a mini-crit.
				bAllSeeCrit = true;
				info.SetCritType( CTakeDamageInfo::CRIT_MINI );
				eBonusEffect = kBonusEffect_MiniCrit;
			}
			else if ( ( pInflictor && pInflictor->IsPlayer() == false ) && ( ( pBaseRocket && pBaseRocket->GetDeflected() ) || ( pBaseGrenade && pBaseGrenade->GetDeflected() && ( pBaseGrenade->ShouldMiniCritOnReflect() ) ) ) )
			{
				// Reflected rockets, grenades (non-remote detonate), arrows always mini-crit
				info.SetCritType( CTakeDamageInfo::CRIT_MINI );
				eBonusEffect = kBonusEffect_MiniCrit;
			}
			else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_PLASMA_CHARGED )
			{
				// Charged plasma shots do minicrits.
				info.SetCritType( CTakeDamageInfo::CRIT_MINI );
				eBonusEffect = kBonusEffect_MiniCrit;
			}
			else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_CLEAVER_CRIT )
			{
				// Long range cleaver hit
				info.SetCritType( CTakeDamageInfo::CRIT_MINI );
				eBonusEffect = kBonusEffect_MiniCrit;
			}
			else if ( pTFAttacker && ( pTFAttacker->m_Shared.InCond( TF_COND_ENERGY_BUFF ) ) )
			{
				// Scouts using crit drink do mini-crits, as well as receive them
				info.SetCritType( CTakeDamageInfo::CRIT_MINI );
				eBonusEffect = kBonusEffect_MiniCrit;
			}
			else
			{
				// Allow Attributes to shortcut out if found, no need to check all of them
				for ( int i = 0; i < 1; ++i )
				{
					// Some weapons force minicrits on burning targets.
					// Does not work for burn but works for ignite
					int iForceMiniCritOnBurning = 0;
					//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iForceMiniCritOnBurning, or_minicrit_vs_playercond_burning );
					if ( iForceMiniCritOnBurning == 1 && pVictim && pVictim->m_Shared.InCond( TF_COND_BURNING ) && !( info.GetDamageType() & DMG_BURN ) )
					{
						bAllSeeCrit = true;
						info.SetCritType( CTakeDamageInfo::CRIT_MINI );
						eBonusEffect = kBonusEffect_MiniCrit;
						break;
					}

					// Some weapons mini-crit airborne targets. Airborne targets are any target that has been knocked 
					// into the air by an explosive force from an enemy.
					int iMiniCritAirborne = 0;
					//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iMiniCritAirborne, mini_crit_airborne );
					if ( iMiniCritAirborne == 1 && pVictim && pVictim->InAirDueToKnockback() )
					{
						bAllSeeCrit = true;
						info.SetCritType( CTakeDamageInfo::CRIT_MINI );
						eBonusEffect = kBonusEffect_MiniCrit;
						break;
					}

					if ( pTFAttacker && pVictim )
					{
						// MiniCrit a victims back at close range
						int iMiniCritBackAttack = 0;
						//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iMiniCritBackAttack, closerange_backattack_minicrits );
						Vector toEnt = pVictim->GetAbsOrigin() - pTFAttacker->GetAbsOrigin();
						if ( iMiniCritBackAttack == 1 && toEnt.LengthSqr() < Square( 512.0f ) )
						{
							Vector entForward; 
							AngleVectors( pVictim->EyeAngles(), &entForward );
							toEnt.z = 0;
							toEnt.NormalizeInPlace();

							if ( DotProduct( toEnt, entForward ) > 0.259f )	// 75 degrees from center (total of 150)
							{
								bAllSeeCrit = true;
								info.SetCritType( CTakeDamageInfo::CRIT_MINI );
								eBonusEffect = kBonusEffect_MiniCrit;
								break;
							}
						}
					}
				}
			}

			// Don't do long range distance falloff when pAttacker has Rocket Specialist attrib and directly hits an enemy
			if ( pBaseRocket && pBaseRocket->GetEnemy() && pBaseRocket->GetEnemy() == pVictimBaseEntity )
			{
				int iRocketSpecialist = 0;
				//CALL_ATTRIB_HOOK_INT_ON_OTHER( pAttacker, iRocketSpecialist, rocket_specialist );
				if ( iRocketSpecialist )
					bIgnoreLongRangeDmgEffects = true;
			}
		}
	}

	if ( info.GetCritType() == CTakeDamageInfo::CRIT_MINI )
	{
		int iPromoteMiniCritToCrit = 0;
		//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iPromoteMiniCritToCrit, minicrits_become_crits );
		if ( iPromoteMiniCritToCrit == 1 )
		{
			info.SetCritType( CTakeDamageInfo::CRIT_FULL );
			eBonusEffect = kBonusEffect_Crit;
			bitsDamage |= DMG_CRITICAL;
			info.AddDamageType( DMG_CRITICAL );
		}
	}

	if ( pVictim )
	{
		pVictim->SetSeeCrit( bAllSeeCrit, info.GetCritType() == CTakeDamageInfo::CRIT_MINI, bShowDisguisedCrit );
		pVictim->SetAttackBonusEffect( eBonusEffect );
	}

	// If we're invulnerable, force ourselves to only take damage events only, so we still get pushed
	if ( pVictim && pVictim->m_Shared.IsInvulnerable() )
	{
		if ( !bAllowDamage )
		{
			int iOldTakeDamage = pVictim->m_takedamage;
			pVictim->m_takedamage = DAMAGE_EVENTS_ONLY;
			// NOTE: Deliberately skip base player OnTakeDamage, because we don't want all the stuff it does re: suit voice
			pVictim->CBaseCombatCharacter::OnTakeDamage( info );
			pVictim->m_takedamage = iOldTakeDamage;

			// Burn sounds are handled in ConditionThink()
			if ( !(bitsDamage & DMG_BURN ) )
			{
				pVictim->SpeakConceptIfAllowed( MP_CONCEPT_HURT );
			}

			return false;
		}
	}


	if ( info.GetDamageCustom() == TF_DMG_CUSTOM_BACKSTAB )
	{
		// Jarate backstabber
		int iJarateBackstabber = 0;
		//CALL_ATTRIB_HOOK_INT_ON_OTHER( pVictim, iJarateBackstabber, jarate_backstabber );
		if ( iJarateBackstabber > 0 && pTFAttacker )
		{
			pTFAttacker->m_Shared.AddCond( TF_COND_URINE, 10.0f, pVictim );
			//pTFAttacker->m_Shared.SetPeeAttacker( pVictim );
			pTFAttacker->SpeakConceptIfAllowed( MP_CONCEPT_JARATE_HIT );
		}

		//if ( pVictim && pVictim->CheckBlockBackstab( pTFAttacker ) )
		//{
		//	// The backstab was absorbed by a shield.
		//	info.SetDamage( 0 );

		//	// Shake nearby players' screens.
		//	UTIL_ScreenShake( pVictim->GetAbsOrigin(), 25.f, 150.0, 1.0, 50.f, SHAKE_START );

		//	// Play the notification sound.
		//	pVictim->EmitSound( "Player.Spy_Shield_Break" );

		//	// Unzoom the sniper.
		//	CTFWeaponBase *pWeapon = pVictim->GetActivePCWeapon();
		//	if ( pWeapon && WeaponID_IsSniperRifle( pWeapon->GetWeaponID() ) )
		//	{
		//		CTFSniperRifle *pSniperRifle = static_cast< CTFSniperRifle* >( pWeapon );
		//		if ( pSniperRifle->IsZoomed() )
		//		{
		//			pSniperRifle->ZoomOut();
		//		}
		//	}

		//	// Vibrate the spy's knife.
		//	if ( pTFAttacker && pTFAttacker->GetActiveWeapon() )
		//	{
		//		CTFKnife *pKnife = (CTFKnife *) pTFAttacker->GetActiveWeapon();
		//		if ( pKnife )
		//		{
		//			pKnife->BackstabBlocked();
		//		}
		//	}

		//	// Tell the clients involved in the jarate
		//	CRecipientFilter involved_filter;
		//	involved_filter.AddRecipient( pVictim );
		//	involved_filter.AddRecipient( pTFAttacker );

		//	UserMessageBegin( involved_filter, "PlayerShieldBlocked" );
		//		WRITE_BYTE( pTFAttacker->entindex() );
		//		WRITE_BYTE( pVictim->entindex() );
		//	MessageEnd();
		//}
	}

	// Apply attributes that increase damage vs players
	if ( pWeapon )
	{
		float flDamage = info.GetDamage();
		//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flDamage, mult_dmg_vs_players );

		// Check if we're to boost damage against the same class
		if( pVictim && pTFAttacker )
		{
			int nVictimClass	= pVictim->GetPlayerClass()->GetClassIndex();
			int nAttackerClass	= pTFAttacker->GetPlayerClass()->GetClassIndex();

			// Same class?
			if( nVictimClass == nAttackerClass )
			{
				// Potentially boost damage
				//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flDamage, mult_dmg_vs_same_class );
			}
		}

		info.SetDamage( flDamage );
	}

	if ( pVictim && !pVictim->m_Shared.InCond( TF_COND_BURNING ) )
	{
		if ( bitsDamage & DMG_CRITICAL )
		{
			if ( pTFAttacker && !pTFAttacker->m_Shared.IsCritBoosted() )
			{
				int iNonBurningCritsDisabled = 0;
				//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iNonBurningCritsDisabled, set_nocrit_vs_nonburning );
				if ( iNonBurningCritsDisabled )
				{
					bitsDamage &= ~DMG_CRITICAL;
					info.SetDamageType( info.GetDamageType() & (~DMG_CRITICAL) );
					info.SetCritType( CTakeDamageInfo::CRIT_NONE );
				}
			}
		}

		float flDamage = info.GetDamage();
		//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flDamage, mult_dmg_vs_nonburning );
		info.SetDamage( flDamage );
	}

	// Alien Isolation SetBonus Checking
	if ( pVictim && pTFAttacker && pWeapon )
	{
		// Alien->Merc melee bonus
		if ( ( info.GetDamageType() & (DMG_CLUB|DMG_SLASH) ) && info.GetDamageCustom() != TF_DMG_CUSTOM_BASEBALL )
		{
			CTFWeaponBaseMelee *pMelee = dynamic_cast<CTFWeaponBaseMelee*>( pWeapon );
			if ( pMelee )
			{
				int iAttackerAlien = 0;
				int iVictimMerc = 0;
				//CALL_ATTRIB_HOOK_INT_ON_OTHER( pTFAttacker, iAttackerAlien, alien_isolation_xeno_bonus_pos );
				//CALL_ATTRIB_HOOK_INT_ON_OTHER( pVictim, iVictimMerc, alien_isolation_merc_bonus_pos );

				if ( iAttackerAlien && iVictimMerc )
				{
					info.SetDamage( info.GetDamage() * 5.0f );
				}
			}
		}

		// Merc->Alien MK50 damage, aka flamethrower
		if ( ( info.GetDamageType() & DMG_IGNITE ) && pWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER )
		{
			int iAttackerMerc = 0;
			int iVictimAlien = 0;
			//CALL_ATTRIB_HOOK_INT_ON_OTHER( pTFAttacker, iAttackerMerc, alien_isolation_merc_bonus_pos );
			//CALL_ATTRIB_HOOK_INT_ON_OTHER( pVictim, iVictimAlien, alien_isolation_xeno_bonus_pos );

			if ( iAttackerMerc && iVictimAlien )
			{
				info.SetDamage( info.GetDamage() * 3.0f );	
			}
		}
	}

	int iPierceResists = 0;
	//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iPierceResists, mod_ignore_resists_absorbs );

	// Use defense buffs if it's not a backstab or direct crush damage (telefrage, etc.)
	if ( pVictim && info.GetDamageCustom() != TF_DMG_CUSTOM_BACKSTAB && ( info.GetDamageType() & DMG_CRUSH ) == 0 )
	{
		if ( pVictim->m_Shared.InCond( TF_COND_DEFENSEBUFF ) )
		{
			// We take no crits of any kind...
			if( eBonusEffect == kBonusEffect_MiniCrit || eBonusEffect == kBonusEffect_Crit )
				eBonusEffect = kBonusEffect_None;
			info.SetCritType( CTakeDamageInfo::CRIT_NONE );
			bAllSeeCrit = false;
			bShowDisguisedCrit = false;

			pVictim->SetSeeCrit( bAllSeeCrit, false, bShowDisguisedCrit );
			pVictim->SetAttackBonusEffect( eBonusEffect );

			bitsDamage &= ~DMG_CRITICAL;
			info.SetDamageType( bitsDamage );
			info.SetCritType( CTakeDamageInfo::CRIT_NONE );
		}

		if ( !iPierceResists )
		{
			// If we are defense buffed...
			if ( pVictim->m_Shared.InCond( TF_COND_DEFENSEBUFF_HIGH ) )
			{
				// We take 75% less damage... still take crits
				info.SetDamage( info.GetDamage() * 0.25f );
			}
			else if ( pVictim->m_Shared.InCond( TF_COND_DEFENSEBUFF ) || pVictim->m_Shared.InCond( TF_COND_DEFENSEBUFF_NO_CRIT_BLOCK ) )
			{
				// defense buffs gives 50% to sentry dmg and 35% from all other sources
				CObjectSentrygun *pSentry = ( info.GetInflictor() && info.GetInflictor()->IsBaseObject() ) ? dynamic_cast< CObjectSentrygun* >( info.GetInflictor() ) : NULL;
				if ( pSentry )
				{
					info.SetDamage( info.GetDamage() * 0.50f );
				}
				else
				{
					// And we take 35% less damage...
					info.SetDamage( info.GetDamage() * 0.65f );
				}
			}
		}
	}

	// A note about why crits now go through the randomness/variance code:
	// Normally critical damage is not affected by variance.  However, we always want to measure what that variance 
	// would have been so that we can lump it into the DamageBonus value inside the info.  This means crits actually
	// boost more than 3X when you factor the reduction we avoided.  Example: a rocket that normally would do 50
	// damage due to range now does the original 100, which is then multiplied by 3, resulting in a 6x increase.
	bool bCrit = ( bitsDamage & DMG_CRITICAL ) ?  true : false;

	// If we're not damaging ourselves, apply randomness
	if ( pAttacker != pVictimBaseEntity && !(bitsDamage & (DMG_DROWN | DMG_FALL)) ) 
	{
		float flDamage = info.GetDamage();
		float flDmgVariance = 0.f;

		int iCritDistanceAttrib = 0;
		//CTFWeaponBase* pTFWeaponInflictor = static_cast<CTFWeaponBase*>(info.GetWeapon());
		//if (pTFWeaponInflictor)
			//CALL_ATTRIB_HOOK_INT_ON_OTHER(pTFWeaponInflictor, iCritDistanceAttrib, crit_dmg_falloff);

		// Minicrits still get short range damage bonus
		bool bForceCritFalloff = iCritDistanceAttrib && info.GetCritType() == CTakeDamageInfo::CRIT_FULL || ( bitsDamage & DMG_USEDISTANCEMOD ) && 
								 ( ( bCrit && tf_weapon_criticals_distance_falloff.GetBool() ) || 
								 ( info.GetCritType() == CTakeDamageInfo::CRIT_MINI && tf_weapon_minicrits_distance_falloff.GetBool() ) );

		bool bDoShortRangeDistanceIncrease = !bCrit || info.GetCritType() == CTakeDamageInfo::CRIT_MINI;
		bool bDoLongRangeDistanceDecrease = !bIgnoreLongRangeDmgEffects && ( bForceCritFalloff || ( !bCrit && info.GetCritType() != CTakeDamageInfo::CRIT_MINI ) );

		// If we're doing any distance modification, we need to do that first
		float flRandomDamage = info.GetDamage() * tf_damage_range.GetFloat();

		float flRandomDamageSpread = 0.10f;
		float flMin = 0.5 - flRandomDamageSpread;
		float flMax = 0.5 + flRandomDamageSpread;

		if ( bitsDamage & DMG_USEDISTANCEMOD )
		{
			Vector vAttackerPos = pAttacker->WorldSpaceCenter();
			float flOptimalDistance = 512.0;

			// Use Sentry position for distance mod
			CObjectSentrygun *pSentry = dynamic_cast<CObjectSentrygun*>( info.GetInflictor() );
			if ( pSentry )
			{
				vAttackerPos = pSentry->WorldSpaceCenter();
				// Sentries have a much further optimal distance
				flOptimalDistance = SENTRY_MAX_RANGE;
			}
			// The base sniper rifle doesn't have DMG_USEDISTANCEMOD, so this isn't used. Unlockable rifle had it for a bit.
			else if ( pWeapon && WeaponID_IsSniperRifle( pWeapon->GetWeaponID() ) )
			{
				flOptimalDistance *= 2.5f;
			}

			float flDistance = MAX( 1.0, ( pVictimBaseEntity->WorldSpaceCenter() - vAttackerPos).Length() );
				
			float flCenter = RemapValClamped( flDistance / flOptimalDistance, 0.0, 2.0, 1.0, 0.0 );
			if ( ( flCenter > 0.5 && bDoShortRangeDistanceIncrease ) || flCenter <= 0.5 )
			{
				if ( bitsDamage & DMG_NOCLOSEDISTANCEMOD )
				{
					if ( flCenter > 0.5 )
					{
						// Reduce the damage bonus at close range
						flCenter = RemapVal( flCenter, 0.5, 1.0, 0.5, 0.65 );
					}
				}
				flMin = MAX( 0.0, flCenter - flRandomDamageSpread );
				flMax = MIN( 1.0, flCenter + flRandomDamageSpread );

				if ( bDebug )
				{
					Warning("    RANDOM: Dist %.2f, Ctr: %.2f, Min: %.2f, Max: %.2f\n", flDistance, flCenter, flMin, flMax );
				}
			}
			else
			{
				if ( bDebug )
				{
					Warning("    NO DISTANCE MOD: Dist %.2f, Ctr: %.2f, Min: %.2f, Max: %.2f\n", flDistance, flCenter, flMin, flMax );
				}
			}
		}
		//Msg("Range: %.2f - %.2f\n", flMin, flMax );
		float flRandomRangeVal;
		if ( tf_damage_disablespread.GetBool() )
		{
			flRandomRangeVal = flMin + flRandomDamageSpread;
		}
		else
		{
			flRandomRangeVal = RandomFloat( flMin, flMax );
		}

		//if ( bDebug )
		//{
		//	Warning( "            Val: %.2f\n", flRandomRangeVal );
		//}

		// Weapon Based Damage Mod
		if ( pWeapon && pAttacker && pAttacker->IsPlayer() )
		{
			switch ( pWeapon->GetWeaponID() )
			{
			// Rocket launcher only has half the bonus of the other weapons at short range
			// Rocket Launchers
			case TF_WEAPON_ROCKETLAUNCHER :
				if ( flRandomRangeVal > 0.5 )
				{
					flRandomDamage *= 0.5f;
				}
				break;
			case TF_WEAPON_PIPEBOMBLAUNCHER :	// Stickies
			case TF_WEAPON_GRENADELAUNCHER :
				if ( !( bitsDamage & DMG_NOCLOSEDISTANCEMOD ) )
				{
					flRandomDamage *= 0.2f;
				}
				break;
			case TF_WEAPON_SCATTERGUN :
				// Scattergun gets 50% bonus at short range
				if ( flRandomRangeVal > 0.5 )
				{
					flRandomDamage *= 1.5f;
				}
				break;
			}
		}

		// Random damage variance.
		// No damage variance for Day of Defeat characters
		if ( pAttacker && pAttacker->IsPlayer() && ToTFPlayer( pAttacker )->IsDODClass() )
			flDmgVariance = 0.0f;
		else
			flDmgVariance = SimpleSplineRemapValClamped( flRandomRangeVal, 0, 1, -flRandomDamage, flRandomDamage );

		if ( ( bDoShortRangeDistanceIncrease && flDmgVariance > 0.f ) || bDoLongRangeDistanceDecrease )
		{
			flDamage = info.GetDamage() + flDmgVariance;
		}

		if ( bDebug )
		{
			Warning("            Out: %.2f -> Final %.2f\n", flDmgVariance, flDamage );
		}

		/*
		for ( float flVal = flMin; flVal <= flMax; flVal += 0.05 )
		{
			float flOut = SimpleSplineRemapValClamped( flVal, 0, 1, -flRandomDamage, flRandomDamage );
			Msg("Val: %.2f, Out: %.2f, Dmg: %.2f\n", flVal, flOut, info.GetDamage() + flOut );
		}
		*/

		// Burn sounds are handled in ConditionThink()
		if ( !(bitsDamage & DMG_BURN ) && pVictim )
		{
			pVictim->SpeakConceptIfAllowed( MP_CONCEPT_HURT );
		}


		// Save any bonus damage as a separate value
		float flCritDamage = 0.f;
		// Yes, it's weird that we sometimes fabs flDmgVariance.  Here's why: In the case of a crit rocket, we
		// know that number will generally be negative due to dist or randomness.  In this case, we want to track
		// that effect - even if we don't apply it.  In the case of our crit rocket that normally would lose 50 
		// damage, we fabs'd so that we can account for it as a bonus - since it's present in a crit.
		float flBonusDamage = bForceCritFalloff ? 0.f : fabs( flDmgVariance );
		CTFPlayer *pProvider = NULL;

		if ( info.GetCritType() == CTakeDamageInfo::CRIT_MINI )
		{
			// We should never have both of these flags set or Weird Things will happen with the damage numbers
			// that aren't clear to the players. Or us, really.
			Assert( !(bitsDamage & DMG_CRITICAL) );

			if ( bDebug )
			{
				Warning( "    MINICRIT: Dmg %.2f -> ", flDamage );
			}

			COMPILE_TIME_ASSERT( TF_DAMAGE_MINICRIT_MULTIPLIER > 1.f );
			flCritDamage = ( TF_DAMAGE_MINICRIT_MULTIPLIER - 1.f ) * flDamage;

			bitsDamage |= DMG_CRITICAL;
			info.AddDamageType( DMG_CRITICAL );

			// Any condition assist stats to send out?
			if ( eDamageBonusCond < TF_COND_LAST )
			{
				if ( pVictim )
				{
					pProvider = ToTFPlayer( pVictim->m_Shared.GetConditionProvider( eDamageBonusCond ) );
					if ( pProvider )
					{
						// SEALTODO
						//CTF_GameStats.Event_PlayerDamageAssist( pProvider, flCritDamage + flBonusDamage );
					}
				}
				if ( pTFAttacker )
				{
					pProvider = ToTFPlayer( pTFAttacker->m_Shared.GetConditionProvider( eDamageBonusCond ) );
					if ( pProvider && pProvider != pTFAttacker )
					{
						// SEALTODO
						//CTF_GameStats.Event_PlayerDamageAssist( pProvider, flCritDamage + flBonusDamage );
					}
				}
			}

			if ( bDebug )
			{
				Warning( "reduced to %.2f before crit mult\n", flDamage );
			}
		}

		if ( bCrit )
		{
			if ( info.GetCritType() != CTakeDamageInfo::CRIT_MINI  )
			{
				COMPILE_TIME_ASSERT( TF_DAMAGE_CRIT_MULTIPLIER > 1.f );
				flCritDamage = ( TF_DAMAGE_CRIT_MULTIPLIER - 1.f ) * flDamage;
			}

			if ( bDebug )
			{
				Warning( "    CRITICAL! Damage: %.2f\n", flDamage );
			}

			// Burn sounds are handled in ConditionThink()
			if ( !(bitsDamage & DMG_BURN ) && pVictim )
			{
				pVictim->SpeakConceptIfAllowed( MP_CONCEPT_HURT, "damagecritical:1" );
			}

			if ( pTFAttacker && pTFAttacker->m_Shared.IsCritBoosted() )
			{
				pProvider = ToTFPlayer( pTFAttacker->m_Shared.GetConditionProvider( TF_COND_CRITBOOSTED ) );
				if ( pProvider && pTFAttacker && pProvider != pTFAttacker )
				{
					// SEALTODO
					//CTF_GameStats.Event_PlayerDamageAssist( pProvider, flCritDamage + flBonusDamage );	
				}
			}
		}
		
		if ( pAttacker && pAttacker->IsPlayer() )
		{
			// Modify damage based on bonuses
			flDamage *= pTFAttacker->m_Shared.GetTmpDamageBonus();
		}

		// Store the extra damage and update actual damage
		if ( bCrit || info.GetCritType() == CTakeDamageInfo::CRIT_MINI  )
		{
			info.SetDamageBonus( flCritDamage + flBonusDamage, pProvider );	// Order-of-operations sensitive, but fine as long as TF_COND_CRITBOOSTED is last
		}

		// Crit-A-Cola and Steak Sandwich - only increase normal damage
		if ( pVictim && pVictim->m_Shared.InCond( TF_COND_ENERGY_BUFF ) && !bCrit && info.GetCritType() != CTakeDamageInfo::CRIT_MINI  )
		{
			float flDmgMult = 1.f;
			//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim, flDmgMult, energy_buff_dmg_taken_multiplier );
			flDamage *= flDmgMult;
		}

		info.SetDamage( flDamage + flCritDamage );
	}

	if ( pTFAttacker && pTFAttacker->IsPlayerClass( TF_CLASS_SPY ) )
	{
		if ( pTFAttacker->GetActiveWeapon() )
		{
			int iAddCloakOnHit = 0;
			//CALL_ATTRIB_HOOK_INT_ON_OTHER( pTFAttacker->GetActiveWeapon(), iAddCloakOnHit, add_cloak_on_hit );
			if ( iAddCloakOnHit > 0 )
			{
				pTFAttacker->m_Shared.AddToSpyCloakMeter( iAddCloakOnHit, true );
			}
		}
	}


	// Apply on-hit attributes
	if ( pVictim && pAttacker && pAttacker->GetTeam() != pVictim->GetTeam() && pAttacker->IsPlayer() && pWeapon )
	{
		//pWeapon->ApplyOnHitAttributes( pVictimBaseEntity, pTFAttacker, info );
	}

	// Give assist points to the provider of any stun on the victim - up to half the damage, based on the amount of stun
	if ( pVictim && pVictim->m_Shared.InCond( TF_COND_STUNNED ) )
	{
		CTFPlayer *pProvider = ToTFPlayer( pVictim->m_Shared.GetConditionProvider( TF_COND_STUNNED ) );
		if ( pProvider && pTFAttacker && pProvider != pTFAttacker )
		{
			float flStunAmount = pVictim->m_Shared.GetAmountStunned( TF_STUN_MOVEMENT );
			if ( flStunAmount < 1.f && pVictim->m_Shared.IsControlStunned() )
				flStunAmount = 1.f;

			int nAssistPoints = RemapValClamped( flStunAmount, 0.1f, 1.f, 1, ( info.GetDamage() / 2 ) );
			if ( nAssistPoints )
			{
				// SEALTODO
				//CTF_GameStats.Event_PlayerDamageAssist( pProvider, nAssistPoints );	
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static bool CheckForDamageTypeImmunity( int nDamageType, CTFPlayer* pVictim, float &flDamageBase, float &flCritBonusDamage )
{
	bool bImmune = false;
	if( nDamageType & (DMG_BURN|DMG_IGNITE) )
	{
		bImmune = pVictim->m_Shared.InCond( TF_COND_FIRE_IMMUNE );
	}
	else if( nDamageType & (DMG_BULLET|DMG_BUCKSHOT) )
	{
		bImmune = pVictim->m_Shared.InCond( TF_COND_BULLET_IMMUNE );
	}
	else if( nDamageType & DMG_BLAST )
	{
		bImmune = pVictim->m_Shared.InCond( TF_COND_BLAST_IMMUNE );
	}

	if( bImmune )
	{
		flDamageBase = flCritBonusDamage = 0.f;

		IGameEvent* event = gameeventmanager->CreateEvent( "damage_resisted" );
		if ( event )
		{
			event->SetInt( "entindex", pVictim->entindex() );
			gameeventmanager->FireEvent( event ); 
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static bool CheckMedicResist( ETFCond ePassiveCond, ETFCond eDeployedCond, CTFPlayer* pVictim, float flRawDamage, float& flDamageBase, bool bCrit, float& flCritBonusDamage )
{
	// Might be a tank or some other object that's getting shot
	if( !pVictim || !pVictim->IsPlayer() )
		return false;

	ETFCond eActiveCond;
	// Be in the condition!
	if( pVictim->m_Shared.InCond( eDeployedCond ) )
	{
		eActiveCond = eDeployedCond;
	}
	else if( pVictim->m_Shared.InCond( ePassiveCond ) )
	{
		eActiveCond = ePassiveCond;
	}
	else
	{
		return false;
	}

	// Get our condition provider
	CBaseEntity* pProvider = pVictim->m_Shared.GetConditionProvider( eActiveCond );
	CTFPlayer* pTFProvider = ToTFPlayer( pProvider );
	Assert( pTFProvider );
	
	float flDamageScale = 0.f;
	//float flCritBarDeplete = 0.f;
	bool bUberResist = false;

	if( pTFProvider )
	{
		switch( eActiveCond )
		{
		case TF_COND_MEDIGUN_UBER_BULLET_RESIST:
			//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFProvider, flDamageScale, medigun_bullet_resist_deployed );
			bUberResist = true;
//			if ( bCrit )
//			{
//				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFProvider, flCritBarDeplete, medigun_crit_bullet_percent_bar_deplete );
//			}
			break;

		case TF_COND_MEDIGUN_SMALL_BULLET_RESIST:
			//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFProvider, flDamageScale, medigun_bullet_resist_passive );
			break;

		case TF_COND_MEDIGUN_UBER_BLAST_RESIST:
			//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFProvider, flDamageScale, medigun_blast_resist_deployed );
			bUberResist = true;
			//if( bCrit )
			//{
			//	CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFProvider, flCritBarDeplete, medigun_crit_blast_percent_bar_deplete );
			//}
			break;

		case TF_COND_MEDIGUN_SMALL_BLAST_RESIST:
			//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFProvider, flDamageScale, medigun_blast_resist_passive );
			break;

		case TF_COND_MEDIGUN_UBER_FIRE_RESIST:
			//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFProvider, flDamageScale, medigun_fire_resist_deployed );
			bUberResist = true;
//			if( bCrit )
//			{
//				CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFProvider, flCritBarDeplete, medigun_crit_fire_percent_bar_deplete );
//			}
			break;

		case TF_COND_MEDIGUN_SMALL_FIRE_RESIST:
			//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFProvider, flDamageScale, medigun_fire_resist_passive );
			break;
		}
	}

	if ( bUberResist && pVictim->m_Shared.InCond( TF_COND_HEALING_DEBUFF ) )
	{
		flDamageScale *= 0.75f;
	}

	flDamageScale = 1.f - flDamageScale;
	if( flDamageScale < 0.f ) 
		flDamageScale = 0.f;

	//// Dont let the medic heal themselves when they take damage
	//if( pTFProvider && pTFProvider != pVictim )
	//{
	//	// Heal the medic for 10% of the incoming damage
	//	int nHeal = flRawDamage * 0.10f;
	//	// Heal!
	//	int iHealed = pTFProvider->TakeHealth( nHeal, DMG_GENERIC );

	//	// Tell them about it!
	//	if ( iHealed )
	//	{
	//		CTF_GameStats.Event_PlayerHealedOther( pTFProvider, iHealed );
	//	}

	//	IGameEvent *event = gameeventmanager->CreateEvent( "player_healonhit" );
	//	if ( event )
	//	{
	//		event->SetInt( "amount", nHeal );
	//		event->SetInt( "entindex", pTFProvider->entindex() );
	//		gameeventmanager->FireEvent( event ); 
	//	}
	//}

	IGameEvent* event = gameeventmanager->CreateEvent( "damage_resisted" );
	if ( event )
	{
		event->SetInt( "entindex", pVictim->entindex() );
		gameeventmanager->FireEvent( event ); 
	}

	if ( bCrit && pTFProvider && bUberResist )
	{
		flCritBonusDamage = ( pVictim->m_Shared.InCond( TF_COND_HEALING_DEBUFF ) ) ? flCritBonusDamage *= 0.25f : 0.f;

		//CWeaponMedigun* pMedigun = dynamic_cast<CWeaponMedigun*>( pTFProvider->Weapon_OwnsThisID( TF_WEAPON_MEDIGUN ) );
		//if( pMedigun )
		//{
		//	if( pMedigun->GetChargeLevel() > 0.f && pMedigun->IsReleasingCharge() )
		//	{
		//		flCritBonusDamage = 0;
		//	}
		//	pMedigun->SubtractCharge( flCritBarDeplete );
		//}
	}

	// Scale the damage!
	flDamageBase = flDamageBase * flDamageScale;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void PotentiallyFireDamageMitigatedEvent( const CTFPlayer* pMitigator, const CTFPlayer* pDamaged, float flBeforeDamage, float flAfterDamage )
{
	int nAmount = flBeforeDamage - flAfterDamage;
	// Nothing mitigated!
	if ( nAmount == 0 )
		return;

	IGameEvent *pEvent = gameeventmanager->CreateEvent( "damage_mitigated" );
	if ( pEvent )
	{
		pEvent->SetInt( "mitigator", pMitigator ? pMitigator->GetUserID() : -1 );
		pEvent->SetInt( "damaged", pDamaged ? pDamaged->GetUserID() : -1 );
		pEvent->SetInt( "amount", nAmount );
		gameeventmanager->FireEvent( pEvent, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFGameRules::ApplyOnDamageAliveModifyRules( const CTakeDamageInfo &info, CBaseEntity *pVictimBaseEntity, DamageModifyExtras_t& outParams )
{
	CTFPlayer *pVictim = ToTFPlayer( pVictimBaseEntity );
	CBaseEntity *pAttacker = info.GetAttacker();
	CTFPlayer *pTFAttacker = ToTFPlayer( pAttacker );

	float flRealDamage = info.GetDamage();

	if ( pVictimBaseEntity && pVictimBaseEntity->m_takedamage != DAMAGE_EVENTS_ONLY && pVictim )
	{
		int iDamageTypeBits = info.GetDamageType() & DMG_IGNITE;

		// Handle attributes that want to change our damage type, but only if we're taking damage from a non-DOT. This
		// stops fire DOT damage from constantly reigniting us. This will also prevent ignites from happening on the
		// damage *from-a-bleed-DOT*, but not from the bleed application attack.
		if ( !IsDOTDmg( info.GetDamageCustom() ) )
		{
			int iAddBurningDamageType = 0;
			//CALL_ATTRIB_HOOK_INT_ON_OTHER( info.GetWeapon(), iAddBurningDamageType, set_dmgtype_ignite );
			if ( iAddBurningDamageType )
			{
				iDamageTypeBits |= DMG_IGNITE;
			}
		}

		// Start burning if we took ignition damage
		outParams.bIgniting = ( ( iDamageTypeBits & DMG_IGNITE ) && ( !pVictim || pVictim->GetWaterLevel() < WL_Waist ) );

		if ( outParams.bIgniting && pVictim )
		{
			if ( pVictim->m_Shared.InCond( TF_COND_DISGUISED ) )
			{
				int iDisguiseNoBurn = 0;
				//CALL_ATTRIB_HOOK_INT_ON_OTHER( pVictim, iDisguiseNoBurn, disguise_no_burn );
				if ( iDisguiseNoBurn == 1 )
				{
					// Do a hard out in the caller
					return -1;
				}
			}

			if ( pVictim->m_Shared.InCond( TF_COND_FIRE_IMMUNE ) )
			{
				// Do a hard out in the caller
				return -1;
			}
		}

		// When obscured by smoke, attacks have a chance to miss
		if ( pVictim && pVictim->m_Shared.InCond( TF_COND_OBSCURED_SMOKE ) )
		{
			if ( RandomInt( 1, 4 ) >= 2 )
			{
				flRealDamage = 0.f;

				pVictim->SpeakConceptIfAllowed( MP_CONCEPT_DODGE_SHOT );

				if ( pTFAttacker )
				{
					CEffectData	data;
					data.m_nHitBox = GetParticleSystemIndex( "miss_text" );
					data.m_vOrigin = pVictim->WorldSpaceCenter() + Vector( 0.f , 0.f, 32.f );
					data.m_vAngles = vec3_angle;
					data.m_nEntIndex = 0;

					CSingleUserRecipientFilter filter( pTFAttacker );
					te->DispatchEffect( filter, 0.f, data.m_vOrigin, "ParticleEffect", data );
				}

				// No damage
				return -1.f;
			}
		}

		// Proc invicibility upon being hit
		float flUberChance = 0.f;
		//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim, flUberChance, uber_on_damage_taken );
		if( RandomFloat() < flUberChance )
		{
			pVictim->m_Shared.AddCond( TF_COND_INVULNERABLE_CARD_EFFECT, 3.f );
			// Make sure we don't take any damage
			flRealDamage = 0.f;
		}

		// Resists and Boosts
		float flDamageBonus = info.GetDamageBonus();
		float flDamageBase = flRealDamage - flDamageBonus;
		Assert( flDamageBase >= 0.f );

		int iPierceResists = 0;
		//CALL_ATTRIB_HOOK_INT_ON_OTHER( info.GetWeapon(), iPierceResists, mod_pierce_resists_absorbs );

		// This raw damage wont get scaled.  Used for determining how much health to give resist medics.
		float flRawDamage = flDamageBase;
		
		// Check if we're immune
		outParams.bPlayDamageReductionSound = CheckForDamageTypeImmunity( info.GetDamageType(), pVictim, flDamageBase, flDamageBonus );

		if ( !iPierceResists )
		{
			// Reduce only the crit portion of the damage with crit resist
			bool bCrit = ( info.GetDamageType() & DMG_CRITICAL ) > 0;
			if ( bCrit )
			{
				// Break the damage down and reassemble
				//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim, flDamageBonus, mult_dmgtaken_from_crit );
			}

			// Apply general dmg type reductions. Should we only ever apply one of these? (Flaregun is DMG_BULLET|DMG_IGNITE, for instance)
			if ( info.GetDamageType() & (DMG_BURN|DMG_IGNITE) )
			{
				//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim, flDamageBase, mult_dmgtaken_from_fire );
				//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim->GetActiveWeapon(), flDamageBase, mult_dmgtaken_from_fire_active );

				// Check for medic resist
				outParams.bPlayDamageReductionSound = CheckMedicResist( TF_COND_MEDIGUN_SMALL_FIRE_RESIST, TF_COND_MEDIGUN_UBER_FIRE_RESIST, pVictim, flRawDamage, flDamageBase, bCrit, flDamageBonus );
			}

			if ( pTFAttacker && pVictim && pVictim->m_Shared.InCond( TF_COND_BURNING ) )
			{
				//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFAttacker->GetActiveWeapon(), flDamageBase, mult_dmg_vs_burning );
			}

			if ( (info.GetDamageType() & (DMG_BLAST) ) )
			{
				bool bReduceBlast = false;

				// If someone else shot us or we're in MvM
				if( pAttacker != pVictimBaseEntity /*|| IsMannVsMachineMode()*/ )
				{
					bReduceBlast = true;
				}

				if ( bReduceBlast )
				{
					//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim, flDamageBase, mult_dmgtaken_from_explosions );

					// Check for medic resist
					outParams.bPlayDamageReductionSound = CheckMedicResist( TF_COND_MEDIGUN_SMALL_BLAST_RESIST, TF_COND_MEDIGUN_UBER_BLAST_RESIST, pVictim, flRawDamage, flDamageBase, bCrit, flDamageBonus );
				}
			}

			if ( info.GetDamageType() & (DMG_BULLET|DMG_BUCKSHOT) )
			{
				//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim, flDamageBase, mult_dmgtaken_from_bullets );

				// Check for medic resist
				outParams.bPlayDamageReductionSound = CheckMedicResist( TF_COND_MEDIGUN_SMALL_BULLET_RESIST, TF_COND_MEDIGUN_UBER_BULLET_RESIST, pVictim, flRawDamage, flDamageBase, bCrit, flDamageBonus );
			}

			if ( info.GetDamageType() & DMG_MELEE )
			{
				//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim, flDamageBase, mult_dmgtaken_from_melee );
			}

			if ( pVictim->IsPlayerClass( TF_CLASS_HEAVYWEAPONS ) && pVictim->m_Shared.InCond( TF_COND_AIMING ) && ( ( pVictim->GetHealth() - flRealDamage ) / pVictim->GetMaxHealth() ) <= 0.5f )
			{
				float flOriginalDamage = flDamageBase;
				//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim, flDamageBase, spunup_damage_resistance );
				if ( flOriginalDamage != flDamageBase )
				{
					pVictim->PlayDamageResistSound( flOriginalDamage, flDamageBase );
				}
			}
		}

		// If the damage changed at all play the resist sound
		if ( flDamageBase != flRawDamage )
		{
			outParams.bPlayDamageReductionSound = true;
		}

		// Stomp flRealDamage with resist adjusted values
		flRealDamage = flDamageBase + flDamageBonus;

		// End Resists

		// Increased damage taken from all sources
		//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim, flRealDamage, mult_dmgtaken );

		//if ( info.GetInflictor() && info.GetInflictor()->IsBaseObject() )
		//{
		//	CObjectSentrygun* pSentry = dynamic_cast<CObjectSentrygun*>( info.GetInflictor() );
		//	if ( pSentry )
		//	{
		//		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim, flRealDamage, dmg_from_sentry_reduced );
		//	}
		//}

		// Heavy rage-based knockback+stun effect that also reduces their damage output
		//if ( pTFAttacker && pTFAttacker->IsPlayerClass( TF_CLASS_HEAVYWEAPONS ) )
		//{
		//	int iRage = 0;
		//	CALL_ATTRIB_HOOK_INT_ON_OTHER( pTFAttacker, iRage, generate_rage_on_dmg );
		//	if ( iRage && pTFAttacker->m_Shared.IsRageDraining() )
		//	{
		//		flRealDamage *= 0.5f;
		//	}
		//}

		//if ( pVictim && pTFAttacker && info.GetWeapon() )
		//{
		//	CTFWeaponBase *pWeapon = pTFAttacker->GetActivePCWeapon();
		//	if ( pWeapon && pWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE && info.GetWeapon() == pWeapon )
		//	{
		//		CTFSniperRifle *pRifle = static_cast< CTFSniperRifle* >( info.GetWeapon() );

		//		float flStun = 1.0f;
		//		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pRifle, flStun, applies_snare_effect );
		//		if ( flStun != 1.0f )
		//		{
		//			float flDuration = pRifle->GetJarateTime();
		//			pVictim->m_Shared.StunPlayer( flDuration, flStun, TF_STUN_MOVEMENT, pTFAttacker );
		//		}
		//	}
		//}

		//if ( pVictim && pVictim->GetActivePCWeapon() )
		//{
		//	if ( info.GetDamageType() & (DMG_CLUB) )
		//	{
		//		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim->GetActivePCWeapon(), flRealDamage, dmg_from_melee );
		//	}
		//	else if ( info.GetDamageType() & (DMG_BLAST|DMG_BULLET|DMG_BUCKSHOT|DMG_IGNITE|DMG_SONIC) )
		//	{
		//		float flBeforeDamage = flRealDamage;
		//		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pVictim->GetActivePCWeapon(), flRealDamage, dmg_from_ranged );
		//		PotentiallyFireDamageMitigatedEvent( pVictim, pVictim, pVictim->GetActivePCWeapon(), flBeforeDamage, flRealDamage );
		//	}
		//}

		outParams.bSendPreFeignDamage = false;
		if ( pVictim && pVictim->IsPlayerClass( TF_CLASS_SPY ) && ( info.GetDamageCustom() != TF_DMG_CUSTOM_TELEFRAG ) && !pVictim->IsTaunting() )
		{
			// STAGING_SPY
			// Reduce damage taken if we have recently feigned death.
			//if ( pVictim->m_Shared.InCond( TF_COND_FEIGN_DEATH ) || pVictim->m_Shared.IsFeignDeathReady() )
			//{
			//	// Damage reduction is proportional to cloak remaining (60%->20%)
			//	float flDamageReduction = RemapValClamped( pVictim->m_Shared.GetSpyCloakMeter(), 50.0f, 0.0f, tf_feign_death_damage_scale.GetFloat(), tf_stealth_damage_reduction.GetFloat() );

			//	// On Activate Reduce Remaining Cloak by 50%
			//	if ( pVictim->m_Shared.IsFeignDeathReady() )
			//	{
			//		flDamageReduction = tf_feign_death_activate_damage_scale.GetFloat();
			//	}
			//	outParams.bSendPreFeignDamage = true;

			//	float flBeforeflRealDamage = flRealDamage;

			//	flRealDamage *= flDamageReduction;

			//	CTFWeaponInvis *pWatch = (CTFWeaponInvis *) pVictim->Weapon_OwnsThisID( TF_WEAPON_INVIS );
			//	PotentiallyFireDamageMitigatedEvent( pVictim, pVictim, pWatch, flBeforeflRealDamage, flRealDamage );

			//	// Original damage would've killed the player, but the reduced damage wont
			//	if ( flBeforeflRealDamage >= pVictim->GetHealth() && flRealDamage < pVictim->GetHealth() )
			//	{
			//		IGameEvent *pEvent = gameeventmanager->CreateEvent( "deadringer_cheat_death" );
			//		if ( pEvent )
			//		{
			//			pEvent->SetInt( "spy", pVictim->GetUserID() );
			//			pEvent->SetInt( "attacker", pTFAttacker ? pTFAttacker->GetUserID() : -1 );
			//			gameeventmanager->FireEvent( pEvent, true );
			//		}
			//	}
			//}
			// Standard Stealth gives small damage reduction
			/*else*/ if ( pVictim->m_Shared.InCond( TF_COND_STEALTHED ) )
			{
				flRealDamage *= tf_stealth_damage_reduction.GetFloat();
			}
		}

		if ( flRealDamage == 0.0f )
		{
			// Do a hard out in the caller
			return -1;
		}

		if ( pAttacker == pVictimBaseEntity && (info.GetDamageType() & DMG_BLAST) &&
			 info.GetDamagedOtherPlayers() == 0 && (info.GetDamageCustom() != TF_DMG_CUSTOM_TAUNTATK_GRENADE) )
		{
			// If we attacked ourselves, hurt no other players, and it is a blast,
			// check the attribute that reduces rocket jump damage.
			//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( info.GetAttacker(), flRealDamage, rocket_jump_dmg_reduction );
			outParams.bSelfBlastDmg = true;
		}

		if ( pAttacker == pVictimBaseEntity )
		{
			enum
			{
				kSelfBlastResponse_IgnoreProjectilesFromThisWeapon = 1,		// the sticky jumper doesn't disable damage from other explosive weapons
				kSelfBlastResponse_IgnoreProjectilesFromAllWeapons = 2,		// the rocket jumper doesn't have a special projectile type and so ignores all self-inflicted damage from explosive sources
			};

			if ( info.GetWeapon() )
			{
				int iNoSelfBlastDamage = 0;
				//CALL_ATTRIB_HOOK_INT_ON_OTHER( info.GetWeapon(), iNoSelfBlastDamage, no_self_blast_dmg );

				const bool bIgnoreThisSelfDamage = ( iNoSelfBlastDamage == kSelfBlastResponse_IgnoreProjectilesFromAllWeapons )
					|| ( (iNoSelfBlastDamage == kSelfBlastResponse_IgnoreProjectilesFromThisWeapon) && (info.GetDamageCustom() == TF_DMG_CUSTOM_PRACTICE_STICKY) );
				if ( bIgnoreThisSelfDamage )
				{
					flRealDamage = 0;
				}

				//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( info.GetWeapon(), flRealDamage, blast_dmg_to_self );
			}
		}

		if ( pTFAttacker && ( pTFAttacker != pVictim ) )
		{
			//int iHypeOnDamage = 0;
			//CALL_ATTRIB_HOOK_INT_ON_OTHER( pTFAttacker, iHypeOnDamage, hype_on_damage );
			//if ( iHypeOnDamage )
			//{
			//	float flHype = RemapValClamped( flRealDamage, 1.f, 200.f, 1.f, 50.f );
			//	pTFAttacker->m_Shared.SetScoutHypeMeter( Min( 100.f, flHype + pTFAttacker->m_Shared.GetScoutHypeMeter() ) );
			//}
		}
	}

	return flRealDamage;
}

// --------------------------------------------------------------------------------------------------- //
// Voice helper
// --------------------------------------------------------------------------------------------------- //

class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	virtual bool		CanPlayerHearPlayer( CBasePlayer *pListener, CBasePlayer *pTalker, bool &bProximity )
	{
		// Dead players can only be heard by other dead team mates but only if a match is in progress
		if ( TFGameRules()->State_Get() != GR_STATE_TEAM_WIN && TFGameRules()->State_Get() != GR_STATE_GAME_OVER ) 
		{
			if ( pTalker->IsAlive() == false )
			{
				if ( pListener->IsAlive() == false || tf_gravetalk.GetBool() )
					return ( pListener->InSameTeam( pTalker ) );

				return false;
			}
		}

		return ( pListener->InSameTeam( pTalker ) );
	}
};
CVoiceGameMgrHelper g_VoiceGameMgrHelper;
IVoiceGameMgrHelper *g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;

// Load the objects.txt file.
class CObjectsFileLoad : public CAutoGameSystem
{
public:
	virtual bool Init()
	{
		LoadObjectInfos( filesystem );
		return true;
	}
} g_ObjectsFileLoad;

// --------------------------------------------------------------------------------------------------- //
// Globals.
// --------------------------------------------------------------------------------------------------- //
/*
// NOTE: the indices here must match TEAM_UNASSIGNED, TEAM_SPECTATOR, TF_TEAM_RED, TF_TEAM_BLUE, etc.
char *sTeamNames[] =
{
	"Unassigned",
	"Spectator",
	"Red",
	"Blue"
};
*/
// --------------------------------------------------------------------------------------------------- //
// Global helper functions.
// --------------------------------------------------------------------------------------------------- //
	
// World.cpp calls this but we don't use it in TF.
void InitBodyQue()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFGameRules::~CTFGameRules()
{
	// Note, don't delete each team since they are in the gEntList and will 
	// automatically be deleted from there, instead.
	TFTeamMgr()->Shutdown();
	ShutdownCustomResponseRulesDicts();
}

//-----------------------------------------------------------------------------
// Purpose: TF2 Specific Client Commands
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CTFGameRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
{
	CTFPlayer *pPlayer = ToTFPlayer( pEdict );

	const char *pcmd = args[0];
	if ( FStrEq( pcmd, "objcmd" ) )
	{
		if ( args.ArgC() < 3 )
			return true;

		int entindex = atoi( args[1] );
		edict_t* pEdict = INDEXENT(entindex);
		if ( pEdict )
		{
			CBaseEntity* pBaseEntity = GetContainingEntity(pEdict);
			CBaseObject* pObject = dynamic_cast<CBaseObject*>(pBaseEntity);

			if ( pObject )
			{
				// We have to be relatively close to the object too...

				// BUG! Some commands need to get sent without the player being near the object, 
				// eg delayed dismantle commands. Come up with a better way to ensure players aren't
				// entering these commands in the console.

				//float flDistSq = pObject->GetAbsOrigin().DistToSqr( pPlayer->GetAbsOrigin() );
				//if (flDistSq <= (MAX_OBJECT_SCREEN_INPUT_DISTANCE * MAX_OBJECT_SCREEN_INPUT_DISTANCE))
				{
					// Strip off the 1st two arguments and make a new argument string
					CCommand objectArgs( args.ArgC() - 2, &args.ArgV()[2]);
					pObject->ClientCommand( pPlayer, objectArgs );
				}
			}
		}

		return true;
	}

	// Handle some player commands here as they relate more directly to gamerules state
	if ( FStrEq( pcmd, "nextmap" ) )
	{
		if ( pPlayer->m_flNextTimeCheck < gpGlobals->curtime )
		{
			char szNextMap[32];

			if ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) )
			{
				Q_strncpy( szNextMap, nextlevel.GetString(), sizeof( szNextMap ) );
			}
			else
			{
				GetNextLevelName( szNextMap, sizeof( szNextMap ) );
			}

			ClientPrint( pPlayer, HUD_PRINTTALK, "#TF_nextmap", szNextMap);

			pPlayer->m_flNextTimeCheck = gpGlobals->curtime + 1;
		}

		return true;
	}
	else if ( FStrEq( pcmd, "timeleft" ) )
	{	
		if ( pPlayer->m_flNextTimeCheck < gpGlobals->curtime )
		{
			if ( mp_timelimit.GetInt() > 0 )
			{
				int iTimeLeft = GetTimeLeft();

				char szMinutes[5];
				char szSeconds[3];

				if ( iTimeLeft <= 0 )
				{
					Q_snprintf( szMinutes, sizeof(szMinutes), "0" );
					Q_snprintf( szSeconds, sizeof(szSeconds), "00" );
				}
				else
				{
					Q_snprintf( szMinutes, sizeof(szMinutes), "%d", iTimeLeft / 60 );
					Q_snprintf( szSeconds, sizeof(szSeconds), "%02d", iTimeLeft % 60 );
				}				

				ClientPrint( pPlayer, HUD_PRINTTALK, "#TF_timeleft", szMinutes, szSeconds );
			}
			else
			{
				ClientPrint( pPlayer, HUD_PRINTTALK, "#TF_timeleft_nolimit" );
			}

			pPlayer->m_flNextTimeCheck = gpGlobals->curtime + 1;
		}
		return true;
	}
	else if( pPlayer->ClientCommand( args ) )
	{
        return true;
	}

	return BaseClass::ClientCommand( pEdict, args );
}

void CTFGameRules::RadiusStun( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius )
{
	CBaseEntity *pEntity = NULL;
	trace_t		tr;
	float		flAdjustedDamage, falloff;
	Vector		vecSpot;
	Vector		vecToTarget;

	if ( flRadius )
		falloff = info.GetDamage() / flRadius;
	else
		falloff = 1.0;

	// ok, now send updates to all clients
	CBitVec< ABSOLUTE_PLAYER_LIMIT > playerbits;
	playerbits.ClearAll();

	// see which players are actually in the PVS of the grenade
	engine->Message_DetermineMulticastRecipients( false, vecSrc, playerbits );

	// Iterate through all players that made it into playerbits, that are inside the radius
	// and give them stun damage
	for ( int i=0;i<MAX_PLAYERS;i++ )
	{
		if ( playerbits.Get(i) == false )
			continue;

		pEntity = UTIL_EntityByIndex( i+1 );

		if ( !pEntity || !pEntity->IsPlayer() )
			continue;

		if ( pEntity->m_takedamage != DAMAGE_NO )
		{
			// radius damage can only be blocked by the world
			vecSpot = pEntity->BodyTarget( vecSrc );

			// the explosion can 'see' this entity, so hurt them!
			vecToTarget = ( vecSpot - vecSrc );

			float flDist = vecToTarget.Length();

			// make sure they are inside the radius
			if ( flDist > flRadius )
				continue;

			// decrease damage for an ent that's farther from the bomb.
			flAdjustedDamage = flDist * falloff;
			flAdjustedDamage = info.GetDamage() - flAdjustedDamage;

			if ( flAdjustedDamage > 0 )
			{
				CTakeDamageInfo adjustedInfo = info;
				adjustedInfo.SetDamage( flAdjustedDamage );

				pEntity->TakeDamage( adjustedInfo );
			}
		}
	}
}

// Add the ability to ignore the world trace
void CTFGameRules::Think()
{
	if ( !g_fGameOver )
	{
		if ( gpGlobals->curtime > m_flNextPeriodicThink )
		{
			if ( State_Get() != GR_STATE_TEAM_WIN )
			{
				if ( CheckCapsPerRound() )
					return;
			}

			// DOD
			CheckPlayerPositions();
		}
	}

	BaseClass::Think();
}

//Runs think for all player's conditions
//Need to do this here instead of the player so players that crash still run their important thinks
void CTFGameRules::RunPlayerConditionThink ( void )
{
	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer )
		{
			pPlayer->m_Shared.ConditionGameRulesThink();
		}
	}
}

void CTFGameRules::FrameUpdatePostEntityThink()
{
	BaseClass::FrameUpdatePostEntityThink();

	RunPlayerConditionThink();
}

bool CTFGameRules::CheckCapsPerRound()
{
	if ( tf_flag_caps_per_round.GetInt() > 0 )
	{
		int iMaxCaps = -1;
		CTFTeam *pMaxTeam = NULL;

		// check to see if any team has won a "round"
		int nTeamCount = TFTeamMgr()->GetTeamCount();
		for ( int iTeam = FIRST_GAME_TEAM; iTeam < nTeamCount; ++iTeam )
		{
			CTFTeam *pTeam = GetGlobalTFTeam( iTeam );
			if ( !pTeam )
				continue;

			// we might have more than one team over the caps limit (if the server op lowered the limit)
			// so loop through to see who has the most among teams over the limit
			if ( pTeam->GetFlagCaptures() >= tf_flag_caps_per_round.GetInt() )
			{
				if ( pTeam->GetFlagCaptures() > iMaxCaps )
				{
					iMaxCaps = pTeam->GetFlagCaptures();
					pMaxTeam = pTeam;
				}
			}
		}

		if ( iMaxCaps != -1 && pMaxTeam != NULL )
		{
			SetWinningTeam( pMaxTeam->GetTeamNumber(), WINREASON_FLAG_CAPTURE_LIMIT );
			return true;
		}
	}

	return false;
}

bool CTFGameRules::CheckWinLimit()
{
	if ( mp_winlimit.GetInt() != 0 )
	{
		bool bWinner = false;

		if ( TFTeamMgr()->GetTeam( TF_TEAM_BLUE )->GetScore() >= mp_winlimit.GetInt() )
		{
			UTIL_LogPrintf( "Team \"BLUE\" triggered \"Intermission_Win_Limit\"\n" );
			bWinner = true;
		}
		else if ( TFTeamMgr()->GetTeam( TF_TEAM_RED )->GetScore() >= mp_winlimit.GetInt() )
		{
			UTIL_LogPrintf( "Team \"RED\" triggered \"Intermission_Win_Limit\"\n" );
			bWinner = true;
		}

		if ( bWinner )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "tf_game_over" );
			if ( event )
			{
				event->SetString( "reason", "Reached Win Limit" );
				gameeventmanager->FireEvent( event );
			}

			GoToIntermission();
			return true;
		}
	}

	return false;
}

bool CTFGameRules::IsInPreMatch() const
{
	// TFTODO    return (cb_prematch_time > gpGlobals->time)
	return false;
}

float CTFGameRules::GetPreMatchEndTime() const
{
	//TFTODO: implement this.
	return gpGlobals->curtime;
}

void CTFGameRules::GoToIntermission( void )
{
	CTF_GameStats.Event_GameEnd();

	BaseClass::GoToIntermission();
}

bool CTFGameRules::FPlayerCanTakeDamage(CBasePlayer *pPlayer, CBaseEntity *pAttacker, const CTakeDamageInfo &info)
{
	// guard against NULL pointers if players disconnect
	if ( !pPlayer || !pAttacker )
		return false;

	// if pAttacker is an object, we can only do damage if pPlayer is our builder
	if ( pAttacker->IsBaseObject() )
	{
		CBaseObject *pObj = ( CBaseObject *)pAttacker;

		if ( pObj->GetBuilder() == pPlayer || pPlayer->GetTeamNumber() != pObj->GetTeamNumber() )
		{
			// Builder and enemies
			return true;
		}
		else
		{
			// Teammates of the builder
			return false;
		}
	}

	return BaseClass::FPlayerCanTakeDamage(pPlayer, pAttacker, info);
}

Vector DropToGround( 
	CBaseEntity *pMainEnt, 
	const Vector &vPos, 
	const Vector &vMins, 
	const Vector &vMaxs )
{
	trace_t trace;
	UTIL_TraceHull( vPos, vPos + Vector( 0, 0, -500 ), vMins, vMaxs, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &trace );
	return trace.endpos;
}


void TestSpawnPointType( const char *pEntClassName )
{
	// Find the next spawn spot.
	CBaseEntity *pSpot = gEntList.FindEntityByClassname( NULL, pEntClassName );

	while( pSpot )
	{
		// trace a box here
		Vector vTestMins = pSpot->GetAbsOrigin() + VEC_HULL_MIN;
		Vector vTestMaxs = pSpot->GetAbsOrigin() + VEC_HULL_MAX;

		if ( UTIL_IsSpaceEmpty( pSpot, vTestMins, vTestMaxs ) )
		{
			// the successful spawn point's location
			NDebugOverlay::Box( pSpot->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 100, 60 );

			// drop down to ground
			Vector GroundPos = DropToGround( NULL, pSpot->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX );

			// the location the player will spawn at
			NDebugOverlay::Box( GroundPos, VEC_HULL_MIN, VEC_HULL_MAX, 0, 0, 255, 100, 60 );

			// draw the spawn angles
			QAngle spotAngles = pSpot->GetLocalAngles();
			Vector vecForward;
			AngleVectors( spotAngles, &vecForward );
			NDebugOverlay::HorzArrow( pSpot->GetAbsOrigin(), pSpot->GetAbsOrigin() + vecForward * 32, 10, 255, 0, 0, 255, true, 60 );
		}
		else
		{
			// failed spawn point location
			NDebugOverlay::Box( pSpot->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 100, 60 );
		}

		// increment pSpot
		pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
	}
}

// -------------------------------------------------------------------------------- //

void TestSpawns()
{
	TestSpawnPointType( "info_player_teamspawn" );
}
ConCommand cc_TestSpawns( "map_showspawnpoints", TestSpawns, "Dev - test the spawn points, draws for 60 seconds", FCVAR_CHEAT );

// -------------------------------------------------------------------------------- //

void cc_ShowRespawnTimes()
{
	CTFGameRules *pRules = TFGameRules();
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );

	if ( pRules && pPlayer )
	{
		float flRedMin = ( pRules->m_TeamRespawnWaveTimes[TF_TEAM_RED] >= 0 ? pRules->m_TeamRespawnWaveTimes[TF_TEAM_RED] : mp_respawnwavetime.GetFloat() );
		float flRedScalar = pRules->GetRespawnTimeScalar( TF_TEAM_RED );
		float flNextRedRespawn = pRules->GetNextRespawnWave( TF_TEAM_RED, NULL ) - gpGlobals->curtime;

		float flBlueMin = ( pRules->m_TeamRespawnWaveTimes[TF_TEAM_BLUE] >= 0 ? pRules->m_TeamRespawnWaveTimes[TF_TEAM_BLUE] : mp_respawnwavetime.GetFloat() );
		float flBlueScalar = pRules->GetRespawnTimeScalar( TF_TEAM_BLUE );
		float flNextBlueRespawn = pRules->GetNextRespawnWave( TF_TEAM_BLUE, NULL ) - gpGlobals->curtime;

		char tempRed[128];
		Q_snprintf( tempRed, sizeof( tempRed ),   "Red:  Min Spawn %2.2f, Scalar %2.2f, Next Spawn In: %.2f\n", flRedMin, flRedScalar, flNextRedRespawn );

		char tempBlue[128];
		Q_snprintf( tempBlue, sizeof( tempBlue ), "Blue: Min Spawn %2.2f, Scalar %2.2f, Next Spawn In: %.2f\n", flBlueMin, flBlueScalar, flNextBlueRespawn );

		ClientPrint( pPlayer, HUD_PRINTTALK, tempRed );
		ClientPrint( pPlayer, HUD_PRINTTALK, tempBlue );
	}
}

ConCommand mp_showrespawntimes( "mp_showrespawntimes", cc_ShowRespawnTimes, "Show the min respawn times for the teams" );

// -------------------------------------------------------------------------------- //

CBaseEntity *CTFGameRules::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	// get valid spawn point
	CBaseEntity *pSpawnSpot = pPlayer->EntSelectSpawnPoint();

	// drop down to ground
	Vector GroundPos = DropToGround( pPlayer, pSpawnSpot->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX );

	// Move the player to the place it said.
	pPlayer->SetLocalOrigin( GroundPos + Vector(0,0,1) );
	pPlayer->SetAbsVelocity( vec3_origin );
	pPlayer->SetLocalAngles( pSpawnSpot->GetLocalAngles() );
	pPlayer->m_Local.m_vecPunchAngle = vec3_angle;
	pPlayer->m_Local.m_vecPunchAngleVel = vec3_angle;
	pPlayer->SnapEyeAngles( pSpawnSpot->GetLocalAngles() );

	return pSpawnSpot;
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if the player is on the correct team and whether or
//          not the spawn point is available.
//-----------------------------------------------------------------------------
bool CTFGameRules::IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer, bool bIgnorePlayers )
{
	// Check the team.
	if ( pSpot->GetTeamNumber() != pPlayer->GetTeamNumber() )
		return false;

	if ( !pSpot->IsTriggered( pPlayer ) )
		return false;

	CTFTeamSpawn *pCTFSpawn = dynamic_cast<CTFTeamSpawn*>( pSpot );
	if ( pCTFSpawn )
	{
		if ( pCTFSpawn->IsDisabled() )
			return false;
	}

	Vector mins = GetViewVectors()->m_vHullMin;
	Vector maxs = GetViewVectors()->m_vHullMax;

	if ( !bIgnorePlayers )
	{
		Vector vTestMins = pSpot->GetAbsOrigin() + mins;
		Vector vTestMaxs = pSpot->GetAbsOrigin() + maxs;
		return UTIL_IsSpaceEmpty( pPlayer, vTestMins, vTestMaxs );
	}

	trace_t trace;
	UTIL_TraceHull( pSpot->GetAbsOrigin(), pSpot->GetAbsOrigin(), mins, maxs, MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
	return ( trace.fraction == 1 && trace.allsolid != 1 && (trace.startsolid != 1) );
}

Vector CTFGameRules::VecItemRespawnSpot( CItem *pItem )
{
	return pItem->GetOriginalSpawnOrigin();
}

QAngle CTFGameRules::VecItemRespawnAngles( CItem *pItem )
{
	return pItem->GetOriginalSpawnAngles();
}

float CTFGameRules::FlItemRespawnTime( CItem *pItem )
{
	return ITEM_RESPAWN_TIME;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTFGameRules::GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer )
{
	if ( !pPlayer )  // dedicated server output
	{
		return NULL;
	}

	const char *pszFormat = NULL;

	// team only
	if ( bTeamOnly == true )
	{
		if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		{
			pszFormat = "TF_Chat_Spec";
		}
		else
		{
			if ( pPlayer->IsAlive() == false && State_Get() != GR_STATE_TEAM_WIN )
			{
				pszFormat = "TF_Chat_Team_Dead";
			}
			else
			{
				const char *chatLocation = GetChatLocation( bTeamOnly, pPlayer );
				if ( chatLocation && *chatLocation )
				{
					pszFormat = "TF_Chat_Team_Loc";
				}
				else
				{
					pszFormat = "TF_Chat_Team";
				}
			}
		}
	}
	// everyone
	else
	{	
		if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		{
			pszFormat = "TF_Chat_AllSpec";	
		}
		else
		{
			if ( pPlayer->IsAlive() == false && State_Get() != GR_STATE_TEAM_WIN )
			{
				pszFormat = "TF_Chat_AllDead";
			}
			else
			{
				pszFormat = "TF_Chat_All";	
			}
		}
	}

	return pszFormat;
}

VoiceCommandMenuItem_t *CTFGameRules::VoiceCommand( CBaseMultiplayerPlayer *pPlayer, int iMenu, int iItem )
{
	VoiceCommandMenuItem_t *pItem = BaseClass::VoiceCommand( pPlayer, iMenu, iItem );

	if ( pItem )
	{
		int iActivity = ActivityList_IndexForName( pItem->m_szGestureActivity );

		if ( iActivity != ACT_INVALID )
		{
			CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );

			if ( pTFPlayer )
			{
				pTFPlayer->DoAnimationEvent( PLAYERANIMEVENT_VOICE_COMMAND_GESTURE, iActivity );
			}
		}
	}

	return pItem;
}

//-----------------------------------------------------------------------------
// Purpose: Actually change a player's name.  
//-----------------------------------------------------------------------------
void CTFGameRules::ChangePlayerName( CTFPlayer *pPlayer, const char *pszNewName )
{
	const char *pszOldName = pPlayer->GetPlayerName();

	CReliableBroadcastRecipientFilter filter;
	UTIL_SayText2Filter( filter, pPlayer, false, "#TF_Name_Change", pszOldName, pszNewName );

	IGameEvent * event = gameeventmanager->CreateEvent( "player_changename" );
	if ( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		event->SetString( "oldname", pszOldName );
		event->SetString( "newname", pszNewName );
		gameeventmanager->FireEvent( event );
	}

	pPlayer->SetPlayerName( pszNewName );

	pPlayer->m_flNextNameChangeTime = gpGlobals->curtime + 10.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::ClientSettingsChanged( CBasePlayer *pPlayer )
{
	const char *pszName = engine->GetClientConVarValue( pPlayer->entindex(), "name" );

	const char *pszOldName = pPlayer->GetPlayerName();

	CTFPlayer *pTFPlayer = (CTFPlayer*)pPlayer;

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	// Note, not using FStrEq so that this is case sensitive
	if ( pszOldName[0] != 0 && Q_strncmp( pszOldName, pszName, MAX_PLAYER_NAME_LENGTH-1 ) )		
	{
		if ( pTFPlayer->m_flNextNameChangeTime < gpGlobals->curtime )
		{
			ChangePlayerName( pTFPlayer, pszName );
		}
		else
		{
			// no change allowed, force engine to use old name again
			engine->ClientCommand( pPlayer->edict(), "name \"%s\"", pszOldName );

			// tell client that he hit the name change time limit
			ClientPrint( pTFPlayer, HUD_PRINTTALK, "#Name_change_limit_exceeded" );
		}
	}

	// keep track of their hud_classautokill value
	int nClassAutoKill = Q_atoi( engine->GetClientConVarValue( pPlayer->entindex(), "hud_classautokill" ) );
	pTFPlayer->SetHudClassAutoKill( nClassAutoKill > 0 ? true : false );

	// keep track of their tf_medigun_autoheal value
	pTFPlayer->SetMedigunAutoHeal( Q_atoi( engine->GetClientConVarValue( pPlayer->entindex(), "tf_medigun_autoheal" ) ) > 0 );

	// keep track of their cl_autorezoom value
	pTFPlayer->SetAutoRezoom( Q_atoi( engine->GetClientConVarValue( pPlayer->entindex(), "cl_autorezoom" ) ) > 0 );

	// keep track of their cl_autorezoom value
	pTFPlayer->SetAutoReload( Q_atoi( engine->GetClientConVarValue( pPlayer->entindex(), "cl_autoreload" ) ) > 0 );

	const char *pszFov = engine->GetClientConVarValue( pPlayer->entindex(), "fov_desired" );
	int iFov = atoi(pszFov);
	iFov = clamp( iFov, 75, 90 );
	pTFPlayer->SetDefaultFOV( iFov );

	pTFPlayer->m_bFlipViewModels = Q_strcmp( engine->GetClientConVarValue( pPlayer->entindex(), "cl_flipviewmodels" ), "1" ) == 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameRules::ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues )
{
	BaseClass::ClientCommandKeyValues( pEntity, pKeyValues );

	CTFPlayer *pPlayer = ToTFPlayer( CBaseEntity::Instance( pEntity ) );
	if ( !pPlayer )
		return;

	if ( FStrEq( pKeyValues->GetName(), "FreezeCamTaunt" ) )
	{
		int iAchieverIndex = pKeyValues->GetInt( "achiever" );
		CTFPlayer *pAchiever = ToTFPlayer( UTIL_PlayerByIndex( iAchieverIndex ) );

		if ( pAchiever && pAchiever != pPlayer )
		{
			int iClass = pAchiever->GetPlayerClass()->GetClassIndex();

			if ( g_TauntCamAchievements[iClass] != 0 )
			{
				pAchiever->AwardAchievement( g_TauntCamAchievements[iClass] );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: return true if this flag is currently allowed to be captured
//-----------------------------------------------------------------------------
bool CTFGameRules::CanFlagBeCaptured( CBaseEntity *pOther )
{
	if ( pOther && ( tf_flag_return_on_touch.GetBool() || IsPowerupMode() ) )
	{
		for ( int i = 0; i < ICaptureFlagAutoList::AutoList().Count(); ++i )
		{
			CCaptureFlag *pListFlag = static_cast<CCaptureFlag*>( ICaptureFlagAutoList::AutoList()[i] );
			if ( ( pListFlag->GetType() == TF_FLAGTYPE_CTF ) && !pListFlag->IsDisabled() && ( pOther->GetTeamNumber() == pListFlag->GetTeamNumber() ) && !pListFlag->IsHome() )
				return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: return true if the game is in a state where both flags are stolen and poisonous
//-----------------------------------------------------------------------------
bool CTFGameRules::PowerupModeFlagStandoffActive( void )
{
	if ( IsPowerupMode() )
	{
		int nQualifyingFlags = 0; // All flags need to be stolen and poisonous (poisonous time delay gives the flag carriers a chance to get out of the enemy base)
		int nEnabledFlags = 0; // Some flags might be in the autolist but be out of play. We don't want them included in the count
		for ( int i = 0; i < ICaptureFlagAutoList::AutoList().Count(); ++i )
		{
			CCaptureFlag *pFlag = static_cast<CCaptureFlag*>( ICaptureFlagAutoList::AutoList()[i] );
			if ( !pFlag->IsDisabled() )
				nEnabledFlags++;
			if ( pFlag->IsPoisonous() && pFlag->IsStolen() )
				nQualifyingFlags++;
		}
		if ( nQualifyingFlags == nEnabledFlags )
		{
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified player can carry any more of the ammo type
//-----------------------------------------------------------------------------
bool CTFGameRules::CanHaveAmmo( CBaseCombatCharacter *pPlayer, int iAmmoIndex )
{
	CTFPlayer *pTFPlayer = (CTFPlayer*)pPlayer;

	if ( pTFPlayer && ( pTFPlayer->IsDODClass() || pTFPlayer->IsCSClass() ) )
		return BaseClass::CanHaveAmmo( pPlayer, iAmmoIndex );

	if ( iAmmoIndex > -1 )
	{
		if ( pTFPlayer )
		{
			// Get the player class data - contains ammo counts for this class.
			TFPlayerClassData_t *pData = pTFPlayer->GetPlayerClass()->GetData();
			if ( pData )
			{
				// Get the max carrying capacity for this ammo
				int iMaxCarry = pData->m_aAmmoMax[iAmmoIndex];

				// Does the player have room for more of this type of ammo?
				if ( pTFPlayer->GetAmmoCount( iAmmoIndex ) < iMaxCarry )
				{
					return true;
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	// Find the killer & the scorer
	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CBaseMultiplayerPlayer *pScorer = ToBaseMultiplayerPlayer( GetDeathScorer( pKiller, pInflictor, pVictim ) );
	CTFPlayer *pAssister = NULL;
	CBaseObject *pObject = NULL;

	// if inflictor or killer is a base object, tell them that they got a kill
	// ( depends if a sentry rocket got the kill, sentry may be inflictor or killer )
	if ( pInflictor )
	{
		if ( pInflictor->IsBaseObject() )
		{
			pObject = dynamic_cast<CBaseObject *>( pInflictor );
		}
		else 
		{
			CBaseEntity *pInflictorOwner = pInflictor->GetOwnerEntity();
			if ( pInflictorOwner && pInflictorOwner->IsBaseObject() )
			{
				pObject = dynamic_cast<CBaseObject *>( pInflictorOwner );
			}
		}
		
	}
	else if( pKiller && pKiller->IsBaseObject() )
	{
		pObject = dynamic_cast<CBaseObject *>( pKiller );
	}

	if ( pObject )
	{
		if ( pObject->GetBuilder() != pVictim )
		{
			pObject->IncrementKills();
		}
		pInflictor = pObject;

		if ( pObject->ObjectType() == OBJ_SENTRYGUN )
		{
			// notify the sentry
			CObjectSentrygun *pSentrygun = dynamic_cast<CObjectSentrygun *>( pObject );
			if ( pSentrygun )
			{
				pSentrygun->OnKilledEnemy( pVictim );
			}

			CTFPlayer *pOwner = pObject->GetOwner();
			if ( pOwner )
			{
				int iKills = pObject->GetKills();

				// keep track of max kills per a single sentry gun in the player object
				if ( pOwner->GetMaxSentryKills() < iKills )
				{
					pOwner->SetMaxSentryKills( iKills );
				}

				// if we just got 10 kills with one sentry, tell the owner's client, which will award achievement if it doesn't have it already
				if ( iKills == 10 )
				{
					pOwner->AwardAchievement( ACHIEVEMENT_TF_GET_TURRETKILLS );
				}
			}
		}
	}

	// if not killed by suicide or killed by world, see if the scorer had an assister, and if so give the assister credit
	if ( ( pVictim != pScorer ) && pKiller )
	{
		pAssister = ToTFPlayer( GetAssister( pVictim, pScorer, pInflictor ) );

		CTFPlayer *pTFVictim = ToTFPlayer( pVictim );
		if ( pAssister && pTFVictim )
		{
			EntityHistory_t* entHist = pTFVictim->m_AchievementData.IsSentryDamagerInHistory( pAssister, 5.0 );
			if ( entHist )
			{
				CBaseObject *pObj = dynamic_cast<CBaseObject*>( entHist->hObject.Get() );
				if ( pObj )
				{
					pObj->IncrementAssists();
				}
			}
		}
	}

	//find the area the player is in and see if his death causes a block
	CTriggerAreaCapture *pArea = dynamic_cast<CTriggerAreaCapture *>(gEntList.FindEntityByClassname( NULL, "trigger_capture_area" ) );
	while( pArea )
	{
		if ( pArea->CheckIfDeathCausesBlock( ToBaseMultiplayerPlayer(pVictim), pScorer ) )
			break;

		pArea = dynamic_cast<CTriggerAreaCapture *>( gEntList.FindEntityByClassname( pArea, "trigger_capture_area" ) );
	}

	// determine if this kill affected a nemesis relationship
	int iDeathFlags = 0;
	CTFPlayer *pTFPlayerVictim = ToTFPlayer( pVictim );
	CTFPlayer *pTFPlayerScorer = ToTFPlayer( pScorer );
	if ( pScorer )
	{	
		CalcDominationAndRevenge( pTFPlayerScorer, pTFPlayerVictim, false, &iDeathFlags );
		if ( pAssister )
		{
			CalcDominationAndRevenge( pAssister, pTFPlayerVictim, true, &iDeathFlags );
		}
	}
	pTFPlayerVictim->SetDeathFlags( iDeathFlags );	

	if ( pAssister )
	{
		CTF_GameStats.Event_AssistKill( ToTFPlayer( pAssister ), pVictim );
	}

	BaseClass::PlayerKilled( pVictim, info );
}

//-----------------------------------------------------------------------------
// Purpose: Determines if attacker and victim have gotten domination or revenge
//-----------------------------------------------------------------------------
void CTFGameRules::CalcDominationAndRevenge( CTFPlayer *pAttacker, CTFPlayer *pVictim, bool bIsAssist, int *piDeathFlags )
{
	// don't do domination stuff in powerup mode
	if ( IsPowerupMode() )
		return;

	// no dominations/revenge in PvE mode
	if ( IsPVEModeActive() )
		return;

	PlayerStats_t *pStatsVictim = CTF_GameStats.FindPlayerStats( pVictim );

	// calculate # of unanswered kills between killer & victim - add 1 to include current kill
	int iKillsUnanswered = pStatsVictim->statsKills.iNumKilledByUnanswered[pAttacker->entindex()] + 1;		
	if ( TF_KILLS_DOMINATION == iKillsUnanswered )
	{			
		// this is the Nth unanswered kill between killer and victim, killer is now dominating victim
		*piDeathFlags |= ( bIsAssist ? TF_DEATH_ASSISTER_DOMINATION : TF_DEATH_DOMINATION );
		// set victim to be dominated by killer
		pAttacker->m_Shared.SetPlayerDominated( pVictim, true );

		int iCurrentlyDominated = pAttacker->GetNumberofDominations() + 1;
		pAttacker->SetNumberofDominations( iCurrentlyDominated );

		// record stats
		CTF_GameStats.Event_PlayerDominatedOther( pAttacker );
	}
	else if ( pVictim->m_Shared.IsPlayerDominated( pAttacker->entindex() ) )
	{
		// the killer killed someone who was dominating him, gains revenge
		*piDeathFlags |= ( bIsAssist ? TF_DEATH_ASSISTER_REVENGE : TF_DEATH_REVENGE );
		// set victim to no longer be dominating the killer
		pVictim->m_Shared.SetPlayerDominated( pAttacker, false );

		int iCurrentlyDominated = pVictim->GetNumberofDominations() - 1;
		if (iCurrentlyDominated < 0)
		{
			iCurrentlyDominated = 0;
		}
		pVictim->SetNumberofDominations( iCurrentlyDominated );

		// record stats
		CTF_GameStats.Event_PlayerRevenge( pAttacker );
	}

}

//-----------------------------------------------------------------------------
// Purpose: create some proxy entities that we use for transmitting data */
//-----------------------------------------------------------------------------
void CTFGameRules::CreateStandardEntities()
{
	// Create the player resource
	g_pPlayerResource = (CPlayerResource*)CBaseEntity::Create( "tf_player_manager", vec3_origin, vec3_angle );

	// Create the objective resource
	g_pObjectiveResource = (CTFObjectiveResource *)CBaseEntity::Create( "tf_objective_resource", vec3_origin, vec3_angle );

	Assert( g_pObjectiveResource );

	// Create the entity that will send our data to the client.
	CBaseEntity *pEnt = CBaseEntity::Create( "tf_gamerules", vec3_origin, vec3_angle );
	Assert( pEnt );
	pEnt->SetName( AllocPooledString("tf_gamerules" ) );

	// SEALTODO make this match live CreateStandardEntities
}

//-----------------------------------------------------------------------------
// Purpose: determine the class name of the weapon that got a kill
//-----------------------------------------------------------------------------
const char *CTFGameRules::GetKillingWeaponName( const CTakeDamageInfo &info, CTFPlayer *pVictim, int *iWeaponID )
{
	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CBasePlayer *pScorer = TFGameRules()->GetDeathScorer( pKiller, pInflictor, pVictim );

	const char *killer_weapon_name = "world";
	*iWeaponID = TF_WEAPON_NONE;

	if ( info.GetDamageCustom() == TF_DMG_CUSTOM_BURNING )
	{
		// special-case burning damage, since persistent burning damage may happen after attacker has switched weapons
		killer_weapon_name = "tf_weapon_flamethrower";
		*iWeaponID = TF_WEAPON_FLAMETHROWER;
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_CARRIED_BUILDING )
	{
		killer_weapon_name = "tf_weapon_building_carried_destroyed";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_TAUNTATK_HADOUKEN )
	{
		killer_weapon_name = "tf_weapon_taunt_pyro";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_TAUNTATK_HIGH_NOON )
	{
		killer_weapon_name = "tf_weapon_taunt_heavy";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_TAUNTATK_FENCING )
	{
		killer_weapon_name = "tf_weapon_taunt_spy";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_TELEFRAG )
	{
		killer_weapon_name = "telefrag";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_EYEBALL_ROCKET )
	{
		killer_weapon_name = "eyeball_rocket";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_MERASMUS_DECAPITATION )
	{
		killer_weapon_name = "merasmus_decap";
	}	
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_MERASMUS_ZAP )
	{
		killer_weapon_name = "merasmus_zap";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_MERASMUS_GRENADE )
	{
		killer_weapon_name = "merasmus_grenade";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_MERASMUS_PLAYER_BOMB )
	{
		killer_weapon_name = "merasmus_player_bomb";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_SPELL_TELEPORT )
	{
		killer_weapon_name = "spellbook_teleport";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_SPELL_SKELETON )
	{
		killer_weapon_name = "spellbook_skeleton";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_SPELL_MIRV )
	{
		killer_weapon_name = "spellbook_mirv";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_SPELL_METEOR )
	{
		killer_weapon_name = "spellbook_meteor";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_SPELL_LIGHTNING )
	{
		killer_weapon_name = "spellbook_lightning";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_SPELL_FIREBALL )
	{
		killer_weapon_name = "spellbook_fireball";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_SPELL_MONOCULUS )
	{
		killer_weapon_name = "spellbook_boss";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_SPELL_BLASTJUMP )
	{
		killer_weapon_name = "spellbook_blastjump";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_SPELL_BATS )
	{
		killer_weapon_name = "spellbook_bats";
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_SPELL_TINY )
	{
		killer_weapon_name = "spellbook_athletic";
	}
	else if ( pScorer && pInflictor && ( pInflictor == pScorer ) )
	{
		// If this is not a suicide
		if ( pVictim != pScorer )
		{
			// If the inflictor is the killer, then it must be their current weapon doing the damage
			if ( pScorer->GetActiveWeapon() )
			{
				killer_weapon_name = pScorer->GetActiveWeapon()->GetClassname(); 
				if ( pScorer->IsPlayer() )
				{
					*iWeaponID = ToTFPlayer(pScorer)->GetActivePCWeapon()->GetWeaponID();
				}
			}
		}
	}
	else if ( pInflictor )
	{
		killer_weapon_name = STRING( pInflictor->m_iClassname );

		// SEALTODO This might need checking since we're casting to TFBase but its fine for now lol

		CTFWeaponBase *pWeapon = dynamic_cast< CTFWeaponBase * >( pInflictor );
		if ( pWeapon )
		{
			*iWeaponID = pWeapon->GetWeaponID();
		}
		else
		{
			CTFBaseRocket *pBaseRocket = dynamic_cast<CTFBaseRocket*>( pInflictor );
			if ( pBaseRocket )
			{
				*iWeaponID = pBaseRocket->GetWeaponID();

				if ( pBaseRocket->GetDeflected() )
				{
					if ( *iWeaponID == TF_WEAPON_ROCKETLAUNCHER )
					{
						killer_weapon_name = "deflect_rocket";
					}
				}
			}
			CTFWeaponBaseGrenadeProj *pBaseGrenade = dynamic_cast<CTFWeaponBaseGrenadeProj*>( pInflictor );
			if ( pBaseGrenade )
			{
				*iWeaponID = pBaseGrenade->GetWeaponID();

				if ( pBaseGrenade->GetDeflected() )
				{
					if ( *iWeaponID == TF_WEAPON_GRENADE_PIPEBOMB )
					{
						killer_weapon_name = "deflect_sticky";
					}
					else if ( *iWeaponID == TF_WEAPON_GRENADE_DEMOMAN )
					{
						killer_weapon_name = "deflect_promode";
					}
				}
			}
			CDODBaseRocket *pDODRocket = dynamic_cast<CDODBaseRocket*>( pInflictor );
			if ( pDODRocket )
			{
				// SEALTODO Weapon ID might be wrong
				*iWeaponID = pDODRocket->GetEmitterWeaponID();

				if ( pDODRocket->GetDeflected() )
				{
					if ( *iWeaponID == DOD_WEAPON_PSCHRECK )
					{
						killer_weapon_name = "deflect_pschreck";
					}
					else if ( *iWeaponID == DOD_WEAPON_BAZOOKA )
					{
						killer_weapon_name = "deflect_bazooka";
					}
				}
			}
			CDODBaseGrenade *pDODGrenade = dynamic_cast<CDODBaseGrenade*>( pInflictor );
			if ( pDODGrenade )
			{
				// SEALTODO Weapon ID might be wrong
				*iWeaponID = pDODGrenade->GetEmitterWeaponID();

				if ( pDODGrenade->GetDeflected() )
				{
					if ( *iWeaponID == DOD_WEAPON_FRAG_GER )
					{
						killer_weapon_name = "deflect_frag_german";
					}
					else if ( *iWeaponID == DOD_WEAPON_FRAG_US )
					{
						killer_weapon_name = "deflect_frag_us";
					}
					else if ( *iWeaponID == DOD_WEAPON_RIFLEGREN_GER )
					{
						killer_weapon_name = "deflect_riflegrenade_german";
					}
					else if ( *iWeaponID == DOD_WEAPON_RIFLEGREN_US )
					{
						killer_weapon_name = "deflect_riflegrenade_us";
					}
				}
			}
		}
	}

	// strip certain prefixes from inflictor's classname
	const char *prefix[] = { "tf_weapon_grenade_", "tf_weapon_", "NPC_", "func_", "dod_weapon_" };
	for ( int i = 0; i< ARRAYSIZE( prefix ); i++ )
	{
		// if prefix matches, advance the string pointer past the prefix
		int len = Q_strlen( prefix[i] );
		if ( strncmp( killer_weapon_name, prefix[i], len ) == 0 )
		{
			killer_weapon_name += len;
			break;
		}
	}

	// look out for sentry rocket as weapon and map it to sentry gun, so we get the sentry death icon
	if ( 0 == Q_strcmp( killer_weapon_name, "tf_projectile_sentryrocket" ) )
	{
		killer_weapon_name = "obj_sentrygun3";
	}
	else if ( 0 == Q_strcmp( killer_weapon_name, "obj_sentrygun" ) )
	{
		CObjectSentrygun *pSentrygun = assert_cast<CObjectSentrygun*>( pInflictor );
		if ( pSentrygun )
		{
			if ( pSentrygun->IsMiniBuilding() )
			{
				killer_weapon_name = "obj_minisentry";
			}
			else
			{
				int iSentryLevel = pSentrygun->GetUpgradeLevel();
				switch( iSentryLevel)
				{
				case 1:
					killer_weapon_name = "obj_sentrygun";
					break;
				case 2:
					killer_weapon_name = "obj_sentrygun2";
					break;
				case 3:
					killer_weapon_name = "obj_sentrygun3";
					break;
				default:
					killer_weapon_name = "obj_sentrygun";
					break;
				}
			}
		}
	}

	return killer_weapon_name;
}

//-----------------------------------------------------------------------------
// Purpose: returns the player who assisted in the kill, or NULL if no assister
//-----------------------------------------------------------------------------
CBasePlayer *CTFGameRules::GetAssister( CBasePlayer *pVictim, CBasePlayer *pScorer, CBaseEntity *pInflictor )
{
	CTFPlayer *pTFScorer = ToTFPlayer( pScorer );
	CTFPlayer *pTFVictim = ToTFPlayer( pVictim );
	if ( pTFScorer && pTFVictim )
	{
		// if victim killed himself, don't award an assist to anyone else, even if there was a recent damager
		if ( pTFScorer == pTFVictim )
			return NULL;

		// If an assist has been specified already, use it.
		if ( pTFVictim->m_Shared.GetAssist() )
		{
			return pTFVictim->m_Shared.GetAssist();
		}

		// If a player is healing the scorer, give that player credit for the assist
		CTFPlayer *pHealer = ToTFPlayer( pTFScorer->m_Shared.GetFirstHealer() );
		// Must be a medic to receive a healing assist, otherwise engineers get credit for assists from dispensers doing healing.
		// Also don't give an assist for healing if the inflictor was a sentry gun, otherwise medics healing engineers get assists for the engineer's sentry kills.
		if ( pHealer && ( TF_CLASS_MEDIC == pHealer->GetPlayerClass()->GetClassIndex() ) && ( NULL == dynamic_cast<CObjectSentrygun *>( pInflictor ) ) )
		{
			return pHealer;
		}

		// If we're under the effect of a condition that grants assists, give one to the player that buffed us
		CTFPlayer *pCondAssister = ToTFPlayer( pTFScorer->m_Shared.GetConditionAssistFromAttacker() );
		if ( pCondAssister )
			return pCondAssister;

		// See who has damaged the victim 2nd most recently (most recent is the killer), and if that is within a certain time window.
		// If so, give that player an assist.  (Only 1 assist granted, to single other most recent damager.)
		CTFPlayer *pRecentDamager = GetRecentDamager( pTFVictim, 1, TF_TIME_ASSIST_KILL );
		if ( pRecentDamager && ( pRecentDamager != pScorer ) )
			return pRecentDamager;

		// if a teammate has recently helped this sentry (ie: wrench hit), they assisted in the kill
		CObjectSentrygun *sentry = dynamic_cast< CObjectSentrygun * >( pInflictor );
		if ( sentry )
		{
			CTFPlayer *pAssister = sentry->GetAssistingTeammate( TF_TIME_ASSIST_KILL );
			if ( pAssister )
				return pAssister;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Returns specifed recent damager, if there is one who has done damage
//			within the specified time period.  iDamager=0 returns the most recent
//			damager, iDamager=1 returns the next most recent damager.
//-----------------------------------------------------------------------------
CTFPlayer *CTFGameRules::GetRecentDamager( CTFPlayer *pVictim, int iDamager, float flMaxElapsed )
{
	EntityHistory_t *damagerHistory = pVictim->m_AchievementData.GetDamagerHistory( iDamager );
	if ( !damagerHistory )
		return NULL;

	if ( damagerHistory->hEntity && ( gpGlobals->curtime - damagerHistory->flTimeDamage <= flMaxElapsed ) )
	{
		CTFPlayer *pRecentDamager = ToTFPlayer( damagerHistory->hEntity );
		if ( pRecentDamager )
			return pRecentDamager;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Returns who should be awarded the kill
//-----------------------------------------------------------------------------
CBasePlayer *CTFGameRules::GetDeathScorer( CBaseEntity *pKiller, CBaseEntity *pInflictor, CBaseEntity *pVictim )
{
	if ( ( pKiller == pVictim ) && ( pKiller == pInflictor ) )
	{
		// If this was an explicit suicide, see if there was a damager within a certain time window.  If so, award this as a kill to the damager.
		CTFPlayer *pTFVictim = ToTFPlayer( pVictim );
		if ( pTFVictim )
		{
			CTFPlayer *pRecentDamager = GetRecentDamager( pTFVictim, 0, TF_TIME_SUICIDE_KILL_CREDIT );
			if ( pRecentDamager )
				return pRecentDamager;
		}
	}

	return BaseClass::GetDeathScorer( pKiller, pInflictor, pVictim );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVictim - 
//			*pKiller - 
//			*pInflictor - 
//-----------------------------------------------------------------------------
void CTFGameRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	DeathNotice( pVictim, info, "player_death" );
}

void CTFGameRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info, const char* eventName )
{
	int killer_ID = 0;

	// Find the killer & the scorer
	CTFPlayer *pTFPlayerVictim = ToTFPlayer( pVictim );
	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CTFPlayer *pScorer = ToTFPlayer( GetDeathScorer( pKiller, pInflictor, pVictim ) );
	CTFPlayer *pAssister = ToTFPlayer( GetAssister( pVictim, pScorer, pInflictor ) );

	// You can't assist yourself!
	Assert( pScorer != pAssister || !pScorer );
	if ( pScorer == pAssister && pScorer )
	{
		pAssister = NULL;
	}

	// Work out what killed the player, and send a message to all clients about it
	int iWeaponID;
	const char *killer_weapon_name = GetKillingWeaponName( info, pTFPlayerVictim, &iWeaponID );

	if ( pScorer )	// Is the killer a client?
	{
		killer_ID = pScorer->GetUserID();
	}

	IGameEvent * event = gameeventmanager->CreateEvent( eventName /* "player_death" */ );

	if ( event )
	{
		event->SetInt( "userid", pVictim->GetUserID() );
		event->SetInt( "victim_entindex", pVictim->entindex() );
		event->SetInt( "attacker", killer_ID );
		event->SetInt( "assister", pAssister ? pAssister->GetUserID() : -1 );
		event->SetString( "weapon", killer_weapon_name );
//		event->SetString( "weapon_logclassname", killer_weapon_log_name );
		event->SetInt( "weaponid", iWeaponID );
		event->SetInt( "damagebits", info.GetDamageType() );
		event->SetInt( "customkill", info.GetDamageCustom() );
		event->SetInt( "inflictor_entindex", pInflictor ? pInflictor->entindex() : -1 );
		event->SetInt( "priority", 7 );	// HLTV event priority, not transmitted

		int iDeathFlags = pTFPlayerVictim->GetDeathFlags();

		// SEALTODO
		//if ( pTFPlayerVictim->WasGibbedOnLastDeath() )
		//{
		//	iDeathFlags |= TF_DEATH_GIBBED;
		//}

		// SEALTODO
		//if ( pTFPlayerVictim->IsInPurgatory() )
		//{
		//	iDeathFlags |= TF_DEATH_PURGATORY;
		//}

		if ( pTFPlayerVictim->IsMiniBoss() )
		{
			iDeathFlags |= TF_DEATH_MINIBOSS;
		}

		// We call this directly since we need more information than provided in the event alone.
		if ( FStrEq( eventName, "player_death" ) )
		{
			// SEALTODO
//			CTF_GameStats.Event_KillDetail( pScorer, pTFPlayerVictim, pAssister, event, info );
			event->SetBool( "rocket_jump", ( pTFPlayerVictim->RocketJumped() == 1 ) );
			event->SetInt( "crit_type", info.GetCritType() );
		}

		if ( pTFPlayerVictim->GetDeathFlags() & TF_DEATH_DOMINATION )
		{
			event->SetInt( "dominated", 1 );
		}
		if ( pTFPlayerVictim->GetDeathFlags() & TF_DEATH_ASSISTER_DOMINATION )
		{
			event->SetInt( "assister_dominated", 1 );
		}
		if ( pTFPlayerVictim->GetDeathFlags() & TF_DEATH_REVENGE )
		{
			event->SetInt( "revenge", 1 );
		}
		if ( pTFPlayerVictim->GetDeathFlags() & TF_DEATH_ASSISTER_REVENGE )
		{
			event->SetInt( "assister_revenge", 1 );
		}

		event->SetInt( "death_flags", iDeathFlags );	
		event->SetInt( "stun_flags", pTFPlayerVictim->m_iOldStunFlags );

		pTFPlayerVictim->m_iOldStunFlags = 0;

		gameeventmanager->FireEvent( event );
	}		
}

void CTFGameRules::ClientDisconnected( edict_t *pClient )
{
	// clean up anything they left behind
	CTFPlayer *pPlayer = ToTFPlayer( GetContainingEntity( pClient ) );
	if ( pPlayer )
	{
		pPlayer->TeamFortress_ClientDisconnected();
	}

	BaseClass::ClientDisconnected( pClient );

	// are any of the spies disguising as this player?
	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CTFPlayer *pTemp = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pTemp && pTemp != pPlayer )
		{
			if ( pTemp->m_Shared.GetDisguiseTarget() == pPlayer )
			{
				// choose someone else...
				pTemp->m_Shared.FindDisguiseTarget();
			}
		}
	}
}

// Falling damage stuff.
#define TF_PLAYER_MAX_SAFE_FALL_SPEED	650		

// Falling damage stuff.
#define DOD_PLAYER_FATAL_FALL_SPEED		900		// approx 60 feet
#define DOD_PLAYER_MAX_SAFE_FALL_SPEED	500		// approx 20 feet
#define DOD_DAMAGE_FOR_FALL_SPEED		((float)100 / ( DOD_PLAYER_FATAL_FALL_SPEED - DOD_PLAYER_MAX_SAFE_FALL_SPEED )) // damage per unit per second.

float CTFGameRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	CTFPlayer *pTFPlayer = (CTFPlayer*)pPlayer;

	if ( pTFPlayer && pTFPlayer->IsDODClass() )
	{
		pPlayer->m_Local.m_flFallVelocity -= DOD_PLAYER_MAX_SAFE_FALL_SPEED;
		return pPlayer->m_Local.m_flFallVelocity * DOD_DAMAGE_FOR_FALL_SPEED;
	}

	if ( pPlayer->m_Local.m_flFallVelocity > TF_PLAYER_MAX_SAFE_FALL_SPEED )
	{
		// Old TFC damage formula
		float flFallDamage = 5 * (pPlayer->m_Local.m_flFallVelocity / 300);

		// Fall damage needs to scale according to the player's max health, or
		// it's always going to be much more dangerous to weaker classes than larger.
		float flRatio = (float)pPlayer->GetMaxHealth() / 100.0;
		flFallDamage *= flRatio;

		flFallDamage *= random->RandomFloat( 0.8, 1.2 );

		return flFallDamage;
	}

	// Fall caused no damage
	return 0;
}

void CTFGameRules::SendWinPanelInfo( bool bGameOver )
{
	IGameEvent *winEvent = gameeventmanager->CreateEvent( "teamplay_win_panel" );

	if ( winEvent )
	{
		int iBlueScore = GetGlobalTeam( TF_TEAM_BLUE )->GetScore();
		int iRedScore = GetGlobalTeam( TF_TEAM_RED )->GetScore();
		int iBlueScorePrev = iBlueScore;
		int iRedScorePrev = iRedScore;

		bool bRoundComplete = m_bForceMapReset || ( IsGameUnderTimeLimit() && ( GetTimeLeft() <= 0 ) );

		CTeamControlPointMaster *pMaster = g_hControlPointMasters.Count() ? g_hControlPointMasters[0] : NULL;
		bool bScoringPerCapture = ( pMaster ) ? ( pMaster->ShouldScorePerCapture() ) : false;

		if ( bRoundComplete && !bScoringPerCapture )
		{
			// if this is a complete round, calc team scores prior to this win
			switch ( m_iWinningTeam )
			{
			case TF_TEAM_BLUE:
				iBlueScorePrev = ( iBlueScore - TEAMPLAY_ROUND_WIN_SCORE >= 0 ) ? ( iBlueScore - TEAMPLAY_ROUND_WIN_SCORE ) : 0;
				break;
			case TF_TEAM_RED:
				iRedScorePrev = ( iRedScore - TEAMPLAY_ROUND_WIN_SCORE >= 0 ) ? ( iRedScore - TEAMPLAY_ROUND_WIN_SCORE ) : 0;
				break;
			case TEAM_UNASSIGNED:
				break;	// stalemate; nothing to do
			}
		}
			
		winEvent->SetInt( "panel_style", WINPANEL_BASIC );
		winEvent->SetInt( "winning_team", m_iWinningTeam );
		winEvent->SetInt( "winreason", m_iWinReason );
		winEvent->SetString( "cappers",  ( m_iWinReason == WINREASON_ALL_POINTS_CAPTURED || m_iWinReason == WINREASON_FLAG_CAPTURE_LIMIT ) ?
			m_szMostRecentCappers : "" );
		winEvent->SetInt( "flagcaplimit", tf_flag_caps_per_round.GetInt() );
		winEvent->SetInt( "blue_score", iBlueScore );
		winEvent->SetInt( "red_score", iRedScore );
		winEvent->SetInt( "blue_score_prev", iBlueScorePrev );
		winEvent->SetInt( "red_score_prev", iRedScorePrev );
		winEvent->SetInt( "round_complete", bRoundComplete );

		CTFPlayerResource *pResource = dynamic_cast< CTFPlayerResource * >( g_pPlayerResource );
		if ( !pResource )
			return;
 
		// determine the 3 players on winning team who scored the most points this round

		// build a vector of players & round scores
		CUtlVector<PlayerRoundScore_t> vecPlayerScore;
		int iPlayerIndex;
		for( iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
		{
			CTFPlayer *pTFPlayer = ToTFPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
			if ( !pTFPlayer || !pTFPlayer->IsConnected() )
				continue;
			// filter out spectators and, if not stalemate, all players not on winning team
			int iPlayerTeam = pTFPlayer->GetTeamNumber();
			if ( ( iPlayerTeam < FIRST_GAME_TEAM ) || ( m_iWinningTeam != TEAM_UNASSIGNED && ( m_iWinningTeam != iPlayerTeam ) ) )
				continue;

			int iRoundScore = 0, iTotalScore = 0;
			PlayerStats_t *pStats = CTF_GameStats.FindPlayerStats( pTFPlayer );
			if ( pStats )
			{
				iRoundScore = CalcPlayerScore( &pStats->statsCurrentRound );
				iTotalScore = CalcPlayerScore( &pStats->statsAccumulated );
			}
			PlayerRoundScore_t &playerRoundScore = vecPlayerScore[vecPlayerScore.AddToTail()];
			playerRoundScore.iPlayerIndex = iPlayerIndex;
			playerRoundScore.iRoundScore = iRoundScore;
			playerRoundScore.iTotalScore = iTotalScore;
		}
		// sort the players by round score
		vecPlayerScore.Sort( PlayerRoundScoreSortFunc );

		// set the top (up to) 3 players by round score in the event data
		int numPlayers = min( 3, vecPlayerScore.Count() );
		for ( int i = 0; i < numPlayers; i++ )
		{
			// only include players who have non-zero points this round; if we get to a player with 0 round points, stop
			if ( 0 == vecPlayerScore[i].iRoundScore )
				break;

			// set the player index and their round score in the event
			char szPlayerIndexVal[64]="", szPlayerScoreVal[64]="";
			Q_snprintf( szPlayerIndexVal, ARRAYSIZE( szPlayerIndexVal ), "player_%d", i+ 1 );
			Q_snprintf( szPlayerScoreVal, ARRAYSIZE( szPlayerScoreVal ), "player_%d_points", i+ 1 );
			winEvent->SetInt( szPlayerIndexVal, vecPlayerScore[i].iPlayerIndex );
			winEvent->SetInt( szPlayerScoreVal, vecPlayerScore[i].iRoundScore );				
		}

		if ( !bRoundComplete && ( TEAM_UNASSIGNED != m_iWinningTeam ) )
		{
			// if this was not a full round ending, include how many mini-rounds remain for winning team to win
			if ( g_hControlPointMasters.Count() && g_hControlPointMasters[0] )
			{
				winEvent->SetInt( "rounds_remaining", g_hControlPointMasters[0]->CalcNumRoundsRemaining( m_iWinningTeam ) );
			}
		}

		// Send the event
		gameeventmanager->FireEvent( winEvent );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sorts players by round score
//-----------------------------------------------------------------------------
int CTFGameRules::PlayerRoundScoreSortFunc( const PlayerRoundScore_t *pRoundScore1, const PlayerRoundScore_t *pRoundScore2 )
{
	// sort first by round score	
	if ( pRoundScore1->iRoundScore != pRoundScore2->iRoundScore )
		return pRoundScore2->iRoundScore - pRoundScore1->iRoundScore;

	// if round scores are the same, sort next by total score
	if ( pRoundScore1->iTotalScore != pRoundScore2->iTotalScore )
		return pRoundScore2->iTotalScore - pRoundScore1->iTotalScore;

	// if scores are the same, sort next by player index so we get deterministic sorting
	return ( pRoundScore2->iPlayerIndex - pRoundScore1->iPlayerIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the teamplay_round_win event is about to be sent, gives
//			this method a chance to add more data to it
//-----------------------------------------------------------------------------
void CTFGameRules::FillOutTeamplayRoundWinEvent( IGameEvent *event )
{
	// determine the losing team
	int iLosingTeam;

	switch( event->GetInt( "team" ) )
	{
	case TF_TEAM_RED:
		iLosingTeam = TF_TEAM_BLUE;
		break;
	case TF_TEAM_BLUE:
		iLosingTeam = TF_TEAM_RED;
		break;
	case TEAM_UNASSIGNED:
	default:
		iLosingTeam = TEAM_UNASSIGNED;
		break;
	}

	// set the number of caps that team got any time during the round
	event->SetInt( "losing_team_num_caps", m_iNumCaps[iLosingTeam] );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::SetupSpawnPointsForRound( void )
{
	if ( !g_hControlPointMasters.Count() || !g_hControlPointMasters[0] || !g_hControlPointMasters[0]->PlayingMiniRounds() )
		return;

	CTeamControlPointRound *pCurrentRound = g_hControlPointMasters[0]->GetCurrentRound();
	if ( !pCurrentRound )
	{
		return;
	}

	// loop through the spawn points in the map and find which ones are associated with this round or the control points in this round
	CBaseEntity *pSpot = gEntList.FindEntityByClassname( NULL, "info_player_teamspawn" );
	while( pSpot )
	{
		CTFTeamSpawn *pTFSpawn = assert_cast<CTFTeamSpawn*>(pSpot);

		if ( pTFSpawn )
		{
			CHandle<CTeamControlPoint> hControlPoint = pTFSpawn->GetControlPoint();
			CHandle<CTeamControlPointRound> hRoundBlue = pTFSpawn->GetRoundBlueSpawn();
			CHandle<CTeamControlPointRound> hRoundRed = pTFSpawn->GetRoundRedSpawn();

			if ( hControlPoint && pCurrentRound->IsControlPointInRound( hControlPoint ) )
			{
				// this spawn is associated with one of our control points
				pTFSpawn->SetDisabled( false );
				pTFSpawn->ChangeTeam( hControlPoint->GetOwner() );
			}
			else if ( hRoundBlue && ( hRoundBlue == pCurrentRound ) )
			{
				pTFSpawn->SetDisabled( false );
				pTFSpawn->ChangeTeam( TF_TEAM_BLUE );
			}
			else if ( hRoundRed && ( hRoundRed == pCurrentRound ) )
			{
				pTFSpawn->SetDisabled( false );
				pTFSpawn->ChangeTeam( TF_TEAM_RED );
			}
			else
			{
				// this spawn isn't associated with this round or the control points in this round
				pTFSpawn->SetDisabled( true );
			}
		}

		pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_teamspawn" );
	}
}


int CTFGameRules::SetCurrentRoundStateBitString( void )
{
	m_iPrevRoundState = m_iCurrentRoundState;

	CTeamControlPointMaster *pMaster = g_hControlPointMasters.Count() ? g_hControlPointMasters[0] : NULL;

	if ( !pMaster )
	{
		return 0;
	}

	int iState = 0;

	for ( int i=0; i<pMaster->GetNumPoints(); i++ )
	{
		CTeamControlPoint *pPoint = pMaster->GetControlPoint( i );

		if ( pPoint->GetOwner() == TF_TEAM_BLUE )
		{
			// Set index to 1 for the point being owned by blue
			iState |= ( 1<<i );
		}
	}

	m_iCurrentRoundState = iState;

	return iState;
}


void CTFGameRules::SetMiniRoundBitMask( int iMask )
{
	m_iCurrentMiniRoundMask = iMask;
}

//-----------------------------------------------------------------------------
// Purpose: NULL pPlayer means show the panel to everyone
//-----------------------------------------------------------------------------
void CTFGameRules::ShowRoundInfoPanel( CTFPlayer *pPlayer /* = NULL */ )
{
	KeyValues *data = new KeyValues( "data" );

	if ( m_iCurrentRoundState < 0 )
	{
		// Haven't set up the round state yet
		return;
	}

	// if prev and cur are equal, we are starting from a fresh round
	if ( m_iPrevRoundState >= 0 && pPlayer == NULL )	// we have data about a previous state
	{
		data->SetInt( "prev", m_iPrevRoundState );
	}
	else
	{
		// don't send a delta if this is just to one player, they are joining mid-round
		data->SetInt( "prev", m_iCurrentRoundState );	
	}

	data->SetInt( "cur", m_iCurrentRoundState );

	// get bitmask representing the current miniround
	data->SetInt( "round", m_iCurrentMiniRoundMask );

	if ( pPlayer )
	{
		pPlayer->ShowViewPortPanel( PANEL_ROUNDINFO, true, data );
	}
	else
	{
		for ( int i = 1;  i <= MAX_PLAYERS; i++ )
		{
			CTFPlayer *pTFPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );

			if ( pTFPlayer && pTFPlayer->IsReadyToPlay() )
			{
				pTFPlayer->ShowViewPortPanel( PANEL_ROUNDINFO, true, data );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::TimerMayExpire( void )
{
	// Prevent timers expiring while control points are contested
	int iNumControlPoints = ObjectiveResource()->GetNumControlPoints();
	for ( int iPoint = 0; iPoint < iNumControlPoints; iPoint++ )
	{
		if ( ObjectiveResource()->GetCappingTeam( iPoint ) )
		{
			// HACK: Fix for some maps adding time to the clock 0.05s after CP is capped.
			m_flTimerMayExpireAt = gpGlobals->curtime + 0.1f;
			return false;
		}
	}

	if ( m_flTimerMayExpireAt >= gpGlobals->curtime )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::RoundRespawn( void )
{
	// remove any buildings, grenades, rockets, etc. the player put into the world
	RemoveAllProjectilesAndBuildings();

	// re-enable any sentry guns the losing team has built (and not hidden!)
	for ( int i=0; i<IBaseObjectAutoList::AutoList().Count(); ++i )
	{
		CBaseObject *pObj = static_cast< CBaseObject* >( IBaseObjectAutoList::AutoList()[i] );
		if ( pObj->ObjectType() == OBJ_SENTRYGUN && pObj->IsEffectActive( EF_NODRAW ) == false && pObj->GetTeamNumber() != m_iWinningTeam )
		{
			pObj->SetDisabled( false );
		}
	}

	// reset the flag captures
	int nTeamCount = TFTeamMgr()->GetTeamCount();
	for ( int iTeam = FIRST_GAME_TEAM; iTeam < nTeamCount; ++iTeam )
	{
		CTFTeam *pTeam = GetGlobalTFTeam( iTeam );
		if ( !pTeam )
			continue;

		pTeam->SetFlagCaptures( 0 );
	}

	CTF_GameStats.ResetRoundStats();

	BaseClass::RoundRespawn();

	// ** AFTER WE'VE BEEN THROUGH THE ROUND RESPAWN, SHOW THE ROUNDINFO PANEL
	if ( !IsInWaitingForPlayers() )
	{
		ShowRoundInfoPanel();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::InternalHandleTeamWin( int iWinningTeam )
{
	// remove any spies' disguises and make them visible (for the losing team only)
	// and set the speed for both teams (winners get a boost and losers have reduced speed)
	for ( int i = 1;  i <= MAX_PLAYERS; i++ )
	{
		CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer )
		{
			if ( pPlayer->GetTeamNumber() > LAST_SHARED_TEAM )
			{
				if ( pPlayer->GetTeamNumber() != iWinningTeam )
				{
					pPlayer->RemoveInvisibility();
//					pPlayer->RemoveDisguise();

					if ( pPlayer->HasTheFlag() )
					{
						pPlayer->DropFlag();
					}
				}

				pPlayer->TeamFortress_SetSpeed();
			}
		}
	}

	// disable any sentry guns the losing team has built
	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "obj_sentrygun" ) ) != NULL )
	{
		CObjectSentrygun *pSentry = dynamic_cast<CObjectSentrygun *>( pEnt );
		if ( pSentry )
		{
			if ( pSentry->GetTeamNumber() != iWinningTeam )
			{
				pSentry->SetDisabled( true );
			}
		}
	}

	if ( m_bForceMapReset )
	{
		m_iPrevRoundState = -1;
		m_iCurrentRoundState = -1;
		m_iCurrentMiniRoundMask = 0;
	}
}

// sort function for the list of players that we're going to use to scramble the teams
int ScramblePlayersSort( CTFPlayer* const *p1, CTFPlayer* const *p2 )
{
	CTFPlayerResource *pResource = dynamic_cast< CTFPlayerResource * >( g_pPlayerResource );

	if ( pResource )
	{
		// check the priority
		if ( pResource->GetTotalScore( (*p2)->entindex() ) > pResource->GetTotalScore( (*p1)->entindex() ) )
		{
			return 1;
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::HandleScrambleTeams( void )
{
	int i = 0;
	CTFPlayer *pTFPlayer = NULL;
	CUtlVector<CTFPlayer *> pListPlayers;

	// add all the players (that are on blue or red) to our temp list
	for ( i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		pTFPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pTFPlayer && ( pTFPlayer->GetTeamNumber() >= TF_TEAM_RED ) )
		{
			pListPlayers.AddToHead( pTFPlayer );
		}
	}

	// sort the list
	pListPlayers.Sort( ScramblePlayersSort );

	// loop through and put everyone on Spectator to clear the teams (or the autoteam step won't work correctly)
	for ( i = 0 ; i < pListPlayers.Count() ; i++ )
	{
		pTFPlayer = pListPlayers[i];

		if ( pTFPlayer )
		{
			pTFPlayer->ForceChangeTeam( TEAM_SPECTATOR );
		}
	}

	// loop through and auto team everyone
	for ( i = 0 ; i < pListPlayers.Count() ; i++ )
	{
		pTFPlayer = pListPlayers[i];

		if ( pTFPlayer )
		{
			pTFPlayer->ForceChangeTeam( TF_TEAM_AUTOASSIGN );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::HandleSwitchTeams( void )
{
	int i = 0;

	// remove everyone's projectiles and objects
	RemoveAllProjectilesAndBuildings();

	// respawn the players
	for ( i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			if ( pPlayer->GetTeamNumber() == TF_TEAM_RED )
			{
				pPlayer->ForceChangeTeam( TF_TEAM_BLUE, true );
			}
			else if ( pPlayer->GetTeamNumber() == TF_TEAM_BLUE )
			{
				pPlayer->ForceChangeTeam( TF_TEAM_RED, true );
			}
		}
	}

	// switch the team scores
	CTFTeam *pRedTeam = GetGlobalTFTeam( TF_TEAM_RED );
	CTFTeam *pBlueTeam = GetGlobalTFTeam( TF_TEAM_BLUE );
	if ( pRedTeam && pBlueTeam )
	{
		int nRed = pRedTeam->GetScore();
		int nBlue = pBlueTeam->GetScore();

		pRedTeam->SetScore( nBlue );
		pBlueTeam->SetScore( nRed );
	}

	UTIL_ClientPrintAll( HUD_PRINTTALK, "#TF_TeamsSwitched" );
}

bool CTFGameRules::CanChangeClassInStalemate( void ) 
{ 
	return (gpGlobals->curtime < (m_flStalemateStartTime + tf_stalematechangeclasstime.GetFloat())); 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::SetRoundOverlayDetails( void )
{
	CTeamControlPointMaster *pMaster = g_hControlPointMasters.Count() ? g_hControlPointMasters[0] : NULL;

	if ( pMaster && pMaster->PlayingMiniRounds() )
	{
		CTeamControlPointRound *pRound = pMaster->GetCurrentRound();

		if ( pRound )
		{
			CHandle<CTeamControlPoint> pRedPoint = pRound->GetPointOwnedBy( TF_TEAM_RED );
			CHandle<CTeamControlPoint> pBluePoint = pRound->GetPointOwnedBy( TF_TEAM_BLUE );

			// do we have opposing points in this round?
			if ( pRedPoint && pBluePoint )
			{
				int iMiniRoundMask = ( 1<<pBluePoint->GetPointIndex() ) | ( 1<<pRedPoint->GetPointIndex() );
				SetMiniRoundBitMask( iMiniRoundMask );
			}
			else
			{
				SetMiniRoundBitMask( 0 );
			}

			SetCurrentRoundStateBitString();
		}
	}

	BaseClass::SetRoundOverlayDetails();
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether a team should score for each captured point
//-----------------------------------------------------------------------------
bool CTFGameRules::ShouldScorePerRound( void )
{ 
	bool bRetVal = true;

	CTeamControlPointMaster *pMaster = g_hControlPointMasters.Count() ? g_hControlPointMasters[0] : NULL;
	if ( pMaster && pMaster->ShouldScorePerCapture() )
	{
		bRetVal = false;
	}

	return bRetVal;
}

#endif  // GAME_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFGameRules::GetFarthestOwnedControlPoint( int iTeam, bool bWithSpawnpoints )
{
	int iOwnedEnd = ObjectiveResource()->GetBaseControlPointForTeam( iTeam );
	if ( iOwnedEnd == -1 )
		return -1;

	int iNumControlPoints = ObjectiveResource()->GetNumControlPoints();
	int iWalk = 1;
	int iEnemyEnd = iNumControlPoints-1;
	if ( iOwnedEnd != 0 )
	{
		iWalk = -1;
		iEnemyEnd = 0;
	}

	// Walk towards the other side, and find the farthest owned point that has spawn points
	int iFarthestPoint = iOwnedEnd;
	for ( int iPoint = iOwnedEnd; iPoint != iEnemyEnd; iPoint += iWalk )
	{
		// If we've hit a point we don't own, we're done
		if ( ObjectiveResource()->GetOwningTeam( iPoint ) != iTeam )
			break;

		if ( bWithSpawnpoints && !m_bControlSpawnsPerTeam[iTeam][iPoint] )
			continue;

		iFarthestPoint = iPoint;
	}

	return iFarthestPoint;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::TeamMayCapturePoint( int iTeam, int iPointIndex ) 
{ 
	if ( !tf_caplinear.GetBool() )
		return true;

	// DOD compatablity
	if ( !ObjectiveResource()->GetShouldRequirePreviousPoints() )
		return true;

	// Any previous points necessary?
	int iPointNeeded = ObjectiveResource()->GetPreviousPointForPoint( iPointIndex, iTeam, 0 );

	// Points set to require themselves are always cappable 
	if ( iPointNeeded == iPointIndex )
		return true;

	// No required points specified? Require all previous points.
	if ( iPointNeeded == -1 )
	{
		if ( !ObjectiveResource()->PlayingMiniRounds() )
		{
			// No custom previous point, team must own all previous points
			int iFarthestPoint = GetFarthestOwnedControlPoint( iTeam, false );
			return (abs(iFarthestPoint - iPointIndex) <= 1);
		}
		else
		{
			// No custom previous point, team must own all previous points in the current mini-round
			//tagES TFTODO: need to figure out a good algorithm for this
			return true;
		}
	}

	// Loop through each previous point and see if the team owns it
	for ( int iPrevPoint = 0; iPrevPoint < MAX_PREVIOUS_POINTS; iPrevPoint++ )
	{
		int iPointNeeded = ObjectiveResource()->GetPreviousPointForPoint( iPointIndex, iTeam, iPrevPoint );
		if ( iPointNeeded != -1 )
		{
			if ( ObjectiveResource()->GetOwningTeam( iPointNeeded ) != iTeam )
				return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::PlayerMayCapturePoint( CBasePlayer *pPlayer, int iPointIndex, char *pszReason /* = NULL */, int iMaxReasonLength /* = 0 */ )
{
	CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );

	if ( !pTFPlayer )
	{
		return false;
	}

	// Disguised and invisible spies cannot capture points
	if ( pTFPlayer->m_Shared.InCond( TF_COND_STEALTHED ) )
	{
		if ( pszReason )
		{
			Q_snprintf( pszReason, iMaxReasonLength, "#Cant_cap_stealthed" );
		}
		return false;
	}

	if ( pTFPlayer->m_Shared.InCond( TF_COND_INVULNERABLE ) )
	{
		if ( pszReason )
		{
			Q_snprintf( pszReason, iMaxReasonLength, "#Cant_cap_invuln" );
		}
		return false;
	}

 	if ( pTFPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		if ( pszReason )
		{
			Q_snprintf( pszReason, iMaxReasonLength, "#Cant_cap_disguised" );
		}
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::PlayerMayBlockPoint( CBasePlayer *pPlayer, int iPointIndex, char *pszReason, int iMaxReasonLength )
{
	CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );
	if ( !pTFPlayer )
		return false;

	// Invuln players can block points
	if ( pTFPlayer->m_Shared.InCond( TF_COND_INVULNERABLE ) )
	{
		if ( pszReason )
		{
			Q_snprintf( pszReason, iMaxReasonLength, "#Cant_cap_invuln" );
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Calculates score for player
//-----------------------------------------------------------------------------
int CTFGameRules::CalcPlayerScore( RoundStats_t *pRoundStats )
{
	Assert( pRoundStats );
	if ( !pRoundStats )
		return 0;

	// defensive fix for the moment for bug where healing value becomes bogus sometimes: if bogus, slam it to 0
	int iHealing = pRoundStats->m_iStat[TFSTAT_HEALING];
	Assert( iHealing >= 0 );
	Assert( iHealing <= 10000000 );
	if ( iHealing < 0 || iHealing > 10000000 )
	{
		iHealing = 0;
	}

	//bool bMvM = TFGameRules() && TFGameRules()->IsMannVsMachineMode();
	//bool bPowerupMode = TFGameRules() && TFGameRules()->IsPowerupMode();

	bool bMvM = false;
	bool bPowerupMode = false;

	int iScore =	( pRoundStats->m_iStat[TFSTAT_KILLS] * TF_SCORE_KILL ) + 
					( pRoundStats->m_iStat[TFSTAT_KILLS_RUNECARRIER] * TF_SCORE_KILL_RUNECARRIER ) + // Kill someone who is carrying a rune
					( pRoundStats->m_iStat[TFSTAT_CAPTURES] * ( ( bPowerupMode ) ? TF_SCORE_CAPTURE_POWERUPMODE : TF_SCORE_CAPTURE ) ) +
					( pRoundStats->m_iStat[TFSTAT_FLAGRETURNS] * TF_SCORE_FLAG_RETURN ) +
					( pRoundStats->m_iStat[TFSTAT_DEFENSES] * TF_SCORE_DEFEND ) + 
					( pRoundStats->m_iStat[TFSTAT_BUILDINGSDESTROYED] * TF_SCORE_DESTROY_BUILDING ) + 
					( pRoundStats->m_iStat[TFSTAT_HEADSHOTS] / TF_SCORE_HEADSHOT_DIVISOR ) + 
					( pRoundStats->m_iStat[TFSTAT_BACKSTABS] * TF_SCORE_BACKSTAB ) + 
					( iHealing / ( ( bMvM ) ? TF_SCORE_DAMAGE : TF_SCORE_HEAL_HEALTHUNITS_PER_POINT ) ) +  // MvM values healing more than PvP
					( pRoundStats->m_iStat[TFSTAT_KILLASSISTS] / TF_SCORE_KILL_ASSISTS_PER_POINT ) + 
					( pRoundStats->m_iStat[TFSTAT_TELEPORTS] / TF_SCORE_TELEPORTS_PER_POINT ) +
					( pRoundStats->m_iStat[TFSTAT_INVULNS] / TF_SCORE_INVULN ) +
					( pRoundStats->m_iStat[TFSTAT_REVENGE] / TF_SCORE_REVENGE ) +
					( pRoundStats->m_iStat[TFSTAT_BONUS_POINTS] / TF_SCORE_BONUS_POINT_DIVISOR );
					( pRoundStats->m_iStat[TFSTAT_CURRENCY_COLLECTED] / TF_SCORE_CURRENCY_COLLECTED );

	// Previously MvM-only
	const int nDivisor = ( bMvM ) ? TF_SCORE_DAMAGE : TF_SCORE_HEAL_HEALTHUNITS_PER_POINT;
	iScore += ( pRoundStats->m_iStat[TFSTAT_DAMAGE] / nDivisor );
	iScore += ( pRoundStats->m_iStat[TFSTAT_DAMAGE_ASSIST] / nDivisor );
	iScore += ( pRoundStats->m_iStat[TFSTAT_DAMAGE_BOSS] / nDivisor );
	iScore += ( pRoundStats->m_iStat[TFSTAT_HEALING_ASSIST] / nDivisor );
	iScore += ( pRoundStats->m_iStat[TFSTAT_DAMAGE_BLOCKED] / nDivisor );

	return max( iScore, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::IsBirthday( void )
{
	if ( IsX360() )
		return false;

	if ( m_iBirthdayMode == BIRTHDAY_RECALCULATE )
	{
		m_iBirthdayMode = BIRTHDAY_OFF;
		if ( tf_birthday.GetBool() )
		{
			m_iBirthdayMode = BIRTHDAY_ON;
		}
		else
		{
			time_t ltime = time(0);
			const time_t *ptime = &ltime;
			struct tm *today = localtime( ptime );
			if ( today )
			{
				if ( today->tm_mon == 7 && today->tm_mday == 24 )
				{
					m_iBirthdayMode = BIRTHDAY_ON;
				}
			}
		}
	}

	return ( m_iBirthdayMode == BIRTHDAY_ON );
}

#if CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::GetTeamGlowColor( int nTeam, float &r, float &g, float &b )
{
	if ( nTeam == TF_TEAM_RED )
	{
		r = 0.74f;
		g = 0.23f;
		b = 0.23f;
	}
	else if ( nTeam == TF_TEAM_BLUE )
	{
		r = 0.49f;
		g = 0.66f;
		b = 0.77f;
	}
	else
	{
		r = 0.76f;
		g = 0.76f;
		b = 0.76f;
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		V_swap( collisionGroup0, collisionGroup1 );
	}
	
	//Don't stand on COLLISION_GROUP_WEAPONs
	if( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT &&
		collisionGroup1 == COLLISION_GROUP_WEAPON )
	{
		return false;
	}

	// Don't stand on projectiles
	if( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT &&
		collisionGroup1 == COLLISION_GROUP_PROJECTILE )
	{
		return false;
	}

	// Rockets need to collide with players when they hit, but
	// be ignored by player movement checks
	if ( ( collisionGroup0 == COLLISION_GROUP_PLAYER ) && 
		( collisionGroup1 == TFCOLLISION_GROUP_ROCKETS ) )
		return true;

	if ( ( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT ) && 
		( collisionGroup1 == TFCOLLISION_GROUP_ROCKETS ) )
		return false;

	if ( ( collisionGroup0 == COLLISION_GROUP_WEAPON ) && 
		( collisionGroup1 == TFCOLLISION_GROUP_ROCKETS ) )
		return false;

	if ( ( collisionGroup0 == TF_COLLISIONGROUP_GRENADES ) && 
		( collisionGroup1 == TFCOLLISION_GROUP_ROCKETS ) )
		return false;

	// Grenades don't collide with players. They handle collision while flying around manually.
	if ( ( collisionGroup0 == COLLISION_GROUP_PLAYER ) && 
		( collisionGroup1 == TF_COLLISIONGROUP_GRENADES ) )
		return false;

	if ( ( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT ) && 
		( collisionGroup1 == TF_COLLISIONGROUP_GRENADES ) )
		return false;

	// Respawn rooms only collide with players
	if ( collisionGroup1 == TFCOLLISION_GROUP_RESPAWNROOMS )
		return ( collisionGroup0 == COLLISION_GROUP_PLAYER ) || ( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT );
	
/*	if ( collisionGroup0 == COLLISION_GROUP_PLAYER )
	{
		// Players don't collide with objects or other players
		if ( collisionGroup1 == COLLISION_GROUP_PLAYER  )
			 return false;
 	}

	if ( collisionGroup1 == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		// This is only for probing, so it better not be on both sides!!!
		Assert( collisionGroup0 != COLLISION_GROUP_PLAYER_MOVEMENT );

		// No collide with players any more
		// Nor with objects or grenades
		switch ( collisionGroup0 )
		{
		default:
			break;
		case COLLISION_GROUP_PLAYER:
			return false;
		}
	}
*/
	// don't want caltrops and other grenades colliding with each other
	// caltops getting stuck on other caltrops, etc.)
	if ( ( collisionGroup0 == TF_COLLISIONGROUP_GRENADES ) && 
		 ( collisionGroup1 == TF_COLLISIONGROUP_GRENADES ) )
	{
		return false;
	}


	if ( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT &&
		collisionGroup1 == TFCOLLISION_GROUP_COMBATOBJECT )
	{
		return false;
	}

	if ( collisionGroup0 == COLLISION_GROUP_PLAYER &&
		collisionGroup1 == TFCOLLISION_GROUP_COMBATOBJECT )
	{
		return false;
	}

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 
}

//-----------------------------------------------------------------------------
// Purpose: Return the value of this player towards capturing a point
//-----------------------------------------------------------------------------
int	CTFGameRules::GetCaptureValueForPlayer( CBasePlayer *pPlayer )
{
	CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );
	if ( pTFPlayer->IsPlayerClass( TF_CLASS_SCOUT ) )
	{
		if ( mp_capstyle.GetInt() == 1 )
		{
			// Scouts count for 2 people in timebased capping.
			return 2;
		}
		else
		{
			// Scouts can cap all points on their own.
			return 10;
		}
	}

	return BaseClass::GetCaptureValueForPlayer( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFGameRules::GetTimeLeft( void )
{
	float flTimeLimit = mp_timelimit.GetInt() * 60;

	Assert( flTimeLimit > 0 && "Should not call this function when !IsGameUnderTimeLimit" );

	float flMapChangeTime = m_flMapResetTime + flTimeLimit;

	return ( (int)(flMapChangeTime - gpGlobals->curtime) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::FireGameEvent( IGameEvent *event )
{
	const char *eventName = event->GetName();

	if ( !Q_strcmp( eventName, "teamplay_point_captured" ) )
	{
#ifdef GAME_DLL
		RecalculateControlPointState();

		// keep track of how many times each team caps
		int iTeam = event->GetInt( "team" );
		Assert( iTeam >= FIRST_GAME_TEAM && iTeam < TF_TEAM_COUNT );
		m_iNumCaps[iTeam]++;

		// award a capture to all capping players
		const char *cappers = event->GetString( "cappers" );

		Q_strncpy( m_szMostRecentCappers, cappers, ARRAYSIZE( m_szMostRecentCappers ) );	
		for ( int i =0; i < Q_strlen( cappers ); i++ )
		{
			int iPlayerIndex = (int) cappers[i];
			CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
			if ( pPlayer )
			{
				CTF_GameStats.Event_PlayerCapturedPoint( pPlayer );				
			}
		}
#endif
	}
	else if ( !Q_strcmp( eventName, "teamplay_capture_blocked" ) )
	{
#ifdef GAME_DLL
		int iPlayerIndex = event->GetInt( "blocker" );
		CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
		CTF_GameStats.Event_PlayerDefendedPoint( pPlayer );
#endif
	}	
	else if ( !Q_strcmp( eventName, "teamplay_round_win" ) )
	{
#ifdef GAME_DLL
		int iWinningTeam = event->GetInt( "team" );
		bool bFullRound = event->GetBool( "full_round" );
		float flRoundTime = event->GetFloat( "round_time" );
		bool bWasSuddenDeath = event->GetBool( "was_sudden_death" );
		CTF_GameStats.Event_RoundEnd( iWinningTeam, bFullRound, flRoundTime, bWasSuddenDeath );
#endif
	}
	else if ( !Q_strcmp( eventName, "teamplay_flag_event" ) )
	{
#ifdef GAME_DLL
		// if this is a capture event, remember the player who made the capture		
		int iEventType = event->GetInt( "eventtype" );
		if ( TF_FLAGEVENT_CAPTURE == iEventType )
		{
			int iPlayerIndex = event->GetInt( "player" );
			m_szMostRecentCappers[0] = iPlayerIndex;
			m_szMostRecentCappers[1] = 0;
		}
#endif
	}
#ifdef CLIENT_DLL
	else if ( !Q_strcmp( eventName, "game_newmap" ) )
	{
		m_iBirthdayMode = BIRTHDAY_RECALCULATE;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Init ammo definitions
//-----------------------------------------------------------------------------

// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

// exaggerate all of the forces, but use real numbers to keep them consistent
#define BULLET_IMPULSE_EXAGGERATION			1	

// convert a velocity in ft/sec and a mass in grains to an impulse in kg in/s
#define BULLET_IMPULSE(grains, ftpersec)	((ftpersec)*12*BULLET_MASS_GRAINS_TO_KG(grains)*BULLET_IMPULSE_EXAGGERATION)

// CSS
ConVar ammo_50AE_max( "ammo_50AE_max", "35", FCVAR_REPLICATED );
ConVar ammo_762mm_max( "ammo_762mm_max", "90", FCVAR_REPLICATED );
ConVar ammo_556mm_max( "ammo_556mm_max", "90", FCVAR_REPLICATED );
ConVar ammo_556mm_box_max( "ammo_556mm_box_max", "200", FCVAR_REPLICATED );
ConVar ammo_338mag_max( "ammo_338mag_max", "30", FCVAR_REPLICATED );
ConVar ammo_9mm_max( "ammo_9mm_max", "120", FCVAR_REPLICATED );
ConVar ammo_buckshot_max( "ammo_buckshot_max", "32", FCVAR_REPLICATED );
ConVar ammo_45acp_max( "ammo_45acp_max", "100", FCVAR_REPLICATED );
ConVar ammo_357sig_max( "ammo_357sig_max", "52", FCVAR_REPLICATED );
ConVar ammo_57mm_max( "ammo_57mm_max", "100", FCVAR_REPLICATED );
ConVar ammo_hegrenade_max( "ammo_hegrenade_max", "1", FCVAR_REPLICATED );
ConVar ammo_flashbang_max( "ammo_flashbang_max", "2", FCVAR_REPLICATED );
ConVar ammo_smokegrenade_max( "ammo_smokegrenade_max", "1", FCVAR_REPLICATED );

CAmmoDef* GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;

	if ( !bInitted )
	{
		bInitted = true;

		// Start at 1 here and skip the dummy ammo type to make CAmmoDef use the same indices
		// as our #defines.
		for ( int i=1; i < TF_AMMO_COUNT; i++ )
		{
			def.AddAmmoType( g_aAmmoNames[i], DMG_BULLET, TRACER_LINE, 0, 0, "ammo_max", 2400, 10, 14 );
			Assert( def.Index( g_aAmmoNames[i] ) == i );
		}

		//pistol ammo
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_COLT],		DMG_BULLET, TRACER_NONE,	0, 0, "ammo_max",	5000, 10, 14 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_P38],		DMG_BULLET, TRACER_NONE,	0, 0, "ammo_max",	5000, 10, 14 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_C96],		DMG_BULLET, TRACER_NONE,	0, 0, "ammo_max",	5000, 10, 14 );
		
		//rifles
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_GARAND],		DMG_BULLET, TRACER_NONE,	0, 0,		"ammo_max",		9000, 10, 14 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_K98],		DMG_BULLET, TRACER_NONE,	0, 0,		"ammo_max",		9000, 10, 14 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_M1CARBINE],	DMG_BULLET, TRACER_NONE,	0, 0,		"ammo_max",		9000, 10, 14 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_SPRING],		DMG_BULLET, TRACER_NONE,	0, 0,		"ammo_max",		9000, 10, 14 );

		//submg
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_SUBMG],		DMG_BULLET, TRACER_NONE,			0, 0, "ammo_max",		7000, 10, 14 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_BAR],		DMG_BULLET, TRACER_LINE_AND_WHIZ,	0, 0, "ammo_max",		9000, 10, 14 );

		//mg
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_30CAL],		DMG_BULLET | DMG_MACHINEGUN, TRACER_LINE_AND_WHIZ, 0, 0, "ammo_max", 9000, 10, 14 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_MG42],		DMG_BULLET | DMG_MACHINEGUN, TRACER_LINE_AND_WHIZ,	0, 0, "ammo_max", 9000, 10, 14 );

		//rockets
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_ROCKET],		DMG_BLAST,	TRACER_NONE,			0, 0, "ammo_max",	9000, 10, 14 );

		//grenades
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_HANDGRENADE],			DMG_BLAST,	TRACER_NONE,		0, 0, "ammo_max", 1, 4, 8 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_STICKGRENADE],			DMG_BLAST,	TRACER_NONE,		0, 0, "ammo_max", 1, 4, 8 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_HANDGRENADE_EX],			DMG_BLAST,	TRACER_NONE,		0, 0, "ammo_max", 1, 4, 8 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_STICKGRENADE_EX],		DMG_BLAST,	TRACER_NONE,		0, 0, "ammo_max", 1, 4, 8 );

		// smoke grenades
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_SMOKEGRENADE_US],		DMG_BLAST,	TRACER_NONE,	0, 0, "ammo_max", 1, 4, 8 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_SMOKEGRENADE_GER],		DMG_BLAST,	TRACER_NONE,	0, 0, "ammo_max", 1, 4, 8 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_SMOKEGRENADE_US_LIVE],	DMG_BLAST,	TRACER_NONE,	0, 0, "ammo_max", 1, 4, 8 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_SMOKEGRENADE_GER_LIVE],	DMG_BLAST,	TRACER_NONE,	0, 0, "ammo_max", 1, 4, 8 );

		// rifle grenades
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_RIFLEGRENADE_US],		DMG_BLAST,	TRACER_NONE,	0, 0, "ammo_max", 1, 4, 8 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_RIFLEGRENADE_GER],		DMG_BLAST,	TRACER_NONE,	0, 0, "ammo_max", 1, 4, 8 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_RIFLEGRENADE_US_LIVE],	DMG_BLAST,	TRACER_NONE,	0, 0, "ammo_max", 1, 4, 8 );
		def.AddAmmoType( g_aAmmoNames[DOD_AMMO_RIFLEGRENADE_GER_LIVE],	DMG_BLAST,	TRACER_NONE,	0, 0, "ammo_max", 1, 4, 8 );

		def.AddAmmoType( g_aAmmoNames[BULLET_PLAYER_50AE],				DMG_BULLET, TRACER_LINE, 0, 0, "ammo_50AE_max",		2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		def.AddAmmoType( g_aAmmoNames[BULLET_PLAYER_762MM],				DMG_BULLET, TRACER_LINE, 0, 0, "ammo_762mm_max",	2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		def.AddAmmoType( g_aAmmoNames[BULLET_PLAYER_556MM],				DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_max",	2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		def.AddAmmoType( g_aAmmoNames[BULLET_PLAYER_556MM_BOX],			DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_box_max",2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		def.AddAmmoType( g_aAmmoNames[BULLET_PLAYER_338MAG],			DMG_BULLET, TRACER_LINE, 0, 0, "ammo_338mag_max",	2800 * BULLET_IMPULSE_EXAGGERATION, 0, 12, 16 );
		def.AddAmmoType( g_aAmmoNames[BULLET_PLAYER_9MM],				DMG_BULLET, TRACER_LINE, 0, 0, "ammo_9mm_max",		2000 * BULLET_IMPULSE_EXAGGERATION, 0, 5, 10 );
		def.AddAmmoType( g_aAmmoNames[BULLET_PLAYER_BUCKSHOT],			DMG_BULLET, TRACER_LINE, 0, 0, "ammo_buckshot_max", 600 * BULLET_IMPULSE_EXAGGERATION,  0, 3, 6 );
		def.AddAmmoType( g_aAmmoNames[BULLET_PLAYER_45ACP],				DMG_BULLET, TRACER_LINE, 0, 0, "ammo_45acp_max",	2100 * BULLET_IMPULSE_EXAGGERATION, 0, 6, 10 );
		def.AddAmmoType( g_aAmmoNames[BULLET_PLAYER_357SIG],			DMG_BULLET, TRACER_LINE, 0, 0, "ammo_357sig_max",	2000 * BULLET_IMPULSE_EXAGGERATION, 0, 4, 8 );
		def.AddAmmoType( g_aAmmoNames[BULLET_PLAYER_57MM],				DMG_BULLET, TRACER_LINE, 0, 0, "ammo_57mm_max",		2000 * BULLET_IMPULSE_EXAGGERATION, 0, 4, 8 );
		def.AddAmmoType( g_aAmmoNames[AMMO_TYPE_HEGRENADE],				DMG_BLAST,	TRACER_LINE, 0, 0, "ammo_hegrenade_max", 1, 0 );
		def.AddAmmoType( g_aAmmoNames[AMMO_TYPE_FLASHBANG],				0,			TRACER_LINE, 0,	0, "ammo_flashbang_max", 1, 0 );
		def.AddAmmoType( g_aAmmoNames[AMMO_TYPE_SMOKEGRENADE],			0,			TRACER_LINE, 0, 0, "ammo_smokegrenade_max", 1, 0 );

		//Adrian: I set all the prices to 0 just so the rest of the buy code works
		//This should be revisited.
		// SEALTODO: MONEY
		//def.AddAmmoCost( g_aAmmoNames[BULLET_PLAYER_50AE], 0, 7 );
		//def.AddAmmoCost( g_aAmmoNames[BULLET_PLAYER_762MM], 0, 30 );
		//def.AddAmmoCost( g_aAmmoNames[BULLET_PLAYER_556MM], 0, 30 );
		//def.AddAmmoCost( g_aAmmoNames[BULLET_PLAYER_556MM_BOX], 0, 30 );
		//def.AddAmmoCost( g_aAmmoNames[BULLET_PLAYER_338MAG], 0, 10 );
		//def.AddAmmoCost( g_aAmmoNames[BULLET_PLAYER_9MM], 0, 30 );
		//def.AddAmmoCost( g_aAmmoNames[BULLET_PLAYER_BUCKSHOT], 0, 8 );
		//def.AddAmmoCost( g_aAmmoNames[BULLET_PLAYER_45ACP], 0, 25 );
		//def.AddAmmoCost( g_aAmmoNames[BULLET_PLAYER_357SIG], 0, 13 );
		//def.AddAmmoCost( g_aAmmoNames[BULLET_PLAYER_57MM], 0, 50 );

		def.AddAmmoType( g_aAmmoNames[HL2_AMMO_AR2],				DMG_BULLET,					TRACER_LINE_AND_WHIZ,	0,			0,			60,			BULLET_IMPULSE(200, 1225),	0 );
		def.AddAmmoType( g_aAmmoNames[HL2_AMMO_AR2ALTFIRE],			DMG_DISSOLVE,				TRACER_NONE,			0,			0,			3,			0,							0 );
		def.AddAmmoType( g_aAmmoNames[HL2_AMMO_PISTOL],				DMG_BULLET,					TRACER_LINE_AND_WHIZ,	0,			0,			150,		BULLET_IMPULSE(200, 1225),	0 );
		def.AddAmmoType( g_aAmmoNames[HL2_AMMO_SMG1],				DMG_BULLET,					TRACER_LINE_AND_WHIZ,	0,			0,			225,		BULLET_IMPULSE(200, 1225),	0 );
		def.AddAmmoType( g_aAmmoNames[HL2_AMMO_357],				DMG_BULLET,					TRACER_LINE_AND_WHIZ,	0,			0,			12,			BULLET_IMPULSE(800, 5000),	0 );
		def.AddAmmoType( g_aAmmoNames[HL2_AMMO_XBOWBOLT],			DMG_BULLET,					TRACER_LINE,			0,			0,			10,			BULLET_IMPULSE(800, 8000),	0 );
		def.AddAmmoType( g_aAmmoNames[HL2_AMMO_BUCKSHOT],			DMG_BULLET | DMG_BUCKSHOT,	TRACER_LINE,			0,			0,			30,			BULLET_IMPULSE(400, 1200),	0 );
		def.AddAmmoType( g_aAmmoNames[HL2_AMMO_RPGROUND],			DMG_BURN,					TRACER_NONE,			0,			0,			3,			0,							0 );
		def.AddAmmoType( g_aAmmoNames[HL2_AMMO_SMG1GRENADE],		DMG_BURN,					TRACER_NONE,			0,			0,			3,			0,							0 );
		def.AddAmmoType( g_aAmmoNames[HL2_AMMO_GRENADE],			DMG_BURN,					TRACER_NONE,			0,			0,			5,			0,							0 );
		def.AddAmmoType( g_aAmmoNames[HL2_AMMO_SLAM],				DMG_BURN,					TRACER_NONE,			0,			0,			5,			0,							0 );
	}

	return &def;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTFGameRules::GetTeamGoalString( int iTeam )
{
	if ( iTeam == TF_TEAM_RED )
		return m_pszTeamGoalStringRed.Get();
	if ( iTeam == TF_TEAM_BLUE )
		return m_pszTeamGoalStringBlue.Get();
	return NULL;
}

#ifdef GAME_DLL

	Vector MaybeDropToGround( 
							CBaseEntity *pMainEnt, 
							bool bDropToGround, 
							const Vector &vPos, 
							const Vector &vMins, 
							const Vector &vMaxs )
	{
		if ( bDropToGround )
		{
			trace_t trace;
			UTIL_TraceHull( vPos, vPos + Vector( 0, 0, -500 ), vMins, vMaxs, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &trace );
			return trace.endpos;
		}
		else
		{
			return vPos;
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: This function can be used to find a valid placement location for an entity.
	//			Given an origin to start looking from and a minimum radius to place the entity at,
	//			it will sweep out a circle around vOrigin and try to find a valid spot (on the ground)
	//			where mins and maxs will fit.
	// Input  : *pMainEnt - Entity to place
	//			&vOrigin - Point to search around
	//			fRadius - Radius to search within
	//			nTries - Number of tries to attempt
	//			&mins - mins of the Entity
	//			&maxs - maxs of the Entity
	//			&outPos - Return point
	// Output : Returns true and fills in outPos if it found a spot.
	//-----------------------------------------------------------------------------
	bool EntityPlacementTest( CBaseEntity *pMainEnt, const Vector &vOrigin, Vector &outPos, bool bDropToGround )
	{
		// This function moves the box out in each dimension in each step trying to find empty space like this:
		//
		//											  X  
		//							   X			  X  
		// Step 1:   X     Step 2:    XXX   Step 3: XXXXX
		//							   X 			  X  
		//											  X  
		//

		Vector mins, maxs;
		pMainEnt->CollisionProp()->WorldSpaceAABB( &mins, &maxs );
		mins -= pMainEnt->GetAbsOrigin();
		maxs -= pMainEnt->GetAbsOrigin();

		// Put some padding on their bbox.
		float flPadSize = 5;
		Vector vTestMins = mins - Vector( flPadSize, flPadSize, flPadSize );
		Vector vTestMaxs = maxs + Vector( flPadSize, flPadSize, flPadSize );

		// First test the starting origin.
		if ( UTIL_IsSpaceEmpty( pMainEnt, vOrigin + vTestMins, vOrigin + vTestMaxs ) )
		{
			outPos = MaybeDropToGround( pMainEnt, bDropToGround, vOrigin, vTestMins, vTestMaxs );
			return true;
		}

		Vector vDims = vTestMaxs - vTestMins;


		// Keep branching out until we get too far.
		int iCurIteration = 0;
		int nMaxIterations = 15;

		int offset = 0;
		do
		{
			for ( int iDim=0; iDim < 3; iDim++ )
			{
				float flCurOffset = offset * vDims[iDim];

				for ( int iSign=0; iSign < 2; iSign++ )
				{
					Vector vBase = vOrigin;
					vBase[iDim] += (iSign*2-1) * flCurOffset;

					if ( UTIL_IsSpaceEmpty( pMainEnt, vBase + vTestMins, vBase + vTestMaxs ) )
					{
						// Ensure that there is a clear line of sight from the spawnpoint entity to the actual spawn point.
						// (Useful for keeping things from spawning behind walls near a spawn point)
						trace_t tr;
						UTIL_TraceLine( vOrigin, vBase, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &tr );

						if ( tr.fraction != 1.0 )
						{
							continue;
						}

						outPos = MaybeDropToGround( pMainEnt, bDropToGround, vBase, vTestMins, vTestMaxs );
						return true;
					}
				}
			}

			++offset;
		} while ( iCurIteration++ < nMaxIterations );

		//	Warning( "EntityPlacementTest for ent %d:%s failed!\n", pMainEnt->entindex(), pMainEnt->GetClassname() );
		return false;
	}

#else // GAME_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( State_Get() == GR_STATE_STARTGAME )
	{
		m_iBirthdayMode = BIRTHDAY_RECALCULATE;
	}
}

void CTFGameRules::HandleOvertimeBegin()
{
	C_TFPlayer *pTFPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( pTFPlayer )
	{
		pTFPlayer->EmitSound( "Game.Overtime" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGameRules::ShouldShowTeamGoal( void )
{
	if ( State_Get() == GR_STATE_PREROUND || State_Get() == GR_STATE_RND_RUNNING || InSetup() )
		return true;

	return false;
}

#endif

#ifdef GAME_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::ShutdownCustomResponseRulesDicts()
{
	DestroyCustomResponseSystems();

	if ( m_ResponseRules.Count() != 0 )
	{
		int nRuleCount = m_ResponseRules.Count();
		for ( int iRule = 0; iRule < nRuleCount; ++iRule )
		{
			m_ResponseRules[iRule].m_ResponseSystems.Purge();
		}
		m_ResponseRules.Purge();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::InitCustomResponseRulesDicts()
{
	MEM_ALLOC_CREDIT();

	// Clear if necessary.
	ShutdownCustomResponseRulesDicts();

	// Initialize the response rules for TF.
	m_ResponseRules.AddMultipleToTail( TF_CLASS_COUNT_ALL );

	// SEALTODO: this is where we want to parse our custom game reponse rules

	char szName[512];
	for ( int iClass = TF_FIRST_NORMAL_CLASS; iClass < TF_CLASS_COUNT_ALL; ++iClass )
	{
		m_ResponseRules[iClass].m_ResponseSystems.AddMultipleToTail( MP_TF_CONCEPT_COUNT );

		for ( int iConcept = 0; iConcept < MP_TF_CONCEPT_COUNT; ++iConcept )
		{
			AI_CriteriaSet criteriaSet;
			criteriaSet.AppendCriteria( "playerclass", g_aPlayerClassNames_NonLocalized[iClass] );
			criteriaSet.AppendCriteria( "Concept", g_pszMPConcepts[iConcept] );

			// 1 point for player class and 1 point for concept.
			float flCriteriaScore = 2.0f;

			// Name.
			V_snprintf( szName, sizeof( szName ), "%s_%s\n", g_aPlayerClassNames_NonLocalized[iClass], g_pszMPConcepts[iConcept] );
			m_ResponseRules[iClass].m_ResponseSystems[iConcept] = BuildCustomResponseSystemGivenCriteria( "scripts/talker/response_rules.txt", szName, criteriaSet, flCriteriaScore );
		}		
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::SendHudNotification( IRecipientFilter &filter, HudNotification_t iType )
{
	UserMessageBegin( filter, "HudNotify" );
		WRITE_BYTE( iType );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::SendHudNotification( IRecipientFilter &filter, const char *pszText, const char *pszIcon, int iTeam /*= TEAM_UNASSIGNED*/ )
{
	UserMessageBegin( filter, "HudNotifyCustom" );
		WRITE_STRING( pszText );
		WRITE_STRING( pszIcon );
		WRITE_BYTE( iTeam );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: Is the player past the required delays for spawning
//-----------------------------------------------------------------------------
bool CTFGameRules::HasPassedMinRespawnTime( CBasePlayer *pPlayer )
{
	CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );

	if ( pTFPlayer && pTFPlayer->GetPlayerClass()->GetClassIndex() == TF_CLASS_UNDEFINED )
		return true;

	float flMinSpawnTime = GetMinTimeWhenPlayerMaySpawn( pPlayer ); 

	return ( gpGlobals->curtime > flMinSpawnTime );
}


#endif


#ifdef CLIENT_DLL
const char *CTFGameRules::GetVideoFileForMap( bool bWithExtension /*= true*/ )
{
	char mapname[MAX_MAP_NAME];

	Q_FileBase( engine->GetLevelName(), mapname, sizeof( mapname ) );
	Q_strlower( mapname );

#ifdef _X360
	// need to remove the .360 extension on the end of the map name
	char *pExt = Q_stristr( mapname, ".360" );
	if ( pExt )
	{
		*pExt = '\0';
	}
#endif

	static char strFullpath[MAX_PATH];
	Q_strncpy( strFullpath, "media/", MAX_PATH );	// Assume we must play out of the media directory
	Q_strncat( strFullpath, mapname, MAX_PATH );

	if ( bWithExtension )
	{
		Q_strncat( strFullpath, ".bik", MAX_PATH );		// Assume we're a .bik extension type
	}

	return strFullpath;
}
#endif

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::OnDispenserBuilt( CBaseEntity *dispenser )
{
	if ( !m_healthVector.HasElement( dispenser ) )
	{
		m_healthVector.AddToTail( dispenser );
	}

	if ( !m_ammoVector.HasElement( dispenser ) )
	{
		m_ammoVector.AddToTail( dispenser );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameRules::OnDispenserDestroyed( CBaseEntity *dispenser )
{
	m_healthVector.FindAndFastRemove( dispenser );
	m_ammoVector.FindAndFastRemove( dispenser );
}

//-----------------------------------------------------------------------------
// Purpose: Compute internal vectors of health and ammo locations
//-----------------------------------------------------------------------------
void CTFGameRules::ComputeHealthAndAmmoVectors( void )
{
	m_ammoVector.RemoveAll();
	m_healthVector.RemoveAll();

	CBaseEntity *pEnt = gEntList.FirstEnt();
	while( pEnt )
	{
		if ( pEnt->ClassMatches( "func_regenerate" ) || pEnt->ClassMatches( "item_healthkit*" ) )
		{
			m_healthVector.AddToTail( pEnt );
		}

		if ( pEnt->ClassMatches( "func_regenerate" ) || pEnt->ClassMatches( "item_ammopack*" ) )
		{
			m_ammoVector.AddToTail( pEnt );
		}

		pEnt = gEntList.NextEnt( pEnt );
	}

	m_areHealthAndAmmoVectorsReady = true;
}


//-----------------------------------------------------------------------------
// Purpose: Return vector of health entities
//-----------------------------------------------------------------------------
const CUtlVector< CHandle< CBaseEntity > > &CTFGameRules::GetHealthEntityVector( void )
{
	// lazy-populate health and ammo vector since some maps (Dario!) move these entities around between stages
	if ( !m_areHealthAndAmmoVectorsReady )
	{
		ComputeHealthAndAmmoVectors();
	}

	return m_healthVector;
}


//-----------------------------------------------------------------------------
// Purpose: Return vector of ammo entities
//-----------------------------------------------------------------------------
const CUtlVector< CHandle< CBaseEntity > > &CTFGameRules::GetAmmoEntityVector( void )
{
	// lazy-populate health and ammo vector since some maps (Dario!) move these entities around between stages
	if ( !m_areHealthAndAmmoVectorsReady )
	{
		ComputeHealthAndAmmoVectors();
	}

	return m_ammoVector;
}

bool PropDynamic_CollidesWithGrenades( CBaseEntity *pBaseEntity )
{
	//CTFTrainingDynamicProp *pTrainingDynamicProp = dynamic_cast< CTFTrainingDynamicProp* >( pBaseEntity );
	//return ( pTrainingDynamicProp && pTrainingDynamicProp->HasSpawnFlags( SF_TF_DYNAMICPROP_GRENADE_COLLISION) );
	return false;
}
#endif

//////////////////////////////// Day Of Defeat ////////////////////////////////
#ifdef GAME_DLL
bool CTFGameRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
{
	if ( pPlayer && !ToTFPlayer( pPlayer )->IsDODClass() )
		return BaseClass::CanHavePlayerItem( pPlayer, pWeapon );

	//only allow one primary, one secondary and one melee
	CWeaponDODBase *pWpn = (CWeaponDODBase *)pWeapon;

	if( pWpn )
	{
		int type = pWpn->GetPCWpnData().m_DODWeaponData.m_WeaponType;

		switch( type )
		{
		case WPN_TYPE_MELEE:
			{
#ifdef DEBUG
				CWeaponDODBase *pMeleeWeapon = (CWeaponDODBase *)pPlayer->Weapon_GetSlot( WPN_SLOT_MELEE );
				bool bHasMelee = ( pMeleeWeapon != NULL );

				if( bHasMelee )
				{
					Assert( !"Why are we trying to add another melee?" );
					return false;
				}
#endif
			}
			break;
		case WPN_TYPE_PISTOL:
		case WPN_TYPE_SIDEARM:
			{
#ifdef DEBUG
				CWeaponDODBase *pSecondaryWeapon = (CWeaponDODBase *)pPlayer->Weapon_GetSlot( WPN_SLOT_SECONDARY );
				bool bHasPistol = ( pSecondaryWeapon != NULL );

				if( bHasPistol )
				{
					Assert( !"Why are we trying to add another pistol?" );
					return false;
				}
#endif
			}
			break;

		case WPN_TYPE_CAMERA:
			return true;

		case WPN_TYPE_RIFLE:
		case WPN_TYPE_SNIPER:
		case WPN_TYPE_SUBMG:
		case WPN_TYPE_MG:
		case WPN_TYPE_BAZOOKA:
			{
				//Don't pick up dropped weapons if we have one already
				CWeaponDODBase *pPrimaryWeapon = (CWeaponDODBase *)pPlayer->Weapon_GetSlot( WPN_SLOT_PRIMARY );
				bool bHasPrimary = ( pPrimaryWeapon != NULL );

				if( bHasPrimary )
					return false;
			}
			break;

		default:
			break;
		}
	}

	return BaseClass::CanHavePlayerItem( pPlayer, pWeapon );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the weapon in the player's inventory that would be better than
//			the given weapon.
// Note, this version allows us to switch to a weapon that has no ammo as a last
// resort.
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CTFGameRules::GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
{
	if ( pPlayer && !ToTFPlayer( pPlayer )->IsDODClass() )
		return BaseClass::GetNextBestWeapon( pPlayer, pCurrentWeapon );

	CBaseCombatWeapon *pCheck;
	CBaseCombatWeapon *pBest;// this will be used in the event that we don't find a weapon in the same category.

	int iCurrentWeight = -1;
	int iBestWeight = -1;// no weapon lower than -1 can be autoswitched to
	pBest = NULL;

	// If I have a weapon, make sure I'm allowed to holster it
	if ( pCurrentWeapon )
	{
		if ( !pCurrentWeapon->AllowsAutoSwitchFrom() || !pCurrentWeapon->CanHolster() )
		{
			// Either this weapon doesn't allow autoswitching away from it or I
			// can't put this weapon away right now, so I can't switch.
			return NULL;
		}

		iCurrentWeight = pCurrentWeapon->GetWeight();
	}

	for ( int i = 0 ; i < pPlayer->WeaponCount(); ++i )
	{
		pCheck = pPlayer->GetWeapon( i );
		if ( !pCheck )
			continue;

		// If we have an active weapon and this weapon doesn't allow autoswitching away
		// from another weapon, skip it.
		if ( pCurrentWeapon && !pCheck->AllowsAutoSwitchTo() )
			continue;

		int iWeight = pCheck->GetWeight();

		// Empty weapons are lowest priority
		if ( !pCheck->HasAnyAmmo() )
		{
			iWeight = 0;
		}

		if ( iWeight > -1 && iWeight == iCurrentWeight && pCheck != pCurrentWeapon )
		{
			// this weapon is from the same category. 
			if ( pPlayer->Weapon_CanSwitchTo( pCheck ) )
			{
				return pCheck;
			}
		}
		else if ( iWeight > iBestWeight && pCheck != pCurrentWeapon )// don't reselect the weapon we're trying to get rid of
		{
			//Msg( "Considering %s\n", STRING( pCheck->GetClassname() );
			// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
			// that the player was using. This will end up leaving the player with his heaviest-weighted 
			// weapon. 

			// if this weapon is useable, flag it as the best
			iBestWeight = pCheck->GetWeight();
			pBest = pCheck;
		}
	}

	// if we make it here, we've checked all the weapons and found no useable 
	// weapon in the same catagory as the current weapon. 

	// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always 
	// at least get the crowbar, but ya never know.
	return pBest;
}
#endif


											//////////// DAY OF DEFEAT ///////////
#ifdef CLIENT_DLL
#else
void CTFGameRules::CheckPlayerPositions()
{
	int i;
	bool bUpdatePlayer[MAX_PLAYERS];
	Q_memset( bUpdatePlayer, 0, sizeof(bUpdatePlayer) );

	// check all players
	for ( i=1; i<=gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		Vector origin = pPlayer->GetAbsOrigin();

		Vector2D pos( (int)(origin.x/4), (int)(origin.y/4) );

		if ( pos == m_vecPlayerPositions[i-1] )
			continue; // player didn't move enough

		m_vecPlayerPositions[i-1] = pos;

		bUpdatePlayer[i-1] = true; // player position changed since last time
	}

	// ok, now send updates to all clients
	CBitVec< ABSOLUTE_PLAYER_LIMIT > playerbits;

	for ( i=1; i<=gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			
		if ( !pPlayer )
			continue;

		if ( !pPlayer->IsConnected() )
			continue;

		CSingleUserRecipientFilter filter(pPlayer);

		UserMessageBegin( filter, "UpdateRadar" );

		playerbits.ClearAll();
				
		// see what other players are in it's PVS, don't update them
		engine->Message_DetermineMulticastRecipients( false, pPlayer->EyePosition(), playerbits );

		for ( int i=0; i < gpGlobals->maxClients; i++ )
		{
			if ( playerbits.Get(i)	)
				continue; // this player is in his PVS, don't update radar pos

			if ( !bUpdatePlayer[i] )
				continue;

			CBasePlayer *pOtherPlayer = UTIL_PlayerByIndex( i+1 );

			if ( !pOtherPlayer )
				continue; // nothing there

			if ( pOtherPlayer == pPlayer )
				continue; // dont update himself

			if ( !pOtherPlayer->IsAlive() || pOtherPlayer->IsObserver() || !pOtherPlayer->IsConnected() )
				continue; // don't update spectators or dead players

			if ( pPlayer->GetTeamNumber() > TEAM_SPECTATOR )
			{
				// update only team mates if not a pure spectator
				if ( pPlayer->GetTeamNumber() != pOtherPlayer->GetTeamNumber() )
					continue;
			}

			WRITE_BYTE( i+1 ); // player entity index 
			WRITE_SBITLONG( m_vecPlayerPositions[i].x, COORD_INTEGER_BITS-1 );
			WRITE_SBITLONG( m_vecPlayerPositions[i].y, COORD_INTEGER_BITS-1 );
			WRITE_SBITLONG( AngleNormalize( pOtherPlayer->GetAbsAngles().y ), 9 );
		}

		WRITE_BYTE( 0 ); // end marker

		MessageEnd();	// send message
	}
}
#endif