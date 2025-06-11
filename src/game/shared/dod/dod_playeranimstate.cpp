//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_playeranimstate.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"
#include "dod_shareddefs.h"

#ifdef CLIENT_DLL
	#include "c_tf_player.h"
	#include "engine/ivdebugoverlay.h"
	#include "filesystem.h"
#else
	#include "tf_player.h"
#endif

ConVar dod_bodyheightoffset( "dod_bodyheightoffset", "4", FCVAR_CHEAT | FCVAR_REPLICATED, "Deploy height offset." );

#define ANIMPART_STAND "stand"
#define ANIMPART_PRONE "prone"
#define ANIMPART_CROUCH "crouch"
#define ANIMPART_SPRINT "sprint"
#define ANIMPART_SANDBAG "sandbag"
#define ANIMPART_BIPOD "bipod"

// When moving this fast, he plays run anim.
#define ARBITRARY_RUN_SPEED		300.0f
#define DOD_BODYYAW_RATE		720.0f

#define DOD_WALK_SPEED		60.0f
#define DOD_RUN_SPEED		120.0f
#define DOD_SPRINT_SPEED	260.0f

#define MOVING_MINIMUM_SPEED 0.5f

CDODPlayerAnimState* CreateDODPlayerAnimState( CTFPlayer *pPlayer )
{
	// Setup the movement data.
	MultiPlayerMovementData_t movementData;
	movementData.m_flBodyYawRate = DOD_BODYYAW_RATE;
	movementData.m_flRunSpeed = DOD_RUN_SPEED;
	movementData.m_flWalkSpeed = DOD_WALK_SPEED;
	movementData.m_flSprintSpeed = DOD_SPRINT_SPEED;

	// Create animation state for this player.
	CDODPlayerAnimState *pState = new CDODPlayerAnimState( pPlayer, movementData );

	pState->InitDOD( pPlayer );

	return pState;
}


// -------------------------------------------------------------------------------- //
// CDODPlayerAnimState implementation.
// -------------------------------------------------------------------------------- //

CDODPlayerAnimState::CDODPlayerAnimState()
{
	m_bGettingDown = false;
	m_bGettingUp = false;
	m_bWasGoingProne = false;
	m_bWasGettingUp = false;

	m_pOuterDOD = NULL;

	m_bPoseParameterInit = false;
	m_PoseParameterData.m_flEstimateYaw = 0.0f;
	m_flLastBodyHeight = 0.0f;
	m_PoseParameterData.m_flLastAimTurnTime = 0.0f;
	m_vecLastMoveYaw.Init();

	m_PoseParameterData.m_iMoveX = 0;
	m_PoseParameterData.m_iMoveY = 0;
	m_PoseParameterData.m_iAimYaw = 0;
	m_PoseParameterData.m_iAimPitch = 0;
	m_PoseParameterData.m_iBodyHeight = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			&movementData - 
//-----------------------------------------------------------------------------
CDODPlayerAnimState::CDODPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData )
: CMultiPlayerAnimState( pPlayer, movementData )
{
	m_pOuterDOD = NULL;

	// Don't initialize TF specific variables here. Init them in InitTF()
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CDODPlayerAnimState::~CDODPlayerAnimState()
{
}

void CDODPlayerAnimState::InitDOD( CTFPlayer *pPlayer )
{
	m_pOuterDOD = pPlayer;
	m_LegAnimType = LEGANIM_GOLDSRC;

	// Seal: the player was proning when spawning idk why
	m_bWasGoingProne = false;
	m_bWasGettingUp = false;
	m_bGettingDown = false;
}


void CDODPlayerAnimState::ClearAnimationState()
{
	m_bJumping = false;
	m_flFireIdleTime = 0;
	m_bLastDeployState = false;
	m_iLastWeaponID = TF_WEAPON_NONE;
	CancelGestures();
	BaseClass::ClearAnimationState();
}

void CDODPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	if ( event == PLAYERANIMEVENT_FIRE_GUN )
	{
		RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_RANGE_ATTACK1, false );

		if( GetOuterDOD()->m_Shared.IsBazookaDeployed() )
		{
			m_flFireIdleTime = gpGlobals->curtime + 0.1;	// don't hold this pose after firing
		}
		else
		{
			// hold last frame of fire pose for 2 seconds ( if we are moving )
			m_flFireIdleTime = gpGlobals->curtime + 2;	
		}
	}
	if ( event == PLAYERANIMEVENT_SECONDARY_ATTACK )
	{
		CancelGestures();
		RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_RANGE_ATTACK2 );
	}
	else if ( event == PLAYERANIMEVENT_RELOAD )
	{
		CancelGestures();
		RestartGesture( GESTURE_SLOT_GRENADE, ACT_RELOAD );
	}
	else if ( event == PLAYERANIMEVENT_THROW_GRENADE )
	{
		CancelGestures();
		RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_RANGE_ATTACK1 );
	}
	else if ( event == PLAYERANIMEVENT_ROLL_GRENADE )
	{
		CancelGestures();
		RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_RANGE_ATTACK2 );
	}
	else if ( event == PLAYERANIMEVENT_JUMP )
	{
		// Play the jump animation.
		m_bJumping = true;
		m_bFirstJumpFrame = true;
		RestartMainSequence();
		m_flJumpStartTime = gpGlobals->curtime;
	}
	else if ( event == PLAYERANIMEVENT_HANDSIGNAL )
	{
		CTFPlayer *pPlayer = GetOuterDOD();
		if ( pPlayer && !( pPlayer->m_Shared.IsBazookaDeployed() || pPlayer->m_Shared.IsProne() || pPlayer->m_Shared.IsProneDeployed() || pPlayer->m_Shared.IsSniperZoomed() || pPlayer->m_Shared.IsSandbagDeployed() ) )
		{
			CancelGestures();
			RestartGesture( GESTURE_SLOT_SWIM, ACT_DOD_HS_IDLE );
		}
	}
	else if ( event == PLAYERANIMEVENT_PLANT_TNT )
	{
		CancelGestures();
		RestartGesture( GESTURE_SLOT_FLINCH, ACT_DOD_PLANT_TNT );
	}
	else if ( event == PLAYERANIMEVENT_DEFUSE_TNT )
	{
		CancelGestures();
		RestartGesture( GESTURE_SLOT_VCD, ACT_DOD_DEFUSE_TNT );
	}
}


void CDODPlayerAnimState::RestartMainSequence()
{
	CancelGestures();

	BaseClass::RestartMainSequence();
}

bool CDODPlayerAnimState::HandleJumping( Activity *idealActivity )
{
	if ( m_bJumping )
	{
		if ( m_bFirstJumpFrame )
		{
			m_bFirstJumpFrame = false;
			RestartMainSequence();	// Reset the animation.
		}

		// Don't check if he's on the ground for a sec.. sometimes the client still has the
		// on-ground flag set right when the message comes in.
		if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f )
		{
			if ( GetBasePlayer()->GetFlags() & FL_ONGROUND )
			{
				m_bJumping = false;
				RestartMainSequence();
			}
		}
	}
	if ( m_bJumping )
	{
		*idealActivity = ACT_HOP;
		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle the prone up animation.
//-----------------------------------------------------------------------------
bool CDODPlayerAnimState::HandleProneDown( CTFPlayer *pPlayer, Activity *idealActivity )
{
	if ( ( pPlayer->GetCycle() > 0.99f ) || ( pPlayer->m_Shared.IsProne() ) )
	{
		*idealActivity = ACT_PRONE_IDLE;
		if ( GetOuterXYSpeed() > MOVING_MINIMUM_SPEED )
		{
			*idealActivity = ACT_PRONE_FORWARD;
		}
		RestartMainSequence();

		m_bGettingDown = false;
	}
	else
	{
		*idealActivity = ACT_GET_DOWN_STAND;
		if ( pPlayer->GetFlags() & FL_DUCKING )
		{
			*idealActivity = ACT_GET_DOWN_CROUCH;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handle the prone up animation.
//-----------------------------------------------------------------------------
bool CDODPlayerAnimState::HandleProneUp( CTFPlayer *pPlayer, Activity *idealActivity )
{
	if ( ( GetBasePlayer()->GetCycle() > 0.99f) || (!pPlayer->m_Shared.IsGettingUpFromProne()))
	{
		m_bGettingUp = false;
		RestartMainSequence();

		return false;
	}

	*idealActivity = ACT_GET_UP_STAND;
	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		*idealActivity = ACT_GET_UP_CROUCH;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handle the prone animations.
//-----------------------------------------------------------------------------
bool CDODPlayerAnimState::HandleProne( Activity *idealActivity )
{
	// Get the player.
	CTFPlayer *pPlayer = GetOuterDOD();
	if ( !pPlayer )
		return false;

	// Find the leading edge on going prone.
	bool bChange = pPlayer->m_Shared.IsGoingProne() && !m_bWasGoingProne;
	m_bWasGoingProne = pPlayer->m_Shared.IsGoingProne();
	if ( bChange )
	{
		m_bGettingDown = true;
		RestartMainSequence();
	}

	// Find the leading edge on getting up.
	bChange = pPlayer->m_Shared.IsGettingUpFromProne() && !m_bWasGettingUp;
	m_bWasGettingUp = pPlayer->m_Shared.IsGettingUpFromProne();
	if ( bChange )
	{
		m_bGettingUp = true;
		RestartMainSequence();
	}

	// Handle the transitions.
	if ( m_bGettingDown )
	{
		return HandleProneDown( pPlayer, idealActivity );
	}
	else if ( m_bGettingUp )
	{
		return HandleProneUp( pPlayer, idealActivity );
	}

	// Handle the prone state.
	if ( pPlayer->m_Shared.IsProne() )
	{
		*idealActivity = ACT_PRONE_IDLE;
		if ( GetOuterXYSpeed() > MOVING_MINIMUM_SPEED )
		{
			*idealActivity = ACT_PRONE_FORWARD;
		}

		return true;
	}

	return false;
}

bool CDODPlayerAnimState::HandleDucked( Activity *idealActivity )
{
	if ( GetBasePlayer()->GetFlags() & FL_DUCKING )
	{
		if ( GetOuterXYSpeed() > MOVING_MINIMUM_SPEED )
			*idealActivity = ACT_RUN_CROUCH;
		else
			*idealActivity = ACT_CROUCHIDLE;
		
		return true;
	}
	else
	{
		return false;
	}
}

Activity CDODPlayerAnimState::CalcMainActivity()
{
	Activity idealActivity = ACT_IDLE;

	float flSpeed = GetOuterXYSpeed();

	if ( HandleJumping( &idealActivity ) || 
		HandleProne( &idealActivity ) ||
		HandleDucked( &idealActivity ) )
	{
		// intentionally blank
	}
	else
	{
		if ( flSpeed > MOVING_MINIMUM_SPEED )
		{
			if( flSpeed >= DOD_SPRINT_SPEED )
			{
				idealActivity = ACT_SPRINT;
				
				// If we sprint, cancel the fire idle time
				CancelGestures();
			}
			else if( flSpeed >= DOD_WALK_SPEED )
				idealActivity = ACT_RUN;
			else
				idealActivity = ACT_WALK;
		}
	}

	// Shouldn't be here but we need to ship - bazooka deployed reload/running check.
	if ( IsGestureSlotActive( GESTURE_SLOT_GRENADE ) )
	{
		if ( flSpeed >= DOD_RUN_SPEED && m_pOuterDOD->m_Shared.IsBazookaOnlyDeployed() )
		{
			CancelGestures();
		}
	}

	return idealActivity;
}

void CDODPlayerAnimState::CancelGestures( void )
{
//	m_bPlayingGesture = false;
//	m_iGestureType = GESTURE_NONE;

#ifdef CLIENT_DLL	
//	m_iGestureSequence = -1;
#else
	GetBasePlayer()->RemoveAllGestures();
#endif 
}

//void CDODPlayerAnimState::RestartGesture( int iGestureType, Activity act, bool bAutoKill /* = true */ )
//{
//	Activity idealActivity = TranslateActivity( act );
//	m_bPlayingGesture = true;
//	m_iGestureType = iGestureType;
//
//#ifdef CLIENT_DLL
//	m_iGestureSequence = GetBasePlayer()->SelectWeightedSequence( idealActivity );
//
//	if( m_iGestureSequence == -1 )
//	{
//		m_bPlayingGesture = false;
//	}
//
//	m_flGestureCycle = 0.0f;
//	m_bAutokillGesture = bAutoKill;
//#else
//	m_pOuterDOD->RestartGesture( idealActivity, true, bAutoKill );
//#endif
//}

Activity CDODPlayerAnimState::TranslateActivity( Activity actDesired )
{
	Activity idealActivity = actDesired;

	if ( m_pOuterDOD->m_Shared.IsSandbagDeployed() )
	{
		switch( idealActivity )
		{
		case ACT_IDLE:
			idealActivity = ACT_DOD_DEPLOYED;
			break;
		case ACT_RANGE_ATTACK1:
			idealActivity = ACT_DOD_PRIMARYATTACK_DEPLOYED;
			break;
		case ACT_RELOAD:
			idealActivity = ACT_DOD_RELOAD_DEPLOYED;
			break;
		default:
			break;
		}
	} 
	else if ( m_pOuterDOD->m_Shared.IsProneDeployed() )
	{
		switch( idealActivity )
		{
		case ACT_PRONE_IDLE:
			idealActivity = ACT_DOD_PRONE_DEPLOYED;
			break;
		case ACT_RANGE_ATTACK1:
			idealActivity = ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED;
			break;
		case ACT_RELOAD:
			idealActivity = ACT_DOD_RELOAD_PRONE_DEPLOYED;
			break;
		default:
			break;
		}
	} 
	else if ( m_pOuterDOD->m_Shared.IsSniperZoomed() || m_pOuterDOD->m_Shared.IsBazookaDeployed() )
	{
		switch( idealActivity )
		{
		case ACT_IDLE:
			idealActivity = ACT_DOD_IDLE_ZOOMED;
			break;
		case ACT_WALK:
			idealActivity = ACT_DOD_WALK_ZOOMED;
			break;
		case ACT_CROUCHIDLE:
			idealActivity = ACT_DOD_CROUCH_ZOOMED;
			break;
		case ACT_RUN_CROUCH:
			idealActivity = ACT_DOD_CROUCHWALK_ZOOMED;
			break;
		case ACT_PRONE_IDLE:
			idealActivity = ACT_DOD_PRONE_ZOOMED;
			break;
		case ACT_PRONE_FORWARD:
			idealActivity = ACT_DOD_PRONE_FORWARD_ZOOMED;
			break;
		case ACT_RANGE_ATTACK1:
			if ( m_pOuterDOD->m_Shared.IsSniperZoomed() )
			{
				if( m_pOuterDOD->m_Shared.IsProne() )
					idealActivity = ACT_DOD_PRIMARYATTACK_PRONE;
			}
			break;
		case ACT_RELOAD:
			if ( m_pOuterDOD->m_Shared.IsBazookaDeployed() )
			{
				if( m_pOuterDOD->m_Shared.IsProne() )
				{
					idealActivity = ACT_DOD_RELOAD_PRONE_DEPLOYED;
				}
				else
				{
					idealActivity = ACT_DOD_RELOAD_DEPLOYED;
				}
			}
			break;
		default:
			break;
		}
	}
	else if ( m_pOuterDOD->m_Shared.IsProne() )
	{
		// translate prone shooting, reload, handsignal

		switch( idealActivity )
		{
		case ACT_RANGE_ATTACK1:
			idealActivity = ACT_DOD_PRIMARYATTACK_PRONE;
			break;
		case ACT_RANGE_ATTACK2:
			idealActivity = ACT_DOD_SECONDARYATTACK_PRONE;
			break;
		case ACT_RELOAD:			
			idealActivity = ACT_DOD_RELOAD_PRONE;
			break;
		default:
			break;
		}
	}
	else if ( GetBasePlayer()->GetFlags() & FL_DUCKING )
	{
		switch( idealActivity )
		{
		case ACT_RANGE_ATTACK1:
			idealActivity = ACT_DOD_PRIMARYATTACK_CROUCH;
			break;
		case ACT_RANGE_ATTACK2:
			idealActivity = ACT_DOD_SECONDARYATTACK_CROUCH;
			break;
		case ACT_DOD_HS_IDLE:
			idealActivity = ACT_DOD_HS_CROUCH;
			break;
		}
	}

	// Are our guns at fire or rest?
	if ( m_flFireIdleTime > gpGlobals->curtime )
	{
		switch( idealActivity )
		{
		case ACT_IDLE:			idealActivity = ACT_DOD_STAND_AIM; break;
		case ACT_CROUCHIDLE:	idealActivity = ACT_DOD_CROUCH_AIM; break;
		case ACT_RUN_CROUCH:	idealActivity = ACT_DOD_CROUCHWALK_AIM; break;
		case ACT_WALK:			idealActivity = ACT_DOD_WALK_AIM; break;
		case ACT_RUN:			idealActivity = ACT_DOD_RUN_AIM; break;
		default: break;
		}
	}
	else
	{
		switch( idealActivity )
		{
		case ACT_IDLE:			idealActivity = ACT_DOD_STAND_IDLE; break;
		case ACT_CROUCHIDLE:	idealActivity = ACT_DOD_CROUCH_IDLE; break;
		case ACT_RUN_CROUCH:	idealActivity = ACT_DOD_CROUCHWALK_IDLE; break;
		case ACT_WALK:			idealActivity = ACT_DOD_WALK_IDLE; break;
		case ACT_RUN:			idealActivity = ACT_DOD_RUN_IDLE; break;
		default: break;
		}
	}

	return m_pOuterDOD->TranslateActivity( idealActivity );
}

CTFPlayer* CDODPlayerAnimState::GetOuterDOD() const
{
	return m_pOuterDOD;
}

float CDODPlayerAnimState::GetCurrentMaxGroundSpeed()
{
	return PLAYER_SPEED_SPRINT; 
}

float CDODPlayerAnimState::CalcMovementPlaybackRate( bool *bIsMoving )
{
	if( ( GetCurrentMainSequenceActivity() == ACT_GET_UP_STAND ) || ( GetCurrentMainSequenceActivity() == ACT_GET_DOWN_STAND ) ||
		( GetCurrentMainSequenceActivity() == ACT_GET_UP_CROUCH ) || ( GetCurrentMainSequenceActivity() == ACT_GET_DOWN_CROUCH ) )
	{
		// We don't want to change the playback speed of these, even if we move.
		*bIsMoving = false;
		return 1.0;
	}

	// it would be a good idea to ramp up from 0.5 to 1.0 as they go from stop to moveing, it looks more natural.
	*bIsMoving = true;
	return 1.0;
}

void CDODPlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
	//if ( !pStudioHdr )
	//	return;

	// Reset some things if we're changed weapons
	// do this before ComputeSequences
	CPCWeaponBase *pWeapon = GetOuterDOD()->GetActivePCWeapon();
	if ( pWeapon )
	{	
		if( pWeapon->GetWeaponID() != m_iLastWeaponID )
		{
			CancelGestures();
			m_iLastWeaponID = pWeapon->GetWeaponID();
			m_flFireIdleTime = 0;
		}
	}

	BaseClass::ComputeSequences( pStudioHdr );

	if( !m_bGettingDown && !m_bGettingUp )
	{
		ComputeFireSequence();
	
#ifdef CLIENT_DLL

		ComputeGestureSequence( pStudioHdr );

		// get the weapon's swap criteria ( reload? attack? deployed? deployed reload? )
		// and determine whether we should use alt model or not

		CPCWeaponBase *pWeapon = GetOuterDOD()->GetActivePCWeapon();
		if ( pWeapon )
		{
			int iCurrentState = ALTWPN_CRITERIA_NONE;

			if( IsGestureSlotActive( GESTURE_SLOT_ATTACK_AND_RELOAD ) )
				iCurrentState |= ALTWPN_CRITERIA_FIRING;

			else if( IsGestureSlotActive( GESTURE_SLOT_GRENADE ) )
				iCurrentState |= ALTWPN_CRITERIA_RELOADING;

			if( m_pOuterDOD->m_Shared.IsProne() )
				iCurrentState |= ALTWPN_CRITERIA_PRONE;

			CWeaponDODBase *pDODWeapon = dynamic_cast< CWeaponDODBase * >( pWeapon );
			if ( pDODWeapon )
			{
				// SEALTODO: this is causing crashing
				// always use default model while proning or hand signal
				//if( !IsGestureSlotActive( GESTURE_SLOT_SWIM ) &&
				//	!IsGestureSlotActive( GESTURE_SLOT_JUMP ) &&
				//	!m_bGettingDown &&
				//	!m_bGettingUp )
				//{
				//	pWeapon->CheckForAltWeapon( iCurrentState );
				//}
				//else
				{
					pDODWeapon->SetUseAltModel( false );
				}
			}
		}
#endif
	}
}

//#define AIMSEQUENCE_LAYER		1	// Aim sequence uses layers 0 and 1 for the weapon idle animation (needs 2 layers so it can blend).
//#define GESTURE_LAYER			AIMSEQUENCE_LAYER
//#define NUM_LAYERS_WANTED		(GESTURE_LAYER + 1)

//void CDODPlayerAnimState::ClearAnimationLayers()
//{
//	if ( !GetBasePlayer() )
//		return;
//
//	GetBasePlayer()->SetNumAnimOverlays( NUM_LAYERS_WANTED );
//	for ( int i=0; i < GetBasePlayer()->GetNumAnimOverlays(); i++ )
//	{
//		GetBasePlayer()->GetAnimOverlay( i )->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
//	}
//}

void CDODPlayerAnimState::ComputeFireSequence( void )
{
	// Hold the shoot pose for a time after firing, unless we stand still
	if( m_flFireIdleTime < gpGlobals->curtime &&
		IsGestureSlotActive( GESTURE_SLOT_ATTACK_AND_RELOAD ) &&
		GetOuterXYSpeed() > MOVING_MINIMUM_SPEED )
	{
		CancelGestures();
	}

	if( GetOuterDOD()->m_Shared.IsInMGDeploy() != m_bLastDeployState )
	{
		CancelGestures();

		m_bLastDeployState = GetOuterDOD()->m_Shared.IsInMGDeploy();
	}
}

//void CDODPlayerAnimState::ComputeGestureSequence( CStudioHdr *pStudioHdr )
//{
//	UpdateLayerSequenceGeneric( pStudioHdr, GESTURE_LAYER, m_bPlayingGesture, m_flGestureCycle, m_iGestureSequence, !m_bAutokillGesture );
//}

void CDODPlayerAnimState::UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd, float flWeight /* = 1.0 */ )
{
	if ( !bEnabled )
		return;

	if( flCurCycle > 1.0 )
		flCurCycle = 1.0;

	// Increment the fire sequence's cycle.
	flCurCycle += GetBasePlayer()->GetSequenceCycleRate( pStudioHdr, iSequence ) * gpGlobals->frametime;
	if ( flCurCycle > 1 )
	{
		if ( bWaitAtEnd )
		{
			flCurCycle = 1;
		}
		else
		{
			// Not firing anymore.
			bEnabled = false;
			iSequence = 0;
			return;
		}
	}

	CAnimationLayer *pLayer = GetBasePlayer()->GetAnimOverlay( iLayer );

	pLayer->m_flCycle = flCurCycle;
	pLayer->m_nSequence = iSequence;

	pLayer->m_flPlaybackRate = 1.0;
	pLayer->m_flWeight = flWeight;
	pLayer->m_nOrder = iLayer;
	
}

extern ConVar mp_facefronttime;
extern ConVar mp_feetyawrate;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::Update( float eyeYaw, float eyePitch )
{
	// Profile the animation update.
	VPROF( "CDODPlayerAnimState::Update" );

	// Get the TF player.
	CTFPlayer *pTFPlayer = GetOuterDOD();
	if ( !pTFPlayer )
		return;

	// Get the studio header for the player.
	CStudioHdr *pStudioHdr = GetOuterDOD()->GetModelPtr();
	if ( !pStudioHdr )
		return;

	// Check to see if we should be updating the animation state - dead, ragdolled?
	if ( !ShouldUpdateAnimState() )
	{
		ClearAnimationState();
		return;
	}

	// Store the eye angles.
	m_flEyeYaw = AngleNormalize( eyeYaw );
	m_flEyePitch = AngleNormalize( eyePitch );

	// Clear animation overlays because we're about to completely reconstruct them.
//	ClearAnimationLayers();

	// Compute the player sequences.
	ComputeSequences( pStudioHdr );

	if ( SetupPoseParameters( pStudioHdr ) )
	{
		// Pose parameter - what direction are the player's legs running in.
		ComputePoseParam_MoveYaw( pStudioHdr );

		// Pose parameter - Torso aiming (up/down).
		ComputePoseParam_AimPitch( pStudioHdr );

		// Pose parameter - Torso aiming (rotation).
		ComputePoseParam_AimYaw( pStudioHdr );

		// Pose parameter - Body Height (torso elevation).
		ComputePoseParam_BodyHeight( pStudioHdr );
	}

#ifdef CLIENT_DLL 
	if ( C_BasePlayer::ShouldDrawLocalPlayer() )
	{
		GetBasePlayer()->SetPlaybackRate( 1.0f );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDODPlayerAnimState::SetupPoseParameters( CStudioHdr *pStudioHdr )
{
	// Check to see if this has already been done.
	if ( m_bPoseParameterInit )
		return true;

	// Save off the pose parameter indices.
	if ( !pStudioHdr )
		return false;

	// Look for the movement blenders.
	m_PoseParameterData.m_iMoveX = GetBasePlayer()->LookupPoseParameter( pStudioHdr, "move_x" );
	m_PoseParameterData.m_iMoveY = GetBasePlayer()->LookupPoseParameter( pStudioHdr, "move_y" );

	// Look for the aim pitch blender.
	m_PoseParameterData.m_iAimPitch = GetBasePlayer()->LookupPoseParameter( pStudioHdr, "body_pitch" );

	// Look for aim yaw blender.
	m_PoseParameterData.m_iAimYaw = GetBasePlayer()->LookupPoseParameter( pStudioHdr, "body_yaw" );

	// Look for the body height blender.
	m_PoseParameterData.m_iBodyHeight = GetBasePlayer()->LookupPoseParameter( pStudioHdr, "body_height" );

	m_bPoseParameterInit = true;

	return true;
}

#define DOD_MOVEMENT_ERROR_LIMIT  1.0

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr )
{
	// Check to see if we are deployed or prone.
	if( GetOuterDOD()->m_Shared.IsInMGDeploy() || GetOuterDOD()->m_Shared.IsProne() )
	{
		// Set the 9-way blend movement pose parameters.
		Vector2D vecCurrentMoveYaw( 0.0f, 0.0f );
		GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveX, vecCurrentMoveYaw.x );
		GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveY, vecCurrentMoveYaw.y );

		m_vecLastMoveYaw = vecCurrentMoveYaw;

#if 0
		// Rotate the entire body instantly.
		m_flGoalFeetYaw = AngleNormalize( m_flEyeYaw );
		m_flCurrentFeetYaw = m_flGoalFeetYaw;
		m_flLastTurnTime = gpGlobals->curtime;

		// Rotate entire body into position.
		m_angRender[YAW] = m_flCurrentFeetYaw;
		m_angRender[PITCH] = m_angRender[ROLL] = 0;

		SetOuterBodyYaw( m_flCurrentFeetYaw );
		g_flLastBodyYaw = m_flCurrentFeetYaw;
#endif
	}
	else
	{
		// Get the estimated movement yaw.
		EstimateYaw();

		// Get the view yaw.
		float flAngle = AngleNormalize( m_flEyeYaw );

		// rotate movement into local reference frame
		float flYaw = flAngle - m_PoseParameterData.m_flEstimateYaw;
		flYaw = AngleNormalize( -flYaw );

		// Get the current speed the character is running.
		Vector vecEstVelocity;
		vecEstVelocity.x = cos( DEG2RAD( flYaw ) ) * m_flEstimateVelocity;
		vecEstVelocity.y = sin( DEG2RAD( flYaw ) ) * m_flEstimateVelocity;

		Vector2D vecCurrentMoveYaw( 0.0f, 0.0f );
		// set the pose parameters to the correct direction, but not value
		if ( vecEstVelocity.x != 0.0f && fabs( vecEstVelocity.x ) > fabs( vecEstVelocity.y ) )
		{
			vecCurrentMoveYaw.x = (vecEstVelocity.x < 0.0) ? -1.0 : 1.0;
			vecCurrentMoveYaw.y = vecEstVelocity.y / fabs( vecEstVelocity.x );
		}
		else if (vecEstVelocity.y != 0.0f)
		{
			vecCurrentMoveYaw.y = (vecEstVelocity.y < 0.0) ? -1.0 : 1.0;
			vecCurrentMoveYaw.x = vecEstVelocity.x / fabs( vecEstVelocity.y );
		}

#ifndef CLIENT_DLL
		GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveX, vecCurrentMoveYaw.x );
		GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveY, vecCurrentMoveYaw.y );
#else

		// refine pose parameters to be more accurate
		int i = 0;
		float dx, dy;
		Vector vecAnimVelocity;

		/*
		if ( m_pOuter->entindex() == 2 )
		{
			GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveX, vecCurrentMoveYaw.x );
			GetOuter()->SetPoseParameter( pStudioHdr, m_iMoveY, vecCurrentMoveYaw.y );
			GetOuterDOD()->GetBlendedLinearVelocity( &vecAnimVelocity );
			DevMsgRT("(%.2f) %.3f : (%.2f) %.3f\n", vecAnimVelocity.x, vecCurrentMoveYaw.x, vecAnimVelocity.y, vecCurrentMoveYaw.y );
		}
		*/

		bool retry = true;
		do 
		{
			// Set the 9-way blend movement pose parameters.
			GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveX, vecCurrentMoveYaw.x );
			GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveY, vecCurrentMoveYaw.y );

			GetOuterDOD()->GetBlendedLinearVelocity( &vecAnimVelocity );

			// adjust X pose parameter based on movement error
			if (fabs( vecAnimVelocity.x ) > 0.001)
			{
				vecCurrentMoveYaw.x *= vecEstVelocity.x / vecAnimVelocity.x;
			}
			else
			{
				vecCurrentMoveYaw.x = 0;
				GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveX, vecCurrentMoveYaw.x );
			}
			// adjust Y pose parameter based on movement error
			if (fabs( vecAnimVelocity.y ) > 0.001)
			{
				vecCurrentMoveYaw.y *= vecEstVelocity.y / vecAnimVelocity.y;
			}
			else
			{
				vecCurrentMoveYaw.y = 0;
				GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveY, vecCurrentMoveYaw.y );
			}

			dx =  vecEstVelocity.x - vecAnimVelocity.x;
			dy =  vecEstVelocity.y - vecAnimVelocity.y;

			retry = (vecCurrentMoveYaw.x < 1.0 && vecCurrentMoveYaw.x > -1.0) && (dx < -DOD_MOVEMENT_ERROR_LIMIT || dx > DOD_MOVEMENT_ERROR_LIMIT);
			retry = retry || ((vecCurrentMoveYaw.y < 1.0 && vecCurrentMoveYaw.y > -1.0) && (dy < -DOD_MOVEMENT_ERROR_LIMIT || dy > DOD_MOVEMENT_ERROR_LIMIT));

		} while (i++ < 5 && retry);

		/*
		if ( m_pOuter->entindex() == 2 )
		{
			DevMsgRT("%d(%.2f : %.2f) %.3f : (%.2f : %.2f) %.3f\n", 
				i,
				vecEstVelocity.x, vecAnimVelocity.x, vecCurrentMoveYaw.x, 
				vecEstVelocity.y, vecAnimVelocity.y, vecCurrentMoveYaw.y );
		}
		*/
#endif

		m_vecLastMoveYaw = vecCurrentMoveYaw;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::EstimateYaw( void )
{
	// Get the frame time.
	float flDeltaTime = gpGlobals->frametime;
	if ( flDeltaTime == 0.0f )
		return;

	// Get the player's velocity and angles.
	Vector vecEstVelocity;
	GetOuterAbsVelocity( vecEstVelocity );
	QAngle angles = GetOuterDOD()->GetLocalAngles();

	// If we are not moving, sync up the feet and eyes slowly.
	if ( vecEstVelocity.x == 0.0f && vecEstVelocity.y == 0.0f )
	{
		float flYawDelta = angles[YAW] - m_PoseParameterData.m_flEstimateYaw;
		flYawDelta = AngleNormalize( flYawDelta );

		if ( flDeltaTime < 0.25f )
		{
			flYawDelta *= ( flDeltaTime * 4.0f );
		}
		else
		{
			flYawDelta *= flDeltaTime;
		}

		m_flEstimateVelocity = 0.0;
		m_PoseParameterData.m_flEstimateYaw += flYawDelta;
		AngleNormalize( m_PoseParameterData.m_flEstimateYaw );
	}
	else
	{
		m_flEstimateVelocity = vecEstVelocity.Length2D();
		m_PoseParameterData.m_flEstimateYaw = ( atan2( vecEstVelocity.y, vecEstVelocity.x ) * 180.0f / M_PI );
		m_PoseParameterData.m_flEstimateYaw = clamp( m_PoseParameterData.m_flEstimateYaw, -180.0f, 180.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::ComputePoseParam_AimPitch( CStudioHdr *pStudioHdr )
{
	// Get the view pitch.
	float flAimPitch = m_flEyePitch;

	// Lock pitch at 0 if a reload gesture is playing
#ifdef CLIENT_DLL
	if ( IsGestureSlotActive( GESTURE_SLOT_GRENADE ) )
		flAimPitch = 0;
#else
	Activity idealActivity = TranslateActivity( ACT_RELOAD );

	if ( GetBasePlayer()->IsPlayingGesture( idealActivity ) )
		flAimPitch = 0;
#endif

	// Set the aim pitch pose parameter and save.
	GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iAimPitch, -flAimPitch );
	m_DebugAnimData.m_flAimPitch = flAimPitch;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::ComputePoseParam_AimYaw( CStudioHdr *pStudioHdr )
{
	// Get the movement velocity.
	Vector vecVelocity;
	GetOuterAbsVelocity( vecVelocity );

	// Check to see if we are moving.
	bool bMoving = ( vecVelocity.Length() > 1.0f ) ? true : false;

	// Check our prone and deployed state.
	bool bDeployed = GetOuterDOD()->m_Shared.IsSandbagDeployed() || GetOuterDOD()->m_Shared.IsProneDeployed();
	bool bProne = GetOuterDOD()->m_Shared.IsProne();

	// If we are moving or are prone and undeployed.
	if ( bMoving || ( bProne && !bDeployed ) || m_bForceAimYaw )
	{
		// The feet match the eye direction when moving - the move yaw takes care of the rest.
		m_flGoalFeetYaw = m_flEyeYaw;
	}
	// Else if we are not moving.
	else
	{
		// Initialize the feet.
		if ( m_PoseParameterData.m_flLastAimTurnTime <= 0.0f )
		{
			m_flGoalFeetYaw	= m_flEyeYaw;
			m_flCurrentFeetYaw = m_flEyeYaw;
			m_PoseParameterData.m_flLastAimTurnTime = gpGlobals->curtime;
		}
		// Make sure the feet yaw isn't too far out of sync with the eye yaw.
		// TODO: Do something better here!
		else
		{
			float flYawDelta = AngleNormalize(  m_flGoalFeetYaw - m_flEyeYaw );

			if ( bDeployed )
			{
				if ( fabs( flYawDelta ) > 20.0f )
				{
					float flSide = ( flYawDelta > 0.0f ) ? -1.0f : 1.0f;
					m_flGoalFeetYaw += ( 20.0f * flSide );
				}
			}
			else
			{
				if ( fabs( flYawDelta ) > 45.0f )
				{
					float flSide = ( flYawDelta > 0.0f ) ? -1.0f : 1.0f;
					m_flGoalFeetYaw += ( 45.0f * flSide );
				}
			}
		}
	}

	// Fix up the feet yaw.
	m_flGoalFeetYaw = AngleNormalize( m_flGoalFeetYaw );
	if ( m_flGoalFeetYaw != m_flCurrentFeetYaw )
	{
		// If you are forcing aim yaw, your code is almost definitely broken if you don't include a delay between 
		// teleporting and forcing yaw. This is due to an unfortunate interaction between the command lookback window,
		// and the fact that m_flEyeYaw is never propogated from the server to the client.
		// TODO: Fix this after Halloween 2014.
		if ( m_bForceAimYaw )
		{
			m_flCurrentFeetYaw = m_flGoalFeetYaw;
		}
		else
		{
			ConvergeYawAngles( m_flGoalFeetYaw, DOD_BODYYAW_RATE, gpGlobals->frametime, m_flCurrentFeetYaw );
			m_flLastAimTurnTime = gpGlobals->curtime;
		}
	}

	// Rotate the body into position.
	m_angRender[YAW] = m_flCurrentFeetYaw;

	// Find the aim(torso) yaw base on the eye and feet yaws.
	float flAimYaw = m_flEyeYaw - m_flCurrentFeetYaw;
	flAimYaw = AngleNormalize( flAimYaw );

	// Set the aim yaw and save.
	GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iAimYaw, -flAimYaw );
	m_DebugAnimData.m_flAimYaw = flAimYaw;

	// Turn off a force aim yaw - either we have already updated or we don't need to.
	m_bForceAimYaw = false;

#ifndef CLIENT_DLL
	QAngle angle = GetBasePlayer()->GetAbsAngles();
	angle[YAW] = m_flCurrentFeetYaw;

	GetBasePlayer()->SetAbsAngles( angle );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::ConvergeYawAngles( float flGoalYaw, float flYawRate, float flDeltaTime, float &flCurrentYaw )
{
#define FADE_TURN_DEGREES 60.0f

	// Find the yaw delta.
	float flDeltaYaw = flGoalYaw - flCurrentYaw;
	float flDeltaYawAbs = fabs( flDeltaYaw );
	flDeltaYaw = AngleNormalize( flDeltaYaw );

	// Always do at least a bit of the turn (1%).
	float flScale = 1.0f;
	flScale = flDeltaYawAbs / FADE_TURN_DEGREES;
	flScale = clamp( flScale, 0.01f, 1.0f );

	float flYaw = flYawRate * flDeltaTime * flScale;
	if ( flDeltaYawAbs < flYaw )
	{
		flCurrentYaw = flGoalYaw;
	}
	else
	{
		float flSide = ( flDeltaYaw < 0.0f ) ? -1.0f : 1.0f;
		flCurrentYaw += ( flYaw * flSide );
	}

	flCurrentYaw = AngleNormalize( flCurrentYaw );

#undef FADE_TURN_DEGREES
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerAnimState::ComputePoseParam_BodyHeight( CStudioHdr *pStudioHdr )
{
	if( m_pOuterDOD->m_Shared.IsSandbagDeployed() )
	{
//		float flHeight = m_pOuterDOD->m_Shared.GetDeployedHeight() - 4.0f;
		float flHeight = m_pOuterDOD->m_Shared.GetDeployedHeight() - dod_bodyheightoffset.GetFloat();
		GetBasePlayer()->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iBodyHeight, flHeight );
		m_flLastBodyHeight = flHeight;
	}
}