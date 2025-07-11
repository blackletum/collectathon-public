//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "tf_player.h"
#include "tf_gamerules.h"
#include "tf_gamestats.h"
#include "KeyValues.h"
#include "viewport_panel_names.h"
#include "client.h"
#include "team.h"
#include "tf_client.h"
#include "tf_team.h"
#include "tf_viewmodel.h"
#include "tf_item.h"
#include "in_buttons.h"
#include "entity_capture_flag.h"
#include "effect_dispatch_data.h"
#include "te_effect_dispatch.h"
#include "game.h"
#include "tf_weapon_builder.h"
#include "tf_obj.h"
#include "tf_ammo_pack.h"
#include "datacache/imdlcache.h"
#include "particle_parse.h"
#include "props_shared.h"
#include "filesystem.h"
#include "toolframework_server.h"
#include "IEffects.h"
#include "func_respawnroom.h"
#include "networkstringtable_gamedll.h"
#include "team_control_point_master.h"
#include "tf_weapon_pda.h"
#include "sceneentity.h"
#include "fmtstr.h"
#include "trigger_area_capture.h"
#include "triggers.h"
#include "tf_weapon_medigun.h"
#include "hl2orange.spa.h"
#include "te_tfblood.h"
#include "activitylist.h"
#include "steam/steam_api.h"
#include "cdll_int.h"
#include "tf_obj_sentrygun.h"

// Weapons
#include "tf_weaponbase.h"
#include "tf_weapon_sniperrifle.h"
#include "tf_weapon_minigun.h"
#include "tf_weapon_grenade_pipebomb.h"
#include "tf_weapon_flamethrower.h"

// DOD
#include "weapon_dodbase.h"
#include "dod_viewmodel.h"
#include "basecombatweapon_shared.h"
#include "ammodef.h"
#include "engine/IEngineSound.h"

// CSS
#include "weapon_csbase.h"
#include "predicted_viewmodel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#pragma warning( disable: 4355 ) // disables ' 'this' : used in base member initializer list'

#define DAMAGE_FORCE_SCALE_SELF				9
#define SCOUT_ADD_BIRD_ON_GIB_CHANCE		5
#define MEDIC_RELEASE_DOVE_COUNT			10

#define JUMP_MIN_SPEED	268.3281572999747f	

extern bool IsInCommentaryMode( void );

extern ConVar	sk_player_head;
extern ConVar	sk_player_chest;
extern ConVar	sk_player_stomach;
extern ConVar	sk_player_arm;
extern ConVar	sk_player_leg;

extern ConVar	tf_spy_invis_time;
extern ConVar	tf_spy_invis_unstealth_time;
extern ConVar	tf_stalematechangeclasstime;

EHANDLE g_pLastSpawnPoints[TF_TEAM_COUNT];

ConVar tf_playerstatetransitions( "tf_playerstatetransitions", "-2", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "tf_playerstatetransitions <ent index or -1 for all>. Show player state transitions." );
ConVar tf_playergib( "tf_playergib", "1", FCVAR_PROTECTED, "Allow player gibbing." );

ConVar tf_weapon_ragdoll_velocity_min( "tf_weapon_ragdoll_velocity_min", "100", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_weapon_ragdoll_velocity_max( "tf_weapon_ragdoll_velocity_max", "150", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_weapon_ragdoll_maxspeed( "tf_weapon_ragdoll_maxspeed", "300", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

ConVar tf_damageforcescale_other( "tf_damageforcescale_other", "6.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_damageforcescale_self_soldier_rj( "tf_damageforcescale_self_soldier_rj", "10.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_damageforcescale_self_soldier_badrj( "tf_damageforcescale_self_soldier_badrj", "5.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_damageforcescale_pyro_jump( "tf_damageforcescale_pyro_jump", "8.5", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar tf_damagescale_self_soldier( "tf_damagescale_self_soldier", "0.60", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

ConVar tf_damage_lineardist( "tf_damage_lineardist", "0", FCVAR_DEVELOPMENTONLY );
ConVar tf_damage_range( "tf_damage_range", "0.5", FCVAR_DEVELOPMENTONLY );

ConVar tf_max_voice_speak_delay( "tf_max_voice_speak_delay", "1.5", FCVAR_DEVELOPMENTONLY, "Max time after a voice command until player can do another one" );

ConVar tf_allow_player_use( "tf_allow_player_use", "0", FCVAR_NOTIFY, "Allow players to execute +use while playing." );

extern ConVar spec_freeze_time;
extern ConVar spec_freeze_traveltime;
extern ConVar sv_maxunlag;
extern ConVar tf_damage_disablespread;
extern ConVar tf_gravetalk;
extern ConVar tf_spectalk;

extern ConVar tf_allow_taunt_switch;

#define DOD_DEAFEN_CONTEXT	"DeafenContext"

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class CTEPlayerAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEPlayerAnimEvent, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTEPlayerAnimEvent( const char *name ) : CBaseTempEntity( name )
	{
		m_iPlayerIndex = TF_PLAYER_INDEX_NONE;
	}

	CNetworkVar( int, m_iPlayerIndex );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	SendPropInt( SENDINFO( m_iPlayerIndex ), 7, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iEvent ), Q_log2( PLAYERANIMEVENT_COUNT ) + 1, SPROP_UNSIGNED ),
	// BUGBUG:  ywb  we assume this is either 0 or an animation sequence #, but it could also be an activity, which should fit within this limit, but we're not guaranteed.
	SendPropInt( SENDINFO( m_nData ), ANIMATION_SEQUENCE_BITS ),
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent( "PlayerAnimEvent" );

void TE_PlayerAnimEvent( CBasePlayer *pPlayer, PlayerAnimEvent_t event, int nData )
{
    Vector vecEyePos = pPlayer->EyePosition();
	CPVSFilter filter( vecEyePos );
	if ( !IsCustomPlayerAnimEvent( event ) && ( event != PLAYERANIMEVENT_SNAP_YAW ) && ( event != PLAYERANIMEVENT_VOICE_COMMAND_GESTURE ) )
	{
		// if prediction is off, alway send jump
		if ( !( ( event == PLAYERANIMEVENT_JUMP ) && ( FStrEq(engine->GetClientConVarValue( pPlayer->entindex(), "cl_predict" ), "0" ) ) ) )
		{
			filter.RemoveRecipient( pPlayer );
		}
	}

	Assert( pPlayer->entindex() >= 1 && pPlayer->entindex() <= MAX_PLAYERS );
	g_TEPlayerAnimEvent.m_iPlayerIndex = pPlayer->entindex();
	g_TEPlayerAnimEvent.m_iEvent = event;
	Assert( nData < (1<<ANIMATION_SEQUENCE_BITS) );
	Assert( (1<<ANIMATION_SEQUENCE_BITS) >= ActivityList_HighestIndex() );
	g_TEPlayerAnimEvent.m_nData = nData;
	g_TEPlayerAnimEvent.Create( filter, 0 );
}

//=================================================================================
//
// Ragdoll Entity
//
class CTFRagdoll : public CBaseAnimatingOverlay
{
public:

	DECLARE_CLASS( CTFRagdoll, CBaseAnimatingOverlay );
	DECLARE_SERVERCLASS();

	CTFRagdoll()
	{
		m_iPlayerIndex.Set( TF_PLAYER_INDEX_NONE );
		m_bGib = false;
		m_bBurning = false;
		m_bElectrocuted = false;
		m_bFeignDeath = false;
		m_bWasDisguised = false;
		m_bBecomeAsh = false;
		m_bOnGround = false;
		m_bCloaked = false;
		m_iDamageCustom = 0;
		m_bCritOnHardHit = false;
		m_vecRagdollOrigin.Init();
		m_vecRagdollVelocity.Init();
	}

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		UseClientSideAnimation();
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	CNetworkVar( int, m_iPlayerIndex );
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
	CNetworkVar( bool, m_bGib );
	CNetworkVar( bool, m_bBurning );
	CNetworkVar( bool, m_bElectrocuted );
	CNetworkVar( bool, m_bFeignDeath );
	CNetworkVar( bool, m_bWasDisguised );
	CNetworkVar( bool, m_bBecomeAsh );
	CNetworkVar( bool, m_bOnGround );
	CNetworkVar( bool, m_bCloaked );
	CNetworkVar( int, m_iDamageCustom );
	CNetworkVar( int, m_iTeam );
	CNetworkVar( int, m_iClass );
	CNetworkVar( bool, m_bGoldRagdoll );
	CNetworkVar( bool, m_bIceRagdoll );
	CNetworkVar( bool, m_bCritOnHardHit );
	CNetworkVar( float, m_flHeadScale );
	CNetworkVar( float, m_flTorsoScale );
	CNetworkVar( float, m_flHandScale );
};

LINK_ENTITY_TO_CLASS( tf_ragdoll, CTFRagdoll );

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTFRagdoll, DT_TFRagdoll )
	SendPropVector( SENDINFO( m_vecRagdollOrigin ), -1,  SPROP_COORD ),
	SendPropInt( SENDINFO( m_iPlayerIndex ), 7, SPROP_UNSIGNED ),
	SendPropVector	( SENDINFO(m_vecForce), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_vecRagdollVelocity ), 13, SPROP_ROUNDDOWN, -2048.0f, 2048.0f ),
	SendPropInt( SENDINFO( m_nForceBone ) ),
	SendPropBool( SENDINFO( m_bGib ) ),
	SendPropBool( SENDINFO( m_bBurning ) ),
	SendPropBool( SENDINFO( m_bElectrocuted ) ),
	SendPropBool( SENDINFO( m_bFeignDeath ) ),
	SendPropBool( SENDINFO( m_bWasDisguised ) ),
	SendPropBool( SENDINFO( m_bBecomeAsh ) ),
	SendPropBool( SENDINFO( m_bOnGround ) ),
	SendPropBool( SENDINFO( m_bCloaked ) ),
	SendPropInt( SENDINFO( m_iDamageCustom ) ),
	SendPropInt( SENDINFO( m_iTeam ), 3, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iClass ), 4, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bGoldRagdoll ) ),
	SendPropBool( SENDINFO( m_bIceRagdoll ) ),
	SendPropBool( SENDINFO( m_bCritOnHardHit ) ),
	SendPropFloat( SENDINFO( m_flHeadScale ) ),
	SendPropFloat( SENDINFO( m_flTorsoScale ) ),
	SendPropFloat( SENDINFO( m_flHandScale ) ),
END_SEND_TABLE()

// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

//-----------------------------------------------------------------------------
// Purpose: Filters updates to a variable so that only non-local players see
// the changes.  This is so we can send a low-res origin to non-local players
// while sending a hi-res one to the local player.
// Input  : *pVarData - 
//			*pOut - 
//			objectID - 
//-----------------------------------------------------------------------------

void* SendProxy_SendNonLocalDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	pRecipients->SetAllRecipients();
	pRecipients->ClearRecipient( objectID - 1 );
	return ( void * )pVarData;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendNonLocalDataTable );

//-----------------------------------------------------------------------------
// Purpose: SendProxy that converts the UtlVector list of objects to entindexes, where it's reassembled on the client
//-----------------------------------------------------------------------------
void SendProxy_PlayerObjectList( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CTFPlayer *pPlayer = (CTFPlayer*)pStruct;

	// If this fails, then SendProxyArrayLength_PlayerObjects didn't work.
	Assert( iElement < pPlayer->GetObjectCount() );

	CBaseObject *pObject = pPlayer->GetObject(iElement);

	EHANDLE hObject;
	hObject = pObject;

	SendProxy_EHandleToInt( pProp, pStruct, &hObject, pOut, iElement, objectID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int SendProxyArrayLength_PlayerObjects( const void *pStruct, int objectID )
{
	CTFPlayer *pPlayer = (CTFPlayer*)pStruct;
	int iObjects = pPlayer->GetObjectCount();
	Assert( iObjects <= MAX_OBJECTS_PER_PLAYER );
	return iObjects;
}

//-----------------------------------------------------------------------------
// Purpose: Send to attached medics
//-----------------------------------------------------------------------------
void* SendProxy_SendHealersDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	CTFPlayer *pPlayer = (CTFPlayer*)pStruct;
	if ( pPlayer )
	{
		// Add attached medics
		for ( int i = 0; i < pPlayer->m_Shared.GetNumHealers(); i++ )
		{
			CTFPlayer *pMedic = ToTFPlayer( pPlayer->m_Shared.GetHealerByIndex( i ) );
			if ( !pMedic )
				continue;

			pRecipients->SetRecipient( pMedic->GetClientIndex() );
			return (void*)pVarData;
		}
	}

	return NULL;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendHealersDataTable );

BEGIN_DATADESC( CTFPlayer )
	DEFINE_THINKFUNC( DeafenThink )
END_DATADESC()
extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

// specific to the local player
BEGIN_SEND_TABLE_NOBASE( CTFPlayer, DT_TFLocalPlayerExclusive )
	// send a hi-res origin to the local player for use in prediction
	SendPropVector	(SENDINFO(m_vecOrigin), -1,  SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_Origin ),
	SendPropArray2( 
		SendProxyArrayLength_PlayerObjects,
		SendPropInt("player_object_array_element", 0, SIZEOF_IGNORE, NUM_NETWORKED_EHANDLE_BITS, SPROP_UNSIGNED, SendProxy_PlayerObjectList), 
		MAX_OBJECTS_PER_PLAYER, 
		0, 
		"player_object_array"
		),

	SendPropFloat( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f ),
//	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN ),

	// Day of Defeat
	SendPropFloat( SENDINFO(m_flStunDuration), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flStunMaxAlpha), 0, SPROP_NOSCALE ),

	// Counter-Strike
	SendPropInt( SENDINFO( m_iShotsFired ), 8, SPROP_UNSIGNED ),

END_SEND_TABLE()

// all players except the local player
BEGIN_SEND_TABLE_NOBASE( CTFPlayer, DT_TFNonLocalPlayerExclusive )
	// send a lo-res origin to other players
	SendPropVectorXY(SENDINFO(m_vecOrigin),               -1, SPROP_COORD_MP_LOWPRECISION|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY ),
	SendPropFloat   (SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_COORD_MP_LOWPRECISION|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ ),

	SendPropFloat( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 8, SPROP_CHANGES_OFTEN, -90.0f, 90.0f ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 10, SPROP_CHANGES_OFTEN ),

END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Sent to attached medics
//-----------------------------------------------------------------------------
BEGIN_SEND_TABLE_NOBASE( CTFPlayer, DT_TFSendHealersDataTable )
	SendPropInt( SENDINFO( m_nActiveWpnClip ), -1, SPROP_VARINT | SPROP_UNSIGNED ),
END_SEND_TABLE()

//============

LINK_ENTITY_TO_CLASS( player, CTFPlayer );
PRECACHE_REGISTER(player);

IMPLEMENT_SERVERCLASS_ST( CTFPlayer, DT_TFPlayer )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	SendPropExclude( "DT_BaseEntity", "m_nModelIndex" ),
	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),

	// cs_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),

	SendPropExclude( "DT_BaseFlex", "m_flexWeight" ),
	SendPropExclude( "DT_BaseFlex", "m_blinktoggle" ),
	SendPropExclude( "DT_BaseFlex", "m_viewtarget" ),

	SendPropBool(SENDINFO(m_bSaveMeParity)),

	// This will create a race condition will the local player, but the data will be the same so.....
	SendPropInt( SENDINFO( m_nWaterLevel ), 2, SPROP_UNSIGNED ),

	SendPropEHandle(SENDINFO(m_hItem)),

	// Ragdoll.
	SendPropEHandle( SENDINFO( m_hRagdoll ) ),

	SendPropDataTable( SENDINFO_DT( m_PlayerClass ), &REFERENCE_SEND_TABLE( DT_TFPlayerClassShared ) ),
	SendPropDataTable( SENDINFO_DT( m_Shared ), &REFERENCE_SEND_TABLE( DT_TFPlayerShared ) ),

	// Data that only gets sent to the local player
	SendPropDataTable( "tflocaldata", 0, &REFERENCE_SEND_TABLE(DT_TFLocalPlayerExclusive), SendProxy_SendLocalDataTable ),

	// Data that gets sent to all other players
	SendPropDataTable( "tfnonlocaldata", 0, &REFERENCE_SEND_TABLE(DT_TFNonLocalPlayerExclusive), SendProxy_SendNonLocalDataTable ),

	SendPropInt( SENDINFO( m_nForceTauntCam ), 2, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flTauntYaw ), 0, SPROP_NOSCALE ),

	SendPropFloat( SENDINFO( m_flLastDamageTime ), 16, SPROP_ROUNDUP ),

	SendPropBool( SENDINFO( m_iSpawnCounter ) ),

	SendPropFloat( SENDINFO( m_flHeadScale ) ),
	SendPropFloat( SENDINFO( m_flTorsoScale ) ),
	SendPropFloat( SENDINFO( m_flHandScale ) ),

	SendPropDataTable( "TFSendHealersDataTable", 0, &REFERENCE_SEND_TABLE( DT_TFSendHealersDataTable ), SendProxy_SendHealersDataTable ),

END_SEND_TABLE()

// -------------------------------------------------------------------------------- //

void cc_CreatePredictionError_f()
{
	CBaseEntity *pEnt = CBaseEntity::Instance( 1 );
	pEnt->SetAbsOrigin( pEnt->GetAbsOrigin() + Vector( 63, 0, 0 ) );
}

ConCommand cc_CreatePredictionError( "CreatePredictionError", cc_CreatePredictionError_f, "Create a prediction error", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFPlayer::CTFPlayer()
{
	GameAnimStateInit();

	item_list = 0;

	SetArmorValue( 10 );

	m_hItem = NULL;
	m_hTauntScene = NULL;

	UseClientSideAnimation();
	m_angEyeAngles.Init();
	m_pStateInfo = NULL;
	m_lifeState = LIFE_DEAD; // Start "dead".
	m_iMaxSentryKills = 0;
	m_flNextNameChangeTime = 0;

	m_flNextTimeCheck = gpGlobals->curtime;
	m_flSpawnTime = 0;

	m_flWaterExitTime = 0;

	SetViewOffset( TF_PLAYER_VIEW_OFFSET );

	m_Shared.Init( this );

	m_iLastSkin = -1;

	m_bHudClassAutoKill = false;
	m_bMedigunAutoHeal = false;

	m_vecLastDeathPosition = Vector( FLT_MAX, FLT_MAX, FLT_MAX );

	SetDesiredPlayerClassIndex( TF_CLASS_UNDEFINED );

	SetContextThink( &CTFPlayer::TFPlayerThink, gpGlobals->curtime, "TFPlayerThink" );

	ResetScores();

	m_flLastAction = gpGlobals->curtime;

	m_bInitTaunt = false;

	m_bSpeakingConceptAsDisguisedSpy = false;

	SetGameMovementType( nullptr );

	m_iOldStunFlags = 0;
	m_iNumberofDominations = 0;
	m_bFlipViewModels = false;
	m_iBlastJumpState = 0;
	m_flBlastJumpLandTime = 0;
	m_iHealthBefore = 0;

	m_flLastDamageResistSoundTime = -1.f;

	m_bIsTargetDummy = false;

	m_flCommentOnCarrying = 0;

	m_flLastThinkTime = -1.f;

	m_nForceTauntCam = 0;

	m_damageRateArray = new int[ DPS_Period ];
	ResetDamagePerSecond();

	m_nActiveWpnClip.Set( 0 );
	m_nActiveWpnClipPrev = 0;
	m_flNextClipSendTime = 0;

	m_flHeadScale = 1.f;
	m_flTorsoScale = 1.f;
	m_flHandScale = 1.f;

	// DOD
	m_hLastDroppedWeapon = NULL;

	// CSS
	m_iShotsFired = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ForcePlayerViewAngles( const QAngle& qTeleportAngles )
{
	CSingleUserRecipientFilter filter( this );

	UserMessageBegin( filter, "ForcePlayerViewAngles" );
	WRITE_BYTE( 0x01 ); // Reserved space for flags.
	WRITE_BYTE( entindex() );
	WRITE_ANGLES( qTeleportAngles );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::TFPlayerThink()
{
	if ( m_pStateInfo && m_pStateInfo->pfnThink )
	{
		(this->*m_pStateInfo->pfnThink)();
	}

	// Time to finish the current random expression? Or time to pick a new one?
	if ( IsAlive() && ( m_flNextSpeakWeaponFire < gpGlobals->curtime ) && m_flNextRandomExpressionTime >= 0 && gpGlobals->curtime > m_flNextRandomExpressionTime )
	{
		// Random expressions need to be cleared, because they don't loop. So if we
		// pick the same one again, we want to restart it.
		ClearExpression();
		m_iszExpressionScene = NULL_STRING;
		UpdateExpression();
	}

	CBaseEntity *pGroundEntity = GetGroundEntity();

	// We consider players "in air" if they have no ground entity and they're not in water.
	if ( pGroundEntity == NULL && GetWaterLevel() == WL_NotInWater )
	{
		if ( m_iLeftGroundHealth < 0 )
		{
			m_iLeftGroundHealth = GetHealth();
		}
	}
	else
	{
		m_iLeftGroundHealth = -1;
		if ( GetFlags() & FL_ONGROUND )
		{
			m_Shared.RemoveCond( TF_COND_KNOCKED_INTO_AIR );
		}

		if ( m_iBlastJumpState )
		{
			const char *pszEvent = NULL;

			if ( StickyJumped() )
			{
				pszEvent = "sticky_jump_landed";
			}
			else if ( RocketJumped() )
			{
				pszEvent = "rocket_jump_landed";
			}

			ClearBlastJumpState();

			if ( pszEvent )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( pszEvent );
				if ( event )
				{
					event->SetInt( "userid", GetUserID() );
					gameeventmanager->FireEvent( event );
				}
			}
		}
	}

	if( IsTaunting() )
	{
		bool bStopTaunt = false;
		// if I'm not supposed to move during taunt
		// stop taunting if I lost my ground entity or was moved at all
//		if ( !CanMoveDuringTaunt() )
		{
			bStopTaunt |= pGroundEntity == NULL;
		}

		if ( !bStopTaunt  )
		{
			bStopTaunt |= ShouldStopTaunting();
		}

		if ( bStopTaunt )
		{
			CancelTaunt();
		}
	}

	if ( ( RocketJumped() || StickyJumped() ) && IsAlive() && m_bCreatedRocketJumpParticles == false )
	{
		const char *pEffectName = "rocketjump_smoke";
		DispatchParticleEffect( pEffectName, PATTACH_POINT_FOLLOW, this, "foot_L" );
		DispatchParticleEffect( pEffectName, PATTACH_POINT_FOLLOW, this, "foot_R" );
		m_bCreatedRocketJumpParticles = true;
	}

	if ( gpGlobals->curtime > m_flCommentOnCarrying && (m_flCommentOnCarrying != 0.f) )
	{
		m_flCommentOnCarrying = 0.f;

		CBaseObject* pObj = m_Shared.GetCarriedObject();
		if ( pObj )
		{
			SpeakConceptIfAllowed( MP_CONCEPT_CARRYING_BUILDING, pObj->GetResponseRulesModifier() );
		}
	}

	// Send active weapon's clip state to attached medics
	bool bSendClipInfo = gpGlobals->curtime > m_flNextClipSendTime &&
						 m_Shared.GetNumHealers() &&
						 IsAlive();
	if ( bSendClipInfo )
	{
		CBaseCombatWeapon *pWeapon = GetActiveWeapon();
		if ( pWeapon )
		{
			int nClip = 0;

			if ( m_Shared.InCond( TF_COND_DISGUISED ) )
			{
				nClip = m_Shared.GetDisguiseAmmoCount();
			}
			else
			{
				nClip = pWeapon->UsesClipsForAmmo1() ? pWeapon->Clip1() : GetAmmoCount( pWeapon->GetPrimaryAmmoType() );
			}

			if ( nClip >= 0 && nClip != m_nActiveWpnClipPrev )
			{
				if ( nClip > 500 )
				{
					Warning( "Heal Target: ClipSize Data Limit Exceeded: %d (max 500)\n", nClip );
					nClip = MIN( nClip, 500 );
				}
				m_nActiveWpnClip.Set( nClip );
				m_nActiveWpnClipPrev = m_nActiveWpnClip;
				m_flNextClipSendTime = gpGlobals->curtime + 0.25f;
			}
		}
	}

	// Scale our head
	m_flHeadScale = Approach( GetDesiredHeadScale(), m_flHeadScale, GetHeadScaleSpeed() );

	// scale our torso
	m_flTorsoScale = Approach( GetDesiredTorsoScale(), m_flTorsoScale, GetTorsoScaleSpeed() );

	// scale our torso
	m_flHandScale = Approach( GetDesiredHandScale(), m_flHandScale, GetHandScaleSpeed() );

	SetContextThink( &CTFPlayer::TFPlayerThink, gpGlobals->curtime, "TFPlayerThink" );
	m_flLastThinkTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a portion of health every think.
//-----------------------------------------------------------------------------
void CTFPlayer::RegenThink( void )
{
	if ( !IsAlive() )
		return;

	// Queue the next think
	SetContextThink( &CTFPlayer::RegenThink, gpGlobals->curtime + TF_REGEN_TIME, "RegenThink" );

	// if we're going in to this too often, quit out.
	if ( m_flLastHealthRegenAt + TF_REGEN_TIME > gpGlobals->curtime )
		return;

	bool bShowRegen = true;

	// Medic has a base regen amount
	if ( GetPlayerClass()->GetClassIndex() == TF_CLASS_MEDIC )
	{
		// Heal faster if we haven't been in combat for a while.
		float flTimeSinceDamage = gpGlobals->curtime - GetLastDamageReceivedTime();
		float flScale = RemapValClamped( flTimeSinceDamage, 5.0f, 10.0f, 1.0f, 2.0f );
		float flRegenAmt = TF_REGEN_AMOUNT;

		// If you are healing a hurt patient, increase your base regen
		CTFPlayer *pPatient = ToTFPlayer( MedicGetHealTarget() );
		if ( pPatient && pPatient->GetHealth() < pPatient->GetMaxHealth() )
		{
			// Double regen amount
			flRegenAmt += TF_REGEN_AMOUNT;
		}

		flRegenAmt *= flScale;

		m_flAccumulatedHealthRegen += flRegenAmt;

		bShowRegen = false;
	}

	int nHealAmount = 0;
	if ( m_flAccumulatedHealthRegen >= 1.f )
	{
		nHealAmount = floor( m_flAccumulatedHealthRegen );
		if ( GetHealth() < GetMaxHealth() )
		{
			int nHealedAmount = TakeHealth( nHealAmount, DMG_GENERIC | DMG_IGNORE_DEBUFFS );
			if ( nHealedAmount > 0 )
			{
				IGameEvent *event = gameeventmanager->CreateEvent( "player_healed" );
				if ( event )
				{
					event->SetInt( "priority", 1 );	// HLTV event priority
					event->SetInt( "patient", GetUserID() );
					event->SetInt( "healer", GetUserID() );
					event->SetInt( "amount", nHealedAmount );
					gameeventmanager->FireEvent( event );
				}
			}
		}
	}
	else if ( m_flAccumulatedHealthRegen < -1.f )
	{
		nHealAmount = ceil( m_flAccumulatedHealthRegen );
		TakeDamage( CTakeDamageInfo( this, this, NULL, vec3_origin, WorldSpaceCenter(), nHealAmount * -1, DMG_GENERIC ) );
	}

	if ( GetHealth() < GetMaxHealth() && nHealAmount != 0 && bShowRegen )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "player_healonhit" );
		if ( event )
		{
			event->SetInt( "amount", nHealAmount );
			event->SetInt( "entindex", entindex() );
			gameeventmanager->FireEvent( event ); 
		}
	}

	m_flAccumulatedHealthRegen -= nHealAmount;
	m_flLastHealthRegenAt = gpGlobals->curtime;
}

CTFPlayer::~CTFPlayer()
{
	delete [] m_damageRateArray;

	DestroyRagdoll();
	if ( GetGameAnimStateType() )
		GetGameAnimStateType()->Release();
}


CTFPlayer *CTFPlayer::CreatePlayer( const char *className, edict_t *ed )
{
	CTFPlayer::s_PlayerEdict = ed;
	return (CTFPlayer*)CreateEntityByName( className );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::UpdateTimers( void )
{
	m_Shared.ConditionThink();
	m_Shared.InvisibilityThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PreThink()
{
	// Update timers.
	UpdateTimers();

	// Pass through to the base class think.
	BaseClass::PreThink();

	// Reset bullet force accumulator, only lasts one frame, for ragdoll forces from multiple shots.
	m_vecTotalBulletForce = vec3_origin;

	CheckForIdle();
}

ConVar mp_idledealmethod( "mp_idledealmethod", "1", FCVAR_GAMEDLL, "Deals with Idle Players. 1 = Sends them into Spectator mode then kicks them if they're still idle, 2 = Kicks them out of the game;" );
ConVar mp_idlemaxtime( "mp_idlemaxtime", "3", FCVAR_GAMEDLL, "Maximum time a player is allowed to be idle (in minutes)" );

void CTFPlayer::CheckForIdle( void )
{
	if ( m_afButtonLast != m_nButtons )
		m_flLastAction = gpGlobals->curtime;

	if ( mp_idledealmethod.GetInt() )
	{
		if ( IsHLTV() )
			return;

		if ( IsFakeClient() )
			return;

		//Don't mess with the host on a listen server (probably one of us debugging something)
		if ( engine->IsDedicatedServer() == false && entindex() == 1 )
			return;

		if ( m_bIsIdle == false )
		{
			if ( StateGet() == TF_STATE_OBSERVER || StateGet() != TF_STATE_ACTIVE )
				return;
		}
		
		float flIdleTime = mp_idlemaxtime.GetFloat() * 60;

		if ( TFGameRules()->InStalemate() )
		{
			flIdleTime = mp_stalemate_timelimit.GetInt() * 0.5f;
		}
		
		if ( (gpGlobals->curtime - m_flLastAction) > flIdleTime  )
		{
			bool bKickPlayer = false;

			ConVarRef mp_allowspectators( "mp_allowspectators" );
			if ( mp_allowspectators.IsValid() && ( mp_allowspectators.GetBool() == false ) )
			{
				// just kick the player if this server doesn't allow spectators
				bKickPlayer = true;
			}
			else if ( mp_idledealmethod.GetInt() == 1 )
			{
				//First send them into spectator mode then kick him.
				if ( m_bIsIdle == false )
				{
					ForceChangeTeam( TEAM_SPECTATOR );
					m_flLastAction = gpGlobals->curtime;
					m_bIsIdle = true;
					return;
				}
				else
				{
					bKickPlayer = true;
				}
			}
			else if ( mp_idledealmethod.GetInt() == 2 )
			{
				bKickPlayer = true;
			}

			if ( bKickPlayer == true )
			{
				UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#game_idle_kick", GetPlayerName() );
				engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", GetUserID() ) );
				m_flLastAction = gpGlobals->curtime;
			}
		}
	}
}

extern ConVar flashlight;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CTFPlayer::FlashlightIsOn( void )
{
	return IsEffectActive( EF_DIMLIGHT );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTFPlayer::FlashlightTurnOn( void )
{
	if( flashlight.GetInt() > 0 && IsAlive() )
	{
		AddEffects( EF_DIMLIGHT );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTFPlayer::FlashlightTurnOff( void )
{
	if( IsEffectActive(EF_DIMLIGHT) )
	{
		RemoveEffects( EF_DIMLIGHT );
	}	
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PostThink()
{
	BaseClass::PostThink();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles( angles );
	
	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

	if ( GetGameAnimStateType() )
		GetGameAnimStateType()->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Precache()
{
	// Precache the player models and gibs.
	PrecachePlayerModels();
	PrecacheTFExtras();
					 
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Precache the player models and player model gibs.
//-----------------------------------------------------------------------------
void CTFPlayer::PrecachePlayerModels( void )
{
	for ( int i = 0; i < GAME_CLASS_COUNT; i++ )
	{
		// Don't precache random
		if ( i == TF_CLASS_RANDOM )
			continue;

		TFPlayerClassData_t *pData = GetPlayerClassData( i );
		const char *pszModel = pData->m_szModelName;
		if ( pszModel && pszModel[0] )
		{
			int iModel = PrecacheModel( pszModel );
			PrecacheGibsForModel( iModel );
		}

		// Precache the hardware facial morphed models as well.
		const char *pszHWMModel = pData->m_szHWMModelName;
		if ( pszHWMModel && pszHWMModel[0] )
		{
			PrecacheModel( pszHWMModel );
		}

		if ( i > 0 && i < TF_CLASS_COUNT_ALL )
		{
			// Precache player class sounds
			PrecacheScriptSound( pData->m_szDeathSound );
			PrecacheScriptSound( pData->m_szCritDeathSound );
			PrecacheScriptSound( pData->m_szMeleeDeathSound );
			PrecacheScriptSound( pData->m_szExplosionDeathSound );
		}
	}
}

void CTFPlayer::PrecacheTFExtras( void )
{
	// Precache the player sounds.
	PrecacheScriptSound( "Player.Spawn" );
	PrecacheScriptSound( "TFPlayer.Pain" );
	PrecacheScriptSound( "TFPlayer.CritHit" );
	PrecacheScriptSound( "TFPlayer.CritPain" );
	PrecacheScriptSound( "TFPlayer.CritDeath" );
	PrecacheScriptSound( "TFPlayer.FreezeCam" );
	PrecacheScriptSound( "TFPlayer.Drown" );
	PrecacheScriptSound( "TFPlayer.AttackerPain" );
	PrecacheScriptSound( "TFPlayer.SaveMe" );
	PrecacheScriptSound( "Camera.SnapShot" );

	PrecacheScriptSound( "Game.YourTeamLost" );
	PrecacheScriptSound( "Game.YourTeamWon" );
	PrecacheScriptSound( "Game.SuddenDeath" );
	PrecacheScriptSound( "Game.Stalemate" );
	PrecacheScriptSound( "TV.Tune" );

	// Precache particle systems
	PrecacheParticleSystem( "crit_text" );
	PrecacheParticleSystem( "cig_smoke" );
	PrecacheParticleSystem( "speech_mediccall" );
	PrecacheParticleSystem( "player_recent_teleport_blue" );
	PrecacheParticleSystem( "player_recent_teleport_red" );
	PrecacheParticleSystem( "particle_nemesis_red" );
	PrecacheParticleSystem( "particle_nemesis_blue" );
	PrecacheParticleSystem( "spy_start_disguise_red" );
	PrecacheParticleSystem( "spy_start_disguise_blue" );
	PrecacheParticleSystem( "burningplayer_red" );
	PrecacheParticleSystem( "burningplayer_blue" );
	PrecacheParticleSystem( "blood_spray_red_01" );
	PrecacheParticleSystem( "blood_spray_red_01_far" );
	PrecacheParticleSystem( "water_blood_impact_red_01" );
	PrecacheParticleSystem( "blood_impact_red_01" );
	PrecacheParticleSystem( "water_playerdive" );
	PrecacheParticleSystem( "water_playeremerge" );

	if ( TFGameRules() && TFGameRules()->IsBirthday() )
	{
		for ( int i = 1; i < ARRAYSIZE(g_pszBDayGibs); i++ )
		{
			PrecacheModel( g_pszBDayGibs[i] );
		}
		PrecacheModel( "models/effects/bday_hat.mdl" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allow pre-frame adjustments on the player
//-----------------------------------------------------------------------------
ConVar sv_runcmds( "sv_runcmds", "1" );
void CTFPlayer::PlayerRunCommand( CUserCmd *ucmd, IMoveHelper *moveHelper )
{
	static bool bSeenSyncError = false;
	VPROF( "CTFPlayer::PlayerRunCommand" );

	if ( !sv_runcmds.GetInt() )
		return;

	if ( IsTaunting() || m_Shared.InCond( TF_COND_HALLOWEEN_THRILLER ) )
	{
		// For some taunts, it is critical that the player not move once they start
//		if ( !CanMoveDuringTaunt() )
		{
			ucmd->forwardmove = 0;
			ucmd->upmove = 0;
			ucmd->sidemove = 0;
			ucmd->viewangles = pl.v_angle;
		}

		if ( tf_allow_taunt_switch.GetInt() == 0 && ucmd->weaponselect != 0 )
		{
			ucmd->weaponselect = 0;

			// FIXME: The client will have predicted the weapon switch and have
			// called Holster/Deploy which will make the wielded weapon
			// invisible on their end.
		}
	}

	BaseClass::PlayerRunCommand( ucmd, moveHelper );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::IsReadyToPlay( void )
{
	return ( ( GetTeamNumber() > LAST_SHARED_TEAM ) &&
			 ( GetDesiredPlayerClassIndex() > TF_CLASS_UNDEFINED ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::IsReadyToSpawn( void )
{
	if ( IsClassMenuOpen() )
	{
		return false;
	}

	return ( StateGet() != TF_STATE_DYING );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this player should be allowed to instantly spawn
//			when they next finish picking a class.
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldGainInstantSpawn( void )
{
	return ( GetPlayerClass()->GetClassIndex() == TF_CLASS_UNDEFINED || IsClassMenuOpen() );
}

//-----------------------------------------------------------------------------
// Purpose: Resets player scores
//-----------------------------------------------------------------------------
void CTFPlayer::ResetScores( void )
{
	CTF_GameStats.ResetPlayerStats( this );
	RemoveNemesisRelationships();
	BaseClass::ResetScores();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::InitialSpawn( void )
{
	BaseClass::InitialSpawn();

	SetWeaponBuilder( NULL );

	m_iMaxSentryKills = 0;
	CTF_GameStats.Event_MaxSentryKills( this, 0 );

	StateEnter( TF_STATE_WELCOME );

	ResetAccumulatedSentryGunDamageDealt();
	ResetAccumulatedSentryGunKillCount();
	ResetDamagePerSecond();

	IGameEvent * event = gameeventmanager->CreateEvent( "player_initial_spawn" );
	if ( event )
	{
		event->SetInt( "index", entindex() );
		gameeventmanager->FireEvent( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override Base ApplyAbsVelocityImpulse (BaseEntity) to apply potential item attributes
//-----------------------------------------------------------------------------
void CTFPlayer::ApplyAbsVelocityImpulse( const Vector &vecImpulse ) 
{
	// Check for Attributes (mult_aiming_knockback_resistance)
	Vector vecForce = vecImpulse;
	float flImpulseScale = 1.0f;
	if ( IsPlayerClass( TF_CLASS_SNIPER ) && m_Shared.InCond( TF_COND_AIMING ) )
	{		
		//CALL_ATTRIB_HOOK_FLOAT( flImpulseScale, mult_aiming_knockback_resistance );
	}

	if ( m_Shared.InCond( TF_COND_HALLOWEEN_TINY ) && !m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
	{
		flImpulseScale *= 2.f;
	}

	// take extra force if you have a parachute deployed in x-y directions
	if ( m_Shared.InCond( TF_COND_PARACHUTE_DEPLOYED ) )
	{
		float flHorizontalScale = 1.5f;
		vecForce.x *= flHorizontalScale;
		vecForce.y *= flHorizontalScale;
	}

	CBaseMultiplayerPlayer::ApplyAbsVelocityImpulse( vecForce * flImpulseScale );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ApplyAirBlastImpulse( const Vector &vecImpulse )
{
	// Knockout powerup carriers are immune to airblast
	if ( m_Shared.InCond( TF_COND_MEGAHEAL ) )
		return;
	
	Vector vForce = vecImpulse;

	float flScale = 1.0f;
	//CALL_ATTRIB_HOOK_FLOAT( flScale, airblast_vulnerability_multiplier );
	vForce *= flScale;

	// if on the ground, require min force to boost you off it
	if ( ( GetFlags() & FL_ONGROUND ) && ( vForce.z < JUMP_MIN_SPEED ) )
	{
		// Minimum value of vecForce.z
		vForce.z = JUMP_MIN_SPEED;
	}
	
	//CALL_ATTRIB_HOOK_FLOAT( vForce.z, airblast_vertical_vulnerability_multiplier );

	RemoveFlag( FL_ONGROUND );
	m_Shared.AddCond( TF_COND_KNOCKED_INTO_AIR );

	ApplyAbsVelocityImpulse( vForce );
}

//-----------------------------------------------------------------------------
// Purpose: Go between for Setting Local Punch Impulses. Checks item attributes
// Use this instead of directly calling m_Local.m_vecPunchAngle.SetX( value );
//-----------------------------------------------------------------------------
bool CTFPlayer::ApplyPunchImpulseX ( float flImpulse ) 
{
	// Check for No Aim Flinch
	bool bFlinch = true;
	
	if ( bFlinch )
	{
		m_Local.m_vecPunchAngle.SetX( flImpulse );
	}

	return bFlinch;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Spawn()
{
	MDLCACHE_CRITICAL_SECTION();

	GameMovementInit();
	GameAnimStateInit();

	m_flSpawnTime = gpGlobals->curtime;
	UpdateModel();

	SetMoveType( MOVETYPE_WALK );

	// Create main viewmodel
	CreateViewModel( 0 );
	// Create our off hand viewmodel if necessary
	CreateViewModel( 1 );
	// Make sure it has no model set, in case it had one before
	if ( GetViewModel( 1 ) )
		GetViewModel( 1 )->SetModel( "" );

	BaseClass::Spawn();

	// Day of Defeat
	{
		m_Shared.SetStamina( 100 );
		InitProne();
		InitSprinting();
	}

	// CSS
	{
		m_iShotsFired = 0;
	}

	// Kind of lame, but CBasePlayer::Spawn resets a lot of the state that we initially want on.
	// So if we're in the welcome state, call its enter function to reset 
	if ( m_Shared.InState( TF_STATE_WELCOME ) )
	{
		StateEnterWELCOME();
	}

	// If they were dead, then they're respawning. Put them in the active state.
	if ( m_Shared.InState( TF_STATE_DYING ) )
	{
		StateTransition( TF_STATE_ACTIVE );
	}

	// If they're spawning into the world as fresh meat, give them items and stuff.
	if ( m_Shared.InState( TF_STATE_ACTIVE ) )
	{
		// remove our disguise each time we spawn
		if ( m_Shared.InCond( TF_COND_DISGUISED ) )
		{
			m_Shared.RemoveDisguise();
		}

		EmitSound( "Player.Spawn" );
		InitClass();
		m_Shared.RemoveAllCond(); // Remove conc'd, burning, rotting, hallucinating, etc.

		// add team glows for a period of time after we respawn
		m_Shared.AddCond( TF_COND_TEAM_GLOWS, tf_spawn_glows_duration.GetInt() );

		UpdateSkin( GetTeamNumber() );
		TeamFortress_SetSpeed();

		// Prevent firing for a second so players don't blow their faces off
		SetNextAttack( gpGlobals->curtime + 1.0 );

		DoAnimationEvent( PLAYERANIMEVENT_SPAWN );

		// Force a taunt off, if we are still taunting, the condition should have been cleared above.
		StopTaunt();

		// turn on separation so players don't get stuck in each other when spawned
		m_Shared.SetSeparation( true );
		m_Shared.SetSeparationVelocity( vec3_origin );

		RemoveTeleportEffect();
	
		//If this is true it means I respawned without dying (changing class inside the spawn room) but doesn't necessarily mean that my healers have stopped healing me
		//This means that medics can still be linked to me but my health would not be affected since this condition is not set.
		//So instead of going and forcing every healer on me to stop healing we just set this condition back on. 
		//If the game decides I shouldn't be healed by someone (LOS, Distance, etc) they will break the link themselves like usual.
		if ( m_Shared.GetNumHealers() > 0 )
		{
			m_Shared.AddCond( TF_COND_HEALTH_BUFF );
		}

		if ( !m_bSeenRoundInfo )
		{
			TFGameRules()->ShowRoundInfoPanel( this );
			m_bSeenRoundInfo = true;
		}

		if ( IsInCommentaryMode() && !IsFakeClient() )
		{
			// Player is spawning in commentary mode. Tell the commentary system.
			CBaseEntity *pEnt = NULL;
			variant_t emptyVariant;
			while ( (pEnt = gEntList.FindEntityByClassname( pEnt, "commentary_auto" )) != NULL )
			{
				pEnt->AcceptInput( "MultiplayerSpawned", this, this, emptyVariant, 0 );
			}
		}
	}

	CTF_GameStats.Event_PlayerSpawned( this );

	m_iSpawnCounter = !m_iSpawnCounter;
	m_bAllowInstantSpawn = false;

	m_Shared.SetSpyCloakMeter( 100.0f );

	m_Shared.ClearDamageEvents();
	m_AchievementData.ClearHistories();

	m_flLastDamageTime = 0;
	m_flLastDamageDoneTime = 0.f;

	m_flNextVoiceCommandTime = gpGlobals->curtime;

	ClearZoomOwner();
	SetFOV( this , 0 );

	// Override base viewoffset
	if ( IsDODClass() )
		SetViewOffset( VEC_VIEW_SCALED_DOD( this ) );
	else
		SetViewOffset( GetClassEyeHeight() );

	RemoveAllScenesInvolvingActor( this );
	ClearExpression();
	m_flNextSpeakWeaponFire = gpGlobals->curtime;

	m_bIsIdle = false;
	m_flPowerPlayTime = 0.0;

	// This makes the surrounding box always the same size as the standing collision box
	// helps with parts of the hitboxes that extend out of the crouching hitbox, eg with the
	// heavyweapons guy
	Vector mins = VEC_HULL_MIN;
	Vector maxs = VEC_HULL_MAX;
	CollisionProp()->SetSurroundingBoundsType( USE_SPECIFIED_BOUNDS, &mins, &maxs );

	m_iLeftGroundHealth = -1;
	m_iBlastJumpState = 0;
	m_bTakenBlastDamageSinceLastMovement = false;

 	m_iOldStunFlags = 0;

	IGameEvent * event = gameeventmanager->CreateEvent( "player_spawn" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetInt( "team", GetTeamNumber() );
		event->SetInt( "class", GetPlayerClass()->GetClassIndex() );

		gameeventmanager->FireEvent( event );
	}

	m_Shared.Spawn();

	m_nForceTauntCam = 0;
	m_bAllowedToRemoveTaunt = true;
}

//-----------------------------------------------------------------------------
// Purpose: Removes all nemesis relationships between this player and others
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveNemesisRelationships()
{
	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CTFPlayer *pTemp = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pTemp && pTemp != this )
		{
			bool bRemove = false;

			if ( TFGameRules()->IsInArenaMode() == true )
			{
				if ( GetTeamNumber() != TEAM_SPECTATOR )
				{
					if ( InSameTeam( pTemp ) == true )
					{
						bRemove = true;
					}
				}

				if ( IsDisconnecting() == true )
				{
					bRemove = true;
				}
			}
			else
			{
				bRemove = true;
			}
			
			if ( bRemove == true )
			{
				// set this player to be not dominating anyone else
				m_Shared.SetPlayerDominated( pTemp, false );
				m_iNumberofDominations = 0;

				// set no one else to be dominating this player	
				bool bThisPlayerIsDominatingMe = m_Shared.IsPlayerDominatingMe( i );
				pTemp->m_Shared.SetPlayerDominated( this, false );
				if ( bThisPlayerIsDominatingMe )
				{
					int iDoms = pTemp->GetNumberofDominations();
					pTemp->SetNumberofDominations( iDoms - 1);
				}
			}
		}
	}	

	if ( TFGameRules()->IsInArenaMode() == false || IsDisconnecting() == true )
	{
		// reset the matrix of who has killed whom with respect to this player
		CTF_GameStats.ResetKillHistory( this );
	}

	IGameEvent *event = gameeventmanager->CreateEvent( "remove_nemesis_relationships" );
	if ( event )
	{
		event->SetInt( "player", entindex() );
		gameeventmanager->FireEvent( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::Regenerate( void )
{
	// We may have been boosted over our max health. If we have, 
	// restore it after we reset out class values.
	int iCurrentHealth = GetHealth();
	m_bRegenerating = true;
	InitClass();
	m_bRegenerating = false;
	if ( iCurrentHealth > GetHealth() )
	{
		SetHealth( iCurrentHealth );
	}

	if ( m_Shared.InCond( TF_COND_BURNING ) )
	{
		m_Shared.RemoveCond( TF_COND_BURNING );
	}

	// DOD Reset their helmet bodygroup
	if ( GetBodygroup( BODYGROUP_HELMET ) == GetPlayerClass()->GetData()->m_iHairGroup && GetClassType() == FACTION_DOD )
		SetBodygroup( BODYGROUP_HELMET, GetPlayerClass()->GetData()->m_iHelmetGroup );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::InitClass( void )
{
	// Set initial health and armor based on class.
	SetMaxHealth( GetPlayerClass()->GetMaxHealth() );
	SetHealth( GetMaxHealth() );

	SetArmorValue( GetPlayerClass()->GetMaxArmor() );

	// Init the anim movement vars
	if ( GetGameAnimStateType() )
	{
		GetGameAnimStateType()->SetRunSpeed(GetPlayerClass()->GetMaxSpeed());
		GetGameAnimStateType()->SetWalkSpeed(GetPlayerClass()->GetMaxSpeed() * 0.5);
	}

	// Give default items for class.
	GiveDefaultItems();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DestroyViewModel( int iViewModel )
{
	CBaseViewModel *vm = GetViewModel( iViewModel );
	if ( !vm )
		return;

	UTIL_Remove( vm );
	m_hViewModel.Set( iViewModel, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::CreateViewModel( int iViewModel )
{
	Assert( iViewModel >= 0 && iViewModel < MAX_VIEWMODELS );

	if ( GetViewModel( iViewModel ) )
		DestroyViewModel( iViewModel );

	switch ( GetClassType() )
	{
		case FACTION_DOD:
		{
			CDODViewModel *vm = ( CDODViewModel * )CreateEntityByName( "dod_viewmodel" );
			if ( vm )
			{
				vm->SetAbsOrigin( GetAbsOrigin() );
				vm->SetOwner( this );
				vm->SetIndex( iViewModel );
				DispatchSpawn( vm );
				vm->FollowEntity( this, false );
				m_hViewModel.Set( iViewModel, vm );
			}
			break;
		}
		case FACTION_CS:
		{
			CPredictedViewModel *pViewModel = ( CPredictedViewModel * )CreateEntityByName( "predicted_viewmodel" );
			if ( pViewModel )
			{
				pViewModel->SetAbsOrigin( GetAbsOrigin() );
				pViewModel->SetOwner( this );
				pViewModel->SetIndex( iViewModel );
				DispatchSpawn( pViewModel );
				pViewModel->FollowEntity( this, false );
				m_hViewModel.Set( iViewModel, pViewModel );
			}
			break;
		}
		case FACTION_HL2:
		{
			CBaseViewModel *pViewModel = ( CBaseViewModel * )CreateEntityByName( "viewmodel" );
			if ( pViewModel )
			{
				pViewModel->SetAbsOrigin( GetAbsOrigin() );
				pViewModel->SetOwner( this );
				pViewModel->SetIndex( iViewModel );
				DispatchSpawn( pViewModel );
				pViewModel->FollowEntity( this, false );
				m_hViewModel.Set( iViewModel, pViewModel );
			}
			break;
		}
		default:
		{
			CTFViewModel *pViewModel = ( CTFViewModel * )CreateEntityByName( "tf_viewmodel" );
			if ( pViewModel )
			{
				pViewModel->SetAbsOrigin( GetAbsOrigin() );
				pViewModel->SetOwner( this );
				pViewModel->SetIndex( iViewModel );
				DispatchSpawn( pViewModel );
				pViewModel->FollowEntity( this, false );
				m_hViewModel.Set( iViewModel, pViewModel );
			}
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets the view model for the player's off hand
//-----------------------------------------------------------------------------
CBaseViewModel *CTFPlayer::GetOffHandViewModel()
{
	// off hand model is slot 1
	return GetViewModel( 1 );
}

//-----------------------------------------------------------------------------
// Purpose: Sends the specified animation activity to the off hand view model
//-----------------------------------------------------------------------------
void CTFPlayer::SendOffHandViewModelActivity( Activity activity )
{
	CBaseViewModel *pViewModel = GetOffHandViewModel();
	if ( pViewModel )
	{
		int sequence = pViewModel->SelectWeightedSequence( activity );
		pViewModel->SendViewModelMatchingSequence( sequence );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the player up with the default weapons, ammo, etc.
//-----------------------------------------------------------------------------
void CTFPlayer::GiveDefaultItems()
{
	// Get the player class data.
	TFPlayerClassData_t *pData = m_PlayerClass.GetData();

	RemoveAllAmmo();
	
	// Give weapons.
	switch ( GetClassType() )
	{
		case FACTION_DOD:
		{
			ManageDODWeapons( pData );
			ManageDODGrenades( pData );
			break;
		}
		case FACTION_CS:
		{
			ManageCSWeapons( pData );
			break;
		}
		case FACTION_PC:
		default:
		{
			// Give ammo. Must be done before weapons, so weapons know the player has ammo for them.
			for ( int iAmmo = 0; iAmmo < TF_AMMO_COUNT; ++iAmmo )
			{
				GiveAmmo( pData->m_aAmmoMax[iAmmo], iAmmo );
			}

			ManageRegularWeapons( pData );
			ManageBuilderWeapons( pData );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ManageBuilderWeapons( TFPlayerClassData_t *pData )
{
	// Collect all builders and validate them against the list of objects (below)
	CUtlVector< CTFWeaponBuilder* > vecBuilderDestroyList;
	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		CTFWeaponBuilder *pBuilder = dynamic_cast< CTFWeaponBuilder* >( GetWeapon( i ) );
		if ( !pBuilder )
			continue;

		vecBuilderDestroyList.AddToTail( pBuilder );
	}

	// Go through each object and see if we need to create or remove builders
	for ( int i = 0; i < OBJ_LAST; ++i )
	{
		if ( !GetPlayerClass() || ( GetPlayerClass() && !GetPlayerClass()->CanBuildObject( i ) ) )
			continue;

		// Do we have a specific builder for this object?
		CTFWeaponBuilder *pBuilder = CTFPlayerSharedUtils::GetBuilderForObjectType( this, i );
		if ( !GetObjectInfo( i )->m_bRequiresOwnBuilder )
		{
			// Do we have a default builder, and an object that doesn't require a specific builder?
			pBuilder = CTFPlayerSharedUtils::GetBuilderForObjectType( this, -1 );
			if ( pBuilder )
			{
				// Flag it as supported by this builder (ugly, but necessary for legacy system)
				pBuilder->SetObjectTypeAsBuildable( i );
			}
		}
				
		// Is a new builder required?
		if ( !pBuilder || ( GetObjectInfo( i )->m_bRequiresOwnBuilder && !( CTFPlayerSharedUtils::GetBuilderForObjectType( this, i ) ) ) )
		{
			pBuilder = dynamic_cast< CTFWeaponBuilder* >( GiveNamedItem( "tf_weapon_builder", i ) );
			if ( pBuilder )
			{
				pBuilder->DefaultTouch( this );
			}
		}

		// Builder settings
		if ( pBuilder )
		{
			if ( m_bRegenerating == false )
			{
				pBuilder->WeaponReset();
			}

			pBuilder->GiveDefaultAmmo();
			pBuilder->ChangeTeam( GetTeamNumber() );
			pBuilder->SetObjectTypeAsBuildable( i );
			pBuilder->m_nSkin = GetTeamNumber() - 2;	// color the w_model to the team

			// Pull it out of the "destroy" list
			vecBuilderDestroyList.FindAndRemove( pBuilder );
		}
	}

	// Anything left should be destroyed
	FOR_EACH_VEC( vecBuilderDestroyList, i )
	{
		Assert( vecBuilderDestroyList[i] );

		Weapon_Detach( vecBuilderDestroyList[i] );
		UTIL_Remove( vecBuilderDestroyList[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ManageRegularWeapons( TFPlayerClassData_t *pData )
{
	// Remove our disguise weapon.
	m_Shared.RemoveDisguiseWeapon();

	for ( int iWeapon = 0; iWeapon < TF_PLAYER_WEAPON_COUNT; ++iWeapon )
	{
		if ( pData->m_aWeapons[iWeapon] != TF_WEAPON_NONE )
		{
			int iWeaponID = pData->m_aWeapons[iWeapon];
			const char *pszWeaponName = WeaponIdToClassname( iWeaponID );

			CTFWeaponBase *pWeapon = (CTFWeaponBase *)GetWeapon( iWeapon );

			//If we already have a weapon in this slot but is not the same type then nuke it (changed classes)
			if ( pWeapon && pWeapon->GetWeaponID() != iWeaponID )
			{
				Weapon_Detach( pWeapon );
				UTIL_Remove( pWeapon );
			}

			pWeapon = (CTFWeaponBase *)Weapon_OwnsThisID( iWeaponID );

			if ( pWeapon )
			{
				pWeapon->ChangeTeam( GetTeamNumber() );
				pWeapon->GiveDefaultAmmo();

				if ( m_bRegenerating == false )
				{
					pWeapon->WeaponReset();
				}
			}
			else
			{
				pWeapon = (CTFWeaponBase *)GiveNamedItem( pszWeaponName );

				if ( pWeapon )
				{
					pWeapon->DefaultTouch( this );
				}
			}
		}
		else
		{
			//I shouldn't have any weapons in this slot, so get rid of it
			CTFWeaponBase *pCarriedWeapon = (CTFWeaponBase *)GetWeapon( iWeapon );

			//Don't nuke builders since they will be nuked if we don't need them later.
			if ( pCarriedWeapon && pCarriedWeapon->GetWeaponID() != TF_WEAPON_BUILDER )
			{
				Weapon_Detach( pCarriedWeapon );
				UTIL_Remove( pCarriedWeapon );
			}
		}
	}

	if ( m_bRegenerating == false )
	{
		SetActiveWeapon( NULL );
		Weapon_Switch( Weapon_GetSlot( 0 ) );
		Weapon_SetLast( Weapon_GetSlot( 1 ) );
	}

	// If our max health dropped below current due to item changes, drop our current health.
	// If we're not being buffed, clamp it to max. Otherwise, clamp it to the max buffed health
	int iMaxHealth = m_Shared.InCond( TF_COND_HEALTH_BUFF ) ? m_Shared.GetMaxBuffedHealth() : GetMaxHealth();
	if ( m_iHealth > iMaxHealth )
	{
		// Modify health manually to prevent showing all the "you got hurt" UI. 
		m_iHealth = iMaxHealth;
	}

	if ( TFGameRules()->InStalemate() && mp_stalemate_meleeonly.GetBool() )
	{
		CBaseCombatWeapon *meleeWeapon = Weapon_GetSlot( TF_WPN_TYPE_MELEE );
		if ( meleeWeapon )
		{
			Weapon_Switch( meleeWeapon );
		}
	}

	if ( m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		// Using weapons lockers destroys our disguise weapon, so we might need a new one.
		m_Shared.DetermineDisguiseWeapon( false );
	}

	// Remove our disguise if we can't disguise.
	if ( !CanDisguise() )
	{
		RemoveDisguise();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ManageDODWeapons( TFPlayerClassData_t *pData )
{
	if ( !m_bRegenerating )
	{
		// Clean up any leftover weapons from previous games
		for ( int iWeapon = 0; iWeapon < TF_PLAYER_WEAPON_COUNT; ++iWeapon )
		{
			CBaseCombatWeapon *pWeapon = (CBaseCombatWeapon *)GetWeapon( iWeapon );
			if ( !pWeapon )
				continue;

			Weapon_Detach( pWeapon );
			UTIL_Remove( pWeapon );
		}
	}

	for ( int iWeapon = 0; iWeapon < DOD_PLAYER_WEAPON_COUNT; ++iWeapon )
	{
		if ( pData->m_aWeapons[iWeapon] != TF_WEAPON_NONE )
		{
			int iWeaponID = pData->m_aWeapons[iWeapon];
			const char *pszWeaponName = WeaponIdToClassname( iWeaponID );

			CWeaponDODBase *pWeapon = (CWeaponDODBase *)GetWeapon( iWeapon );

			//If we already have a weapon in this slot but is not the same type then nuke it (changed classes)
			if ( pWeapon && pWeapon->GetWeaponID() != iWeaponID )
			{
				Weapon_Detach( pWeapon );
				UTIL_Remove( pWeapon );
			}

			pWeapon = (CWeaponDODBase *)DODWeapon_OwnsThisID( iWeaponID );

			if ( pWeapon )
			{
				pWeapon->ChangeTeam( GetTeamNumber() );
			}
			else
			{
				pWeapon = (CWeaponDODBase *)GiveNamedItem( pszWeaponName );

				if ( pWeapon )
				{
					pWeapon->DefaultTouch( this );
				}
			}

			if ( pWeapon )
			{
				int iNumClip = pWeapon->GetPCWpnData().m_DODWeaponData.m_iDefaultAmmoClips;
				int iClipSize = pWeapon->GetPCWpnData().iMaxClip1;
				GiveAmmo( iNumClip * iClipSize, GetAmmoDef()->Index( pWeapon->GetPCWpnData().szAmmo1 ) );
				pWeapon->SetClip1( iClipSize );
			}
		}
		else
		{
			//I shouldn't have any weapons in this slot, so get rid of it
			CWeaponDODBase *pCarriedWeapon = (CWeaponDODBase *)GetWeapon( iWeapon );

			if ( pCarriedWeapon )
			{
				Weapon_Detach( pCarriedWeapon );
				UTIL_Remove( pCarriedWeapon );
			}
		}
	}

	if ( !m_bRegenerating )
	{
		SetActiveWeapon( NULL );
		Weapon_Switch( Weapon_GetSlot( 0 ) );
		Weapon_SetLast( Weapon_GetSlot( 1 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ManageDODGrenades( TFPlayerClassData_t *pData )
{
	// Continue where normal weapons left off
	for ( int iWeapon = DOD_PLAYER_WEAPON_COUNT; iWeapon < DOD_PLAYER_WEAPON_COUNT + DOD_PLAYER_GRENADE_COUNT; ++iWeapon )
	{
		int iGrenadeType = pData->m_iGrenType1;
		int iNumGrenades = pData->m_iNumGrensType1;

		if ( iWeapon == DOD_PLAYER_WEAPON_COUNT + 1 )
		{
			iGrenadeType = pData->m_iGrenType2;
			iNumGrenades = pData->m_iNumGrensType2;
		}

		if ( iGrenadeType != TF_WEAPON_NONE )
		{
			int iWeaponID = iGrenadeType;
			const char *pszWeaponName = WeaponIdToClassname( iWeaponID );

			CWeaponDODBase *pWeapon = (CWeaponDODBase *)GetWeapon( iWeapon );

			//If we already have a weapon in this slot but is not the same type then nuke it (changed classes)
			if ( pWeapon && pWeapon->GetWeaponID() != iWeaponID )
			{
				Weapon_Detach( pWeapon );
				UTIL_Remove( pWeapon );
			}

			pWeapon = (CWeaponDODBase *)DODWeapon_OwnsThisID( iWeaponID );

			if ( pWeapon )
			{
				pWeapon->ChangeTeam( GetTeamNumber() );
			}
			else
			{
				pWeapon = (CWeaponDODBase *)GiveNamedItem( pszWeaponName );

				if ( pWeapon )
				{
					pWeapon->DefaultTouch( this );
				}
			}

			if ( pWeapon )
			{
				GiveAmmo( iNumGrenades, GetAmmoDef()->Index( pWeapon->GetPCWpnData().szAmmo1 ) );
			}
		}
		else
		{
			//I shouldn't have any weapons in this slot, so get rid of it
			CWeaponDODBase *pCarriedWeapon = (CWeaponDODBase *)GetWeapon( iWeapon );

			if ( pCarriedWeapon )
			{
				Weapon_Detach( pCarriedWeapon );
				UTIL_Remove( pCarriedWeapon );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ManageCSWeapons( TFPlayerClassData_t *pData )
{
	if ( !m_bRegenerating )
	{
		// Clean up any leftover weapons from previous games
		for ( int iWeapon = 0; iWeapon < TF_PLAYER_WEAPON_COUNT; ++iWeapon )
		{
			CBaseCombatWeapon *pWeapon = (CBaseCombatWeapon *)GetWeapon( iWeapon );
			if ( !pWeapon )
				continue;

			Weapon_Detach( pWeapon );
			UTIL_Remove( pWeapon );
		}
	}

	for ( int iWeapon = 0; iWeapon < DOD_PLAYER_WEAPON_COUNT; ++iWeapon )
	{
		if ( pData->m_aWeapons[iWeapon] != TF_WEAPON_NONE )
		{
			int iWeaponID = pData->m_aWeapons[iWeapon];
			const char *pszWeaponName = WeaponIdToClassname( iWeaponID );

			CWeaponCSBase *pWeapon = (CWeaponCSBase *)GetWeapon( iWeapon );

			//If we already have a weapon in this slot but is not the same type then nuke it (changed classes)
			if ( pWeapon && pWeapon->GetWeaponID() != iWeaponID )
			{
				Weapon_Detach( pWeapon );
				UTIL_Remove( pWeapon );
			}

			pWeapon = (CWeaponCSBase *)CSWeapon_OwnsThisID( iWeaponID );

			if ( pWeapon )
			{
				pWeapon->ChangeTeam( GetTeamNumber() );
			}
			else
			{
				pWeapon = (CWeaponCSBase *)GiveNamedItem( pszWeaponName );

				if ( pWeapon )
				{
					pWeapon->DefaultTouch( this );
				}
			}

			if ( pWeapon )
			{
				GiveAmmo( 9999, GetAmmoDef()->Index( pWeapon->GetPCWpnData().szAmmo1 ) );
			}
		}
		else
		{
			//I shouldn't have any weapons in this slot, so get rid of it
			CWeaponCSBase *pCarriedWeapon = (CWeaponCSBase *)GetWeapon( iWeapon );

			if ( pCarriedWeapon )
			{
				Weapon_Detach( pCarriedWeapon );
				UTIL_Remove( pCarriedWeapon );
			}
		}
	}

	if ( !m_bRegenerating )
	{
		SetActiveWeapon( NULL );
		// always switch to the pistol
		Weapon_Switch( Weapon_GetSlot( 1 ) );
		Weapon_SetLast( Weapon_GetSlot( 1 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find a spawn point for the player.
//-----------------------------------------------------------------------------
CBaseEntity* CTFPlayer::EntSelectSpawnPoint()
{
	CBaseEntity *pSpot = g_pLastSpawnPoints[ GetTeamNumber() ];
	const char *pSpawnPointName = "";

	switch( GetTeamNumber() )
	{
	case TF_TEAM_RED:
	case TF_TEAM_BLUE:
		{
			pSpawnPointName = "info_player_teamspawn";
			if ( SelectSpawnSpot( pSpawnPointName, pSpot ) )
			{
				g_pLastSpawnPoints[ GetTeamNumber() ] = pSpot;
			}

			// need to save this for later so we can apply and modifiers to the armor and grenades...after the call to InitClass()
			m_pSpawnPoint = dynamic_cast<CTFTeamSpawn*>( pSpot );
			break;
		}
	case TEAM_SPECTATOR:
	case TEAM_UNASSIGNED:
	default:
		{
			pSpot = CBaseEntity::Instance( INDEXENT(0) );
			break;		
		}
	}

	if ( !pSpot )
	{
		Warning( "PutClientInServer: no %s on level\n", pSpawnPointName );
		return CBaseEntity::Instance( INDEXENT(0) );
	}

	return pSpot;
} 

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayer::SelectSpawnSpot( const char *pEntClassName, CBaseEntity* &pSpot )
{
	// Get an initial spawn point.
	pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
	if ( !pSpot )
	{
		// Sometimes the first spot can be NULL????
		pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
	}

	// First we try to find a spawn point that is fully clear. If that fails,
	// we look for a spawnpoint that's clear except for another players. We
	// don't collide with our team members, so we should be fine.
	bool bIgnorePlayers = false;

	CBaseEntity *pFirstSpot = pSpot;
	do 
	{
		if ( pSpot )
		{
			// Check to see if this is a valid team spawn (player is on this team, etc.).
			if( TFGameRules()->IsSpawnPointValid( pSpot, this, bIgnorePlayers ) )
			{
				// Check for a bad spawn entity.
				if ( pSpot->GetAbsOrigin() == Vector( 0, 0, 0 ) )
				{
					pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
					continue;
				}

				// Found a valid spawn point.
				return true;
			}
		}

		// Get the next spawning point to check.
		pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );

		if ( pSpot == pFirstSpot && !bIgnorePlayers )
		{
			// Loop through again, ignoring players
			bIgnorePlayers = true;
			pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
		}
	} 
	// Continue until a valid spawn point is found or we hit the start.
	while ( pSpot != pFirstSpot ); 

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	if ( GetGameAnimStateType() )
		GetGameAnimStateType()->DoAnimationEvent( event, nData );

	TE_PlayerAnimEvent( this, event, nData );	// Send to any clients who can see this guy.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PhysObjectSleep()
{
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj )
		pObj->Sleep();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PhysObjectWake()
{
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj )
		pObj->Wake();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayer::GetAutoTeam( void )
{
	int iTeam = TEAM_SPECTATOR;

	CTFTeam *pBlue = TFTeamMgr()->GetTeam( TF_TEAM_BLUE );
	CTFTeam *pRed  = TFTeamMgr()->GetTeam( TF_TEAM_RED );

	if ( pBlue && pRed )
	{
		if ( pBlue->GetNumPlayers() < pRed->GetNumPlayers() )
		{
			iTeam = TF_TEAM_BLUE;
		}
		else if ( pRed->GetNumPlayers() < pBlue->GetNumPlayers() )
		{
			iTeam = TF_TEAM_RED;
		}
		else
		{
			iTeam = RandomInt( 0, 1 ) ? TF_TEAM_RED : TF_TEAM_BLUE;
		}
	}

	return iTeam;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::HandleCommand_JoinTeam( const char *pTeamName )
{
	int iTeam = TF_TEAM_RED;
	if ( stricmp( pTeamName, "auto" ) == 0 )
	{
		iTeam = GetAutoTeam();
	}
	else if ( stricmp( pTeamName, "spectate" ) == 0 )
	{
		iTeam = TEAM_SPECTATOR;
	}
	else
	{
		for ( int i = 0; i < TF_TEAM_COUNT; ++i )
		{
			if ( stricmp( pTeamName, g_aTeamNames[i] ) == 0 )
			{
				iTeam = i;
				break;
			}
		}
	}

	if ( iTeam == TEAM_SPECTATOR )
	{
		// Prevent this is the cvar is set
		if ( !mp_allowspectators.GetInt() && !IsHLTV() )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Cannot_Be_Spectator" );
			return;
		}
		
		if ( GetTeamNumber() != TEAM_UNASSIGNED && !IsDead() )
		{
			CommitSuicide( false, true );
		}

		ChangeTeam( TEAM_SPECTATOR );

		// do we have fadetoblack on? (need to fade their screen back in)
		if ( mp_fadetoblack.GetBool() )
		{
			color32_s clr = { 0,0,0,255 };
			UTIL_ScreenFade( this, clr, 0, 0, FFADE_IN | FFADE_PURGE );
		}
	}
	else
	{
		if ( iTeam == GetTeamNumber() )
		{
			return;	// we wouldn't change the team
		}

		// if this join would unbalance the teams, refuse
		// come up with a better way to tell the player they tried to join a full team!
		if ( TFGameRules()->WouldChangeUnbalanceTeams( iTeam, GetTeamNumber() ) )
		{
			ShowViewPortPanel( PANEL_TEAM );
			return;
		}

		ChangeTeam( iTeam );

		ShowViewPortPanel( PANEL_GAMEMENU, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Join a team without using the game menus
//-----------------------------------------------------------------------------
void CTFPlayer::HandleCommand_JoinTeam_NoMenus( const char *pTeamName )
{
	Assert( IsX360() );

	Msg( "Client command HandleCommand_JoinTeam_NoMenus: %s\n", pTeamName );

	// Only expected to be used on the 360 when players leave the lobby to start a new game
	if ( !IsInCommentaryMode() )
	{
		Assert( GetTeamNumber() == TEAM_UNASSIGNED );
		Assert( IsX360() );
	}

	int iTeam = TEAM_SPECTATOR;
	if ( Q_stricmp( pTeamName, "spectate" ) )
	{
		for ( int i = 0; i < TF_TEAM_COUNT; ++i )
		{
			if ( stricmp( pTeamName, g_aTeamNames[i] ) == 0 )
			{
				iTeam = i;
				break;
			}
		}
	}

	ForceChangeTeam( iTeam );
}

//-----------------------------------------------------------------------------
// Purpose: Player has been forcefully changed to another team
//-----------------------------------------------------------------------------
void CTFPlayer::ForceChangeTeam( int iTeamNum, bool bFullTeamSwitch )
{
	int iNewTeam = iTeamNum;

	if ( iNewTeam == TF_TEAM_AUTOASSIGN )
	{
		iNewTeam = GetAutoTeam();
	}

	if ( !GetGlobalTeam( iNewTeam ) )
	{
		Warning( "CTFPlayer::ForceChangeTeam( %d ) - invalid team index.\n", iNewTeam );
		return;
	}

	int iOldTeam = GetTeamNumber();

	// if this is our current team, just abort
	if ( iNewTeam == iOldTeam )
		return;

	RemoveAllOwnedEntitiesFromWorld( true );

	BaseClass::ChangeTeam( iNewTeam );

	if ( !bFullTeamSwitch )
	{
		RemoveNemesisRelationships();

		if ( TFGameRules() && TFGameRules()->IsInHighlanderMode() )
		{
			if ( IsAlive() )
			{
				CommitSuicide( false, true );
			}

//			ResetPlayerClass();
		}
	}

	if ( iNewTeam == TEAM_UNASSIGNED )
	{
		StateTransition( TF_STATE_OBSERVER );
	}
	else if ( iNewTeam == TEAM_SPECTATOR )
	{
		m_bIsIdle = false;
		StateTransition( TF_STATE_OBSERVER );

		RemoveAllWeapons();
		DestroyViewModels();
	}

	// Don't modify living players in any way
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::HandleFadeToBlack( void )
{
	if ( mp_fadetoblack.GetBool() )
	{
		color32_s clr = { 0,0,0,255 };
		UTIL_ScreenFade( this, clr, 0.75, 0, FFADE_OUT | FFADE_STAYOUT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ChangeTeam( int iTeamNum )
{
	if ( !GetGlobalTeam( iTeamNum ) )
	{
		Warning( "CTFPlayer::ChangeTeam( %d ) - invalid team index.\n", iTeamNum );
		return;
	}

	int iOldTeam = GetTeamNumber();

	// if this is our current team, just abort
	if ( iTeamNum == iOldTeam )
		return;

	RemoveAllOwnedEntitiesFromWorld( true );
	RemoveNemesisRelationships();

	BaseClass::ChangeTeam( iTeamNum );

	if ( iTeamNum == TEAM_UNASSIGNED )
	{
		StateTransition( TF_STATE_OBSERVER );
	}
	else if ( iTeamNum == TEAM_SPECTATOR )
	{
		m_bIsIdle = false;

		StateTransition( TF_STATE_OBSERVER );

		RemoveAllWeapons();
		DestroyViewModels();
	}
	else // active player
	{
		if ( !IsDead() && (iOldTeam == TF_TEAM_RED || iOldTeam == TF_TEAM_BLUE) )
		{
			// Kill player if switching teams while alive
			CommitSuicide( false, true );
		}
		else if ( IsDead() && iOldTeam < FIRST_GAME_TEAM )
		{
			SetObserverMode( OBS_MODE_CHASE );
			HandleFadeToBlack();
		}

		// let any spies disguising as me know that I've changed teams
		for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
		{
			CTFPlayer *pTemp = ToTFPlayer( UTIL_PlayerByIndex( i ) );
			if ( pTemp && pTemp != this )
			{
				if ( ( pTemp->m_Shared.GetDisguiseTarget() == this ) || // they were disguising as me and I've changed teams
 					 ( !pTemp->m_Shared.GetDisguiseTarget() && pTemp->m_Shared.GetDisguiseTeam() == iTeamNum ) ) // they don't have a disguise and I'm joining the team they're disguising as
				{
					// choose someone else...
					pTemp->m_Shared.FindDisguiseTarget();
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::HandleCommand_JoinClass( const char *pClassName )
{
	// can only join a class after you join a valid team
	if ( GetTeamNumber() <= LAST_SHARED_TEAM )
		return;

	// In case we don't get the class menu message before the spawn timer
	// comes up, fake that we've closed the menu.
	SetClassMenuOpen( false );

	if ( TFGameRules()->InStalemate() )
	{
		if ( IsAlive() && !TFGameRules()->CanChangeClassInStalemate() )
		{
			ClientPrint(this, HUD_PRINTTALK, "#game_stalemate_cant_change_class" );
			return;
		}
	}

	int iClass = TF_CLASS_UNDEFINED;
	int iTranslatedClass = -1;
	bool bShouldNotRespawn = false;

	if ( ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN ) && ( TFGameRules()->GetWinningTeam() != GetTeamNumber() ) )
	{
		m_bAllowInstantSpawn = false;
		bShouldNotRespawn = true;
	}

	if ( stricmp( pClassName, "civilian" ) == 0 )
	{
		Warning( "You're not a good person!\n" );
		return;
	}

	if ( stricmp( pClassName, "random" ) != 0 
		&& stricmp( pClassName, "dod_random" ) != 0 
		&& stricmp( pClassName, "tf_random" ) != 0 )
	{
		int i = 0;

		for ( i = TF_CLASS_SCOUT; i < GAME_CLASS_COUNT; i++ )
		{
			if ( stricmp( pClassName, GetPlayerClassData( i )->m_szClassName ) == 0 )
			{
				iClass = i;
				break;
			}
		}
		if ( i >= GAME_CLASS_COUNT || ( i == PC_CLASS_AIGIS && !PlayerHasPowerplay() ) )
		{
			Warning( "HandleCommand_JoinClass( %s ) - invalid class name.\n", pClassName );
			return;
		}
	}
	else
	{
		int iMin = TF_FIRST_NORMAL_CLASS; 
		int iMax = TF_LAST_NORMAL_CLASS;

		if ( stricmp( pClassName, "tf_random" ) == 0 )
			iMin = TF_FIRST_NORMAL_CLASS; iMax = TF_LAST_NORMAL_CLASS;

		if ( stricmp( pClassName, "dod_random" ) == 0 )
		{
			iMin = GetTeamNumber() == TF_TEAM_RED ? DOD_FIRST_ALLIES_CLASS : DOD_FIRST_AXIS_CLASS;
			iMax = GetTeamNumber() == TF_TEAM_RED ? DOD_LAST_ALLIES_CLASS : DOD_LAST_AXIS_CLASS;
		}

		// The player has selected Random class...so let's pick one for them.
		do{
			// Don't let them be the same class twice in a row
			iClass = random->RandomInt( iMin, iMax );
		} while( iClass == GetPlayerClass()->GetClassIndex() );
	}

	iTranslatedClass = TranslateClassIndex( iClass, GetTeamNumber() );
	if ( iTranslatedClass != -1 )
		iClass = iTranslatedClass;

	// joining the same class?
	if ( iClass != TF_CLASS_RANDOM && iClass == GetDesiredPlayerClassIndex() )
	{
		// If we're dead, and we have instant spawn, respawn us immediately. Catches the case
		// were a player misses respawn wave because they're at the class menu, and then changes
		// their mind and reselects their current class.
		if ( m_bAllowInstantSpawn && !IsAlive() )
		{
			ForceRespawn();
		}
		return;
	}

	SetDesiredPlayerClassIndex( iClass );
	IGameEvent * event = gameeventmanager->CreateEvent( "player_changeclass" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetInt( "class", iClass );

		gameeventmanager->FireEvent( event );
	}

	// are they TF_CLASS_RANDOM and trying to select the class they're currently playing as (so they can stay this class)?
	if ( iClass == GetPlayerClass()->GetClassIndex() )
	{
		// If we're dead, and we have instant spawn, respawn us immediately. Catches the case
		// were a player misses respawn wave because they're at the class menu, and then changes
		// their mind and reselects their current class.
		if ( m_bAllowInstantSpawn && !IsAlive() )
		{
			ForceRespawn();
		}
		return;
	}

	// We can respawn instantly if:
	//	- We're dead, and we're past the required post-death time
	//	- We're inside a respawn room
	//	- We're in the stalemate grace period
	bool bInRespawnRoom = PointInRespawnRoom( this, WorldSpaceCenter() );
	if ( bInRespawnRoom && !IsAlive() )
	{
		// If we're not spectating ourselves, ignore respawn rooms. Otherwise we'll get instant spawns
		// by spectating someone inside a respawn room.
		bInRespawnRoom = (GetObserverTarget() == this);
	}
	bool bDeadInstantSpawn = !IsAlive();
	if ( bDeadInstantSpawn && m_flDeathTime )
	{
		// In death mode, don't allow class changes to force respawns ahead of respawn waves
		float flWaveTime = TFGameRules()->GetNextRespawnWave( GetTeamNumber(), this );
		bDeadInstantSpawn = (gpGlobals->curtime > flWaveTime);
	}
	bool bInStalemateClassChangeTime = false;
	if ( TFGameRules()->InStalemate() )
	{
		// Stalemate overrides respawn rules. Only allow spawning if we're in the class change time.
		bInStalemateClassChangeTime = TFGameRules()->CanChangeClassInStalemate();
		bDeadInstantSpawn = false;
		bInRespawnRoom = false;
	}
	if ( bShouldNotRespawn == false && ( m_bAllowInstantSpawn || bDeadInstantSpawn || bInRespawnRoom || bInStalemateClassChangeTime ) )
	{
		ForceRespawn();
		return;
	}

	if( iClass == TF_CLASS_RANDOM )
	{
		if( IsAlive() )
		{
			ClientPrint(this, HUD_PRINTTALK, "#game_respawn_asrandom" );
		}
		else
		{
			ClientPrint(this, HUD_PRINTTALK, "#game_spawn_asrandom" );
		}
	}
	else
	{
		if( IsAlive() )
		{
			ClientPrint(this, HUD_PRINTTALK, "#game_respawn_as", GetPlayerClassData( iClass )->m_szLocalizableName );
		}
		else
		{
			ClientPrint(this, HUD_PRINTTALK, "#game_spawn_as", GetPlayerClassData( iClass )->m_szLocalizableName );
		}
	}

	if ( IsAlive() && ( GetHudClassAutoKill() == true ) && bShouldNotRespawn == false )
	{
		CommitSuicide( false, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::ClientCommand( const CCommand &args )
{
	const char *pcmd = args[0];
	
	m_flLastAction = gpGlobals->curtime;

	if ( FStrEq( pcmd, "addcond" ) )
	{
		if ( sv_cheats->GetBool() && args.ArgC() >= 2 )
		{
			ETFCond eCond = (ETFCond)clamp( atoi( args[1] ), 0, TF_COND_LAST-1 );

			CTFPlayer *pTargetPlayer = this;
			if ( args.ArgC() >= 4 )
			{
				// Find the matching netname
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *pPlayer = ToBasePlayer( UTIL_PlayerByIndex(i) );
					if ( pPlayer )
					{
						if ( Q_strstr( pPlayer->GetPlayerName(), args[3] ) )
						{
							pTargetPlayer = ToTFPlayer(pPlayer);
							break;
						}
					}
				}
			}

			if ( args.ArgC() >= 3 )
			{
				float flDuration = atof( args[2] );
				pTargetPlayer->m_Shared.AddCond( eCond, flDuration );
			}
			else
			{
				pTargetPlayer->m_Shared.AddCond( eCond );
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "removecond" ) )
	{
		if ( sv_cheats->GetBool() && args.ArgC() >= 2 )
		{
			CTFPlayer *pTargetPlayer = this;
			if ( args.ArgC() >= 3 )
			{
				// Find the matching netname
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *pPlayer = ToBasePlayer( UTIL_PlayerByIndex(i) );
					if ( pPlayer )
					{
						if ( Q_strstr( pPlayer->GetPlayerName(), args[2] ) )
						{
							pTargetPlayer = ToTFPlayer(pPlayer);
							break;
						}
					}
				}
			}

			ETFCond eCond = (ETFCond)clamp( atoi( args[1] ), 0, TF_COND_LAST-1 );
			pTargetPlayer->m_Shared.RemoveCond( eCond );
		}
		return true;
	}
#ifdef _DEBUG
	else if ( FStrEq( pcmd, "burn" ) ) 
	{
		m_Shared.Burn( this );
		return true;
	}
	else if ( FStrEq( pcmd, "dump_damagers" ) )
	{
		m_AchievementData.DumpDamagers();
		return true;
	}
	else if ( FStrEq( pcmd, "stun" ) )
	{
		if ( args.ArgC() >= 4 )
		{
			m_Shared.StunPlayer( atof(args[1]), atof(args[2]), atof(args[3]) );
		}
		return true;
	}
#endif

	if ( FStrEq( pcmd, "jointeam" ) )
	{
		if ( args.ArgC() >= 2 )
		{
			HandleCommand_JoinTeam( args[1] );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "jointeam_nomenus" ) )
	{
		if ( IsX360() )
		{
			if ( args.ArgC() >= 2 )
			{
				HandleCommand_JoinTeam_NoMenus( args[1] );
			}
			return true;
		}
		return false;
	}
	else if ( FStrEq( pcmd, "closedwelcomemenu" ) )
	{
		if ( GetTeamNumber() == TEAM_UNASSIGNED )
		{
			ShowViewPortPanel( PANEL_TEAM, true );
		}
		else if ( IsPlayerClass( TF_CLASS_UNDEFINED ) )
		{
			ShowViewPortPanel( PANEL_GAMEMENU, true );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "joinclass" ) ) 
	{
		if ( args.ArgC() >= 2 )
		{
			HandleCommand_JoinClass( args[1] );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "mp_playgesture" ) )
	{
		if ( args.ArgC() == 1 )
		{
			Warning( "mp_playgesture: Gesture activity or sequence must be specified!\n" );
			return true;
		}

		if ( sv_cheats->GetBool() )
		{
			if ( !PlayGesture( args[1] ) )
			{
				Warning( "mp_playgesture: unknown sequence or activity name \"%s\"\n", args[1] );
				return true;
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "mp_playanimation" ) )
	{
		if ( args.ArgC() == 1 )
		{
			Warning( "mp_playanimation: Activity or sequence must be specified!\n" );
			return true;
		}

		if ( sv_cheats->GetBool() )
		{
			if ( !PlaySpecificSequence( args[1] ) )
			{
				Warning( "mp_playanimation: Unknown sequence or activity name \"%s\"\n", args[1] );
				return true;
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "menuopen" ) )
	{
		SetClassMenuOpen( true );
		return true;
	}
	else if ( FStrEq( pcmd, "menuclosed" ) )
	{
		SetClassMenuOpen( false );
		return true;
	}
	else if ( FStrEq( pcmd, "pda_click" ) )
	{
		if ( ShouldRunRateLimitedCommand( args ) )
		{
			// player clicked on the PDA, play attack animation
			CTFWeaponPDA *pPDA = dynamic_cast<CTFWeaponPDA *>( GetActivePCWeapon() );
			if ( pPDA && !m_Shared.InCond( TF_COND_DISGUISED ) )
			{
				DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "taunt" ) )
	{
		Taunt();
		return true;
	}
	else if ( FStrEq( pcmd, "build" ) )
	{
		if ( ShouldRunRateLimitedCommand( args ) )
		{
			if ( TFGameRules()->InStalemate() && mp_stalemate_meleeonly.GetBool() )
				return true;

			// can't issue a build command while carrying an object
			if ( m_Shared.IsCarryingObject() )
				return true;

			if ( IsTaunting() )
				return true;

			int iBuilding = 0;
			int iMode = 0;
			bool bArgsChecked = false;

			// Fixup old binds.
			if ( args.ArgC() == 2 )
			{
				iBuilding = atoi( args[ 1 ] );
				if ( iBuilding == 3 ) // Teleport exit is now a mode.
				{
					iBuilding = 1;
					iMode = 1;
				}
				bArgsChecked = true;
			}
			else if ( args.ArgC() == 3 )
			{
				iBuilding = atoi( args[ 1 ] );
				iMode = atoi( args[ 2 ] );
				bArgsChecked = true;
			}

			if ( bArgsChecked )
			{
				StartBuildingObjectOfType( iBuilding, iMode );
			}
			else
			{
				Warning( "Usage: build <building> <mode>\n" );
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "destroy" ) )
	{
		if ( ShouldRunRateLimitedCommand( args ) )
		{
			if ( IsPlayerClass( TF_CLASS_ENGINEER ) ) // Spies can't destroy buildings (sappers)
			{
				int iBuilding = 0;
				int iMode = 0;
				bool bArgsChecked = false;

				// Fixup old binds.
				if ( args.ArgC() == 2 )
				{
					iBuilding = atoi( args[ 1 ] );
					if ( iBuilding == 3 ) // Teleport exit is now a mode.
					{
						iBuilding = 1;
						iMode = 1;
					}
					bArgsChecked = true;
				}
				else if ( args.ArgC() == 3 )
				{
					iBuilding = atoi( args[ 1 ] );
					iMode = atoi( args[ 2 ] );
					bArgsChecked = true;
				}

				if ( bArgsChecked )
				{
					DetonateObjectOfType( iBuilding, iMode );
				}
				else
				{
					Warning( "Usage: destroy <building> <mode>\n" );
				}
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "extendfreeze" ) )
	{
		m_flDeathTime += 2.0f;
		return true;
	}
	else if ( FStrEq( pcmd, "show_motd" ) )
	{
		KeyValues *data = new KeyValues( "data" );
		data->SetString( "title", "#TF_Welcome" );	// info panel title
		data->SetString( "type", "1" );				// show userdata from stringtable entry
		data->SetString( "msg",	"motd" );			// use this stringtable entry
		data->SetString( "cmd", "mapinfo" );		// exec this command if panel closed

		ShowViewPortPanel( PANEL_INFO, true, data );

		data->deleteThis();
	}
	else if ( FStrEq( pcmd, "condump_on" ) )
	{
		if ( !PlayerHasPowerplay() )
		{
			Msg("Console dumping on.\n");
			return true;
		}
		else 
		{
			if ( args.ArgC() == 2 && GetTeam() )
			{
				for ( int i = 0; i < GetTeam()->GetNumPlayers(); i++ )
				{
					CTFPlayer *pTeamPlayer = ToTFPlayer( GetTeam()->GetPlayer(i) );
					if ( pTeamPlayer )
					{
						pTeamPlayer->SetPowerplayEnabled( true );
					}
				}
				return true;
			}
			else
			{
				if ( SetPowerplayEnabled( true ) )
					return true;
			}
		}
	}
	else if ( FStrEq( pcmd, "condump_off" ) )
	{
		if ( !PlayerHasPowerplay() )
		{
			Msg("Console dumping off.\n");
			return true;
		}
		else
		{
			if ( args.ArgC() == 2 && GetTeam() )
			{
				for ( int i = 0; i < GetTeam()->GetNumPlayers(); i++ )
				{
					CTFPlayer *pTeamPlayer = ToTFPlayer( GetTeam()->GetPlayer(i) );
					if ( pTeamPlayer )
					{
						pTeamPlayer->SetPowerplayEnabled( false );
					}
				}
				return true;
			}
			else
			{
				if ( SetPowerplayEnabled( false ) )
					return true;
			}
		}
	} 
	else if ( FStrEq( pcmd, "drop" ) ) // DOD
	{
		DropActiveWeapon();
		return true;
	}

	return BaseClass::ClientCommand( args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SetClassMenuOpen( bool bOpen )
{
	m_bIsClassMenuOpen = bOpen;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::IsClassMenuOpen( void )
{
	return m_bIsClassMenuOpen;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::PlayGesture( const char *pGestureName )
{
	Activity nActivity = (Activity)LookupActivity( pGestureName );
	if ( nActivity != ACT_INVALID )
	{
		DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_GESTURE, nActivity );
		return true;
	}

	int nSequence = LookupSequence( pGestureName );
	if ( nSequence != -1 )
	{
		DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_GESTURE_SEQUENCE, nSequence );
		return true;
	} 

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::PlaySpecificSequence( const char *pAnimationName )
{
	Activity nActivity = (Activity)LookupActivity( pAnimationName );
	if ( nActivity != ACT_INVALID )
	{
		DoAnimationEvent( PLAYERANIMEVENT_CUSTOM, nActivity );
		return true;
	}

	int nSequence = LookupSequence( pAnimationName );
	if ( nSequence != -1 )
	{
		DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_SEQUENCE, nSequence );
		return true;
	} 

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DetonateObjectOfType( int iType, int iMode, bool bIgnoreSapperState )
{
	CBaseObject *pObj = GetObjectOfType( iType, iMode );
	if( !pObj )
		return;

	if( !bIgnoreSapperState && ( pObj->HasSapper() || pObj->IsPlasmaDisabled() ) )
		return;

	IGameEvent *event = gameeventmanager->CreateEvent( "object_removed" );	
	if ( event )
	{
		event->SetInt( "userid", GetUserID() ); // user ID of the object owner
		event->SetInt( "objecttype", iType ); // type of object removed
		event->SetInt( "index", pObj->entindex() ); // index of the object removed
		gameeventmanager->FireEvent( event );
	}

	SpeakConceptIfAllowed( MP_CONCEPT_DETONATED_OBJECT, pObj->GetResponseRulesModifier() );
	pObj->DetonateObject();

	const CObjectInfo *pInfo = GetObjectInfo( iType );

	if ( pInfo )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"killedobject\" (object \"%s\") (weapon \"%s\") (objectowner \"%s<%i><%s><%s>\") (attacker_position \"%d %d %d\")\n",   
			GetPlayerName(),
			GetUserID(),
			GetNetworkIDString(),
			GetTeam()->GetName(),
			pInfo->m_pObjectName,
			"pda_engineer",
			GetPlayerName(),
			GetUserID(),
			GetNetworkIDString(),
			GetTeam()->GetName(),
			(int)GetAbsOrigin().x, 
			(int)GetAbsOrigin().y,
			(int)GetAbsOrigin().z );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayer::GetObjectBuildSpeedMultiplier( int iObjectType, bool bIsRedeploy ) const
{
	float flBuildRate = 1.f; // need a base value for mult

	switch( iObjectType )
	{
	case OBJ_SENTRYGUN:
		flBuildRate += bIsRedeploy ? 2.0 : 0.0f;
		break;

	case OBJ_TELEPORTER:
		flBuildRate += bIsRedeploy ? 3.0 : 0.0f;
		break;

	case OBJ_DISPENSER:
		flBuildRate += bIsRedeploy ? 3.0 : 0.0f;
		break;
	}

	return flBuildRate - 1.0f; // sub out the initial 1 so the final result is added
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	if ( m_takedamage != DAMAGE_YES )
		return;

	CTFPlayer *pAttacker = (CTFPlayer*)ToTFPlayer( info.GetAttacker() );
	if ( pAttacker )
	{
		// Prevent team damage here so blood doesn't appear
		if ( info.GetAttacker()->IsPlayer() )
		{
			if ( !g_pGameRules->FPlayerCanTakeDamage( this, info.GetAttacker(), info ) )
				return;
		}

		CWeaponDODBase *pDODWeapon = dynamic_cast< CWeaponDODBase * >( pAttacker->GetActivePCWeapon() );
		if ( pDODWeapon )
			return TraceAttackDOD( info, vecDir, ptr, pAccumulator );
	}

	// Save this bone for the ragdoll.
	m_nForceBone = ptr->physicsbone;

	SetLastHitGroup( ptr->hitgroup );

	// Ignore hitboxes for all weapons except the sniper rifle

	CTakeDamageInfo info_modified = info;

	if ( info_modified.GetDamageType() & DMG_USE_HITLOCATIONS )
	{
		if ( !m_Shared.InCond( TF_COND_INVULNERABLE ) && ptr->hitgroup == HITGROUP_HEAD )
		{
			CPCWeaponBase *pWpn = pAttacker->GetActivePCWeapon();
			bool bCritical = true;

			if ( pWpn )
			{
				CTFWeaponBase *pTFWeapon = dynamic_cast<CTFWeaponBase*>( pWpn );
				if ( pTFWeapon && !pTFWeapon->CanFireCriticalShot( true ) )
					bCritical = false;
			}

			if ( bCritical )
			{
				info_modified.AddDamageType( DMG_CRITICAL );
				info_modified.SetDamageCustom( TF_DMG_CUSTOM_HEADSHOT );

				// play the critical shot sound to the shooter	
				if ( pWpn )
				{
					pWpn->WeaponSound( BURST );
				}
			}
		}
	}

	if ( m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		// no impact effects
	}
	else if ( m_Shared.InCond( TF_COND_INVULNERABLE ) )
	{ 
		// Make bullet impacts
		g_pEffects->Ricochet( ptr->endpos - (vecDir * 8), -vecDir );
	}
	else
	{	
		// Since this code only runs on the server, make sure it shows the tempents it creates.
		CDisablePredictionFiltering disabler;

		// This does smaller splotches on the guy and splats blood on the world.
		TraceBleed( info_modified.GetDamage(), vecDir, ptr, info_modified.GetDamageType() );
	}

	AddMultiDamage( info_modified, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayer::TakeHealth( float flHealth, int bitsDamageType )
{
	int bResult = false;

	// If the bit's set, add over the max health
	if ( bitsDamageType & DMG_IGNORE_MAXHEALTH )
	{
		int iTimeBasedDamage = g_pGameRules->Damage_GetTimeBased();
		m_bitsDamageType &= ~(bitsDamageType & ~iTimeBasedDamage);
		m_iHealth += flHealth;
		bResult = true;
	}
	else
	{
		float flHealthToAdd = flHealth;
		float flMaxHealth = GetPlayerClass()->GetMaxHealth();
		
		// don't want to add more than we're allowed to have
		if ( flHealthToAdd > flMaxHealth - m_iHealth )
		{
			flHealthToAdd = flMaxHealth - m_iHealth;
		}

		if ( flHealthToAdd <= 0 )
		{
			bResult = false;
		}
		else
		{
			bResult = BaseClass::TakeHealth( flHealthToAdd, bitsDamageType );
		}
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::TFWeaponRemove( int iWeaponID )
{
	// find the weapon that matches the id and remove it
	int i;
	for (i = 0; i < WeaponCount(); i++) 
	{
		CTFWeaponBase *pWeapon = ( CTFWeaponBase *)GetWeapon( i );
		if ( !pWeapon )
			continue;

		if ( pWeapon->GetWeaponID() != iWeaponID )
			continue;

		RemovePlayerItem( pWeapon );
		UTIL_Remove( pWeapon );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::DropCurrentWeapon( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DropFlag( bool bSilent /* = false */ )
{
	if ( HasItem() )
	{
		CCaptureFlag *pFlag = dynamic_cast<CCaptureFlag*>( GetItem() );
		if ( pFlag )
		{
			int nFlagTeamNumber = pFlag->GetTeamNumber();
			pFlag->Drop( this, true, true, !bSilent );
			IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
			if ( event )
			{
				event->SetInt( "player", entindex() );
				event->SetInt( "eventtype", TF_FLAGEVENT_DROPPED );
				event->SetInt( "priority", 8 );
				event->SetInt( "team", nFlagTeamNumber );

				gameeventmanager->FireEvent( event );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
EHANDLE CTFPlayer::TeamFortress_GetDisguiseTarget( int nTeam, int nClass )
{
	if ( /*nTeam == GetTeamNumber() ||*/ nTeam == TF_SPY_UNDEFINED )
	{
		// we're not disguised as the enemy team
		return NULL;
	}

	CUtlVector<int> potentialTargets;

	CBaseEntity *pLastTarget = m_Shared.GetDisguiseTarget(); // don't redisguise self as this person
	
	// Find a player on the team the spy is disguised as to pretend to be
	CTFPlayer *pPlayer = NULL;

	// Loop through players and attempt to find a player as the team/class we're disguising as
	int i;
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && ( pPlayer != pLastTarget ) )
		{
			// First, try to find a player with the same color AND skin
			if ( ( pPlayer->GetTeamNumber() == nTeam ) && ( pPlayer->GetPlayerClass()->GetClassIndex() == nClass ) )
			{
				potentialTargets.AddToHead( i );
			}
		}
	}

	// do we have any potential targets in the list?
	if ( potentialTargets.Count() > 0 )
	{
		int iIndex = random->RandomInt( 0, potentialTargets.Count() - 1 );
		return UTIL_PlayerByIndex( potentialTargets[iIndex] );
	}

	// we didn't find someone with the class, so just find someone with the same team color
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && ( pPlayer->GetTeamNumber() == nTeam ) )
		{
			potentialTargets.AddToHead( i );
		}
	}

	if ( potentialTargets.Count() > 0 )
	{
		int iIndex = random->RandomInt( 0, potentialTargets.Count() - 1 );
		return UTIL_PlayerByIndex( potentialTargets[iIndex] );
	}

	// we didn't find anyone
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float DamageForce( const Vector &size, float damage, float scale )
{ 
	float force = damage * ((48 * 48 * 82.0) / (size.x * size.y * size.z)) * scale;
	
	if ( force > 1000.0 )
	{
		force = 1000.0;
	}

	return force;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SetBlastJumpState( int iState, bool bPlaySound /*= false*/ )
{
	m_iBlastJumpState |= iState;

	const char *pszEvent = NULL;
	if ( iState == TF_PLAYER_STICKY_JUMPED )
	{
		pszEvent = "sticky_jump";
	}
	else if ( iState == TF_PLAYER_ROCKET_JUMPED )
	{
		pszEvent = "rocket_jump";
	}

	if ( pszEvent )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( pszEvent );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetBool( "playsound", bPlaySound );
			gameeventmanager->FireEvent( event );
		}
	}

	m_Shared.AddCond( TF_COND_BLASTJUMPING );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ClearBlastJumpState( void )
{
	m_bCreatedRocketJumpParticles = false;
	m_iBlastJumpState = 0;
	m_flBlastJumpLandTime = gpGlobals->curtime;
	m_Shared.RemoveCond( TF_COND_BLASTJUMPING );
}

// we want to ship this...do not remove
ConVar tf_debug_damage( "tf_debug_damage", "0", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayer::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	//bool bIsObject = info.GetInflictor() && info.GetInflictor()->IsBaseObject(); 

	// damage may not come from a weapon (ie: Bosses, etc)
	// The existing code below already checked for NULL pWeapon, anyways
	CTFWeaponBase *pWeapon = dynamic_cast< CTFWeaponBase * >( inputInfo.GetWeapon() );

	if ( GetFlags() & FL_GODMODE )
		return 0;

	if ( IsInCommentaryMode() )
		return 0;

	bool bBuddha = ( m_debugOverlays & OVERLAY_BUDDHA_MODE ) ? true : false;

	if ( bBuddha )
	{
		if ( ( m_iHealth - info.GetDamage() ) <= 0 )
		{
			m_iHealth = 1;
			return 0;
		}
	}

	if ( !IsAlive() )
		return 0;

	// Early out if there's no damage
	if ( !info.GetDamage() )
		return 0;

	// Ghosts dont take damage
	if ( m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) )
	{
		return 0;
	}

	CBaseEntity *pAttacker = info.GetAttacker();
	CTFPlayer *pTFAttacker = ToTFPlayer( pAttacker );

	bool bDebug = tf_debug_damage.GetBool();

	// Make sure the player can take damage from the attacking entity
	if ( !g_pGameRules->FPlayerCanTakeDamage( this, pAttacker, info ) )
	{
		if ( bDebug )
		{
			Warning( "    ABORTED: Player can't take damage from that attacker.\n" );
		}

		return 0;
	}

	m_iHealthBefore = GetHealth();

	bool bIsSoldierRocketJumping = ( IsPlayerClass( TF_CLASS_SOLDIER ) && (pAttacker == this) && !(GetFlags() & FL_ONGROUND) && !(GetFlags() & FL_INWATER)) && (inputInfo.GetDamageType() & DMG_BLAST);
	bool bIsDemomanPipeJumping = ( IsPlayerClass( TF_CLASS_DEMOMAN) && (pAttacker == this) && !(GetFlags() & FL_ONGROUND) && !(GetFlags() & FL_INWATER)) && (inputInfo.GetDamageType() & DMG_BLAST);
	
	if ( bDebug )
	{
		Warning( "%s taking damage from %s, via %s. Damage: %.2f\n", GetDebugName(), info.GetInflictor() ? info.GetInflictor()->GetDebugName() : "Unknown Inflictor", pAttacker ? pAttacker->GetDebugName() : "Unknown Attacker", info.GetDamage() );
	}

	if ( IsDODClass() )
	{
		// Do special stun damage effect
		if ( info.GetDamageType() & DMG_STUN )
		{
			OnDamageByStun( info );
			return 0;
		}
	}

	// Ignore damagers on our team, to prevent capturing rocket jumping, etc.
	if ( pAttacker && pAttacker->GetTeam() != GetTeam() )
	{
		m_AchievementData.AddDamagerToHistory( pAttacker );
		if ( pAttacker->IsPlayer() )
		{
			ToTFPlayer( pAttacker )->m_AchievementData.AddTargetToHistory( this );

			// add to list of damagers via sentry so that later we can check for achievement: ACHIEVEMENT_TF_ENGINEER_SHOTGUN_KILL_PREV_SENTRY_TARGET
			CBaseEntity *pInflictor = info.GetInflictor();
			CObjectSentrygun *pSentry = dynamic_cast< CObjectSentrygun * >( pInflictor );
			if ( pSentry )
			{
				m_AchievementData.AddSentryDamager( pAttacker, pInflictor );
			}
		}
	}

	// keep track of amount of damage last sustained
	m_lastDamageAmount = info.GetDamage();
	m_LastDamageType = info.GetDamageType();

	if ( bIsSoldierRocketJumping || bIsDemomanPipeJumping )
	{
		int nJumpType = 0;

		// If this is our own rocket, scale down the damage if we're rocket jumping
		if ( bIsSoldierRocketJumping ) 
		{
			float flDamage = info.GetDamage() * tf_damagescale_self_soldier.GetFloat();
			info.SetDamage( flDamage );

			if ( m_iHealthBefore - flDamage > 0 )
			{
				nJumpType = TF_PLAYER_ROCKET_JUMPED;
			}
		}
		else if ( bIsDemomanPipeJumping )
		{
			nJumpType = TF_PLAYER_STICKY_JUMPED;
		}

		if ( nJumpType )
		{
			bool bPlaySound = false;
			if ( pWeapon )
			{
				int iNoBlastDamage = 0;
//				CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iNoBlastDamage, no_self_blast_dmg )
					bPlaySound = iNoBlastDamage ? true : false;
			}

			SetBlastJumpState( nJumpType, bPlaySound );
		}
	}

	// Save damage force for ragdolls.
	m_vecTotalBulletForce = info.GetDamageForce();
	m_vecTotalBulletForce.x = clamp( m_vecTotalBulletForce.x, -15000.0f, 15000.0f );
	m_vecTotalBulletForce.y = clamp( m_vecTotalBulletForce.y, -15000.0f, 15000.0f );
	m_vecTotalBulletForce.z = clamp( m_vecTotalBulletForce.z, -15000.0f, 15000.0f );

	int bTookDamage = 0;
 	int bitsDamage = inputInfo.GetDamageType();

	bool bAllowDamage = false;

	// check to see if our attacker is a trigger_hurt entity (and allow it to kill us even if we're invuln)
	if ( pAttacker && pAttacker->IsSolidFlagSet( FSOLID_TRIGGER ) )
	{
		CTriggerHurt *pTrigger = dynamic_cast<CTriggerHurt *>( pAttacker );
		if ( pTrigger )
		{
			bAllowDamage = true;
			info.SetDamageCustom( TF_DMG_CUSTOM_TRIGGER_HURT );
		}
	}
	else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_TELEFRAG )
	{
		bAllowDamage = true;
	}

	if ( !TFGameRules()->ApplyOnDamageModifyRules( info, this, bAllowDamage ) )
	{
		return 0;
	}

	bool bFatal = ( m_iHealth - info.GetDamage() ) <= 0;

	bool bTrackEvent = pTFAttacker && pTFAttacker != this && !pTFAttacker->IsBot() && !IsBot();
	if ( bTrackEvent )
	{
		float flHealthRemoved = bFatal ? m_iHealth : info.GetDamage();
		if ( info.GetDamageBonus() && info.GetDamageBonusProvider() )
		{
			// Don't deal with raw damage numbers, only health removed.
			// Example based on a crit rocket to a player with 120 hp:
			// Actual damage is 120, but potential damage is 300, where
			// 100 is the base, and 200 is the bonus.  Apply this ratio
			// to actual (so, attacker did 40, and provider added 80).
			float flBonusMult = info.GetDamage() / abs( info.GetDamageBonus() - info.GetDamage() );
			float flBonus = flHealthRemoved - ( flHealthRemoved / flBonusMult );
			m_AchievementData.AddDamageEventToHistory( info.GetDamageBonusProvider(), flBonus );
			flHealthRemoved -= flBonus;
		}
		m_AchievementData.AddDamageEventToHistory( pAttacker, flHealthRemoved );
	}

	// This should kill us
	if ( bFatal )
	{
		// Damage could have been modified since we started
		// Try to prevent death with buddha one more time
		if ( bBuddha )
		{
			m_iHealth = 1;
			return 0;
		}

		// Check to see if we have the cheat death attribute that makes
		// us teleport to base rather than die
		float flCheatDeathChance = 0.f;
		//CALL_ATTRIB_HOOK_FLOAT( flCheatDeathChance, teleport_instead_of_die );
		if( RandomFloat() < flCheatDeathChance )
		{
			// Send back to base
			ForceRespawn();

			m_iHealth = 1;
			return 0;
		}

		// Avoid one death
		if ( m_Shared.InCond( TF_COND_PREVENT_DEATH ) )
		{
			m_Shared.RemoveCond( TF_COND_PREVENT_DEATH );
			m_iHealth = 1;
			return 0;
		}

		// Powerup-sourced reflected damage should not kill player
		if ( info.GetDamageCustom() == TF_DMG_CUSTOM_RUNE_REFLECT )
		{
			m_iHealth = 1;
			return 0;
		}
	}

	// NOTE: Deliberately skip base player OnTakeDamage, because we don't want all the stuff it does re: suit voice
	bTookDamage = CBaseCombatCharacter::OnTakeDamage( info );

	// Early out if the base class took no damage
	if ( !bTookDamage )
	{
		if ( bDebug )
		{
			Warning( "    ABORTED: Player failed to take the damage.\n" );
		}
		return 0;
	}

	if ( bTookDamage == false )
		return 0;

	if ( bDebug )
	{
		Warning( "    DEALT: Player took %.2f damage.\n", info.GetDamage() );
		Warning( "    HEALTH LEFT: %d\n", GetHealth() );
	}

	// Some weapons have the ability to impart extra moment just because they feel like it. Let their attributes
	// do so if they're in the mood.
	if ( pWeapon != NULL )
	{
		float flZScale = 0.0f;
		//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flZScale, apply_z_velocity_on_damage );
		if ( flZScale != 0.0f )
		{
			ApplyAbsVelocityImpulse( Vector( 0.0f, 0.0f, flZScale ) );
		}

		float flDirScale = 0.0f;
		//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flDirScale, apply_look_velocity_on_damage );
		if ( flDirScale != 0.0f && pAttacker != NULL )
		{
			Vector vecForward;
			AngleVectors( pAttacker->EyeAngles(), &vecForward );

			Vector vecForwardNoDownward = Vector( vecForward.x, vecForward.y, MIN( 0.0f, vecForward.z ) ).Normalized();
			ApplyAbsVelocityImpulse( vecForwardNoDownward * flDirScale );
		}
	}

	// Send the damage message to the client for the hud damage indicator
	// Try and figure out where the damage is coming from
	Vector vecDamageOrigin = info.GetReportedPosition();

	// If we didn't get an origin to use, try using the attacker's origin
	if ( vecDamageOrigin == vec3_origin && info.GetInflictor() )
	{
		vecDamageOrigin = info.GetInflictor()->GetAbsOrigin();
	}

	// Tell the player's client that he's been hurt.
	if ( m_iHealthBefore != GetHealth() )
	{
 		CSingleUserRecipientFilter user( this );
		UserMessageBegin( user, "Damage" );
			WRITE_SHORT( clamp( (int)info.GetDamage(), 0, 32000 ) );
			WRITE_LONG( info.GetDamageType() );
			// Tell the client whether they should show it in the indicator
			if ( bitsDamage != DMG_GENERIC && !(bitsDamage & (DMG_DROWN | DMG_FALL | DMG_BURN) ) )
			{
				WRITE_BOOL( true );
				WRITE_VEC3COORD( vecDamageOrigin );
			}
			else
			{
				WRITE_BOOL( false );
			}
		MessageEnd();
	}

	// add to the damage total for clients, which will be sent as a single
	// message at the end of the frame
	// todo: remove after combining shotgun blasts?
	if ( info.GetInflictor() && info.GetInflictor()->edict() )
	{
		m_DmgOrigin = info.GetInflictor()->GetAbsOrigin();
	}

	m_DmgTake += (int)info.GetDamage();

	// Reset damage time countdown for each type of time based damage player just sustained
	for (int i = 0; i < CDMG_TIMEBASED; i++)
	{
		// Make sure the damage type is really time-based.
		// This is kind of hacky but necessary until we setup DamageType as an enum.
		int iDamage = ( DMG_PARALYZE << i );
		if ( ( info.GetDamageType() & iDamage ) && g_pGameRules->Damage_IsTimeBased( iDamage ) )
		{
			m_rgbTimeBasedDamage[i] = 0;
		}
	}

	// Display any effect associate with this damage type
	DamageEffect( info.GetDamage(),bitsDamage );

	m_bitsDamageType |= bitsDamage; // Save this so we can report it to the client
	m_bitsHUDDamage = -1;  // make sure the damage bits get reset

	// Flinch
	bool bFlinch = true;

	// DOD Classes do not get flinch
	if ( IsDODClass() )
		bFlinch = false;

	if ( bitsDamage != DMG_GENERIC )
	{
		if ( IsPlayerClass( TF_CLASS_SNIPER ) && m_Shared.InCond( TF_COND_AIMING ) )
		{
			if ( pTFAttacker && pWeapon && pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN )
			{
				float flDistSqr = ( pTFAttacker->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
				if ( flDistSqr > 750 * 750 )
				{
					bFlinch = false;
				}
			}
		}

		if ( bFlinch )
		{
			if ( ApplyPunchImpulseX( -2 ) ) 
			{
				PlayFlinch( info );
			}
		}
	}

	// Do special explosion damage effect
	if ( bitsDamage & DMG_BLAST )
	{
		if ( IsDODClass() )
			OnDamagedByExplosionDOD( info );
		else
			OnDamagedByExplosion( info );
	}

	if ( m_iHealthBefore != GetHealth() )
	{
		PainSound( info );
	}

	// Detect drops below 25% health and restart expression, so that characters look worried.
	int iHealthBoundary = (GetMaxHealth() * 0.25);
	if ( GetHealth() <= iHealthBoundary && m_iHealthBefore > iHealthBoundary )
	{
		ClearExpression();
	}

#ifdef _DEBUG
	// Report damage from the info in debug so damage against targetdummies goes
	// through the system, as m_iHealthBefore - GetHealth() will always be 0.
	CTF_GameStats.Event_PlayerDamage( this, info, info.GetDamage() );
#else
	CTF_GameStats.Event_PlayerDamage( this, info, m_iHealthBefore - GetHealth() );
#endif // _DEBUG

	// if we take damage after we leave the ground, update the health if its less
	if ( bTookDamage && m_iLeftGroundHealth > 0 )
	{
		if ( GetHealth() < m_iLeftGroundHealth )
		{
			m_iLeftGroundHealth = GetHealth();
		}
	}

	if ( IsPlayerClass( TF_CLASS_SPY ) && !( info.GetDamageType() & DMG_FALL ) )
	{
		m_Shared.NoteLastDamageTime( m_lastDamageAmount );
	}

	// Let attacker react to the damage they dealt
	if ( pTFAttacker )
	{
		pTFAttacker->OnDealtDamage( this, info );
	}

	return info.GetDamage();
}

//-----------------------------------------------------------------------------
// Purpose: Invoked when we deal damage to another victim
//-----------------------------------------------------------------------------
void CTFPlayer::OnDealtDamage( CBaseCombatCharacter *pVictim, const CTakeDamageInfo &info )
{
	if ( pVictim )
	{
		// which second of the window are we in
		int i = (int)gpGlobals->curtime;
		i %= DPS_Period;

		if ( i != m_lastDamageRateIndex )
		{
			// a second has ticked over, start a new accumulation
			m_damageRateArray[ i ] = info.GetDamage();
			m_lastDamageRateIndex = i;

			// track peak DPS for this player
			m_peakDamagePerSecond = 0;
			for( i=0; i<DPS_Period; ++i )
			{
				if ( m_damageRateArray[i] > m_peakDamagePerSecond )
				{
					m_peakDamagePerSecond = m_damageRateArray[i];
				}
			}
		}
		else
		{
			m_damageRateArray[ i ] += info.GetDamage();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DamageEffect(float flDamage, int fDamageType)
{
	bool bDisguised = m_Shared.InCond( TF_COND_DISGUISED );

	if (fDamageType & DMG_CRUSH)
	{
		//Red damage indicator
		color32 red = {128,0,0,128};
		UTIL_ScreenFade( this, red, 1.0f, 0.1f, FFADE_IN );
	}
	else if (fDamageType & DMG_DROWN)
	{
		//Red damage indicator
		color32 blue = {0,0,128,128};
		UTIL_ScreenFade( this, blue, 1.0f, 0.1f, FFADE_IN );
	}
	else if (fDamageType & DMG_SLASH)
	{
		if ( !bDisguised )
		{
			// If slash damage shoot some blood
			SpawnBlood(EyePosition(), g_vecAttackDir, BloodColor(), flDamage);
		}
	}
	else if ( fDamageType & DMG_BULLET )
	{
		if ( !bDisguised )
		{
			EmitSound( "Flesh.BulletImpact" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : collisionGroup - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	if ( ( ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT ) && tf_avoidteammates.GetBool() ) ||
		collisionGroup == TFCOLLISION_GROUP_ROCKETS )
	{
		switch( GetTeamNumber() )
		{
		case TF_TEAM_RED:
			if ( !( contentsMask & CONTENTS_REDTEAM ) )
				return false;
			break;

		case TF_TEAM_BLUE:
			if ( !( contentsMask & CONTENTS_BLUETEAM ) )
				return false;
			break;
		}
	}
	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

//---------------------------------------
// Is the player the passed player class?
//---------------------------------------
bool CTFPlayer::IsPlayerClass( int iClass ) const
{
	const CTFPlayerClass *pClass = &m_PlayerClass;

	if ( !pClass )
		return false;

	return ( pClass->IsClass( iClass ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::CommitSuicide( bool bExplode /* = false */, bool bForce /*= false*/ )
{
	// Don't suicide if we haven't picked a class for the first time, or we're not in active state
	if ( IsPlayerClass( TF_CLASS_UNDEFINED ) || !m_Shared.InState( TF_STATE_ACTIVE ) )
		return;

	// Don't suicide during the "bonus time" if we're not on the winning team
	if ( !bForce && TFGameRules()->State_Get() == GR_STATE_TEAM_WIN && 
		 GetTeamNumber() != TFGameRules()->GetWinningTeam() )
	{
		return;
	}
	
	m_iSuicideCustomKillFlags = TF_DMG_CUSTOM_SUICIDE;

	BaseClass::CommitSuicide( bExplode, bForce );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : int
//-----------------------------------------------------------------------------
ConVar tf_preround_push_from_damage_enable( "tf_preround_push_from_damage_enable", "0", FCVAR_NONE, "If enabled, this will allow players using certain type of damage to move during pre-round freeze time." );
void CTFPlayer::ApplyPushFromDamage( const CTakeDamageInfo &info, Vector vecDir )
{
	// check if player can be moved
	if ( !tf_preround_push_from_damage_enable.GetBool() && !CanPlayerMove() )
		return;

	if ( m_bIsTargetDummy )
		return;

	Vector vecForce;
	vecForce.Init();
	if ( info.GetAttacker() == this )
	{
		Vector vecSize = WorldAlignSize();
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;

		if ( vecSize == hullSizeCrouch )
		{
			// Use the original hull for damage force calculation to ensure our RJ height doesn't change due to crouch hull increase
			// ^^ Comment above is an ancient lie, Ducking actually increases blast force, this value increases it even more 82 standing, 62 ducking, 55 modified
			vecSize.z = 55;
		}

		float flDamageForForce = info.GetDamageForForceCalc() ? info.GetDamageForForceCalc() : info.GetDamage();

		float flSelfPushMult = 1.0;
		//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( info.GetWeapon(), flSelfPushMult, mult_dmgself_push_force );

		
		if ( IsPlayerClass( TF_CLASS_SOLDIER ) )
		{
			// Rocket Jump
			if ( (info.GetDamageType() & DMG_BLAST) )
			{
				if ( GetFlags() & FL_ONGROUND )
				{
					vecForce = vecDir * -DamageForce( vecSize, flDamageForForce, tf_damageforcescale_self_soldier_badrj.GetFloat() ) * flSelfPushMult;
				}
				else
				{
					vecForce = vecDir * -DamageForce( vecSize, flDamageForForce, tf_damageforcescale_self_soldier_rj.GetFloat() ) * flSelfPushMult;
				}

				SetBlastJumpState( TF_PLAYER_ROCKET_JUMPED );

				// Reset duck in air on self rocket impulse.
				m_Shared.SetAirDucked( 0 );
			}
			else
			{
				// Self Damage no force
				vecForce.Zero();
			}
			
		}
		else
		{
			// Detonator blast jump modifier
			if ( IsPlayerClass( TF_CLASS_PYRO ) && info.GetDamageCustom() == TF_DMG_CUSTOM_FLARE_EXPLOSION )
			{
				vecForce = vecDir * -DamageForce( vecSize, flDamageForForce, tf_damageforcescale_pyro_jump.GetFloat() ) * flSelfPushMult;
			}
			else
			{
				// Other Jumps (Stickies)
				vecForce = vecDir * -DamageForce( vecSize, flDamageForForce, DAMAGE_FORCE_SCALE_SELF ) * flSelfPushMult;
			}

			// Reset duck in air on self grenade impulse.
			m_Shared.SetAirDucked( 0 );
		}
	}
	else
	{
		// Don't let bot get pushed while they're in spawn area
		if ( m_Shared.InCond( TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED ) )
		{
			return;
		}

		// Sentryguns push a lot harder
		if ( (info.GetDamageType() & DMG_BULLET) && info.GetInflictor() && info.GetInflictor()->IsBaseObject() )
		{
			float flSentryPushMultiplier = 16.f;
			CObjectSentrygun* pSentry = dynamic_cast<CObjectSentrygun*>( info.GetInflictor() );
			if ( pSentry )
			{
				// SEALTODO
				//flSentryPushMultiplier = pSentry->GetPushMultiplier();
				flSentryPushMultiplier = 16.f;

				// Scale the force based on Distance, Wrangled Sentries should not push so hard at distance
				// get the distance between sentry and victim and lower push force if outside of attack range (wrangled)
				float flDistSqr = (pSentry->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
				if ( flDistSqr > SENTRY_MAX_RANGE_SQRD )
				{
					flSentryPushMultiplier *= 0.5f;
				}
			}
			vecForce = vecDir * -DamageForce( WorldAlignSize(), info.GetDamage(), flSentryPushMultiplier );
		}
		else
		{
//			CTFWeaponBase *pWeapon = dynamic_cast<CTFWeaponBase*>(info.GetWeapon());
			if ( info.GetDamageCustom() == TF_DMG_CUSTOM_PLASMA_CHARGED )
			{
				vecForce = vecDir * -DamageForce( WorldAlignSize(), info.GetDamage(), tf_damageforcescale_other.GetFloat() ) * 1.25f;
			}
			else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_KART )
			{
				vecForce = info.GetDamageForce();
			}
			else
			{
				vecForce = vecDir * -DamageForce( WorldAlignSize(), info.GetDamage(), tf_damageforcescale_other.GetFloat() );
			}

			if ( IsPlayerClass( TF_CLASS_HEAVYWEAPONS ) )
			{
				// Heavies take less push from non sentryguns
				vecForce *= 0.5;
			}

//			CBaseEntity* pInflictor = info.GetInflictor();

			int iAirBlast = 0;
			//CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iAirBlast, damage_causes_airblast );
			if ( iAirBlast )
			{
				float force = -DamageForce( WorldAlignSize(), 100, 6 );
				ApplyAirBlastImpulse( force * vecDir );
				vecForce.Zero();
			}
		}

//		bool bBigKnockback = false;

		CTFPlayer *pAttacker = ToTFPlayer( info.GetAttacker() );

		// Airblast effect for general attacks.  Scaled by range.
		float flImpactBlastForce = 1.f;
		//CALL_ATTRIB_HOOK_INT_ON_OTHER( info.GetWeapon(), flImpactBlastForce, damage_blast_push );
		if ( flImpactBlastForce != 1.f )
		{
			CBaseEntity *pInflictor = info.GetInflictor();
			if ( pInflictor )
			{
				const float flMaxPushBackDistSqr = 700.f * 700.f;
				float flDistSqr = ( WorldSpaceCenter() - pInflictor->WorldSpaceCenter() ).LengthSqr();
				if ( flDistSqr <= flMaxPushBackDistSqr )
				{
					if ( vecForce.z < 0 )
					{
						vecForce.z = 0;
					}

					m_Shared.StunPlayer( 0.3f, 1.f, TF_STUN_MOVEMENT | TF_STUN_MOVEMENT_FORWARD_ONLY, pAttacker );
					flImpactBlastForce = RemapValClamped( flDistSqr, 1000.f, flMaxPushBackDistSqr, flImpactBlastForce, ( flImpactBlastForce * 0.5f ) );
					float flForce = -DamageForce( WorldAlignSize(), info.GetDamage() * 2, flImpactBlastForce );
					ApplyAirBlastImpulse( flForce * vecDir );
				}
			}
		}

		float flDamageForceReduction = 1.f;
		//CALL_ATTRIB_HOOK_FLOAT( flDamageForceReduction, damage_force_reduction );
		vecForce *= flDamageForceReduction;
	}

	ApplyAbsVelocityImpulse( vecForce );

	// If we were pushed by an enemy explosion, we're now marked as being blasted by an enemy.
	// If we stay on the ground, next frame our player think will remove this flag.
	if ( info.GetAttacker() != this && info.GetDamageType() & DMG_BLAST )
	{
		m_bTakenBlastDamageSinceLastMovement = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PlayDamageResistSound( float flStartDamage, float flModifiedDamage )
{
	if ( flStartDamage <= 0.f )
		return;

	// Spam control
	if ( gpGlobals->curtime - m_flLastDamageResistSoundTime <= 0.1f )
		return;

	// Play an absorb sound based on the percentage the damage has been reduced to
	float flDamagePercent = flModifiedDamage / flStartDamage;
	if ( flDamagePercent > 0.f && flDamagePercent < 1.f )
	{
		const char *pszSoundName = ( flDamagePercent >= 0.75f ) ? "Player.ResistanceLight" :
								   ( flDamagePercent <= 0.25f ) ? "Player.ResistanceHeavy" : "Player.ResistanceMedium";

		CSoundParameters params;
		if ( CBaseEntity::GetParametersForSound( pszSoundName, params, NULL ) )
		{
			CPASAttenuationFilter filter( GetAbsOrigin(), params.soundlevel );
			EmitSound_t ep( params );
			ep.m_flVolume *= RemapValClamped( flStartDamage, 1.f, 70.f, 0.7f, 1.f );
			EmitSound( filter, entindex(), ep );
			m_flLastDamageResistSoundTime = gpGlobals->curtime;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : int
//-----------------------------------------------------------------------------
int CTFPlayer::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// Always NULL check this below
	CTFPlayer *pTFAttacker = ToTFPlayer( info.GetAttacker() );
	
	CTFGameRules::DamageModifyExtras_t outParams;
	outParams.bIgniting = false;
	outParams.bSelfBlastDmg = false;
	outParams.bSendPreFeignDamage = false;
	outParams.bPlayDamageReductionSound = false;
	float realDamage = info.GetDamage();
	if ( TFGameRules() )
	{
		realDamage = TFGameRules()->ApplyOnDamageAliveModifyRules( info, this, outParams );

		if ( realDamage == -1 )
		{
			// Hard out requested from ApplyOnDamageModifyRules 
			return 0;
		}
	}

	if ( outParams.bPlayDamageReductionSound )
	{
		PlayDamageResistSound( info.GetDamage(), realDamage );
	}

	// Grab the vector of the incoming attack. 
	// (Pretend that the inflictor is a little lower than it really is, so the body will tend to fly upward a bit).
	Vector vecDir = vec3_origin;
	if ( info.GetInflictor() )
	{
		vecDir = info.GetInflictor()->WorldSpaceCenter() - Vector ( 0.0f, 0.0f, 10.0f ) - WorldSpaceCenter();
		info.GetInflictor()->AdjustDamageDirection( info, vecDir, this );
		VectorNormalize( vecDir );
	}
	g_vecAttackDir = vecDir;

	// Do the damage.
	m_bitsDamageType |= info.GetDamageType();

	float flBleedingTime = 0.0f;
	int iPrevHealth = m_iHealth;

	if ( m_takedamage != DAMAGE_EVENTS_ONLY )
	{
		// Take damage - round to the nearest integer.
		int iOldHealth = m_iHealth;
		m_iHealth -= ( realDamage + 0.5f );

		if ( IsHeadshot( info.GetDamageCustom() ) && (m_iHealth <= 0) && (iOldHealth != 1) )
		{
			int iNoDeathFromHeadshots = 0;
			if ( iNoDeathFromHeadshots == 1 )
			{
				m_iHealth = 1;
			}
		}

		// For lifeleech, calculate how much damage we actually inflicted.
		CTFPlayer *pAttackingPlayer = dynamic_cast<CTFPlayer *>( info.GetAttacker() );
		if ( pAttackingPlayer && pAttackingPlayer->GetActiveWeapon() )
		{
			float fLifeleechOnDamage = 0.0f;
			if ( fLifeleechOnDamage > 0.0f )
			{
				const float fActualDamageDealt = iOldHealth - m_iHealth;
				const float fHealAmount = fActualDamageDealt * fLifeleechOnDamage;

				if ( fHealAmount >= 0.5f )
				{
					const int iHealthToAdd = MIN( (int)(fHealAmount + 0.5f), pAttackingPlayer->m_Shared.GetMaxBuffedHealth() - pAttackingPlayer->GetHealth() );
					pAttackingPlayer->TakeHealth( iHealthToAdd, DMG_GENERIC );
				}
			}
		}

		// track accumulated sentry gun damage dealt by players
		if ( pTFAttacker )
		{
			// track amount of damage dealt by defender's sentry guns
			CObjectSentrygun *sentry = dynamic_cast< CObjectSentrygun * >( info.GetInflictor() );
			CTFProjectile_SentryRocket *sentryRocket = dynamic_cast< CTFProjectile_SentryRocket * >( info.GetInflictor() );

			if ( ( sentry && !sentry->IsDisposableBuilding() ) || sentryRocket )
			{
				int flooredHealth = clamp( m_iHealth, 0, m_iHealth );

				pTFAttacker->AccumulateSentryGunDamageDealt( iOldHealth - flooredHealth );
			}
		}
	}

	m_flLastDamageTime = gpGlobals->curtime;

	// Apply a damage force.
	CBaseEntity *pAttacker = info.GetAttacker();
	if ( !pAttacker )
		return 0;

	// DOD characters take no push from damage, they are very strong
	if ( ( info.GetDamageType() & DMG_PREVENT_PHYSICS_FORCE ) == 0 && !IsDODClass() )
	{
		if ( info.GetInflictor() && ( GetMoveType() == MOVETYPE_WALK ) && 
		   ( !pAttacker->IsSolidFlagSet( FSOLID_TRIGGER ) ) && 
		   ( !m_Shared.InCond( TF_COND_DISGUISED ) ) )	
		{
			if ( !m_Shared.InCond( TF_COND_MEGAHEAL ) || outParams.bSelfBlastDmg )
			{
				ApplyPushFromDamage( info, vecDir );
			}
		}
	}

	if ( outParams.bIgniting && pTFAttacker )
	{
		m_Shared.Burn( pTFAttacker );
	}

	if ( flBleedingTime > 0 && pTFAttacker )
	{
		//m_Shared.MakeBleed( pTFAttacker, dynamic_cast< CTFWeaponBase * >( info.GetWeapon() ), flBleedingTime );
	}

	// Fire a global game event - "player_hurt"
	IGameEvent * event = gameeventmanager->CreateEvent( "player_hurt" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetInt( "health", MAX( 0, m_iHealth ) );

		// DOD
		event->SetInt("hitgroup", m_LastHitGroup );

		// HLTV event priority, not transmitted
		event->SetInt( "priority", 5 );	

		int iDamageAmount = ( iPrevHealth - m_iHealth );
		event->SetInt( "damageamount", iDamageAmount );

		// Hurt by another player.
		if ( pAttacker->IsPlayer() )
		{
			CBasePlayer *pPlayer = ToBasePlayer( pAttacker );
			event->SetInt( "attacker", pPlayer->GetUserID() );
			
			event->SetInt( "custom", info.GetDamageCustom() );
			event->SetBool( "showdisguisedcrit", m_bShowDisguisedCrit );
			event->SetBool( "crit", (info.GetDamageType() & DMG_CRITICAL) != 0 );
			event->SetBool( "minicrit", m_bMiniCrit );
			event->SetBool( "allseecrit", m_bAllSeeCrit );
			Assert( (int)m_eBonusAttackEffect < 256 );
			event->SetInt( "bonuseffect", (int)m_eBonusAttackEffect );

			if ( pTFAttacker && pTFAttacker->GetActivePCWeapon() )
			{
				event->SetInt( "weaponid", pTFAttacker->GetActivePCWeapon()->GetWeaponID() );
			}
		}
		// Hurt by world.
		else
		{
			event->SetInt( "attacker", 0 );
		}

        gameeventmanager->FireEvent( event );
	}
	
	if ( pTFAttacker && pTFAttacker != this )
	{
		pTFAttacker->RecordDamageEvent( info, (m_iHealth <= 0), iPrevHealth );
	}

	//No bleeding while invul or disguised.
	bool bBleed = ( ( m_Shared.InCond( TF_COND_DISGUISED ) == false || m_Shared.GetDisguiseTeam() != pAttacker->GetTeamNumber() )
					&& !m_Shared.IsInvulnerable() );

	// No bleed effects for DMG_GENERIC
	if ( info.GetDamageType() == 0 )
	{
		bBleed = false;
	}

	// Except if we are really bleeding!
	bBleed |= m_Shared.InCond( TF_COND_BLEEDING );

	if ( bBleed && pTFAttacker )
	{
		CPCWeaponBase *pWeapon = pTFAttacker->GetActivePCWeapon();
		if ( pWeapon && pWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER )
		{
			bBleed = false;
		}
	}

	if ( bBleed && ( realDamage > 0.f ) )
	{
		Vector vDamagePos = info.GetDamagePosition();

		if ( vDamagePos == vec3_origin )
		{
			vDamagePos = WorldSpaceCenter();
		}

		CPVSFilter filter( vDamagePos );
		TE_TFBlood( filter, 0.0, vDamagePos, -vecDir, entindex() );
	}

	if ( m_bIsTargetDummy )
	{
		// In the case of a targetdummy bot, restore any damage so it can never die
		TakeHealth( ( iPrevHealth - m_iHealth ), DMG_GENERIC );
	}

	// Done.
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Adds this damager to the history list of people who damaged player
//-----------------------------------------------------------------------------
void CAchievementData::AddDamagerToHistory( EHANDLE hDamager )
{
	if ( !hDamager )
		return;

	EntityHistory_t newHist;
	newHist.hEntity = hDamager;
	newHist.flTimeDamage = gpGlobals->curtime;
	aDamagers.InsertHistory( newHist );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not pDamager has damaged the player in the time specified
//-----------------------------------------------------------------------------
bool CAchievementData::IsDamagerInHistory( CBaseEntity *pDamager, float flTimeWindow )
{
	for ( int i = 0; i < aDamagers.Count(); i++ )
	{
		if ( ( gpGlobals->curtime - aDamagers[i].flTimeDamage ) > flTimeWindow )
			return false;

		if ( aDamagers[i].hEntity == pDamager )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of players who've damaged us in the time specified
//-----------------------------------------------------------------------------
int	CAchievementData::CountDamagersWithinTime( float flTime )
{
	int iCount = 0;
	for ( int i = 0; i < aDamagers.Count(); i++ )
	{
		if ( gpGlobals->curtime - aDamagers[i].flTimeDamage < flTime )
		{
			iCount++;
		}
	}

	return iCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAchievementData::AddTargetToHistory( EHANDLE hTarget )
{
	if ( !hTarget )
		return;

	EntityHistory_t newHist;
	newHist.hEntity = hTarget;
	newHist.flTimeDamage = gpGlobals->curtime;
	aTargets.InsertHistory( newHist );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAchievementData::IsTargetInHistory( CBaseEntity *pTarget, float flTimeWindow )
{
	for ( int i = 0; i < aTargets.Count(); i++ )
	{
		if ( ( gpGlobals->curtime - aTargets[i].flTimeDamage ) > flTimeWindow )
			return false;

		if ( aTargets[i].hEntity == pTarget )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAchievementData::CountTargetsWithinTime( float flTime )
{
	int iCount = 0;
	for ( int i = 0; i < aTargets.Count(); i++ )
	{
		if ( ( gpGlobals->curtime - aTargets[i].flTimeDamage ) < flTime )
		{
			iCount++;
		}
	}

	return iCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAchievementData::DumpDamagers( void )
{
	Msg("Damagers:\n");
	for ( int i = 0; i < aDamagers.Count(); i++ )
	{
		if ( aDamagers[i].hEntity )
		{
			if ( aDamagers[i].hEntity->IsPlayer() )
			{
				Msg("   %s : at %.2f (%.2f ago)\n", ToTFPlayer(aDamagers[i].hEntity)->GetPlayerName(), aDamagers[i].flTimeDamage, gpGlobals->curtime - aDamagers[i].flTimeDamage );
			}
			else
			{
				Msg("   %s : at %.2f (%.2f ago)\n", aDamagers[i].hEntity->GetDebugName(), aDamagers[i].flTimeDamage, gpGlobals->curtime - aDamagers[i].flTimeDamage );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds this attacker to the history of people who damaged this player
//-----------------------------------------------------------------------------
void CAchievementData::AddDamageEventToHistory( EHANDLE hAttacker, float flDmgAmount /*= 0.f*/ )
{
	if ( !hAttacker )
		return;

	EntityDamageHistory_t newHist;
	newHist.hEntity = hAttacker;
	newHist.flTimeDamage = gpGlobals->curtime;
	newHist.nDamageAmount = flDmgAmount;
	aDamageEvents.InsertHistory( newHist );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not pEntity has damaged the player in the time specified
//-----------------------------------------------------------------------------
bool CAchievementData::IsEntityInDamageEventHistory( CBaseEntity *pEntity, float flTimeWindow )
{
	for ( int i = 0; i < aDamageEvents.Count(); i++ )
	{
		if ( aDamageEvents[i].hEntity != pEntity )
			continue;

		// Sorted
		if ( ( gpGlobals->curtime - aDamageEvents[i].flTimeDamage ) > flTimeWindow )
			break;

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: The sum of damage events from pEntity
//-----------------------------------------------------------------------------
int CAchievementData::GetAmountForDamagerInEventHistory( CBaseEntity *pEntity, float flTimeWindow )
{
	int nAmount = 0;

	for ( int i = 0; i < aDamageEvents.Count(); i++ )
	{
		if ( aDamageEvents[i].hEntity != pEntity )
			continue;
		
		// Msg( "   %s : at %.2f (%.2f ago)\n", ToTFPlayer( aDamageEvents[i].hEntity )->GetPlayerName(), aDamageEvents[i].flTimeDamage, gpGlobals->curtime - aDamageEvents[i].flTimeDamage );

		// Sorted
		if ( ( gpGlobals->curtime - aDamageEvents[i].flTimeDamage ) > flTimeWindow )
			break;

		nAmount += aDamageEvents[i].nDamageAmount;
	}

	return nAmount;
}

//-----------------------------------------------------------------------------
// Purpose: Adds hPlayer to the history of people who pushed this player
//-----------------------------------------------------------------------------
void CAchievementData::AddPusherToHistory( EHANDLE hPlayer )
{
	if ( !hPlayer )
		return;

	EntityHistory_t newHist;
	newHist.hEntity = hPlayer;
	newHist.flTimeDamage = gpGlobals->curtime;
	aPushers.InsertHistory( newHist );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not pPlayer pushed the player in the time specified
//-----------------------------------------------------------------------------
bool CAchievementData::IsPusherInHistory( CBaseEntity *pPlayer, float flTimeWindow )
{
	for ( int i = 0; i < aPushers.Count(); i++ )
	{
		if ( ( gpGlobals->curtime - aPushers[i].flTimeDamage ) > flTimeWindow )
			return false;

		if ( aPushers[i].hEntity == pPlayer )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Adds this damager to the history list of people whose sentry damaged player
//-----------------------------------------------------------------------------
void CAchievementData::AddSentryDamager( EHANDLE hDamager, EHANDLE hObject )
{
	if ( !hDamager )
		return;

	EntityHistory_t newHist;
	newHist.hEntity = hDamager;
	newHist.hObject = hObject;
	newHist.flTimeDamage = gpGlobals->curtime;
	aSentryDamagers.InsertHistory( newHist );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not pDamager has damaged the player in the time specified (by way of sentry gun)
//-----------------------------------------------------------------------------
EntityHistory_t* CAchievementData::IsSentryDamagerInHistory( CBaseEntity *pDamager, float flTimeWindow )
{
	for ( int i = 0; i < aSentryDamagers.Count(); i++ )
	{
		if ( ( gpGlobals->curtime - aSentryDamagers[i].flTimeDamage ) > flTimeWindow )
			return NULL;

		if ( aSentryDamagers[i].hEntity == pDamager )
		{
			return &aSentryDamagers[i];
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldGib( const CTakeDamageInfo &info )
{
	// Check to see if we should allow players to gib.
	if ( !tf_playergib.GetBool() )
		return false;

	// Only TF2 classes can gib, this may change in the future
	if ( !IsTF2Class() )
		return false;

	// Only blast & half falloff damage can gib.
	if ( ( (info.GetDamageType() & DMG_BLAST) == 0 ) &&
		( (info.GetDamageType() & DMG_HALF_FALLOFF) == 0 ) )
		return false;

	// Explosive crits always gib.
	if ( info.GetDamageType() & DMG_CRITICAL )
		return true;

	// Hard hits also gib.
	if ( GetHealth() <= -10 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Figures out if there is a special assist responsible for our death.
// Must be called before conditions are cleared druing death.
//-----------------------------------------------------------------------------
void CTFPlayer::DetermineAssistForKill( const CTakeDamageInfo &info )
{
	CTFPlayer *pPlayerAttacker = ToTFPlayer( info.GetAttacker() );
	if ( !pPlayerAttacker )
		return;

	CTFPlayer *pPlayerAssist = NULL;

	if ( m_Shared.GetConditionAssistFromVictim() )
	{
		// If we are covered in urine, mad milk, etc, then give the provider an assist.
		pPlayerAssist = ToTFPlayer( m_Shared.GetConditionAssistFromVictim() );
	}

	if ( m_Shared.IsControlStunned() )
	{
		// If we've been stunned, the stunner gets credit for the assist.
		pPlayerAssist = m_Shared.GetStunner();
	}

	// Can't assist ourself.
	if ( pPlayerAttacker && (pPlayerAttacker != pPlayerAssist) )
	{
		m_Shared.SetAssist( pPlayerAssist );
	}
	else
	{
		m_Shared.SetAssist( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::Event_KilledOther( pVictim, info );

	if ( pVictim->IsPlayer() )
	{
		CTFPlayer *pTFVictim = ToTFPlayer(pVictim);

		// Custom death handlers
		// TODO: Need a system here!  This conditional is getting pretty big.
		const char *pszCustomDeath = "customdeath:none";
		if ( info.GetAttacker() && info.GetAttacker()->IsBaseObject() )
		{
			pszCustomDeath = "customdeath:sentrygun";
		}
		else if ( info.GetInflictor() && info.GetInflictor()->IsBaseObject() )
		{
			CBaseObject* pObj = dynamic_cast<CBaseObject*>( info.GetInflictor() );
			if ( pObj->IsMiniBuilding() )
			{
				pszCustomDeath = "customdeath:minisentrygun";
			}
			else
			{
				pszCustomDeath = "customdeath:sentrygun";
			}
		}
		else if ( IsHeadshot( info.GetDamageCustom() ) )
		{				
			pszCustomDeath = "customdeath:headshot";
		}
		else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_BACKSTAB )
		{
			pszCustomDeath = "customdeath:backstab";
		}
		else if ( info.GetDamageCustom() == TF_DMG_CUSTOM_BURNING )
		{
			pszCustomDeath = "customdeath:burning";
		}
		else if ( IsTauntDmg( info.GetDamageCustom() ) )
		{
			pszCustomDeath = "customdeath:taunt";
		}

		// Revenge handler
		const char *pszDomination = "domination:none";
		if ( pTFVictim->GetDeathFlags() & (TF_DEATH_REVENGE|TF_DEATH_ASSISTER_REVENGE) )
		{
			pszDomination = "domination:revenge";
		}
		else if ( pTFVictim->GetDeathFlags() & TF_DEATH_DOMINATION )
		{
			pszDomination = "domination:dominated";
		}

		const char *pszVictimStunned = "victimstunned:0";
		if ( pTFVictim->m_Shared.InCond( TF_COND_STUNNED ) )
		{
			pszVictimStunned = "victimstunned:1";
		}

		const char *pszVictimDoubleJumping = "victimdoublejumping:0";
		if ( pTFVictim->m_Shared.GetAirDash() > 0 )
		{
			pszVictimDoubleJumping = "victimdoublejumping:1";
		}

		CFmtStrN<128> modifiers( "%s,%s,%s,%s,victimclass:%s", pszCustomDeath, pszDomination, pszVictimStunned, pszVictimDoubleJumping, g_aPlayerClassNames_NonLocalized[ pTFVictim->GetPlayerClass()->GetClassIndex() ] );

		bool bPlayspeech = true;

		if ( bPlayspeech )
		{
			SpeakConceptIfAllowed( MP_CONCEPT_KILLED_PLAYER, modifiers );
		}

		// track accumulated sentry gun kills on owning player for Sentry Busters in MvM (so they can't clear this by rebuilding their sentry)
		CObjectSentrygun *sentry = dynamic_cast< CObjectSentrygun * >( info.GetInflictor() );
		CTFProjectile_SentryRocket *sentryRocket = dynamic_cast< CTFProjectile_SentryRocket * >( info.GetInflictor() );

		if ( ( sentry && !sentry->IsDisposableBuilding() ) || sentryRocket )
		{
			IncrementSentryGunKillCount();
		}
	}
	else
	{
		if ( pVictim->IsBaseObject() )
		{
			CBaseObject *pObject = dynamic_cast<CBaseObject *>( pVictim );
			SpeakConceptIfAllowed( MP_CONCEPT_KILLED_OBJECT, pObject->GetResponseRulesModifier() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Event_Killed( const CTakeDamageInfo &info )
{
	SpeakConceptIfAllowed( MP_CONCEPT_DIED );

	StateTransition( TF_STATE_DYING );	// Transition into the dying state.

	CTFPlayer *pPlayerAttacker = NULL;
	if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() )
	{
		pPlayerAttacker = ToTFPlayer( info.GetAttacker() );
	}

	bool bOnGround = GetFlags() & FL_ONGROUND;
	bool bElectrocuted = false;
	bool bDisguised = m_Shared.InCond( TF_COND_DISGUISED );
	// we want the rag doll to burn if the player was burning and was not a pryo (who only burns momentarily)
	bool bBurning = m_Shared.InCond( TF_COND_BURNING ) && ( TF_CLASS_PYRO != GetPlayerClass()->GetClassIndex() );

	if ( IsPlayerClass( TF_CLASS_MEDIC ) )
	{
		CWeaponMedigun* pMedigun = assert_cast<CWeaponMedigun*>( Weapon_OwnsThisID( TF_WEAPON_MEDIGUN ) );
		float flChargeLevel = pMedigun ? pMedigun->GetChargeLevel() : 0.f;
#if 0 // SEALTODO
		float flMinChargeLevel = pMedigun ? pMedigun->GetMinChargeAmount() : 1.f;
#else
		float flMinChargeLevel = 1.f;
#endif

		bool bCharged = flChargeLevel >= flMinChargeLevel;

		if ( bCharged )
		{
			bElectrocuted = true;
		}
	}
	else if ( IsPlayerClass( TF_CLASS_ENGINEER ) )
	{
		if ( m_Shared.IsCarryingObject() )
		{
			CTakeDamageInfo info( pPlayerAttacker, pPlayerAttacker, NULL, vec3_origin, GetAbsOrigin(), 0, DMG_GENERIC );
			info.SetDamageCustom( TF_DMG_CUSTOM_CARRIED_BUILDING );
			if ( m_Shared.GetCarriedObject() != NULL )
			{
				m_Shared.GetCarriedObject()->Killed( info );
			}
		}
	}

	// Record if we were stunned for achievement tracking.
	m_iOldStunFlags = m_Shared.GetStunFlags();

	// Determine the optional assist for the kill.
	DetermineAssistForKill( info );

	// put here to stop looping kritz sound from playing til respawn.
	if ( m_Shared.InCond( TF_COND_CRITBOOSTED ) )
	{
		StopSound( "TFPlayer.CritBoostOn" );
	}

	// Reset our model if we were disguised
	if ( bDisguised )
	{
		UpdateModel();
	}

	RemoveTeleportEffect();

	// Drop their weapon
	if ( IsDODClass() )
		DropPrimaryWeapon();
	else // Drop a pack with their leftover ammo
		DropAmmoPack();

	// If the player has a capture flag and was killed by another player, award that player a defense
	if ( HasItem() && pPlayerAttacker && ( pPlayerAttacker != this ) )
	{
		CCaptureFlag *pCaptureFlag = dynamic_cast<CCaptureFlag *>( GetItem() );
		if ( pCaptureFlag )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
			if ( event )
			{
				event->SetInt( "player", pPlayerAttacker->entindex() );
				event->SetInt( "eventtype", TF_FLAGEVENT_DEFEND );
				event->SetInt( "priority", 8 );
				gameeventmanager->FireEvent( event );
			}
			CTF_GameStats.Event_PlayerDefendedPoint( pPlayerAttacker );
		}
	}

	// Remove all items...
	RemoveAllItems( true );

	for ( int iWeapon = 0; iWeapon < TF_PLAYER_WEAPON_COUNT; ++iWeapon )
	{
		CBaseCombatWeapon *pWeapon = (CBaseCombatWeapon *)GetWeapon( iWeapon );

		if ( pWeapon && pWeapon->GetWeaponTypeReference() == WEAPON_TF )
		{
			pWeapon->WeaponReset();
		}
	}

	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->SendViewModelAnim( ACT_IDLE );
		GetActiveWeapon()->Holster();
		SetActiveWeapon( NULL );
	}

	ClearZoomOwner();

	m_vecLastDeathPosition = GetAbsOrigin();

	CTakeDamageInfo info_modified = info;

	// Ragdoll, gib, or death animation.
	bool bRagdoll = true;
	bool bGib = false;

	// See if we should gib.
	if ( ShouldGib( info ) )
	{
		bGib = true;
		bRagdoll = false;
	}
	else
	// See if we should play a custom death animation.
	{
		// If this was a rocket/grenade kill that didn't gib, exaggerated the blast force
		if ( ( info.GetDamageType() & DMG_BLAST ) != 0 )
		{
			Vector vForceModifier = info.GetDamageForce();
			vForceModifier.x *= 2.5;
			vForceModifier.y *= 2.5;
			vForceModifier.z *= 2;
			info_modified.SetDamageForce( vForceModifier );
		}
	}

	if ( bElectrocuted && bGib )
	{
		const char *pEffectName = ( GetTeamNumber() == TF_TEAM_RED ) ? "electrocuted_gibbed_red" : "electrocuted_gibbed_blue";
		DispatchParticleEffect( pEffectName, GetAbsOrigin(), vec3_angle );
		EmitSound( "TFPlayer.MedicChargedDeath" );
	}

	// show killer in death cam mode
	// chopped down version of SetObserverTarget without the team check
	if( pPlayerAttacker )
	{
		// See if we were killed by a sentrygun. If so, look at that instead of the player
		if ( info.GetInflictor() && info.GetInflictor()->IsBaseObject() )
		{
			// Catches the case where we're killed directly by the sentrygun (i.e. bullets)
			// Look at the sentrygun
			m_hObserverTarget.Set( info.GetInflictor() ); 
		}
		// See if we were killed by a projectile emitted from a base object. The attacker
		// will still be the owner of that object, but we want the deathcam to point to the 
		// object itself.
		else if ( info.GetInflictor() && info.GetInflictor()->GetOwnerEntity() && 
					info.GetInflictor()->GetOwnerEntity()->IsBaseObject() )
		{
			m_hObserverTarget.Set( info.GetInflictor()->GetOwnerEntity() );
		}
		else
		{
			// Look at the player
			m_hObserverTarget.Set( info.GetAttacker() ); 
		}

		// reset fov to default
		SetFOV( this, 0 );
	}
	else if ( info.GetAttacker() && info.GetAttacker()->IsBaseObject() )
	{
		// Catches the case where we're killed by entities spawned by the sentrygun (i.e. rockets)
		// Look at the sentrygun. 
		m_hObserverTarget.Set( info.GetAttacker() ); 
	}
	else
	{
		m_hObserverTarget.Set( NULL );
	}

	if ( info_modified.GetDamageCustom() == TF_DMG_CUSTOM_SUICIDE )
	{
		// if this was suicide, recalculate attacker to see if we want to award the kill to a recent damager
		info_modified.SetAttacker( TFGameRules()->GetDeathScorer( info.GetAttacker(), info.GetInflictor(), this ) );
	}

	BaseClass::Event_Killed( info_modified );

	int iIceRagdoll = 0;

	CTFPlayer *pInflictor = ToTFPlayer( info.GetInflictor() );
	if ( ( TF_DMG_CUSTOM_HEADSHOT == info.GetDamageCustom() ) && pInflictor )
	{				
		CTF_GameStats.Event_Headshot( pInflictor );
	}
	else if ( ( TF_DMG_CUSTOM_BACKSTAB == info.GetDamageCustom() ) && pInflictor )
	{
		CTF_GameStats.Event_Backstab( pInflictor );
	}

	bool bCloakedCorpse = false;

	int iGoldRagdoll = 0;

	int iRagdollsBecomeAsh = 0;

	int iRagdollsPlasmaEffect = 0;

	int iCustomDamage = info.GetDamageCustom();
	if ( iRagdollsPlasmaEffect )
	{
		iCustomDamage = TF_DMG_CUSTOM_PLASMA;
	}

	int iCritOnHardHit = 0;

	// Create the ragdoll entity.
	if ( bGib || bRagdoll )
	{
		CreateRagdollEntity( bGib, bBurning, bElectrocuted, bOnGround, bCloakedCorpse, iGoldRagdoll != 0, iIceRagdoll != 0, iRagdollsBecomeAsh != 0, iCustomDamage, ( iCritOnHardHit != 0 ) );
	}

	// Remove all conditions...
	m_Shared.RemoveAllCond();

	// Don't overflow the value for this.
	m_iHealth = 0;

	// If we died in sudden death and we're an engineer, explode our buildings
	if ( IsPlayerClass( TF_CLASS_ENGINEER ) && TFGameRules()->InStalemate() )
	{
		for (int i = GetObjectCount()-1; i >= 0; i--)
		{
			CBaseObject *obj = GetObject(i);
			Assert( obj );

			if ( obj )
			{
				obj->DetonateObject();
			}		
		}
	}

	// Day of Defeat
	m_flStunDuration = 0.0f;
	m_flStunMaxAlpha = 0;

	// cancel deafen think
	SetContextThink( NULL, 0.0, DOD_DEAFEN_CONTEXT );

	// cancel deafen dsp
	CSingleUserRecipientFilter user( this );
	enginesound->SetPlayerDSP( user, 0, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pWeapon - 
//			&vecOrigin - 
//			&vecAngles - 
//-----------------------------------------------------------------------------
bool CTFPlayer::CalculateAmmoPackPositionAndAngles( CTFWeaponBase *pWeapon, Vector &vecOrigin, QAngle &vecAngles )
{
	// Look up the hand and weapon bones.
	int iHandBone = LookupBone( "weapon_bone" );
	if ( iHandBone == -1 )
		return false;

	GetBonePosition( iHandBone, vecOrigin, vecAngles );

	// Draw the position and angles.
	Vector vecDebugForward2, vecDebugRight2, vecDebugUp2;
	AngleVectors( vecAngles, &vecDebugForward2, &vecDebugRight2, &vecDebugUp2 );

	/*
	NDebugOverlay::Line( vecOrigin, ( vecOrigin + vecDebugForward2 * 25.0f ), 255, 0, 0, false, 30.0f );
	NDebugOverlay::Line( vecOrigin, ( vecOrigin + vecDebugRight2 * 25.0f ), 0, 255, 0, false, 30.0f );
	NDebugOverlay::Line( vecOrigin, ( vecOrigin + vecDebugUp2 * 25.0f ), 0, 0, 255, false, 30.0f ); 
	*/

	VectorAngles( vecDebugUp2, vecAngles );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// NOTE: If we don't let players drop ammo boxes, we don't need this code..
//-----------------------------------------------------------------------------
void CTFPlayer::AmmoPackCleanUp( void )
{
	// If we have more than 3 ammo packs out now, destroy the oldest one.
	int iNumPacks = 0;
	CTFAmmoPack *pOldestBox = NULL;

	// Cycle through all ammobox in the world and remove them
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "tf_ammo_pack" );
	while ( pEnt )
	{
		CBaseEntity *pOwner = pEnt->GetOwnerEntity();
		if (pOwner == this)
		{
			CTFAmmoPack *pThisBox = dynamic_cast<CTFAmmoPack *>( pEnt );
			Assert( pThisBox );
			if ( pThisBox )
			{
				iNumPacks++;

				// Find the oldest one
				if ( pOldestBox == NULL || pOldestBox->GetCreationTime() > pThisBox->GetCreationTime() )
				{
					pOldestBox = pThisBox;
				}
			}
		}

		pEnt = gEntList.FindEntityByClassname( pEnt, "tf_ammo_pack" );
	}

	// If they have more than 3 packs active, remove the oldest one
	if ( iNumPacks > 3 && pOldestBox )
	{
		UTIL_Remove( pOldestBox );
	}
}		

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DropAmmoPack( void )
{
	// We want the ammo packs to look like the player's weapon model they were carrying.
	// except if they are melee or building weapons
	CTFWeaponBase *pWeapon = NULL;
	CTFWeaponBase *pActiveWeapon = dynamic_cast<CTFWeaponBase*>( m_Shared.GetActivePCWeapon() );

	if ( !pActiveWeapon || pActiveWeapon->GetPCWpnData().m_ExtraTFWeaponData.m_bDontDrop )
	{
		// Don't drop this one, find another one to drop

		int iWeight = -1;

		// find the highest weighted weapon
		for (int i = 0;i < WeaponCount(); i++) 
		{
			CTFWeaponBase *pWpn = ( CTFWeaponBase *)GetWeapon(i);
			if ( !pWpn )
				continue;

			if ( pWpn->GetPCWpnData().m_ExtraTFWeaponData.m_bDontDrop )
				continue;

			int iThisWeight = pWpn->GetPCWpnData().iWeight;

			if ( iThisWeight > iWeight )
			{
				iWeight = iThisWeight;
				pWeapon = pWpn;
			}
		}
	}
	else
	{
		pWeapon = pActiveWeapon;
	}

	// If we didn't find one, bail
	if ( !pWeapon )
		return;

	// We need to find bones on the world model, so switch the weapon to it.
	const char *pszWorldModel = pWeapon->GetWorldModel();
	pWeapon->SetModel( pszWorldModel );


	// Find the position and angle of the weapons so the "ammo box" matches.
	Vector vecPackOrigin;
	QAngle vecPackAngles;
	if( !CalculateAmmoPackPositionAndAngles( pWeapon, vecPackOrigin, vecPackAngles ) )
		return;

	// Fill the ammo pack with unused player ammo, if out add a minimum amount.
	int iPrimary = max( 5, GetAmmoCount( TF_AMMO_PRIMARY ) );
	int iSecondary = max( 5, GetAmmoCount( TF_AMMO_SECONDARY ) );
	int iMetal = max( 5, GetAmmoCount( TF_AMMO_METAL ) );	

	// Create the ammo pack.
	CTFAmmoPack *pAmmoPack = CTFAmmoPack::Create( vecPackOrigin, vecPackAngles, this, pszWorldModel );
	Assert( pAmmoPack );
	if ( pAmmoPack )
	{
		// Remove all of the players ammo.
		RemoveAllAmmo();

		// Fill up the ammo pack.
		pAmmoPack->GiveAmmo( iPrimary, TF_AMMO_PRIMARY );
		pAmmoPack->GiveAmmo( iSecondary, TF_AMMO_SECONDARY );
		pAmmoPack->GiveAmmo( iMetal, TF_AMMO_METAL );

		Vector vecRight, vecUp;
		AngleVectors( EyeAngles(), NULL, &vecRight, &vecUp );

		// Calculate the initial impulse on the weapon.
		Vector vecImpulse( 0.0f, 0.0f, 0.0f );
		vecImpulse += vecUp * random->RandomFloat( -0.25, 0.25 );
		vecImpulse += vecRight * random->RandomFloat( -0.25, 0.25 );
		VectorNormalize( vecImpulse );
		vecImpulse *= random->RandomFloat( tf_weapon_ragdoll_velocity_min.GetFloat(), tf_weapon_ragdoll_velocity_max.GetFloat() );
		vecImpulse += GetAbsVelocity();

		// Cap the impulse.
		float flSpeed = vecImpulse.Length();
		if ( flSpeed > tf_weapon_ragdoll_maxspeed.GetFloat() )
		{
			VectorScale( vecImpulse, tf_weapon_ragdoll_maxspeed.GetFloat() / flSpeed, vecImpulse );
		}

		if ( pAmmoPack->VPhysicsGetObject() )
		{
			// We can probably remove this when the mass on the weapons is correct!
			pAmmoPack->VPhysicsGetObject()->SetMass( 25.0f );
			AngularImpulse angImpulse( 0, random->RandomFloat( 0, 100 ), 0 );
			pAmmoPack->VPhysicsGetObject()->SetVelocityInstantaneous( &vecImpulse, &angImpulse );
		}

		pAmmoPack->SetInitialVelocity( vecImpulse );

		pAmmoPack->m_nSkin = ( GetTeamNumber() == TF_TEAM_RED ) ? 0 : 1;

		// Give the ammo pack some health, so that trains can destroy it.
		pAmmoPack->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		pAmmoPack->m_takedamage = DAMAGE_YES;		
		pAmmoPack->SetHealth( 900 );
		
		pAmmoPack->SetBodygroup( 1, 1 );
	
		// Clean up old ammo packs if they exist in the world
		AmmoPackCleanUp();	
	}	
	pWeapon->SetModel( pWeapon->GetViewModel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PlayerDeathThink( void )
{
	//overridden, do nothing
}

//-----------------------------------------------------------------------------
// Purpose: Remove the tf items from the player then call into the base class
//          removal of items.
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveAllItems( bool removeSuit )
{
	// If the player has a capture flag, drop it.
	if ( HasItem() )
	{
		int nFlagTeamNumber = GetItem()->GetTeamNumber();
		GetItem()->Drop( this, true );

		IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
		if ( event )
		{
			event->SetInt( "player", entindex() );
			event->SetInt( "eventtype", TF_FLAGEVENT_DROPPED );
			event->SetInt( "priority", 8 );
			event->SetInt( "team", nFlagTeamNumber );
			gameeventmanager->FireEvent( event );
		}
	}

	if ( m_hOffHandWeapon.Get() )
	{ 
		HolsterOffHandWeapon();

		// hide the weapon model
		// don't normally have to do this, unless we have a holster animation
		CBaseViewModel *vm = GetViewModel( 1 );
		if ( vm )
		{
			vm->SetWeaponModel( NULL, NULL );
		}

		m_hOffHandWeapon = NULL;
	}

	Weapon_SetLast( NULL );
	UpdateClientData();
}

void CTFPlayer::ClientHearVox( const char *pSentence )
{
	//TFTODO: implement this.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::UpdateModel( void )
{
	SetModel( GetPlayerClass()->GetModelName() );

	// Immediately reset our collision bounds - our collision bounds will be set to the model's bounds.
	SetCollisionBounds( GetPlayerMins(), GetPlayerMaxs() );

	// DOD Reset their helmet bodygroup
	if ( GetBodygroup( BODYGROUP_HELMET ) == GetPlayerClass()->GetData()->m_iHairGroup && IsDODClass() )
		SetBodygroup( BODYGROUP_HELMET, GetPlayerClass()->GetData()->m_iHelmetGroup );

	if ( GetGameAnimStateType() )
		GetGameAnimStateType()->OnNewModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iSkin - 
//-----------------------------------------------------------------------------
void CTFPlayer::UpdateSkin( int iTeam )
{
	// The player's skin is team - 2.
	int iSkin = iTeam - 2;

	// No other factions supports team-based skins, this may change in the future
	if ( !IsTF2Class() )
		iSkin = 0;

	// Check to see if the skin actually changed.
	if ( iSkin != m_iLastSkin )
	{
		m_nSkin = iSkin;
		m_iLastSkin = iSkin;
	}
}

//=========================================================================
// Displays the state of the items specified by the Goal passed in
void CTFPlayer::DisplayLocalItemStatus( CTFGoal *pGoal )
{
#if 0
	for (int i = 0; i < 4; i++)
	{
		if (pGoal->display_item_status[i] != 0)
		{
			CTFGoalItem *pItem = Finditem(pGoal->display_item_status[i]);
			if (pItem)
				DisplayItemStatus(pGoal, this, pItem);
			else
				ClientPrint( this, HUD_PRINTTALK, "#Item_missing" );
		}
	}
#endif
}

//=========================================================================
// Called when the player disconnects from the server.
void CTFPlayer::TeamFortress_ClientDisconnected( void )
{
	RemoveAllOwnedEntitiesFromWorld( true );
	RemoveNemesisRelationships();

	StopTaunt();

	RemoveAllWeapons();

	RemoveAllItems( true );
}

//=========================================================================
// Removes everything this player has (buildings, grenades, etc.) from the world
void CTFPlayer::RemoveAllOwnedEntitiesFromWorld( bool bExplodeBuildings /* = false */ )
{
	RemoveOwnedProjectiles();

	if ( TFGameRules()->IsMannVsMachineMode() && ( GetTeamNumber() == TF_TEAM_PVE_INVADERS ) )
	{
		// MvM engineer bots leave their sentries behind when they die
		return;
	}

	// SEALTODO
	//if ( IsBotOfType( TF_BOT_TYPE ) && ToTFBot( this )->HasAttribute( CTFBot::RETAIN_BUILDINGS ) )
	//{
	//	// keep this bot's buildings
	//	return;
	//}

	// Destroy any buildables - this should replace TeamFortress_RemoveBuildings
	RemoveAllObjects( bExplodeBuildings );
}

//=========================================================================
// Removes all rockets the player has fired into the world
// (this prevents a team kill cheat where players would fire rockets 
// then change teams to kill their own team)
void CTFPlayer::RemoveOwnedProjectiles( void )
{
	FOR_EACH_VEC( IBaseProjectileAutoList::AutoList(), i )
	{
		CBaseProjectile *pProjectile = static_cast< CBaseProjectile* >( IBaseProjectileAutoList::AutoList()[i] );

		// if the player owns this entity, remove it
		bool bOwner = ( pProjectile->GetOwnerEntity() == this );

		if ( !bOwner )
		{
			if ( pProjectile->GetBaseProjectileType() == TF_BASE_PROJECTILE_GRENADE )
			{

				CTFWeaponBaseGrenadeProj *pGrenade = assert_cast<CTFWeaponBaseGrenadeProj*>( pProjectile );
				if ( pGrenade )
				{
					bOwner = ( pGrenade->GetThrower() == this );
				}
			}
			else if ( pProjectile->GetProjectileType() == TF_PROJECTILE_SENTRY_ROCKET )
			{
				CTFProjectile_SentryRocket *pRocket = assert_cast<CTFProjectile_SentryRocket*>( pProjectile );
				if ( pRocket )
				{
					bOwner = ( pRocket->GetScorer() == this );
				}
			}
		}

		if ( bOwner )
		{
			pProjectile->SetTouch( NULL );
			pProjectile->AddEffects( EF_NODRAW );
			UTIL_Remove( pProjectile );
		}
	}

	FOR_EACH_VEC( ITFFlameEntityAutoList::AutoList(), i )
	{
		CTFFlameEntity *pFlameEnt = static_cast< CTFFlameEntity* >( ITFFlameEntityAutoList::AutoList()[i] );

		if ( pFlameEnt->IsEntityAttacker( this ) )
		{
			pFlameEnt->SetTouch( NULL );
			pFlameEnt->AddEffects( EF_NODRAW );
			UTIL_Remove( pFlameEnt );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::NoteWeaponFired()
{
	Assert( m_pCurrentCommand );
	if( m_pCurrentCommand )
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}

	// Remember the tickcount when the weapon was fired and lock viewangles here!
	if ( m_iLockViewanglesTickNumber != gpGlobals->tickcount )
	{
		m_iLockViewanglesTickNumber = gpGlobals->tickcount;
		m_qangLockViewangles = pl.v_angle;
	}
}

//=============================================================================
//
// Player state functions.
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CPlayerStateInfo *CTFPlayer::StateLookupInfo( int nState )
{
	// This table MUST match the 
	static CPlayerStateInfo playerStateInfos[] =
	{
		{ TF_STATE_ACTIVE,				"TF_STATE_ACTIVE",				&CTFPlayer::StateEnterACTIVE,				NULL,	NULL },
		{ TF_STATE_WELCOME,				"TF_STATE_WELCOME",				&CTFPlayer::StateEnterWELCOME,				NULL,	&CTFPlayer::StateThinkWELCOME },
		{ TF_STATE_OBSERVER,			"TF_STATE_OBSERVER",			&CTFPlayer::StateEnterOBSERVER,				NULL,	&CTFPlayer::StateThinkOBSERVER },
		{ TF_STATE_DYING,				"TF_STATE_DYING",				&CTFPlayer::StateEnterDYING,				NULL,	&CTFPlayer::StateThinkDYING },
	};

	for ( int iState = 0; iState < ARRAYSIZE( playerStateInfos ); ++iState )
	{
		if ( playerStateInfos[iState].m_nPlayerState == nState )
			return &playerStateInfos[iState];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateEnter( int nState )
{
	m_Shared.m_nPlayerState = nState;
	m_pStateInfo = StateLookupInfo( nState );

	if ( tf_playerstatetransitions.GetInt() == -1 || tf_playerstatetransitions.GetInt() == entindex() )
	{
		if ( m_pStateInfo )
			Msg( "ShowStateTransitions: entering '%s'\n", m_pStateInfo->m_pStateName );
		else
			Msg( "ShowStateTransitions: entering #%d\n", nState );
	}

	// Initialize the new state.
	if ( m_pStateInfo && m_pStateInfo->pfnEnterState )
	{
		(this->*m_pStateInfo->pfnEnterState)();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateLeave( void )
{
	if ( m_pStateInfo && m_pStateInfo->pfnLeaveState )
	{
		(this->*m_pStateInfo->pfnLeaveState)();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateTransition( int nState )
{
	StateLeave();
	StateEnter( nState );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateEnterWELCOME( void )
{
	PickWelcomeObserverPoint();  
	
	StartObserverMode( OBS_MODE_FIXED );

	// Important to set MOVETYPE_NONE or our physics object will fall while we're sitting at one of the intro cameras.
	SetMoveType( MOVETYPE_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW | EF_NOSHADOW );		

	PhysObjectSleep();

	if ( gpGlobals->eLoadType == MapLoad_Background )
	{
		m_bSeenRoundInfo = true;

		ChangeTeam( TEAM_SPECTATOR );
	}
	else if ( (TFGameRules() && TFGameRules()->IsLoadingBugBaitReport()) )
	{
		m_bSeenRoundInfo = true;
		
		ChangeTeam( TF_TEAM_BLUE );
		SetDesiredPlayerClassIndex( TF_CLASS_SCOUT );
		ForceRespawn();
	}
	else if ( IsInCommentaryMode() )
	{
		m_bSeenRoundInfo = true;
	}
	else
	{
		if ( !IsX360() )
		{
			KeyValues *data = new KeyValues( "data" );
			data->SetString( "title", "#TF_Welcome" );	// info panel title
			data->SetString( "type", "1" );				// show userdata from stringtable entry
			data->SetString( "msg",	"motd" );			// use this stringtable entry
			data->SetString( "cmd", "mapinfo" );		// exec this command if panel closed

			ShowViewPortPanel( PANEL_INFO, true, data );

			data->deleteThis();
		}
		else
		{
			ShowViewPortPanel( PANEL_MAPINFO, true );
		}

		m_bSeenRoundInfo = false;
	}

	m_bIsIdle = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::StateThinkWELCOME( void )
{
	if ( IsInCommentaryMode() && !IsFakeClient() )
	{
		ChangeTeam( TF_TEAM_BLUE );
		SetDesiredPlayerClassIndex( TF_CLASS_SCOUT );
		ForceRespawn();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateEnterACTIVE()
{
	SetMoveType( MOVETYPE_WALK );
	RemoveEffects( EF_NODRAW | EF_NOSHADOW );
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	m_Local.m_iHideHUD = 0;
	PhysObjectWake();

	m_flLastAction = gpGlobals->curtime;
	m_flLastHealthRegenAt = gpGlobals->curtime;
	SetContextThink( &CTFPlayer::RegenThink, gpGlobals->curtime + TF_REGEN_TIME, "RegenThink" );
	m_bIsIdle = false;

	// If we're a Medic, start thinking to regen myself
	if ( IsPlayerClass( TF_CLASS_MEDIC ) )
	{
		SetContextThink( &CTFPlayer::RegenThink, gpGlobals->curtime + TF_REGEN_TIME, "RegenThink" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::SetObserverMode(int mode)
{
	if ( mode < OBS_MODE_NONE || mode >= NUM_OBSERVER_MODES )
		return false;

	// Skip OBS_MODE_POI as we're not using that.
	if ( mode == OBS_MODE_POI )
	{
		mode++;
	}

	// Skip over OBS_MODE_ROAMING for dead players
	if( GetTeamNumber() > TEAM_SPECTATOR )
	{
		if ( IsDead() && ( mode > OBS_MODE_FIXED ) && mp_fadetoblack.GetBool() )
		{
			mode = OBS_MODE_CHASE;
		}
		else if ( mode == OBS_MODE_ROAMING )
		{
			mode = OBS_MODE_IN_EYE;
		}
	}

	if ( m_iObserverMode > OBS_MODE_DEATHCAM )
	{
		// remember mode if we were really spectating before
		m_iObserverLastMode = m_iObserverMode;
	}

	m_iObserverMode = mode;
	m_flLastAction = gpGlobals->curtime;

	switch ( mode )
	{
	case OBS_MODE_NONE:
	case OBS_MODE_FIXED :
	case OBS_MODE_DEATHCAM :
		SetFOV( this, 0 );	// Reset FOV
		SetViewOffset( vec3_origin );
		SetMoveType( MOVETYPE_NONE );
		break;

	case OBS_MODE_CHASE :
	case OBS_MODE_IN_EYE :	
		// udpate FOV and viewmodels
		SetObserverTarget( m_hObserverTarget );	
		SetMoveType( MOVETYPE_OBSERVER );
		break;

	case OBS_MODE_ROAMING :
		SetFOV( this, 0 );	// Reset FOV
		SetObserverTarget( m_hObserverTarget );
		SetViewOffset( vec3_origin );
		SetMoveType( MOVETYPE_OBSERVER );
		break;
		
	case OBS_MODE_FREEZECAM:
		SetFOV( this, 0 );	// Reset FOV
		SetObserverTarget( m_hObserverTarget );
		SetViewOffset( vec3_origin );
		SetMoveType( MOVETYPE_OBSERVER );
		break;
	}

	CheckObserverSettings();

	return true;	
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateEnterOBSERVER( void )
{
	// Always start a spectator session in chase mode
	m_iObserverLastMode = OBS_MODE_CHASE;

	if( m_hObserverTarget == NULL )
	{
		// find a new observer target
		CheckObserverSettings();
	}

	if ( !m_bAbortFreezeCam )
	{
		FindInitialObserverTarget();
	}

	StartObserverMode( m_iObserverLastMode );

	PhysObjectSleep();

	m_bIsIdle = false;

	if ( GetTeamNumber() != TEAM_SPECTATOR )
	{
		HandleFadeToBlack();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::StateThinkOBSERVER()
{
	// Make sure nobody has changed any of our state.
	Assert( m_takedamage == DAMAGE_NO );
	Assert( IsSolidFlagSet( FSOLID_NOT_SOLID ) );

	// Must be dead.
	Assert( m_lifeState == LIFE_DEAD );
	Assert( pl.deadflag );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::StateEnterDYING( void )
{
	SetMoveType( MOVETYPE_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );

	m_bPlayedFreezeCamSound = false;
	m_bAbortFreezeCam = false;
}

//-----------------------------------------------------------------------------
// Purpose: Move the player to observer mode once the dying process is over
//-----------------------------------------------------------------------------
void CTFPlayer::StateThinkDYING( void )
{
	// If we have a ragdoll, it's time to go to deathcam
	if ( !m_bAbortFreezeCam && m_hRagdoll && 
		(m_lifeState == LIFE_DYING || m_lifeState == LIFE_DEAD) && 
		GetObserverMode() != OBS_MODE_FREEZECAM )
	{
		if ( GetObserverMode() != OBS_MODE_DEATHCAM )
		{
			StartObserverMode( OBS_MODE_DEATHCAM );	// go to observer mode
		}
		RemoveEffects( EF_NODRAW | EF_NOSHADOW );	// still draw player body
	}

	float flTimeInFreeze = spec_freeze_traveltime.GetFloat() + spec_freeze_time.GetFloat();
	float flFreezeEnd = (m_flDeathTime + TF_DEATH_ANIMATION_TIME + flTimeInFreeze );
	if ( !m_bPlayedFreezeCamSound  && GetObserverTarget() && GetObserverTarget() != this )
	{
		// Start the sound so that it ends at the freezecam lock on time
		float flFreezeSoundLength = 0.3;
		float flFreezeSoundTime = (m_flDeathTime + TF_DEATH_ANIMATION_TIME ) + spec_freeze_traveltime.GetFloat() - flFreezeSoundLength;
		if ( gpGlobals->curtime >= flFreezeSoundTime )
		{
			CSingleUserRecipientFilter filter( this );
			EmitSound_t params;
			params.m_flSoundTime = 0;
			params.m_pSoundName = "TFPlayer.FreezeCam";
			EmitSound( filter, entindex(), params );

			m_bPlayedFreezeCamSound = true;
		}
	}

	if ( gpGlobals->curtime >= (m_flDeathTime + TF_DEATH_ANIMATION_TIME ) )	// allow x seconds death animation / death cam
	{
		if ( GetObserverTarget() && GetObserverTarget() != this )
		{
			if ( !m_bAbortFreezeCam && gpGlobals->curtime < flFreezeEnd )
			{
				if ( GetObserverMode() != OBS_MODE_FREEZECAM )
				{
					StartObserverMode( OBS_MODE_FREEZECAM );
					PhysObjectSleep();
				}
				return;
			}
		}

		if ( GetObserverMode() == OBS_MODE_FREEZECAM )
		{
			// If we're in freezecam, and we want out, abort.  (only if server is not using mp_fadetoblack)
			if ( m_bAbortFreezeCam && !mp_fadetoblack.GetBool() )
			{
				if ( m_hObserverTarget == NULL )
				{
					// find a new observer target
					CheckObserverSettings();
				}

				FindInitialObserverTarget();
				SetObserverMode( OBS_MODE_CHASE );
				ShowViewPortPanel( "specgui" , ModeWantsSpectatorGUI(OBS_MODE_CHASE) );
			}
		}

		// Don't allow anyone to respawn until freeze time is over, even if they're not
		// in freezecam. This prevents players skipping freezecam to spawn faster.
		if ( gpGlobals->curtime < flFreezeEnd )
			return;

		m_lifeState = LIFE_RESPAWNABLE;

		StopAnimation();

		AddEffects( EF_NOINTERP );

		if ( GetMoveType() != MOVETYPE_NONE && (GetFlags() & FL_ONGROUND) )
			SetMoveType( MOVETYPE_NONE );

		StateTransition( TF_STATE_OBSERVER );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::AttemptToExitFreezeCam( void )
{
	float flFreezeTravelTime = (m_flDeathTime + TF_DEATH_ANIMATION_TIME ) + spec_freeze_traveltime.GetFloat() + 0.5;
	if ( gpGlobals->curtime < flFreezeTravelTime )
		return;

	m_bAbortFreezeCam = true;
}

class CIntroViewpoint : public CPointEntity
{
	DECLARE_CLASS( CIntroViewpoint, CPointEntity );
public:
	DECLARE_DATADESC();

	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	int			m_iIntroStep;
	float		m_flStepDelay;
	string_t	m_iszMessage;
	string_t	m_iszGameEvent;
	float		m_flEventDelay;
	int			m_iGameEventData;
	float		m_flFOV;
};

BEGIN_DATADESC( CIntroViewpoint )
	DEFINE_KEYFIELD( m_iIntroStep,	FIELD_INTEGER,	"step_number" ),
	DEFINE_KEYFIELD( m_flStepDelay,	FIELD_FLOAT,	"time_delay" ),
	DEFINE_KEYFIELD( m_iszMessage,	FIELD_STRING,	"hint_message" ),
	DEFINE_KEYFIELD( m_iszGameEvent,	FIELD_STRING,	"event_to_fire" ),
	DEFINE_KEYFIELD( m_flEventDelay,	FIELD_FLOAT,	"event_delay" ),
	DEFINE_KEYFIELD( m_iGameEventData,	FIELD_INTEGER,	"event_data_int" ),
	DEFINE_KEYFIELD( m_flFOV,	FIELD_FLOAT,	"fov" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( game_intro_viewpoint, CIntroViewpoint );

//-----------------------------------------------------------------------------
// Purpose: Give the player some ammo.
// Input  : iCount - Amount of ammo to give.
//			iAmmoIndex - Index of the ammo into the AmmoInfoArray
//			iMax - Max carrying capability of the player
// Output : Amount of ammo actually given
//-----------------------------------------------------------------------------
int CTFPlayer::GiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound )
{
	if ( iCount <= 0 )
	{
        return 0;
	}

	if ( !g_pGameRules->CanHaveAmmo( this, iAmmoIndex ) )
	{
		// game rules say I can't have any more of this ammo type.
		return 0;
	}

	if ( iAmmoIndex < 0 || iAmmoIndex >= MAX_AMMO_SLOTS )
	{
		return 0;
	}

	int iMax = GetMaxAmmo( iAmmoIndex );

	int iAdd = min( iCount, iMax - GetAmmoCount(iAmmoIndex) );
	if ( iAdd < 1 )
		return 0;

	// Ammo pickup sound
	if ( !bSuppressSound )
		EmitSound( "BaseCombatCharacter.AmmoPickup" );

	m_iAmmo.Set( iAmmoIndex, m_iAmmo[iAmmoIndex] + iAdd );
	return iAdd;
}

//-----------------------------------------------------------------------------
// Purpose: Has to be const for override, but needs to access non-const member methods.
//-----------------------------------------------------------------------------
int	CTFPlayer::GetMaxHealth() const
{
	int iMax = GetMaxHealthForBuffing();

	return MAX( iMax, 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayer::GetMaxHealthForBuffing() const
{
	int iMax = m_PlayerClass.GetMaxHealth();

	return iMax;
}

//-----------------------------------------------------------------------------
// Purpose: Reset player's information and force him to spawn
//-----------------------------------------------------------------------------
void CTFPlayer::ForceRespawn( void )
{
	CTF_GameStats.Event_PlayerForceRespawn( this );

	m_flSpawnTime = gpGlobals->curtime;

	int iDesiredClass = GetDesiredPlayerClassIndex();

	if ( iDesiredClass == TF_CLASS_UNDEFINED )
	{
		return;
	}

	// SEALTODO: support game based random here
	if ( iDesiredClass == TF_CLASS_RANDOM )
	{
		// Don't let them be the same class twice in a row
		do{
			iDesiredClass = random->RandomInt( TF_FIRST_NORMAL_CLASS, TF_LAST_NORMAL_CLASS );
		} while( iDesiredClass == GetPlayerClass()->GetClassIndex() );
	}

	// Catch if we're allowed to play this class, translate if not
	int iTranslatedClass = TranslateClassIndex( iDesiredClass, GetTeamNumber() );
	if ( iTranslatedClass != -1 )
		iDesiredClass = iTranslatedClass;

	if ( HasTheFlag() )
	{
		DropFlag();
	}

	if ( GetPlayerClass()->GetClassIndex() != iDesiredClass )
	{
		// clean up any pipebombs/buildings in the world (no explosions)
		RemoveAllOwnedEntitiesFromWorld();

		GetPlayerClass()->Init( iDesiredClass );

		CTF_GameStats.Event_PlayerChangedClass( this );
	}

	m_Shared.RemoveAllCond();

	RemoveAllItems( true );

	// Reset ground state for airwalk animations
	SetGroundEntity( NULL );

	// TODO: move this into conditions
	RemoveTeleportEffect();

	// remove invisibility very quickly	
	m_Shared.FadeInvis( 0.1 );

	// Stop any firing that was taking place before respawn.
	m_nButtons = 0;

	StateTransition( TF_STATE_ACTIVE );
	Spawn();
}

int iAlliesClass[] = {
	DOD_CLASS_TOMMY,
	DOD_CLASS_BAR,
	DOD_CLASS_GARAND,
	DOD_CLASS_SPRING,
	DOD_CLASS_BAZOOKA,
	DOD_CLASS_30CAL,
};

int iAxisClass[] = {
	DOD_CLASS_MP40,
	DOD_CLASS_MP44,
	DOD_CLASS_K98,
	DOD_CLASS_K98s,
	DOD_CLASS_PSCHRECK,
	DOD_CLASS_MG42,
};

int CTFPlayer::TranslateClassIndex( int iClass, int iTeam )
{
	int iOverride = -1;

	// if we're axis on red, translate us over to allies
	if ( iTeam == TF_TEAM_RED )
	{
		if ( iClass >= DOD_FIRST_AXIS_CLASS && iClass <= DOD_LAST_AXIS_CLASS )
			iOverride = iAlliesClass[iClass - DOD_FIRST_AXIS_CLASS];
	}
	else
	{
		if ( iClass >= DOD_FIRST_ALLIES_CLASS && iClass <= DOD_LAST_ALLIES_CLASS )
			iOverride = iAxisClass[iClass - DOD_FIRST_ALLIES_CLASS];
	}

	return iOverride;
}

//-----------------------------------------------------------------------------
// Purpose: Do nothing multiplayer_animstate takes care of animation.
// Input  : playerAnim - 
//-----------------------------------------------------------------------------
void CTFPlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	return;
}

//-----------------------------------------------------------------------------
// Purpose: Handle cheat commands
// Input  : iImpulse - 
//-----------------------------------------------------------------------------
void CTFPlayer::CheatImpulseCommands( int iImpulse )
{
	switch( iImpulse )
	{
	case 101:
		{
			if( sv_cheats->GetBool() )
			{
				extern int gEvilImpulse101;
				gEvilImpulse101 = true;

				switch ( GetClassType() )
				{
					case FACTION_DOD:
					{
						GiveAmmo( 1000, DOD_AMMO_COLT );
						GiveAmmo( 1000, DOD_AMMO_P38 );
						GiveAmmo( 1000, DOD_AMMO_C96 );
						GiveAmmo( 1000, DOD_AMMO_GARAND );
						GiveAmmo( 1000, DOD_AMMO_K98 );
						GiveAmmo( 1000, DOD_AMMO_M1CARBINE );
						GiveAmmo( 1000, DOD_AMMO_SPRING );
						GiveAmmo( 1000, DOD_AMMO_SUBMG );
						GiveAmmo( 1000, DOD_AMMO_BAR );
						GiveAmmo( 1000, DOD_AMMO_30CAL );
						GiveAmmo( 1000, DOD_AMMO_MG42 );
						GiveAmmo( 1000, DOD_AMMO_ROCKET );
						break;
					}
					case FACTION_CS:
					{
						// SEALTODO: MONEY
						//AddAccount( 16000 );

						GiveAmmo( 250, BULLET_PLAYER_50AE );
						GiveAmmo( 250, BULLET_PLAYER_762MM );
						GiveAmmo( 250, BULLET_PLAYER_338MAG );
						GiveAmmo( 250, BULLET_PLAYER_556MM );
						GiveAmmo( 250, BULLET_PLAYER_556MM_BOX );
						GiveAmmo( 250, BULLET_PLAYER_9MM );
						GiveAmmo( 250, BULLET_PLAYER_BUCKSHOT );
						GiveAmmo( 250, BULLET_PLAYER_45ACP );
						GiveAmmo( 250, BULLET_PLAYER_357SIG );
						GiveAmmo( 250, BULLET_PLAYER_57MM );
						break;
					}
					case FACTION_HL2:
					{
						GiveAmmo( 250, HL2_AMMO_AR2 );
						GiveAmmo( 250, HL2_AMMO_AR2ALTFIRE );
						GiveAmmo( 250, HL2_AMMO_PISTOL );
						GiveAmmo( 250, HL2_AMMO_SMG1 );
						GiveAmmo( 250, HL2_AMMO_357 );
						GiveAmmo( 250, HL2_AMMO_XBOWBOLT );
						GiveAmmo( 250, HL2_AMMO_BUCKSHOT );
						GiveAmmo( 250, HL2_AMMO_RPGROUND );
						GiveAmmo( 250, HL2_AMMO_SMG1GRENADE );
						GiveAmmo( 250, HL2_AMMO_GRENADE );
						GiveAmmo( 250, HL2_AMMO_SLAM );
						break;
					}
					default:
					{
						GiveAmmo( 1000, TF_AMMO_PRIMARY );
						GiveAmmo( 1000, TF_AMMO_SECONDARY );
						GiveAmmo( 1000, TF_AMMO_METAL );
						break;
					}
				}

				TakeHealth( 999, DMG_GENERIC );

				gEvilImpulse101 = false;
			}
		}
		break;

	default:
		{
			BaseClass::CheatImpulseCommands( iImpulse );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SetWeaponBuilder( CTFWeaponBuilder *pBuilder )
{
	m_hWeaponBuilder = pBuilder;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFWeaponBuilder *CTFPlayer::GetWeaponBuilder( void )
{
	Assert( 0 );
	return m_hWeaponBuilder;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this player is building something
//-----------------------------------------------------------------------------
bool CTFPlayer::IsBuilding( void )
{
	/*
	CTFWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
		return pBuilder->IsBuilding();
		*/

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveBuildResources( int iAmount )
{
	RemoveAmmo( iAmount, TF_AMMO_METAL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::AddBuildResources( int iAmount )
{
	GiveAmmo( iAmount, TF_AMMO_METAL, false );	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObject	*CTFPlayer::GetObject( int index ) const
{
	return (CBaseObject *)( m_aObjects[index].Get() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObject	*CTFPlayer::GetObjectOfType( int iObjectType, int iObjectMode ) const
{
	int iNumObjects = GetObjectCount();
	for ( int i=0; i<iNumObjects; i++ )
	{
		CBaseObject *pObj = GetObject(i);

		if ( !pObj )
			continue;

		if ( pObj->GetType() != iObjectType )
			continue;

		if ( pObj->GetObjectMode() != iObjectMode )
			continue;

		if ( pObj->IsDisposableBuilding() )
			continue;

		return pObj;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFPlayer::GetObjectCount( void ) const
{
	return m_aObjects.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Remove all the player's objects
//			If bExplodeBuildings is not set, remove all of them immediately.
//			Otherwise, make them all explode.
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveAllObjects( bool bExplodeBuildings /* = false */ )
{
	// Remove all the player's objects
	for (int i = GetObjectCount()-1; i >= 0; i--)
	{
		CBaseObject *obj = GetObject(i);
		Assert( obj );

		if ( obj )
		{
			// this is separate from the object_destroyed event, which does
			// not get sent when we remove the objects from the world
			IGameEvent *event = gameeventmanager->CreateEvent( "object_removed" );	
			if ( event )
			{
				event->SetInt( "userid", GetUserID() ); // user ID of the object owner
				event->SetInt( "objecttype", obj->GetType() ); // type of object removed
				event->SetInt( "index", obj->entindex() ); // index of the object removed
				gameeventmanager->FireEvent( event );
			}

			if ( bExplodeBuildings )
			{
				obj->DetonateObject();
			}
			else
			{
				// This fixes a bug in Raid mode where we could spawn where our sentry was but 
				// we didn't get the weapons because they couldn't trace to us in FVisible
				obj->SetSolid( SOLID_NONE );
				UTIL_Remove( obj );
			}
		}		
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::StopPlacement( void )
{
	/*
	// Tell our builder weapon
	CTFWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->StopPlacement();
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Player has started building an object
//-----------------------------------------------------------------------------
int	CTFPlayer::StartedBuildingObject( int iObjectType )
{
	// Deduct the cost of the object
	int iCost = m_Shared.CalculateObjectCost( this, iObjectType );
	if ( iCost > GetBuildResources() )
	{
		// Player must have lost resources since he started placing
		return 0;
	}

	RemoveBuildResources( iCost );

	// If the object costs 0, we need to return non-0 to mean success
	if ( !iCost )
		return 1;

	return iCost;
}

//-----------------------------------------------------------------------------
// Purpose: Player has aborted building something
//-----------------------------------------------------------------------------
void CTFPlayer::StoppedBuilding( int iObjectType )
{
	/*
	int iCost = CalculateObjectCost( iObjectType );

	AddBuildResources( iCost );

	// Tell our builder weapon
	CTFWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->StoppedBuilding( iObjectType );
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Object has been built by this player
//-----------------------------------------------------------------------------
void CTFPlayer::FinishedObject( CBaseObject *pObject )
{
	AddObject( pObject );

	CTF_GameStats.Event_PlayerCreatedBuilding( this, pObject );

	/*
	// Tell our builder weapon
	CTFWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->FinishedObject();
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Add the specified object to this player's object list.
//-----------------------------------------------------------------------------
void CTFPlayer::AddObject( CBaseObject *pObject )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseTFPlayer::AddObject adding object %p:%s to player %s\n", gpGlobals->curtime, pObject, pObject->GetClassname(), GetPlayerName() ) );

	// Make a handle out of it
	CHandle<CBaseObject> hObject;
	hObject = pObject;

	bool alreadyInList = PlayerOwnsObject( pObject );
	// Assert( !alreadyInList );
	if ( !alreadyInList )
	{
		m_aObjects.AddToTail( hObject );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Object built by this player has been destroyed
//-----------------------------------------------------------------------------
void CTFPlayer::OwnedObjectDestroyed( CBaseObject *pObject )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseTFPlayer::OwnedObjectDestroyed player %s object %p:%s\n", gpGlobals->curtime, 
		GetPlayerName(),
		pObject,
		pObject->GetClassname() ) );

	RemoveObject( pObject );

	// Tell our builder weapon so it recalculates the state of the build icons
	/*
	CTFWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->RecalcState();
	}
	*/
}

//-----------------------------------------------------------------------------
// Removes an object from the player
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveObject( CBaseObject *pObject )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseTFPlayer::RemoveObject %p:%s from player %s\n", gpGlobals->curtime, 
		pObject,
		pObject->GetClassname(),
		GetPlayerName() ) );

	Assert( pObject );

	int i;
	for ( i = m_aObjects.Count(); --i >= 0; )
	{
		// Also, while we're at it, remove all other bogus ones too...
		if ( (!m_aObjects[i].Get()) || (m_aObjects[i] == pObject))
		{
			m_aObjects.FastRemove(i);
		}
	}
}

//-----------------------------------------------------------------------------
// See if the player owns this object
//-----------------------------------------------------------------------------
bool CTFPlayer::PlayerOwnsObject( CBaseObject *pObject )
{
	return ( m_aObjects.Find( pObject ) != -1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PlayFlinch( const CTakeDamageInfo &info )
{
	// Don't play flinches if we just died. 
	if ( !IsAlive() )
		return;

	// No pain flinches while disguised, our man has supreme discipline
	if ( m_Shared.InCond( TF_COND_DISGUISED ) )
		return;

	PlayerAnimEvent_t flinchEvent;

	switch ( LastHitGroup() )
	{
		// pick a region-specific flinch
	case HITGROUP_HEAD:
		flinchEvent = PLAYERANIMEVENT_FLINCH_HEAD;
		break;
	case HITGROUP_LEFTARM:
		flinchEvent = PLAYERANIMEVENT_FLINCH_LEFTARM;
		break;
	case HITGROUP_RIGHTARM:
		flinchEvent = PLAYERANIMEVENT_FLINCH_RIGHTARM;
		break;
	case HITGROUP_LEFTLEG:
		flinchEvent = PLAYERANIMEVENT_FLINCH_LEFTLEG;
		break;
	case HITGROUP_RIGHTLEG:
		flinchEvent = PLAYERANIMEVENT_FLINCH_RIGHTLEG;
		break;
	case HITGROUP_STOMACH:
	case HITGROUP_CHEST:
	case HITGROUP_GEAR:
	case HITGROUP_GENERIC:
	default:
		// just get a generic flinch.
		flinchEvent = PLAYERANIMEVENT_FLINCH_CHEST;
		break;
	}

	DoAnimationEvent( flinchEvent );
}

//-----------------------------------------------------------------------------
// Purpose: Plays the crit sound that players that get crit hear
//-----------------------------------------------------------------------------
float CTFPlayer::PlayCritReceivedSound( void )
{
	float flCritPainLength = 0;
	// Play a custom pain sound to the guy taking the damage
	CSingleUserRecipientFilter receiverfilter( this );
	EmitSound_t params;
	params.m_flSoundTime = 0;
	params.m_pSoundName = "TFPlayer.CritPain";
	params.m_pflSoundDuration = &flCritPainLength;
	EmitSound( receiverfilter, entindex(), params );

	return flCritPainLength;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PainSound( const CTakeDamageInfo &info )
{
	// Don't make sounds if we just died. DeathSound will handle that.
	if ( !IsAlive() )
		return;

	// no pain sounds while disguised, our man has supreme discipline
	if ( m_Shared.InCond( TF_COND_DISGUISED ) )
		return;

	if ( m_flNextPainSoundTime > gpGlobals->curtime )
		return;

	switch ( GetClassType() )
	{
		case FACTION_DOD:
		{
			DODPainSound( info );
			break;
		}
		default:
		{
			TFPainSound( info );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::TFPainSound( const CTakeDamageInfo &info )
{
	// No sound for DMG_GENERIC
	if ( info.GetDamageType() == 0 || info.GetDamageType() == DMG_PREVENT_PHYSICS_FORCE )
		return;

	if ( info.GetDamageType() & DMG_DROWN )
	{
		EmitSound( "TFPlayer.Drown" );
		return;
	}

	if ( info.GetDamageType() & DMG_BURN )
	{
		// Looping fire pain sound is done in CTFPlayerShared::ConditionThink
		return;
	}

	float flPainLength = 0;

	bool bAttackerIsPlayer = ( info.GetAttacker() && info.GetAttacker()->IsPlayer() );

	CMultiplayer_Expresser *pExpresser = GetMultiplayerExpresser();
	Assert( pExpresser );

	pExpresser->AllowMultipleScenes();

	// speak a pain concept here, send to everyone but the attacker
	CPASFilter filter( GetAbsOrigin() );

	if ( bAttackerIsPlayer )
	{
		filter.RemoveRecipient( ToBasePlayer( info.GetAttacker() ) );
	}

	// play a crit sound to the victim ( us )
	if ( info.GetDamageType() & DMG_CRITICAL )
	{
		flPainLength = PlayCritReceivedSound();

		// remove us from hearing our own pain sound if we hear the crit sound
		filter.RemoveRecipient( this );
	}

	char szResponse[AI_Response::MAX_RESPONSE_NAME];

	if ( SpeakConceptIfAllowed( MP_CONCEPT_PLAYER_PAIN, "damagecritical:1", szResponse, AI_Response::MAX_RESPONSE_NAME, &filter ) )
	{
		flPainLength = max( GetSceneDuration( szResponse ), flPainLength );
	}

	// speak a louder pain concept to just the attacker
	if ( bAttackerIsPlayer )
	{
		CSingleUserRecipientFilter attackerFilter( ToBasePlayer( info.GetAttacker() ) );
		SpeakConceptIfAllowed( MP_CONCEPT_PLAYER_ATTACKER_PAIN, "damagecritical:1", szResponse, AI_Response::MAX_RESPONSE_NAME, &attackerFilter );
	}

	pExpresser->DisallowMultipleScenes();

	m_flNextPainSoundTime = gpGlobals->curtime + flPainLength;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DODPainSound( const CTakeDamageInfo &info )
{
	// don't play fall damage sounds
	if ( info.GetDamageType() & DMG_FALL )
		return;

	float flPainLength = 0;

	CMultiplayer_Expresser *pExpresser = GetMultiplayerExpresser();
	Assert( pExpresser );

	pExpresser->AllowMultipleScenes();

	// speak a pain concept here, send to everyone but the attacker
	CPASFilter filter( GetAbsOrigin() );

	// play a crit sound to the victim ( us )
	if ( info.GetDamageType() & DMG_CRITICAL )
	{
		flPainLength = PlayCritReceivedSound();

		// remove us from hearing our own pain sound if we hear the crit sound
		filter.RemoveRecipient( this );
	}

	if ( info.GetDamageType() & DMG_CLUB )
	{
		EmitSound( filter, entindex(), "DODPlayer.MajorPain" );
	}
	else if ( info.GetDamageType() & DMG_BLAST )
	{
		EmitSound( filter, entindex(), "DODPlayer.MajorPain" );
	}
	else
	{
		EmitSound( filter, entindex(), "DODPlayer.MinorPain" );
	}

	pExpresser->DisallowMultipleScenes();

	m_flNextPainSoundTime = gpGlobals->curtime + flPainLength;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DeathSound( const CTakeDamageInfo &info )
{
	// Don't make death sounds when choosing a class
	if ( IsPlayerClass( TF_CLASS_UNDEFINED ) )
		return;

	switch ( GetClassType() )
	{
		case FACTION_DOD:
		{
			DODDeathSound( info );
			break;
		}
		default:
		{
			TFDeathSound( info );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::TFDeathSound( const CTakeDamageInfo &info )
{
	TFPlayerClassData_t *pData = GetPlayerClass()->GetData();
	if ( !pData )
		return;

	if ( m_LastDamageType & DMG_FALL ) // Did we die from falling?
	{
		// They died in the fall. Play a splat sound.
		EmitSound( "Player.FallGib" );
	}
	else if ( m_LastDamageType & DMG_BLAST )
	{
		EmitSound( pData->m_szExplosionDeathSound );
	}
	else if ( m_LastDamageType & DMG_CRITICAL )
	{
		EmitSound( pData->m_szCritDeathSound );

		PlayCritReceivedSound();
	}
	else if ( m_LastDamageType & DMG_CLUB )
	{
		EmitSound( pData->m_szMeleeDeathSound );
	}
	else
	{
		EmitSound( pData->m_szDeathSound );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::DODDeathSound( const CTakeDamageInfo &info )
{
	TFPlayerClassData_t *pData = GetPlayerClass()->GetData();
	if ( !pData )
		return;

	if ( m_LastDamageType & DMG_CLUB )
	{
		EmitSound( "DODPlayer.MegaPain" );
	}
	else if ( m_LastDamageType & DMG_BLAST )
	{
		EmitSound( "DODPlayer.MegaPain" );
	}
	else if ( m_LastHitGroup == HITGROUP_HEAD )	
	{
		EmitSound( "DODPlayer.DeathHeadShot" );
	}
	else
	{
		EmitSound( "DODPlayer.MinorPain" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::StunSound( CTFPlayer* pAttacker, int iStunFlags, int iOldStunFlags )
{
	if ( !IsAlive() )
		return;

	if ( !(iStunFlags & TF_STUN_CONTROLS) && !(iStunFlags & TF_STUN_LOSER_STATE) )
		return;

	if ( (iStunFlags & TF_STUN_BY_TRIGGER) && (iOldStunFlags != 0) )
		return; // Only play stun triggered sounds when not already stunned.

	// Play the stun sound for everyone but the attacker.
	CMultiplayer_Expresser *pExpresser = GetMultiplayerExpresser();
	Assert( pExpresser );

	pExpresser->AllowMultipleScenes();

	float flStunSoundLength = 0;
	EmitSound_t params;
	params.m_flSoundTime = 0;
	if ( iStunFlags & TF_STUN_SPECIAL_SOUND )
	{
		params.m_pSoundName = "TFPlayer.StunImpactRange";
	}
	else if ( (iStunFlags & TF_STUN_LOSER_STATE) && !pAttacker )
	{
		params.m_pSoundName = "Halloween.PlayerScream";
	}
	else
	{
		params.m_pSoundName = "TFPlayer.StunImpact";
	}
	params.m_pflSoundDuration = &flStunSoundLength;

	if ( pAttacker )
	{
		CPASFilter filter( GetAbsOrigin() );
		filter.RemoveRecipient( pAttacker );
		EmitSound( filter, entindex(), params );

		// Play a louder pain sound for the person who got the stun.
		CSingleUserRecipientFilter attackerFilter( pAttacker );
		EmitSound( attackerFilter, pAttacker->entindex(), params );
	}
	else
	{
		EmitSound( params.m_pSoundName );
	}

	pExpresser->DisallowMultipleScenes();

	// Suppress any pain sound that might come right after this stun sound.
	m_flNextPainSoundTime = gpGlobals->curtime + 2.0f;
}

//-----------------------------------------------------------------------------
// Purpose: called when this player burns another player
//-----------------------------------------------------------------------------
void CTFPlayer::OnBurnOther( CTFPlayer *pTFPlayerVictim )
{
#define ACHIEVEMENT_BURN_TIME_WINDOW	30.0f
#define ACHIEVEMENT_BURN_VICTIMS	5
	// add current time we burned another player to head of vector
	m_aBurnOtherTimes.AddToHead( gpGlobals->curtime );

	// remove any burn times that are older than the burn window from the list
	float flTimeDiscard = gpGlobals->curtime - ACHIEVEMENT_BURN_TIME_WINDOW;
	for ( int i = 1; i < m_aBurnOtherTimes.Count(); i++ )
	{
		if ( m_aBurnOtherTimes[i] < flTimeDiscard )
		{
			m_aBurnOtherTimes.RemoveMultiple( i, m_aBurnOtherTimes.Count() - i );
			break;
		}
	}

	// see if we've burned enough players in time window to satisfy achievement
	if ( m_aBurnOtherTimes.Count() >= ACHIEVEMENT_BURN_VICTIMS )
	{
		CSingleUserRecipientFilter filter( this );
		UserMessageBegin( filter, "AchievementEvent" );
		WRITE_BYTE( ACHIEVEMENT_TF_BURN_PLAYERSINMINIMIMTIME );
		MessageEnd();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the player is capturing a point.
//-----------------------------------------------------------------------------
bool CTFPlayer::IsCapturingPoint()
{
	CTriggerAreaCapture *pAreaTrigger = GetControlPointStandingOn();
	if ( pAreaTrigger )
	{
		CTeamControlPoint *pCP = pAreaTrigger->GetControlPoint();
		if ( pCP )
		{
			if ( TeamplayGameRules()->TeamMayCapturePoint( GetTeamNumber(), pCP->GetPointIndex() ) && 
				TeamplayGameRules()->PlayerMayCapturePoint( this, pCP->GetPointIndex() ) )
			{
				// if we own this point, we're no longer "capturing" it
				return pCP->GetOwner() != GetTeamNumber();
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFTeam *CTFPlayer::GetTFTeam( void )
{
	CTFTeam *pTeam = dynamic_cast<CTFTeam *>( GetTeam() );
	Assert( pTeam );
	return pTeam;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFTeam *CTFPlayer::GetOpposingTFTeam( void )
{
	int iTeam = GetTeamNumber();
	if ( iTeam == TF_TEAM_RED )
	{
		return TFTeamMgr()->GetTeam( TF_TEAM_BLUE );
	}
	else
	{
		return TFTeamMgr()->GetTeam( TF_TEAM_RED );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Give this player the "i just teleported" effect for 12 seconds
//-----------------------------------------------------------------------------
void CTFPlayer::TeleportEffect( void )
{
	m_Shared.AddCond( TF_COND_TELEPORTED );

	// Also removed on death
	SetContextThink( &CTFPlayer::RemoveTeleportEffect, gpGlobals->curtime + 12, "TFPlayer_TeleportEffect" );
}

//-----------------------------------------------------------------------------
// Purpose: Remove the teleporter effect
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveTeleportEffect( void )
{
	m_Shared.RemoveCond( TF_COND_TELEPORTED );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::CreateRagdollEntity( void )
{
	CreateRagdollEntity( false, false, false, false, false, false, false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Create a ragdoll entity to pass to the client.
//-----------------------------------------------------------------------------
void CTFPlayer::CreateRagdollEntity( bool bGib, bool bBurning, bool bElectrocuted, bool bOnGround, bool bCloakedCorpse, bool bGoldRagdoll, bool bIceRagdoll, bool bBecomeAsh, int iDamageCustom, bool bCritOnHardHit )
{
	// If we already have a ragdoll destroy it.
	CTFRagdoll *pRagdoll = dynamic_cast<CTFRagdoll*>( m_hRagdoll.Get() );
	if( pRagdoll )
	{
		UTIL_Remove( pRagdoll );
		pRagdoll = NULL;
	}
	Assert( pRagdoll == NULL );

	// Create a ragdoll.
	pRagdoll = dynamic_cast<CTFRagdoll*>( CreateEntityByName( "tf_ragdoll" ) );
	if ( pRagdoll )
	{
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_vecForce = m_vecForce;
		pRagdoll->m_nForceBone = m_nForceBone;
		Assert( entindex() >= 1 && entindex() <= MAX_PLAYERS );
		pRagdoll->m_iPlayerIndex.Set( entindex() );
		pRagdoll->m_bGib = bGib;
		pRagdoll->m_bBurning = bBurning;
		pRagdoll->m_bElectrocuted = bElectrocuted;
		pRagdoll->m_bOnGround = bOnGround;
		pRagdoll->m_bCloaked = bCloakedCorpse;
		pRagdoll->m_iDamageCustom = iDamageCustom;
		pRagdoll->m_iTeam = GetTeamNumber();
		pRagdoll->m_iClass = GetPlayerClass()->GetClassIndex();
		pRagdoll->m_bGoldRagdoll = bGoldRagdoll;
		pRagdoll->m_bIceRagdoll = bIceRagdoll;
		pRagdoll->m_bBecomeAsh = bBecomeAsh;
		pRagdoll->m_bCritOnHardHit = bCritOnHardHit;
		pRagdoll->m_flHeadScale = m_flHeadScale;
		pRagdoll->m_flTorsoScale = m_flTorsoScale;
		pRagdoll->m_flHandScale = m_flHandScale;
	}

	// Turn off the player.
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW | EF_NOSHADOW );
	SetMoveType( MOVETYPE_NONE );

	// Add additional gib setup.
	if ( bGib )
	{
		m_nRenderFX = kRenderFxRagdoll;
	}

	// Save ragdoll handle.
	m_hRagdoll = pRagdoll;
}

// Purpose: Destroy's a ragdoll, called with a player is disconnecting.
//-----------------------------------------------------------------------------
void CTFPlayer::DestroyRagdoll( void )
{
	CTFRagdoll *pRagdoll = dynamic_cast<CTFRagdoll*>( m_hRagdoll.Get() );	
	if( pRagdoll )
	{
		UTIL_Remove( pRagdoll );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Weapon_FrameUpdate( void )
{
	BaseClass::Weapon_FrameUpdate();

	if ( m_hOffHandWeapon.Get() && m_hOffHandWeapon->IsWeaponVisible() )
	{
		m_hOffHandWeapon->Operator_FrameUpdate( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CTFPlayer::Weapon_HandleAnimEvent( animevent_t *pEvent )
{
	BaseClass::Weapon_HandleAnimEvent( pEvent );

	if ( m_hOffHandWeapon.Get() )
	{
		m_hOffHandWeapon->Operator_HandleAnimEvent( pEvent, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CTFPlayer::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget , const Vector *pVelocity ) 
{
	// TF leaves this blank intentionally, however we need it for DOD
	if ( IsDODClass() )
		return BaseClass::Weapon_Drop( pWeapon, pvecTarget, pVelocity );
}

//-----------------------------------------------------------------------------
// Purpose: Remove invisibility, called when player attacks
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveInvisibility( void )
{
	if ( !m_Shared.IsStealthed() )
		return;

	// remove quickly
	CTFPlayer *pProvider = ToTFPlayer( m_Shared.GetConditionProvider( TF_COND_STEALTHED_USER_BUFF ) );
	bool bAEStealth = ( m_Shared.InCond( TF_COND_STEALTHED_USER_BUFF ) && 
						pProvider && 
						( pProvider->IsPlayerClass( TF_CLASS_SPY ) ? true : false ) &&
						( pProvider != this ) );
	if ( m_Shared.InCond( TF_COND_STEALTHED_USER_BUFF ) )
	{
		m_Shared.AddCond( TF_COND_STEALTHED_USER_BUFF_FADING, ( bAEStealth ) ? 4.f : 0.5f );
	}

	m_Shared.FadeInvis( bAEStealth ? 2.f : 0.5f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SaveMe( void )
{
	if ( !IsAlive() || IsPlayerClass( TF_CLASS_UNDEFINED ) || GetTeamNumber() < TF_TEAM_RED )
		return;

	m_bSaveMeParity = !m_bSaveMeParity;
}

//-----------------------------------------------------------------------------
// Purpose: drops the flag
//-----------------------------------------------------------------------------
void CC_DropItem( void )
{
	CTFPlayer *pPlayer = ToTFPlayer( UTIL_GetCommandClient() ); 
	if ( pPlayer )
	{
		pPlayer->DropFlag();
	}
}
static ConCommand dropitem( "dropitem", CC_DropItem, "Drop the flag." );

class CObserverPoint : public CPointEntity
{
	DECLARE_CLASS( CObserverPoint, CPointEntity );
public:
	DECLARE_DATADESC();

	virtual void Activate( void )
	{
		BaseClass::Activate();

		if ( m_iszAssociateTeamEntityName != NULL_STRING )
		{
			m_hAssociatedTeamEntity = gEntList.FindEntityByName( NULL, m_iszAssociateTeamEntityName );
			if ( !m_hAssociatedTeamEntity )
			{
				Warning("info_observer_point (%s) couldn't find associated team entity named '%s'\n", GetDebugName(), STRING(m_iszAssociateTeamEntityName) );
			}
		}
	}

	bool CanUseObserverPoint( CTFPlayer *pPlayer )
	{
		if ( m_bDisabled )
			return false;

		if ( m_hAssociatedTeamEntity && ( mp_forcecamera.GetInt() == OBS_ALLOW_TEAM ) )
		{
			// If we don't own the associated team entity, we can't use this point
			if ( m_hAssociatedTeamEntity->GetTeamNumber() != pPlayer->GetTeamNumber() && pPlayer->GetTeamNumber() >= FIRST_GAME_TEAM )
				return false;
		}

		// Only spectate observer points on control points in the current miniround
		if ( g_pObjectiveResource->PlayingMiniRounds() && m_hAssociatedTeamEntity )
		{
			CTeamControlPoint *pPoint = dynamic_cast<CTeamControlPoint*>(m_hAssociatedTeamEntity.Get());
			if ( pPoint )
			{
				bool bInRound = g_pObjectiveResource->IsInMiniRound( pPoint->GetPointIndex() );
				if ( !bInRound )
					return false;
			}
		}

		return true;
	}

	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	void InputEnable( inputdata_t &inputdata )
	{
		m_bDisabled = false;
	}
	void InputDisable( inputdata_t &inputdata )
	{
		m_bDisabled = true;
	}
	bool IsDefaultWelcome( void ) { return m_bDefaultWelcome; }

public:
	bool		m_bDisabled;
	bool		m_bDefaultWelcome;
	EHANDLE		m_hAssociatedTeamEntity;
	string_t	m_iszAssociateTeamEntityName;
	float		m_flFOV;
};

BEGIN_DATADESC( CObserverPoint )
	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD( m_bDefaultWelcome, FIELD_BOOLEAN, "defaultwelcome" ),
	DEFINE_KEYFIELD( m_iszAssociateTeamEntityName,	FIELD_STRING,	"associated_team_entity" ),
	DEFINE_KEYFIELD( m_flFOV,	FIELD_FLOAT,	"fov" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

LINK_ENTITY_TO_CLASS(info_observer_point,CObserverPoint);

//-----------------------------------------------------------------------------
// Purpose: Builds a list of entities that this player can observe.
//			Returns the index into the list of the player's current observer target.
//-----------------------------------------------------------------------------
int CTFPlayer::BuildObservableEntityList( void )
{
	m_hObservableEntities.Purge();
	int iCurrentIndex = -1;

	// Add all the map-placed observer points
	CBaseEntity *pObserverPoint = gEntList.FindEntityByClassname( NULL, "info_observer_point" );
	while ( pObserverPoint )
	{
		m_hObservableEntities.AddToTail( pObserverPoint );

		if ( m_hObserverTarget.Get() == pObserverPoint )
		{
			iCurrentIndex = (m_hObservableEntities.Count()-1);
		}

		pObserverPoint = gEntList.FindEntityByClassname( pObserverPoint, "info_observer_point" );
	}

	// Add all the players
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
			m_hObservableEntities.AddToTail( pPlayer );

			if ( m_hObserverTarget.Get() == pPlayer )
			{
				iCurrentIndex = (m_hObservableEntities.Count()-1);
			}
		}
	}

	// Add all my objects
	int iNumObjects = GetObjectCount();
	for ( int i = 0; i < iNumObjects; i++ )
	{
		CBaseObject *pObj = GetObject(i);
		if ( pObj )
		{
			m_hObservableEntities.AddToTail( pObj );

			if ( m_hObserverTarget.Get() == pObj )
			{
				iCurrentIndex = (m_hObservableEntities.Count()-1);
			}
		}
	}

	return iCurrentIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayer::GetNextObserverSearchStartPoint( bool bReverse )
{
	int iDir = bReverse ? -1 : 1; 
	int startIndex = BuildObservableEntityList();
	int iMax = m_hObservableEntities.Count()-1;

	startIndex += iDir;
	if (startIndex > iMax)
		startIndex = 0;
	else if (startIndex < 0)
		startIndex = iMax;

	return startIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayer::FindNextObserverTarget(bool bReverse)
{
	int startIndex = GetNextObserverSearchStartPoint( bReverse );

	int	currentIndex = startIndex;
	int iDir = bReverse ? -1 : 1; 

	int iMax = m_hObservableEntities.Count()-1;

	// Make sure the current index is within the max. Can happen if we were previously
	// spectating an object which has been destroyed.
	if ( startIndex > iMax )
	{
		currentIndex = startIndex = 1;
	}

	do
	{
		CBaseEntity *nextTarget = m_hObservableEntities[currentIndex];

		if ( IsValidObserverTarget( nextTarget ) )
			return nextTarget;	
 
		currentIndex += iDir;

		// Loop through the entities
		if (currentIndex > iMax)
		{
			currentIndex = 0;
		}
		else if (currentIndex < 0)
		{
			currentIndex = iMax;
		}
	} while ( currentIndex != startIndex );

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::IsValidObserverTarget(CBaseEntity * target)
{
	if ( target && !target->IsPlayer() )
	{
		CObserverPoint *pObsPoint = dynamic_cast<CObserverPoint *>(target);
		if ( pObsPoint && !pObsPoint->CanUseObserverPoint( this ) )
			return false;
		
		if ( GetTeamNumber() == TEAM_SPECTATOR )
			return true;

		switch ( mp_forcecamera.GetInt() )	
		{
		case OBS_ALLOW_ALL		:	break;
		case OBS_ALLOW_TEAM		:	if (target->GetTeamNumber() != TEAM_UNASSIGNED && GetTeamNumber() != target->GetTeamNumber())
										return false;
									break;
		case OBS_ALLOW_NONE		:	return false;
		}

		return true;
	}

	return BaseClass::IsValidObserverTarget( target );
}


void CTFPlayer::PickWelcomeObserverPoint( void )
{
	//Don't just spawn at the world origin, find a nice spot to look from while we choose our team and class.
	CObserverPoint *pObserverPoint = (CObserverPoint *)gEntList.FindEntityByClassname( NULL, "info_observer_point" );

	while ( pObserverPoint )
	{
		if ( IsValidObserverTarget( pObserverPoint ) )
		{
			SetObserverTarget( pObserverPoint );
		}

		if ( pObserverPoint->IsDefaultWelcome() )
			break;

		pObserverPoint = (CObserverPoint *)gEntList.FindEntityByClassname( pObserverPoint, "info_observer_point" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::SetObserverTarget(CBaseEntity *target)
{
	ClearZoomOwner();
	SetFOV( this, 0 );
		
	if ( !BaseClass::SetObserverTarget(target) )
		return false;

	CObserverPoint *pObsPoint = dynamic_cast<CObserverPoint *>(target);
	if ( pObsPoint )
	{
		SetViewOffset( vec3_origin );
		JumptoPosition( target->GetAbsOrigin(), target->EyeAngles() );
		SetFOV( pObsPoint, pObsPoint->m_flFOV );
	}

	m_flLastAction = gpGlobals->curtime;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Find the nearest team member within the distance of the origin.
//			Favor players who are the same class.
//-----------------------------------------------------------------------------
CBaseEntity *CTFPlayer::FindNearestObservableTarget( Vector vecOrigin, float flMaxDist )
{
	CTeam *pTeam = GetTeam();
	CBaseEntity *pReturnTarget = NULL;
	bool bFoundClass = false;
	float flCurDistSqr = (flMaxDist * flMaxDist);
	int iNumPlayers = pTeam->GetNumPlayers();

	if ( pTeam->GetTeamNumber() == TEAM_SPECTATOR )
	{
		iNumPlayers = gpGlobals->maxClients;
	}


	for ( int i = 0; i < iNumPlayers; i++ )
	{
		CTFPlayer *pPlayer = NULL;

		if ( pTeam->GetTeamNumber() == TEAM_SPECTATOR )
		{
			pPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
		}
		else
		{
			pPlayer = ToTFPlayer( pTeam->GetPlayer(i) );
		}

		if ( !pPlayer )
			continue;

		if ( !IsValidObserverTarget(pPlayer) )
			continue;

		float flDistSqr = ( pPlayer->GetAbsOrigin() - vecOrigin ).LengthSqr();

		if ( flDistSqr < flCurDistSqr )
		{
			// If we've found a player matching our class already, this guy needs
			// to be a matching class and closer to boot.
			if ( !bFoundClass || pPlayer->IsPlayerClass( GetPlayerClass()->GetClassIndex() ) )
			{
				pReturnTarget = pPlayer;
				flCurDistSqr = flDistSqr;

				if ( pPlayer->IsPlayerClass( GetPlayerClass()->GetClassIndex() ) )
				{
					bFoundClass = true;
				}
			}
		}
		else if ( !bFoundClass )
		{
			if ( pPlayer->IsPlayerClass( GetPlayerClass()->GetClassIndex() ) )
			{
				pReturnTarget = pPlayer;
				flCurDistSqr = flDistSqr;
				bFoundClass = true;
			}
		}
	}

	if ( !bFoundClass && IsPlayerClass( TF_CLASS_ENGINEER ) )
	{
		// let's spectate our sentry instead, we didn't find any other engineers to spec
		int iNumObjects = GetObjectCount();
		for ( int i = 0; i < iNumObjects; i++ )
		{
			CBaseObject *pObj = GetObject(i);

			if ( pObj && pObj->GetType() == OBJ_SENTRYGUN )
			{
				pReturnTarget = pObj;
			}
		}
	}		

	return pReturnTarget;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::FindInitialObserverTarget( void )
{
	// If we're on a team (i.e. not a pure observer), try and find
	// a target that'll give the player the most useful information.
	if ( GetTeamNumber() >= FIRST_GAME_TEAM )
	{
		CTeamControlPointMaster *pMaster = g_hControlPointMasters.Count() ? g_hControlPointMasters[0] : NULL;
		if ( pMaster )
		{
			// Has our forward cap point been contested recently?
			int iFarthestPoint = TFGameRules()->GetFarthestOwnedControlPoint( GetTeamNumber(), false );
			if ( iFarthestPoint != -1 )
			{
				float flTime = pMaster->PointLastContestedAt( iFarthestPoint );
				if ( flTime != -1 && flTime > (gpGlobals->curtime - 30) )
				{
					// Does it have an associated viewpoint?
					CBaseEntity *pObserverPoint = gEntList.FindEntityByClassname( NULL, "info_observer_point" );
					while ( pObserverPoint )
					{
						CObserverPoint *pObsPoint = assert_cast<CObserverPoint *>(pObserverPoint);
						if ( pObsPoint && pObsPoint->m_hAssociatedTeamEntity == pMaster->GetControlPoint(iFarthestPoint) )
						{
							if ( IsValidObserverTarget( pObsPoint ) )
							{
								m_hObserverTarget.Set( pObsPoint );
								return;
							}
						}

						pObserverPoint = gEntList.FindEntityByClassname( pObserverPoint, "info_observer_point" );
					}
				}
			}

			// Has the point beyond our farthest been contested lately?
			iFarthestPoint += (ObjectiveResource()->GetBaseControlPointForTeam( GetTeamNumber() ) == 0 ? 1 : -1);
			if ( iFarthestPoint >= 0 && iFarthestPoint < MAX_CONTROL_POINTS )
			{
				float flTime = pMaster->PointLastContestedAt( iFarthestPoint );
				if ( flTime != -1 && flTime > (gpGlobals->curtime - 30) )
				{
					// Try and find a player near that cap point
					CBaseEntity *pCapPoint = pMaster->GetControlPoint(iFarthestPoint);
					if ( pCapPoint )
					{
						CBaseEntity *pTarget = FindNearestObservableTarget( pCapPoint->GetAbsOrigin(), 1500 );
						if ( pTarget )
						{
							m_hObserverTarget.Set( pTarget );
							return;
						}
					}
				}
			}
		}
	}

	// Find the nearest guy near myself
	CBaseEntity *pTarget = FindNearestObservableTarget( GetAbsOrigin(), FLT_MAX );
	if ( pTarget )
	{
		m_hObserverTarget.Set( pTarget );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ValidateCurrentObserverTarget( void )
{
	// If our current target is a dead player who's gibbed / died, refind as if 
	// we were finding our initial target, so we end up somewhere useful.
	if ( m_hObserverTarget && m_hObserverTarget->IsPlayer() )
	{
		CBasePlayer *player = ToBasePlayer( m_hObserverTarget );

		if ( player->m_lifeState == LIFE_DEAD || player->m_lifeState == LIFE_DYING )
		{
			// Once we're past the pause after death, find a new target
			if ( (player->GetDeathTime() + DEATH_ANIMATION_TIME ) < gpGlobals->curtime )
			{
				FindInitialObserverTarget();
			}

			return;
		}
	}

	if ( m_hObserverTarget && m_hObserverTarget->IsBaseObject() )
	{
		if ( m_iObserverMode == OBS_MODE_IN_EYE )
		{
			ForceObserverMode( OBS_MODE_CHASE );
		}
	}

	BaseClass::ValidateCurrentObserverTarget();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Touch( CBaseEntity *pOther )
{
	CTFPlayer *pPlayer = ToTFPlayer( pOther );

	if ( pPlayer )
	{
		CheckUncoveringSpies( pPlayer );
	}

	BaseClass::Touch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if this player has seen through an enemy spy's disguise
//-----------------------------------------------------------------------------
void CTFPlayer::CheckUncoveringSpies( CTFPlayer *pTouchedPlayer )
{
	// Only uncover enemies
	if ( m_Shared.IsAlly( pTouchedPlayer ) )
	{
		return;
	}

	// Only uncover if they're stealthed
	if ( !pTouchedPlayer->m_Shared.InCond( TF_COND_STEALTHED ) )
	{
		return;
	}

	// pulse their invisibility
	pTouchedPlayer->m_Shared.OnSpyTouchedByEnemy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::Taunt( void )
{
	if ( !IsAllowedToTaunt() )
		return;

	// Allow voice commands, etc to be interrupted.
	CMultiplayer_Expresser *pExpresser = GetMultiplayerExpresser();
	Assert( pExpresser );
	pExpresser->AllowMultipleScenes();

	m_bInitTaunt = true;
	char szResponse[AI_Response::MAX_RESPONSE_NAME];
	if ( SpeakConceptIfAllowed( MP_CONCEPT_PLAYER_TAUNT, NULL, szResponse, AI_Response::MAX_RESPONSE_NAME ) )
	{
		// Get the duration of the scene.
		float flDuration = GetSceneDuration( szResponse ) + 0.2f;

		// Set player state as taunting.
		m_flTauntStartTime = gpGlobals->curtime;
		m_Shared.AddCond( TF_COND_TAUNTING );

		m_flTauntRemoveTime = gpGlobals->curtime + flDuration;
		m_angTauntCamera = EyeAngles();

		// Slam velocity to zero.
		SetAbsVelocity( vec3_origin );

		// set initial taunt yaw to make sure that the client anim not off because of lag
		SetTauntYaw( GetAbsAngles()[YAW] );

//		m_vecTauntStartPosition = GetAbsOrigin();
	}

	pExpresser->DisallowMultipleScenes();
}

//-----------------------------------------------------------------------------
// Purpose: Aborts a taunt in progress.
//-----------------------------------------------------------------------------
void CTFPlayer::CancelTaunt( void )
{
	StopTaunt();
}

//-----------------------------------------------------------------------------
// Purpose: Stops taunting
//-----------------------------------------------------------------------------
void CTFPlayer::StopTaunt( void )
{
	if ( m_hTauntScene.Get() )
	{
		StopScriptedScene( this, m_hTauntScene );
		m_flTauntRemoveTime = 0.0f;
		m_bAllowedToRemoveTaunt = true;
		m_hTauntScene = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play a one-shot scene
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CTFPlayer::PlayScene( const char *pszScene, float flDelay, AI_Response *response, IRecipientFilter *filter )
{
	// This is a lame way to detect a taunt!
	if ( m_bInitTaunt )
	{
		m_bInitTaunt = false;
		return InstancedScriptedScene( this, pszScene, &m_hTauntScene, flDelay, false, response, true, filter );
	}
	else
	{
		return InstancedScriptedScene( this, pszScene, NULL, flDelay, false, response, true, filter );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet )
{
	BaseClass::ModifyOrAppendCriteria( criteriaSet );

	// If we have 'disguiseclass' criteria, pretend that we are actually our
	// disguise class. That way we just look up the scene we would play as if 
	// we were that class.
	int disguiseIndex = criteriaSet.FindCriterionIndex( "disguiseclass" );

	if ( disguiseIndex != -1 )
	{
		criteriaSet.AppendCriteria( "playerclass", criteriaSet.GetValue(disguiseIndex) );
	}
	else
	{
		if ( GetPlayerClass() )
		{
			criteriaSet.AppendCriteria( "playerclass", g_aPlayerClassNames_NonLocalized[ GetPlayerClass()->GetClassIndex() ] );
		}
	}

	bool bRedTeam = ( GetTeamNumber() == TF_TEAM_RED );
	if ( m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		bRedTeam = ( m_Shared.GetDisguiseTeam() == TF_TEAM_RED );
	}
	criteriaSet.AppendCriteria( "OnRedTeam", bRedTeam ? "1" : "0" );

	criteriaSet.AppendCriteria( "recentkills", UTIL_VarArgs("%d", m_Shared.GetNumKillsInTime(30.0)) );

	int iTotalKills = 0;
	PlayerStats_t *pStats = CTF_GameStats.FindPlayerStats( this );
	if ( pStats )
	{
		iTotalKills = pStats->statsCurrentLife.m_iStat[TFSTAT_KILLS] + pStats->statsCurrentLife.m_iStat[TFSTAT_KILLASSISTS]+ 
			pStats->statsCurrentLife.m_iStat[TFSTAT_BUILDINGSDESTROYED];
	}
	criteriaSet.AppendCriteria( "killsthislife", UTIL_VarArgs( "%d", iTotalKills ) );
	criteriaSet.AppendCriteria( "disguised", m_Shared.InCond( TF_COND_DISGUISED ) ? "1" : "0" );
	criteriaSet.AppendCriteria( "invulnerable", m_Shared.InCond( TF_COND_INVULNERABLE ) ? "1" : "0" );
	criteriaSet.AppendCriteria( "beinghealed", m_Shared.InCond( TF_COND_HEALTH_BUFF ) ? "1" : "0" );
	criteriaSet.AppendCriteria( "waitingforplayers", (TFGameRules()->IsInWaitingForPlayers() || TFGameRules()->IsInPreMatch()) ? "1" : "0" );

	criteriaSet.AppendCriteria( "stunned", m_Shared.IsControlStunned() ? "1" : "0" );
	criteriaSet.AppendCriteria( "snared",  m_Shared.IsSnared() ? "1" : "0" );
	criteriaSet.AppendCriteria( "dodging",  (m_Shared.InCond( TF_COND_PHASE ) || m_Shared.InCond( TF_COND_PASSTIME_INTERCEPTION )) ? "1" : "0" );
	criteriaSet.AppendCriteria( "doublejumping", (m_Shared.GetAirDash()>0) ? "1" : "0" );

	switch ( GetTFTeam()->GetRole() )
	{
	case TEAM_ROLE_DEFENDERS:
		criteriaSet.AppendCriteria( "teamrole", "defense" );
		break;
	case TEAM_ROLE_ATTACKERS:
		criteriaSet.AppendCriteria( "teamrole", "offense" );
		break;
	}

	// Current weapon role
	CTFWeaponBase *pActiveWeapon = dynamic_cast<CTFWeaponBase*>( m_Shared.GetActivePCWeapon() );
	if ( pActiveWeapon )
	{
		int iWeaponRole = pActiveWeapon->GetPCWpnData().m_ExtraTFWeaponData.m_iWeaponType;
		switch( iWeaponRole )
		{
		case TF_WPN_TYPE_PRIMARY:
		default:
			criteriaSet.AppendCriteria( "weaponmode", "primary" );
			break;
		case TF_WPN_TYPE_SECONDARY:
			criteriaSet.AppendCriteria( "weaponmode", "secondary" );
			break;
		case TF_WPN_TYPE_MELEE:
			criteriaSet.AppendCriteria( "weaponmode", "melee" );
			break;
		case TF_WPN_TYPE_BUILDING:
			criteriaSet.AppendCriteria( "weaponmode", "building" );
			break;
		case TF_WPN_TYPE_PDA:
			criteriaSet.AppendCriteria( "weaponmode", "pda" );
			break;
		case TF_WPN_TYPE_ITEM1:
			criteriaSet.AppendCriteria( "weaponmode", "item1" );
			break;
		case TF_WPN_TYPE_ITEM2:
			criteriaSet.AppendCriteria( "weaponmode", "item2" );
			break;
		}

		if ( WeaponID_IsSniperRifle( pActiveWeapon->GetWeaponID() ) )
		{
			CTFSniperRifle *pRifle = dynamic_cast<CTFSniperRifle*>(pActiveWeapon);
			if ( pRifle && pRifle->IsZoomed() )
			{
				criteriaSet.AppendCriteria( "sniperzoomed", "1" );
			}
		}
		else if ( pActiveWeapon->GetWeaponID() == TF_WEAPON_MINIGUN )
		{
			CTFMinigun *pMinigun = dynamic_cast<CTFMinigun*>(pActiveWeapon);
			if ( pMinigun )
			{
				criteriaSet.AppendCriteria( "minigunfiretime", UTIL_VarArgs("%.1f", pMinigun->GetFiringTime() ) );
			}
		}
	}

	// Player under crosshair
	trace_t tr;
	Vector forward;
	EyeVectors( &forward );
	UTIL_TraceLine( EyePosition(), EyePosition() + (forward * MAX_TRACE_LENGTH), MASK_BLOCKLOS_AND_NPCS, this, COLLISION_GROUP_NONE, &tr );
	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		CBaseEntity *pEntity = tr.m_pEnt;
		if ( pEntity && pEntity->IsPlayer() )
		{
			CTFPlayer *pTFPlayer = ToTFPlayer(pEntity);
			if ( pTFPlayer )
			{
				int iClass = pTFPlayer->GetPlayerClass()->GetClassIndex();
				if ( !InSameTeam(pTFPlayer) )
				{
					// Prevent spotting stealthed enemies who haven't been exposed recently
					if ( pTFPlayer->m_Shared.InCond( TF_COND_STEALTHED ) )
					{
						if ( pTFPlayer->m_Shared.GetLastStealthExposedTime() < (gpGlobals->curtime - 3.0) )
						{
							iClass = TF_CLASS_UNDEFINED;
						}
						else
						{
							iClass = TF_CLASS_SPY;
						}
					}
					else if ( pTFPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
					{
						iClass = pTFPlayer->m_Shared.GetDisguiseClass();
					}
				}

				if ( iClass > TF_CLASS_UNDEFINED && iClass <= TF_LAST_NORMAL_CLASS )
				{
					criteriaSet.AppendCriteria( "crosshair_on", g_aPlayerClassNames_NonLocalized[iClass] );

					int iVisibleTeam = pTFPlayer->GetTeamNumber();
					if ( pTFPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
					{
						iVisibleTeam = pTFPlayer->m_Shared.GetDisguiseTeam();
					}

					if ( iVisibleTeam != GetTeamNumber() )
					{
						criteriaSet.AppendCriteria( "crosshair_enemy", "yes" );
					}
				}
			}
		}
	}

	// Previous round win
	bool bLoser = ( TFGameRules()->GetPreviousRoundWinners() != TEAM_UNASSIGNED && TFGameRules()->GetPreviousRoundWinners() != GetTeamNumber() );
	criteriaSet.AppendCriteria( "LostRound", UTIL_VarArgs("%d", bLoser) );

	bool bPrevRoundTie = ( ( TFGameRules()->GetRoundsPlayed() > 0 ) && ( TFGameRules()->GetPreviousRoundWinners() == TEAM_UNASSIGNED ) );
	criteriaSet.AppendCriteria( "PrevRoundWasTie", bPrevRoundTie ? "1" : "0" );

	// Control points
	CTriggerAreaCapture *pAreaTrigger = GetControlPointStandingOn();
	if ( pAreaTrigger )
	{
		CTeamControlPoint *pCP = pAreaTrigger->GetControlPoint();
		if ( pCP )
		{
			if ( pCP->GetOwner() == GetTeamNumber() )
			{
				criteriaSet.AppendCriteria( "OnFriendlyControlPoint", "1" );
			}
			else 
			{
				if ( TeamplayGameRules()->TeamMayCapturePoint( GetTeamNumber(), pCP->GetPointIndex() ) && 
					 TeamplayGameRules()->PlayerMayCapturePoint( this, pCP->GetPointIndex() ) )
				{
					criteriaSet.AppendCriteria( "OnCappableControlPoint", "1" );
				}
			}
		}
	}

	bool bIsBonusTime = false;
	bool bGameOver = false;

	// Current game state
	criteriaSet.AppendCriteria( "GameRound", UTIL_VarArgs( "%d", TFGameRules()->State_Get() ) ); 
	if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN )
	{
		criteriaSet.AppendCriteria( "OnWinningTeam", ( TFGameRules()->GetWinningTeam() == GetTeamNumber() ) ? "1" : "0" ); 

		bIsBonusTime = ( TFGameRules()->GetStateTransitionTime() > gpGlobals->curtime );
		bGameOver = TFGameRules()->IsGameOver();
	}

	// Number of rounds played
	criteriaSet.AppendCriteria( "RoundsPlayed", UTIL_VarArgs( "%d", TFGameRules()->GetRoundsPlayed() ) );

	// Force the thriller taunt if we have the thriller condition
	if( m_Shared.InCond( TF_COND_HALLOWEEN_THRILLER ) )
	{
		criteriaSet.AppendCriteria( "IsHalloweenTaunt", "1" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTriggerAreaCapture *CTFPlayer::GetControlPointStandingOn( void )
{
	touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
	if ( root )
	{
		for ( touchlink_t *link = root->nextLink; link != root; link = link->nextLink )
		{
			CBaseEntity *pTouch = link->entityTouched;
			if ( pTouch && pTouch->IsSolidFlagSet( FSOLID_TRIGGER ) && pTouch->IsBSPModel() )
			{
				CTriggerAreaCapture *pAreaTrigger = dynamic_cast<CTriggerAreaCapture*>(pTouch);
				if ( pAreaTrigger )
					return pAreaTrigger;
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::CanHearAndReadChatFrom( CBasePlayer *pPlayer )
{
	// can always hear the console unless we're ignoring all chat
	if ( !pPlayer )
		return m_iIgnoreGlobalChat != CHAT_IGNORE_ALL;

	// check if we're ignoring all chat
	if ( m_iIgnoreGlobalChat == CHAT_IGNORE_ALL )
		return false;

	// check if we're ignoring all but teammates
	if ( m_iIgnoreGlobalChat == CHAT_IGNORE_TEAM && g_pGameRules->PlayerRelationship( this, pPlayer ) != GR_TEAMMATE )
		return false;

	if ( !pPlayer->IsAlive() && IsAlive() )
	{
		// Everyone can chat like normal when the round/game ends
		if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN || TFGameRules()->State_Get() == GR_STATE_GAME_OVER )
			return true;

		// Separate rule for spectators.
		if ( pPlayer->GetTeamNumber() < FIRST_GAME_TEAM )
			return tf_spectalk.GetBool();

		// Living players can't hear dead ones unless gravetalk is enabled.
		return tf_gravetalk.GetBool();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IResponseSystem *CTFPlayer::GetResponseSystem()
{
	int iClass = GetPlayerClass()->GetClassIndex();

	if ( m_bSpeakingConceptAsDisguisedSpy && m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		iClass = m_Shared.GetDisguiseClass();
	}

	bool bValidClass = ( iClass >= TF_CLASS_SCOUT && iClass <= TF_LAST_NORMAL_CLASS );
	bool bValidConcept = ( m_iCurrentConcept >= 0 && m_iCurrentConcept < MP_TF_CONCEPT_COUNT );
	Assert( bValidClass );
	Assert( bValidConcept );

	if ( !bValidClass || !bValidConcept )
	{
		return BaseClass::GetResponseSystem();
	}
	else
	{
		return TFGameRules()->m_ResponseRules[iClass].m_ResponseSystems[m_iCurrentConcept];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::SpeakConceptIfAllowed( int iConcept, const char *modifiers, char *pszOutResponseChosen, size_t bufsize, IRecipientFilter *filter )
{
	if ( !IsAlive() )
		return false;

	bool bReturn = false;

	if ( IsSpeaking() )
	{
		if ( iConcept != MP_CONCEPT_DIED )
			return false;
	}

	// Save the current concept.
	m_iCurrentConcept = iConcept;

	if ( m_Shared.InCond( TF_COND_DISGUISED ) && !filter && ( iConcept != MP_CONCEPT_KILLED_PLAYER ) )
	{
		CSingleUserRecipientFilter filter(this);

		int iEnemyTeam = ( GetTeamNumber() == TF_TEAM_RED ) ? TF_TEAM_BLUE : TF_TEAM_RED;

		// test, enemies and myself
		CTeamRecipientFilter disguisedFilter( iEnemyTeam );
		disguisedFilter.AddRecipient( this );

		CMultiplayer_Expresser *pExpresser = GetMultiplayerExpresser();
		Assert( pExpresser );

		pExpresser->AllowMultipleScenes();

		// play disguised concept to enemies and myself
		char buf[128];
		Q_snprintf( buf, sizeof(buf), "disguiseclass:%s", g_aPlayerClassNames_NonLocalized[ m_Shared.GetDisguiseClass() ] );

		if ( modifiers )
		{
			Q_strncat( buf, ",", sizeof(buf), 1 );
			Q_strncat( buf, modifiers, sizeof(buf), COPY_ALL_CHARACTERS );
		}

		m_bSpeakingConceptAsDisguisedSpy = true;

		bool bPlayedDisguised = SpeakIfAllowed( g_pszMPConcepts[iConcept], buf, pszOutResponseChosen, bufsize, &disguisedFilter );

		m_bSpeakingConceptAsDisguisedSpy = false;

		// test, everyone except enemies and myself
		CBroadcastRecipientFilter undisguisedFilter;
		undisguisedFilter.RemoveRecipientsByTeam( GetGlobalTFTeam(iEnemyTeam) );
		undisguisedFilter.RemoveRecipient( this );

		// play normal concept to teammates
		bool bPlayedNormally = SpeakIfAllowed( g_pszMPConcepts[iConcept], modifiers, pszOutResponseChosen, bufsize, &undisguisedFilter );

		pExpresser->DisallowMultipleScenes();

		bReturn = ( bPlayedDisguised || bPlayedNormally );
	}
	else
	{
		// play normally
		bReturn = SpeakIfAllowed( g_pszMPConcepts[iConcept], modifiers, pszOutResponseChosen, bufsize, filter );
	}

	//Add bubble on top of a player calling for medic.
	if ( bReturn )
	{
		if ( iConcept == MP_CONCEPT_PLAYER_MEDIC )
		{
			SaveMe();
		}
	}

	return bReturn;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::UpdateExpression( void )
{
	char szScene[ MAX_PATH ];
	if ( !GetResponseSceneFromConcept( MP_CONCEPT_PLAYER_EXPRESSION, szScene, sizeof( szScene ) ) )
	{
		ClearExpression();
		m_flNextRandomExpressionTime = gpGlobals->curtime + RandomFloat(30,40);
		return;
	}
	
	// Ignore updates that choose the same scene
	if ( m_iszExpressionScene != NULL_STRING && stricmp( STRING(m_iszExpressionScene), szScene ) == 0 )
		return;

	if ( m_hExpressionSceneEnt )
	{
		ClearExpression();
	}

	m_iszExpressionScene = AllocPooledString( szScene );
	float flDuration = InstancedScriptedScene( this, szScene, &m_hExpressionSceneEnt, 0.0, true, NULL, true );
	m_flNextRandomExpressionTime = gpGlobals->curtime + flDuration;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ClearExpression( void )
{
	if ( m_hExpressionSceneEnt != NULL )
	{
		StopScriptedScene( this, m_hExpressionSceneEnt );
	}
	m_flNextRandomExpressionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Only show subtitle to enemy if we're disguised as the enemy
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldShowVoiceSubtitleToEnemy( void )
{
	return ( m_Shared.InCond( TF_COND_DISGUISED ) && m_Shared.GetDisguiseTeam() != GetTeamNumber() );
}

//-----------------------------------------------------------------------------
// Purpose: Don't allow rapid-fire voice commands
//-----------------------------------------------------------------------------
bool CTFPlayer::CanSpeakVoiceCommand( void )
{
	return ( gpGlobals->curtime > m_flNextVoiceCommandTime );
}

//-----------------------------------------------------------------------------
// Purpose: Note the time we're allowed to next speak a voice command
//-----------------------------------------------------------------------------
void CTFPlayer::NoteSpokeVoiceCommand( const char *pszScenePlayed )
{
	Assert( pszScenePlayed );
	m_flNextVoiceCommandTime = gpGlobals->curtime + min( GetSceneDuration( pszScenePlayed ), tf_max_voice_speak_delay.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::WantsLagCompensationOnEntity( const CBasePlayer *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const
{
	bool bIsMedic = false;

	//Do Lag comp on medics trying to heal team mates.
	if ( IsPlayerClass( TF_CLASS_MEDIC ) == true )
	{
		bIsMedic = true;

		if ( pPlayer->GetTeamNumber() == GetTeamNumber()  )
		{
			CWeaponMedigun *pWeapon = dynamic_cast <CWeaponMedigun*>( GetActiveWeapon() );

			if ( pWeapon && pWeapon->GetHealTarget() )
			{
				if ( pWeapon->GetHealTarget() == pPlayer )
					return true;
				else
					return false;
			}
		}
	}

	if ( pPlayer->GetTeamNumber() == GetTeamNumber() && bIsMedic == false )
		return false;
	
	// If this entity hasn't been transmitted to us and acked, then don't bother lag compensating it.
	if ( pEntityTransmitBits && !pEntityTransmitBits->Get( pPlayer->entindex() ) )
		return false;

	const Vector &vMyOrigin = GetAbsOrigin();
	const Vector &vHisOrigin = pPlayer->GetAbsOrigin();

	// get max distance player could have moved within max lag compensation time, 
	// multiply by 1.5 to to avoid "dead zones"  (sqrt(2) would be the exact value)
	float maxDistance = 1.5 * pPlayer->MaxSpeed() * sv_maxunlag.GetFloat();

	// If the player is within this distance, lag compensate them in case they're running past us.
	if ( vHisOrigin.DistTo( vMyOrigin ) < maxDistance )
		return true;

	// If their origin is not within a 45 degree cone in front of us, no need to lag compensate.
	Vector vForward;
	AngleVectors( pCmd->viewangles, &vForward );

	Vector vDiff = vHisOrigin - vMyOrigin;
	VectorNormalize( vDiff );

	float flCosAngle = 0.707107f;	// 45 degree angle
	if ( vForward.Dot( vDiff ) < flCosAngle )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SpeakWeaponFire( int iCustomConcept )
{
	if ( iCustomConcept == MP_CONCEPT_NONE )
	{
		if ( m_flNextSpeakWeaponFire > gpGlobals->curtime )
			return;

		iCustomConcept = MP_CONCEPT_FIREWEAPON;
	}

	m_flNextSpeakWeaponFire = gpGlobals->curtime + 5;

	// Don't play a weapon fire scene if we already have one
	if ( m_hWeaponFireSceneEnt )
		return;

	char szScene[ MAX_PATH ];
	if ( !GetResponseSceneFromConcept( iCustomConcept, szScene, sizeof( szScene ) ) )
		return;

	float flDuration = InstancedScriptedScene( this, szScene, &m_hExpressionSceneEnt, 0.0, true, NULL, true );
	m_flNextSpeakWeaponFire = gpGlobals->curtime + flDuration;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ClearWeaponFireScene( void )
{
	if ( m_hWeaponFireSceneEnt )
	{
		StopScriptedScene( this, m_hWeaponFireSceneEnt );
		m_hWeaponFireSceneEnt = NULL;
	}
	m_flNextSpeakWeaponFire = gpGlobals->curtime;
}

int CTFPlayer::DrawDebugTextOverlays(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		Q_snprintf( tempstr, sizeof( tempstr ),"Health: %d / %d ( %.1f )", GetHealth(), GetMaxHealth(), (float)GetHealth() / (float)GetMaxHealth() );
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: Get response scene corresponding to concept
//-----------------------------------------------------------------------------
bool CTFPlayer::GetResponseSceneFromConcept( int iConcept, char *chSceneBuffer, int numSceneBufferBytes )
{
	AI_Response response;
	bool bResult = SpeakConcept( response, iConcept );

	if ( bResult )
	{
		if ( response.IsApplyContextToWorld() )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance( engine->PEntityOfEntIndex( 0 ) );
			if ( pEntity )
			{
				pEntity->AddContext( response.GetContext() );
			}
		}
		else
		{
			AddContext( response.GetContext() );
		}

		V_strncpy( chSceneBuffer, response.GetResponsePtr(), numSceneBufferBytes );
	}

	return bResult;
}


//-----------------------------------------------------------------------------
// Purpose:calculate a score for this player. higher is more likely to be switched
//-----------------------------------------------------------------------------
int	CTFPlayer::CalculateTeamBalanceScore( void )
{
	int iScore = BaseClass::CalculateTeamBalanceScore();

	// switch engineers less often
	if ( IsPlayerClass( TF_CLASS_ENGINEER ) )
	{
		iScore -= 120;
	}

	return iScore;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// Debugging Stuff
extern CBaseEntity *FindPickerEntity( CBasePlayer *pPlayer );
void DebugParticles( const CCommand &args )
{
	CBaseEntity *pEntity = FindPickerEntity( UTIL_GetCommandClient() );

	if ( pEntity && pEntity->IsPlayer() )
	{
		CTFPlayer *pPlayer = ToTFPlayer( pEntity );

		// print out their conditions
		pPlayer->m_Shared.DebugPrintConditions();	
	}
}

static ConCommand sv_debug_stuck_particles( "sv_debug_stuck_particles", DebugParticles, "Debugs particles attached to the player under your crosshair.", FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose: Debug concommand to set the player on fire
//-----------------------------------------------------------------------------
void IgnitePlayer()
{
	CTFPlayer *pPlayer = ToTFPlayer( ToTFPlayer( UTIL_PlayerByIndex( 1 ) ) );
	pPlayer->m_Shared.Burn( pPlayer );
}
static ConCommand cc_IgnitePlayer( "tf_ignite_player", IgnitePlayer, "Sets you on fire", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TestVCD( const CCommand &args )
{
	CBaseEntity *pEntity = FindPickerEntity( UTIL_GetCommandClient() );
	if ( pEntity && pEntity->IsPlayer() )
	{
		CTFPlayer *pPlayer = ToTFPlayer( pEntity );
		if ( pPlayer )
		{
			if ( args.ArgC() >= 2 )
			{
				InstancedScriptedScene( pPlayer, args[1], NULL, 0.0f, false, NULL, true );
			}
			else
			{
				InstancedScriptedScene( pPlayer, "scenes/heavy_test.vcd", NULL, 0.0f, false, NULL, true );
			}
		}
	}
}
static ConCommand tf_testvcd( "tf_testvcd", TestVCD, "Run a vcd on the player currently under your crosshair. Optional parameter is the .vcd name (default is 'scenes/heavy_test.vcd')", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TestRR( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Msg("No concept specified. Format is tf_testrr <concept>\n");
		return;
	}

	CBaseEntity *pEntity = NULL;
	const char *pszConcept = args[1];

	if ( args.ArgC() == 3 )
	{
		pszConcept = args[2];
		pEntity = UTIL_PlayerByName( args[1] );
	}

	if ( !pEntity || !pEntity->IsPlayer() )
	{
		pEntity = FindPickerEntity( UTIL_GetCommandClient() );
		if ( !pEntity || !pEntity->IsPlayer() )
		{
			pEntity = ToTFPlayer( UTIL_GetCommandClient() ); 
		}
	}

	if ( pEntity && pEntity->IsPlayer() )
	{
		CTFPlayer *pPlayer = ToTFPlayer( pEntity );
		if ( pPlayer )
		{
			int iConcept = GetMPConceptIndexFromString( pszConcept );
			if ( iConcept != MP_CONCEPT_NONE )
			{
				pPlayer->SpeakConceptIfAllowed( iConcept );
			}
			else
			{
				Msg( "Attempted to speak unknown multiplayer concept: %s\n", pszConcept );
			}
		}
	}
}
static ConCommand tf_testrr( "tf_testrr", TestRR, "Force the player under your crosshair to speak a response rule concept. Format is tf_testrr <concept>, or tf_testrr <player name> <concept>", FCVAR_CHEAT );


CON_COMMAND_F( tf_crashclients, "testing only, crashes about 50 percent of the connected clients.", FCVAR_DEVELOPMENTONLY )
{
	for ( int i = 1; i < gpGlobals->maxClients; ++i )
	{
		if ( RandomFloat( 0.0f, 1.0f ) < 0.5f )
		{
			CBasePlayer *pl = UTIL_PlayerByIndex( i + 1 );
			if ( pl )
			{
				engine->ClientCommand( pl->edict(), "crash\n" );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::SetPowerplayEnabled( bool bOn )
{
	if ( bOn )
	{
		m_flPowerPlayTime = gpGlobals->curtime + 99999;
		m_Shared.RecalculateInvuln();
		m_Shared.Burn( this );

		PowerplayThink();
	}
	else
	{
		m_flPowerPlayTime = 0.0;
		m_Shared.RemoveCond( TF_COND_BURNING );
		m_Shared.RecalculateInvuln();
	}
	return true;
}

uint64 powerplaymask = 0xAAB9124CFFA517BA;
uint64 powerplay_ids[] =
{
	76561198082283950 ^ powerplaymask, // Seal
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::PlayerHasPowerplay( void )
{
	if ( !engine->IsClientFullyAuthenticated( edict() ) )
		return false;

	player_info_t pi;
	if ( engine->GetPlayerInfo( entindex(), &pi ) && ( pi.friendsID ) )
	{
		CSteamID steamIDForPlayer( pi.friendsID, 1, k_EUniversePublic, k_EAccountTypeIndividual );
		for ( int i = 0; i < ARRAYSIZE(powerplay_ids); i++ )
		{
			if ( steamIDForPlayer.ConvertToUint64() == (powerplay_ids[i] ^ powerplaymask) )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::PowerplayThink( void )
{
	if ( m_flPowerPlayTime > gpGlobals->curtime )
	{
		float flDuration = 0;
		if ( GetPlayerClass() )
		{
			switch ( GetPlayerClass()->GetClassIndex() )
			{
			case TF_CLASS_SCOUT: flDuration = InstancedScriptedScene( this, "scenes/player/scout/low/laughlong02.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_SNIPER: flDuration = InstancedScriptedScene( this, "scenes/player/sniper/low/laughlong01.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_SOLDIER: flDuration = InstancedScriptedScene( this, "scenes/player/soldier/low/laughevil02.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_DEMOMAN: flDuration = InstancedScriptedScene( this, "scenes/player/demoman/low/laughlong02.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_MEDIC: flDuration = InstancedScriptedScene( this, "scenes/player/medic/low/laughlong02.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_HEAVYWEAPONS: flDuration = InstancedScriptedScene( this, "scenes/player/heavy/low/laughlong01.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_PYRO: flDuration = InstancedScriptedScene( this, "scenes/player/pyro/low/laughlong01.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_SPY: flDuration = InstancedScriptedScene( this, "scenes/player/spy/low/laughevil01.vcd", NULL, 0.0f, false, NULL, true ); break;
			case TF_CLASS_ENGINEER: flDuration = InstancedScriptedScene( this, "scenes/player/engineer/low/laughlong01.vcd", NULL, 0.0f, false, NULL, true ); break;
			}
		}

		SetContextThink( &CTFPlayer::PowerplayThink, gpGlobals->curtime + flDuration + RandomFloat( 2, 5 ), "TFPlayerLThink" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldAnnouceAchievement( void )
{ 
	if ( IsPlayerClass( TF_CLASS_SPY ) )
	{
		if ( m_Shared.IsStealthed() ||
			 m_Shared.InCond( TF_COND_DISGUISED ) ||
			 m_Shared.InCond( TF_COND_DISGUISING ) )
		{
			return false;
		}
	}

	return true; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::RemoveAllWeapons()
{
	// Base class RemoveAllWeapons() doesn't remove them properly.
	// (doesn't call unequip, or remove immediately. Results in incorrect provision
	//  state for players over round restarts, because players have 2x weapon entities)
	ClearActiveWeapon();
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		CBaseCombatWeapon *pWpn = m_hMyWeapons[i];
		if ( pWpn )
		{
			Weapon_Detach( pWpn );
			UTIL_Remove( pWpn );
		}
	}

	m_Shared.RemoveDisguiseWeapon();
}

//-----------------------------------------------------------------------------
// Purpose: Handles USE keypress
//-----------------------------------------------------------------------------
void CTFPlayer::PlayerUse ( bool bPushAway )
{
	if ( !tf_allow_player_use.GetBool() && IsTF2Class() )
	{
		if ( !IsObserver() && !IsInCommentaryMode() )
		{
			return;
		}
	}

	BaseClass::PlayerUse( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayer::GetHeadScaleSpeed() const
{
	// change size now
	if (
		m_Shared.InCond( TF_COND_HALLOWEEN_BOMB_HEAD ) ||
		m_Shared.InCond( TF_COND_MELEE_ONLY ) ||
		m_Shared.InCond( TF_COND_HALLOWEEN_KART ) ||
		m_Shared.InCond( TF_COND_BALLOON_HEAD )
		)
	{	
		return GetDesiredHeadScale();
	}

	return gpGlobals->frametime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayer::GetTorsoScaleSpeed() const
{
	return gpGlobals->frametime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayer::GetHandScaleSpeed() const
{
	if ( m_Shared.InCond( TF_COND_MELEE_ONLY ) )
	{
		return GetDesiredHandScale();
	}

	return gpGlobals->frametime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::CreateDisguiseWeaponList( CTFPlayer *pDisguiseTarget )
{
	ClearDisguiseWeaponList();

	// copy disguise target weapons
	if ( pDisguiseTarget )
	{
		for ( int i=0; i<TF_PLAYER_WEAPON_COUNT; ++i )
		{
			CTFWeaponBase *pWeapon = dynamic_cast<CTFWeaponBase *>( pDisguiseTarget->GetWeapon( i ) );

			if ( !pWeapon )
				continue;

			int iSubType = 0;
			CTFWeaponBase *pCopyWeapon = dynamic_cast<CTFWeaponBase*>( GiveNamedItem( pWeapon->GetClassname(), iSubType ) );
			if ( pCopyWeapon )
			{
				pCopyWeapon->AddEffects( EF_NODRAW | EF_NOSHADOW );
				m_hDisguiseWeaponList.AddToTail( pCopyWeapon );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ClearDisguiseWeaponList()
{
	FOR_EACH_VEC( m_hDisguiseWeaponList, i )
	{
		if ( m_hDisguiseWeaponList[i] )
		{
			m_hDisguiseWeaponList[i]->Drop( vec3_origin );
		}
	}

	m_hDisguiseWeaponList.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::ShouldForceTransmitsForTeam( int iTeam )
{ 
	return ( ( GetTeamNumber() == TEAM_SPECTATOR ) || 
			 ( ( GetTeamNumber() == iTeam ) && ( m_Shared.InCond( TF_COND_TEAM_GLOWS ) || !IsAlive() ) ) );
}

//////////////////////////////// Day Of Defeat ////////////////////////////////
//-----------------------------------------------------------------------------
// Purpose: Initialize prone at spawn.
//-----------------------------------------------------------------------------
void CTFPlayer::InitProne( void )
{
	m_Shared.SetProne( false, true );
}

void CTFPlayer::InitSprinting( void )
{
	m_Shared.SetSprinting( false );
}

ConVar dod_friendlyfiresafezone( "dod_friendlyfiresafezone",
								"100",
								FCVAR_ARCHIVE,
								"Units around a player where they will not damage teammates, even if FF is on",
								true, 0, false, 0 );

ConVar dod_debugdamage( "dod_debugdamage", "0", FCVAR_CHEAT );

void CTFPlayer::TraceAttackDOD( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	bool bTakeDamage = true;

	CTakeDamageInfo info_modified = info;

	CBasePlayer *pAttacker = (CBasePlayer*)ToBasePlayer( info_modified.GetAttacker() );

	bool bFriendlyFire = friendlyfire.GetBool();

	if ( pAttacker && ( GetTeamNumber() == pAttacker->GetTeamNumber() ) )
	{
		bTakeDamage = bFriendlyFire;

		// Create a FF-free zone around players for melee and bullets
		if ( bFriendlyFire && ( info_modified.GetDamageType() & (DMG_SLASH | DMG_BULLET | DMG_CLUB) ) )
		{
			float flDist = dod_friendlyfiresafezone.GetFloat();

			Vector vecDist = pAttacker->WorldSpaceCenter() - WorldSpaceCenter();

			if ( vecDist.LengthSqr() < ( flDist*flDist ) )
			{
				bTakeDamage = false;
			}			
		}
	}
		
	if ( m_takedamage != DAMAGE_YES )
		return;

	m_LastHitGroup = ptr->hitgroup;	
	m_LastDamageType = info_modified.GetDamageType();

	Assert( ptr->hitgroup != HITGROUP_GENERIC );

	m_nForceBone = ptr->physicsbone;	//Save this bone for ragdoll

	float flDamage = info_modified.GetDamage();
	float flOriginalDmg = flDamage;

	bool bHeadShot = false;

	//if its an enemy OR ff is on.
	if( bTakeDamage )
	{
		m_Shared.SetSlowedTime( 0.5f );
	}

	char *szHitbox = NULL;

	if( info_modified.GetDamageType() & DMG_BLAST )
	{
		// don't do hitgroup specific grenade damage
		flDamage *= 1.0;
		szHitbox = "dmg_blast";
	}
	else
	{
		switch ( ptr->hitgroup )
		{
		case HITGROUP_HEAD:
			{
				flDamage *= 2.5; //regular head shot multiplier

				if( bTakeDamage )
				{
					Vector dir = vecDir;
					VectorNormalize(dir);

					if ( info_modified.GetDamageType() & DMG_CLUB )
						dir *= 800;
					else
                        dir *= 400;

					dir.z += 100;		// add some lift so it pops better.

					PopHelmet( dir, ptr->endpos );
				}

				szHitbox = "head";
				
				bHeadShot = true;
				info_modified.SetDamageCustom( DOD_DMG_CUSTOM_HEADSHOT );
			}
			break;

		case HITGROUP_CHEST:
			szHitbox = "chest";
			break;

		case HITGROUP_STOMACH:	
			szHitbox = "stomach";
			break;

		case HITGROUP_LEFTARM:
			flDamage *= 0.75;	
			szHitbox = "left arm";
			break;
			
		case HITGROUP_RIGHTARM:
			flDamage *= 0.75;			
			szHitbox = "right arm";
			break;

		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			{
				//we are slowed for 2 full seconds if we get a leg hit
				if( bTakeDamage )
				{
					//m_bSlowedByHit = true;
					//m_flUnslowTime = gpGlobals->curtime + 2;
					m_Shared.SetSlowedTime( 2.0f );
				}

				flDamage *= 0.75;
				
				szHitbox = "leg";
			}
			break;
			
		case HITGROUP_GENERIC:
			szHitbox = "(error - hit generic)";
			break;

		default:
			szHitbox = "(error - hit default)";
			break;
		}
	}	
	
	if ( bTakeDamage )
	{	
		if( dod_debugdamage.GetInt() )
		{
			char buf[256];

			Q_snprintf( buf, sizeof(buf), "%s hit %s in the %s hitgroup for %f damage ( %f base damage ) ( %s now has %d health )\n",
				pAttacker->GetPlayerName(),
				GetPlayerName(),
				szHitbox,
				flDamage, 
				flOriginalDmg,
				GetPlayerName(),
				GetHealth() - (int)flDamage );	

			// print to server
			UTIL_LogPrintf( "%s", buf );

			// print to injured
			ClientPrint( this, HUD_PRINTCONSOLE, buf );

			// print to shooter
			if ( pAttacker )
				ClientPrint( pAttacker, HUD_PRINTCONSOLE, buf );
		}

#if 0 // I don't know whether I want different blood particles running so use TF2 for now
		// Since this code only runs on the server, make sure it shows the tempents it creates.
		CDisablePredictionFiltering disabler;

		// This does smaller splotches on the guy and splats blood on the world.
		TraceBleed( flDamage, vecDir, ptr, info_modified.GetDamageType() );

		CEffectData	data;
		data.m_vOrigin = ptr->endpos;
		data.m_vNormal = vecDir * -1;
		data.m_flScale = 4;
		data.m_fFlags = FX_BLOODSPRAY_ALL;
		data.m_nEntIndex = ptr->m_pEnt ?  ptr->m_pEnt->entindex() : 0;
		data.m_flMagnitude = flDamage;

		DispatchEffect( "dodblood", data );
#endif

		info_modified.SetDamage( flDamage );

		AddMultiDamage( info_modified, this );
	}
}

bool CTFPlayer::DropActiveWeapon( void )
{
	CWeaponDODBase *pWeapon = dynamic_cast< CWeaponDODBase* >( GetActivePCWeapon() );

	if( pWeapon )
	{
		if( pWeapon->CanDrop() )
		{
			DODWeaponDrop( pWeapon, true );
			return true;
		}
		else
		{
			int iWeaponType = pWeapon->GetPCWpnData().m_DODWeaponData.m_WeaponType;
			if( iWeaponType == WPN_TYPE_BAZOOKA || iWeaponType == WPN_TYPE_MG )
			{
				// SEALTODO Hint system
				// they are deployed, cannot drop
				//Hints()->HintMessage( "#game_cannot_drop_while" );
			}
			else
			{
				// SEALTODO Hint system
				// must be a weapon type that cannot be dropped
				//Hints()->HintMessage( "#game_cannot_drop" );
			}

			return false;
		}
	}

	return false;
}

void CTFPlayer::PickUpWeapon( CWeaponDODBase *pWeapon )
{
	if ( !IsDODClass() )
		return;

	// if we have a primary weapon and we are allowed to drop it, drop it and 
	// pick up the one we +used

	CWeaponDODBase *pCurrentPrimaryWpn = dynamic_cast< CWeaponDODBase* >( Weapon_GetSlot( WPN_SLOT_PRIMARY ) );

	// drop primary if we can
	if( pCurrentPrimaryWpn )
	{
		if ( pCurrentPrimaryWpn->CanDrop() == false )
		{
			return;
		}

		DODWeaponDrop( pCurrentPrimaryWpn, true );
	}

	// pick up the new one
	if ( BumpWeapon( pWeapon ) )
	{
		pWeapon->OnPickedUp( this );
	}
}

bool CTFPlayer::DropPrimaryWeapon( void )
{
	CWeaponDODBase *pWeapon = dynamic_cast< CWeaponDODBase* >( Weapon_GetSlot( WPN_SLOT_PRIMARY ) );

	if( pWeapon )
	{
		DODWeaponDrop( pWeapon, false );
		return true;
	}

	return false; 
}

bool CTFPlayer::DODWeaponDrop( CBaseCombatWeapon *pWeapon, bool bThrowForward )
{
	bool bSuccess = false;

	if ( pWeapon )
	{
		Vector vForward;

		AngleVectors( EyeAngles(), &vForward, NULL, NULL );
		Vector vTossPos = WorldSpaceCenter();

		if( bThrowForward )
			vTossPos = vTossPos + vForward * 64;

		Weapon_Drop( pWeapon, &vTossPos, NULL );
			
		pWeapon->SetSolidFlags( FSOLID_NOT_STANDABLE | FSOLID_TRIGGER | FSOLID_USE_TRIGGER_BOUNDS );
		pWeapon->SetMoveCollide( MOVECOLLIDE_FLY_BOUNCE );

		CWeaponDODBase *pDODWeapon = dynamic_cast< CWeaponDODBase* >( pWeapon );

		if( pDODWeapon )
		{
			pDODWeapon->SetDieThink(true);

			pDODWeapon->SetWeaponModelIndex( pDODWeapon->GetPCWpnData().szWorldModel );

			//Find out the index of the ammo type
			int iAmmoIndex = pDODWeapon->GetPrimaryAmmoType();

			//If it has an ammo type, find out how much the player has
			if( iAmmoIndex != -1 )
			{
				int iAmmoToDrop = GetAmmoCount( iAmmoIndex );

				//Add this much to the dropped weapon
				pDODWeapon->SetExtraAmmoCount( iAmmoToDrop );

				//Remove all ammo of this type from the player
				SetAmmoCount( 0, iAmmoIndex );
			}
		}

		//=========================================
		// Teleport the weapon to the player's hand
		//=========================================
		int iBIndex = -1;
		int iWeaponBoneIndex = -1;

		CStudioHdr *hdr = pWeapon->GetModelPtr();
		// If I have a hand, set the weapon position to my hand bone position.
		if ( hdr && hdr->numbones() > 0 )
		{
			// Assume bone zero is the root
			for ( iWeaponBoneIndex = 0; iWeaponBoneIndex < hdr->numbones(); ++iWeaponBoneIndex )
			{
				iBIndex = LookupBone( hdr->pBone( iWeaponBoneIndex )->pszName() );
				// Found one!
				if ( iBIndex != -1 )
				{
					break;
				}
			}
			
			if ( iWeaponBoneIndex == hdr->numbones() )
				 return true;

			if ( iBIndex == -1 )
			{
				iBIndex = LookupBone( "ValveBiped.Bip01_R_Hand" );
			}
		}
		else
		{
			iBIndex = LookupBone( "ValveBiped.Bip01_R_Hand" );
		}

		if ( iBIndex != -1)  
		{
			Vector origin;
			QAngle angles;
			matrix3x4_t transform;

			// Get the transform for the weapon bonetoworldspace in the NPC
			GetBoneTransform( iBIndex, transform );

			// find offset of root bone from origin in local space
			// Make sure we're detached from hierarchy before doing this!!!
			pWeapon->StopFollowingEntity();
			pWeapon->SetAbsOrigin( Vector( 0, 0, 0 ) );
			pWeapon->SetAbsAngles( QAngle( 0, 0, 0 ) );
			pWeapon->InvalidateBoneCache();
			matrix3x4_t rootLocal;
			pWeapon->GetBoneTransform( iWeaponBoneIndex, rootLocal );

			// invert it
			matrix3x4_t rootInvLocal;
			MatrixInvert( rootLocal, rootInvLocal );

			matrix3x4_t weaponMatrix;
			ConcatTransforms( transform, rootInvLocal, weaponMatrix );
			MatrixAngles( weaponMatrix, angles, origin );

			// trace the bounding box of the weapon in the desired position
			trace_t tr;
			Vector mins, maxs;

			// not exactly correct bounds, we haven't rotated them to match the attachment
			pWeapon->CollisionProp()->WorldSpaceSurroundingBounds( &mins, &maxs );

			UTIL_TraceHull( WorldSpaceCenter(), origin, mins, maxs, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
			
			if ( tr.fraction < 1.0 )
				origin = WorldSpaceCenter();
			
			pWeapon->Teleport( &origin, &angles, NULL );
			
			//Have to teleport the physics object as well

			IPhysicsObject *pWeaponPhys = pWeapon->VPhysicsGetObject();

			if( pWeaponPhys )
			{
				Vector vPos;
				QAngle vAngles;
				pWeaponPhys->GetPosition( &vPos, &vAngles );
				pWeaponPhys->SetPosition( vPos, angles, true );

				AngularImpulse	angImp(0,0,0);
				Vector vecAdd = GetAbsVelocity();
				pWeaponPhys->AddVelocity( &vecAdd, &angImp );
			}

			m_hLastDroppedWeapon = pWeapon;
		}

		if ( !GetActiveWeapon() )
		{
			// we haven't auto-switched to anything usable
			// switch to the first weapon we find, even if its empty

			CBaseCombatWeapon *pCheck;

			for ( int i = 0 ; i < WeaponCount(); ++i )
			{
				pCheck = GetWeapon( i );
				if ( !pCheck || !pCheck->CanDeploy() )
				{
					continue;
				}

				Weapon_Switch( pCheck );
				break;
			}
		}
		
		bSuccess = true;
	}

	return bSuccess;
}

extern int g_iHelmetModels[NUM_HELMETS];

void CTFPlayer::PopHelmet( Vector vecDir, Vector vecForceOrigin )
{
	TFPlayerClassData_t *pClassInfo = GetPlayerClass()->GetData();

	// See if they already lost their helmet
	if( GetBodygroup( BODYGROUP_HELMET ) == pClassInfo->m_iHairGroup )
		return;
	
	// Nope.. take it off
	SetBodygroup( BODYGROUP_HELMET, pClassInfo->m_iHairGroup );

	// Add the velocity of the player
	vecDir += GetAbsVelocity();

	//CDisablePredictionFiltering disabler;

	EntityMessageBegin( this, true );
		WRITE_BYTE( DOD_PLAYER_POP_HELMET );
		WRITE_VEC3COORD( vecDir );
		WRITE_VEC3COORD( vecForceOrigin );
		WRITE_SHORT( g_iHelmetModels[pClassInfo->m_iDropHelmet] );
	MessageEnd();
}

bool CTFPlayer::BumpWeapon( CBaseCombatWeapon *pBaseWeapon )
{
	CWeaponDODBase *pWeapon = dynamic_cast< CWeaponDODBase* >( pBaseWeapon );

	if ( !IsDODClass() )
	{
		if ( pWeapon ) // Prevent picking up dod weapons
			return false;
		else
			return BaseClass::BumpWeapon( pBaseWeapon ); // Return default class
	}

	if ( !pWeapon )
	{
		Assert( false );
		return false;
	}
	
	CBaseCombatCharacter *pOwner = pWeapon->GetOwner();

	// Can I have this weapon type?
	if ( pOwner || !Weapon_CanUse( pWeapon ) || !g_pGameRules->CanHavePlayerItem( this, pWeapon ) )
	{
		extern int gEvilImpulse101;
		if ( gEvilImpulse101 )
		{
			UTIL_Remove( pWeapon );
		}
		return false;
	}

	// Don't let the player fetch weapons through walls
	if( !pWeapon->FVisible( this ) && !(GetFlags() & FL_NOTARGET) )
	{
		return false;
	}

	/*
	CBaseCombatWeapon *pOwnedWeapon = Weapon_OwnsThisType( pWeapon->GetClassname() );

	if( pOwnedWeapon != NULL && ( pWeapon->GetWpnData().iFlags & ITEM_FLAG_EXHAUSTIBLE ) )
	{
		// its an item that we can hold several of, and use up

		// Just give one ammo
		if( GiveAmmo( 1, pOwnedWeapon->GetPrimaryAmmoType(), true ) > 0 )
			return true;
		else
			return false;
	}
	*/

	// ----------------------------------------
	// If I already have it just take the ammo
	// ----------------------------------------
	if (Weapon_OwnsThisType( pWeapon->GetClassname(), pWeapon->GetSubType())) 
	{
		if( Weapon_EquipAmmoOnly( pWeapon ) )
		{
			// Only remove me if I have no ammo left
			if ( pWeapon->HasPrimaryAmmo() )
				return false;

			UTIL_Remove( pWeapon );
			return true;
		}
		else
		{
			return false;
		}
	}

	pWeapon->CheckRespawn();

	pWeapon->AddSolidFlags( FSOLID_NOT_SOLID );
	pWeapon->AddEffects( EF_NODRAW );

	Weapon_Equip( pWeapon );

	int iExtraAmmo = pWeapon->GetExtraAmmoCount();
	
	if( iExtraAmmo )
	{
		//Find out the index of the ammo
		int iAmmoIndex = pWeapon->GetPrimaryAmmoType();

		if( iAmmoIndex != -1 )
		{
			//Remove the extra ammo from the weapon
			pWeapon->SetExtraAmmoCount(0);

			//Give it to the player
			SetAmmoCount( iExtraAmmo, iAmmoIndex );
		}
	}

	return true;
}

CBaseEntity	*CTFPlayer::GiveNamedItem( const char *pszName, int iSubType )
{
	if ( !IsDODClass() )
		return BaseClass::GiveNamedItem( pszName,iSubType );

	EHANDLE pent;

	pent = CreateEntityByName(pszName);
	if ( pent == NULL )
	{
		Msg( "NULL Ent in GiveNamedItem!\n" );
		return NULL;
	}

	pent->SetLocalOrigin( GetLocalOrigin() );
	pent->AddSpawnFlags( SF_NORESPAWN );

	if ( iSubType )
	{
		CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>( (CBaseEntity*)pent );
		if ( pWeapon )
		{
			pWeapon->SetSubType( iSubType );
		}
	}

	DispatchSpawn( pent );

	if ( pent != NULL && !(pent->IsMarkedForDeletion()) ) 
	{
		pent->Touch( this );
	}

	return pent;
}

int CTFPlayer::GetMaxAmmo( int iAmmoIndex, int iClassIndex /*= -1*/ )
{
	int iMax = 0;

	switch ( GetClassType() )
	{
		case FACTION_DOD:
		{
			for ( int i = 0; i < WeaponCount(); i++ )
			{
				CWeaponDODBase *pWpn = ( CWeaponDODBase *)GetWeapon( i );

				if ( !pWpn )
					continue;

				if ( pWpn->GetPrimaryAmmoType() != iAmmoIndex )
					continue;

				iMax = ( pWpn->GetPCWpnData().m_DODWeaponData.m_iDefaultAmmoClips - 1 ) * pWpn->GetPCWpnData().iMaxClip1;

				int iGrenade1 = GetPlayerClass()->GetData()->m_iGrenType1;
				int iGrenade2 = GetPlayerClass()->GetData()->m_iGrenType2;

				if ( pWpn->GetWeaponID() == iGrenade1 )
					iMax = GetPlayerClass()->GetData()->m_iNumGrensType1;
				else if ( pWpn->GetWeaponID() == iGrenade2 )
					iMax = GetPlayerClass()->GetData()->m_iNumGrensType2;

				break;
			}
			break;
		}
		case FACTION_HL2:
		case FACTION_CS:
		{
			iMax = GetAmmoDef()->MaxCarry( iAmmoIndex );
			break;
		}
		default:
		{
			iMax = ( iClassIndex == -1 ) ? m_PlayerClass.GetData()->m_aAmmoMax[iAmmoIndex] : 
				GetPlayerClassData( iClassIndex )->m_aAmmoMax[iAmmoIndex];
			break;
		}
	}

	return iMax;
}

void CTFPlayer::OnDamagedByExplosionDOD( const CTakeDamageInfo &info )
{
	if ( info.GetDamage() >= 30.0f )
	{
		SetContextThink( &CTFPlayer::DeafenThink, gpGlobals->curtime + 0.3, DOD_DEAFEN_CONTEXT );

		// The blast will naturally blow the temp ent helmet away
		PopHelmet( info.GetDamagePosition(), info.GetDamageForce() );
	}	
}

ConVar dod_stun_min_pitch( "dod_stun_min_pitch", "30", FCVAR_CHEAT );
ConVar dod_stun_max_pitch( "dod_stun_max_pitch", "50", FCVAR_CHEAT );
ConVar dod_stun_min_yaw( "dod_stun_min_yaw", "120", FCVAR_CHEAT );
ConVar dod_stun_max_yaw( "dod_stun_max_yaw", "150", FCVAR_CHEAT );
ConVar dod_stun_min_roll( "dod_stun_min_roll", "15", FCVAR_CHEAT );
ConVar dod_stun_max_roll( "dod_stun_max_roll", "30", FCVAR_CHEAT );

void CTFPlayer::OnDamageByStun( const CTakeDamageInfo &info )
{
	DevMsg( 2, "took %.1f stun damage\n", info.GetDamage() );

	float flPercent = ( info.GetDamage() / 100.0f );

	m_flStunDuration = 0.0;	// mark it as dirty so it transmits, incase we get the same value twice
	m_flStunDuration = 4.0f * flPercent;
	m_flStunMaxAlpha = 255;

	float flPitch = flPercent * RandomFloat( dod_stun_min_pitch.GetFloat(), dod_stun_max_pitch.GetFloat() ) *
		( (RandomInt( 0, 1 )) ? 1 : -1 );

	float flYaw = flPercent * RandomFloat( dod_stun_min_yaw.GetFloat(), dod_stun_max_yaw.GetFloat() )
		* ( (RandomInt( 0, 1 )) ? 1 : -1 );

	float flRoll = flPercent * RandomFloat( dod_stun_min_roll.GetFloat(), dod_stun_max_roll.GetFloat() )
		* ( (RandomInt( 0, 1 )) ? 1 : -1 );

	DevMsg( 2, "punch: pitch %.1f  yaw %.1f  roll %.1f\n", flPitch, flYaw, flRoll );

	SetPunchAngle( QAngle( flPitch, flYaw, flRoll ) );
}

void CTFPlayer::DeafenThink( void )
{
	int effect = random->RandomInt( 32, 34 );

	CSingleUserRecipientFilter user( this );
	enginesound->SetPlayerDSP( user, effect, false );
}

//////////////////////////////// Counter-Strike ////////////////////////////////
//void CTFPlayer::TraceAttackCS( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
//{
//	bool bShouldBleed = true;
//	bool bShouldSpark = false;
//#if 0
//	bool bHitShield = IsHittingShield( vecDir, ptr );
//#else
//	bool bHitShield = false;
//#endif
//
//	CBasePlayer *pAttacker = (CBasePlayer*)ToBasePlayer( info.GetAttacker() );
//
//	// show blood for firendly fire only if FF is on
//	if ( pAttacker && ( GetTeamNumber() == pAttacker->GetTeamNumber() ) )
//		 bShouldBleed = TFGameRules()->IsFriendlyFireOn();
//
//	if ( m_takedamage != DAMAGE_YES )
//		return;
//
//	m_LastHitGroup = ptr->hitgroup;
////=============================================================================
//// HPE_BEGIN:
//// [menglish] Used when calculating the position this player was hit at in the bone space
////=============================================================================
//	m_LastHitBox = ptr->hitbox;
////=============================================================================
//// HPE_END
////=============================================================================
//
//	m_nForceBone = ptr->physicsbone;	//Save this bone for ragdoll
//
//	float flDamage = info.GetDamage();
//
//	bool bHeadShot = false;
//
//	if ( bHitShield )
//	{
//		flDamage = 0;
//		bShouldBleed = false;
//		bShouldSpark = true;
//	}
//	else if( info.GetDamageType() & DMG_BLAST )
//	{
//		if ( ArmorValue() > 0 )
//			 bShouldBleed = false;
//
//		if ( bShouldBleed == true )
//		{
//			// punch view if we have no armor
//			QAngle punchAngle = GetPunchAngle();
//			punchAngle.x = flDamage * -0.1;
//
//			if ( punchAngle.x < -4 )
//				punchAngle.x = -4;
//
//			SetPunchAngle( punchAngle );
//		}
//	}
//	else
//	{
////=============================================================================
//// HPE_BEGIN:
//// [menglish] Calculate the position this player was hit at in the bone space
////=============================================================================
//		 
//		matrix3x4_t boneTransformToWorld, boneTransformToObject;
//		GetBoneTransform(GetHitboxBone(ptr->hitbox), boneTransformToWorld);
//		MatrixInvert(boneTransformToWorld, boneTransformToObject);
//		VectorTransform(ptr->endpos, boneTransformToObject, m_vLastHitLocationObjectSpace);
//		 
////=============================================================================
//// HPE_END
////=============================================================================
//
//		switch ( ptr->hitgroup )
//		{
//		case HITGROUP_GENERIC:
//			break;
//
//		case HITGROUP_HEAD:
//
//			if ( m_bHasHelmet )
//			{
////				bShouldBleed = false;
//				bShouldSpark = true;
//			}
//
//			flDamage *= 4;
//
//			if ( !m_bHasHelmet )
//			{
//				QAngle punchAngle = GetPunchAngle();
//				punchAngle.x = flDamage * -0.5;
//
//				if ( punchAngle.x < -12 )
//					punchAngle.x = -12;
//
//				punchAngle.z = flDamage * random->RandomFloat(-1,1);
//
//				if ( punchAngle.z < -9 )
//					punchAngle.z = -9;
//
//				else if ( punchAngle.z > 9 )
//					punchAngle.z = 9;
//
//				SetPunchAngle( punchAngle );
//			}
//
//			bHeadShot = true;
//
//			break;
//
//		case HITGROUP_CHEST:
//
//			flDamage *= 1.0;
//
//			if ( ArmorValue() <= 0 )
//			{
//				QAngle punchAngle = GetPunchAngle();
//				punchAngle.x = flDamage * -0.1;
//
//				if ( punchAngle.x < -4 )
//					punchAngle.x = -4;
//
//				SetPunchAngle( punchAngle );
//			}
//			break;
//
//		case HITGROUP_STOMACH:
//
//			flDamage *= 1.25;
//
//			if ( ArmorValue() <= 0 )
//			{
//				QAngle punchAngle = GetPunchAngle();
//				punchAngle.x = flDamage * -0.1;
//
//				if ( punchAngle.x < -4 )
//					punchAngle.x = -4;
//
//				SetPunchAngle( punchAngle );
//			}
//
//			break;
//
//		case HITGROUP_LEFTARM:
//		case HITGROUP_RIGHTARM:
//			flDamage *= 1.0;
//			break;
//
//		case HITGROUP_LEFTLEG:
//		case HITGROUP_RIGHTLEG:
//			flDamage *= 0.75;
//			break;
//
//		default:
//			break;
//		}
//	}
//
//	// Since this code only runs on the server, make sure it shows the tempents it creates.
//	CDisablePredictionFiltering disabler;
//
//	if ( bShouldBleed )
//	{
//		// This does smaller splotches on the guy and splats blood on the world.
//		TraceBleed( flDamage, vecDir, ptr, info.GetDamageType() );
//
//		CEffectData	data;
//		data.m_vOrigin = ptr->endpos;
//		data.m_vNormal = vecDir * -1;
//		data.m_nEntIndex = ptr->m_pEnt ?  ptr->m_pEnt->entindex() : 0;
//		data.m_flMagnitude = flDamage;
//
//		// reduce blood effect if target has armor
//		if ( ArmorValue() > 0 )
//			data.m_flMagnitude *= 0.5f;
//
//		// reduce blood effect if target is hit in the helmet
//		if ( ptr->hitgroup == HITGROUP_HEAD && bShouldSpark )
//			data.m_flMagnitude *= 0.5;
//
//		DispatchEffect( "csblood", data );
//	}
//	if ( ( ptr->hitgroup == HITGROUP_HEAD || bHitShield ) && bShouldSpark ) // they hit a helmet
//	{
//		// show metal spark effect
//		g_pEffects->Sparks( ptr->endpos, 1, 1, &ptr->plane.normal );
//	}
//
//	if ( !bHitShield )
//	{
//		CTakeDamageInfo subInfo = info;
//
//		subInfo.SetDamage( flDamage );
//
//		if( bHeadShot )
//			subInfo.AddDamageType( DMG_HEADSHOT );
//
//		AddMultiDamage( subInfo, this );
//	}
//}