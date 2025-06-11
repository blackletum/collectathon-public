//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Weapon Knife.
//
//=============================================================================
#include "cbase.h"
#include "tf_gamerules.h"
#include "tf_weapon_knife.h"
#include "decals.h"
#include "debugoverlay_shared.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
// Server specific.
#else
#include "tf_player.h"
#include "tf_gamestats.h"
#include "ilagcompensationmanager.h"
#endif

//=============================================================================
//
// Weapon Knife tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFKnife, DT_TFWeaponKnife )

BEGIN_NETWORK_TABLE( CTFKnife, DT_TFWeaponKnife )
#if defined( CLIENT_DLL )
	RecvPropBool( RECVINFO( m_bReadyToBackstab ) ),
#else
	SendPropBool( SENDINFO( m_bReadyToBackstab ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFKnife )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_bReadyToBackstab, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_knife, CTFKnife );
PRECACHE_WEAPON_REGISTER( tf_weapon_knife );

//=============================================================================
//
// Weapon Knife functions.
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFKnife::CTFKnife()
{
	m_bReadyToBackstab = false;

	m_bWasTaunting = false;
}

//-----------------------------------------------------------------------------
bool CTFKnife::DoSwingTrace( trace_t &trace )
{
	return BaseClass::DoSwingTrace( trace );
}

//-----------------------------------------------------------------------------
// Purpose: Set stealth attack bool
//-----------------------------------------------------------------------------
void CTFKnife::PrimaryAttack( void )
{
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );

	if ( !CanAttack() )
		return;

	// Set the weapon usage mode - primary, secondary.
	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;

	m_hBackstabVictim = NULL;
	int iBackstabVictimHealth = 0;

#if !defined (CLIENT_DLL)
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, pPlayer->GetCurrentCommand() );
#endif

	trace_t trace;
	if ( DoSwingTrace( trace ) == true )
	{
		// we will hit something with the attack
		if( trace.m_pEnt && trace.m_pEnt->IsPlayer() )
		{
			CTFPlayer *pTarget = ToTFPlayer( trace.m_pEnt );

			if ( pTarget && pTarget->GetTeamNumber() != pPlayer->GetTeamNumber() )
			{
				// Deal extra damage to players when stabbing them from behind
				if ( CanPerformBackstabAgainstTarget( pTarget ) )
				{
					// store the victim to compare when we do the damage
					m_hBackstabVictim.Set( pTarget );
					iBackstabVictimHealth = Max( m_hBackstabVictim->GetHealth(), 75 );
				}
			}
		} 
	}

#ifndef CLIENT_DLL
	pPlayer->RemoveInvisibility();
	pPlayer->RemoveDisguise();
	lagcompensation->FinishLagCompensation( pPlayer );
#endif

	// Swing the weapon.
	Swing( pPlayer );
	Smack();
	m_flSmackTime = -1.0f;

	m_bReadyToBackstab = false; // Hand is down.

#if !defined( CLIENT_DLL ) 
	pPlayer->SpeakWeaponFire();
	CTF_GameStats.Event_PlayerFiredWeapon( pPlayer, IsCurrentAttackACrit() );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Do backstab damage
//-----------------------------------------------------------------------------
float CTFKnife::GetMeleeDamage( CBaseEntity *pTarget, int* piCustomDamage )
{
	float flBaseDamage = BaseClass::GetMeleeDamage( pTarget, piCustomDamage );
	CTFPlayer *pTFOwner = ToTFPlayer( GetPlayerOwner() );
	if ( !pTFOwner )
		return false;

	if ( pTarget->IsPlayer() )
	{
		if ( IsBackstab() )
		{
			//CTFPlayer *pTFTarget = ToTFPlayer( pTarget );
			//// Special rules in modes where player power grows significantly
			//if ( !pTFOwner->IsBot() && pTFTarget && pTFTarget->IsMiniBoss() )
			//{
			//	// MvM: Cap damage against bots and check for a damage upgrade
			//	float flBonusDmg = 1.f;
			//	flBaseDamage = 250.f * flBonusDmg; 

			//	// Minibosses:  Adjust damage when backstabbing based on level of armor piercing
			//	// Base amount is 25% of normal damage.  Each level adds 25% to a cap of 125%.
			//	float flArmorPiercing = 25.f;
			//	CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pTFOwner, flArmorPiercing, armor_piercing );
			//	flBaseDamage *= clamp( flArmorPiercing / 100.0f, 0.25f, 1.25f );	
			//}
			//else // Regular game mode, or the attacker is a bot
			{
				// Do twice the target's health so that random modification will still kill him.
				flBaseDamage = pTarget->GetHealth() * 2; 
			}

			// Declare a backstab.
			*piCustomDamage = TF_DMG_CUSTOM_BACKSTAB;
		}
		else if (pTFOwner->m_Shared.IsCritBoosted())
		{
			m_bCurrentAttackIsCrit = true;
		}
		else
		{
			m_bCurrentAttackIsCrit = false;	// don't do a crit if we failed the above checks.
		}
	}

	return flBaseDamage;
}

//-----------------------------------------------------------------------------
// Purpose: Are we in a backstab position?
//-----------------------------------------------------------------------------
bool CTFKnife::CanPerformBackstabAgainstTarget( CTFPlayer *pTarget )
{
	if ( !pTarget )
		return false;

	// Immune?
	int iNoBackstab = 0;
	if ( iNoBackstab )
		return false;

	// Behind and facing target's back?
	if ( IsBehindAndFacingTarget( pTarget ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Determine if we are reasonably facing our target.
//-----------------------------------------------------------------------------
bool CTFKnife::IsBehindAndFacingTarget( CTFPlayer *pTarget )
{
	CTFPlayer *pOwner = ToTFPlayer( GetPlayerOwner() );
	if ( !pOwner )
		return false;

	// Get a vector from owner origin to target origin
	Vector vecToTarget;
	vecToTarget = pTarget->WorldSpaceCenter() - pOwner->WorldSpaceCenter();
	vecToTarget.z = 0.0f;
	vecToTarget.NormalizeInPlace();

	// Get owner forward view vector
	Vector vecOwnerForward;
	AngleVectors( pOwner->EyeAngles(), &vecOwnerForward, NULL, NULL );
	vecOwnerForward.z = 0.0f;
	vecOwnerForward.NormalizeInPlace();

	// Get target forward view vector
	Vector vecTargetForward;
	AngleVectors( pTarget->EyeAngles(), &vecTargetForward, NULL, NULL );
	vecTargetForward.z = 0.0f;
	vecTargetForward.NormalizeInPlace();

	// Make sure owner is behind, facing and aiming at target's back
	float flPosVsTargetViewDot = DotProduct( vecToTarget, vecTargetForward );	// Behind?
	float flPosVsOwnerViewDot = DotProduct( vecToTarget, vecOwnerForward );		// Facing?
	float flViewAnglesDot = DotProduct( vecTargetForward, vecOwnerForward );	// Facestab?

	// Debug
	// 	NDebugOverlay::HorzArrow( pTarget->WorldSpaceCenter(), pTarget->WorldSpaceCenter() + 50.0f * vecTargetForward, 5.0f, 0, 255, 0, 255, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
	// 	NDebugOverlay::HorzArrow( pOwner->WorldSpaceCenter(), pOwner->WorldSpaceCenter() + 50.0f * vecOwnerForward, 5.0f, 0, 255, 0, 255, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );
	// 	DevMsg( "PosDot: %3.2f FacingDot: %3.2f AnglesDot: %3.2f\n", flPosVsTargetViewDot, flPosVsOwnerViewDot, flViewAnglesDot );

	return ( flPosVsTargetViewDot > 0.f && flPosVsOwnerViewDot > 0.5 && flViewAnglesDot > -0.3f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFKnife::CalcIsAttackCriticalHelper( void )
{
	// Always crit from behind, never from front
	return IsBackstab();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFKnife::CalcIsAttackCriticalHelperNoCrits( void )
{
	// Always crit from behind, never from front
	return IsBackstab();
}

//-----------------------------------------------------------------------------
// Purpose: Allow melee weapons to send different anim events
// Input  :  - 
//-----------------------------------------------------------------------------
void CTFKnife::SendPlayerAnimEvent( CTFPlayer *pPlayer )
{
	if ( IsBackstab() )
	{
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_GESTURE, ACT_MP_ATTACK_STAND_SECONDARYFIRE );
	}
	else
	{
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFKnife::Deploy( void )
{
	bool bDeployed = BaseClass::Deploy();

	m_bReadyToBackstab = false;

	return bDeployed;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFKnife::ItemPreFrame( void )
{
	BaseClass::ItemPreFrame();
	ProcessDisguiseImpulse();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFKnife::ItemPostFrame( void )
{
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( pPlayer )
	{
		if ( pPlayer->m_Shared.InCond( TF_COND_TAUNTING ) )
		{
			m_bWasTaunting = true;
		}
		else if ( m_bWasTaunting )
		{
			// we were taunting and now we're not
			m_bWasTaunting = false;

			// force switch away from knife if we can't deploy it right now
			if ( !CanDeploy() )
			{
				// force switch to sapper
				CBaseCombatWeapon *mySapper = pPlayer->Weapon_GetWeaponByType( TF_WPN_TYPE_BUILDING );
				if ( !mySapper )
				{
					// this should never happen
					pPlayer->SwitchToNextBestWeapon( this );
				}
				else
				{
					pPlayer->Weapon_Switch( mySapper );
				}
			}
		}
	}

	BackstabVMThink();
	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFKnife::ItemBusyFrame( void )
{
	BaseClass::ItemBusyFrame();
	ProcessDisguiseImpulse();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFKnife::ItemHolsterFrame( void )
{
	BaseClass::ItemHolsterFrame();
	ProcessDisguiseImpulse();
}

//-----------------------------------------------------------------------------
// Purpose:  Special case handling of 'Your Eternal Reward' input polling
//-----------------------------------------------------------------------------
void CTFKnife::ProcessDisguiseImpulse( void )
{
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	// If we're not already disguised, ignore
	if ( !pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
		return;

	pPlayer->m_Shared.ProcessDisguiseImpulse( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFKnife::BackstabVMThink( void )
{
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	if ( pPlayer->GetActiveWeapon() != this )
		return;

	// Don't do this if we are doing something other than idling.
//	int iIdealActivity = GetIdealActivity();
//	if ( (iIdealActivity != ACT_VM_IDLE) && (iIdealActivity != ACT_BACKSTAB_VM_IDLE) )
//		return;
	int iActivity = GetActivity();
	if ( (iActivity != ACT_VM_IDLE) && (iActivity != ACT_BACKSTAB_VM_IDLE) && (iActivity != ACT_MELEE_VM_IDLE) && 
		 (iActivity != ACT_ITEM1_VM_IDLE) && (iActivity != ACT_ITEM1_BACKSTAB_VM_IDLE) &&
		 (iActivity != ACT_ITEM2_VM_IDLE) && (iActivity != ACT_ITEM2_BACKSTAB_VM_IDLE) )
		return;


	// Are we in backstab range and not cloaked?
	trace_t trace;
	if ( DoSwingTrace( trace ) == true && CanAttack() )
	{
		// We will hit something if we attack.
		if( trace.m_pEnt && trace.m_pEnt->IsPlayer() )
		{
			CTFPlayer *pTarget = ToTFPlayer( trace.m_pEnt );

			if ( pTarget && pTarget->GetTeamNumber() != pPlayer->GetTeamNumber() )
			{
				if ( CanPerformBackstabAgainstTarget( pTarget ) )
				{
					if ( !m_bReadyToBackstab )
					{
						SendWeaponAnim( ACT_BACKSTAB_VM_UP );

						m_bReadyToBackstab = true;
					}
				}
				else if ( m_bReadyToBackstab )
				{

					SendWeaponAnim( ACT_BACKSTAB_VM_DOWN );

					m_bReadyToBackstab = false;
				}
			}
		} 
	}
	else if ( m_bReadyToBackstab )
	{
		SendWeaponAnim( ACT_BACKSTAB_VM_DOWN );
		m_bReadyToBackstab = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play animation appropriate to ball status.
//-----------------------------------------------------------------------------
bool CTFKnife::SendWeaponAnim( int iActivity )
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return BaseClass::SendWeaponAnim( iActivity );

	if ( m_bReadyToBackstab )
	{
		switch ( iActivity )
		{
		case ACT_VM_IDLE:
		case ACT_ITEM1_VM_IDLE:
		case ACT_ITEM2_VM_IDLE:
			iActivity = ACT_BACKSTAB_VM_IDLE;
			break;
		}
	}

	return BaseClass::SendWeaponAnim( iActivity );
}