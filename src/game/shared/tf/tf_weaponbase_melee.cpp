//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weaponbase_melee.h"
#include "effect_dispatch_data.h"
#include "tf_gamerules.h"

// Server specific.
#if !defined( CLIENT_DLL )
#include "tf_player.h"
#include "tf_gamestats.h"
#include "ilagcompensationmanager.h"
// Client specific.
#else
#include "c_tf_player.h"
#endif

//=============================================================================
//
// TFWeaponBase Melee tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFWeaponBaseMelee, DT_TFWeaponBaseMelee )

BEGIN_NETWORK_TABLE( CTFWeaponBaseMelee, DT_TFWeaponBaseMelee )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFWeaponBaseMelee )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weaponbase_melee, CTFWeaponBaseMelee );

// Server specific.
#if !defined( CLIENT_DLL ) 
BEGIN_DATADESC( CTFWeaponBaseMelee )
DEFINE_THINKFUNC( Smack )
END_DATADESC()
#endif

#ifndef CLIENT_DLL
ConVar tf_meleeattackforcescale( "tf_meleeattackforcescale", "80.0", FCVAR_CHEAT | FCVAR_GAMEDLL | FCVAR_DEVELOPMENTONLY );
#endif

ConVar tf_weapon_criticals_melee( "tf_weapon_criticals_melee", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Controls random crits for melee weapons.\n0 - Melee weapons do not randomly crit. \n1 - Melee weapons can randomly crit only if tf_weapon_criticals is also enabled. \n2 - Melee weapons can always randomly crit regardless of the tf_weapon_criticals setting.", true, 0, true, 2 );
extern ConVar tf_weapon_criticals;

#ifdef _DEBUG
extern ConVar tf_weapon_criticals_force_random;
#endif // _DEBUG

//=============================================================================
//
// TFWeaponBase Melee functions.
//

// -----------------------------------------------------------------------------
// Purpose: Constructor.
// -----------------------------------------------------------------------------
CTFWeaponBaseMelee::CTFWeaponBaseMelee()
{
	WeaponReset();
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::WeaponReset( void )
{
	BaseClass::WeaponReset();

	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;
	m_flSmackTime = -1.0f;
	m_bConnected = false;
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::Precache()
{
	BaseClass::Precache();
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::Spawn()
{
	Precache();

	// Get the weapon information.
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( GetClassname() );
	Assert( hWpnInfo != GetInvalidWeaponInfoHandle() );
	CTFWeaponInfo *pWeaponInfo = dynamic_cast< CTFWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );
	Assert( pWeaponInfo && "Failed to get CTFWeaponInfo in melee weapon spawn" );
	m_pWeaponInfo = pWeaponInfo;
	Assert( m_pWeaponInfo );

	// No ammo.
	m_iClip1 = -1;

	BaseClass::Spawn();
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
bool CTFWeaponBaseMelee::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_flSmackTime = -1.0f;
	if ( GetPlayerOwner() )
	{
		GetPlayerOwner()->m_flNextAttack = gpGlobals->curtime + 0.5;
	}
	return BaseClass::Holster( pSwitchingTo );
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::PrimaryAttack()
{
	// Get the current player.
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return;

	if ( !CanAttack() )
		return;

	// Set the weapon usage mode - primary, secondary.
	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;
	m_bConnected = false;

	// Swing the weapon.
	Swing( pPlayer );

#if !defined( CLIENT_DLL ) 
	pPlayer->SpeakWeaponFire();
	CTF_GameStats.Event_PlayerFiredWeapon( pPlayer, IsCurrentAttackACritical() );

	if ( pPlayer->m_Shared.IsStealthed() )
	{
		pPlayer->RemoveInvisibility();
	}
#endif
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::SecondaryAttack()
{
	// semi-auto behaviour
	if ( m_bInAttack2 )
		return;

	// Get the current player.
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return;

	pPlayer->DoClassSpecialSkill();

	m_bInAttack2 = true;

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CTFWeaponBaseMelee::Swing( CTFPlayer *pPlayer )
{
	CalcIsAttackCritical();

#ifdef GAME_DLL
	CTF_GameStats.Event_PlayerFiredWeapon( pPlayer, IsCurrentAttackACrit() );
#endif

	// Play the melee swing and miss (whoosh) always.
	SendPlayerAnimEvent( pPlayer );

	DoViewModelAnimation();

	// Set next attack times.
	float flFireDelay = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeFireDelay;

	m_flNextPrimaryAttack = gpGlobals->curtime + flFireDelay;
	m_flNextSecondaryAttack = gpGlobals->curtime + flFireDelay;
	//pPlayer->m_Shared.SetNextStealthTime( m_flNextSecondaryAttack ); //SEALTODO

	SetWeaponIdleTime( m_flNextPrimaryAttack + m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeIdleEmpty );
	
	if ( IsCurrentAttackACrit() )
	{
		WeaponSound( BURST );
	}
	else
	{
		WeaponSound( MELEE_MISS );
	}

	m_flSmackTime = gpGlobals->curtime + m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flSmackDelay;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBaseMelee::DoViewModelAnimation( void )
{
	if ( IsCurrentAttackACrit() )
	{
		if ( SendWeaponAnim( ACT_VM_SWINGHARD ) )
		{
			// check that weapon has the activity
			return;
		}
	}

	Activity act = ( m_iWeaponMode == TF_WEAPON_PRIMARY_MODE ) ? ACT_VM_HITCENTER : ACT_VM_SWINGHARD;

	SendWeaponAnim( act );
}

//-----------------------------------------------------------------------------
// Purpose: Allow melee weapons to send different anim events
// Input  :  - 
//-----------------------------------------------------------------------------
void CTFWeaponBaseMelee::SendPlayerAnimEvent( CTFPlayer *pPlayer )
{
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CTFWeaponBaseMelee::ItemPostFrame()
{
	// Check for smack.
	if ( m_flSmackTime > 0.0f && gpGlobals->curtime > m_flSmackTime )
	{
		Smack();
		m_flSmackTime = -1.0f;
	}

	BaseClass::ItemPostFrame();
}

bool CTFWeaponBaseMelee::DoSwingTrace( trace_t &trace )
{
	// Setup a volume for the melee weapon to be swung - approx size, so all melee behave the same.
	static Vector vecSwingMins( -18, -18, -18 );
	static Vector vecSwingMaxs( 18, 18, 18 );

	// Get the current player.
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return false;

	// Setup the swing range.
	float fSwingRange = 48.0f;

	// Scale the range and bounds by the model scale if they're larger
	// Not scaling down the range for smaller models because midgets need all the help they can get
	if ( pPlayer->GetModelScale() > 1.0f )
	{
		fSwingRange *= pPlayer->GetModelScale();
		vecSwingMins *= pPlayer->GetModelScale();
		vecSwingMaxs *= pPlayer->GetModelScale();
	}

	Vector vecForward; 
	AngleVectors( pPlayer->EyeAngles(), &vecForward );
	Vector vecSwingStart = pPlayer->Weapon_ShootPosition();
	Vector vecSwingEnd = vecSwingStart + vecForward * fSwingRange;

	// See if we hit anything.
	UTIL_TraceLine( vecSwingStart, vecSwingEnd, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &trace );
	if ( trace.fraction >= 1.0 )
	{
		UTIL_TraceHull( vecSwingStart, vecSwingEnd, vecSwingMins, vecSwingMaxs, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &trace );
		if ( trace.fraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = trace.m_pEnt;
			if ( !pHit || pHit->IsBSPModel() )
			{
				// Why duck hull min/max?
				FindHullIntersection( vecSwingStart, trace, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, pPlayer );
			}

			// This is the point on the actual surface (the hull could have hit space)
			vecSwingEnd = trace.endpos;	
		}
	}

	return ( trace.fraction < 1.0f );
}

// -----------------------------------------------------------------------------
// Purpose:
// Note: Think function to delay the impact decal until the animation is finished 
//       playing.
// -----------------------------------------------------------------------------
void CTFWeaponBaseMelee::Smack( void )
{
	trace_t trace;

	CBasePlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

#if !defined (CLIENT_DLL)
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, pPlayer->GetCurrentCommand() );
#endif

	// We hit, setup the smack.
	if ( DoSwingTrace( trace ) )
	{
		// Hit sound - immediate.
		if( trace.m_pEnt->IsPlayer()  )
		{
			WeaponSound( MELEE_HIT );
		}
		else
		{
			WeaponSound( MELEE_HIT_WORLD );
		}

		DoMeleeDamage( trace.m_pEnt, trace );
	}

#if !defined (CLIENT_DLL)
	lagcompensation->FinishLagCompensation( pPlayer );
#endif
}

void CTFWeaponBaseMelee::DoMeleeDamage( CBaseEntity* ent, trace_t& trace )
{
	DoMeleeDamage( ent, trace, 1.f );
}

void CTFWeaponBaseMelee::DoMeleeDamage( CBaseEntity* ent, trace_t& trace, float flDamageMod )
{
	// Get the current player.
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return;

	Vector vecForward; 
	AngleVectors( pPlayer->EyeAngles(), &vecForward );
	Vector vecSwingStart = pPlayer->Weapon_ShootPosition();
	Vector vecSwingEnd = vecSwingStart + vecForward * 48;

#ifndef CLIENT_DLL
	// Do Damage.
	int iCustomDamage = GetDamageCustom();
	int iDmgType = DMG_MELEE | DMG_NEVERGIB | DMG_CLUB;

	float flDamage = GetMeleeDamage( ent, &iCustomDamage ) * flDamageMod;

	if ( IsCurrentAttackACrit() )
	{
		// TODO: Not removing the old critical path yet, but the new custom damage is marking criticals as well for melee now.
		iDmgType |= DMG_CRITICAL;
	}

	CTakeDamageInfo info( pPlayer, pPlayer, this, flDamage, iDmgType, iCustomDamage );

	if ( fabs( flDamage ) >= 1.0f )
	{
		CalculateMeleeDamageForce( &info, vecForward, vecSwingEnd, 1.0f / flDamage * GetForceScale() );
	}
	else
	{
		info.SetDamageForce( vec3_origin );
	}
	
	ent->DispatchTraceAttack( info, vecForward, &trace ); 
	ApplyMultiDamage();

	OnEntityHit( ent );

#endif
	// Don't impact trace friendly players or objects
	if ( ent && ent->GetTeamNumber() != pPlayer->GetTeamNumber() )
	{
#ifdef CLIENT_DLL
		UTIL_ImpactTrace( &trace, DMG_CLUB );
#endif
		m_bConnected = true;
	}
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CTFWeaponBaseMelee::GetForceScale( void )
{
	return tf_meleeattackforcescale.GetFloat();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CTFWeaponBaseMelee::GetMeleeDamage( CBaseEntity *pTarget, int* piCustomDamage )
{
	return static_cast<float>( m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_nDamage );
}

void CTFWeaponBaseMelee::OnEntityHit( CBaseEntity *pEntity )
{
	NULL;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CTFWeaponBaseMelee::CalcIsAttackCriticalHelperNoCrits( void )
{
	// This function was called because the tf_weapon_criticals ConVar is off, but if
	// melee crits are set to be forced on, then call the regular crit helper function.
	if ( tf_weapon_criticals_melee.GetInt() > 1 )
	{
		return CalcIsAttackCriticalHelper();
	}

	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return false;

	return BaseClass::CalcIsAttackCriticalHelperNoCrits();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBaseMelee::CalcIsAttackCriticalHelper( void )
{
	// If melee crits are off, then check the NoCrits helper.
	if ( tf_weapon_criticals_melee.GetInt() == 0 )
	{
		return CalcIsAttackCriticalHelperNoCrits();
	}

	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return false;

	if ( !CanFireCriticalShot() )
		return false;

	// Crit boosted players fire all crits
	if ( pPlayer->m_Shared.IsCritBoosted() )
		return true;

	float flPlayerCritMult = pPlayer->GetCritMult();
	float flCritChance = TF_DAMAGE_CRIT_CHANCE_MELEE * flPlayerCritMult;

	// mess with the crit chance seed so it's not based solely on the prediction seed
	int iMask = ( entindex() << 16 ) | ( pPlayer->entindex() << 8 );
	int iSeed = CBaseEntity::GetPredictionRandomSeed() ^ iMask;
	if ( iSeed != m_iCurrentSeed )
	{
		m_iCurrentSeed = iSeed;
		RandomSeed( m_iCurrentSeed );
	}

	// Regulate crit frequency to reduce client-side seed hacking
	float flDamage = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_nDamage;
	AddToCritBucket( flDamage );

	// Track each request
	m_nCritChecks++;

	bool bCrit = ( RandomInt( 0, WEAPON_RANDOM_RANGE-1 ) < ( flCritChance ) * WEAPON_RANDOM_RANGE );

#ifdef _DEBUG
	// Force seed to always say yes
	if ( tf_weapon_criticals_force_random.GetInt() )
	{
		bCrit = true;
	}
#endif // _DEBUG

	if ( bCrit )
	{
		// Seed says crit.  Run it by the manager.
		bCrit = IsAllowedToWithdrawFromCritBucket( flDamage );
	}

	return bCrit;
}
