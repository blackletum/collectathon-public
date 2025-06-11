//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "c_tf_player.h"
#include "c_user_message_register.h"
#include "view.h"
#include "iclientvehicle.h"
#include "ivieweffects.h"
#include "input.h"
#include "IEffects.h"
#include "fx.h"
#include "c_basetempentity.h"
#include "hud_macros.h"
#include "engine/ivdebugoverlay.h"
#include "smoke_fog_overlay.h"
#include "playerandobjectenumerator.h"
#include "bone_setup.h"
#include "in_buttons.h"
#include "r_efx.h"
#include "dlight.h"
#include "shake.h"
#include "cl_animevent.h"
#include "tf_weaponbase.h"
#include "c_tf_playerresource.h"
#include "toolframework/itoolframework.h"
#include "tier1/KeyValues.h"
#include "tier0/vprof.h"
#include "prediction.h"
#include "effect_dispatch_data.h"
#include "c_te_effect_dispatch.h"
#include "tf_fx_muzzleflash.h"
#include "tf_gamerules.h"
#include "view_scene.h"
#include "c_baseobject.h"
#include "toolframework_client.h"
#include "soundenvelope.h"
#include "voice_status.h"
#include "clienteffectprecachesystem.h"
#include "functionproxy.h"
#include "toolframework_client.h"
#include "choreoevent.h"
#include "vguicenterprint.h"
#include "eventlist.h"
#include "tf_hud_statpanel.h"
#include "input.h"
#include "tf_weapon_medigun.h"
#include "tf_weapon_pipebomblauncher.h"
#include "tf_hud_mediccallers.h"
#include "in_main.h"
#include "c_team.h"
#include "collisionutils.h"
// for spy material proxy
#include "tf_proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexturecompositor.h"
#include "c_tf_team.h"
#include "model_types.h"
#include "dt_utlvector_recv.h"

#include "c_entitydissolve.h"

#if defined( CTFPlayer )
#undef CTFPlayer
#endif

#include "materialsystem/imesh.h"		//for materials->FindMaterial
#include "iviewrender.h"				//for view->

#include "cam_thirdperson.h"

// DOD Includes
#include "dod_shareddefs.h"
#include "weapon_dodbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar tf_playergib_forceup( "tf_playersgib_forceup", "1.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Upward added velocity for gibs." );
ConVar tf_playergib_force( "tf_playersgib_force", "500.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Gibs force." );
ConVar tf_playergib_maxspeed( "tf_playergib_maxspeed", "400", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Max gib speed." );

ConVar tf_always_deathanim( "tf_always_deathanim", "0", FCVAR_CHEAT, "Force death anims to always play." );

ConVar tf_clientsideeye_lookats( "tf_clientsideeye_lookats", "1", FCVAR_NONE, "When on, players will turn their pupils to look at nearby players." );

ConVar cl_autoreload( "cl_autoreload", "1", FCVAR_USERINFO | FCVAR_ARCHIVE, "Set to 1 to auto reload your weapon when it is empty" );
ConVar cl_autorezoom( "cl_autorezoom", "1", FCVAR_USERINFO | FCVAR_ARCHIVE, "When set to 1, sniper rifle will re-zoom after firing a zoomed shot." );

ConVar tf_taunt_first_person( "tf_taunt_first_person", "0", FCVAR_NONE, "1 = taunts remain first-person" );

#define BDAY_HAT_MODEL		"models/effects/bday_hat.mdl"
#define BOMB_HAT_MODEL		"models/props_lakeside_event/bomb_temp_hat.mdl"
#define BOMBONOMICON_MODEL  "models/props_halloween/bombonomicon.mdl"

IMaterial	*g_pHeadLabelMaterial[2] = { NULL, NULL }; 
void	SetupHeadLabelMaterials( void );

extern CBaseEntity *BreakModelCreateSingle( CBaseEntity *pOwner, breakmodel_t *pModel, const Vector &position, 
										   const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, int nSkin, const breakablepropparams_t &params );

const char *g_pszHeadGibs[] =
{
	"",
	"models/player\\gibs\\scoutgib007.mdl",
	"models/player\\gibs\\snipergib005.mdl",
	"models/player\\gibs\\soldiergib007.mdl",
	"models/player\\gibs\\demogib006.mdl",
	"models/player\\gibs\\medicgib007.mdl",
	"models/player\\gibs\\heavygib007.mdl",
	"models/player\\gibs\\pyrogib008.mdl",
	"models/player\\gibs\\spygib007.mdl",
	"models/player\\gibs\\engineergib006.mdl",
};

const char *pszHeadLabelNames[] =
{
	"effects/speech_voice_red",
	"effects/speech_voice_blue"
};

#define TF_PLAYER_HEAD_LABEL_RED 0
#define TF_PLAYER_HEAD_LABEL_BLUE 1

CLIENTEFFECT_REGISTER_BEGIN( PrecacheInvuln )
CLIENTEFFECT_MATERIAL( "models/effects/invulnfx_blue.vmt" )
CLIENTEFFECT_MATERIAL( "models/effects/invulnfx_red.vmt" )
CLIENTEFFECT_REGISTER_END()

// thirdperson medieval
static ConVar tf_medieval_thirdperson( "tf_medieval_thirdperson", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE , "Turns on third-person camera in medieval mode." );
static ConVar tf_medieval_cam_idealdist( "tf_medieval_cam_idealdist", "125", FCVAR_CLIENTDLL | FCVAR_CHEAT );	 // thirdperson distance
static ConVar tf_medieval_cam_idealdistright( "tf_medieval_cam_idealdistright", "25", FCVAR_CLIENTDLL | FCVAR_CHEAT );	 // thirdperson distance
static ConVar tf_medieval_cam_idealdistup( "tf_medieval_cam_idealdistup", "-10", FCVAR_CLIENTDLL | FCVAR_CHEAT );	 // thirdperson distance
static ConVar tf_medieval_cam_idealpitch( "tf_medieval_cam_idealpitch", "0", FCVAR_CLIENTDLL | FCVAR_CHEAT );	 // thirdperson pitch
extern ConVar cam_idealpitch;
extern ConVar tf_allow_taunt_switch;

C_EntityDissolve *DissolveEffect( C_BaseEntity *pTarget, float flTime );

void SetAppropriateCamera( C_TFPlayer *pPlayer )
{
	if ( pPlayer->IsLocalPlayer() == false )
		return;

	// SEALTODO add medival one day?
	bool bIsInMedival = false;

	if ( TFGameRules() &&
		( ( bIsInMedival && tf_medieval_thirdperson.GetBool() )
		|| pPlayer->m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) ) )
	{
		g_ThirdPersonManager.SetForcedThirdPerson( true );
		Vector offset( tf_medieval_cam_idealdist.GetFloat(), tf_medieval_cam_idealdistright.GetFloat(), tf_medieval_cam_idealdistup.GetFloat() );
		g_ThirdPersonManager.SetDesiredCameraOffset( offset );
		cam_idealpitch.SetValue( tf_medieval_cam_idealpitch.GetFloat() );

		::input->CAM_ToThirdPerson();

		pPlayer->ThirdPersonSwitch( true );
	}
	else
	{
		g_ThirdPersonManager.SetForcedThirdPerson( false );
	}
}

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		VPROF( "C_TEPlayerAnimEvent::PostDataUpdate" );

		// Create the effect.
		if ( m_iPlayerIndex == TF_PLAYER_INDEX_NONE )
			return;

		EHANDLE hPlayer = cl_entitylist->GetNetworkableHandle( m_iPlayerIndex );
		if ( !hPlayer )
			return;

		C_TFPlayer *pPlayer = dynamic_cast< C_TFPlayer* >( hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			pPlayer->DoAnimationEvent( (PlayerAnimEvent_t)m_iEvent.Get(), m_nData );
		}	
	}

public:
	CNetworkVar( int, m_iPlayerIndex );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );

//-----------------------------------------------------------------------------
// Data tables and prediction tables.
//-----------------------------------------------------------------------------
BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	RecvPropInt( RECVINFO( m_iPlayerIndex ) ),
	RecvPropInt( RECVINFO( m_iEvent ) ),
	RecvPropInt( RECVINFO( m_nData ) )
END_RECV_TABLE()


//=============================================================================
//
// Ragdoll
//
// ----------------------------------------------------------------------------- //
// Client ragdoll entity.
// ----------------------------------------------------------------------------- //
ConVar cl_ragdoll_physics_enable( "cl_ragdoll_physics_enable", "1", 0, "Enable/disable ragdoll physics." );
ConVar cl_ragdoll_fade_time( "cl_ragdoll_fade_time", "15", FCVAR_CLIENTDLL );
ConVar cl_ragdoll_forcefade( "cl_ragdoll_forcefade", "0", FCVAR_CLIENTDLL );
ConVar cl_ragdoll_pronecheck_distance( "cl_ragdoll_pronecheck_distance", "64", FCVAR_GAMEDLL );

class C_TFRagdoll : public C_BaseFlex
{
public:

	DECLARE_CLASS( C_TFRagdoll, C_BaseFlex );
	DECLARE_CLIENTCLASS();
	
	C_TFRagdoll();
	~C_TFRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	IRagdoll* GetIRagdoll() const;

	void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );

	void ClientThink( void );

	// Deal with recording
	virtual void GetToolRecordingState( KeyValues *msg );

	void StartFadeOut( float fDelay );
	void EndFadeOut();
	void DissolveEntity( CBaseEntity* pEnt );

	EHANDLE GetPlayerHandle( void ) 	
	{
		if ( m_iPlayerIndex == TF_PLAYER_INDEX_NONE )
			return NULL;
		return cl_entitylist->GetNetworkableHandle( m_iPlayerIndex );
	}

	bool IsRagdollVisible();
	float GetBurnStartTime() { return m_flBurnEffectStartTime; }

	virtual void BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );

	virtual void SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );

	bool IsDeathAnim() { return m_bDeathAnim; }

	int GetDamageCustom() { return m_iDamageCustom; }

	virtual bool GetAttachment( int iAttachment, matrix3x4_t &attachmentToWorld );

	int GetClass() { return m_iClass; }

	float GetPercentInvisible( void ) { return m_flPercentInvisible; }
	bool IsCloaked( void ) { return m_bCloaked; }

	int GetRagdollTeam( void ){ return m_iTeam; }

	float GetHeadScale() const { return m_flHeadScale; }
	float GetTorsoScale() const { return m_flTorsoScale; }
	float GetHandScale() const { return m_flHandScale; }
private:
	
	C_TFRagdoll( const C_TFRagdoll & ) {}

	void Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity );

	void CreateTFRagdoll();
	void CreateTFGibs( bool bDestroyRagdoll = true, bool bCurrentPosition = false );
	void CreateTFHeadGib();

	virtual float FrameAdvance( float flInterval );

	bool IsDecapitation();
	bool IsHeadSmash();

	virtual int	InternalDrawModel( int flags );
private:

	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
	int	  m_iPlayerIndex;
	float m_fDeathTime;
	bool  m_bFadingOut;
	bool  m_bGib;
	bool  m_bBurning;
	bool  m_bElectrocuted;
	bool  m_bBatted;
	bool  m_bDissolving;
	bool  m_bFeignDeath;
	bool  m_bWasDisguised;
	bool  m_bCloaked;
	bool  m_bBecomeAsh;
	int	  m_iDamageCustom;
	bool  m_bGoldRagdoll;
	bool  m_bIceRagdoll;
	CountdownTimer m_freezeTimer;
	CountdownTimer m_frozenTimer;
	int	  m_iTeam;
	int	  m_iClass;
	float m_flBurnEffectStartTime;	// start time of burning, or 0 if not burning
	bool  m_bRagdollOn;
	bool  m_bDeathAnim;				
	bool  m_bOnGround;
	bool  m_bFixedConstraints;
	matrix3x4_t m_mHeadAttachment;
	bool  m_bBaseTransform;
	float m_flPercentInvisible;
	float m_flTimeToDissolve;
	bool  m_bCritOnHardHit;	// plays the red mist particle effect
	float m_flHeadScale;
	float m_flTorsoScale;
	float m_flHandScale;

	CMaterialReference		m_MaterialOverride;

	bool  m_bCreatedWhilePlaybackSkipping;
};

IMPLEMENT_CLIENTCLASS_DT_NOBASE( C_TFRagdoll, DT_TFRagdoll, CTFRagdoll )
	RecvPropVector( RECVINFO(m_vecRagdollOrigin) ),
	RecvPropInt( RECVINFO( m_iPlayerIndex ) ),
	RecvPropVector( RECVINFO(m_vecForce) ),
	RecvPropVector( RECVINFO(m_vecRagdollVelocity) ),
	RecvPropInt( RECVINFO( m_nForceBone ) ),
	RecvPropBool( RECVINFO( m_bGib ) ),
	RecvPropBool( RECVINFO( m_bBurning ) ),
	RecvPropBool( RECVINFO( m_bElectrocuted ) ),
	RecvPropBool( RECVINFO( m_bFeignDeath ) ),
	RecvPropBool( RECVINFO( m_bWasDisguised ) ),
	RecvPropBool( RECVINFO( m_bOnGround ) ),
	RecvPropBool( RECVINFO( m_bCloaked ) ),
	RecvPropBool( RECVINFO( m_bBecomeAsh ) ),
	RecvPropInt( RECVINFO( m_iDamageCustom ) ),
	RecvPropInt( RECVINFO( m_iTeam ) ),
	RecvPropInt( RECVINFO( m_iClass ) ),
	RecvPropBool( RECVINFO( m_bGoldRagdoll ) ),
	RecvPropBool( RECVINFO( m_bIceRagdoll ) ),
	RecvPropBool( RECVINFO( m_bCritOnHardHit ) ),
	RecvPropFloat( RECVINFO( m_flHeadScale ) ),
	RecvPropFloat( RECVINFO( m_flTorsoScale ) ),
	RecvPropFloat( RECVINFO( m_flHandScale ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_TFRagdoll::C_TFRagdoll()
{
	m_iPlayerIndex = TF_PLAYER_INDEX_NONE;
	m_fDeathTime = -1;
	m_bFadingOut = false;
	m_bGib = false;
	m_bBurning = false;
	m_bElectrocuted = false;
	m_bBatted = false;
	m_bDissolving = false;
	m_bFeignDeath = false;
	m_bWasDisguised = false;
	m_bCloaked = false;
	m_bBecomeAsh = false;
	m_flBurnEffectStartTime = 0.0f;
	m_iDamageCustom = 0;
	m_bGoldRagdoll = false;
	m_bIceRagdoll = false;
	m_freezeTimer.Invalidate();
	m_frozenTimer.Invalidate();
	m_iTeam = -1;
	m_iClass = -1;
	m_nForceBone = -1;
	m_bRagdollOn = false;
	m_bDeathAnim = false;
	m_bOnGround = false;
	m_bBaseTransform = false;
	m_bFixedConstraints = false;
	m_flTimeToDissolve = 0.3f;
	m_bCritOnHardHit = false;
	m_flHeadScale = 1.f;
	m_flTorsoScale = 1.f;
	m_flHandScale = 1.f;

	UseClientSideAnimation();

	m_bCreatedWhilePlaybackSkipping = engine->IsSkippingPlayback();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_TFRagdoll::~C_TFRagdoll()
{
	PhysCleanupFrictionSounds( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSourceEntity - 
//-----------------------------------------------------------------------------
void C_TFRagdoll::Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity )
{
	if ( !pSourceEntity )
		return;
	
	VarMapping_t *pSrc = pSourceEntity->GetVarMapping();
	VarMapping_t *pDest = GetVarMapping();
    	
	// Find all the VarMapEntry_t's that represent the same variable.
	for ( int i = 0; i < pDest->m_Entries.Count(); i++ )
	{
		VarMapEntry_t *pDestEntry = &pDest->m_Entries[i];
		for ( int j=0; j < pSrc->m_Entries.Count(); j++ )
		{
			VarMapEntry_t *pSrcEntry = &pSrc->m_Entries[j];
			if ( !Q_strcmp( pSrcEntry->watcher->GetDebugName(), pDestEntry->watcher->GetDebugName() ) )
			{
				pDestEntry->watcher->Copy( pSrcEntry->watcher );
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Setup vertex weights for drawing
//-----------------------------------------------------------------------------
void C_TFRagdoll::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	// While we're dying, we want to mimic the facial animation of the player. Once they're dead, we just stay as we are.
	EHANDLE hPlayer = GetPlayerHandle();
	if ( ( hPlayer && hPlayer->IsAlive()) || !hPlayer )
	{
		BaseClass::SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
	else if ( hPlayer )
	{
		hPlayer->SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTrace - 
//			iDamageType - 
//			*pCustomImpactName - 
//-----------------------------------------------------------------------------
void C_TFRagdoll::ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	VPROF( "C_TFRagdoll::ImpactTrace" );
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if( !pPhysicsObject )
		return;

	Vector vecDir;
	VectorSubtract( pTrace->endpos, pTrace->startpos, vecDir );

	if ( iDamageType == DMG_BLAST )
	{
		// Adjust the impact strength and apply the force at the center of mass.
		vecDir *= 4000;
		pPhysicsObject->ApplyForceCenter( vecDir );
	}
	else
	{
		// Find the apporx. impact point.
		Vector vecHitPos;  
		VectorMA( pTrace->startpos, pTrace->fraction, vecDir, vecHitPos );
		VectorNormalize( vecDir );

		// Adjust the impact strength and apply the force at the impact point..
		vecDir *= 4000;
		pPhysicsObject->ApplyForceOffset( vecDir, vecHitPos );	
	}

	m_pRagdoll->ResetRagdollSleepAfterTime();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void C_TFRagdoll::CreateTFRagdoll()
{
	// Get the player.
	C_TFPlayer *pPlayer = NULL;
	EHANDLE hPlayer = GetPlayerHandle();
	if ( hPlayer )
	{
		pPlayer = dynamic_cast<C_TFPlayer*>( hPlayer.Get() );
	}

	int nModelIndex = -1;

	if ( pPlayer && pPlayer->GetPlayerClass() && !pPlayer->ShouldDrawSpyAsDisguised() )
	{
		nModelIndex = modelinfo->GetModelIndex( pPlayer->GetPlayerClass()->GetModelName() );
	}
	else
	{
		TFPlayerClassData_t *pData = GetPlayerClassData( m_iClass );
		if ( pData )
		{
			nModelIndex = modelinfo->GetModelIndex( pData->GetModelName() );
		}
	}

	if ( pPlayer )
	{
		m_flHeadScale = pPlayer->GetHeadScale();
		m_flTorsoScale = pPlayer->GetTorsoScale();
		m_flHandScale = pPlayer->GetHandScale();
	}

	if ( nModelIndex != -1 )
	{
		SetModelIndex( nModelIndex );

		bool bTeamSkins = pPlayer->IsTF2Class();

		if ( bTeamSkins )
		{
			if ( m_iTeam == TF_TEAM_RED )
			{
				m_nSkin = 0;
			}
			else
			{
				m_nSkin = 1;
			}
		}
		else
		{
			m_nSkin = 0;
		}
	}

	// We check against new-style (special flag to indicate goldification) and old style (custom damage type)
	// to maintain old demos involving the golden wrench.
	if ( m_bGoldRagdoll || m_iDamageCustom == TF_DMG_CUSTOM_GOLD_WRENCH )
	{
		EmitSound( "Saxxy.TurnGold" );
		m_bFixedConstraints = true;
	}

	if ( m_bIceRagdoll )
	{
		EmitSound( "Icicle.TurnToIce" );
		ParticleProp()->Create( "xms_icicle_impact_dryice", PATTACH_ABSORIGIN_FOLLOW );
		m_freezeTimer.Start( RandomFloat( 0.1f, 0.75f ) );
		m_frozenTimer.Start( RandomFloat( 9.0f, 11.0f ) );
	}

#ifdef _DEBUG
	DevMsg( 2, "CreateTFRagdoll %d %d\n", gpGlobals->framecount, pPlayer ? pPlayer->entindex() : 0 );
#endif
	
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// Move my current model instance to the ragdoll's so decals are preserved.
		pPlayer->SnatchModelInstance( this );

		VarMapping_t *varMap = GetVarMapping();

		// Copy all the interpolated vars from the player entity.
		// The entity uses the interpolated history to get bone velocity.		
		if ( !pPlayer->IsLocalPlayer() && pPlayer->IsInterpolationEnabled() )
		{
			Interp_Copy( pPlayer );

			SetAbsAngles( pPlayer->GetRenderAngles() );
			GetRotationInterpolator().Reset();

			m_flAnimTime = pPlayer->m_flAnimTime;
			SetSequence( pPlayer->GetSequence() );
			m_flPlaybackRate = pPlayer->GetPlaybackRate();
		}
		else
		{
			// This is the local player, so set them in a default
			// pose and slam their velocity, angles and origin
			SetAbsOrigin( /* m_vecRagdollOrigin : */ pPlayer->GetRenderOrigin() );			
			SetAbsAngles( pPlayer->GetRenderAngles() );
			SetAbsVelocity( m_vecRagdollVelocity );

			// Hack! Find a neutral standing pose or use the idle.
			int iSeq = LookupSequence( "RagdollSpawn" );
			if ( iSeq == -1 )
			{
				Assert( false );
				iSeq = 0;
			}			
			SetSequence( iSeq );
			SetCycle( 0.0 );

			Interp_Reset( varMap );
		}

		if ( !m_bFeignDeath || m_bWasDisguised )
		{
//			pPlayer->RecalcBodygroupsIfDirty();
			m_nBody = pPlayer->GetBody();
		}
	}
	else
	{
		// Overwrite network origin so later interpolation will use this position.
		SetNetworkOrigin( m_vecRagdollOrigin );
		SetAbsOrigin( m_vecRagdollOrigin );
		SetAbsVelocity( m_vecRagdollVelocity );

		Interp_Reset( GetVarMapping() );
	}

	if ( IsCloaked() )
	{
		AddEffects( EF_NOSHADOW );
	}

	// Play a death anim depending on the custom damage type.
	bool bPlayDeathInAir = false;
	int iDeathSeq = -1;
	if ( pPlayer && !m_bGoldRagdoll )
	{
		iDeathSeq = pPlayer->m_Shared.GetSequenceForDeath( this, m_bBurning, m_iDamageCustom );

		if ( m_bDissolving && !m_bGib )
		{
			bPlayDeathInAir = true;
			iDeathSeq = LookupSequence( "dieviolent" );
		}

		// did we find a death sequence?
		if ( iDeathSeq > -1 && (m_iDamageCustom != TF_DMG_CUSTOM_TAUNTATK_BARBARIAN_SWING) &&
			(m_iDamageCustom != TF_DMG_CUSTOM_TAUNTATK_ENGINEER_GUITAR_SMASH) && (m_iDamageCustom != TF_DMG_CUSTOM_TAUNTATK_ALLCLASS_GUITAR_RIFF) )
		{
			// we only want to show the death anims 25% of the time, unless this is a demoman kill taunt
			// always play backstab animations for the ice ragdoll
			if ( !m_bIceRagdoll && !tf_always_deathanim.GetBool() && (RandomFloat( 0, 1 ) > 0.25f) )
			{
				iDeathSeq = -1;
			}
		}
	}

	bool bPlayDeathAnim = cl_ragdoll_physics_enable.GetBool() && (iDeathSeq > -1) && pPlayer;

	if ( !m_bOnGround && bPlayDeathAnim && !bPlayDeathInAir )
		bPlayDeathAnim = false; // Don't play most death anims in the air (headshot, etc).

	if ( bPlayDeathAnim )
	{
		// Set our position for a death anim.
		SetAbsOrigin( pPlayer->GetNetworkOrigin() );			
		SetAbsAngles( pPlayer->GetNetworkAngles() );
		SetAbsVelocity( Vector(0,0,0) );
		m_vecForce = Vector(0,0,0);

		// Play the death anim.
		ResetSequence( iDeathSeq );
		m_bDeathAnim = true;
	}
	else if ( m_bIceRagdoll )
	{
		// couldn't play death anim because we were in midair - go ridig immediately
		m_freezeTimer.Invalidate();
		m_frozenTimer.Invalidate();
		m_bFixedConstraints = true;
	}

	// Fade out the ragdoll in a while
	StartFadeOut( cl_ragdoll_fade_time.GetFloat() );
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	// Copy over impact attachments.
	if ( pPlayer )
	{
		pPlayer->MoveBoneAttachments( this );
	}

	if ( m_iDamageCustom == TF_DMG_CUSTOM_KART )
	{
		m_vecForce *= 100.0f;
		SetAbsVelocity( GetAbsVelocity() + m_vecForce );
		ApplyAbsVelocityImpulse( m_vecForce );
	}

	// Save ragdoll information.
	if ( cl_ragdoll_physics_enable.GetBool() && !m_bDeathAnim )
	{
		// Make us a ragdoll..
		m_nRenderFX = kRenderFxRagdoll;

		matrix3x4_t boneDelta0[MAXSTUDIOBONES];
		matrix3x4_t boneDelta1[MAXSTUDIOBONES];
		matrix3x4_t currentBones[MAXSTUDIOBONES];
		const float boneDt = 0.05f;

		// We have to make sure that we're initting this client ragdoll off of the same model.
		// GetRagdollInitBoneArrays uses the *player* Hdr, which may be a different model than
		// the ragdoll Hdr, if we try to create a ragdoll in the same frame that the player
		// changes their player model.
		CStudioHdr *pRagdollHdr = GetModelPtr();
		CStudioHdr *pPlayerHdr = pPlayer ? pPlayer->GetModelPtr() : NULL;

		bool bChangedModel = false;

		if ( pRagdollHdr && pPlayerHdr )
		{
			bChangedModel = pRagdollHdr->GetVirtualModel() != pPlayerHdr->GetVirtualModel();

			// Assert( !bChangedModel && "C_TFRagdoll::CreateTFRagdoll: Trying to create ragdoll with a different model than the player it's based on" );
		}

		bool bBoneArraysInited;
		if ( pPlayer && !pPlayer->IsDormant() && !bChangedModel )
		{
			bBoneArraysInited = pPlayer->GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
		}
		else
		{
			bBoneArraysInited = GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
		}
		
		if ( bBoneArraysInited )
		{
			InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt, m_bFixedConstraints );
		}
	}
	else
	{
		ClientLeafSystem()->SetRenderGroup( GetRenderHandle(), RENDER_GROUP_TRANSLUCENT_ENTITY );
	}

	if ( m_bBurning )
	{
		m_flBurnEffectStartTime = gpGlobals->curtime;
		ParticleProp()->Create( "burningplayer_corpse", PATTACH_ABSORIGIN_FOLLOW );
	}

	if ( m_bElectrocuted )
	{
		const char *pEffectName = ( m_iTeam == TF_TEAM_RED ) ? "electrocuted_red" : "electrocuted_blue";
		ParticleProp()->Create( pEffectName, PATTACH_ABSORIGIN_FOLLOW );
		C_BaseEntity::EmitSound( "TFPlayer.MedicChargedDeath" );
	}

	if ( m_bBecomeAsh && !m_bDissolving && !m_bGib )
	{
		ParticleProp()->Create( "drg_fiery_death", PATTACH_ABSORIGIN_FOLLOW );
		m_flTimeToDissolve = 0.5f;
	}

	// Birthday mode.
	if ( pPlayer && TFGameRules() && TFGameRules()->IsBirthday() )
	{
		AngularImpulse angularImpulse( RandomFloat( 0.0f, 120.0f ), RandomFloat( 0.0f, 120.0f ), 0.0 );
		breakablepropparams_t breakParams( m_vecRagdollOrigin, GetRenderAngles(), m_vecRagdollVelocity, angularImpulse );
		breakParams.impactEnergyScale = 1.0f;
		pPlayer->DropPartyHat( breakParams, m_vecRagdollVelocity.GetForModify() );
	}

	const char *materialOverrideFilename = NULL;

	if ( m_bFixedConstraints )
	{
		if ( m_bGoldRagdoll )
		{
			// Gold texture...we've been turned into a golden corpse!
			materialOverrideFilename = "models/player/shared/gold_player.vmt";
		}
	}

	if ( m_bIceRagdoll )
	{
		// Ice texture...we've been turned into an ice statue!
		materialOverrideFilename = "models/player/shared/ice_player.vmt";
	}

	if ( materialOverrideFilename )
	{
		// Ice texture...we've been turned into an ice statue!
		m_MaterialOverride.Init( materialOverrideFilename, TEXTURE_GROUP_CLIENT_EFFECTS );
	}
}

float C_TFRagdoll::FrameAdvance( float flInterval )
{
	// if we're in the process of becoming an ice statue, freeze
	if ( m_freezeTimer.HasStarted() && !m_freezeTimer.IsElapsed() )
	{
		// play the backstab anim until the timer is up
		return BaseClass::FrameAdvance( flInterval );
	}

	if ( m_frozenTimer.HasStarted() )
	{
		if ( m_frozenTimer.IsElapsed() )
		{
			// holding frozen time is up - turn to a stiff ragdoll and fall over
			m_frozenTimer.Invalidate();

			m_nRenderFX = kRenderFxRagdoll;

			matrix3x4_t boneDelta0[MAXSTUDIOBONES];
			matrix3x4_t boneDelta1[MAXSTUDIOBONES];
			matrix3x4_t currentBones[MAXSTUDIOBONES];
			const float boneDt = 0.1f;
			GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
			InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt, true );

			SetAbsVelocity( Vector( 0,0,0 ) );
			m_bRagdollOn = true;
		}
		else
		{
			// don't move at all
			return 0.0f;
		}
	}

	float fRes = BaseClass::FrameAdvance( flInterval );

	if ( !m_bRagdollOn && IsSequenceFinished() && m_bDeathAnim )
	{
		m_nRenderFX = kRenderFxRagdoll;

		matrix3x4_t boneDelta0[MAXSTUDIOBONES];
		matrix3x4_t boneDelta1[MAXSTUDIOBONES];
		matrix3x4_t currentBones[MAXSTUDIOBONES];
		const float boneDt = 0.1f;
		if ( !GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt ) )
		{
			Warning( "C_TFRagdoll::FrameAdvance GetRagdollInitBoneArrays failed.\n" );
		}
		else
		{
			InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );
		}

		SetAbsVelocity( Vector( 0,0,0 ) );
		m_bRagdollOn = true;

		// Make it fade out.
		StartFadeOut( cl_ragdoll_fade_time.GetFloat() );
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	return fRes;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFRagdoll::CreateTFHeadGib( void )
{
	C_TFPlayer *pPlayer = NULL;
	EHANDLE hPlayer = GetPlayerHandle();
	if ( hPlayer )
	{
		pPlayer = dynamic_cast<C_TFPlayer*>( hPlayer.Get() );
	}
	if ( pPlayer && ((pPlayer->m_hFirstGib == NULL) || m_bFeignDeath) )
	{
		Vector vecVelocity = m_vecForce + m_vecRagdollVelocity;
		VectorNormalize( vecVelocity );

		pPlayer->CreatePlayerGibs( m_vecRagdollOrigin, vecVelocity, m_vecForce.Length(), m_bBurning, true );
		// Decap Death Camera is disorienting on range Decaps (aka bullets)
		// Use normal Deathcam
		if ( m_iDamageCustom == TF_DMG_CUSTOM_HEADSHOT_DECAPITATION )
		{
			pPlayer->m_hHeadGib = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFRagdoll::CreateTFGibs( bool bDestroyRagdoll, bool bCurrentPosition )
{
	C_TFPlayer *pPlayer = NULL;
	EHANDLE hPlayer = GetPlayerHandle();
	if ( hPlayer )
	{
		pPlayer = dynamic_cast<C_TFPlayer*>( hPlayer.Get() );
	}
	
	// SEALTODO
	//if ( pPlayer && pPlayer->HasBombinomiconEffectOnDeath() )
	//{
	//	m_vecForce *= 2.0f;
	//	m_vecForce.z *= 3.0f;

	//	DispatchParticleEffect( TFGameRules()->IsHolidayActive( kHoliday_Halloween ) ? "bombinomicon_burningdebris_halloween" : "bombinomicon_burningdebris", 
	//							bCurrentPosition ? GetAbsOrigin() : m_vecRagdollOrigin, GetAbsAngles() );
	//	EmitSound( "Bombinomicon.Explode" );
	//}

	if ( pPlayer && ((pPlayer->m_hFirstGib == NULL) || m_bFeignDeath) )
	{
		Vector vecVelocity = m_vecForce + m_vecRagdollVelocity;
		VectorNormalize( vecVelocity );
		pPlayer->CreatePlayerGibs( bCurrentPosition ? pPlayer->GetRenderOrigin() : m_vecRagdollOrigin, vecVelocity, m_vecForce.Length(), m_bBurning );
	}

	if ( pPlayer )
	{
		if ( TFGameRules() && TFGameRules()->IsBirthday() )
		{
			DispatchParticleEffect( "bday_confetti", pPlayer->GetAbsOrigin() + Vector(0,0,32), vec3_angle );

			if ( TFGameRules() && TFGameRules()->IsBirthday() )
			{
				C_BaseEntity::EmitSound( "Game.HappyBirthday" );
			}
		}
		else if ( m_bCritOnHardHit && !UTIL_IsLowViolence() )
		{
			DispatchParticleEffect( "tfc_sniper_mist", pPlayer->WorldSpaceCenter(), vec3_angle );
		}
	}

	if ( bDestroyRagdoll )
	{
		EndFadeOut();
	}
	else
	{
		SetRenderMode( kRenderNone );
		UpdateVisibility();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void C_TFRagdoll::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		bool bCreateRagdoll = true;

		// Get the player.
		EHANDLE hPlayer = GetPlayerHandle();
		if ( hPlayer )
		{
			// If we're getting the initial update for this player (e.g., after resetting entities after
			//  lots of packet loss, then don't create gibs, ragdolls if the player and it's gib/ragdoll
			//  both show up on same frame.
			if ( abs( hPlayer->GetCreationTick() - gpGlobals->tickcount ) < TIME_TO_TICKS( 1.0f ) )
			{
				bCreateRagdoll = false;
			}
		}
		else if ( C_BasePlayer::GetLocalPlayer() )
		{
			// Ditto for recreation of the local player
			if ( abs( C_BasePlayer::GetLocalPlayer()->GetCreationTick() - gpGlobals->tickcount ) < TIME_TO_TICKS( 1.0f ) )
			{
				bCreateRagdoll = false;
			}
		}

		// Prevent replays from creating ragdolls on the first frame of playback after skipping through playback.
		// If a player died (leaving a ragdoll) previous to the first frame of replay playback,
		// their ragdoll wasn't yet initialized because OnDataChanged events are queued but not processed
		// until the first render. 
		if ( engine->IsPlayingDemo() )
		{
			bCreateRagdoll = !m_bCreatedWhilePlaybackSkipping;
		}

		if ( GetDamageCustom() == TF_DMG_CUSTOM_TAUNTATK_GRAND_SLAM )
		{
			m_bBatted = true;
		}
	
#if 0 // SEALTODO
		C_TFPlayer *pPlayer = ToTFPlayer( hPlayer.Get() );
		bool bMiniBoss = ( pPlayer && pPlayer->IsMiniBoss() ) ? true : false;
#else
		bool bMiniBoss = false;
#endif

		if ( GetDamageCustom() == TF_DMG_CUSTOM_PLASMA )
		{
			if ( !m_bBecomeAsh && !bMiniBoss )
			{
				m_bDissolving = true;
			}

			m_bGib = false;
		}

		if ( GetDamageCustom() == TF_DMG_CUSTOM_PLASMA_CHARGED )
		{
			if ( !m_bBecomeAsh && !bMiniBoss )
			{
				m_bDissolving = true;
			}

			m_bGib = true;
			SetNextClientThink( CLIENT_THINK_ALWAYS );
		}

		if ( bCreateRagdoll )
		{
			if ( m_bGib )
			{
				CreateTFGibs( !m_bDissolving );
			}
			else
			{
				CreateTFRagdoll();
				if ( IsDecapitation() )
				{
					CreateTFHeadGib();
					EmitSound( "TFPlayer.Decapitated" );

					bool bBlood = true;
					if ( UTIL_IsLowViolence() )
					{
						bBlood = false;
					}

					if ( bBlood )
					{
						ParticleProp()->Create( "blood_decap", PATTACH_POINT_FOLLOW, "head" );
					}
				}
			}

			m_bNoModelParticles = true;	
		}
	}
	else 
	{
		if ( !cl_ragdoll_physics_enable.GetBool() )
		{
			// Don't let it set us back to a ragdoll with data from the server.
			m_nRenderFX = kRenderFxNone;
		}
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int C_TFRagdoll::InternalDrawModel( int flags )
{
	if ( m_MaterialOverride.IsValid() )
	{
		modelrender->ForcedMaterialOverride( m_MaterialOverride );
	}

	int ret = BaseClass::InternalDrawModel( flags );

	if ( m_MaterialOverride.IsValid() )
	{
		modelrender->ForcedMaterialOverride( NULL );
	}

	return ret;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool C_TFRagdoll::IsDecapitation()
{
	return (cl_ragdoll_fade_time.GetFloat() > 5.f) && 
		((m_iDamageCustom == TF_DMG_CUSTOM_DECAPITATION) 
		|| (m_iDamageCustom == TF_DMG_CUSTOM_TAUNTATK_BARBARIAN_SWING)
		|| (m_iDamageCustom == TF_DMG_CUSTOM_DECAPITATION_BOSS) 
		|| (m_iDamageCustom == TF_DMG_CUSTOM_HEADSHOT_DECAPITATION)
		|| (m_iDamageCustom == TF_DMG_CUSTOM_MERASMUS_DECAPITATION) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool C_TFRagdoll::IsHeadSmash()
{
	return ((cl_ragdoll_fade_time.GetFloat() > 5.f) && (m_iDamageCustom == TF_DMG_CUSTOM_TAUNTATK_ENGINEER_GUITAR_SMASH));
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool C_TFRagdoll::GetAttachment( int iAttachment, matrix3x4_t &attachmentToWorld )
{
	int iHeadAttachment = LookupAttachment( "head" );
	if ( IsDecapitation() && (iAttachment == iHeadAttachment) )
	{
		MatrixCopy( m_mHeadAttachment, attachmentToWorld );
		return true;
	}
	else
	{
		return BaseClass::GetAttachment( iAttachment, attachmentToWorld );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IRagdoll*
//-----------------------------------------------------------------------------
IRagdoll* C_TFRagdoll::GetIRagdoll() const
{
	return m_pRagdoll;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_TFRagdoll::IsRagdollVisible()
{
	Vector vMins = Vector(-1,-1,-1);	//WorldAlignMins();
	Vector vMaxs = Vector(1,1,1);	//WorldAlignMaxs();
		
	Vector origin = GetAbsOrigin();
	
	if( !engine->IsBoxInViewCluster( vMins + origin, vMaxs + origin) )
	{
		return false;
	}
	else if( engine->CullBox( vMins + origin, vMaxs + origin ) )
	{
		return false;
	}

	return true;
}

#define DISSOLVE_FADE_IN_START_TIME			0.0f
#define DISSOLVE_FADE_IN_END_TIME			1.0f
#define DISSOLVE_FADE_OUT_MODEL_START_TIME	1.9f
#define DISSOLVE_FADE_OUT_MODEL_END_TIME	2.0f
#define DISSOLVE_FADE_OUT_START_TIME		2.0f
#define DISSOLVE_FADE_OUT_END_TIME			2.0f

void C_TFRagdoll::ClientThink( void )
{
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	// Store off the un-shrunken head location for blood spurts.
	if ( IsDecapitation() ) 
	{
		int iAttach = LookupAttachment( "head" );

		m_bBaseTransform = true;
		BaseClass::GetAttachment( iAttach, m_mHeadAttachment );
		m_bBaseTransform = false;

		m_BoneAccessor.SetReadableBones( 0 );
		SetupBones( NULL, -1, BONE_USED_BY_ATTACHMENT, gpGlobals->curtime );
	}

	if ( m_bCloaked && m_flPercentInvisible < 1.f )
	{
		m_flPercentInvisible += gpGlobals->frametime;
		if ( m_flPercentInvisible > 1.f )
		{
			m_flPercentInvisible = 1.f;
		}
	}

	C_TFPlayer *pPlayer = ToTFPlayer( GetPlayerHandle() );
#if 0 // SEALTODO
	bool bBombinomicon = ( pPlayer && pPlayer->HasBombinomiconEffectOnDeath() );
#else
	bool bBombinomicon = false;
#endif

	if ( !m_bGib )
	{
		if ( m_bDissolving )
		{
			m_bDissolving = false;
			m_flTimeToDissolve = 1.2f;

			DissolveEntity( this );
			EmitSound( "TFPlayer.Dissolve" );
		}
		else if ( bBombinomicon && ( GetFlags() & FL_DISSOLVING ) )
		{
			m_flTimeToDissolve -= gpGlobals->frametime;
			if ( m_flTimeToDissolve <= 0 )
			{
				CreateTFGibs( true, true );
			}
		}
		else if ( m_bBecomeAsh )
		{
			m_flTimeToDissolve -= gpGlobals->frametime;
			if ( m_flTimeToDissolve <= 0 )
			{
				if ( bBombinomicon )
				{
					CreateTFGibs( true, true );
				}
				else
				{
					// Hide the ragdoll and stop everything but the ash particle effect
					AddEffects( EF_NODRAW );
					ParticleProp()->StopParticlesNamed( "drg_fiery_death", true, true );
				}
				return;
			}
		}
		else if ( bBombinomicon )
		{
			m_flTimeToDissolve -= gpGlobals->frametime;
			if ( m_flTimeToDissolve <= 0 )
			{
				CreateTFGibs( true, true );
				return;
			}
		}
	}
	// Gibbing
	else
	{
		if ( m_bDissolving )
		{
			m_flTimeToDissolve -= gpGlobals->frametime;
			if ( m_flTimeToDissolve <= 0 )
			{
				m_bDissolving = false;

				if ( pPlayer )
				{
					if ( bBombinomicon )
					{
						CreateTFGibs( true, true );
					}
					else
					{
						for ( int i=0; i<pPlayer->m_hSpawnedGibs.Count(); i++ )
						{
							C_BaseEntity* pGib = pPlayer->m_hSpawnedGibs[i].Get();
							if ( pGib )
							{
								pGib->SetAbsVelocity( vec3_origin );
								DissolveEntity( pGib );
								pGib->ParticleProp()->StopParticlesInvolving( pGib );
							}
						}
					}
				}
				EndFadeOut();
			}
			return;
		}
	}

	// Fade us away...
	if ( m_bFadingOut == true )
	{
		int iAlpha = GetRenderColor().a;
		int iFadeSpeed = 600.0f;

		iAlpha = MAX( iAlpha - ( iFadeSpeed * gpGlobals->frametime ), 0 );

		SetRenderMode( kRenderTransAlpha );
		SetRenderColorA( iAlpha );

		if ( iAlpha == 0 )
		{
			// Remove clientside ragdoll.
			EndFadeOut();
		}
		return;
	}

	// If the player is looking at us, delay the fade.
	if ( IsRagdollVisible() )
	{
		if ( cl_ragdoll_forcefade.GetBool() )
		{
			m_bFadingOut = true;
			float flDelay = cl_ragdoll_fade_time.GetFloat() * 0.33f;
			m_fDeathTime = gpGlobals->curtime + flDelay;

			RemoveAllDecals();
		}

		// Fade out after the specified delay.
		StartFadeOut( cl_ragdoll_fade_time.GetFloat() * 0.33f );
		return;
	}

	// Remove us if our death time has passed.
	if ( m_fDeathTime < gpGlobals->curtime )
	{
		EndFadeOut();
		return;
	}

	// Fire an event if we were batted by the scout's taunt kill and we have come to rest.
	if ( m_bBatted )
	{
		Vector vVelocity;
		EstimateAbsVelocity( vVelocity );
		if ( vVelocity.LengthSqr() == 0.f )
		{
			m_bBatted = false;
			IGameEvent *event = gameeventmanager->CreateEvent( "scout_slamdoll_landed" );
			if ( event )
			{
				Vector absOrigin = GetAbsOrigin();
				event->SetInt( "target_index", m_iPlayerIndex );
				event->SetFloat( "x", absOrigin.x );
				event->SetFloat( "y", absOrigin.y );
				event->SetFloat( "z", absOrigin.z );
				gameeventmanager->FireEventClientSide( event );
			}
		}
	}
}

// Deal with recording
void C_TFRagdoll::GetToolRecordingState( KeyValues *msg )
{
#ifndef _XBOX
	BaseClass::GetToolRecordingState( msg );

	if ( m_MaterialOverride.IsValid() )
	{
		msg->SetString( "materialOverride", m_MaterialOverride->GetName() );
	}
#endif
}

void C_TFRagdoll::DissolveEntity( CBaseEntity* pEnt )
{
	C_EntityDissolve *pDissolve = DissolveEffect( pEnt, gpGlobals->curtime );
	if ( pDissolve )
	{
		pDissolve->SetRenderMode( kRenderTransColor );
		pDissolve->m_nRenderFX = kRenderFxNone;
		pDissolve->SetRenderColor( 255, 255, 255, 255 );

		Vector vColor;
		if ( m_iTeam == TF_TEAM_BLUE )
		{
			vColor = TF_PARTICLE_WEAPON_RED_1 * 255;
			pDissolve->SetEffectColor( vColor );
		}
		else
		{
			vColor = TF_PARTICLE_WEAPON_BLUE_1 * 255;
			pDissolve->SetEffectColor( vColor );
		}

		pDissolve->m_vDissolverOrigin = GetAbsOrigin();

		pDissolve->m_flFadeInStart = DISSOLVE_FADE_IN_START_TIME;
		pDissolve->m_flFadeInLength = DISSOLVE_FADE_IN_END_TIME - DISSOLVE_FADE_IN_START_TIME;

		pDissolve->m_flFadeOutModelStart = DISSOLVE_FADE_OUT_MODEL_START_TIME;
		pDissolve->m_flFadeOutModelLength = DISSOLVE_FADE_OUT_MODEL_END_TIME - DISSOLVE_FADE_OUT_MODEL_START_TIME;

		pDissolve->m_flFadeOutStart = DISSOLVE_FADE_OUT_START_TIME;
		pDissolve->m_flFadeOutLength = DISSOLVE_FADE_OUT_END_TIME - DISSOLVE_FADE_OUT_START_TIME;
	}
}

void C_TFRagdoll::StartFadeOut( float fDelay )
{
	if ( !cl_ragdoll_forcefade.GetBool() )
	{
		m_fDeathTime = gpGlobals->curtime + fDelay;
	}
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}


void C_TFRagdoll::EndFadeOut()
{
	SetNextClientThink( CLIENT_THINK_NEVER );
	ClearRagdoll();
	SetRenderMode( kRenderNone );
	UpdateVisibility();
	DestroyBoneAttachments();

	// Remove attached effect entity
	C_BaseEntity *pEffect = GetEffectEntity();
	if ( pEffect )
	{
		pEffect->SUB_Remove();
	}

	ParticleProp()->StopEmission();
}


//-----------------------------------------------------------------------------
// Purpose: Used for spy invisiblity material
//-----------------------------------------------------------------------------
class CSpyInvisProxy : public CBaseInvisMaterialProxy
{
public:
	CSpyInvisProxy( void );
	virtual bool		Init( IMaterial *pMaterial, KeyValues* pKeyValues ) OVERRIDE;
	virtual void		OnBind( C_BaseEntity *pBaseEntity ) OVERRIDE;
	virtual void		OnBindNotEntity( void *pRenderable ) OVERRIDE;

private:
	IMaterialVar		*m_pCloakColorTint;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSpyInvisProxy::CSpyInvisProxy( void )
{
	m_pCloakColorTint = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get pointer to the color value
// Input  : *pMaterial - 
//-----------------------------------------------------------------------------
bool CSpyInvisProxy::Init( IMaterial *pMaterial, KeyValues* pKeyValues )
{
	// Need to get the material var
	bool bInvis = CBaseInvisMaterialProxy::Init( pMaterial, pKeyValues );

	bool bTint;
	m_pCloakColorTint = pMaterial->FindVar( "$cloakColorTint", &bTint );

	return ( bInvis && bTint );
}

ConVar tf_teammate_max_invis( "tf_teammate_max_invis", "0.95", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
//-----------------------------------------------------------------------------
void CSpyInvisProxy::OnBind( C_BaseEntity *pBaseEntity )
{
	if( !m_pPercentInvisible || !m_pCloakColorTint )
		return;

	float fInvis = 0.0f;

	C_TFPlayer *pPlayer = ToTFPlayer( pBaseEntity );

	if ( !pPlayer )
	{
		C_TFPlayer *pOwningPlayer = ToTFPlayer( pBaseEntity->GetOwnerEntity() );

		C_TFRagdoll *pRagdoll = dynamic_cast< C_TFRagdoll* >( pBaseEntity );
		if ( pRagdoll && pRagdoll->IsCloaked() )
		{
			fInvis = pRagdoll->GetPercentInvisible();
		}
		else if ( pOwningPlayer )
		{
			// mimic the owner's invisibility
			fInvis = pOwningPlayer->GetEffectiveInvisibilityLevel();
		}
	}
	else
	{
		float r = 1.0f, g = 1.0f, b = 1.0f;
		fInvis = pPlayer->GetEffectiveInvisibilityLevel();

		switch( pPlayer->GetTeamNumber() )
		{
		case TF_TEAM_RED:
			r = 1.0; g = 0.5; b = 0.4;
			break;

		case TF_TEAM_BLUE:
		default:
			r = 0.4; g = 0.5; b = 1.0;
			break;
		}

		m_pCloakColorTint->SetVecValue( r, g, b );
	}

	m_pPercentInvisible->SetFloatValue( fInvis );
}

void CSpyInvisProxy::OnBindNotEntity( void *pRenderable )
{
	CBaseInvisMaterialProxy::OnBindNotEntity( pRenderable );

	if ( m_pCloakColorTint )
	{
		m_pCloakColorTint->SetVecValue( 1.f, 1.f, 1.f );
	}
}

EXPOSE_INTERFACE( CSpyInvisProxy, IMaterialProxy, "spy_invis" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: Used for invulnerability material
//			Returns 1 if the player is invulnerable, and 0 if the player is losing / doesn't have invuln.
//-----------------------------------------------------------------------------
class CProxyInvulnLevel : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		float flResult = 1.0;

		C_TFPlayer *pPlayer = NULL;
		C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
		if ( !pEntity )
		{
			m_pResult->SetFloatValue( flResult );
			return;
		}

		if ( pEntity->IsPlayer()  )
		{
			pPlayer = dynamic_cast< C_TFPlayer* >( pEntity );
		}
		else
		{
			IHasOwner *pOwnerInterface = dynamic_cast< IHasOwner* >( pEntity );
			if ( pOwnerInterface )
			{
				pPlayer = ToTFPlayer( pOwnerInterface->GetOwnerViaInterface() );
			}
		}

		if ( pPlayer && pPlayer->m_Shared.IsInvulnerable() && pPlayer->m_Shared.InCond( TF_COND_INVULNERABLE_WEARINGOFF ) )
		{
			flResult = 0.0;
		}

		m_pResult->SetFloatValue( flResult );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyInvulnLevel, IMaterialProxy, "InvulnLevel" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: Used for burning material on player models
//			Returns 0.0->1.0 for level of burn to show on player skin
//-----------------------------------------------------------------------------
class CProxyBurnLevel : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		if ( !pC_BaseEntity )
			return;

		C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
		if ( !pEntity )
			return;

		// default to zero
		float flBurnStartTime = 0;
			
		C_TFPlayer *pPlayer = dynamic_cast< C_TFPlayer* >( pEntity );
		if ( pPlayer )		
		{
			// is the player burning?
			if (  pPlayer->m_Shared.InCond( TF_COND_BURNING ) )
			{
				flBurnStartTime = pPlayer->m_flBurnEffectStartTime;
			}
		}
		else
		{
			// is the ragdoll burning?
			C_TFRagdoll *pRagDoll = dynamic_cast< C_TFRagdoll* >( pEntity );
			if ( pRagDoll )
			{
				flBurnStartTime = pRagDoll->GetBurnStartTime();
			}
		}

		float flResult = 0.0;
		
		// if player/ragdoll is burning, set the burn level on the skin
		if ( flBurnStartTime > 0 )
		{
			float flBurnPeakTime = flBurnStartTime + 0.3;
			float flTempResult;
			if ( gpGlobals->curtime < flBurnPeakTime )
			{
				// fade in from 0->1 in 0.3 seconds
				flTempResult = RemapValClamped( gpGlobals->curtime, flBurnStartTime, flBurnPeakTime, 0.0, 1.0 );
			}
			else
			{
				// fade out from 1->0 in the remaining time until flame extinguished
				flTempResult = RemapValClamped( gpGlobals->curtime, flBurnPeakTime, flBurnStartTime + TF_BURNING_FLAME_LIFE, 1.0, 0.0 );
			}	

			// We have to do some more calc here instead of in materialvars.
			flResult = 1.0 - abs( flTempResult - 1.0 );
		}

		m_pResult->SetFloatValue( flResult );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyBurnLevel, IMaterialProxy, "BurnLevel" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: Used for turning player models yellow (jarate) 
//			Returns 0.0->1.0 for level of yellow to show on player skin
//-----------------------------------------------------------------------------
class CProxyUrineLevel : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		// default to zero
		Vector vResult = Vector( 1, 1, 1 );

		C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
		if ( !pEntity )
		{
			m_pResult->SetVecValue( vResult.x, vResult.y, vResult.z );
			return;
		}

		C_TFPlayer *pPlayer = NULL;
		if ( pEntity->IsPlayer() )
		{
			pPlayer = assert_cast< C_TFPlayer* >( pEntity );
		}
		else if ( pEntity->GetOwnerEntity() && pEntity->GetOwnerEntity()->IsPlayer() )
		{
			pPlayer = assert_cast< C_TFPlayer* >( pEntity->GetOwnerEntity() );
		}

		if ( pPlayer )
		{
			// is the player peed on?
			if (  pPlayer->m_Shared.InCond( TF_COND_URINE ) )
			{
				int iVisibleTeam = pPlayer->GetTeamNumber();
				if ( pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
				{
					if ( !pPlayer->IsLocalPlayer() && iVisibleTeam != GetLocalPlayerTeam() )
					{
						iVisibleTeam = pPlayer->m_Shared.GetDisguiseTeam();
					}
				}
				if ( iVisibleTeam == TF_TEAM_RED )
				{
					vResult = Vector ( 6, 9, 2 );
				}
				else	
				{
					vResult = Vector ( 7, 5, 1 );
				}
			}
			else
			{
				vResult = Vector( 1, 1, 1 );
			}
		}

		m_pResult->SetVecValue( vResult.x, vResult.y, vResult.z );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyUrineLevel, IMaterialProxy, "YellowLevel" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: CritBoosted FX
//			
//-----------------------------------------------------------------------------
class CProxyModelGlowColor : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity )
	{
		Assert( m_pResult );

		C_TFPlayer *pPlayer = NULL;
		C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
		if ( !pEntity )
		{
			Vector vResult = Vector( 1, 1, 1 );
			m_pResult->SetVecValue( vResult.x, vResult.y, vResult.z );
			return;
		}

		// default to [1 1 1]
		Vector vResult = Vector( 1, 1, 1 );
		int iVisibleTeam = 0;

		IHasOwner *pOwnerInterface = dynamic_cast< IHasOwner* >( pEntity );
		if ( pOwnerInterface )
		{
			pPlayer = ToTFPlayer( pOwnerInterface->GetOwnerViaInterface() );
		}

		if ( pPlayer )
		{
			iVisibleTeam = pPlayer->GetTeamNumber();

			if ( pPlayer->m_Shared.IsCritBoosted() )
			{
				// never show critboosted effect on a disguised spy (unless it's me)
				if ( !pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) || pPlayer->IsLocalPlayer() )
				{
					if ( iVisibleTeam == TF_TEAM_RED )
					{
						vResult = Vector ( 80, 8, 5 );
					}
					else	
					{
						vResult = Vector ( 5, 20, 80 );
					}
				}
			}
			else if ( pPlayer->m_Shared.InCond( TF_COND_OFFENSEBUFF ) || pPlayer->m_Shared.InCond( TF_COND_ENERGY_BUFF ) )
			{
				// Temporarily hijacking this proxy for buff FX.
				if ( iVisibleTeam == TF_TEAM_RED )
				{
					vResult = Vector ( 226, 150, 62 );
				}
				else
				{
					vResult = Vector( 29, 202, 135 );
				}
			}
		}
		m_pResult->SetVecValue( vResult.x, vResult.y, vResult.z );

		if ( ToolsEnabled() )
		{
			ToolFramework_RecordMaterialParams( GetMaterial() );
		}
	}
};

EXPOSE_INTERFACE( CProxyModelGlowColor, IMaterialProxy, "ModelGlowColor" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: RecvProxy that converts the Player's object UtlVector to entindexes
//-----------------------------------------------------------------------------
void RecvProxy_PlayerObjectList( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_TFPlayer *pPlayer = (C_TFPlayer*)pStruct;
	CBaseHandle *pHandle = (CBaseHandle*)(&(pPlayer->m_aObjects[pData->m_iElement])); 
	RecvProxy_IntToEHandle( pData, pStruct, pHandle );
}

void RecvProxyArrayLength_PlayerObjects( void *pStruct, int objectID, int currentArrayLength )
{
	C_TFPlayer *pPlayer = (C_TFPlayer*)pStruct;

	if ( pPlayer->m_aObjects.Count() != currentArrayLength )
	{
		pPlayer->m_aObjects.SetSize( currentArrayLength );
	}

	pPlayer->ForceUpdateObjectHudState();
}

// Day of Defeat
void RecvProxy_StunTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_TFPlayer *pPlayerData = (C_TFPlayer *) pStruct;

	if( pPlayerData != C_BasePlayer::GetLocalPlayer() )
		return;

	if ( (pPlayerData->m_flStunDuration != pData->m_Value.m_Float) && pData->m_Value.m_Float > 0 )
	{
		pPlayerData->m_flStunAlpha = 1;
	}

	pPlayerData->m_flStunDuration = pData->m_Value.m_Float;
	pPlayerData->m_flStunEffectTime = gpGlobals->curtime + pPlayerData->m_flStunDuration;
}

// specific to the local player
BEGIN_RECV_TABLE_NOBASE( C_TFPlayer, DT_TFLocalPlayerExclusive )
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropArray2( 
		RecvProxyArrayLength_PlayerObjects,
		RecvPropInt( "player_object_array_element", 0, SIZEOF_IGNORE, 0, RecvProxy_PlayerObjectList ), 
		MAX_OBJECTS_PER_PLAYER, 
		0, 
		"player_object_array"	),

	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
//	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),

	// Day of Defeat
	RecvPropFloat( RECVINFO( m_flStunDuration ), 0, RecvProxy_StunTime ),
	RecvPropFloat( RECVINFO( m_flStunMaxAlpha)),

	// Counter-Strike
	RecvPropInt( RECVINFO( m_iShotsFired ) ),

END_RECV_TABLE()

// all players except the local player
BEGIN_RECV_TABLE_NOBASE( C_TFPlayer, DT_TFNonLocalPlayerExclusive )
	RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ) ),

	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),

END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Data that gets sent to attached medics
//-----------------------------------------------------------------------------
BEGIN_RECV_TABLE_NOBASE( C_TFPlayer, DT_TFSendHealersDataTable )
	RecvPropInt( RECVINFO( m_nActiveWpnClip ) ),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT( C_TFPlayer, DT_TFPlayer, CTFPlayer )

	RecvPropBool(RECVINFO(m_bSaveMeParity)),

	// This will create a race condition will the local player, but the data will be the same so.....
	RecvPropInt( RECVINFO( m_nWaterLevel ) ),
	RecvPropEHandle( RECVINFO( m_hRagdoll ) ),
	RecvPropDataTable( RECVINFO_DT( m_PlayerClass ), 0, &REFERENCE_RECV_TABLE( DT_TFPlayerClassShared ) ),
	RecvPropDataTable( RECVINFO_DT( m_Shared ), 0, &REFERENCE_RECV_TABLE( DT_TFPlayerShared ) ),
	RecvPropEHandle( RECVINFO(m_hItem ) ),

	RecvPropDataTable( "tflocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_TFLocalPlayerExclusive) ),
	RecvPropDataTable( "tfnonlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_TFNonLocalPlayerExclusive) ),

	RecvPropInt( RECVINFO( m_nForceTauntCam ) ),
	RecvPropFloat( RECVINFO( m_flTauntYaw ) ),

	RecvPropFloat( RECVINFO( m_flLastDamageTime ) ),

	RecvPropInt( RECVINFO( m_iSpawnCounter ) ),

	RecvPropFloat( RECVINFO( m_flHeadScale ) ),
	RecvPropFloat( RECVINFO( m_flTorsoScale ) ),
	RecvPropFloat( RECVINFO( m_flHandScale ) ),

	RecvPropBool( RECVINFO( m_bGlowEnabled ) ),

	RecvPropDataTable("TFSendHealersDataTable", 0, 0, &REFERENCE_RECV_TABLE( DT_TFSendHealersDataTable ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_TFPlayer )
	DEFINE_PRED_TYPEDESCRIPTION( m_Shared, CTFPlayerShared ),
	DEFINE_PRED_FIELD( m_nSkin, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_nBody, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flPlaybackRate, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_ARRAY_TOL( m_flEncodedController, FIELD_FLOAT, MAXSTUDIOBONECTRLS, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE, 0.02f ),
	DEFINE_PRED_FIELD( m_nNewSequenceParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nResetEventsParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nMuzzleFlashParity, FIELD_CHARACTER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE  ),
	DEFINE_PRED_FIELD( m_hOffHandWeapon, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flTauntYaw, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),

	// CSS
	DEFINE_PRED_FIELD( m_iShotsFired, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

// ------------------------------------------------------------------------------------------ //
// C_TFPlayer implementation.
// ------------------------------------------------------------------------------------------ //

C_TFPlayer::C_TFPlayer() : 
	m_iv_angEyeAngles( "C_TFPlayer::m_iv_angEyeAngles" )
{
	GameAnimStateInit();
	m_Shared.Init( this );

	m_iIDEntIndex = 0;

	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );

	m_pTeleporterEffect = NULL;
	m_pBurningSound = NULL;
	m_pBurningEffect = NULL;
	m_pCritBoostEffect = NULL;
	m_flBurnEffectStartTime = 0;
	m_flBurnEffectEndTime = 0;
	m_pDisguisingEffect = NULL;
	m_pSaveMeEffect = NULL;
	m_pStunnedEffect = NULL;
	
	m_aGibs.Purge();
	m_aNormalGibs.PurgeAndDeleteElements();
	m_aSillyGibs.Purge();

	m_bCigaretteSmokeActive = false;

	m_hRagdoll.Set( NULL );

	m_iPreviousMetal = 0;
	m_bIsDisplayingNemesisIcon = false;

	m_bWasTaunting = false;
	m_angTauntPredViewAngles.Init();
	m_angTauntEngViewAngles.Init();

	m_flWaterImpactTime = 0.0f;

	m_flWaterEntryTime = 0;
	m_nOldWaterLevel = WL_NotInWater;
	m_bWaterExitEffectActive = false;

	m_bDuckJumpInterp = false;
	m_flFirstDuckJumpInterp = 0.0f;
	m_flLastDuckJumpInterp = 0.0f;
	m_flDuckJumpInterp = 0.0f;

	m_pPowerupGlowEffect = NULL;

	m_bUpdateObjectHudState = false;

	SetGameMovementType( nullptr );

	for( int i=0; i<kBonusEffect_Count; ++i )
	{
		m_flNextMiniCritEffectTime[i] = 0;
	}

	m_flHeadScale = 1.f;
	m_flTorsoScale = 1.f;
	m_flHandScale = 1.f;

	ListenForGameEvent( "player_hurt" );
	ListenForGameEvent( "player_spawn" );

	// DOD
	m_flPitchRecoilAccumulator = 0.0;
	m_flYawRecoilAccumulator = 0.0;
	m_flRecoilTimeRemaining = 0.0;

	m_flProneViewOffset = 0.0;
	m_bProneSwayingRight = true;

	// Cold breath.
	m_bColdBreathOn = false;
	m_flColdBreathTimeStart = 0.0f;
	m_flColdBreathTimeEnd = 0.0f;
	m_hColdBreathEmitter = NULL;
	m_hColdBreathMaterial = INVALID_MATERIAL_HANDLE;
}

C_TFPlayer::~C_TFPlayer()
{
	ShowNemesisIcon( false );
	if ( GetGameAnimStateType() )
		GetGameAnimStateType()->Release();

	// Day of Defeat

	// Kill the stamina sound!
	if ( m_pStaminaSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pStaminaSound );
		m_pStaminaSound = NULL;
	}

	// Cold breath.
	DestroyColdBreathEmitter();
}


C_TFPlayer* C_TFPlayer::GetLocalTFPlayer()
{
	return ToTFPlayer( C_BasePlayer::GetLocalPlayer() );
}

const QAngle& C_TFPlayer::GetRenderAngles()
{
	if ( IsRagdoll() )
	{
		return vec3_angle;
	}
	else
	{
		if ( GetGameAnimStateType() )
			return GetGameAnimStateType()->GetRenderAngles();
		else
			return vec3_angle;
	}
}

bool C_TFPlayer::CanDisplayAllSeeEffect( EAttackBonusEffects_t effect ) const
{ 
	if( effect >= EAttackBonusEffects_t(0) && effect < kBonusEffect_Count )
	{
		return gpGlobals->curtime > m_flNextMiniCritEffectTime[ effect ];
	}

	return true;
}

void C_TFPlayer::SetNextAllSeeEffectTime( EAttackBonusEffects_t effect, float flTime )
{ 
	if( effect >= EAttackBonusEffects_t(0) && effect < kBonusEffect_Count )
	{
		if ( gpGlobals->curtime > m_flNextMiniCritEffectTime[ effect ] )
		{
			m_flNextMiniCritEffectTime[ effect ] = flTime;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::UpdateOnRemove( void )
{
	// Stop the taunt.
	if ( m_bWasTaunting )
	{
		// Need to go ahead and call both. Otherwise, if we changelevel while we're taunting or 
		// otherwise in "game wants us in third person mode", we will stay in third person mode 
		// in the new map.
		TurnOffTauntCam();
		TurnOffTauntCam_Finish();
	}

	// HACK!!! ChrisG needs to fix this in the particle system.
	ParticleProp()->OwnerSetDormantTo( true );
	ParticleProp()->StopParticlesInvolving( this );

	m_Shared.RemoveAllCond();

	if ( IsLocalPlayer() )
	{
		CTFStatPanel *pStatPanel = GetStatPanel();
		pStatPanel->OnLocalPlayerRemove( this );
	}

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: returns max health for this player
//-----------------------------------------------------------------------------
int C_TFPlayer::GetMaxHealth( void ) const
{	
	return ( g_TF_PR ) ? g_TF_PR->GetMaxHealth( entindex() ) : 1;
}

//-----------------------------------------------------------------------------
// Purpose: returns max buffed health for this player
//-----------------------------------------------------------------------------
int C_TFPlayer::GetMaxHealthForBuffing( void ) const
{
	return ( g_TF_PR ) ? g_TF_PR->GetMaxHealthForBuffing( entindex() ) : 1;
}

//-----------------------------------------------------------------------------
// Deal with recording
//-----------------------------------------------------------------------------
void C_TFPlayer::GetToolRecordingState( KeyValues *msg )
{
#ifndef _XBOX
	BaseClass::GetToolRecordingState( msg );
	BaseEntityRecordingState_t *pBaseEntityState = (BaseEntityRecordingState_t*)msg->GetPtr( "baseentity" );

	bool bDormant = IsDormant();
	bool bDead = !IsAlive();
	bool bSpectator = ( GetTeamNumber() == TEAM_SPECTATOR );
	bool bNoRender = ( GetRenderMode() == kRenderNone );
	bool bDeathCam = (GetObserverMode() == OBS_MODE_DEATHCAM);
	bool bNoDraw = IsEffectActive(EF_NODRAW);

	bool bVisible = 
		!bDormant && 
		!bDead && 
		!bSpectator &&
		!bNoRender &&
		!bDeathCam &&
		!bNoDraw;

	bool changed = m_bToolRecordingVisibility != bVisible;
	// Remember state
	m_bToolRecordingVisibility = bVisible;

	pBaseEntityState->m_bVisible = bVisible;
	if ( changed && !bVisible )
	{
		// If the entity becomes invisible this frame, we still want to record a final animation sample so that we have data to interpolate
		//  toward just before the logs return "false" for visiblity.  Otherwise the animation will freeze on the last frame while the model
		//  is still able to render for just a bit.
		pBaseEntityState->m_bRecordFinalVisibleSample = true;
	}
#endif
}


void C_TFPlayer::UpdateClientSideAnimation()
{
	// Update the animation data. It does the local check here so this works when using
	// a third-person camera (and we don't have valid player angles).
	if ( GetGameAnimStateType() )
	{
		if ( this == C_TFPlayer::GetLocalTFPlayer() )
			GetGameAnimStateType()->Update( EyeAngles()[YAW], EyeAngles()[PITCH] );
		else
			GetGameAnimStateType()->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );
	}

	BaseClass::UpdateClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::SetDormant( bool bDormant )
{
	// If I'm burning, stop the burning sounds
	if ( !IsDormant() && bDormant )
	{
		if ( m_pBurningSound) 
		{
			StopBurningSound();
		}
		if ( m_bIsDisplayingNemesisIcon )
		{
			ShowNemesisIcon( false );
		}
	}

	if ( IsDormant() && !bDormant )
	{
		m_bUpdatePartyHat = true;
	}

	// Deliberately skip base combat weapon
	C_BaseEntity::SetDormant( bDormant );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	m_iOldHealth = m_iHealth;
	m_iOldPlayerClass = m_PlayerClass.GetClassIndex();
	m_iOldSpawnCounter = m_iSpawnCounter;
	m_bOldSaveMeParity = m_bSaveMeParity;
	m_nOldWaterLevel = GetWaterLevel();

	m_iOldTeam = GetTeamNumber();
	C_TFPlayerClass *pClass = GetPlayerClass();
	m_iOldClass = pClass ? pClass->GetClassIndex() : TF_CLASS_UNDEFINED;
	m_bDisguised = m_Shared.InCond( TF_COND_DISGUISED );
	m_iOldDisguiseTeam = m_Shared.GetDisguiseTeam();
	m_iOldDisguiseClass = m_Shared.GetDisguiseClass();

	m_flPrevTauntYaw = m_flTauntYaw;

	m_Shared.OnPreDataChanged();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::OnDataChanged( DataUpdateType_t updateType )
{
	// C_BaseEntity assumes we're networking the entity's angles, so pretend that it
	// networked the same value we already have.
	SetNetworkAngles( GetLocalAngles() );
	
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		InitInvulnerableMaterial();
	}
	else
	{
		if ( m_iOldTeam != GetTeamNumber() || m_iOldDisguiseTeam != m_Shared.GetDisguiseTeam() )
		{
			InitInvulnerableMaterial();
			m_bUpdatePartyHat = true;
		}

		if ( m_iOldDisguiseClass != m_Shared.GetDisguiseClass() )
		{
			RemoveAllDecals();
		}
	}

	UpdateVisibility();

	// Check for full health and remove decals.
	if ( ( m_iHealth > m_iOldHealth && m_iHealth >= GetMaxHealth() ) || m_Shared.InCond( TF_COND_INVULNERABLE ) )
	{
		// If we were just fully healed, remove all decals
		RemoveAllDecals();
	}

	if ( ( m_iOldHealth != m_iHealth ) || ( m_iOldTeam != GetTeamNumber() ) )
	{
		UpdateGlowColor();
	}

	if ( TFGameRules() && TFGameRules()->IsPowerupMode() )
	{
		if ( m_Shared.InCond( TF_COND_KING_BUFFED ) )
		{
			const char *m_szRadiusEffect;
			int nTeamNumber = GetTeamNumber();
			if ( IsPlayerClass( TF_CLASS_SPY ) && m_Shared.InCond( TF_COND_DISGUISED ) )
			{
				if ( !IsLocalPlayer() && GetTeamNumber() == GetLocalPlayerTeam() ) // Always display own team colors even when disguised, unless it's you (same rules as uber skin)
				{
					nTeamNumber = GetLocalPlayerTeam();
				}
				else
				{
					nTeamNumber = m_Shared.GetDisguiseTeam();
				}
			}

			if ( nTeamNumber == TF_TEAM_RED )
			{
				m_szRadiusEffect = "powerup_king_red";
			}
			else
			{
				m_szRadiusEffect = "powerup_king_blue";
			}
			// SEALTODO
			//if ( !m_pKingBuffRadiusEffect )
			//{
			//	m_pKingBuffRadiusEffect = ParticleProp()->Create( m_szRadiusEffect, PATTACH_ABSORIGIN_FOLLOW );
			//}
		}
		// SEALTODO
		//else if ( m_pKingBuffRadiusEffect )
		//{
		//	m_Shared.EndKingBuffRadiusEffect();
		//}

		bool bNeedsPowerupGlow = ShouldShowPowerupGlowEffect();
		bool bHasPowerupGlow = m_pPowerupGlowEffect != NULL;
		if ( bNeedsPowerupGlow != bHasPowerupGlow )
		{
			UpdateGlowEffect();
		}
	}

	// Detect class changes
	if ( m_iOldPlayerClass != m_PlayerClass.GetClassIndex() )
	{
		OnPlayerClassChange();
	}

	bool bJustSpawned = false;

	if ( m_iOldSpawnCounter != m_iSpawnCounter )
	{
		ClientPlayerRespawn();

		bJustSpawned = true;
		m_bUpdatePartyHat = true;
	}

	if ( m_bSaveMeParity != m_bOldSaveMeParity )
	{
		// Player has triggered a save me command
		CreateSaveMeEffect();
	}

	if ( m_Shared.InCond( TF_COND_BURNING ) && !m_pBurningSound )
	{
		StartBurningSound();
	}

	// See if we should show or hide nemesis icon for this player
	bool bShouldDisplayNemesisIcon = ShouldShowNemesisIcon();
	if ( bShouldDisplayNemesisIcon != m_bIsDisplayingNemesisIcon )
	{
		ShowNemesisIcon( bShouldDisplayNemesisIcon );
	}

	m_Shared.OnDataChanged();

	
	if ( m_bDisguised != m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		m_flDisguiseEndEffectStartTime = max( m_flDisguiseEndEffectStartTime, gpGlobals->curtime );

		// Remove decals.
		RemoveAllDecals();
	}

	int nNewWaterLevel = GetWaterLevel();

	if ( nNewWaterLevel != m_nOldWaterLevel )
	{
		if ( ( m_nOldWaterLevel == WL_NotInWater ) && ( nNewWaterLevel > WL_NotInWater ) )
		{
			// Set when we do a transition to/from partially in water to completely out
			m_flWaterEntryTime = gpGlobals->curtime;
		}

		// If player is now up to his eyes in water and has entered the water very recently (not just bobbing eyes in and out), play a bubble effect.
		if ( ( nNewWaterLevel == WL_Eyes ) && ( gpGlobals->curtime - m_flWaterEntryTime ) < 0.5f ) 
		{
			CNewParticleEffect *pEffect = ParticleProp()->Create( "water_playerdive", PATTACH_ABSORIGIN_FOLLOW );
			ParticleProp()->AddControlPoint( pEffect, 1, NULL, PATTACH_WORLDORIGIN, NULL, WorldSpaceCenter() );
		}		
		// If player was up to his eyes in water and is now out to waist level or less, play a water drip effect
		else if ( m_nOldWaterLevel == WL_Eyes && ( nNewWaterLevel < WL_Eyes ) && !bJustSpawned )
		{
			CNewParticleEffect *pWaterExitEffect = ParticleProp()->Create( "water_playeremerge", PATTACH_ABSORIGIN_FOLLOW );
			ParticleProp()->AddControlPoint( pWaterExitEffect, 1, this, PATTACH_ABSORIGIN_FOLLOW );
			m_bWaterExitEffectActive = true;
		}
	}

	if ( IsLocalPlayer() )
	{
		if ( updateType == DATA_UPDATE_CREATED )
		{
			SetupHeadLabelMaterials();
			GetClientVoiceMgr()->SetHeadLabelOffset( 50 );
		}

		if ( m_iOldTeam != GetTeamNumber() )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "localplayer_changeteam" );
			if ( event )
			{
				gameeventmanager->FireEventClientSide( event );
			}
			if ( IsX360() )
			{
				const char *pTeam = NULL;
				switch( GetTeamNumber() )
				{
					case TF_TEAM_RED:
						pTeam = "red";
						break;

					case TF_TEAM_BLUE:
						pTeam = "blue";
						break;

					case TEAM_SPECTATOR:
						pTeam = "spectate";
						break;
				}

				if ( pTeam )
				{
					engine->ChangeTeam( pTeam );
				}
			}
		}

		if ( !IsPlayerClass(m_iOldClass) )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "localplayer_changeclass" );
			if ( event )
			{
				event->SetInt( "updateType", updateType );
				gameeventmanager->FireEventClientSide( event );
			}
		}


		if ( m_iOldClass == TF_CLASS_SPY && 
		   ( m_bDisguised != m_Shared.InCond( TF_COND_DISGUISED ) || m_iOldDisguiseClass != m_Shared.GetDisguiseClass() ) )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "localplayer_changedisguise" );
			if ( event )
			{
				event->SetBool( "disguised", m_Shared.InCond( TF_COND_DISGUISED ) );
				gameeventmanager->FireEventClientSide( event );
			}
		}

		// If our metal amount changed, send a game event
		int iCurrentMetal = GetAmmoCount( TF_AMMO_METAL );	

		if ( iCurrentMetal != m_iPreviousMetal )
		{
			//msg
			IGameEvent *event = gameeventmanager->CreateEvent( "player_account_changed" );
			if ( event )
			{
				event->SetInt( "old_account", m_iPreviousMetal );
				event->SetInt( "new_account", iCurrentMetal );
				gameeventmanager->FireEventClientSide( event );
			}

			m_iPreviousMetal = iCurrentMetal;
		}

		// did the local player get any health?
		if ( m_iHealth > m_iOldHealth )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "localplayer_healed" );
			if ( event )
			{
				event->SetInt( "amount", m_iHealth - m_iOldHealth );
				gameeventmanager->FireEventClientSide( event );
			}
		}
	}

	// Some time in this network transmit we changed the size of the object array.
	// recalc the whole thing and update the hud
	if ( m_bUpdateObjectHudState )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "building_info_changed" );
		if ( event )
		{
			event->SetInt( "building_type", -1 );
			gameeventmanager->FireEventClientSide( event );
		}
	
		m_bUpdateObjectHudState = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::InitInvulnerableMaterial( void )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pLocalPlayer )
		return;

	const char *pszMaterial = NULL;

	int iVisibleTeam = GetTeamNumber();
	// if this player is disguised and on the other team, use disguise team
	if ( m_Shared.InCond( TF_COND_DISGUISED ) && !InSameTeam( pLocalPlayer ) )
	{
		iVisibleTeam = m_Shared.GetDisguiseTeam();
	}

	switch ( iVisibleTeam )
	{
	case TF_TEAM_BLUE:	
		pszMaterial = "models/effects/invulnfx_blue.vmt";
		break;
	case TF_TEAM_RED:	
		pszMaterial = "models/effects/invulnfx_red.vmt";
		break;
	default:
		break;
	}

	if ( pszMaterial )
	{
		m_InvulnerableMaterial.Init( pszMaterial, TEXTURE_GROUP_CLIENT_EFFECTS );
	}
	else
	{
		m_InvulnerableMaterial.Shutdown();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::StartBurningSound( void )
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	if ( !m_pBurningSound )
	{
		CLocalPlayerFilter filter;
		m_pBurningSound = controller.SoundCreate( filter, entindex(), "Player.OnFire" );
	}

	controller.Play( m_pBurningSound, 0.0, 100 );
	controller.SoundChangeVolume( m_pBurningSound, 1.0, 0.1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::StopBurningSound( void )
{
	if ( m_pBurningSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pBurningSound );
		m_pBurningSound = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::OnAddTeleported( void )
{
	if ( !m_pTeleporterEffect )
	{
		char *pEffect = NULL;

		switch( GetTeamNumber() )
		{
		case TF_TEAM_BLUE:
			pEffect = "player_recent_teleport_blue";
			break;
		case TF_TEAM_RED:
			pEffect = "player_recent_teleport_red";
			break;
		default:
			break;
		}

		if ( pEffect )
		{
			m_pTeleporterEffect = ParticleProp()->Create( pEffect, PATTACH_ABSORIGIN_FOLLOW );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::OnRemoveTeleported( void )
{
	if ( m_pTeleporterEffect )
	{
		ParticleProp()->StopEmission( m_pTeleporterEffect );
		m_pTeleporterEffect = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::OnPlayerClassChange( void )
{
	// Init the anim movement vars
	if ( GetGameAnimStateType() )
	{
		GetGameAnimStateType()->SetRunSpeed(GetPlayerClass()->GetMaxSpeed());
		GetGameAnimStateType()->SetWalkSpeed(GetPlayerClass()->GetMaxSpeed() * 0.5);
	}

	SetAppropriateCamera( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::InitPhonemeMappings()
{
	CStudioHdr *pStudio = GetModelPtr();
	if ( pStudio )
	{
		char szBasename[MAX_PATH];
		Q_StripExtension( pStudio->pszName(), szBasename, sizeof( szBasename ) );
		char szExpressionName[MAX_PATH];
		Q_snprintf( szExpressionName, sizeof( szExpressionName ), "%s/phonemes/phonemes", szBasename );
		if ( FindSceneFile( szExpressionName ) )
		{
			SetupMappings( szExpressionName );	
		}
		else
		{
			BaseClass::InitPhonemeMappings();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::ResetFlexWeights( CStudioHdr *pStudioHdr )
{
	if ( !pStudioHdr || pStudioHdr->numflexdesc() == 0 )
		return;

	// Reset the flex weights to their starting position.
	LocalFlexController_t iController;
	for ( iController = LocalFlexController_t(0); iController < pStudioHdr->numflexcontrollers(); ++iController )
	{
		SetFlexWeight( iController, 0.0f );
	}

	// Reset the prediction interpolation values.
	m_iv_flexWeight.Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CStudioHdr *C_TFPlayer::OnNewModel( void )
{
	CStudioHdr *hdr = BaseClass::OnNewModel();

	// Initialize the gibs.
	InitPlayerGibs();

	InitializePoseParams();

	// Init flexes, cancel any scenes we're playing
	ClearSceneEvents( NULL, false );

	// Reset the flex weights.
	ResetFlexWeights( hdr );

	// Reset the players animation states, gestures
	if ( GetGameAnimStateType() )
	{
		GetGameAnimStateType()->OnNewModel();
	}

	if ( hdr )
	{
		InitPhonemeMappings();
	}

	if ( IsPlayerClass( TF_CLASS_SPY ) )
	{
		m_iSpyMaskBodygroup = FindBodygroupByName( "spyMask" );
	}
	else
	{
		m_iSpyMaskBodygroup = -1;
	}

	m_bUpdatePartyHat = true;

	return hdr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::UpdatePartyHat( void )
{
	if ( TFGameRules() && TFGameRules()->IsBirthday() && !IsLocalPlayer() && IsAlive() && 
		GetTeamNumber() >= FIRST_GAME_TEAM && !IsPlayerClass(TF_CLASS_UNDEFINED) )
	{
		if ( m_hPartyHat )
		{
			m_hPartyHat->Release();
		}

		m_hPartyHat = C_PlayerAttachedModel::Create( BDAY_HAT_MODEL, this, LookupAttachment("partyhat"), vec3_origin, PAM_PERMANENT, 0 );

		// C_PlayerAttachedModel::Create can return NULL!
		if ( m_hPartyHat )
		{
			int iVisibleTeam = GetTeamNumber();
			if ( m_Shared.InCond( TF_COND_DISGUISED ) && IsEnemyPlayer() )
			{
				iVisibleTeam = m_Shared.GetDisguiseTeam();
			}
			m_hPartyHat->m_nSkin = iVisibleTeam - 2;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Is this player an enemy to the local player
//-----------------------------------------------------------------------------
bool C_TFPlayer::IsEnemyPlayer( void )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pLocalPlayer )
		return false;

	switch( pLocalPlayer->GetTeamNumber() )
	{
	case TF_TEAM_RED:
		return ( GetTeamNumber() == TF_TEAM_BLUE );
	
	case TF_TEAM_BLUE:
		return ( GetTeamNumber() == TF_TEAM_RED );

	default:
		break;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Displays a nemesis icon on this player to the local player
//-----------------------------------------------------------------------------
void C_TFPlayer::ShowNemesisIcon( bool bShow )
{
	if ( bShow )
	{
		const char *pszEffect = NULL;
		switch ( GetTeamNumber() )
		{
		case TF_TEAM_RED:
			pszEffect = "particle_nemesis_red";
			break;
		case TF_TEAM_BLUE:
			pszEffect = "particle_nemesis_blue";
			break;
		default:
			return;	// shouldn't get called if we're not on a team; bail out if it does
		}
		ParticleProp()->Create( pszEffect, PATTACH_POINT_FOLLOW, "head" );
	}
	else
	{
		// stop effects for both team colors (to make sure we remove effects in event of team change)
		ParticleProp()->StopParticlesNamed( "particle_nemesis_red", true );
		ParticleProp()->StopParticlesNamed( "particle_nemesis_blue", true );
	}
	m_bIsDisplayingNemesisIcon = bShow;
}

#define	TF_TAUNT_PITCH	0
#define TF_TAUNT_YAW	1
#define TF_TAUNT_DIST	2

#define TF_TAUNT_MAXYAW		135
#define TF_TAUNT_MINYAW		-135
#define TF_TAUNT_MAXPITCH	90
#define TF_TAUNT_MINPITCH	0
#define TF_TAUNT_IDEALLAG	4.0f

static Vector TF_TAUNTCAM_HULL_MIN( -9.0f, -9.0f, -9.0f );
static Vector TF_TAUNTCAM_HULL_MAX( 9.0f, 9.0f, 9.0f );

static ConVar tf_tauntcam_yaw( "tf_tauntcam_yaw", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConVar tf_tauntcam_pitch( "tf_tauntcam_pitch", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConVar tf_tauntcam_dist( "tf_tauntcam_dist", "150", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConVar tf_tauntcam_speed( "tf_tauntcam_speed", "300", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

ConVar tf_halloween_kart_cam_dist( "tf_halloween_kart_cam_dist", "225", FCVAR_CHEAT );
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::TurnOnTauntCam( void )
{
	if ( !IsLocalPlayer() )
		return;

	m_flTauntCamTargetDist = ( m_flTauntCamTargetDist != 0.0f ) ? m_flTauntCamTargetDist : tf_tauntcam_dist.GetFloat();
	m_flTauntCamTargetDistUp = ( m_flTauntCamTargetDistUp != 0.0f ) ? m_flTauntCamTargetDistUp : 0.f;

	m_flTauntCamCurrentDist = 0.f;
	m_flTauntCamCurrentDistUp = 0.f;

	// Save the old view angles.
	engine->GetViewAngles( m_angTauntEngViewAngles );
	prediction->GetViewAngles( m_angTauntPredViewAngles );

	m_TauntCameraData.m_flPitch = 0;
	m_TauntCameraData.m_flYaw =  0;
	m_TauntCameraData.m_flDist = m_flTauntCamTargetDist;
	m_TauntCameraData.m_flLag = 1.f;
	m_TauntCameraData.m_vecHullMin.Init( -9.0f, -9.0f, -9.0f );
	m_TauntCameraData.m_vecHullMax.Init( 9.0f, 9.0f, 9.0f );

	if ( tf_taunt_first_person.GetBool() )
	{
		// Remain in first-person.
	}
	else
	{
		g_ThirdPersonManager.SetDesiredCameraOffset( Vector( 0, 0, 0 ) );
		g_ThirdPersonManager.SetOverridingThirdPerson( true );
	
		::input->CAM_ToThirdPerson();
		ThirdPersonSwitch( true );
		//UpdateKillStreakEffects( m_Shared.GetStreak( CTFPlayerShared::kTFStreak_Kills ) );
	}

	m_bTauntInterpolating = true;

	if ( m_hItem )
	{
		m_hItem->UpdateVisibility();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::TurnOnTauntCam_Finish( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::TurnOffTauntCam( void )
{
	// We want to interpolate back into the guy's head.
	if ( g_ThirdPersonManager.GetForcedThirdPerson() == false )
	{
		m_flTauntCamTargetDist = 0.f;
		m_TauntCameraData.m_flDist = m_flTauntCamTargetDist;
	}

	g_ThirdPersonManager.SetOverridingThirdPerson( false );

	if ( g_ThirdPersonManager.GetForcedThirdPerson() )
	{
		TurnOffTauntCam_Finish();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::TurnOffTauntCam_Finish( void )
{
	if ( !IsLocalPlayer() )
		return;

	const Vector& vecOffset = g_ThirdPersonManager.GetCameraOffsetAngles();
	tf_tauntcam_pitch.SetValue( vecOffset[PITCH] - m_angTauntPredViewAngles[PITCH] );
	tf_tauntcam_yaw.SetValue( vecOffset[YAW] - m_angTauntPredViewAngles[YAW] );
	
	QAngle angles;

	angles[PITCH] = vecOffset[PITCH];
	angles[YAW] = vecOffset[YAW];
	angles[DIST] = vecOffset[DIST];

	if( g_ThirdPersonManager.WantToUseGameThirdPerson() == false )
	{
		::input->CAM_ToFirstPerson();
		ThirdPersonSwitch( false );
		//UpdateKillStreakEffects( m_Shared.GetStreak( CTFPlayerShared::kTFStreak_Kills ) );
		angles = vec3_angle;
	}

	::input->CAM_SetCameraThirdData( NULL, angles );

	// Reset the old view angles.
//	engine->SetViewAngles( m_angTauntEngViewAngles );
//	prediction->SetViewAngles( m_angTauntPredViewAngles );

	// Force the feet to line up with the view direction post taunt.
	// If you are forcing aim yaw, your code is almost definitely broken if you don't include a delay between 
	// teleporting and forcing yaw. This is due to an unfortunate interaction between the command lookback window,
	// and the fact that m_flEyeYaw is never propogated from the server to the client.
	// TODO: Fix this after Halloween 2014.
	if ( GetGameAnimStateType() )
		GetGameAnimStateType()->m_bForceAimYaw = true;

	m_bTauntInterpolating = false;

	if ( GetViewModel() )
	{
		GetViewModel()->UpdateVisibility();
	}

	if ( m_hItem )
	{
		m_hItem->UpdateVisibility();
	}

	SetAppropriateCamera( this );
}

void C_TFPlayer::SetTauntCameraTargets( float back, float up )
{
	m_flTauntCamTargetDist = back;
	m_flTauntCamTargetDistUp = up;
	// Force this on
	m_bTauntInterpolating = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::HandleTaunting( void )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	// Clear the taunt slot.
	if (	!m_bWasTaunting &&
			(	
				m_Shared.InCond( TF_COND_TAUNTING ) ||
				m_Shared.IsControlStunned() ||
				m_Shared.IsLoser() ||
				m_nForceTauntCam ||
				m_Shared.InCond( TF_COND_HALLOWEEN_BOMB_HEAD ) ||
				m_Shared.InCond( TF_COND_HALLOWEEN_GIANT ) ||
				m_Shared.InCond( TF_COND_HALLOWEEN_TINY ) ||
				m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) ||
				m_Shared.InCond( TF_COND_HALLOWEEN_KART ) ||
				m_Shared.InCond( TF_COND_MELEE_ONLY ) ||
				m_Shared.InCond( TF_COND_SWIMMING_CURSE )
			)
		) 
	{
		m_bWasTaunting = true;

		// Handle the camera for the local player.
		if ( pLocalPlayer )
		{
			TurnOnTauntCam();
		}
	}

	if (	( !IsAlive() && m_nForceTauntCam < 2 ) || 
			(
				m_bWasTaunting && !m_Shared.InCond( TF_COND_TAUNTING ) && !m_Shared.IsControlStunned() && 
				!m_Shared.InCond( TF_COND_PHASE ) && !m_Shared.IsLoser() &&
				!m_nForceTauntCam && !m_Shared.InCond( TF_COND_HALLOWEEN_BOMB_HEAD ) &&
				!m_Shared.InCond( TF_COND_HALLOWEEN_THRILLER ) &&
				!m_Shared.InCond( TF_COND_HALLOWEEN_GIANT ) &&
				!m_Shared.InCond( TF_COND_HALLOWEEN_TINY ) &&
				!m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) &&
				!m_Shared.InCond( TF_COND_HALLOWEEN_KART ) &&
				!m_Shared.InCond( TF_COND_MELEE_ONLY ) &&
				!m_Shared.InCond( TF_COND_SWIMMING_CURSE )
			)
		)
	{
		m_bWasTaunting = false;

		// Clear the vcd slot.
		if ( GetGameAnimStateType() )
			GetGameAnimStateType()->ResetGestureSlot( GESTURE_SLOT_VCD );

		// Handle the camera for the local player.
		if ( pLocalPlayer )
		{
			TurnOffTauntCam();
		}
	}

	TauntCamInterpolation();
}

//-----------------------------------------------------------------------------
// Purpose: Handles third person camera interpolation directly 
// so we can manage enter & exit behavior without hacking the camera.
//-----------------------------------------------------------------------------
void C_TFPlayer::TauntCamInterpolation()
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pLocalPlayer && m_bTauntInterpolating )
	{
		if ( m_flTauntCamCurrentDist != m_flTauntCamTargetDist )
		{
			m_flTauntCamCurrentDist += Sign( m_flTauntCamTargetDist - m_flTauntCamCurrentDist ) * gpGlobals->frametime * tf_tauntcam_speed.GetFloat();
			m_flTauntCamCurrentDist = clamp( m_flTauntCamCurrentDist, m_flTauntCamCurrentDist, m_flTauntCamTargetDist );
		}
		
		if ( m_flTauntCamCurrentDistUp != m_flTauntCamTargetDistUp )
		{
			m_flTauntCamCurrentDistUp += Sign( m_flTauntCamTargetDistUp - m_flTauntCamCurrentDistUp ) * gpGlobals->frametime * tf_tauntcam_speed.GetFloat();
			m_flTauntCamCurrentDistUp = clamp( m_flTauntCamCurrentDistUp, m_flTauntCamCurrentDistUp, m_flTauntCamTargetDistUp );
		}

		const Vector& vecCamOffset = g_ThirdPersonManager.GetCameraOffsetAngles();

		Vector vecOrigin = pLocalPlayer->GetLocalOrigin();
		vecOrigin += pLocalPlayer->GetViewOffset();

		Vector vecForward, vecUp;
		AngleVectors( QAngle( vecCamOffset[PITCH], vecCamOffset[YAW], 0 ), &vecForward, NULL, &vecUp );

		trace_t trace;
		UTIL_TraceHull( vecOrigin, vecOrigin - ( vecForward * m_flTauntCamCurrentDist ) + ( vecUp * m_flTauntCamCurrentDistUp ), Vector( -9.f, -9.f, -9.f ), 
			Vector( 9.f, 9.f, 9.f ), MASK_SOLID_BRUSHONLY, pLocalPlayer, COLLISION_GROUP_DEBRIS, &trace );

		if ( trace.fraction < 1.0 )
			m_flTauntCamCurrentDist *= trace.fraction;

		QAngle angCameraOffset = QAngle( vecCamOffset[PITCH], vecCamOffset[YAW], m_flTauntCamCurrentDist );
		::input->CAM_SetCameraThirdData( &m_TauntCameraData, angCameraOffset ); // Override camera distance interpolation.

		g_ThirdPersonManager.SetDesiredCameraOffset( Vector( m_flTauntCamCurrentDist, 0, m_flTauntCamCurrentDistUp ) );

		if ( m_flTauntCamCurrentDist == m_flTauntCamTargetDist && m_flTauntCamCurrentDistUp == m_flTauntCamTargetDistUp )
		{
			if ( m_flTauntCamTargetDist == 0.f )
				TurnOffTauntCam_Finish();
			else
				TurnOnTauntCam_Finish();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check if passed in player is on the local player's friends list
//-----------------------------------------------------------------------------
bool C_TFPlayer::IsPlayerOnSteamFriendsList( C_BasePlayer *pPlayer )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return false;

	if ( !pPlayer )
		return false;

	if ( !steamapicontext->SteamFriends() || !steamapicontext->SteamUtils() )
		return false;

	player_info_t pi;
	if ( engine->GetPlayerInfo( pPlayer->entindex(), &pi ) && pi.friendsID )
	{
		CSteamID steamID( pi.friendsID, 1, GetUniverse(), k_EAccountTypeIndividual );
		if ( steamapicontext->SteamFriends()->HasFriend( steamID, k_EFriendFlagImmediate ) )
			return true;
	}

	return false;
}

void C_TFPlayer::ClientThink()
{
	if ( !GetGameMovementType() || !DoesMovementTypeMatch() )
		GameMovementInit();

	if ( !GetGameAnimStateType() || !DoesAnimStateTypeMatch() )
		GameAnimStateInit();

	// Pass on through to the base class.
	BaseClass::ClientThink();

	UpdateIDTarget();

	UpdateLookAt();

	if ( !IsTaunting() && GetGameAnimStateType() )
	{
		if ( GetGameAnimStateType()->IsGestureSlotActive( GESTURE_SLOT_VCD ) )
			GetGameAnimStateType()->ResetGestureSlot( GESTURE_SLOT_VCD );
	}

	// Clear our healer, it'll be reset by the medigun client think if we're being healed
	m_hHealer = NULL;

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	// Ugh, this check is getting ugly

	// Start smoke if we're not invisible or disguised
	if ( IsPlayerClass( TF_CLASS_SPY ) && IsAlive() &&									// only on spy model
		( !m_Shared.InCond( TF_COND_DISGUISED ) || !IsEnemyPlayer() ) &&	// disguise doesn't show for teammates
		GetPercentInvisible() <= 0 &&										// don't start if invis
		( pLocalPlayer != this ) && 										// don't show to local player
		!( pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pLocalPlayer->GetObserverTarget() == this ) )	// not if we're spectating this player first person
	{
		if ( !m_bCigaretteSmokeActive )
		{
			int iSmokeAttachment = LookupAttachment( "cig_smoke" );
			ParticleProp()->Create( "cig_smoke", PATTACH_POINT_FOLLOW, iSmokeAttachment );
			m_bCigaretteSmokeActive = true;
		}
	}
	else	// stop the smoke otherwise if its active
	{
		if ( m_bCigaretteSmokeActive )
		{
			ParticleProp()->StopParticlesNamed( "cig_smoke", false );
			m_bCigaretteSmokeActive = false;
		}
	}

	if ( m_bWaterExitEffectActive && !IsAlive() )
	{
		ParticleProp()->StopParticlesNamed( "water_playeremerge", false );
		m_bWaterExitEffectActive = false;
	}

	if ( m_bUpdatePartyHat )
	{
		UpdatePartyHat();
		m_bUpdatePartyHat = false;
	}

	if ( m_pSaveMeEffect )
	{
		// Kill the effect if either
		// a) the player is dead
		// b) the enemy disguised spy is now invisible

		if ( !IsAlive() ||
			( m_Shared.InCond( TF_COND_DISGUISED ) && IsEnemyPlayer() && ( GetPercentInvisible() > 0 ) ) )
		{
			ParticleProp()->StopEmissionAndDestroyImmediately( m_pSaveMeEffect );
			m_pSaveMeEffect = NULL;
		}
	}

	if ( m_Shared.IsEnteringOrExitingFullyInvisible() )
	{
		UpdateSpyStateChange();
	}

	if ( HasTheFlag() && GetGlowObject() )
	{
		C_TFItem *pFlag = GetItem();
		if ( pFlag->ShouldHideGlowEffect() )
		{
			GetGlowObject()->SetEntity( NULL );
		}
		else
		{
			GetGlowObject()->SetEntity( this );
		}
	}

	// DOD
	if ( IsLocalPlayer() )
	{
		StaminaSoundThink();

		// Recoil
		QAngle viewangles;
		engine->GetViewAngles( viewangles );

		float flYawRecoil;
		float flPitchRecoil;
		GetRecoilToAddThisFrame( flPitchRecoil, flYawRecoil );

		// Recoil
		if( flPitchRecoil > 0 )
		{
			//add the recoil pitch
			viewangles[PITCH] -= flPitchRecoil;
			viewangles[YAW] += flYawRecoil;
		}

		// Sniper sway
		if( m_Shared.IsSniperZoomed() && GetFOV() <= 20 )
		{
			//multiply by frametime to balance for framerate changes
			float x = gpGlobals->frametime * cos( gpGlobals->curtime );
			float y = gpGlobals->frametime * 2 * cos( 2 * gpGlobals->curtime );

			float scale;

			if( m_Shared.IsDucking() )				//duck
				scale = ZOOM_SWAY_DUCKING;
			else if( m_Shared.IsProne() )
				scale = ZOOM_SWAY_PRONE;
			else									//standing
				scale = ZOOM_SWAY_STANDING; 

			if( GetAbsVelocity().Length() > 10 )
				scale += ZOOM_SWAY_MOVING_PENALTY;

			viewangles[PITCH] += y * scale;
			viewangles[YAW] += x * scale;
		}

		engine->SetViewAngles( viewangles );
	}
	else
	{
		// Cold breath.
		if ( IsDODClass() )
			UpdateColdBreath();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::UpdateLookAt( void )
{
	bool bFoundViewTarget = false;

	Vector vForward;
	AngleVectors( GetLocalAngles(), &vForward );

	Vector vMyOrigin =  GetAbsOrigin();

	Vector vecLookAtTarget = vec3_origin;

	if ( tf_clientsideeye_lookats.GetBool() )
	{
		for( int iClient = 1; iClient <= gpGlobals->maxClients; ++iClient )
		{
			CBaseEntity *pEnt = UTIL_PlayerByIndex( iClient );
			if ( !pEnt || !pEnt->IsPlayer() )
				continue;

			if ( !pEnt->IsAlive() )
				continue;

			if ( pEnt == this )
				continue;

			Vector vDir = pEnt->GetAbsOrigin() - vMyOrigin;

			if ( vDir.Length() > 300 ) 
				continue;

			VectorNormalize( vDir );

			if ( DotProduct( vForward, vDir ) < 0.0f )
				continue;

			vecLookAtTarget = pEnt->EyePosition();
			bFoundViewTarget = true;
			break;
		}
	}

	if ( bFoundViewTarget == false )
	{
		// no target, look forward
		vecLookAtTarget = GetAbsOrigin() + vForward * 512;
	}

	// orient eyes
	m_viewtarget = vecLookAtTarget;

	/*
	// blinking
	if (m_blinkTimer.IsElapsed())
	{
		m_blinktoggle = !m_blinktoggle;
		m_blinkTimer.Start( RandomFloat( 1.5f, 4.0f ) );
	}
	*/

	/*
	// Figure out where we want to look in world space.
	QAngle desiredAngles;
	Vector to = vecLookAtTarget - EyePosition();
	VectorAngles( to, desiredAngles );

	// Figure out where our body is facing in world space.
	QAngle bodyAngles( 0, 0, 0 );
	bodyAngles[YAW] = GetLocalAngles()[YAW];

	float flBodyYawDiff = bodyAngles[YAW] - m_flLastBodyYaw;
	m_flLastBodyYaw = bodyAngles[YAW];

	// Set the head's yaw.
	float desired = AngleNormalize( desiredAngles[YAW] - bodyAngles[YAW] );
	desired = clamp( -desired, m_headYawMin, m_headYawMax );
	m_flCurrentHeadYaw = ApproachAngle( desired, m_flCurrentHeadYaw, 130 * gpGlobals->frametime );

	// Counterrotate the head from the body rotation so it doesn't rotate past its target.
	m_flCurrentHeadYaw = AngleNormalize( m_flCurrentHeadYaw - flBodyYawDiff );

	SetPoseParameter( m_headYawPoseParam, m_flCurrentHeadYaw );

	// Set the head's yaw.
	desired = AngleNormalize( desiredAngles[PITCH] );
	desired = clamp( desired, m_headPitchMin, m_headPitchMax );

	m_flCurrentHeadPitch = ApproachAngle( -desired, m_flCurrentHeadPitch, 130 * gpGlobals->frametime );
	m_flCurrentHeadPitch = AngleNormalize( m_flCurrentHeadPitch );
	SetPoseParameter( m_headPitchPoseParam, m_flCurrentHeadPitch );
	*/
}


//-----------------------------------------------------------------------------
// Purpose: Try to steer away from any players and objects we might interpenetrate
//-----------------------------------------------------------------------------
#define TF_AVOID_MAX_RADIUS_SQR		5184.0f			// Based on player extents and max buildable extents.
#define TF_OO_AVOID_MAX_RADIUS_SQR	0.00019f

ConVar tf_max_separation_force ( "tf_max_separation_force", "256", FCVAR_DEVELOPMENTONLY );

extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar cl_sidespeed;

void C_TFPlayer::AvoidPlayers( CUserCmd *pCmd )
{
	// Turn off the avoid player code.
	if ( !tf_avoidteammates.GetBool() )
		return;

	// Don't test if the player doesn't exist or is dead.
	if ( IsAlive() == false )
		return;

	C_Team *pTeam = ( C_Team * )GetTeam();
	if ( !pTeam )
		return;

	// Up vector.
	static Vector vecUp( 0.0f, 0.0f, 1.0f );

	Vector vecTFPlayerCenter = GetAbsOrigin();
	Vector vecTFPlayerMin = GetPlayerMins();
	Vector vecTFPlayerMax = GetPlayerMaxs();
	float flZHeight = vecTFPlayerMax.z - vecTFPlayerMin.z;
	vecTFPlayerCenter.z += 0.5f * flZHeight;
	VectorAdd( vecTFPlayerMin, vecTFPlayerCenter, vecTFPlayerMin );
	VectorAdd( vecTFPlayerMax, vecTFPlayerCenter, vecTFPlayerMax );

	// Find an intersecting player or object.
	int nAvoidPlayerCount = 0;
	C_TFPlayer *pAvoidPlayerList[MAX_PLAYERS];

	C_TFPlayer *pIntersectPlayer = NULL;
	CBaseObject *pIntersectObject = NULL;
	float flAvoidRadius = 0.0f;

	Vector vecAvoidCenter, vecAvoidMin, vecAvoidMax;
	for ( int i = 0; i < pTeam->GetNumPlayers(); ++i )
	{
		C_TFPlayer *pAvoidPlayer = static_cast< C_TFPlayer * >( pTeam->GetPlayer( i ) );
		if ( pAvoidPlayer == NULL )
			continue;
		// Is the avoid player me?
		if ( pAvoidPlayer == this )
			continue;

		// Save as list to check against for objects.
		pAvoidPlayerList[nAvoidPlayerCount] = pAvoidPlayer;
		++nAvoidPlayerCount;

		// Check to see if the avoid player is dormant.
		if ( pAvoidPlayer->IsDormant() )
			continue;

		// Is the avoid player solid?
		if ( pAvoidPlayer->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
			continue;

		Vector t1, t2;

		vecAvoidCenter = pAvoidPlayer->GetAbsOrigin();
		vecAvoidMin = pAvoidPlayer->GetPlayerMins();
		vecAvoidMax = pAvoidPlayer->GetPlayerMaxs();
		flZHeight = vecAvoidMax.z - vecAvoidMin.z;
		vecAvoidCenter.z += 0.5f * flZHeight;
		VectorAdd( vecAvoidMin, vecAvoidCenter, vecAvoidMin );
		VectorAdd( vecAvoidMax, vecAvoidCenter, vecAvoidMax );

		if ( IsBoxIntersectingBox( vecTFPlayerMin, vecTFPlayerMax, vecAvoidMin, vecAvoidMax ) )
		{
			// Need to avoid this player.
			if ( !pIntersectPlayer )
			{
				pIntersectPlayer = pAvoidPlayer;
				break;
			}
		}
	}

	// We didn't find a player - look for objects to avoid.
	if ( !pIntersectPlayer )
	{
		for ( int iPlayer = 0; iPlayer < nAvoidPlayerCount; ++iPlayer )
		{	
			// Stop when we found an intersecting object.
			if ( pIntersectObject )
				break;

			C_TFTeam *pTeam = (C_TFTeam*)GetTeam();

			for ( int iObject = 0; iObject < pTeam->GetNumObjects(); ++iObject )
			{
				CBaseObject *pAvoidObject = pTeam->GetObject( iObject );
				if ( !pAvoidObject )
					continue;

				// Check to see if the object is dormant.
				if ( pAvoidObject->IsDormant() )
					continue;

				// Is the object solid.
				if ( pAvoidObject->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
					continue;

				// If we shouldn't avoid it, see if we intersect it.
				if ( pAvoidObject->ShouldPlayersAvoid() )
				{
					vecAvoidCenter = pAvoidObject->WorldSpaceCenter();
					vecAvoidMin = pAvoidObject->WorldAlignMins();
					vecAvoidMax = pAvoidObject->WorldAlignMaxs();
					VectorAdd( vecAvoidMin, vecAvoidCenter, vecAvoidMin );
					VectorAdd( vecAvoidMax, vecAvoidCenter, vecAvoidMax );

					if ( IsBoxIntersectingBox( vecTFPlayerMin, vecTFPlayerMax, vecAvoidMin, vecAvoidMax ) )
					{
						// Need to avoid this object.
						pIntersectObject = pAvoidObject;
						break;
					}
				}
			}
		}
	}

	// Anything to avoid?
	if ( !pIntersectPlayer && !pIntersectObject )
	{
		m_Shared.SetSeparation( false );
		m_Shared.SetSeparationVelocity( vec3_origin );
		return;
	}

	// Calculate the push strength and direction.
	Vector vecDelta;

	// Avoid a player - they have precedence.
	if ( pIntersectPlayer )
	{
		VectorSubtract( pIntersectPlayer->WorldSpaceCenter(), vecTFPlayerCenter, vecDelta );

		Vector vRad = pIntersectPlayer->WorldAlignMaxs() - pIntersectPlayer->WorldAlignMins();
		vRad.z = 0;

		flAvoidRadius = vRad.Length();
	}
	// Avoid a object.
	else
	{
		VectorSubtract( pIntersectObject->WorldSpaceCenter(), vecTFPlayerCenter, vecDelta );

		Vector vRad = pIntersectObject->WorldAlignMaxs() - pIntersectObject->WorldAlignMins();
		vRad.z = 0;

		flAvoidRadius = vRad.Length();
	}

	float flPushStrength = RemapValClamped( vecDelta.Length(), flAvoidRadius, 0, 0, tf_max_separation_force.GetInt() ); //flPushScale;

	//Msg( "PushScale = %f\n", flPushStrength );

	// Check to see if we have enough push strength to make a difference.
	if ( flPushStrength < 0.01f )
		return;

	Vector vecPush;
	if ( GetAbsVelocity().Length2DSqr() > 0.1f )
	{
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity.z = 0.0f;
		CrossProduct( vecUp, vecVelocity, vecPush );
		VectorNormalize( vecPush );
	}
	else
	{
		// We are not moving, but we're still intersecting.
		QAngle angView = pCmd->viewangles;
		angView.x = 0.0f;
		AngleVectors( angView, NULL, &vecPush, NULL );
	}

	// Move away from the other player/object.
	Vector vecSeparationVelocity;
	if ( vecDelta.Dot( vecPush ) < 0 )
	{
		vecSeparationVelocity = vecPush * flPushStrength;
	}
	else
	{
		vecSeparationVelocity = vecPush * -flPushStrength;
	}

	// Don't allow the max push speed to be greater than the max player speed.
	float flMaxPlayerSpeed = MaxSpeed();
	float flCropFraction = 1.33333333f;

	if ( ( GetFlags() & FL_DUCKING ) && ( GetGroundEntity() != NULL ) )
	{	
		flMaxPlayerSpeed *= flCropFraction;
	}	

	float flMaxPlayerSpeedSqr = flMaxPlayerSpeed * flMaxPlayerSpeed;

	if ( vecSeparationVelocity.LengthSqr() > flMaxPlayerSpeedSqr )
	{
		vecSeparationVelocity.NormalizeInPlace();
		VectorScale( vecSeparationVelocity, flMaxPlayerSpeed, vecSeparationVelocity );
	}

	QAngle vAngles = pCmd->viewangles;
	vAngles.x = 0;
	Vector currentdir;
	Vector rightdir;

	AngleVectors( vAngles, &currentdir, &rightdir, NULL );

	Vector vDirection = vecSeparationVelocity;

	VectorNormalize( vDirection );

	float fwd = currentdir.Dot( vDirection );
	float rt = rightdir.Dot( vDirection );

	float forward = fwd * flPushStrength;
	float side = rt * flPushStrength;

	//Msg( "fwd: %f - rt: %f - forward: %f - side: %f\n", fwd, rt, forward, side );

	m_Shared.SetSeparation( true );
	m_Shared.SetSeparationVelocity( vecSeparationVelocity );

	pCmd->forwardmove	+= forward;
	pCmd->sidemove		+= side;

	// Clamp the move to within legal limits, preserving direction. This is a little
	// complicated because we have different limits for forward, back, and side

	//Msg( "PRECLAMP: forwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );

	float flForwardScale = 1.0f;
	if ( pCmd->forwardmove > fabs( cl_forwardspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_forwardspeed.GetFloat() ) / pCmd->forwardmove;
	}
	else if ( pCmd->forwardmove < -fabs( cl_backspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_backspeed.GetFloat() ) / fabs( pCmd->forwardmove );
	}

	float flSideScale = 1.0f;
	if ( fabs( pCmd->sidemove ) > fabs( cl_sidespeed.GetFloat() ) )
	{
		flSideScale = fabs( cl_sidespeed.GetFloat() ) / fabs( pCmd->sidemove );
	}

	float flScale = min( flForwardScale, flSideScale );
	pCmd->forwardmove *= flScale;
	pCmd->sidemove *= flScale;

	//Msg( "Pforwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flInputSampleTime - 
//			*pCmd - 
//-----------------------------------------------------------------------------
bool C_TFPlayer::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{	
	// Lock view if deployed
	if( m_Shared.IsInMGDeploy() )
	{
		m_Shared.ClampDeployedAngles( &pCmd->viewangles );
	}

	//if we're prone and moving, do some sway
	if( m_Shared.IsProne() && IsAlive() )
	{
		float flSpeed = GetAbsVelocity().Length();

		float flSwayAmount = PRONE_SWAY_AMOUNT * gpGlobals->frametime;

		if( flSpeed > 10 )
		{
			if (m_flProneViewOffset >= PRONE_MAX_SWAY)
			{
				m_bProneSwayingRight = false;
			}
			else if (m_flProneViewOffset <= -PRONE_MAX_SWAY)
			{
				m_bProneSwayingRight = true;
			}

			if (m_bProneSwayingRight)
			{
				pCmd->viewangles[YAW]	+= flSwayAmount;
				m_flProneViewOffset		+= flSwayAmount;
			}
			else
			{
				pCmd->viewangles[YAW]	-= flSwayAmount;
				m_flProneViewOffset		-= flSwayAmount;
			}
		}
		else
		{
			// Return to 0 prone sway offset gradually

			//Quick Checks to make sure it isn't bigger or smaller than our sway amount
			if ( (m_flProneViewOffset < 0.0 && m_flProneViewOffset > -flSwayAmount) ||
				 (m_flProneViewOffset > 0.0 && m_flProneViewOffset < flSwayAmount) )
			{
				m_flProneViewOffset = 0.0;
			}

			if (m_flProneViewOffset > 0.0)
			{
				pCmd->viewangles[YAW]	-= flSwayAmount;
				m_flProneViewOffset		-= flSwayAmount;
			}
			else if (m_flProneViewOffset < 0.0)
			{
				pCmd->viewangles[YAW]	+= flSwayAmount;
				m_flProneViewOffset		+= flSwayAmount;
			}
		}
	}

	static QAngle angMoveAngle( 0.0f, 0.0f, 0.0f );
	
	bool bNoTaunt = true;
	bool bInTaunt = m_Shared.InCond( TF_COND_TAUNTING ) || m_Shared.InCond( TF_COND_HALLOWEEN_THRILLER );

	if ( m_Shared.InCond( TF_COND_FREEZE_INPUT ) )
	{
		pCmd->viewangles = angMoveAngle; // use the last save angles
		pCmd->forwardmove = 0.0f;
		pCmd->sidemove = 0.0f;
		pCmd->upmove = 0.0f;
		pCmd->buttons = 0;
		pCmd->weaponselect = 0;
		pCmd->weaponsubtype = 0;
		pCmd->mousedx = 0;
		pCmd->mousedy = 0;
	}

	if ( bInTaunt )
	{
		if ( tf_allow_taunt_switch.GetInt() <= 1 )
		{
			pCmd->weaponselect = 0;
		}

		int nCurrentButtons = pCmd->buttons;
		pCmd->buttons = 0;
		// Allow demo to detonate his stickies while taunting
		C_TFWeaponBase* pLauncher = static_cast<C_TFWeaponBase*>(Weapon_OwnsThisID(TF_WEAPON_PIPEBOMBLAUNCHER));
		if (pLauncher)
			pCmd->buttons = nCurrentButtons & IN_ATTACK2;

		pCmd->forwardmove = 0.0f;
		pCmd->sidemove = 0.0f;
		pCmd->upmove = 0.0f;

		VectorCopy( angMoveAngle, pCmd->viewangles );

		bNoTaunt = false;
	}
	else
	{
		VectorCopy( pCmd->viewangles, angMoveAngle );
	}

	BaseClass::CreateMove( flInputSampleTime, pCmd );

	if ( !bInTaunt )
		AvoidPlayers( pCmd );

	return bNoTaunt;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	if ( IsLocalPlayer() )
	{
		if ( !prediction->IsFirstTimePredicted() )
			return;
	}

	MDLCACHE_CRITICAL_SECTION();
	if ( GetGameAnimStateType() )
		GetGameAnimStateType()->DoAnimationEvent( event, nData );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Vector C_TFPlayer::GetObserverCamOrigin( void )
{
	if ( !IsAlive() )
	{
		if ( m_hFirstGib )
		{
			IPhysicsObject *pPhysicsObject = m_hFirstGib->VPhysicsGetObject();
			if( pPhysicsObject )
			{
				Vector vecMassCenter = pPhysicsObject->GetMassCenterLocalSpace();
				Vector vecWorld;
				m_hFirstGib->CollisionProp()->CollisionToWorldSpace( vecMassCenter, &vecWorld );
				return (vecWorld);
			}
			return m_hFirstGib->GetRenderOrigin();
		}

		return GetDeathViewPosition();
	}

	return BaseClass::GetObserverCamOrigin();	
}

//-----------------------------------------------------------------------------
// Purpose: Consider the viewer and other factors when determining resulting
// invisibility
//-----------------------------------------------------------------------------
float C_TFPlayer::GetEffectiveInvisibilityLevel( void )
{
	float flPercentInvisible = GetPercentInvisible();

	bool bLimitedInvis = !IsEnemyPlayer();

	// If this is a teammate of the local player or viewer is observer,
	// dont go above a certain max invis
	if ( bLimitedInvis )
	{
		float flMax = tf_teammate_max_invis.GetFloat();
		if ( flPercentInvisible > flMax )
		{
			flPercentInvisible = flMax;
		}
	}
	else
	{
		// If this player just killed me, show them slightly
		// less than full invis in the deathcam and freezecam

		C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

		if ( pLocalPlayer )
		{
			int iObserverMode = pLocalPlayer->GetObserverMode();

			if ( ( iObserverMode == OBS_MODE_FREEZECAM || iObserverMode == OBS_MODE_DEATHCAM ) && 
				pLocalPlayer->GetObserverTarget() == this )
			{
				float flMax = tf_teammate_max_invis.GetFloat();
				if ( flPercentInvisible > flMax )
				{
					flPercentInvisible = flMax;
				}
			}
		}
	}

	return flPercentInvisible;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_TFPlayer::DrawModel( int flags )
{
	// If we're a dead player with a fresh ragdoll, don't draw
	if ( m_nRenderFX == kRenderFxRagdoll )
		return 0;

	// Don't draw the model at all if we're fully invisible
	if ( GetEffectiveInvisibilityLevel() >= 1.0f )
	{
		if ( m_hPartyHat && ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 90 ) && !m_hPartyHat->IsEffectActive( EF_NODRAW ) )
		{
			m_hPartyHat->SetEffects( EF_NODRAW );
		}
		return 0;
	}
	else
	{
		if ( m_hPartyHat && ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 90 ) && m_hPartyHat->IsEffectActive( EF_NODRAW ) )
		{
			m_hPartyHat->RemoveEffects( EF_NODRAW );
		}
	}

	CMatRenderContextPtr pRenderContext( materials );
	bool bDoEffect = false;

	float flAmountToChop = 0.0;
	if ( m_Shared.InCond( TF_COND_DISGUISING ) )
	{
		flAmountToChop = ( gpGlobals->curtime - m_flDisguiseEffectStartTime ) *
			( 1.0 / TF_TIME_TO_DISGUISE );
	}
	else
	{
		if ( m_Shared.InCond( TF_COND_DISGUISED ) )
		{
			float flETime = gpGlobals->curtime - m_flDisguiseEffectStartTime;
			if ( ( flETime > 0.0 ) && ( flETime < TF_TIME_TO_SHOW_DISGUISED_FINISHED_EFFECT ) )
			{
				flAmountToChop = 1.0 - ( flETime * ( 1.0/TF_TIME_TO_SHOW_DISGUISED_FINISHED_EFFECT ) );
			}
		}
	}

	bDoEffect = ( flAmountToChop > 0.0 ) && ( ! IsLocalPlayer() );
#if ( SHOW_DISGUISE_EFFECT == 0  )
	bDoEffect = false;
#endif
	bDoEffect = false;
	if ( bDoEffect )
	{
		Vector vMyOrigin =  GetAbsOrigin();
		BoxDeformation_t mybox;
		mybox.m_ClampMins = vMyOrigin - Vector(100,100,100);
		mybox.m_ClampMaxes = vMyOrigin + Vector(500,500,72 * ( 1 - flAmountToChop ) );
		pRenderContext->PushDeformation( &mybox );
	}

	int ret = BaseClass::DrawModel( flags );

	if ( bDoEffect )
		pRenderContext->PopDeformation();
	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::ProcessMuzzleFlashEvent()
{
	if ( IsDODClass() )
		return ProcessMuzzleFlashEventDOD();

	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	// Reenable when the weapons have muzzle flash attachments in the right spot.
	bool bInToolRecordingMode = ToolsEnabled() && clienttools->IsInRecordingMode();
	if ( this == pLocalPlayer && !bInToolRecordingMode )
		return; // don't show own world muzzle flash for localplayer

	if ( pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		// also don't show in 1st person spec mode
		if ( pLocalPlayer->GetObserverTarget() == this )
			return;
	}

	CPCWeaponBase *pWeapon = m_Shared.GetActivePCWeapon();
	if ( !pWeapon )
		return;

	pWeapon->ProcessMuzzleFlashEvent();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_TFPlayer::GetIDTarget() const
{
	return m_iIDEntIndex;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::SetForcedIDTarget( int iTarget )
{
	m_iForcedIDTarget = iTarget;
}

//-----------------------------------------------------------------------------
// Purpose: Update this client's targetid entity
//-----------------------------------------------------------------------------
void C_TFPlayer::UpdateIDTarget()
{
	if ( !IsLocalPlayer() )
		return;

	// don't show IDs if mp_fadetoblack is on
	if ( GetTeamNumber() > TEAM_SPECTATOR && mp_fadetoblack.GetBool() && !IsAlive() )
	{
		m_iIDEntIndex = 0;
		return;
	}

	if ( m_iForcedIDTarget )
	{
		m_iIDEntIndex = m_iForcedIDTarget;
		return;
	}

	// If we're in deathcam, ID our killer
	if ( (GetObserverMode() == OBS_MODE_DEATHCAM || GetObserverMode() == OBS_MODE_CHASE) && GetObserverTarget() && GetObserverTarget() != GetLocalTFPlayer() )
	{
		m_iIDEntIndex = GetObserverTarget()->entindex();
		return;
	}

	// Clear old target and find a new one
	m_iIDEntIndex = 0;

	trace_t tr;
	Vector vecStart, vecEnd;
	VectorMA( MainViewOrigin(), MAX_TRACE_LENGTH, MainViewForward(), vecEnd );
	VectorMA( MainViewOrigin(), 10,   MainViewForward(), vecStart );

	// If we're in observer mode, ignore our observer target. Otherwise, ignore ourselves.
	if ( IsObserver() )
	{
		UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, GetObserverTarget(), COLLISION_GROUP_NONE, &tr );
	}
	else
	{
		UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	}

	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		C_BaseEntity *pEntity = tr.m_pEnt;

		if ( pEntity && ( pEntity != this ) )
		{
			m_iIDEntIndex = pEntity->entindex();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Display appropriate hints for the target we're looking at
//-----------------------------------------------------------------------------
void C_TFPlayer::DisplaysHintsForTarget( C_BaseEntity *pTarget )
{
	// If the entity provides hints, ask them if they have one for this player
	ITargetIDProvidesHint *pHintInterface = dynamic_cast<ITargetIDProvidesHint*>(pTarget);
	if ( pHintInterface )
	{
		pHintInterface->DisplayHintTo( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_TFPlayer::GetRenderTeamNumber( void )
{
	return m_nSkin;
}

static Vector WALL_MIN(-WALL_OFFSET,-WALL_OFFSET,-WALL_OFFSET);
static Vector WALL_MAX(WALL_OFFSET,WALL_OFFSET,WALL_OFFSET);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector C_TFPlayer::GetDeathViewPosition()
{
	Vector origin = EyePosition();

	C_TFRagdoll *pRagdoll = static_cast<C_TFRagdoll*>( m_hRagdoll.Get() );
	if ( pRagdoll )
	{
		if ( pRagdoll->IsDeathAnim() )
		{
			origin.z += VEC_DEAD_VIEWHEIGHT_SCALED( this ).z*4;
		}
		else
		{
			IRagdoll *pIRagdoll = GetRepresentativeRagdoll();
			if ( pIRagdoll )
			{
				origin = pIRagdoll->GetRagdollOrigin();
				origin.z += VEC_DEAD_VIEWHEIGHT_SCALED( this ).z; // look over ragdoll, not through
			}
		}
	}

	return origin;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::CalcDeathCamView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	CBaseEntity	* killer = GetObserverTarget();

	// Swing to face our killer within half the death anim time
	float interpolation = ( gpGlobals->curtime - m_flDeathTime ) / (TF_DEATH_ANIMATION_TIME * 0.5);
	interpolation = clamp( interpolation, 0.0f, 1.0f );
	interpolation = SimpleSpline( interpolation );

	m_flObserverChaseDistance += gpGlobals->frametime*48.0f;
	m_flObserverChaseDistance = clamp(m_flObserverChaseDistance, CHASE_CAM_DISTANCE_MIN, CHASE_CAM_DISTANCE_MAX);

	QAngle aForward = eyeAngles = EyeAngles();
	Vector origin = EyePosition();			

	IRagdoll *pRagdoll = GetRepresentativeRagdoll();
	if ( pRagdoll )
	{
		origin = pRagdoll->GetRagdollOrigin();
		origin.z += VEC_DEAD_VIEWHEIGHT.z; // look over ragdoll, not through
	}

	if ( killer && (killer != this) ) 
	{
		C_TFPlayer *pPlayer = ToTFPlayer( killer );

		// DOD specific behaviour
		if ( pPlayer && pPlayer->IsDODClass() )
		{
			Vector vecKiller = killer->GetAbsOrigin();
			if ( pPlayer->IsAlive() )
			{
				if ( pPlayer->m_Shared.IsProne() )
					VectorAdd( vecKiller, VEC_PRONE_VIEW_SCALED( this ), vecKiller );
				else if( pPlayer->GetFlags() & FL_DUCKING )
					VectorAdd( vecKiller, VEC_DUCK_VIEW_SCALED_DOD( this ), vecKiller );
				else
					VectorAdd( vecKiller, VEC_VIEW_SCALED_DOD( this ), vecKiller );
			}

			Vector vecToKiller = vecKiller - origin;
			QAngle aKiller;
			VectorAngles( vecToKiller, aKiller );
			InterpolateAngles( aForward, aKiller, eyeAngles, interpolation );
		}
		else
		{
			// Original TF behaviour
			Vector vKiller = killer->EyePosition() - origin;
			QAngle aKiller; VectorAngles( vKiller, aKiller );
			InterpolateAngles( aForward, aKiller, eyeAngles, interpolation );
		}
	};

	Vector vForward; AngleVectors( eyeAngles, &vForward );

	VectorNormalize( vForward );

	VectorMA( origin, -m_flObserverChaseDistance, vForward, eyeOrigin );

	trace_t trace; // clip against world
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( origin, eyeOrigin, WALL_MIN, WALL_MAX, MASK_SOLID, this, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();

	if (trace.fraction < 1.0)
	{
		eyeOrigin = trace.endpos;
		m_flObserverChaseDistance = VectorLength(origin - eyeOrigin);
	}

	fov = GetFOV();
}

void C_TFPlayer::CalcChaseCamView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	C_BaseEntity *target = GetObserverTarget();

	if ( !target ) 
	{
		// just copy a save in-map position
		VectorCopy( EyePosition(), eyeOrigin );
		VectorCopy( EyeAngles(), eyeAngles );
		return;
	};

	C_TFPlayer *player = ToTFPlayer( target );
	if ( !player->IsDODClass() ) // Only return base class on non-dod classes
		return BaseClass::CalcChaseCamView( eyeOrigin, eyeAngles, fov );

	Vector forward, viewpoint;

	// GetRenderOrigin() returns ragdoll pos if player is ragdolled
	Vector origin = target->GetRenderOrigin();

	if ( player && player->IsAlive() )
	{
		if ( player->m_Shared.IsProne() )
		{
			VectorAdd( origin, VEC_PRONE_VIEW_SCALED( this ), origin );
		}
		else if( player->GetFlags() & FL_DUCKING )
		{
			Vector DuckView = VEC_DUCK_VIEW_SCALED( this );

			if ( player->IsDODClass() )
				DuckView = VEC_DUCK_VIEW_SCALED_DOD( this );

			VectorAdd( origin, DuckView, origin );
		}
		else
		{
			Vector View = VEC_VIEW_SCALED( this );

			if ( player->IsDODClass() )
				View = VEC_VIEW_SCALED_DOD( this );

			VectorAdd( origin, View, origin );
		}
	}
	else
	{
		Vector DeadView = VEC_DEAD_VIEWHEIGHT_SCALED( this );

		if ( player && player->IsDODClass() )
			DeadView = VEC_DEAD_VIEWHEIGHT_SCALED_DOD( this );

		// assume it's the players ragdoll
		VectorAdd( origin, DeadView, origin );
	}

	QAngle viewangles;

	if ( IsLocalPlayer() )
	{
		engine->GetViewAngles( viewangles );
	}
	else
	{
		viewangles = EyeAngles();
	}

	m_flObserverChaseDistance += gpGlobals->frametime*48.0f;
	m_flObserverChaseDistance = clamp( m_flObserverChaseDistance, CHASE_CAM_DISTANCE_MIN, CHASE_CAM_DISTANCE_MAX );

	AngleVectors( viewangles, &forward );

	VectorNormalize( forward );

	VectorMA(origin, -m_flObserverChaseDistance, forward, viewpoint );

	trace_t trace;
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( origin, viewpoint, WALL_MIN, WALL_MAX, MASK_SOLID, target, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();

	if (trace.fraction < 1.0)
	{
		viewpoint = trace.endpos;
		m_flObserverChaseDistance = VectorLength(origin - eyeOrigin);
	}

	VectorCopy( viewangles, eyeAngles );
	VectorCopy( viewpoint, eyeOrigin );

	fov = GetFOV();
}

//-----------------------------------------------------------------------------
// Purpose: Do nothing multiplayer_animstate takes care of animation.
// Input  : playerAnim - 
//-----------------------------------------------------------------------------
void C_TFPlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	return;
}

float C_TFPlayer::GetMinFOV() const
{
	// Min FOV for Sniper Rifle
	return 20;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const QAngle& C_TFPlayer::EyeAngles()
{
	if ( IsLocalPlayer() && g_nKillCamMode == OBS_MODE_NONE )
	{
		return BaseClass::EyeAngles();
	}
	else
	{
		return m_angEyeAngles;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &color - 
//-----------------------------------------------------------------------------
void C_TFPlayer::GetTeamColor( Color &color )
{
	color[3] = 255;

	if ( GetTeamNumber() == TF_TEAM_RED )
	{
		color[0] = 159;
		color[1] = 55;
		color[2] = 34;
	}
	else if ( GetTeamNumber() == TF_TEAM_BLUE )
	{
		color[0] = 76;
		color[1] = 109;
		color[2] = 129;
	}
	else
	{
		color[0] = 255;
		color[1] = 255;
		color[2] = 255;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bCopyEntity - 
// Output : C_BaseAnimating *
//-----------------------------------------------------------------------------
C_BaseAnimating *C_TFPlayer::BecomeRagdollOnClient()
{
	// Let the C_TFRagdoll take care of this.
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IRagdoll*
//-----------------------------------------------------------------------------
IRagdoll* C_TFPlayer::GetRepresentativeRagdoll() const
{
	if ( m_hRagdoll.Get() )
	{
		C_TFRagdoll *pRagdoll = static_cast<C_TFRagdoll*>( m_hRagdoll.Get() );
		if ( !pRagdoll )
			return NULL;

		return pRagdoll->GetIRagdoll();
	}
	else
	{
		return NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::InitPlayerGibs( void )
{
	// Clear out the gib list and create a new one.
	m_aGibs.Purge();
	m_aNormalGibs.PurgeAndDeleteElements();
	m_aSillyGibs.Purge();

#if 0 // SEALTODO
	int nModelIndex = GetPlayerClass()->HasCustomModel() ? modelinfo->GetModelIndex( GetPlayerClass()->GetModelName() ) : GetModelIndex();
#else
	int nModelIndex = GetModelIndex();
#endif
	BuildGibList( m_aGibs, nModelIndex, 1.0f, COLLISION_GROUP_NONE );

	if ( TFGameRules() && TFGameRules()->IsBirthday() )
	{
		for ( int i = 0; i < m_aGibs.Count(); i++ )
		{
			if ( RandomFloat(0,1) < 0.75 )
			{
				V_strcpy_safe( m_aGibs[i].modelName, g_pszBDayGibs[ RandomInt(0,ARRAYSIZE(g_pszBDayGibs)-1) ] );
			}
		}
	}

	// Copy the normal gibs list to be saved for later when swapping with Pyro Vision
	FOR_EACH_VEC ( m_aGibs, i )
	{
		char *cloneStr = new char [ 512 ];
		Q_strncpy( cloneStr, m_aGibs[i].modelName, 512 );
		m_aNormalGibs.AddToTail( cloneStr );

		// Create a list of silly gibs
		int iRandIndex = RandomInt(4,ARRAYSIZE(g_pszBDayGibs)-1);
		m_aSillyGibs.AddToTail( iRandIndex );
	}
}

//-----------------------------------------------------------------------------
// Purpose : Checks vision flags and ensures the proper gib models are loaded for vision mode
//-----------------------------------------------------------------------------
void C_TFPlayer::CheckAndUpdateGibType( void )
{
	// check the first gib, if it's different copy them all over
	if ( UTIL_IsLowViolence() )
	{
		if ( Q_strcmp( m_aGibs[0].modelName, g_pszBDayGibs[ m_aSillyGibs[0] ]) != 0 )
		{
			FOR_EACH_VEC( m_aGibs, i )
			{
				V_strcpy_safe( m_aGibs[i].modelName, g_pszBDayGibs[ m_aSillyGibs[i] ] );
			}
		}
	}
	else 
	{
		if ( Q_strcmp( m_aGibs[0].modelName, m_aNormalGibs[0]) != 0 )
		{
			FOR_EACH_VEC( m_aGibs, i )
			{
				V_strcpy_safe( m_aGibs[i].modelName, m_aNormalGibs[i] );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecOrigin - 
//			&vecVelocity - 
//			&vecImpactVelocity - 
//-----------------------------------------------------------------------------
void C_TFPlayer::CreatePlayerGibs( const Vector &vecOrigin, const Vector &vecVelocity, float flImpactScale, bool bBurning, bool bOnlyHead, bool bDisguiseGibs )
{
	// Make sure we have Gibs to create.
	if ( m_aGibs.Count() == 0 )
		return;

	AngularImpulse angularImpulse( RandomFloat( 0.0f, 120.0f ), RandomFloat( 0.0f, 120.0f ), 0.0 );

	Vector vecBreakVelocity = vecVelocity;
	vecBreakVelocity.z += tf_playergib_forceup.GetFloat();
	VectorNormalize( vecBreakVelocity );
	vecBreakVelocity *= tf_playergib_force.GetFloat();

	// Cap the impulse.
	float flSpeed = vecBreakVelocity.Length();
	if ( flSpeed > tf_playergib_maxspeed.GetFloat() )
	{
		VectorScale( vecBreakVelocity, tf_playergib_maxspeed.GetFloat() / flSpeed, vecBreakVelocity );
	}

	breakablepropparams_t breakParams( vecOrigin, GetRenderAngles(), vecBreakVelocity, angularImpulse );
	breakParams.impactEnergyScale = 1.0f;//

	// Gib the player's body.
	m_hHeadGib = NULL;
	m_hSpawnedGibs.Purge();

#if 0
	bool bHasCustomModel = GetPlayerClass()->HasCustomModel();
#else
	bool bHasCustomModel = false;
#endif
	int nModelIndex = bHasCustomModel ? modelinfo->GetModelIndex( GetPlayerClass()->GetModelName() ) : GetModelIndex();

	if ( bOnlyHead )
	{
		if ( UTIL_IsLowViolence() )
		{
			// No bloody gibs with pyro-vision goggles
			return;
		}

		// Create only a head gib.
		CUtlVector<breakmodel_t> headGib;
		int nClassIndex = GetPlayerClass()->GetClassIndex();
		for ( int i=0; i<m_aGibs.Count(); ++i )
		{
			if ( Q_strcmp( m_aGibs[i].modelName, g_pszHeadGibs[nClassIndex] ) == 0 )
			{
				headGib.AddToHead( m_aGibs[i] );
			}
		}
			
		m_hFirstGib = CreateGibsFromList( headGib, nModelIndex, NULL, breakParams, this, -1 , false, true, &m_hSpawnedGibs, bBurning );
		m_hHeadGib = m_hFirstGib;
		if ( m_hFirstGib )
		{
			IPhysicsObject *pPhysicsObject = m_hFirstGib->VPhysicsGetObject();
			if( pPhysicsObject )
			{
				// Give the head some rotational damping so it doesn't roll so much (for the player's view).
				float damping, rotdamping;
				pPhysicsObject->GetDamping( &damping, &rotdamping );
				rotdamping *= 6.f;
				pPhysicsObject->SetDamping( &damping, &rotdamping );
			}
		}
	}
	else
	{
		CheckAndUpdateGibType();
		m_hFirstGib = CreateGibsFromList( m_aGibs, nModelIndex, NULL, breakParams, this, -1 , false, true, &m_hSpawnedGibs, bBurning );
	}
	DropPartyHat( breakParams, vecBreakVelocity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::DropPartyHat( breakablepropparams_t &breakParams, Vector &vecBreakVelocity )
{
	if ( m_hPartyHat )
	{
		breakmodel_t breakModel;
		Q_strncpy( breakModel.modelName, BDAY_HAT_MODEL, sizeof(breakModel.modelName) );
		breakModel.health = 1;
		breakModel.fadeTime = RandomFloat(5,10);
		breakModel.fadeMinDist = 0.0f;
		breakModel.fadeMaxDist = 0.0f;
		breakModel.burstScale = breakParams.defBurstScale;
		breakModel.collisionGroup = COLLISION_GROUP_DEBRIS;
		breakModel.isRagdoll = false;
		breakModel.isMotionDisabled = false;
		breakModel.placementName[0] = 0;
		breakModel.placementIsBone = false;
		breakModel.offset = GetAbsOrigin() - m_hPartyHat->GetAbsOrigin();
		BreakModelCreateSingle( this, &breakModel, m_hPartyHat->GetAbsOrigin(), m_hPartyHat->GetAbsAngles(), vecBreakVelocity, breakParams.angularVelocity, m_hPartyHat->m_nSkin, breakParams );

		m_hPartyHat->Release();
	}
}

//-----------------------------------------------------------------------------
// Purpose: How many buildables does this player own
//-----------------------------------------------------------------------------
int	C_TFPlayer::GetObjectCount( void )
{
	return m_aObjects.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Get a specific buildable that this player owns
//-----------------------------------------------------------------------------
C_BaseObject *C_TFPlayer::GetObject( int index_ )
{
	return m_aObjects[index_].Get();
}

//-----------------------------------------------------------------------------
// Purpose: Get a specific buildable that this player owns
//-----------------------------------------------------------------------------
C_BaseObject *C_TFPlayer::GetObjectOfType( int iObjectType, int iObjectMode ) const
{
	int iCount = m_aObjects.Count();

	for ( int i=0;i<iCount;i++ )
	{
		C_BaseObject *pObj = m_aObjects[i].Get();

		if ( !pObj )
			continue;

		if ( pObj->IsDormant() || pObj->IsMarkedForDeletion() )
			continue;

		if ( pObj->GetType() != iObjectType )
			continue;

		if ( pObj->GetObjectMode() != iObjectMode )
			continue;

		return pObj;
	}
	
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : collisionGroup - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_TFPlayer::ShouldCollide( int collisionGroup, int contentsMask ) const
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

float C_TFPlayer::GetPercentInvisible( void )
{
	return m_Shared.GetPercentInvisible();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_TFPlayer::GetSkin()
{
	C_TFPlayer *pLocalPlayer = GetLocalTFPlayer();

	if ( !pLocalPlayer )
		return 0;

	int iVisibleTeam = GetTeamNumber();

	// if this player is disguised and on the other team, use disguise team
	if ( m_Shared.InCond( TF_COND_DISGUISED ) && IsEnemyPlayer() )
	{
		iVisibleTeam = m_Shared.GetDisguiseTeam();
	}

	int nSkin;

	switch( iVisibleTeam )
	{
	case TF_TEAM_RED:
		nSkin = 0;
		break;

	case TF_TEAM_BLUE:
		nSkin = 1;
		break;

	default:
		nSkin = 0;
		break;
	}

	// Assume we'll switch skins to show the spy mask
	bool bCheckSpyMask = true;

	// 3 and 4 are invulnerable
	if ( m_Shared.IsInvulnerable() && 
		 ( !m_Shared.InCond( TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED ) || gpGlobals->curtime < GetLastDamageTime() + 2.0f ) )
	{
		nSkin += 2;
		bCheckSpyMask = false;
	}

	if ( bCheckSpyMask && m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		if ( !IsEnemyPlayer() )
		{
			nSkin += 4 + ( ( m_Shared.GetDisguiseClass() - TF_FIRST_NORMAL_CLASS ) * 2 );
		}
		else if ( m_Shared.GetDisguiseClass() == TF_CLASS_SPY )
		{
			nSkin += 4 + ( ( m_Shared.GetDisguiseMask() - TF_FIRST_NORMAL_CLASS ) * 2 );
		}
	}

	if ( IsDODClass() )
	{
		nSkin = 0;

		if ( m_Shared.InCond( TF_COND_INVULNERABLE ) )
			nSkin = 1;
	}

	return nSkin;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iClass - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_TFPlayer::IsPlayerClass( int iClass ) const
{
	const C_TFPlayerClass *pClass = GetPlayerClass();
	if ( !pClass )
		return false;

	return ( pClass->GetClassIndex() == iClass );
}

//-----------------------------------------------------------------------------
// Purpose: Don't take damage decals while stealthed
//-----------------------------------------------------------------------------
void C_TFPlayer::AddDecal( const Vector& rayStart, const Vector& rayEnd,
							const Vector& decalCenter, int hitbox, int decalIndex, bool doTrace, trace_t& tr, int maxLODToDecal )
{
	if ( m_Shared.InCond( TF_COND_STEALTHED ) )
	{
		return;
	}

	if ( m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		return;
	}

	if ( m_Shared.InCond( TF_COND_INVULNERABLE ) )
	{ 
		Vector vecDir = rayEnd - rayStart;
		VectorNormalize(vecDir);
		g_pEffects->Ricochet( rayEnd - (vecDir * 8), -vecDir );
		return;
	}

	// don't decal from inside the player
	if ( tr.startsolid )
	{
		return;
	}

	BaseClass::AddDecal( rayStart, rayEnd, decalCenter, hitbox, decalIndex, doTrace, tr, maxLODToDecal );
}

//-----------------------------------------------------------------------------
// Called every time the player respawns
//-----------------------------------------------------------------------------
void C_TFPlayer::ClientPlayerRespawn( void )
{
	if ( IsLocalPlayer() )
	{
		// DOD
		MoveToLastReceivedPosition( true );
		ResetLatched();
		m_Shared.m_bForceProneChange = true;

		// Reset the camera.
		HandleTaunting();

		ResetToneMapping(1.0);

		// Release the duck toggle key
		KeyUp( &in_ducktoggle, NULL );

		IGameEvent *event = gameeventmanager->CreateEvent( "localplayer_respawn" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}

		SetAppropriateCamera( this );
	}

	UpdateVisibility();

	m_hFirstGib = NULL;
	m_hSpawnedGibs.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::CreateSaveMeEffect( void )
{
	// Don't create them for the local player in first-person view.
	if ( IsLocalPlayer() && InFirstPersonView() )
		return;

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	// If I'm disguised as the enemy, play to all players
	if ( m_Shared.InCond( TF_COND_DISGUISED ) && m_Shared.GetDisguiseTeam() != GetTeamNumber() && !m_Shared.IsStealthed() )
	{
		// play to all players
	}
	else
	{
		// only play to teammates
		if ( pLocalPlayer && pLocalPlayer->GetTeamNumber() != GetTeamNumber() )
			return;
	}

	if ( m_pSaveMeEffect )
	{
		ParticleProp()->StopEmission( m_pSaveMeEffect );
		m_pSaveMeEffect = NULL;
	}

	m_pSaveMeEffect = ParticleProp()->Create( "speech_mediccall", PATTACH_POINT_FOLLOW, "head" );

	// If the local player is a medic, add this player to our list of medic callers
	if ( pLocalPlayer && pLocalPlayer->IsPlayerClass( TF_CLASS_MEDIC ) && pLocalPlayer->IsAlive() == true )
	{
		Vector vecPos;
		if ( GetAttachmentLocal( LookupAttachment( "head" ), vecPos ) )
		{
			vecPos += Vector(0,0,18);	// Particle effect is 18 units above the attachment
			CTFMedicCallerPanel::AddMedicCaller( this, 5.0, vecPos );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_TFPlayer::IsOverridingViewmodel( void )
{
	C_TFPlayer *pPlayer = this;
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && 
		 pLocalPlayer->GetObserverTarget() && pLocalPlayer->GetObserverTarget()->IsPlayer() )
	{
		pPlayer = assert_cast<C_TFPlayer*>(pLocalPlayer->GetObserverTarget());
	}

	if ( pPlayer->m_Shared.InCond( TF_COND_INVULNERABLE ) )
		return true;

	return BaseClass::IsOverridingViewmodel();
}

//-----------------------------------------------------------------------------
// Purpose: Draw my viewmodel in some special way
//-----------------------------------------------------------------------------
int	C_TFPlayer::DrawOverriddenViewmodel( C_BaseViewModel *pViewmodel, int flags )
{
	int ret = 0;

	C_TFPlayer *pPlayer = this;
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && 
		pLocalPlayer->GetObserverTarget() && pLocalPlayer->GetObserverTarget()->IsPlayer() )
	{
		pPlayer = assert_cast<C_TFPlayer*>(pLocalPlayer->GetObserverTarget());
	}

	if ( pPlayer->m_Shared.InCond( TF_COND_INVULNERABLE ) )
	{
		// Force the invulnerable material
		modelrender->ForcedMaterialOverride( *pPlayer->GetInvulnMaterialRef() );

		ret = pViewmodel->DrawOverriddenViewmodel( flags );

		modelrender->ForcedMaterialOverride( NULL );
	}

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void BuildBigHeadTransformations( CBaseAnimating *pObject, CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, float flScale )
{
	if ( !pObject || flScale == 1.f )
		return;

	// Scale the head.
	int iHeadBone = pObject->LookupBone( "bip_head" );
	if ( iHeadBone == -1 )
		return;

	matrix3x4_t  &transform = pObject->GetBoneForWrite( iHeadBone );
	Vector head_position;
	MatrixGetTranslation( transform, head_position );

	// Scale the head.
	MatrixScaleBy ( flScale, transform );

	const int cMaxNumHelms = 2;
	int iHelmIndex[cMaxNumHelms];
	iHelmIndex[0] = pObject->LookupBone( "prp_helmet" );
	iHelmIndex[1] = pObject->LookupBone( "prp_hat" );

	for ( int i = 0; i < cMaxNumHelms; i++ )
	{
		if ( iHelmIndex[i] != -1 )
		{
			matrix3x4_t &transformhelmet = pObject->GetBoneForWrite( iHelmIndex[i] );
			MatrixScaleBy ( flScale, transformhelmet );

			Vector helmet_position;
			MatrixGetTranslation( transformhelmet, helmet_position );
			Vector offset = helmet_position - head_position;
			MatrixSetTranslation ( ( offset * flScale ) + head_position, transformhelmet );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void BuildDecapitatedTransformations( CBaseAnimating *pObject, CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	if ( !pObject )
		return;

	// Scale the head to nothing.
	int iHeadBone = pObject->LookupBone( "bip_head" );
	if ( iHeadBone != -1 )
	{	
		matrix3x4_t  &transform = pObject->GetBoneForWrite( iHeadBone );
		MatrixScaleByZero ( transform );
	}

	int iHelm = pObject->LookupBone( "prp_helmet" );
	if ( iHelm != -1 )
	{
		// Scale the helmet.
		matrix3x4_t  &transformhelmet = pObject->GetBoneForWrite( iHelm );
		MatrixScaleByZero ( transformhelmet );
	}

	iHelm = pObject->LookupBone( "prp_hat" );
	if ( iHelm != -1 )
	{
		matrix3x4_t  &transformhelmet = pObject->GetBoneForWrite( iHelm );
		MatrixScaleByZero ( transformhelmet );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void BuildNeckScaleTransformations( CBaseAnimating *pObject, CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, float flScale, int iClass )
{
	if ( !pObject || flScale == 1.f )
		return;

	int iNeck = pObject->LookupBone( "bip_neck" );
	if ( iNeck == -1 )
		return;

	matrix3x4_t &neck_transform = pObject->GetBoneForWrite( iNeck );

	Vector spine_position, neck_position, head_position, position, offset(0, 0, 0);
	if ( iClass != TF_CLASS_HEAVYWEAPONS )
	{
		// Compress the neck into the spine.
		int iSpine = pObject->LookupBone( "bip_spine_3" );
		if ( iSpine != -1 )
		{
			matrix3x4_t &spine_transform = pObject->GetBoneForWrite( iSpine );
			MatrixPosition( spine_transform, spine_position );
			MatrixPosition( neck_transform, neck_position );
			position = flScale * ( neck_position - spine_position );
			MatrixSetTranslation( spine_position + position, neck_transform );
		}
	}

	if ( iClass == TF_CLASS_SPY )
	{
		int iCig = pObject->LookupBone( "prp_cig" );
		if ( iCig != -1 )
		{
			matrix3x4_t  &cig_transform = pObject->GetBoneForWrite( iCig );
			MatrixScaleByZero ( cig_transform );
		}
	}

	// Compress the head into the neck.
	int iHead = pObject->LookupBone( "bip_head" );
	if ( iHead != -1 )
	{
		matrix3x4_t  &head_transform = pObject->GetBoneForWrite( iHead );
		MatrixPosition( head_transform, head_position );
		MatrixPosition( neck_transform, neck_position );
		offset = head_position - neck_position;
		MatrixSetTranslation( neck_position, head_transform );
	}

	// Store helmet bone offset.
	int iHelm = pObject->LookupBone( "prp_helmet" );
	if ( iHelm != -1 ) 
	{
		matrix3x4_t  &helmet_transform = pObject->GetBoneForWrite( iHelm );
		MatrixPosition( helmet_transform, position );
		MatrixSetTranslation( position - offset, helmet_transform );
	}

	// Store alternate helmet bone offset.
	iHelm = pObject->LookupBone( "prp_hat" );
	if ( iHelm != -1 )
	{
		matrix3x4_t  &hat_transform = pObject->GetBoneForWrite( iHelm );
		MatrixPosition( hat_transform, position );
		MatrixSetTranslation( position - offset, hat_transform );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get child bones from a specified bone index
//-----------------------------------------------------------------------------
void AppendChildren_R( CUtlVector< const mstudiobone_t * > *pChildBones, const studiohdr_t *pHdr, int nBone )
{
	if ( !pChildBones || !pHdr )
		return;

    // Child bones have to have a larger bone index than their parent, so start searching from nBone + 1
    for ( int i = nBone + 1; i < pHdr->numbones; ++i )
    {
        const mstudiobone_t *pBone = pHdr->pBone( i );
        if ( pBone->parent == nBone )
        {
            pChildBones->AddToTail( pBone );
            // If you just want immediate children don't recurse, this will do a depth first traversal, could do
			// breadth first by adding all children first and then looping through the added bones and recursing
            AppendChildren_R( pChildBones, pHdr, i );
        }
    }
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void BuildTorsoScaleTransformations( CBaseAnimating *pObject, CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, float flScale, int iClass )
{
	if ( !pObject || flScale == 1.f )
		return;

	int iPelvis = pObject->LookupBone( "bip_pelvis" );
	if ( iPelvis == -1 )
		return;

	const studiohdr_t *pHdr = modelinfo->GetStudiomodel( pObject->GetModel() );

	int iTargetBone = iPelvis;

	// must be in this order
	static const char *s_torsoBoneNames[] =
	{
		"bip_spine_0",
		"bip_spine_1",
		"bip_spine_2",
		"bip_spine_3",
		"bip_neck"
	};

	// Compress torso bones toward pelvis in order.
	for ( int i=0; i<ARRAYSIZE( s_torsoBoneNames ); ++i )
	{
		int iMoveBone = pObject->LookupBone( s_torsoBoneNames[i] );
		if ( iMoveBone == -1 )
		{
			return;
		}

		const matrix3x4_t &targetBone_transform = pObject->GetBone( iTargetBone );
		Vector vTargetBonePos;
		MatrixPosition( targetBone_transform, vTargetBonePos );

		matrix3x4_t &moveBone_transform = pObject->GetBoneForWrite( iMoveBone );
		Vector vMoveBonePos;
		MatrixPosition( moveBone_transform, vMoveBonePos );
		Vector vNewMovePos = vTargetBonePos + flScale * ( vMoveBonePos - vTargetBonePos );
		MatrixSetTranslation( vNewMovePos, moveBone_transform );

		iTargetBone = iMoveBone;

		Vector vOffset = vNewMovePos - vMoveBonePos;

		// apply to all its child bones
		CUtlVector< const mstudiobone_t * > vecChildBones;
		AppendChildren_R( &vecChildBones, pHdr, iMoveBone );
		for ( int j=0; j<vecChildBones.Count(); ++j )
		{
			int iChildBone = pObject->LookupBone( vecChildBones[j]->pszName() );
			if ( iChildBone == -1 )
				continue;

			matrix3x4_t &childBone_transform = pObject->GetBoneForWrite( iChildBone );
			Vector vChildPos;
			MatrixPosition( childBone_transform, vChildPos );
			MatrixSetTranslation( vChildPos + vOffset, childBone_transform );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void BuildHandScaleTransformations( CBaseAnimating *pObject, CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed, float flScale )
{
	if ( !pObject || flScale == 1.f )
		return;

	const studiohdr_t *pHdr = modelinfo->GetStudiomodel( pObject->GetModel() );

	// must be in this order
	static const char *s_handBoneNames[] =
	{
		"bip_hand_L",
		"bip_hand_R"
	};

	// scale hand bones
	for ( int i=0; i<ARRAYSIZE( s_handBoneNames ); ++i )
	{
		int iHand = pObject->LookupBone( s_handBoneNames[i] );
		if ( iHand == -1 )
		{
			continue;
		}

		matrix3x4_t& transform = pObject->GetBoneForWrite( iHand );
		MatrixScaleBy( flScale, transform );

		// apply to all its child bones
		CUtlVector< const mstudiobone_t * > vecChildBones;
		AppendChildren_R( &vecChildBones, pHdr, iHand );
		for ( int j=0; j<vecChildBones.Count(); ++j )
		{
			int iChildBone = pObject->LookupBone( vecChildBones[j]->pszName() );
			if ( iChildBone == -1 )
				continue;

			matrix3x4_t &childBone_transform = pObject->GetBoneForWrite( iChildBone );
			MatrixScaleBy( flScale, childBone_transform );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	BaseClass::BuildTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed );

	if ( GetGroundEntity() == NULL )
	{
		Vector hullSizeNormal = VEC_HULL_MAX_SCALED( this ) - VEC_HULL_MIN_SCALED( this );
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX_SCALED( this ) - VEC_DUCK_HULL_MIN_SCALED( this );
		Vector duckOffset = ( hullSizeNormal - hullSizeCrouch );

		// The player is in the air.
		if ( GetFlags() & FL_DUCKING )
		{
			if ( !m_bDuckJumpInterp )
			{
				m_flFirstDuckJumpInterp = gpGlobals->curtime;
			}
			m_bDuckJumpInterp = true;
			m_flLastDuckJumpInterp = gpGlobals->curtime;

			float flRatio = MIN( 0.15f, gpGlobals->curtime - m_flFirstDuckJumpInterp ) / 0.15f;
			m_flDuckJumpInterp = 1.f - flRatio;
		}
		else if ( m_bDuckJumpInterp )
		{
			float flRatio = MIN( 0.15f, gpGlobals->curtime - m_flLastDuckJumpInterp ) / 0.15f;
			m_flDuckJumpInterp = -(1.f - flRatio);
			if ( m_flDuckJumpInterp == 0.f )
			{
				// Turn off interpolation when we return to our normal, unducked location.
				m_bDuckJumpInterp = false;
			}
		}

		if ( m_bDuckJumpInterp && m_flDuckJumpInterp != 0.f )
		{
			duckOffset *= m_flDuckJumpInterp;
			for (int i = 0; i < hdr->numbones(); i++) 
			{
				if ( !( hdr->boneFlags( i ) & boneMask ) )
					continue;

				matrix3x4_t &transform = GetBoneForWrite( i );
				Vector bone_pos;
				MatrixGetTranslation( transform, bone_pos );
				MatrixSetTranslation( bone_pos - duckOffset, transform );
			}
		}
	}
	else if ( m_bDuckJumpInterp )
	{
		m_bDuckJumpInterp = false;
	}

	m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );
	float flHeadScale = m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) ? 1.5 : m_flHeadScale;
	BuildBigHeadTransformations( this, hdr, pos, q, cameraTransform, boneMask, boneComputed, flHeadScale );
	BuildTorsoScaleTransformations( this, hdr, pos, q, cameraTransform, boneMask, boneComputed, m_flTorsoScale, GetPlayerClass()->GetClassIndex() );
	BuildHandScaleTransformations( this, hdr, pos, q, cameraTransform, boneMask, boneComputed, m_flHandScale );

	BuildFirstPersonMeathookTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed, "bip_head" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFRagdoll::BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	BaseClass::BuildTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed );

	m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );
	BuildBigHeadTransformations( this, hdr, pos, q, cameraTransform, boneMask, boneComputed, m_flHeadScale );
	BuildTorsoScaleTransformations( this, hdr, pos, q, cameraTransform, boneMask, boneComputed, m_flTorsoScale, GetClass() );
	BuildHandScaleTransformations( this, hdr, pos, q, cameraTransform, boneMask, boneComputed, m_flHandScale );

	if ( IsDecapitation() && !m_bBaseTransform )
	{
		m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );
		BuildDecapitatedTransformations( this, hdr, pos, q, cameraTransform, boneMask, boneComputed );
	}

	if ( IsHeadSmash() && !m_bBaseTransform )
	{
		m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );
		BuildNeckScaleTransformations( this, hdr, pos, q, cameraTransform, boneMask, boneComputed, 0.5f, GetClass() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::SetHealer( C_TFPlayer *pHealer, float flChargeLevel )
{
	if ( pHealer && IsPlayerClass( TF_CLASS_SPY ) )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "player_healedbymedic" );

		if ( event )
		{
			event->SetInt( "medic", pHealer->entindex() );
			gameeventmanager->FireEventClientSide( event );
		}
	}

	// We may be getting healed by multiple healers. Show the healer
	// who's got the highest charge level.
	if ( m_hHealer )
	{
		if ( m_flHealerChargeLevel > flChargeLevel )
			return;
	}

	m_hHealer = pHealer;
	m_flHealerChargeLevel = flChargeLevel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_TFPlayer::CanShowClassMenu( void )
{
	return ( GetTeamNumber() > LAST_SHARED_TEAM );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::InitializePoseParams( void )
{
	/*
	m_headYawPoseParam = LookupPoseParameter( "head_yaw" );
	GetPoseParameterRange( m_headYawPoseParam, m_headYawMin, m_headYawMax );

	m_headPitchPoseParam = LookupPoseParameter( "head_pitch" );
	GetPoseParameterRange( m_headPitchPoseParam, m_headPitchMin, m_headPitchMax );
	*/

	CStudioHdr *hdr = GetModelPtr();
	Assert( hdr );
	if ( !hdr )
		return;

	for ( int i = 0; i < hdr->GetNumPoseParameters() ; i++ )
	{
		SetPoseParameter( hdr, i, 0.0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector C_TFPlayer::GetChaseCamViewOffset( CBaseEntity *target )
{
	if ( target->IsBaseObject() )
		return Vector(0,0,64);

	// DOD
	C_TFPlayer *pPlayer = ToTFPlayer( target );
	if ( pPlayer && pPlayer->IsAlive() )
	{
		if ( pPlayer->m_Shared.IsProne() )
		{
			return VEC_PRONE_VIEW;
		}
	}

	return BaseClass::GetChaseCamViewOffset( target );
}

//-----------------------------------------------------------------------------
// Purpose: Called from PostDataUpdate to update the model index
//-----------------------------------------------------------------------------
void C_TFPlayer::ValidateModelIndex( void )
{
	if ( m_Shared.InCond( TF_COND_DISGUISED ) && IsEnemyPlayer() )
	{
		TFPlayerClassData_t *pData = GetPlayerClassData( m_Shared.GetDisguiseClass() );
		m_nModelIndex = modelinfo->GetModelIndex( pData->GetModelName() );
	}
	else
	{
		C_TFPlayerClass *pClass = GetPlayerClass();
		if ( pClass )
		{
			m_nModelIndex = modelinfo->GetModelIndex( pClass->GetModelName() );
		}
	}

	if ( m_iSpyMaskBodygroup > -1 && GetModelPtr() != NULL && IsPlayerClass( TF_CLASS_SPY ) )
	{
		if ( m_Shared.InCond( TF_COND_DISGUISED ) || m_Shared.InCond( TF_COND_DISGUISED_AS_DISPENSER ) )
		{
			if ( !IsEnemyPlayer() || (m_Shared.GetDisguiseClass() == TF_CLASS_SPY) )
			{
				SetBodygroup( m_iSpyMaskBodygroup, 1 );
			}
		}
		else
		{
			SetBodygroup( m_iSpyMaskBodygroup, 0 );
		}
	}

	BaseClass::ValidateModelIndex();
}

//-----------------------------------------------------------------------------
// Purpose: Simulate the player for this frame
//-----------------------------------------------------------------------------
void C_TFPlayer::Simulate( void )
{
	//Frame updates
	if ( this == C_BasePlayer::GetLocalPlayer() )
	{
		//Update the flashlight
		Flashlight();
	}

	// TF doesn't do step sounds based on velocity, instead using anim events
	// So we deliberately skip over the base player simulate, which calls them.
	BaseClass::BaseClass::Simulate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	if ( event == 7001 )
	{
		// Force a footstep sound
		m_flStepSoundTime = 0;
		Vector vel;
		EstimateAbsVelocity( vel );
		UpdateStepSound( GetGroundSurface(), GetAbsOrigin(), vel );
	}
	else if ( event == AE_WPN_HIDE )
	{
		if ( GetActiveWeapon() )
		{
			GetActiveWeapon()->SetWeaponVisible( false );
		}
	}
	else if ( event == AE_WPN_UNHIDE )
	{
		if ( GetActiveWeapon() )
		{
			GetActiveWeapon()->SetWeaponVisible( true );
		}
	}
	else if ( event == TF_AE_CIGARETTE_THROW )
	{
		CEffectData data;
		int iAttach = LookupAttachment( options );
		GetAttachment( iAttach, data.m_vOrigin, data.m_vAngles );

		data.m_vAngles = GetRenderAngles();

		data.m_hEntity = ClientEntityList().EntIndexToHandle( entindex() );
		DispatchEffect( "TF_ThrowCigarette", data );
		return;
	}
	else
		BaseClass::FireEvent( origin, angles, event, options );
}

// Shadows

ConVar cl_blobbyshadows( "cl_blobbyshadows", "0", FCVAR_CLIENTDLL );
ShadowType_t C_TFPlayer::ShadowCastType( void ) 
{
	// Removed the GetPercentInvisible - should be taken care off in BindProxy now.
	if ( !IsVisible() /*|| GetPercentInvisible() > 0.0f*/ )
		return SHADOWS_NONE;

	if ( IsEffectActive(EF_NODRAW | EF_NOSHADOW) )
		return SHADOWS_NONE;

	// If in ragdoll mode.
	if ( m_nRenderFX == kRenderFxRagdoll )
		return SHADOWS_NONE;

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	// if we're first person spectating this player
	if ( pLocalPlayer && 
		pLocalPlayer->GetObserverTarget() == this &&
		pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		return SHADOWS_NONE;		
	}

	if( cl_blobbyshadows.GetBool() )
		return SHADOWS_SIMPLE;

	return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;
}

float g_flFattenAmt = 4;
void C_TFPlayer::GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType )
{
	if ( shadowType == SHADOWS_SIMPLE )
	{
		// Don't let the render bounds change when we're using blobby shadows, or else the shadow
		// will pop and stretch.
		mins = CollisionProp()->OBBMins();
		maxs = CollisionProp()->OBBMaxs();
	}
	else
	{
		GetRenderBounds( mins, maxs );

		// We do this because the normal bbox calculations don't take pose params into account, and 
		// the rotation of the guy's upper torso can place his gun a ways out of his bbox, and 
		// the shadow will get cut off as he rotates.
		//
		// Thus, we give it some padding here.
		mins -= Vector( g_flFattenAmt, g_flFattenAmt, 0 );
		maxs += Vector( g_flFattenAmt, g_flFattenAmt, 0 );
	}
}


void C_TFPlayer::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	// TODO POSTSHIP - this hack/fix goes hand-in-hand with a fix in CalcSequenceBoundingBoxes in utils/studiomdl/simplify.cpp.
	// When we enable the fix in CalcSequenceBoundingBoxes, we can get rid of this.
	//
	// What we're doing right here is making sure it only uses the bbox for our lower-body sequences since,
	// with the current animations and the bug in CalcSequenceBoundingBoxes, are WAY bigger than they need to be.
	C_BaseAnimating::GetRenderBounds( theMins, theMaxs );
}


bool C_TFPlayer::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const
{ 
	if ( shadowType == SHADOWS_SIMPLE )
	{
		// Blobby shadows should sit directly underneath us.
		pDirection->Init( 0, 0, -1 );
		return true;
	}
	else
	{
		return BaseClass::GetShadowCastDirection( pDirection, shadowType );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether this player is the nemesis of the local player
//-----------------------------------------------------------------------------
bool C_TFPlayer::IsNemesisOfLocalPlayer()
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( pLocalPlayer )
	{
		// return whether this player is dominating the local player
		return m_Shared.IsPlayerDominated( pLocalPlayer->entindex() );
	}		
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether we should show the nemesis icon for this player
//-----------------------------------------------------------------------------
bool C_TFPlayer::ShouldShowNemesisIcon()
{
	// we should show the nemesis effect on this player if he is the nemesis of the local player,
	// and is not dead, cloaked or disguised
	if ( IsNemesisOfLocalPlayer() && g_PR && g_PR->IsConnected( entindex() ) )
	{
		bool bStealthed = m_Shared.IsStealthed();
		bool bDisguised = m_Shared.InCond( TF_COND_DISGUISED );
		if ( IsAlive() && !bStealthed && !bDisguised )
			return true;
	}

	return false;
}

bool C_TFPlayer::IsWeaponLowered( void )
{
	CPCWeaponBase *pWeapon = GetActivePCWeapon();

	if ( !pWeapon )
		return false;

	CTFGameRules *pRules = TFGameRules();

	// Lower losing team's weapons in bonus round
	if ( ( pRules->State_Get() == GR_STATE_TEAM_WIN ) && ( pRules->GetWinningTeam() != GetTeamNumber() ) )
		return true;

	// Hide all view models after the game is over
	if ( pRules->State_Get() == GR_STATE_GAME_OVER )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_TFPlayer::StartSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget )
{
	switch ( event->GetType() )
	{
	case CChoreoEvent::SEQUENCE:
	case CChoreoEvent::GESTURE:
		return StartGestureSceneEvent( info, scene, event, actor, pTarget );
	default:
		return BaseClass::StartSceneEvent( info, scene, event, actor, pTarget );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_TFPlayer::StartGestureSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget )
{
	// Get the (gesture) sequence.
	info->m_nSequence = LookupSequence( event->GetParameters() );
	if ( info->m_nSequence < 0 )
		return false;

	// Player the (gesture) sequence.
	if ( GetGameAnimStateType() )
		GetGameAnimStateType()->AddVCDSequenceToGestureSlot( GESTURE_SLOT_VCD, info->m_nSequence );

	return true;
}

bool C_TFPlayer::IsAllowedToSwitchWeapons( void )
{
	if ( IsWeaponLowered() == true )
		return false;

	// Can't weapon switch during a taunt.
	if( m_Shared.InCond( TF_COND_TAUNTING ) && tf_allow_taunt_switch.GetInt() <= 1 )
		return false;

	return BaseClass::IsAllowedToSwitchWeapons();
}

IMaterial *C_TFPlayer::GetHeadLabelMaterial( void )
{
	if ( g_pHeadLabelMaterial[0] == NULL )
		SetupHeadLabelMaterials();

	if ( GetTeamNumber() == TF_TEAM_RED )
	{
		return g_pHeadLabelMaterial[TF_PLAYER_HEAD_LABEL_RED];
	}
	else
	{
		return g_pHeadLabelMaterial[TF_PLAYER_HEAD_LABEL_BLUE];
	}

	return BaseClass::GetHeadLabelMaterial();
}

void SetupHeadLabelMaterials( void )
{
	for ( int i = 0; i < 2; i++ )
	{
		if ( g_pHeadLabelMaterial[i] )
		{
			g_pHeadLabelMaterial[i]->DecrementReferenceCount();
			g_pHeadLabelMaterial[i] = NULL;
		}

		g_pHeadLabelMaterial[i] = materials->FindMaterial( pszHeadLabelNames[i], TEXTURE_GROUP_VGUI );
		if ( g_pHeadLabelMaterial[i] )
		{
			g_pHeadLabelMaterial[i]->IncrementReferenceCount();
		}
	}
}

void C_TFPlayer::ComputeFxBlend( void )
{
	BaseClass::ComputeFxBlend();

	if ( GetPlayerClass()->IsClass( TF_CLASS_SPY ) )
	{
		float flInvisible = GetPercentInvisible();
		if ( flInvisible != 0.0f )
		{
			// Tell our shadow
			ClientShadowHandle_t hShadow = GetShadowHandle();
			if ( hShadow != CLIENTSHADOW_INVALID_HANDLE )
			{
				g_pClientShadowMgr->SetFalloffBias( hShadow, flInvisible * 255 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov )
{
	HandleTaunting();
	BaseClass::CalcView( eyeOrigin, eyeAngles, zNear, zFar, fov );
}

void SelectDisguise( int iClass, int iTeam );

static void cc_tf_player_lastdisguise( const CCommand &args )
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pPlayer == NULL )
		return;

	// disguise as our last known disguise. desired disguise will be initted to something sensible
	if ( pPlayer->CanDisguise() )
	{
		// disguise as the previous class, if one exists
		int nClass = pPlayer->m_Shared.GetDesiredDisguiseClass();

		int iLocalTeam = pPlayer->GetTeamNumber();
		int iEnemyTeam = ( iLocalTeam == TF_TEAM_BLUE ) ? TF_TEAM_RED : TF_TEAM_BLUE;
		int nTeam = pPlayer->m_Shared.WasLastDisguiseAsOwnTeam() ? iLocalTeam : iEnemyTeam; 

		//If we pass in "random" or whatever then just make it pick a random class.
		if ( args.ArgC() > 1 )
		{
			nClass = TF_CLASS_UNDEFINED;
		}

		if ( nClass == TF_CLASS_UNDEFINED )
		{
			// they haven't disguised yet, pick a nice one for them.
			// exclude some undesirable classes
			do
			{
				nClass = random->RandomInt( TF_FIRST_NORMAL_CLASS, TF_LAST_NORMAL_CLASS );
			} while( nClass == TF_CLASS_SCOUT || nClass == TF_CLASS_SPY );

			nTeam = iEnemyTeam;
		}

		SelectDisguise( nClass, nTeam );
		
	}

}
static ConCommand lastdisguise( "lastdisguise", cc_tf_player_lastdisguise, "Disguises the spy as the last disguise." );


static void cc_tf_player_disguise( const CCommand &args )
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pPlayer == NULL )
		return;

	if ( args.ArgC() >= 3 )
	{
		if ( pPlayer->CanDisguise() )
		{
			int nClass = atoi( args[ 1 ] );
			int nTeam = atoi( args[ 2 ] );

			//Disguise as enemy team
			if ( nTeam == -1 )
			{
				if ( pPlayer->GetTeamNumber() == TF_TEAM_BLUE )
				{
					nTeam = TF_TEAM_RED;
				}
				else
				{
					nTeam = TF_TEAM_BLUE;
				}
			}
			else if ( nTeam == -2 ) //disguise as my own team
			{
				nTeam = pPlayer->GetTeamNumber();
			}
			else
			{
				nTeam = ( nTeam == 1 ) ? TF_TEAM_BLUE : TF_TEAM_RED;
			}
			
			// intercepting the team value and reassigning what gets passed into Disguise()
			// because the team numbers in the client menu don't match the #define values for the teams
			SelectDisguise( nClass, nTeam );
		}
	}
}

static ConCommand disguise( "disguise", cc_tf_player_disguise, "Disguises the spy." );

static void cc_tf_crashclient()
{
	C_TFPlayer *pPlayer = NULL;
	pPlayer->ComputeFxBlend();
}
static ConCommand tf_crashclient( "tf_crashclient", cc_tf_crashclient, "Crashes this client for testing.", FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::ForceUpdateObjectHudState( void )
{
	m_bUpdateObjectHudState = true;
}

#include "c_obj_sentrygun.h"

static void cc_tf_debugsentrydmg()
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	pPlayer->UpdateIDTarget();
	int iTarget = pPlayer->GetIDTarget();
	if ( iTarget > 0 )
	{
		C_BaseEntity *pEnt = cl_entitylist->GetEnt( iTarget );

		C_ObjectSentrygun *pSentry = dynamic_cast< C_ObjectSentrygun * >( pEnt );

		if ( pSentry )
		{
			pSentry->DebugDamageParticles();
		}
	}
}
static ConCommand tf_debugsentrydamage( "tf_debugsentrydamage", cc_tf_debugsentrydmg, "", FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::GetTargetIDDataString( bool bIsDisguised, OUT_Z_BYTECAP(iMaxLenInBytes) wchar_t *sDataString, int iMaxLenInBytes, bool &bIsAmmoData )
{
	Assert( iMaxLenInBytes >= sizeof(sDataString[0]) );
	// Make sure the output string is always initialized to a null-terminated string,
	// since the conditions below are tricky.
	sDataString[0] = 0;

	if ( bIsDisguised )
	{
		if ( !IsEnemyPlayer() )
		{
			// The target is a disguised friendly spy.  They appear to the player with no disguise.  Add the disguise
			// team & class to the target ID element.
			bool bDisguisedAsEnemy = ( m_Shared.GetDisguiseTeam() != GetTeamNumber() );
			const wchar_t *wszAlignment = g_pVGuiLocalize->Find( bDisguisedAsEnemy ? "#TF_enemy" : "#TF_friendly" );

			int classindex = m_Shared.GetDisguiseClass();
			const wchar_t *wszClassName = g_pVGuiLocalize->Find( g_aPlayerClassNames[classindex] );

			// build a string with disguise information
			g_pVGuiLocalize->ConstructString( sDataString, iMaxLenInBytes, g_pVGuiLocalize->Find( "#TF_playerid_friendlyspy_disguise" ), 
				2, wszAlignment, wszClassName );
		}
		else if ( IsEnemyPlayer() && (m_Shared.GetDisguiseClass() == TF_CLASS_SPY) )
		{
			// The target is an enemy spy disguised as a friendly spy. Show a fake team & class ID element.
			int classindex = m_Shared.GetDisguiseMask();
			const wchar_t *wszClassName = g_pVGuiLocalize->Find( g_aPlayerClassNames[classindex] );
			const wchar_t *wszAlignment = g_pVGuiLocalize->Find( "#TF_enemy" );

			g_pVGuiLocalize->ConstructString( sDataString, iMaxLenInBytes, g_pVGuiLocalize->Find( "#TF_playerid_friendlyspy_disguise" ), 
				2, wszAlignment, wszClassName );
		}
	}

	if ( IsPlayerClass( TF_CLASS_MEDIC ) )
	{
		CTFWeaponBase *pMedigun = NULL;

		// Medics put their ubercharge & medigun type into the data string
		wchar_t wszChargeLevel[ 10 ];
		_snwprintf( wszChargeLevel, ARRAYSIZE(wszChargeLevel) - 1, L"%.0f", MedicGetChargeLevel( &pMedigun ) * 100 );
		wszChargeLevel[ ARRAYSIZE(wszChargeLevel)-1 ] = '\0';

		g_pVGuiLocalize->ConstructString( sDataString, iMaxLenInBytes, g_pVGuiLocalize->Find( "#TF_playerid_mediccharge" ), 1, wszChargeLevel );
	}
	else if ( bIsDisguised && (m_Shared.GetDisguiseClass() == TF_CLASS_MEDIC) && IsEnemyPlayer() )
	{
		// Show a fake charge level for a disguised enemy medic.
		g_pVGuiLocalize->ConstructString( sDataString, iMaxLenInBytes, g_pVGuiLocalize->Find( "#TF_playerid_mediccharge" ), 1, L"0" );
	}
	else
	{
		C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
		if ( pLocalPlayer && pLocalPlayer->IsPlayerClass( TF_CLASS_MEDIC ) )
		{
			CBaseCombatWeapon *pWeapon = GetActiveWeapon();
			if ( pWeapon )
			{
				// Check for weapon_blocks_healing
				int iBlockHealing = 0;
				if ( iBlockHealing )
				{
					g_pVGuiLocalize->ConstructString( sDataString, iMaxLenInBytes, g_pVGuiLocalize->Find( "#TF_playerid_noheal_unknown" ), 0 );
				}

				// Show target's clip state to attached medics
				if ( !sDataString[0] && m_nActiveWpnClip >= 0 )
				{
					C_TFPlayer *pTFHealTarget = ToTFPlayer( pLocalPlayer->MedicGetHealTarget() );
					if ( pTFHealTarget && pTFHealTarget == this )
					{
						wchar_t wszClip[10];
						V_snwprintf( wszClip, ARRAYSIZE(wszClip) - 1, L"%d", m_nActiveWpnClip );
						g_pVGuiLocalize->ConstructString( sDataString, iMaxLenInBytes, g_pVGuiLocalize->Find( "#TF_playerid_ammo" ), 1, wszClip );
						bIsAmmoData = true;
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::UpdateSpyStateChange( void )
{
	// SEALTODO
	//UpdateOverhealEffect();
	//UpdateRecentlyTeleportedEffect();

	if ( m_Shared.IsStealthed() )
	{
//		m_Shared.EndRadiusHealEffect();
	}

	// Force Weapon updates
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->PreDataUpdate( DATA_UPDATE_DATATABLE_CHANGED );
	}
}

//////////////////////////////// Day Of Defeat ////////////////////////////////
void C_TFPlayer::DoRecoil( int iWpnID, float flWpnRecoil )
{
	float flPitchRecoil = flWpnRecoil;
	float flYawRecoil = flPitchRecoil / 4;

	if( iWpnID == DOD_WEAPON_BAR )
		flYawRecoil = MIN( flYawRecoil, 1.3 );

	if ( m_Shared.IsInMGDeploy() )
	{
		flPitchRecoil = 0.0;
		flYawRecoil = 0.0;
	}
	else if ( m_Shared.IsProne() && 
		iWpnID != DOD_WEAPON_30CAL && 
		iWpnID != DOD_WEAPON_MG42 ) //minor hackage
	{
		flPitchRecoil = flPitchRecoil / 4;
		flYawRecoil = flYawRecoil / 4;
	}
	else if ( m_Shared.IsDucking() )
	{
		flPitchRecoil = flPitchRecoil / 2;
		flYawRecoil = flYawRecoil / 2;
	}

	SetRecoilAmount( flPitchRecoil, flYawRecoil );
}

//Set the amount of pitch and yaw recoil we want to do over the next RECOIL_DURATION seconds
void C_TFPlayer::SetRecoilAmount( float flPitchRecoil, float flYawRecoil )
{
	//Slam the values, abandon previous recoils
	m_flPitchRecoilAccumulator = flPitchRecoil;

	flYawRecoil = flYawRecoil * random->RandomFloat( 0.8, 1.1 );

	if( random->RandomInt( 0,1 ) <= 0 )
		m_flYawRecoilAccumulator = flYawRecoil;
	else
		m_flYawRecoilAccumulator = -flYawRecoil;

	m_flRecoilTimeRemaining = RECOIL_DURATION;
}

//Get the amount of recoil we should do this frame
void C_TFPlayer::GetRecoilToAddThisFrame( float &flPitchRecoil, float &flYawRecoil )
{
	if( m_flRecoilTimeRemaining <= 0 )
	{
		flPitchRecoil = 0.0;
		flYawRecoil = 0.0;
		return;
	}

	float flRemaining = MIN( m_flRecoilTimeRemaining, gpGlobals->frametime );

	float flRecoilProportion = ( flRemaining / RECOIL_DURATION );

	flPitchRecoil = m_flPitchRecoilAccumulator * flRecoilProportion;
	flYawRecoil = m_flYawRecoilAccumulator * flRecoilProportion;

	m_flRecoilTimeRemaining -= gpGlobals->frametime;
}

//////////////////////////////// Day Of Defeat ////////////////////////////////
#include "physpropclientside.h"

class C_FadingPhysPropClientside : public C_PhysPropClientside
{
public:	
	DECLARE_CLASS( C_FadingPhysPropClientside, C_PhysPropClientside );

	// if we wake, extend fade time

	virtual void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
	{
		// If we haven't started fading
		if( GetRenderColor().a >= 255 )
		{
			// delay the fade
			StartFadeOut( 10.0 );

			// register the impact
			BaseClass::ImpactTrace( pTrace, iDamageType, pCustomImpactName );
		}		
	}
};

void C_TFPlayer::PopHelmet( Vector vecDir, Vector vecForceOrigin, int iModel )
{
	if ( IsDormant() )
		return;	// We can't see them anyway, just bail

	C_FadingPhysPropClientside *pEntity = new C_FadingPhysPropClientside();

	if ( !pEntity )
		return;

	const model_t *model = modelinfo->GetModel( iModel );

	if ( !model )
	{
		DevMsg("CTempEnts::PhysicsProp: model index %i not found\n", iModel );
		return;
	}

	Vector vecHead;
	QAngle angHeadAngles;

	{
		C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false );
		int iAttachment = LookupAttachment( "head" );
		GetAttachment( iAttachment, vecHead, angHeadAngles );	//attachment 1 is the head attachment
	}

	pEntity->SetModelName( modelinfo->GetModelName(model) );
	pEntity->SetAbsOrigin( vecHead );
	pEntity->SetAbsAngles( angHeadAngles );
	pEntity->SetPhysicsMode( PHYSICS_MULTIPLAYER_CLIENTSIDE );

	if ( !pEntity->Initialize() )
	{
		pEntity->Release();
		return;
	}

	IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();

	if( pPhysicsObject )
	{
#ifdef DEBUG
		if( vecForceOrigin == vec3_origin )
		{
			vecForceOrigin = GetAbsOrigin();
		}
#endif

		Vector vecForce = vecDir;		
		Vector vecOffset = vecForceOrigin - pEntity->GetAbsOrigin();
		pPhysicsObject->ApplyForceOffset( vecForce, vecOffset );
	}
	else
	{
		// failed to create a physics object
		pEntity->Release();
		return;
	}

	pEntity->StartFadeOut( 10.0 );
}

void C_TFPlayer::ReceiveMessage( int classID, bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		// message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	int messageType = msg.ReadByte();
	switch( messageType )
	{
	case DOD_PLAYER_POP_HELMET:
		{
			Vector	vecDir, vecForceOffset;
			msg.ReadBitVec3Coord( vecDir );
			msg.ReadBitVec3Coord( vecForceOffset );

			int model = msg.ReadShort();

			PopHelmet( vecDir, vecForceOffset, model );
		}
		break;
	//case DOD_PLAYER_REMOVE_DECALS:
	//	{
	//		RemoveAllDecals();
	//	}
	//	break;
	default:
		break;
	}
}

bool C_TFPlayer::IsDODWeaponLowered( void )
{
	if ( GetMoveType() == MOVETYPE_LADDER )
		return true;

	CPCWeaponBase *pWeapon = GetActivePCWeapon();

	if ( !pWeapon )
		return false;

	// Lower when underwater ( except if its melee )
	if ( GetWaterLevel() > 2 && pWeapon->GetPCWpnData().m_DODWeaponData.m_WeaponType != WPN_TYPE_MELEE )
		return true;

	if ( m_Shared.IsProne() && GetAbsVelocity().LengthSqr() > 1 )
		return true;

	if ( m_Shared.IsGoingProne() || m_Shared.IsGettingUpFromProne() )
		return true;

	if ( m_Shared.IsJumping() )
		return true;

	//if ( m_Shared.IsDefusing() )
	//	return true;

	// Lower losing team's weapons in bonus round
	CTFGameRules* pRules = TFGameRules();

	// Lower losing team's weapons in bonus round
	if ( ( pRules->State_Get() == GR_STATE_TEAM_WIN ) && ( pRules->GetWinningTeam() != GetTeamNumber() ) )
		return true;

	// Hide all view models after the game is over
	if ( pRules->State_Get() == GR_STATE_GAME_OVER )
		return true;

	if ( m_Shared.IsBazookaDeployed() )
		return false;

	Vector vel = GetAbsVelocity();
	if ( vel.Length2D() < 50 )
		return false;

	if ( m_nButtons & IN_SPEED && ( m_nButtons & IN_FORWARD ) &&
		m_Shared.GetStamina() >= 5 &&
		!m_Shared.IsDucking() )
		return true;

	return m_bWeaponLowered;
}

// Start or stop the stamina breathing sound if necessary
void C_TFPlayer::StaminaSoundThink( void )
{
	if ( m_bPlayingLowStaminaSound )
	{
		if ( !IsAlive() || m_Shared.GetStamina() >= LOW_STAMINA_THRESHOLD )
		{
			// stop the sprint sound
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			controller.SoundFadeOut( m_pStaminaSound, 1.0, true );

			// SoundFadeOut will destroy this sound, so we will have to create another one
			// if we go below the threshold again soon
			m_pStaminaSound = NULL;

			m_bPlayingLowStaminaSound = false;
		}
	}
	else
	{
		if ( IsAlive() && m_Shared.GetStamina() < LOW_STAMINA_THRESHOLD )
		{
			// we are alive and have low stamina
			CLocalPlayerFilter filter;

			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

			if ( !m_pStaminaSound )
                m_pStaminaSound = controller.SoundCreate( filter, entindex(), "DODPlayer.Sprint" );

			controller.Play( m_pStaminaSound, 0.0, 100 );
			controller.SoundChangeVolume( m_pStaminaSound, 1.0, 2.0 );

			m_bPlayingLowStaminaSound = true;
		}
	}
}

//======================================================
//
// Cold Breath Emitter - for DOD players.
//
class ColdBreathEmitter : public CSimpleEmitter
{
public:
	
	ColdBreathEmitter( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}

	static ColdBreathEmitter *Create( const char *pDebugName )
	{
		return new ColdBreathEmitter( pDebugName );
	}

	void UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
	{
		// Float up when lifetime is half gone.
		pParticle->m_vecVelocity[2] -= ( 8.0f * timeDelta );


		// FIXME: optimize this....
		pParticle->m_vecVelocity *= ExponentialDecay( 0.9, 0.03, timeDelta );
	}

	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta )
	{
		pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
		
		pParticle->m_flRollDelta += pParticle->m_flRollDelta * ( timeDelta * -2.0f );

		//Cap the minimum roll
		if ( fabs( pParticle->m_flRollDelta ) < 0.5f )
		{
			pParticle->m_flRollDelta = ( pParticle->m_flRollDelta > 0.0f ) ? 0.5f : -0.5f;
		}

		return pParticle->m_flRoll;
	}

private:

	ColdBreathEmitter( const ColdBreathEmitter & );
};

// Cold breath defines.
#define COLDBREATH_EMIT_MIN				2.0f
#define COLDBREATH_EMIT_MAX				3.0f
#define COLDBREATH_EMIT_SCALE			0.35f
#define COLDBREATH_PARTICLE_LIFE_MIN	0.25f
#define COLDBREATH_PARTICLE_LIFE_MAX	1.0f
#define COLDBREATH_PARTICLE_LIFE_SCALE  0.75
#define COLDBREATH_PARTICLE_SIZE_MIN	1.0f
#define COLDBREATH_PARTICLE_SIZE_MAX	4.0f
#define COLDBREATH_PARTICLE_SIZE_SCALE	1.1f
#define COLDBREATH_PARTICLE_COUNT		1
#define COLDBREATH_DURATION_MIN			0.0f
#define COLDBREATH_DURATION_MAX			1.0f
#define COLDBREATH_ALPHA_MIN			0.0f
#define COLDBREATH_ALPHA_MAX			0.3f
#define COLDBREATH_ENDSCALE_MIN			0.1f
#define COLDBREATH_ENDSCALE_MAX			0.4f

static ConVar cl_coldbreath_forcestamina( "cl_coldbreath_forcestamina", "0", FCVAR_CHEAT );
static ConVar cl_coldbreath_enable( "cl_coldbreath_enable", "1" );

#include "c_world.h"

//-----------------------------------------------------------------------------
// Purpose: Create the emitter of cold breath particles
//-----------------------------------------------------------------------------
bool C_TFPlayer::CreateColdBreathEmitter( void )
{
	// Check to see if we are in a cold breath scenario.
	if ( !GetClientWorldEntity()->m_bColdWorld || !IsDODClass() )
		return false;

	// Set cold breath to true.
	m_bColdBreathOn = true;

	// Create a cold breath emitter if one doesn't already exist.
	if ( !m_hColdBreathEmitter )
	{
		m_hColdBreathEmitter = ColdBreathEmitter::Create( "ColdBreath" );
		if ( !m_hColdBreathEmitter )
			return false;

		// Get the particle material.
		m_hColdBreathMaterial = m_hColdBreathEmitter->GetPMaterial( "sprites/frostbreath" );
		Assert( m_hColdBreathMaterial != INVALID_MATERIAL_HANDLE );

		// Cache off the head attachment for setting up cold breath.
		m_iHeadAttach = LookupAttachment( "head" );		
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Destroy the cold breath emitter
//-----------------------------------------------------------------------------
void C_TFPlayer::DestroyColdBreathEmitter( void )
{
#if 0
	if ( m_hColdBreathEmitter.IsValid() )
	{
		UTIL_Remove( m_hColdBreathEmitter );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::UpdateColdBreath( void )
{
	if ( !cl_coldbreath_enable.GetBool() )
		return;

	// Check to see if the cold breath emitter has been created.
	if ( !m_hColdBreathEmitter.IsValid() )
	{
		if ( !CreateColdBreathEmitter() )
			return;
	}

	// Cold breath updates.
	if ( !m_bColdBreathOn )
		return;

	// Don't emit breath if we are dead.
	if ( !IsAlive() || IsDormant() )
		return;

	// Check player speed, do emit cold breath when moving quickly.
	float flSpeed = GetAbsVelocity().Length();
	if ( flSpeed > 60.0f )
		return;

	if ( m_flColdBreathTimeStart < gpGlobals->curtime )
	{
		// Spawn cold breath particles.
		EmitColdBreathParticles();

		// Update the timer.
		if ( m_flColdBreathTimeEnd < gpGlobals->curtime )
		{
			// Check stamina and modify the time accordingly.
			if ( m_Shared.m_flStamina < LOW_STAMINA_THRESHOLD || cl_coldbreath_forcestamina.GetBool() )
			{
				m_flColdBreathTimeStart = gpGlobals->curtime + RandomFloat( COLDBREATH_EMIT_MIN * COLDBREATH_EMIT_SCALE, COLDBREATH_EMIT_MAX * COLDBREATH_EMIT_SCALE );
				float flDuration = RandomFloat( COLDBREATH_DURATION_MIN, COLDBREATH_DURATION_MAX );
				m_flColdBreathTimeEnd = m_flColdBreathTimeStart + flDuration;
			}
			else
			{
				m_flColdBreathTimeStart = gpGlobals->curtime + RandomFloat( COLDBREATH_EMIT_MIN, COLDBREATH_EMIT_MAX );
				float flDuration = RandomFloat( COLDBREATH_DURATION_MIN, COLDBREATH_DURATION_MAX );
				m_flColdBreathTimeEnd = m_flColdBreathTimeStart + flDuration;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::EmitColdBreathParticles( void )
{
	// Get the position to emit from - look into caching this off we are doing redundant work in the case
	// of allies (see dod_headiconmanager.cpp).
	Vector vecOrigin; 
	QAngle vecAngle;
	GetAttachment( m_iHeadAttach, vecOrigin, vecAngle );
	Vector vecForward, vecRight, vecUp;
	AngleVectors( vecAngle, &vecUp, &vecForward, &vecRight );

	vecOrigin += ( vecForward * 8.0f );

	SimpleParticle *pParticle = static_cast<SimpleParticle*>( m_hColdBreathEmitter->AddParticle( sizeof( SimpleParticle ),m_hColdBreathMaterial, vecOrigin ) );
	if ( pParticle )
	{
		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime = RandomFloat( COLDBREATH_PARTICLE_LIFE_MIN, COLDBREATH_PARTICLE_LIFE_MAX );
		if ( m_Shared.m_flStamina < LOW_STAMINA_THRESHOLD || cl_coldbreath_forcestamina.GetBool() )
		{
			pParticle->m_flDieTime *= COLDBREATH_PARTICLE_LIFE_SCALE;
		}

		// Add just a little movement.
		if ( m_Shared.m_flStamina < LOW_STAMINA_THRESHOLD || cl_coldbreath_forcestamina.GetBool() )
		{
			pParticle->m_vecVelocity = ( vecForward * RandomFloat( 10.0f, 30.0f ) ) + ( vecRight * RandomFloat( -2.0f, 2.0f ) ) +
				                       ( vecUp * RandomFloat( 0.0f, 0.5f ) );
		}
		else
		{
			pParticle->m_vecVelocity = ( vecForward * RandomFloat( 10.0f, 20.0f ) ) + ( vecRight * RandomFloat( -2.0f, 2.0f ) ) +
				                       ( vecUp * RandomFloat( 0.0f, 1.5f ) );
		}
				
		pParticle->m_uchColor[0] = 200;
		pParticle->m_uchColor[1] = 200;
		pParticle->m_uchColor[2] = 210;
				
		float flParticleSize = RandomFloat( COLDBREATH_PARTICLE_SIZE_MIN, COLDBREATH_PARTICLE_SIZE_MAX );
		float flParticleScale = RandomFloat( COLDBREATH_ENDSCALE_MIN, COLDBREATH_ENDSCALE_MAX );
		if ( m_Shared.m_flStamina < LOW_STAMINA_THRESHOLD || cl_coldbreath_forcestamina.GetBool() )
		{
			pParticle->m_uchEndSize = flParticleSize * COLDBREATH_PARTICLE_SIZE_SCALE;
			flParticleScale *= COLDBREATH_PARTICLE_SIZE_SCALE;
		}
		else
		{
			pParticle->m_uchEndSize = flParticleSize;
		}
		pParticle->m_uchStartSize = ( flParticleSize * flParticleScale );
				
		float flAlpha = RandomFloat( COLDBREATH_ALPHA_MIN, COLDBREATH_ALPHA_MAX );
		pParticle->m_uchStartAlpha = flAlpha * 255; 
		pParticle->m_uchEndAlpha = 0;
				
		pParticle->m_flRoll	= RandomInt( 0, 360 );
		pParticle->m_flRollDelta = RandomFloat( 0.0f, 1.25f );
	}
}

ConVar cl_muzzleflash_dlight_3rd( "cl_muzzleflash_dlight_3rd", "1" );

void C_TFPlayer::ProcessMuzzleFlashEventDOD()
{
	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	bool bInToolRecordingMode = ToolsEnabled() && clienttools->IsInRecordingMode();

	// Reenable when the weapons have muzzle flash attachments in the right spot.
	if ( this == pLocalPlayer && !bInToolRecordingMode )
		return; // don't show own world muzzle flashs in for localplayer

	if ( pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		// also don't show in 1st person spec mode
		if ( pLocalPlayer->GetObserverTarget() == this )
			return;
	}

	CPCWeaponBase *pWeapon = GetActivePCWeapon();
	if ( !pWeapon )
		return;

	int nModelIndex = pWeapon->GetModelIndex();
	int nWorldModelIndex = pWeapon->GetWorldModelIndex();
	if ( bInToolRecordingMode && nModelIndex != nWorldModelIndex )
	{
		pWeapon->SetModelIndex( nWorldModelIndex );
	}

	Vector vecOrigin;
	QAngle angAngles;

	//MATTTODO - use string names of the weapon
	const static int iMuzzleFlashAttachment = 1;
	const static int iEjectBrassAttachment = 2;

	// If we have an attachment, then stick a light on it.
	if ( cl_muzzleflash_dlight_3rd.GetBool() && pWeapon->GetAttachment( iMuzzleFlashAttachment, vecOrigin, angAngles ) )
	{
		// Muzzleflash light
		dlight_t *el = effects->CL_AllocDlight( LIGHT_INDEX_MUZZLEFLASH );
		el->origin = vecOrigin;
		el->radius = 70; 

		if ( pWeapon->GetPCWpnData().m_DODWeaponData.m_WeaponType == WPN_TYPE_SNIPER )
			el->radius = 150;

		el->decay = el->radius / 0.05f;
		el->die = gpGlobals->curtime + 0.05f;
		el->color.r = 255;
		el->color.g = 192;
		el->color.b = 64;
		el->color.exponent = 5;

		if ( bInToolRecordingMode )
		{
			Color clr( el->color.r, el->color.g, el->color.b );

			KeyValues *msg = new KeyValues( "TempEntity" );

			msg->SetInt( "te", TE_DYNAMIC_LIGHT );
			msg->SetString( "name", "TE_DynamicLight" );
			msg->SetFloat( "time", gpGlobals->curtime );
			msg->SetFloat( "duration", el->die );
			msg->SetFloat( "originx", el->origin.x );
			msg->SetFloat( "originy", el->origin.y );
			msg->SetFloat( "originz", el->origin.z );
			msg->SetFloat( "radius", el->radius );
			msg->SetFloat( "decay", el->decay );
			msg->SetColor( "color", clr );
			msg->SetInt( "exponent", el->color.exponent );
			msg->SetInt( "lightindex", LIGHT_INDEX_MUZZLEFLASH );

			ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
			msg->deleteThis();
		}
	}

	const char *pszMuzzleFlashEffect = NULL;

	switch( pWeapon->GetPCWpnData().m_DODWeaponData.m_iMuzzleFlashType )
	{
	case DOD_MUZZLEFLASH_PISTOL:
		pszMuzzleFlashEffect = "muzzle_pistols";
		break;
	case DOD_MUZZLEFLASH_AUTO:
		pszMuzzleFlashEffect = "muzzle_fullyautomatic";
		break;
	case DOD_MUZZLEFLASH_RIFLE:
		pszMuzzleFlashEffect = "muzzle_rifles";
		break;
	case DOD_MUZZLEFLASH_ROCKET:
		pszMuzzleFlashEffect = "muzzle_rockets";
		break;
	case DOD_MUZZLEFLASH_MG42:
		pszMuzzleFlashEffect = "muzzle_mg42";
		break;
	default:
		break;
	}

	if ( pszMuzzleFlashEffect )
	{
		DispatchParticleEffect( pszMuzzleFlashEffect, PATTACH_POINT_FOLLOW, pWeapon, 1 );
	}

	CWeaponDODBase *pDODWeapon = dynamic_cast<CWeaponDODBase*>( pWeapon );
	if( pDODWeapon && pDODWeapon->ShouldAutoEjectBrass() )
	{
		// shell eject
		if( pWeapon->GetAttachment( iEjectBrassAttachment, vecOrigin, angAngles ) )
		{
			int shellType = pDODWeapon->GetEjectBrassShellType();

			CEffectData data;
			data.m_nHitBox = shellType;
			data.m_vOrigin = vecOrigin;
			data.m_vAngles = angAngles;
			DispatchEffect( "DOD_EjectBrass", data );
		}
	}

	if ( bInToolRecordingMode && nModelIndex != nWorldModelIndex )
	{
		pWeapon->SetModelIndex( nModelIndex );
	}
}

bool C_TFPlayer::InSameDisguisedTeam( CBaseEntity *pEnt )
{
	if ( pEnt == NULL )
		return false;

	int iMyApparentTeam = GetTeamNumber();

	if ( m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		iMyApparentTeam = m_Shared.GetDisguiseTeam();
	}

	C_TFPlayer *pPlayerEnt = ToTFPlayer( pEnt );
	int iTheirApparentTeam = pEnt->GetTeamNumber();
	if ( pPlayerEnt && pPlayerEnt->m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		iTheirApparentTeam = pPlayerEnt->m_Shared.GetDisguiseTeam();
	}

	return ( iMyApparentTeam == iTheirApparentTeam || GetTeamNumber() == pEnt->GetTeamNumber() || iTheirApparentTeam == GetTeamNumber() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_TFPlayer::CanUseFirstPersonCommand( void )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( pLocalPlayer )
	{
		if ( pLocalPlayer->m_Shared.InCond( TF_COND_PHASE ) || 
			 pLocalPlayer->m_Shared.InCond( TF_COND_TAUNTING ) || 
			 pLocalPlayer->m_Shared.IsControlStunned() ||
			 pLocalPlayer->m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) )
		{
			return false;
		}
	}

	return BaseClass::CanUseFirstPersonCommand();
}

//-----------------------------------------------------------------------------
// Purpose: Check if local player should see spy as disguised body
//-----------------------------------------------------------------------------
bool C_TFPlayer::ShouldDrawSpyAsDisguised()
{
	if ( C_BasePlayer::GetLocalPlayer() && m_Shared.InCond( TF_COND_DISGUISED ) && 
		( GetEnemyTeam( GetTeamNumber() ) == C_BasePlayer::GetLocalPlayer()->GetTeamNumber() ) )
	{
		if ( m_Shared.GetDisguiseClass() == TF_CLASS_SPY &&
			m_Shared.GetDisguiseTeam()  == C_BasePlayer::GetLocalPlayer()->GetTeamNumber() )
		{
			// This enemy is disguised as a friendly spy.
			// Show the spy's original bodygroups.
			return false;
		}
		else
		{
			// This enemy is disguised. Show the disguise body.
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_TFPlayer::GetBody( void )
{
	if ( ShouldDrawSpyAsDisguised() )
	{
		// This enemy is disguised. Show the disguise body.
		return m_Shared.GetDisguiseBody();
	}
	else
	{
		return BaseClass::GetBody();
	}
}

bool C_TFPlayer::IsEffectRateLimited( EBonusEffectFilter_t effect, const C_TFPlayer* pAttacker ) const
{
	// Check if we're rate limited
	switch( effect )
	{
		case kEffectFilter_AttackerOnly:
		case kEffectFilter_VictimOnly:
		case kEffectFilter_AttackerAndVictimOnly:
			return false;
		case kEffectFilter_AttackerTeam:
		case kEffectFilter_VictimTeam:
		case kEffectFilter_BothTeams:
			// Dont rate limit ourselves
			if( pAttacker == this )
				return false;

			return true;
		default:
			AssertMsg1( 0, "EBonusEffectFilter_t type not handled in %s", __FUNCTION__ );
			return false;
	}
}

bool C_TFPlayer::ShouldPlayEffect( EBonusEffectFilter_t filter, const C_TFPlayer* pAttacker, const C_TFPlayer* pVictim ) const
{
	Assert( pAttacker );
	Assert( pVictim );
	if( !pAttacker || !pVictim )
		return false;

	// Check if the right player relationship
	switch( filter )
	{
		case kEffectFilter_AttackerOnly:
			return ( pAttacker == this );
		case kEffectFilter_AttackerTeam:
			return ( pAttacker->GetTeamNumber() == this->GetTeamNumber() );
		case kEffectFilter_VictimOnly:
			return ( pVictim == this );
		case kEffectFilter_VictimTeam:
			return ( pVictim->GetTeamNumber() == this->GetTeamNumber() );
		case kEffectFilter_AttackerAndVictimOnly:
			return ( pAttacker == this || pVictim == this );
		case kEffectFilter_BothTeams:
			return ( pAttacker->GetTeamNumber() == this->GetTeamNumber() || pVictim->GetTeamNumber() == this->GetTeamNumber() );

		default:
			AssertMsg1( 0, "EBonusEffectFilter_t type not handled in %s", __FUNCTION__ );
			return false;
	};
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_TFPlayer::FireGameEvent( IGameEvent *event )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( FStrEq( event->GetName(), "player_hurt" ) )
	{
		static BonusEffect_t bonusEffects[] = 
		{
			//		       Sound name					Particle name		Particle filter				Sound filter				Sound plays in attacker's ears for them, the world for everyone else
			BonusEffect_t( "TFPlayer.CritHit",			"crit_text",		kEffectFilter_AttackerOnly, kEffectFilter_AttackerOnly,	true	),
			BonusEffect_t( "TFPlayer.CritHitMini",		"minicrit_text",	kEffectFilter_AttackerOnly, kEffectFilter_AttackerOnly,	true	)
		};
		COMPILE_TIME_ASSERT( ARRAYSIZE( bonusEffects ) == kBonusEffect_Count );

		// These only should affect the local player
		if ( !pLocalPlayer || pLocalPlayer != this )
			return;

		// By default we get kBonusEffect_None. We want to use whatever value we get here if it's not kBonusEffect_None.
		// If it's not, then check for crit or minicrit
		EAttackBonusEffects_t eBonusEffect = (EAttackBonusEffects_t)event->GetInt( "bonuseffect", (int)kBonusEffect_None );
		if( eBonusEffect == kBonusEffect_None )
		{
			// Keep reading for these fields to keep replays happy
			eBonusEffect = event->GetBool( "minicrit", false )	? kBonusEffect_MiniCrit : eBonusEffect;
			eBonusEffect = event->GetBool( "crit", false )		? kBonusEffect_Crit		: eBonusEffect;
		}

		// No effect to show?  Bail
		if( eBonusEffect == kBonusEffect_None || eBonusEffect >= kBonusEffect_Count )
			return;

		const int iAttacker = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
		C_TFPlayer *pAttacker = ToTFPlayer( UTIL_PlayerByIndex( iAttacker ) );

		const int iVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
		C_TFPlayer *pVictim = ToTFPlayer( UTIL_PlayerByIndex( iVictim ) );

		// No pointers to players?  Bail
		if( !pAttacker || !pVictim )
			return;

		bool bShowDisguisedCrit = event->GetBool( "showdisguisedcrit", 0 );

		// Victim is disguised and we're not showing disguised effects?  Bail
		if ( pVictim->m_Shared.InCond( TF_COND_DISGUISED ) && !bShowDisguisedCrit )
			return;

		// Victim is carrying Resist Powerup, which is immune to crit damage
		//if ( pVictim && pVictim->m_Shared.GetCarryingRuneType() == RUNE_RESIST &&
		//	 ( eBonusEffect == kBonusEffect_Crit || eBonusEffect == kBonusEffect_MiniCrit ) )
		//{
		//	return;
		//}

		// Support old system.  If "allseecrit" is set that means we want this to show for our whole team.
		EBonusEffectFilter_t eParticleFilter = bonusEffects[ eBonusEffect ].m_eParticleFilter;
		EBonusEffectFilter_t eSoundFilter = bonusEffects[ eBonusEffect ].m_eSoundFilter;
		if( event->GetBool( "allseecrit", false ) )
		{
			eParticleFilter = kEffectFilter_AttackerTeam;
			eSoundFilter	= kEffectFilter_AttackerTeam;
		}

		// Check if either of our effects are rate limited
		if( IsEffectRateLimited( eParticleFilter, pAttacker ) || IsEffectRateLimited( eSoundFilter, pAttacker ) )
		{
			// Check if we're cooling down
			if( !pVictim->CanDisplayAllSeeEffect( eBonusEffect ) )
			{
				// Too often!  Return
				return;
			}

			// Set cooldown period
			pVictim->SetNextAllSeeEffectTime( eBonusEffect, gpGlobals->curtime + 0.5f );
		}
		
		ConVarRef hud_combattext( "hud_combattext", false );
		ConVarRef hud_combattext_doesnt_block_overhead_text( "hud_combattext_doesnt_block_overhead_text", false );
		bool bCombatTextBlocks = hud_combattext.GetBool() && !hud_combattext_doesnt_block_overhead_text.GetBool();

		// Show the effect, unless combat text blocks
		if( ShouldPlayEffect( eParticleFilter, pAttacker, pVictim ) && !bCombatTextBlocks )
		{
			pVictim->ParticleProp()->Create( bonusEffects[ eBonusEffect ].m_pszParticleName, PATTACH_POINT_FOLLOW, "head" );
		}
		// Play the sound!
		if( ShouldPlayEffect( eSoundFilter, pAttacker, pVictim ) && bonusEffects[ eBonusEffect ].m_pszSoundName != NULL )
		{
			const bool& bPlayInAttackersEars = bonusEffects[ eBonusEffect ].m_bPlaySoundInAttackersEars;

			// sound effects
			EmitSound_t params;
			params.m_flSoundTime = 0;
			params.m_pflSoundDuration = 0;
			params.m_pSoundName = bonusEffects[ eBonusEffect ].m_pszSoundName;

			CPASFilter filter( pVictim->GetAbsOrigin() );
			if( bPlayInAttackersEars && pAttacker == this )
			{
				// Don't let the attacker hear this version if its to be played in their ears
				filter.RemoveRecipient( pAttacker );

				// Play a sound in the ears of the attacker
				CSingleUserRecipientFilter attackerFilter( pAttacker );
				EmitSound( attackerFilter, pAttacker->entindex(), params );
			}

			EmitSound( filter, pVictim->entindex(), params );
		}
	}

	BaseClass::FireGameEvent( event );
}

void C_TFPlayer::UpdateGlowEffect( void )
{
	DestroyGlowEffect();

	BaseClass::UpdateGlowEffect();

	// create a power up effect if needed
	if ( ShouldShowPowerupGlowEffect() )
	{
		float r, g, b;
		GetPowerupGlowEffectColor( &r, &g, &b );

		m_pPowerupGlowEffect = new CGlowObject( this, Vector( r, g, b ), 1.0, true );
	}
}

void C_TFPlayer::DestroyGlowEffect( void )
{
	BaseClass::DestroyGlowEffect();

	if ( m_pPowerupGlowEffect )
	{
		delete m_pPowerupGlowEffect;
		m_pPowerupGlowEffect = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::UpdateGlowColor( void )
{
	CGlowObject* pGlowObject = GetGlowObject();
	if ( pGlowObject )
	{
		float r, g, b;
		GetGlowEffectColor( &r, &g, &b );

		pGlowObject->SetColor( Vector( r, g, b ) );
	}

	if ( m_pPowerupGlowEffect )
	{
		float r, g, b;
		GetPowerupGlowEffectColor( &r, &g, &b );

		m_pPowerupGlowEffect->SetColor( Vector( r, g, b ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::GetGlowEffectColor( float *r, float *g, float *b )
{
	int nTeam = GetTeamNumber();

	C_TFPlayer *pLocalPlayer = GetLocalTFPlayer();
	// In CTF, show health color glow for alive player
	if ( pLocalPlayer && pLocalPlayer->IsAlive() && TFGameRules() && ( TFGameRules()->GetGameType() == TF_GAMETYPE_CTF ) && HasTheFlag() )
	{
		float flHealth = (float)GetHealth() / (float)GetMaxHealth();

		if ( flHealth > 0.6 )
		{
			*r = 0.33f;
			*g = 0.75f;
			*b = 0.23f;
		}
		else if( flHealth > 0.3 )
		{
			*r = 0.75f;
			*g = 0.72f;
			*b = 0.23f;
		}
		else
		{
			*r = 0.75f;
			*g = 0.23f;
			*b = 0.23f;
		}
		return;
	}

	if ( !engine->IsHLTV() && ( GetLocalPlayerTeam() >= FIRST_GAME_TEAM ) )
	{
		if ( IsPlayerClass( TF_CLASS_SPY ) && m_Shared.InCond( TF_COND_DISGUISED ) && ( GetTeamNumber() != GetLocalPlayerTeam() ) )
		{
			nTeam = m_Shared.GetDisguiseTeam();
		}
	}

	TFGameRules()->GetTeamGlowColor( nTeam, *r, *g, *b );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_TFPlayer::ShouldShowPowerupGlowEffect()
{
	// should local player see enemy glow with powerup related
	// SEALTODO
	//C_TFPlayer *pLocalPlayer = GetLocalTFPlayer();
	//if ( pLocalPlayer->IsAlive() && this != pLocalPlayer && GetTeamNumber() != pLocalPlayer->GetTeamNumber() )
	//{
	//	// give advantage to local player who doesn't have rune to fight against enemy with rune by glowing their health
	//	if ( m_Shared.IsCarryingRune() && !pLocalPlayer->m_Shared.IsCarryingRune() )
	//	{
	//		// only show glow when the enemy is lower than 30% HP
	//		float flHealth = ( float )GetHealth() / ( float )GetMaxHealth();
	//		return flHealth <= 0.3 && pLocalPlayer->IsLineOfSightClear( this, IGNORE_ACTORS );
	//	}
	//	// local player with supernova can see enemy glow within supernova range
	//	else if ( pLocalPlayer->m_Shared.GetCarryingRuneType() == RUNE_SUPERNOVA && pLocalPlayer->m_Shared.IsRuneCharged() && !m_Shared.IsStealthed() )
	//	{
	//		const float flEffectRadiusSqr = Sqr( 1500.f );
	//		Vector toPlayer = WorldSpaceCenter() - pLocalPlayer->WorldSpaceCenter();
	//		return toPlayer.LengthSqr() <= flEffectRadiusSqr && pLocalPlayer->IsLineOfSightClear( this, IGNORE_ACTORS );
	//	}
	//}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFPlayer::GetPowerupGlowEffectColor( float *r, float *g, float *b )
{
	// SEALTODO
	//C_TFPlayer *pLocalPlayer = GetLocalTFPlayer();
	// no need to add extra logics here. we already know that other players are glowing from SUPERNOVA
	//if ( pLocalPlayer->m_Shared.GetCarryingRuneType() == RUNE_SUPERNOVA )
	//{
	//	*r = 255;
	//	*g = 255;
	//	*b = 0;
	//}
	//else
	{
		GetGlowEffectColor( r, g, b );
	}
}