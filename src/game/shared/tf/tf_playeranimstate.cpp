//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"
#include "tf_playeranimstate.h"
#include "base_playeranimstate.h"
#include "datacache/imdlcache.h"
#include "tf_gamerules.h"
#include "in_buttons.h"
#include "debugoverlay_shared.h"

#ifdef CLIENT_DLL
#include "c_tf_player.h"

#define CCaptureZone C_CaptureZone
#else
#include "tf_player.h"
#endif

#define TF_RUN_SPEED			320.0f
#define TF_WALK_SPEED			75.0f
#define TF_CROUCHWALK_SPEED		110.0f

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
// Output : CMultiPlayerAnimState*
//-----------------------------------------------------------------------------
CTFPlayerAnimState* CreateTFPlayerAnimState( CTFPlayer *pPlayer )
{
	MDLCACHE_CRITICAL_SECTION();

	// Setup the movement data.
	MultiPlayerMovementData_t movementData;
	movementData.m_flBodyYawRate = 720.0f;
	movementData.m_flRunSpeed = TF_RUN_SPEED;
	movementData.m_flWalkSpeed = TF_WALK_SPEED;
	movementData.m_flSprintSpeed = -1.0f;

	// Create animation state for this player.
	CTFPlayerAnimState *pRet = new CTFPlayerAnimState( pPlayer, movementData );

	// Specific TF player initialization.
	pRet->InitTF( pPlayer );

	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CTFPlayerAnimState::CTFPlayerAnimState()
{
	m_pTFPlayer = NULL;

	// Don't initialize TF specific variables here. Init them in InitTF()
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			&movementData - 
//-----------------------------------------------------------------------------
CTFPlayerAnimState::CTFPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData )
: CMultiPlayerAnimState( pPlayer, movementData )
{
	m_pTFPlayer = NULL;

	// Don't initialize TF specific variables here. Init them in InitTF()
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CTFPlayerAnimState::~CTFPlayerAnimState()
{
}

//-----------------------------------------------------------------------------
// Purpose: Initialize Team Fortress specific animation state.
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CTFPlayerAnimState::InitTF( CTFPlayer *pPlayer )
{
	m_pTFPlayer = pPlayer;
	m_bInAirWalk = false;
	m_flHoldDeployedPoseUntilTime = 0.0f;
	m_flTauntMoveX = 0.f;
	m_flTauntMoveY = 0.f;
	m_vecSmoothedUp = Vector( 0.f, 0.f, 1.f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerAnimState::ClearAnimationState( void )
{
	m_bInAirWalk = false;

	BaseClass::ClearAnimationState();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : actDesired - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CTFPlayerAnimState::TranslateActivity( Activity actDesired )
{
	Activity translateActivity = BaseClass::TranslateActivity( actDesired );

	translateActivity = ActivityOverride( translateActivity, NULL );

	CBaseCombatWeapon *pWeapon = GetTFPlayer()->GetActiveWeapon();
	if ( pWeapon )
		translateActivity = pWeapon->ActivityOverride( translateActivity, NULL );

	CTFPlayer *pPlayer = GetTFPlayer();
	if ( pPlayer->m_Shared.InCond( TF_COND_COMPETITIVE_WINNER ) )
	{
		if ( translateActivity == ACT_MP_STAND_PRIMARY || 
		   ( pPlayer->IsPlayerClass( TF_CLASS_SPY ) && ( translateActivity == ACT_MP_STAND_MELEE ) ) || 
		   ( pPlayer->IsPlayerClass( TF_CLASS_DEMOMAN ) && ( translateActivity == ACT_MP_STAND_SECONDARY ) ) )
		{
			translateActivity = ACT_MP_COMPETITIVE_WINNERSTATE;
		}
	}

	return translateActivity;
}


static acttable_t s_acttableKartState[] = 
{
	{ ACT_MP_STAND_IDLE,					ACT_KART_IDLE,			false },
	{ ACT_MP_RUN,							ACT_KART_IDLE,			false },
	{ ACT_MP_WALK,							ACT_KART_IDLE,			false },
	{ ACT_MP_AIRWALK,						ACT_KART_IDLE,			false },
	{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,		ACT_KART_ACTION_SHOOT,	false },
	{ ACT_MP_ATTACK_AIRWALK_PRIMARYFIRE,	ACT_KART_ACTION_SHOOT,	false },
	{ ACT_MP_JUMP_START,					ACT_KART_JUMP_START,	false },
	{ ACT_MP_JUMP_FLOAT,					ACT_KART_JUMP_FLOAT,	false },
	{ ACT_MP_JUMP_LAND,						ACT_KART_JUMP_LAND,		false },
};

static acttable_t s_acttableLoserState[] = 
{
	{ ACT_MP_STAND_IDLE,		ACT_MP_STAND_LOSERSTATE,			false },
	{ ACT_MP_CROUCH_IDLE,		ACT_MP_CROUCH_LOSERSTATE,			false },
	{ ACT_MP_RUN,				ACT_MP_RUN_LOSERSTATE,				false },
	{ ACT_MP_WALK,				ACT_MP_WALK_LOSERSTATE,				false },
	{ ACT_MP_AIRWALK,			ACT_MP_AIRWALK_LOSERSTATE,			false },
	{ ACT_MP_CROUCHWALK,		ACT_MP_CROUCHWALK_LOSERSTATE,		false },
	{ ACT_MP_JUMP,				ACT_MP_JUMP_LOSERSTATE,				false },
	{ ACT_MP_JUMP_START,		ACT_MP_JUMP_START_LOSERSTATE,		false },
	{ ACT_MP_JUMP_FLOAT,		ACT_MP_JUMP_FLOAT_LOSERSTATE,		false },
	{ ACT_MP_JUMP_LAND,			ACT_MP_JUMP_LAND_LOSERSTATE,		false },
	{ ACT_MP_SWIM,				ACT_MP_SWIM_LOSERSTATE,				false },
	{ ACT_MP_DOUBLEJUMP_CROUCH,	ACT_MP_DOUBLEJUMP_CROUCH_LOSERSTATE,false },
};

static acttable_t s_acttableCompetitiveLoserState[] =
{
	{ ACT_MP_STAND_IDLE, ACT_MP_COMPETITIVE_LOSERSTATE, false },
};

static acttable_t s_acttableBuildingDeployed[] = 
{
	{ ACT_MP_STAND_IDLE,		ACT_MP_STAND_BUILDING_DEPLOYED,			false },
	{ ACT_MP_CROUCH_IDLE,		ACT_MP_CROUCH_BUILDING_DEPLOYED,		false },
	{ ACT_MP_RUN,				ACT_MP_RUN_BUILDING_DEPLOYED,			false },
	{ ACT_MP_WALK,				ACT_MP_WALK_BUILDING_DEPLOYED,			false },
	{ ACT_MP_AIRWALK,			ACT_MP_AIRWALK_BUILDING_DEPLOYED,		false },
	{ ACT_MP_CROUCHWALK,		ACT_MP_CROUCHWALK_BUILDING_DEPLOYED,	false },
	{ ACT_MP_JUMP,				ACT_MP_JUMP_BUILDING_DEPLOYED,			false },
	{ ACT_MP_JUMP_START,		ACT_MP_JUMP_START_BUILDING_DEPLOYED,	false },
	{ ACT_MP_JUMP_FLOAT,		ACT_MP_JUMP_FLOAT_BUILDING_DEPLOYED,	false },
	{ ACT_MP_JUMP_LAND,			ACT_MP_JUMP_LAND_BUILDING_DEPLOYED,		false },
	{ ACT_MP_SWIM,				ACT_MP_SWIM_BUILDING_DEPLOYED,			false },

	{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,		ACT_MP_ATTACK_STAND_BUILDING_DEPLOYED,		false },
	{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,		ACT_MP_ATTACK_CROUCH_BUILDING_DEPLOYED,		false },
	{ ACT_MP_ATTACK_SWIM_PRIMARYFIRE,		ACT_MP_ATTACK_SWIM_BUILDING_DEPLOYED,		false },
	{ ACT_MP_ATTACK_AIRWALK_PRIMARYFIRE,	ACT_MP_ATTACK_AIRWALK_BUILDING_DEPLOYED,	false },

	{ ACT_MP_ATTACK_STAND_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE_BUILDING_DEPLOYED,	false },
	{ ACT_MP_ATTACK_CROUCH_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE_BUILDING_DEPLOYED,	false },
	{ ACT_MP_ATTACK_SWIM_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE_BUILDING_DEPLOYED,	false },
	{ ACT_MP_ATTACK_AIRWALK_GRENADE,	ACT_MP_ATTACK_STAND_GRENADE_BUILDING_DEPLOYED,	false },

	{ ACT_MP_GESTURE_VC_HANDMOUTH,	ACT_MP_GESTURE_VC_HANDMOUTH_BUILDING,		false },
	{ ACT_MP_GESTURE_VC_FINGERPOINT,	ACT_MP_GESTURE_VC_FINGERPOINT_BUILDING,	false },
	{ ACT_MP_GESTURE_VC_FISTPUMP,	ACT_MP_GESTURE_VC_FISTPUMP_BUILDING,		false },
	{ ACT_MP_GESTURE_VC_THUMBSUP,	ACT_MP_GESTURE_VC_THUMBSUP_BUILDING,		false },
	{ ACT_MP_GESTURE_VC_NODYES,	ACT_MP_GESTURE_VC_NODYES_BUILDING,				false },
	{ ACT_MP_GESTURE_VC_NODNO,	ACT_MP_GESTURE_VC_NODNO_BUILDING,				false },
};

Activity CTFPlayerAnimState::ActivityOverride( Activity baseAct, bool *pRequired )
{
	acttable_t *pTable = NULL;
	int iActivityCount = 0;

	CTFPlayer *pPlayer = GetTFPlayer();

	// Override if we're in a kart
	if ( pPlayer->m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
	{
		pTable = s_acttableKartState;
		iActivityCount = ARRAYSIZE( s_acttableKartState );
	}
	else
	{
		if ( pPlayer->m_Shared.InCond( TF_COND_COMPETITIVE_LOSER ) )
		{
			iActivityCount = ARRAYSIZE( s_acttableCompetitiveLoserState );
			pTable = s_acttableCompetitiveLoserState;
		}
		else if ( pPlayer->m_Shared.IsLoser() )
		{
			iActivityCount = ARRAYSIZE( s_acttableLoserState );
			pTable = s_acttableLoserState;
		}
		else if ( pPlayer->m_Shared.IsCarryingObject() )
		{
			iActivityCount = ARRAYSIZE( s_acttableBuildingDeployed );
			pTable = s_acttableBuildingDeployed;
		}
	}

	for ( int i = 0; i < iActivityCount; i++ )
	{
		const acttable_t& act = pTable[i];
		if ( baseAct == act.baseAct )
		{
			if (pRequired)
			{
				*pRequired = act.required;
			}
			return (Activity)act.weaponAct;
		}
	}

	return baseAct;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerAnimState::Update( float eyeYaw, float eyePitch )
{
	// Profile the animation update.
	VPROF( "CMultiPlayerAnimState::Update" );

	// Get the TF player.
	CTFPlayer *pTFPlayer = GetTFPlayer();
	if ( !pTFPlayer )
		return;

	// Get the studio header for the player.
	CStudioHdr *pStudioHdr = pTFPlayer->GetModelPtr();
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

	// Compute the player sequences.
	ComputeSequences( pStudioHdr );

	bool bInTaunt = pTFPlayer->m_Shared.InCond( TF_COND_TAUNTING );
	bool bInKart = pTFPlayer->m_Shared.InCond( TF_COND_HALLOWEEN_KART );
	bool bIsImmobilized = bInTaunt || pTFPlayer->m_Shared.IsControlStunned();

	if ( SetupPoseParameters( pStudioHdr ) )
	{
		if ( !bIsImmobilized )
		{
			// Pose parameter - what direction are the player's legs running in.
			ComputePoseParam_MoveYaw( pStudioHdr );
		}

		if ( bInTaunt )
		{
			// If you are forcing aim yaw, your code is almost definitely broken if you don't include a delay between 
			// teleporting and forcing yaw. This is due to an unfortunate interaction between the command lookback window,
			// and the fact that m_flEyeYaw is never propogated from the server to the client.
			// TODO: Fix this after Halloween 2014.
			m_bForceAimYaw = true;
			m_flEyeYaw = pTFPlayer->GetTauntYaw();

			Taunt_ComputePoseParam_MoveX( pStudioHdr );
			Taunt_ComputePoseParam_MoveY( pStudioHdr );
		}
		
		if ( !bIsImmobilized || bInTaunt || bInKart )
		{
			// Pose parameter - Torso aiming (up/down).
			ComputePoseParam_AimPitch( pStudioHdr );

			// Pose parameter - Torso aiming (rotation).
			ComputePoseParam_AimYaw( pStudioHdr );
		}
	}

#ifdef CLIENT_DLL 
	if ( C_BasePlayer::ShouldDrawLocalPlayer() )
	{
		GetBasePlayer()->SetPlaybackRate( 1.0f );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Updates animation state if we're stunned.
//-----------------------------------------------------------------------------
void CTFPlayerAnimState::CheckStunAnimation()
{
	CTFPlayer *pPlayer = GetTFPlayer();
	if ( !pPlayer )
		return;

	// do not play stun anims if in kart
	if ( pPlayer->m_Shared.InCond( TF_COND_HALLOWEEN_KART ) )
		return;

	// State machine to determine the correct stun activity.
	if ( !pPlayer->m_Shared.IsControlStunned() && 
		 (pPlayer->m_Shared.m_iStunAnimState == STUN_ANIM_LOOP) )
	{
		// Clean up if the condition went away before we finished.
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_STUN_END );
		pPlayer->m_Shared.m_iStunAnimState = STUN_ANIM_NONE;
	}
	else if ( pPlayer->m_Shared.IsControlStunned() &&
		      (pPlayer->m_Shared.m_iStunAnimState == STUN_ANIM_NONE) &&
		      (gpGlobals->curtime < pPlayer->m_Shared.GetStunExpireTime()) )
	{
		// Play the start up animation.
		int iSeq = pPlayer->SelectWeightedSequence( ACT_MP_STUN_BEGIN );
		pPlayer->m_Shared.m_flStunMid = gpGlobals->curtime + pPlayer->SequenceDuration( iSeq );
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_STUN_BEGIN );
		pPlayer->m_Shared.m_iStunAnimState = STUN_ANIM_LOOP;
	}
	else if ( pPlayer->m_Shared.m_iStunAnimState == STUN_ANIM_LOOP )
	{
		// We are playing the looping part of the stun animation cycle.
		if ( gpGlobals->curtime > pPlayer->m_Shared.m_flStunFade )
		{
			// Gameplay is telling us to fade out. Time for the end anim.
			int iSeq = pPlayer->SelectWeightedSequence( ACT_MP_STUN_END );
			pPlayer->m_Shared.SetStunExpireTime( gpGlobals->curtime + pPlayer->SequenceDuration( iSeq ) );
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_STUN_END );
			pPlayer->m_Shared.m_iStunAnimState = STUN_ANIM_END;
		}
		else if ( gpGlobals->curtime > pPlayer->m_Shared.m_flStunMid )
		{
			// Loop again.
			int iSeq = pPlayer->SelectWeightedSequence( ACT_MP_STUN_MIDDLE );
			pPlayer->m_Shared.m_flStunMid = gpGlobals->curtime + pPlayer->SequenceDuration( iSeq );
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_STUN_MIDDLE );
		}
	}
	else if ( pPlayer->m_Shared.m_iStunAnimState == STUN_ANIM_END )
	{
		if ( gpGlobals->curtime > pPlayer->m_Shared.GetStunExpireTime() )
		{
			// The animation loop is over.
			pPlayer->m_Shared.m_iStunAnimState = STUN_ANIM_NONE;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CTFPlayerAnimState::CalcMainActivity()
{
	CheckStunAnimation();

	return BaseClass::CalcMainActivity();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerAnimState::Taunt_ComputePoseParam_MoveX( CStudioHdr *pStudioHdr )
{
	m_flTauntMoveX = 0.f;

	CTFPlayer *pTFPlayer = GetTFPlayer();
	pTFPlayer->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveX, Sign( m_flTauntMoveX ) * SimpleSpline( fabs( m_flTauntMoveX ) ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerAnimState::Taunt_ComputePoseParam_MoveY( CStudioHdr *pStudioHdr )
{
	m_flTauntMoveY = 0.f;

	CTFPlayer *pTFPlayer = GetTFPlayer();
	pTFPlayer->SetPoseParameter( pStudioHdr, m_PoseParameterData.m_iMoveY, Sign( m_flTauntMoveY ) * SimpleSpline( fabs( m_flTauntMoveY ) ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : event - 
//-----------------------------------------------------------------------------
void CTFPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	bool bInDuck = ( m_pTFPlayer->GetFlags() & FL_DUCKING ) ? true : false;
	if ( bInDuck && SelectWeightedSequence( TranslateActivity( ACT_MP_CROUCHWALK ) ) < 0 )
	{
		bInDuck = false;
	}

	Activity iGestureActivity = ACT_INVALID;

	switch( event )
	{
	case PLAYERANIMEVENT_ATTACK_PRIMARY:
		{
			CTFPlayer *pPlayer = GetTFPlayer();
			if ( !pPlayer )
				return;

			CPCWeaponBase *pWpn = pPlayer->GetActivePCWeapon();
			bool bIsMinigun = ( pWpn && pWpn->GetWeaponID() == TF_WEAPON_MINIGUN );
			bool bIsSniperRifle = ( pWpn && WeaponID_IsSniperRifle( pWpn->GetWeaponID() ) );

			// Heavy weapons primary fire.
			if ( bIsMinigun )
			{
				// Play standing primary fire.
				iGestureActivity = ACT_MP_ATTACK_STAND_PRIMARYFIRE;

				if ( m_bInSwim )
				{
					// Play swimming primary fire.
					iGestureActivity = ACT_MP_ATTACK_SWIM_PRIMARYFIRE;
				}
				else if ( bInDuck )
				{
					// Play crouching primary fire.
					iGestureActivity = ACT_MP_ATTACK_CROUCH_PRIMARYFIRE;
				}

				if ( !IsGestureSlotPlaying( GESTURE_SLOT_ATTACK_AND_RELOAD, TranslateActivity(iGestureActivity) ) )
				{
					RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, iGestureActivity );
				}
			}
			else if ( bIsSniperRifle && pPlayer->m_Shared.InCond( TF_COND_ZOOMED ) )
			{
				// Weapon primary fire, zoomed in
				if ( bInDuck )
				{
					RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_ATTACK_CROUCH_PRIMARYFIRE_DEPLOYED );
				}
				else
				{
					RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_ATTACK_STAND_PRIMARYFIRE_DEPLOYED );
				}

				iGestureActivity = ACT_VM_PRIMARYATTACK;

				// Hold our deployed pose for a few seconds
				m_flHoldDeployedPoseUntilTime = gpGlobals->curtime + 2.0;
			}
			else
			{
				Activity baseActivity = bInDuck ? ACT_MP_ATTACK_CROUCH_PRIMARYFIRE : ACT_MP_ATTACK_STAND_PRIMARYFIRE;

				RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, baseActivity );
//				iGestureActivity = ACT_VM_PRIMARYATTACK;
			}

			break;
		}

	case PLAYERANIMEVENT_ATTACK_PRIMARY_SUPER:
		{
			Activity baseActivity = bInDuck ? ACT_MP_ATTACK_CROUCH_PRIMARY_SUPER : ACT_MP_ATTACK_STAND_PRIMARY_SUPER;

			if ( m_bInSwim )
				baseActivity = ACT_MP_ATTACK_SWIM_PRIMARY_SUPER;

			RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, baseActivity );
//			iGestureActivity = ACT_VM_PRIMARYATTACK;
		}
		break;

	case PLAYERANIMEVENT_VOICE_COMMAND_GESTURE:
		{
			if ( !IsGestureSlotActive( GESTURE_SLOT_ATTACK_AND_RELOAD ) )
			{
				RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, (Activity)nData );
			}
			break;
		}
	case PLAYERANIMEVENT_ATTACK_SECONDARY:
		{
			Activity baseActivity = bInDuck ? ACT_MP_ATTACK_CROUCH_SECONDARYFIRE : ACT_MP_ATTACK_STAND_SECONDARYFIRE;
			if ( GetBasePlayer()->GetWaterLevel() >= WL_Waist )
			{
				baseActivity = ACT_MP_ATTACK_SWIM_SECONDARYFIRE;
			}
			
			RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, baseActivity );
			iGestureActivity = ACT_VM_SECONDARYATTACK;
			break;
		}
	case PLAYERANIMEVENT_ATTACK_COMBO:
	{
		Activity baseActivity = bInDuck ? ACT_MP_ATTACK_CROUCH_HARD_ITEM2 : ACT_MP_ATTACK_STAND_HARD_ITEM2;
		if (GetBasePlayer()->GetWaterLevel() >= WL_Waist)
			baseActivity = ACT_MP_ATTACK_SWIM_HARD_ITEM2;

		RestartGesture(GESTURE_SLOT_ATTACK_AND_RELOAD, baseActivity);
		break;
	}
	case PLAYERANIMEVENT_ATTACK_PRE:
		{
			CTFPlayer *pPlayer = GetTFPlayer();
			if ( !pPlayer )
				return;

			CPCWeaponBase *pWpn = pPlayer->GetActivePCWeapon();
			bool bIsMinigun  = ( pWpn && pWpn->GetWeaponID() == TF_WEAPON_MINIGUN );

			bool bAutoKillPreFire = false;
			if ( bIsMinigun )
			{
				bAutoKillPreFire = true;
			}

			if ( m_bInSwim && bIsMinigun )
			{
				// Weapon pre-fire. Used for minigun windup while swimming
				iGestureActivity = ACT_MP_ATTACK_SWIM_PREFIRE;
			}
			else if ( bInDuck ) 
			{
				// Weapon pre-fire. Used for minigun windup, sniper aiming start, etc in crouch.
				iGestureActivity = ACT_MP_ATTACK_CROUCH_PREFIRE;
			}
			else
			{
				// Weapon pre-fire. Used for minigun windup, sniper aiming start, etc.
				iGestureActivity = ACT_MP_ATTACK_STAND_PREFIRE;
			}

			RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, iGestureActivity, bAutoKillPreFire );

			break;
		}
	case PLAYERANIMEVENT_ATTACK_POST:
		{
			CTFPlayer *pPlayer = GetTFPlayer();
			if ( !pPlayer )
				return;

			CPCWeaponBase *pWpn = pPlayer->GetActivePCWeapon();
			bool bIsMinigun  = ( pWpn && pWpn->GetWeaponID() == TF_WEAPON_MINIGUN );

			if ( m_bInSwim && bIsMinigun )
			{
				// Weapon pre-fire. Used for minigun winddown while swimming
				iGestureActivity = ACT_MP_ATTACK_SWIM_POSTFIRE;
			}
			else if ( bInDuck ) 
			{
				// Weapon post-fire. Used for minigun winddown in crouch.
				iGestureActivity = ACT_MP_ATTACK_CROUCH_POSTFIRE;
			}
			else
			{
				// Weapon post-fire. Used for minigun winddown.
				iGestureActivity = ACT_MP_ATTACK_STAND_POSTFIRE;
			}

			RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, iGestureActivity );

			break;
		}

	case PLAYERANIMEVENT_RELOAD:
		{
			// Weapon reload.
			if ( m_bInAirWalk )
			{
				RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_RELOAD_AIRWALK );
			}
			else
			{
				BaseClass::DoAnimationEvent( event, nData );
			}
			break;
		}
	case PLAYERANIMEVENT_RELOAD_LOOP:
		{
			// Weapon reload.
			if ( m_bInAirWalk )
			{
				RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_RELOAD_AIRWALK_LOOP );
			}
			else
			{
				BaseClass::DoAnimationEvent( event, nData );
			}
			break;
		}
	case PLAYERANIMEVENT_RELOAD_END:
		{
			// Weapon reload.
			if ( m_bInAirWalk )
			{
				RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_RELOAD_AIRWALK_END );
			}
			else
			{
				BaseClass::DoAnimationEvent( event, nData );
			}
			break;
		}
	case PLAYERANIMEVENT_DOUBLEJUMP:
		{
			CTFPlayer *pPlayer = GetTFPlayer();
			if ( !pPlayer )
				return;

			// Check to see if we are jumping!
			if ( !m_bJumping )
			{
				m_bJumping = true;
				m_bFirstJumpFrame = true;
				m_flJumpStartTime = gpGlobals->curtime;
				RestartMainSequence();
			}

			// Force the air walk off.
			m_bInAirWalk = false;

			// Player the air dash gesture.
			if ( pPlayer->m_Shared.IsLoser() )
			{
				RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_DOUBLEJUMP_LOSERSTATE );
			}
			else
			{
				RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_DOUBLEJUMP );
			}
			break;
		}
	case PLAYERANIMEVENT_DOUBLEJUMP_CROUCH:
//		RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_DOUBLEJUMP_CROUCH );
//		m_aGestureSlots[GESTURE_SLOT_JUMP].m_pAnimLayer->m_flBlendIn = 0.4f;
//		m_aGestureSlots[GESTURE_SLOT_JUMP].m_pAnimLayer->m_flBlendOut = 0.4f;
#ifdef CLIENT_DLL
//		m_aGestureSlots[GESTURE_SLOT_JUMP].m_pAnimLayer->m_bClientBlend = true;
#endif
		break;
	case PLAYERANIMEVENT_STUN_BEGIN:
		RestartGesture( GESTURE_SLOT_CUSTOM, ACT_MP_STUN_BEGIN, false );
		break;
	case PLAYERANIMEVENT_STUN_MIDDLE:
		RestartGesture( GESTURE_SLOT_CUSTOM, ACT_MP_STUN_MIDDLE, false );
		break;
	case PLAYERANIMEVENT_STUN_END:
		RestartGesture( GESTURE_SLOT_CUSTOM, ACT_MP_STUN_END );
		break;
	case PLAYERANIMEVENT_PASSTIME_THROW_BEGIN:
		RestartGesture( GESTURE_SLOT_CUSTOM, ACT_MP_PASSTIME_THROW_BEGIN, false );
		break;
	case PLAYERANIMEVENT_PASSTIME_THROW_MIDDLE:
		RestartGesture( GESTURE_SLOT_CUSTOM, ACT_MP_PASSTIME_THROW_MIDDLE, false );
		break;
	case PLAYERANIMEVENT_PASSTIME_THROW_END:
		RestartGesture( GESTURE_SLOT_CUSTOM, ACT_MP_PASSTIME_THROW_END );
		break;
	case PLAYERANIMEVENT_PASSTIME_THROW_CANCEL:
		RestartGesture( GESTURE_SLOT_CUSTOM, ACT_MP_PASSTIME_THROW_CANCEL );
		break;
	default:
		{
			BaseClass::DoAnimationEvent( event, nData );
			break;
		}
	}

#ifdef CLIENT_DLL
	// Make the weapon play the animation as well
	if ( iGestureActivity != ACT_INVALID )
	{
		CBaseCombatWeapon *pWeapon = GetTFPlayer()->GetActiveWeapon();
		if ( pWeapon )
		{
			pWeapon->SendWeaponAnim( iGestureActivity );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
//-----------------------------------------------------------------------------
bool CTFPlayerAnimState::HandleSwimming( Activity &idealActivity )
{
	bool bInWater = BaseClass::HandleSwimming( idealActivity );

	if ( bInWater )
	{
		if ( m_pTFPlayer->m_Shared.IsAiming() )
		{
			CPCWeaponBase *pWpn = m_pTFPlayer->GetActivePCWeapon();
			if ( pWpn && pWpn->GetWeaponID() == TF_WEAPON_MINIGUN )
			{
				idealActivity = ACT_MP_SWIM_DEPLOYED;
			}
			// Check for sniper deployed underwater - should only be when standing on something
			else if ( pWpn && WeaponID_IsSniperRifle( pWpn->GetWeaponID() ) )
			{
				if ( m_pTFPlayer->m_Shared.InCond( TF_COND_ZOOMED ) )
				{
					idealActivity = ACT_MP_SWIM_DEPLOYED;
				}
			}
		}
	}

	return bInWater;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFPlayerAnimState::HandleMoving( Activity &idealActivity )
{
	float flSpeed = GetOuterXYSpeed();

	// If we move, cancel the deployed anim hold
	if ( flSpeed > MOVING_MINIMUM_SPEED )
	{
		m_flHoldDeployedPoseUntilTime = 0.0;
	}

	if ( m_pTFPlayer->m_Shared.IsLoser() )
	{
		return BaseClass::HandleMoving( idealActivity );
	}

	if ( m_pTFPlayer->m_Shared.IsAiming() ) 
	{
		if ( flSpeed > MOVING_MINIMUM_SPEED )
		{
			idealActivity = ACT_MP_DEPLOYED;
		}
		else
		{
			idealActivity = ACT_MP_DEPLOYED_IDLE;
		}
	}
	else if ( m_flHoldDeployedPoseUntilTime > gpGlobals->curtime )
	{
		// Unless we move, hold the deployed pose for a number of seconds after being deployed
		idealActivity = ACT_MP_DEPLOYED_IDLE;
	}
	else 
	{
		return BaseClass::HandleMoving( idealActivity );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFPlayerAnimState::HandleDucking( Activity &idealActivity )
{
	bool bInDuck = ( m_pTFPlayer->GetFlags() & FL_DUCKING ) ? true : false;
	if ( bInDuck && SelectWeightedSequence( TranslateActivity( ACT_MP_CROUCHWALK ) ) < 0 && !m_pTFPlayer->m_Shared.IsLoser() )
	{
		bInDuck = false;
	}

	if ( bInDuck )
	{
		if ( GetOuterXYSpeed() < MOVING_MINIMUM_SPEED || m_pTFPlayer->m_Shared.IsLoser() )
		{
			idealActivity = ACT_MP_CROUCH_IDLE;		
			if ( m_pTFPlayer->m_Shared.IsAiming() || m_flHoldDeployedPoseUntilTime > gpGlobals->curtime )
			{
				idealActivity = ACT_MP_CROUCH_DEPLOYED_IDLE;
			}
		}
		else
		{
			if ( m_pTFPlayer->m_Shared.GetAirDash() > 0 )
			{
				idealActivity = ACT_MP_DOUBLEJUMP_CROUCH;
			}
			else
			{
				idealActivity = ACT_MP_CROUCHWALK;		
			}

			if ( m_pTFPlayer->m_Shared.IsAiming() )
			{
				// Don't do this for the heavy! we don't usually let him deployed crouch walk
				bool bIsMinigun = false;

				CTFPlayer *pPlayer = GetTFPlayer();
				if ( pPlayer && pPlayer->GetActivePCWeapon() )
				{
					bIsMinigun = ( pPlayer->GetActivePCWeapon()->GetWeaponID() == TF_WEAPON_MINIGUN );
				}

				if ( !bIsMinigun )
				{
					idealActivity = ACT_MP_CROUCH_DEPLOYED;
				}
			}
		}

		return true;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayerAnimState::GetCurrentMaxGroundSpeed()
{
	float flSpeed = BaseClass::GetCurrentMaxGroundSpeed();

	if ( m_pTFPlayer->m_Shared.GetAirDash() > 0 )
	{
		return 1.f;
	}
	else
	{
		return flSpeed;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayerAnimState::GetGesturePlaybackRate( void )
{
	float flPlaybackRate = 1.f;
	return flPlaybackRate;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayerAnimState::HandleJumping( Activity &idealActivity )
{
	bool bInDuck = ( m_pTFPlayer->GetFlags() & FL_DUCKING ) ? true : false;
	if ( bInDuck && SelectWeightedSequence( TranslateActivity( ACT_MP_CROUCHWALK ) ) < 0 )
	{
		bInDuck = false;
	}

	Vector vecVelocity;
	GetOuterAbsVelocity( vecVelocity );

	// Don't allow a firing heavy to jump or air walk.
	if ( m_pTFPlayer->GetPlayerClass()->IsClass( TF_CLASS_HEAVYWEAPONS ) && m_pTFPlayer->m_Shared.InCond( TF_COND_AIMING ) )
		return false;
		
	// Handle air walking before handling jumping - air walking supersedes jump
	TFPlayerClassData_t *pData = m_pTFPlayer->GetPlayerClass()->GetData();
	bool bValidAirWalkClass = ( pData && pData->m_bDontDoAirwalk == false );

	if ( bValidAirWalkClass && ( vecVelocity.z > 300.0f || m_bInAirWalk ) && !bInDuck )
	{
		// Check to see if we were in an airwalk and now we are basically on the ground.
		if ( ( GetBasePlayer()->GetFlags() & FL_ONGROUND ) && m_bInAirWalk )
		{				
			m_bInAirWalk = false;
			RestartMainSequence();
			RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_JUMP_LAND );	
		}
		else if ( GetBasePlayer()->GetWaterLevel() >= WL_Waist )
		{
			// Turn off air walking and reset the animation.
			m_bInAirWalk = false;
			RestartMainSequence();
		}
		else if ( ( GetBasePlayer()->GetFlags() & FL_ONGROUND ) == 0 )
		{
			// In an air walk.
			idealActivity = ACT_MP_AIRWALK;
			m_bInAirWalk = true;
		}
	}
	// Jumping.
	else
	{
		if ( m_bJumping )
		{
			// Remove me once all classes are doing the new jump
			TFPlayerClassData_t *pDataJump = m_pTFPlayer->GetPlayerClass()->GetData();
			bool bNewJump = (pDataJump && pDataJump->m_bDontDoNewJump == false );

			if ( m_bFirstJumpFrame )
			{
				m_bFirstJumpFrame = false;
				RestartMainSequence();	// Reset the animation.
			}

			// Reset if we hit water and start swimming.
			if ( GetBasePlayer()->GetWaterLevel() >= WL_Waist )
			{
				m_bJumping = false;
				RestartMainSequence();
			}
			// Don't check if he's on the ground for a sec.. sometimes the client still has the
			// on-ground flag set right when the message comes in.
			else if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f )
			{
				if ( GetBasePlayer()->GetFlags() & FL_ONGROUND )
				{
					m_bJumping = false;
					RestartMainSequence();

					if ( bNewJump )
					{
						RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_JUMP_LAND );					
					}
				}
			}

			// if we're still jumping
			if ( m_bJumping )
			{
				if ( bNewJump )
				{
					if ( gpGlobals->curtime - m_flJumpStartTime > 0.5 )
					{
						idealActivity = ACT_MP_JUMP_FLOAT;
					}
					else
					{
						idealActivity = ACT_MP_JUMP_START;
					}
				}
				else
				{
					idealActivity = ACT_MP_JUMP;
				}
			}
		}	
	}

	if ( m_bJumping || m_bInAirWalk )
		return true;

	return false;
}