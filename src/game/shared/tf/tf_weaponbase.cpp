//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
//	Weapons.
//
//=============================================================================
#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "tf_weaponbase.h"
#include "ammodef.h"
#include "tf_gamerules.h"
#include "eventlist.h"

// Server specific.
#if !defined( CLIENT_DLL )
#include "tf_player.h"
#include "tf_gamestats.h"
#include "ilagcompensationmanager.h"
#include "particle_parse.h"
// Client specific.
#else
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "c_tf_player.h"
#include "tf_viewmodel.h"
#include "hud_crosshair.h"
#include "c_tf_playerresource.h"
#include "clientmode_tf.h"
#include "r_efx.h"
#include "dlight.h"
#include "effect_dispatch_data.h"
#include "c_te_effect_dispatch.h"
#include "toolframework_client.h"
#include "prediction.h"

// needed for stupid check, sorry
#include "dod_viewmodel.h"

// for spy material proxy
#include "proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"

extern CTFWeaponInfo *GetTFWeaponInfo( int iWeapon );
#endif

ConVar tf_weapon_criticals( "tf_weapon_criticals", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Whether or not random crits are enabled." );
ConVar tf_weapon_criticals_nopred( "tf_weapon_criticals_nopred", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT );

#ifdef _DEBUG
ConVar tf_weapon_criticals_anticheat( "tf_weapon_criticals_anticheat", "1.0", FCVAR_REPLICATED );
ConVar tf_weapon_criticals_debug( "tf_weapon_criticals_debug", "0.0", FCVAR_REPLICATED );
extern ConVar tf_weapon_criticals_force_random;
#endif // _DEBUG
extern ConVar tf_weapon_criticals_bucket_cap;
extern ConVar tf_weapon_criticals_bucket_bottom;

extern ConVar tf_useparticletracers;

#ifdef CLIENT_DLL
extern ConVar cl_crosshair_file;
extern ConVar cl_flipviewmodels;
#endif

//=============================================================================
//
// TFWeaponBase tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFWeaponBase, DT_TFWeaponBase )

#ifdef GAME_DLL
void* SendProxy_SendActiveLocalWeaponDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );
void* SendProxy_SendNonLocalWeaponDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );
#endif

//-----------------------------------------------------------------------------
// Purpose: Only sent to the player holding it.
//-----------------------------------------------------------------------------
BEGIN_NETWORK_TABLE_NOBASE( CTFWeaponBase, DT_LocalTFWeaponData )
#if defined( CLIENT_DLL )
	RecvPropTime( RECVINFO( m_flLastCritCheckTime ) ),
	RecvPropTime( RECVINFO( m_flReloadPriorNextFire ) ),
	RecvPropTime( RECVINFO( m_flLastFireTime ) ),
	RecvPropFloat( RECVINFO( m_flObservedCritChance ) ),
#else
	SendPropTime( SENDINFO( m_flLastCritCheckTime ) ),
	SendPropTime( SENDINFO( m_flReloadPriorNextFire ) ),
	SendPropTime( SENDINFO( m_flLastFireTime ) ),
	SendPropFloat( SENDINFO( m_flObservedCritChance ), 16, SPROP_NOSCALE, 0.0, 100.0 ),
#endif
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Variables sent at low precision to non-holding observers.
//-----------------------------------------------------------------------------
BEGIN_NETWORK_TABLE_NOBASE( CTFWeaponBase, DT_TFWeaponDataNonLocal )
END_NETWORK_TABLE()

BEGIN_NETWORK_TABLE( CTFWeaponBase, DT_TFWeaponBase )
// Client specific.
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bLowered ) ),
	RecvPropInt( RECVINFO( m_iReloadMode ) ),
	RecvPropBool( RECVINFO( m_bResetParity ) ), 
	RecvPropBool( RECVINFO( m_bReloadedThroughAnimEvent ) ),
	RecvPropBool( RECVINFO( m_bDisguiseWeapon ) ),
	RecvPropDataTable("LocalActiveTFWeaponData", 0, 0, &REFERENCE_RECV_TABLE(DT_LocalTFWeaponData)),
	RecvPropDataTable("NonLocalTFWeaponData", 0, 0, &REFERENCE_RECV_TABLE(DT_TFWeaponDataNonLocal)),
// Server specific.
#else
	SendPropBool( SENDINFO( m_bLowered ) ),
	SendPropBool( SENDINFO( m_bResetParity ) ),
	SendPropInt( SENDINFO( m_iReloadMode ), 4, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bReloadedThroughAnimEvent ) ),
	SendPropBool( SENDINFO( m_bDisguiseWeapon ) ),
	SendPropDataTable("LocalActiveTFWeaponData", 0, &REFERENCE_SEND_TABLE(DT_LocalTFWeaponData), SendProxy_SendActiveLocalWeaponDataTable ),
	SendPropDataTable("NonLocalTFWeaponData", 0, &REFERENCE_SEND_TABLE(DT_TFWeaponDataNonLocal), SendProxy_SendNonLocalWeaponDataTable ),

	// World models have no animations so don't send these.
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_AnimTimeMustBeFirst", "m_flAnimTime" ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFWeaponBase )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_bLowered, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iReloadMode, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bReloadedThroughAnimEvent, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDisguiseWeapon, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flLastCritCheckTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),	
	DEFINE_PRED_FIELD_TOL( m_flReloadPriorNextFire, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),	
	DEFINE_PRED_FIELD_TOL( m_flLastFireTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_bCurrentAttackIsCrit, FIELD_BOOLEAN, 0 ),
	DEFINE_PRED_FIELD( m_iCurrentSeed, FIELD_INTEGER, 0 ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_base, CTFWeaponBase );

// Server specific.
#if !defined( CLIENT_DLL )

BEGIN_DATADESC( CTFWeaponBase )
DEFINE_FUNCTION( FallThink )
END_DATADESC()

// Client specific
#else

ConVar cl_crosshaircolor( "cl_crosshaircolor", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
ConVar cl_dynamiccrosshair( "cl_dynamiccrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
ConVar cl_scalecrosshair( "cl_scalecrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
ConVar cl_crosshairalpha( "cl_crosshairalpha", "200", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );

int g_iScopeTextureID = 0;
int g_iScopeDustTextureID = 0;

#endif

//=============================================================================
//
// TFWeaponBase shared functions.
//

// -----------------------------------------------------------------------------
// Purpose: Constructor.
// -----------------------------------------------------------------------------
CTFWeaponBase::CTFWeaponBase()
{
	SetPredictionEligible( true );

	// Nothing collides with these, but they get touch calls.
	AddSolidFlags( FSOLID_TRIGGER );

	// Weapons can fire underwater.
	m_bFiresUnderwater = true;
	m_bAltFiresUnderwater = true;

	// Initialize the weapon modes.
	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;
	m_iReloadMode.Set( TF_RELOAD_START );

	m_iAltFireHint = 0;
	m_bInAttack = false;
	m_bInAttack2 = false;

	m_flCritTime = 0;
	m_flLastCritCheckTime = 0;
	m_flLastRapidFireCritCheckTime = 0;
	m_iLastCritCheckFrame = 0;
	m_flObservedCritChance = 0.f;
	m_flLastFireTime = 0;
	m_bCurrentAttackIsCrit = false;
	m_bCurrentCritIsRandom = false;
	m_iCurrentSeed = -1;
	m_flReloadPriorNextFire = 0;

	m_flLastDeployTime = 0;

	m_bDisguiseWeapon = false;

	m_flLastPrimaryAttackTime = 0.f;
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBase::Spawn()
{
	// Base class spawn.
	BaseClass::Spawn();

	// Set this here to allow players to shoot dropped weapons.
	SetCollisionGroup( COLLISION_GROUP_WEAPON );

	// Get the weapon information.
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( GetClassname() );
	Assert( hWpnInfo != GetInvalidWeaponInfoHandle() );
	CTFWeaponInfo *pWeaponInfo = dynamic_cast<CTFWeaponInfo*>( GetFileWeaponInfoFromHandle( hWpnInfo ) );
	Assert( pWeaponInfo && "Failed to get CTFWeaponInfo in weapon spawn" );
	m_pWeaponInfo = pWeaponInfo;

	if ( GetPlayerOwner() )
	{
		ChangeTeam( GetPlayerOwner()->GetTeamNumber() );
	}

#ifdef GAME_DLL
	// Move it up a little bit, otherwise it'll be at the guy's feet, and its sound origin 
	// will be in the ground so its EmitSound calls won't do anything.
	Vector vecOrigin = GetAbsOrigin();
	SetAbsOrigin( Vector( vecOrigin.x, vecOrigin.y, vecOrigin.z + 5.0f ) );
#endif

	m_szTracerName[0] = '\0';
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBase::FallInit( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CTFWeaponBase::Precache()
{
	BaseClass::Precache();

	if ( GetMuzzleFlashModel() )
	{
		PrecacheModel( GetMuzzleFlashModel() );
	}

	const CTFWeaponInfo *pTFInfo = &GetPCWpnData();

	if ( pTFInfo->m_ExtraTFWeaponData.m_szExplosionSound && pTFInfo->m_ExtraTFWeaponData.m_szExplosionSound[0] )
	{
		CBaseEntity::PrecacheScriptSound( pTFInfo->m_ExtraTFWeaponData.m_szExplosionSound );
	}

	if ( pTFInfo->m_ExtraTFWeaponData.m_szBrassModel[0] )
	{
		PrecacheModel( pTFInfo->m_ExtraTFWeaponData.m_szBrassModel );
	}

	if ( pTFInfo->m_ExtraTFWeaponData.m_szMuzzleFlashParticleEffect && pTFInfo->m_ExtraTFWeaponData.m_szMuzzleFlashParticleEffect[0] )
	{
		PrecacheParticleSystem( pTFInfo->m_ExtraTFWeaponData.m_szMuzzleFlashParticleEffect );
	}

	if ( pTFInfo->m_ExtraTFWeaponData.m_szExplosionEffect && pTFInfo->m_ExtraTFWeaponData.m_szExplosionEffect[0] )
	{
		PrecacheParticleSystem( pTFInfo->m_ExtraTFWeaponData.m_szExplosionEffect );
	}

	if ( pTFInfo->m_ExtraTFWeaponData.m_szExplosionPlayerEffect && pTFInfo->m_ExtraTFWeaponData.m_szExplosionPlayerEffect[0] )
	{
		PrecacheParticleSystem( pTFInfo->m_ExtraTFWeaponData.m_szExplosionPlayerEffect );
	}

	if ( pTFInfo->m_ExtraTFWeaponData.m_szExplosionWaterEffect && pTFInfo->m_ExtraTFWeaponData.m_szExplosionWaterEffect[0] )
	{
		PrecacheParticleSystem( pTFInfo->m_ExtraTFWeaponData.m_szExplosionWaterEffect );
	}

	if ( pTFInfo->m_ExtraTFWeaponData.m_szTracerEffect && pTFInfo->m_ExtraTFWeaponData.m_szTracerEffect[0] )
	{
		char pTracerEffect[128];
		char pTracerEffectCrit[128];

		Q_snprintf( pTracerEffect, sizeof(pTracerEffect), "%s_red", pTFInfo->m_ExtraTFWeaponData.m_szTracerEffect );
		Q_snprintf( pTracerEffectCrit, sizeof(pTracerEffectCrit), "%s_red_crit", pTFInfo->m_ExtraTFWeaponData.m_szTracerEffect );
		PrecacheParticleSystem( pTracerEffect );
		PrecacheParticleSystem( pTracerEffectCrit );

		Q_snprintf( pTracerEffect, sizeof(pTracerEffect), "%s_blue", pTFInfo->m_ExtraTFWeaponData.m_szTracerEffect );
		Q_snprintf( pTracerEffectCrit, sizeof(pTracerEffectCrit), "%s_blue_crit", pTFInfo->m_ExtraTFWeaponData.m_szTracerEffect );
		PrecacheParticleSystem( pTracerEffect );
		PrecacheParticleSystem( pTracerEffectCrit );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBase::Drop( const Vector &vecVelocity )
{
#ifndef CLIENT_DLL
	if ( m_iAltFireHint )
	{
		CBasePlayer *pPlayer = GetPlayerOwner();
		if ( pPlayer )
		{
			pPlayer->StopHintTimer( m_iAltFireHint );
		}
	}
#endif

	BaseClass::Drop( vecVelocity );

#ifndef CLIENT_DLL
	// Never allow weapons to lie around on the ground
	UTIL_Remove( this );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	if ( m_iAltFireHint )
	{
		CBasePlayer *pPlayer = GetPlayerOwner();
		if ( pPlayer )
		{
			pPlayer->StopHintTimer( m_iAltFireHint );
		}
	}
#endif

	m_iReloadMode.Set( TF_RELOAD_START );

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::Deploy( void )
{
#ifndef CLIENT_DLL
	if ( m_iAltFireHint )
	{
		CBasePlayer *pPlayer = GetPlayerOwner();
		if ( pPlayer )
		{
			pPlayer->StartHintTimer( m_iAltFireHint );
		}
	}
#endif
	m_iReloadMode.Set( TF_RELOAD_START );

	float flOriginalPrimaryAttack = m_flNextPrimaryAttack;
	float flOriginalSecondaryAttack = m_flNextSecondaryAttack;

	bool bDeploy = BaseClass::Deploy();

	if ( bDeploy )
	{
		CTFPlayer *pPlayer = ToTFPlayer( GetOwner() );
		if ( !pPlayer )
			return false;

		float flWeaponSwitchTime = 0.5f;

		// Overrides the anim length for calculating ready time.
		float flDeployTimeMultiplier = 1.0f;
		
		flDeployTimeMultiplier = MAX( flDeployTimeMultiplier, 0.00001f );
		float flDeployTime = flWeaponSwitchTime * flDeployTimeMultiplier;
		float flPlaybackRate = Clamp( ( 1.f / flDeployTimeMultiplier ) * ( 0.67f / flWeaponSwitchTime ), -4.f, 12.f ); // clamp between the range that's defined in send table
		if ( pPlayer->GetViewModel(0) )
		{
			pPlayer->GetViewModel(0)->SetPlaybackRate( flPlaybackRate );
		}
		if ( pPlayer->GetViewModel(1) )
		{
			pPlayer->GetViewModel(1)->SetPlaybackRate( flPlaybackRate );
		}
		
		// Don't override primary attacks that are already further out than this. This prevents
		// people exploiting weapon switches to allow weapons to fire faster.
		m_flNextPrimaryAttack = MAX( flOriginalPrimaryAttack, gpGlobals->curtime + flDeployTime );
		m_flNextSecondaryAttack = MAX( flOriginalSecondaryAttack, m_flNextPrimaryAttack.Get() );

		pPlayer->SetNextAttack( m_flNextPrimaryAttack );

		m_flLastDeployTime = gpGlobals->curtime;
	}

	return bDeploy;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
void CTFWeaponBase::PrimaryAttack( void )
{
	// Set the weapon mode.
	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;

	if ( !CanAttack() )
		return;

	BaseClass::PrimaryAttack();

	if ( m_bReloadsSingly )
	{
		m_iReloadMode.Set( TF_RELOAD_START );
	}

	m_flLastPrimaryAttackTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CTFWeaponBase::SecondaryAttack( void )
{
	// Set the weapon mode.
	m_iWeaponMode = TF_WEAPON_SECONDARY_MODE;

	// Don't hook secondary for now.
	return;
}

//-----------------------------------------------------------------------------
// Purpose: Most calls use the prediction seed
//-----------------------------------------------------------------------------
void CTFWeaponBase::CalcIsAttackCritical( void)
{
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	if ( gpGlobals->framecount == m_iLastCritCheckFrame )
		return;
	m_iLastCritCheckFrame = gpGlobals->framecount;

	m_bCurrentCritIsRandom = false;

	if ( (TFGameRules()->State_Get() == GR_STATE_TEAM_WIN) && (TFGameRules()->GetWinningTeam() == pPlayer->GetTeamNumber()) )
	{
		m_bCurrentAttackIsCrit = true;
	}
	else if ( !AreRandomCritsEnabled() )
	{
		// Support critboosted even in no crit mode
		m_bCurrentAttackIsCrit = CalcIsAttackCriticalHelperNoCrits();
	}
	else
	{
		// call the weapon-specific helper method
		m_bCurrentAttackIsCrit = CalcIsAttackCriticalHelper();
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::CalcIsAttackCriticalHelperNoCrits()
{
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return false;

	return pPlayer->m_Shared.IsCritBoosted();
}

//-----------------------------------------------------------------------------
// Purpose: Weapon-specific helper method to calculate if attack is crit
//-----------------------------------------------------------------------------
bool CTFWeaponBase::CalcIsAttackCriticalHelper()
{
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return false;

	float flCritChance = 0.f;
	float flPlayerCritMult = pPlayer->GetCritMult();

	if ( !CanFireCriticalShot() )
		return false;

	// Crit boosted players fire all crits
	if ( pPlayer->m_Shared.IsCritBoosted() )
		return true;

	// For rapid fire weapons, allow crits while period is active
	bool bRapidFire = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_bUseRapidFireCrits;
	if ( bRapidFire && m_flCritTime > gpGlobals->curtime )
		return true;

	// --- Random crits from this point on ---
	
	// Monitor and enforce short-term random crit rate - via bucket

	// Figure out how much to add/remove from token bucket
	int nProjectilesPerShot = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_nBulletsPerShot;

	// Damage
	float flDamage = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_nDamage;
	flDamage *= nProjectilesPerShot;
	AddToCritBucket( flDamage );

	bool bCrit = false;
	m_bCurrentCritIsRandom = true;
	int iRandom = 0;

	if ( bRapidFire )
	{
		// only perform one crit check per second for rapid fire weapons
		if ( tf_weapon_criticals_nopred.GetBool() )
		{
			if ( gpGlobals->curtime < m_flLastRapidFireCritCheckTime + 1.f )
				return false;

			m_flLastRapidFireCritCheckTime = gpGlobals->curtime;
		}
		else
		{
			if ( gpGlobals->curtime < m_flLastCritCheckTime + 1.f )
				return false;

			m_flLastCritCheckTime = gpGlobals->curtime;
		}

		// get the total crit chance (ratio of total shots fired we want to be crits)
		float flTotalCritChance = clamp( TF_DAMAGE_CRIT_CHANCE_RAPID * flPlayerCritMult, 0.01f, 0.99f );
		// get the fixed amount of time that we start firing crit shots for	
		float flCritDuration = TF_DAMAGE_CRIT_DURATION_RAPID;
		// calculate the amount of time, on average, that we want to NOT fire crit shots for in order to achieve the total crit chance we want
		float flNonCritDuration = ( flCritDuration / flTotalCritChance ) - flCritDuration;
		// calculate the chance per second of non-crit fire that we should transition into critting such that on average we achieve the total crit chance we want
		float flStartCritChance = 1 / flNonCritDuration;

		// if base entity seed has changed since last calculation, reseed with new seed
		int iMask = ( entindex() << 8 ) | ( pPlayer->entindex() );
		int iSeed = CBaseEntity::GetPredictionRandomSeed() ^ iMask;
		if ( iSeed != m_iCurrentSeed )
		{
			m_iCurrentSeed = iSeed;
			RandomSeed( m_iCurrentSeed );
		}

		// see if we should start firing crit shots
		iRandom = RandomInt( 0, WEAPON_RANDOM_RANGE-1 );
		if ( iRandom < flStartCritChance * WEAPON_RANDOM_RANGE )
		{
			bCrit = true;
			flCritChance = flStartCritChance;
		}
	}
	else
	{
		// single-shot weapon, just use random pct per shot
		flCritChance = TF_DAMAGE_CRIT_CHANCE * flPlayerCritMult;

		// mess with the crit chance seed so it's not based solely on the prediction seed
		int iMask = ( entindex() << 8 ) | ( pPlayer->entindex() );
		int iSeed = CBaseEntity::GetPredictionRandomSeed() ^ iMask;
		if ( iSeed != m_iCurrentSeed )
		{
			m_iCurrentSeed = iSeed;
			RandomSeed( m_iCurrentSeed );
		}

		iRandom = RandomInt( 0, WEAPON_RANDOM_RANGE - 1 );
		bCrit = ( iRandom < flCritChance * WEAPON_RANDOM_RANGE );
	}

#ifdef _DEBUG
	if ( tf_weapon_criticals_debug.GetBool() )
	{
#ifdef GAME_DLL
		DevMsg( "Roll (server): %i out of %f (crit: %d)\n", iRandom, ( flCritChance * WEAPON_RANDOM_RANGE ), bCrit );
#else
		if ( prediction->IsFirstTimePredicted() )
		{
			DevMsg( "\tRoll (client): %i out of %f (crit: %d)\n", iRandom, ( flCritChance * WEAPON_RANDOM_RANGE ), bCrit );
		}
#endif // GAME_DLL
	}

	// Force seed to always say yes
	if ( tf_weapon_criticals_force_random.GetInt() )
	{
		bCrit = true;
	}
#endif // _DEBUG
	
	// Track each check
#ifdef GAME_DLL
	m_nCritChecks++;
#else
	if ( prediction->IsFirstTimePredicted() )
	{
		m_nCritChecks++;
	}
#endif // GAME_DLL

	// Seed says crit.  Run it by the manager.
	if ( bCrit )
	{
		bool bAntiCheat = true;
#ifdef _DEBUG
		bAntiCheat = tf_weapon_criticals_anticheat.GetBool();
#endif // _DEBUG

		// Monitor and enforce long-term random crit rate - via stats
		if ( bAntiCheat )
		{
			if ( !CanFireRandomCriticalShot( flCritChance ) )
				return false;

			// Make sure rapid fire weapons can pay the cost of the entire period up-front
			if ( bRapidFire )
			{
				flDamage *= TF_DAMAGE_CRIT_DURATION_RAPID / m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeFireDelay;

				// Never try to drain more than cap
				int nBucketCap = tf_weapon_criticals_bucket_cap.GetInt();
				if ( flDamage * TF_DAMAGE_CRIT_MULTIPLIER > nBucketCap )
					flDamage = (float)nBucketCap / TF_DAMAGE_CRIT_MULTIPLIER;
			}

			bCrit = IsAllowedToWithdrawFromCritBucket( flDamage );
		}

		if ( bCrit && bRapidFire )
		{
			m_flCritTime = gpGlobals->curtime + TF_DAMAGE_CRIT_DURATION_RAPID;
		}
	}

	return bCrit;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFWeaponBase::Reload( void )
{
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return false;

	// If we're not already reloading, check to see if we have ammo to reload and check to see if we are max ammo.
	if ( m_iReloadMode == TF_RELOAD_START ) 
	{
		// If I don't have any spare ammo, I can't reload
		if ( GetOwner()->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
			return false;

		if ( Clip1() >= GetMaxClip1())
			return false;
	}

	// Reload one object at a time.
	if ( m_bReloadsSingly )
		return ReloadSingly();

	// Normal reload.
	DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBase::AbortReload( void )
{
	BaseClass::AbortReload();

	m_iReloadMode.Set( TF_RELOAD_START );

	// Make sure our reloading bodygroup is hidden (shells/grenades/etc)
	int indexR = FindBodygroupByName( "reload" );
	if ( indexR >= 0 )
	{
		SetBodygroup( indexR, 0 );
	}
}

//-----------------------------------------------------------------------------
// Is the weapon reloading right now?
bool CTFWeaponBase::IsReloading() const
{
	return m_iReloadMode != TF_RELOAD_START;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFWeaponBase::ReloadSingly( void )
{
	// Don't reload.
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return false;

	// Get the current player.
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return false;

	// check to see if we're ready to reload
	switch ( m_iReloadMode )
	{
	case TF_RELOAD_START:
		{
			// Play weapon and player animations.
			if ( SendWeaponAnim( ACT_RELOAD_START ) )
			{
				SetReloadTimer( SequenceDuration() );
			}
			else
			{
				// Update the reload timers with script values.
				UpdateReloadTimers( true );
			}

			// Next reload the shells.
			m_iReloadMode.Set( TF_RELOADING );

			m_iReloadStartClipAmount = Clip1();

			return true;
		}
	case TF_RELOADING:
		{
			// Did we finish the reload start?  Now we can reload a rocket.
			if ( m_flTimeWeaponIdle > gpGlobals->curtime )
				return false;

			// Play weapon reload animations and sound.
			if ( Clip1() == m_iReloadStartClipAmount )
			{
				pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD );
			}
			else
			{
				pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_LOOP );
			}

			m_bReloadedThroughAnimEvent = false;

			if ( SendWeaponAnim( ACT_VM_RELOAD ) )
			{
				if ( GetWeaponID() == TF_WEAPON_GRENADELAUNCHER )
				{
					SetReloadTimer( GetPCWpnData().m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeReload );
				}
				else
				{
					SetReloadTimer( SequenceDuration() );
				}
			}
			else
			{
				// Update the reload timers.
				UpdateReloadTimers( false );
			}

#ifndef CLIENT_DLL
			WeaponSound( RELOAD );
#endif

			// Next continue to reload shells?
			m_iReloadMode.Set( TF_RELOADING_CONTINUE );

			return true;
		}
	case TF_RELOADING_CONTINUE:
		{
			// Did we finish the reload start?  Now we can finish reloading the rocket.
			if ( m_flTimeWeaponIdle > gpGlobals->curtime )
				return false;

			// If we have ammo, remove ammo and add it to clip
			if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) > 0 && !m_bReloadedThroughAnimEvent )
			{
				m_iClip1 = min( ( m_iClip1 + 1 ), GetMaxClip1() );
				pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
			}

			if ( Clip1() == GetMaxClip1() || pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
			{
				m_iReloadMode.Set( TF_RELOAD_FINISH );
			}
			else
			{
				m_iReloadMode.Set( TF_RELOADING );
			}

			return true;
		}

	case TF_RELOAD_FINISH:
	default:
		{
			if ( SendWeaponAnim( ACT_RELOAD_FINISH ) )
			{
				// We're done, allow primary attack as soon as we like
				//SetReloadTimer( SequenceDuration() );
			}

			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_END );

			m_iReloadMode.Set( TF_RELOAD_START );
			return true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CTFWeaponBase::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	if ( (pEvent->type & AE_TYPE_NEWEVENTSYSTEM) /*&& (pEvent->type & AE_TYPE_SERVER)*/ )
	{
		if ( pEvent->event == AE_WPN_INCREMENTAMMO )
		{
			if ( pOperator->GetAmmoCount( m_iPrimaryAmmoType ) > 0 && !m_bReloadedThroughAnimEvent )
			{
				m_iClip1 = min( ( m_iClip1 + 1 ), GetMaxClip1() );
				pOperator->RemoveAmmo( 1, m_iPrimaryAmmoType );
			}

			m_bReloadedThroughAnimEvent = true;
			return;
		}
	}
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
bool CTFWeaponBase::DefaultReload( int iClipSize1, int iClipSize2, int iActivity )
{
	// The the owning local player.
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return false;

	// Setup and check for reload.
	bool bReloadPrimary = false;
	bool bReloadSecondary = false;

	// If you don't have clips, then don't try to reload them.
	if ( UsesClipsForAmmo1() )
	{
		// need to reload primary clip?
		int primary	= min( iClipSize1 - m_iClip1, pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) );
		if ( primary != 0 )
		{
			bReloadPrimary = true;
		}
	}

	if ( UsesClipsForAmmo2() )
	{
		// need to reload secondary clip?
		int secondary = min( iClipSize2 - m_iClip2, pPlayer->GetAmmoCount( m_iSecondaryAmmoType ) );
		if ( secondary != 0 )
		{
			bReloadSecondary = true;
		}
	}

	// We didn't reload.
	if ( !( bReloadPrimary || bReloadSecondary )  )
		return false;

#ifndef CLIENT_DLL
	// Play reload
	WeaponSound( RELOAD );
#endif

	// Play the player's reload animation
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD );

	float flReloadTime;
	// First, see if we have a reload animation
	if ( SendWeaponAnim( iActivity ) )
	{
		// We consider the reload finished 0.2 sec before the anim is, so that players don't keep accidentally aborting their reloads
		flReloadTime = SequenceDuration() - 0.2;
	}
	else
	{
		// No reload animation. Use the script time.
		flReloadTime = GetPCWpnData().m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeReload;  
		if ( bReloadSecondary )
		{
			flReloadTime = GetPCWpnData().m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_flTimeReload;  
		}
	}

	SetReloadTimer( flReloadTime );

	m_bInReload = true;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::NeedsReloadForAmmo1( int iClipSize1 ) const
{
	CBaseCombatCharacter *pOwner = GetOwner();
	if ( pOwner )
	{
		// If you don't have clips, then don't try to reload them.
		if ( UsesClipsForAmmo1() )
		{
			// need to reload primary clip?
			int primary = MIN( iClipSize1 - m_iClip1, pOwner->GetAmmoCount( m_iPrimaryAmmoType ) );
			if ( primary != 0 )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::NeedsReloadForAmmo2( int iClipSize2 ) const
{
	CBaseCombatCharacter *pOwner = GetOwner();
	if ( pOwner )
	{
		if ( UsesClipsForAmmo2() )
		{
			// need to reload secondary clip?
			int secondary = MIN( iClipSize2 - m_iClip2, pOwner->GetAmmoCount( m_iSecondaryAmmoType ) );
			if ( secondary != 0 )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBase::UpdateReloadTimers( bool bStart )
{
	// Starting a reload?
	if ( bStart )
	{
		// Get the reload start time.
		SetReloadTimer( m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeReloadStart );
	}
	// In reload.
	else
	{
		SetReloadTimer( m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeReload );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBase::SetReloadTimer( float flReloadTime )
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return;

	float flBaseReloadTime = flReloadTime;

	flReloadTime = MAX( flReloadTime, 0.00001f );
	if ( pPlayer->GetViewModel(0) )
	{
		pPlayer->GetViewModel(0)->SetPlaybackRate( flBaseReloadTime / flReloadTime );
	}
	if ( pPlayer->GetViewModel(1) )
	{
		pPlayer->GetViewModel(1)->SetPlaybackRate( flBaseReloadTime / flReloadTime );
	}

	m_flReloadPriorNextFire = m_flNextPrimaryAttack;

	float flTime = gpGlobals->curtime + flReloadTime;

	// Set next player attack time (weapon independent).
	pPlayer->m_flNextAttack = flTime;

	// Set next weapon attack times (based on reloading).
	m_flNextPrimaryAttack = Max( flTime, (float)m_flReloadPriorNextFire);

	// Don't push out secondary attack, because our secondary fire
	// systems are all separate from primary fire (sniper zooming, demoman pipebomb detonating, etc)
	//m_flNextSecondaryAttack = flTime;

	// Set next idle time (based on reloading).
	SetWeaponIdleTime( flTime );
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
bool CTFWeaponBase::PlayEmptySound()
{
	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();

	// TFTODO: Add default empty sound here!
//	EmitSound( filter, entindex(), "Default.ClipEmpty_Rifle" );

	return false;
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBase::SendReloadEvents()
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return;

	// Make the player play his reload animation.
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBase::ItemBusyFrame( void )
{
	// Call into the base ItemBusyFrame.
	BaseClass::ItemBusyFrame();

	CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
	if ( !pOwner )
	{
		return;
	}

	if ( ( pOwner->m_nButtons & IN_ATTACK2 ) && /*m_bInReload == false &&*/ m_bInAttack2 == false )
	{
		pOwner->DoClassSpecialSkill();
		m_bInAttack2 = true;
	}
	else if ( !(pOwner->m_nButtons & IN_ATTACK2) && m_bInAttack2 )
	{
		m_bInAttack2 = false;
	}

	// Interrupt a reload on reload singly weapons.
	if ( ( pOwner->m_nButtons & IN_ATTACK ) && Clip1() > 0 )
	{
		bool bAbortReload = false;
		if ( m_bReloadsSingly )
		{
			if ( m_iReloadMode != TF_RELOAD_START )
			{
				m_iReloadMode.Set( TF_RELOAD_START );
				bAbortReload = true;
			}
		}
		else if ( m_bInReload )
		{
			// We don't let them abort before the next fire point, so they can't use reload to fire before they would have fired if they hadn't reloaded
			if ( gpGlobals->curtime >= m_flReloadPriorNextFire )
			{
				bAbortReload = true;
			}
		}

		if ( bAbortReload )
		{
			AbortReload();
			m_bInReload = false;
			pOwner->m_flNextAttack = gpGlobals->curtime;
			m_flNextPrimaryAttack = Max<float>( gpGlobals->curtime, m_flReloadPriorNextFire );
			SetWeaponIdleTime( gpGlobals->curtime + m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeIdle );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBase::ItemPostFrame( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
	if ( !pOwner )
	{
		return;
	}

	bool bNeedsReload = NeedsReloadForAmmo1( GetMaxClip1() );

	// If we're not shooting, and we want to autoreload, press our reload key
	if ( !AutoFiresFullClip() && pOwner->ShouldAutoReload() && UsesClipsForAmmo1() && !(pOwner->m_nButtons & (IN_ATTACK|IN_ATTACK2)) && bNeedsReload )
	{
		pOwner->m_nButtons |= IN_RELOAD;
	}

	// debounce InAttack flags
	if ( m_bInAttack && !( pOwner->m_nButtons & IN_ATTACK ) )
	{
		m_bInAttack = false;
	}

	if ( m_bInAttack2 && !( pOwner->m_nButtons & IN_ATTACK2 ) )
	{
		m_bInAttack2 = false;
	}

	// If we're lowered, we're not allowed to fire
	if ( m_bLowered )
		return;

	// Call the base item post frame.
	BaseClass::ItemPostFrame();

	// Check for reload singly interrupts.
	if ( m_bReloadsSingly )
	{
		ReloadSinglyPostFrame();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBase::ReloadSinglyPostFrame( void )
{
	if ( m_flTimeWeaponIdle > gpGlobals->curtime )
		return;

	// if the clip is empty and we have ammo remaining, 
	if ( ( ( Clip1() == 0 ) && ( GetOwner()->GetAmmoCount(m_iPrimaryAmmoType) > 0 ) ) ||
		// or we are already in the process of reloading but not finished
		( m_iReloadMode != TF_RELOAD_START ) )
	{
		// reload/continue reloading
		Reload();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::WeaponShouldBeLowered( void )
{
	// Can't be in the middle of another animation
	if ( GetIdealActivity() != ACT_VM_IDLE_LOWERED && GetIdealActivity() != ACT_VM_IDLE &&
		GetIdealActivity() != ACT_VM_IDLE_TO_LOWERED && GetIdealActivity() != ACT_VM_LOWERED_TO_IDLE )
		return false;

	if ( m_bLowered )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::Ready( void )
{
	// If we don't have the anim, just hide for now
	if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) == ACTIVITY_NOT_AVAILABLE )
	{
		RemoveEffects( EF_NODRAW );
	}

	m_bLowered = false;	
	SendWeaponAnim( ACT_VM_IDLE );

	// Prevent firing until our weapon is back up
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	pPlayer->m_flNextAttack = gpGlobals->curtime + SequenceDuration();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::Lower( void )
{
	AbortReload();

	// If we don't have the anim, just hide for now
	if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) == ACTIVITY_NOT_AVAILABLE )
	{
		AddEffects( EF_NODRAW );
	}

	m_bLowered = true;
	SendWeaponAnim( ACT_VM_IDLE_LOWERED );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Show/hide weapon and corresponding view model if any
// Input  : visible - 
//-----------------------------------------------------------------------------
void CTFWeaponBase::SetWeaponVisible( bool visible )
{
	if ( visible )
	{
		RemoveEffects( EF_NODRAW );
	}
	else
	{
		AddEffects( EF_NODRAW );
	}
	
#ifdef CLIENT_DLL
	UpdateVisibility();

	// Force an update
	PreDataUpdate( DATA_UPDATE_DATATABLE_CHANGED );
#endif

}

//-----------------------------------------------------------------------------
// Purpose: Allows the weapon to choose proper weapon idle animation
//-----------------------------------------------------------------------------
void CTFWeaponBase::WeaponIdle( void )
{
	//See if we should idle high or low
	if ( WeaponShouldBeLowered() )
	{
		// Move to lowered position if we're not there yet
		if ( GetActivity() != ACT_VM_IDLE_LOWERED && GetActivity() != ACT_VM_IDLE_TO_LOWERED && GetActivity() != ACT_TRANSITION )
		{
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
		else if ( HasWeaponIdleTimeElapsed() )
		{
			// Keep idling low
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
	}
	else
	{
		// See if we need to raise immediately
		if ( GetActivity() == ACT_VM_IDLE_LOWERED ) 
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
		else if ( HasWeaponIdleTimeElapsed() ) 
		{
			if ( !( m_bReloadsSingly && m_iReloadMode != TF_RELOAD_START ) )
			{
				SendWeaponAnim( ACT_VM_IDLE );
				m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
			}
		}
	}
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
const char *CTFWeaponBase::GetMuzzleFlashModel( void )
{ 
	const char *pszModel = GetPCWpnData().m_ExtraTFWeaponData.m_szMuzzleFlashModel;

	if ( Q_strlen( pszModel ) > 0 )
	{
		return pszModel;
	}

	return NULL;
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
const char *CTFWeaponBase::GetMuzzleFlashParticleEffect( void )
{ 
	const char *pszPEffect = GetPCWpnData().m_ExtraTFWeaponData.m_szMuzzleFlashParticleEffect;

	if ( Q_strlen( pszPEffect ) > 0 )
	{
		return pszPEffect;
	}

	return NULL;
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
float CTFWeaponBase::GetMuzzleFlashModelLifetime( void )
{ 
	return GetPCWpnData().m_ExtraTFWeaponData.m_flMuzzleFlashModelDuration;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTFWeaponBase::GetTracerType( void )
{ 
	if ( tf_useparticletracers.GetBool() && GetPCWpnData().m_ExtraTFWeaponData.m_szTracerEffect && GetPCWpnData().m_ExtraTFWeaponData.m_szTracerEffect[0] )
	{
		if ( !m_szTracerName[0] )
		{
			Q_snprintf( m_szTracerName, MAX_TRACER_NAME, "%s_%s", GetPCWpnData().m_ExtraTFWeaponData.m_szTracerEffect,
				(GetOwner() && GetOwner()->GetTeamNumber() == TF_TEAM_RED ) ? "red" : "blue" );
		}

		return m_szTracerName;
	}

	if ( GetWeaponID() == TF_WEAPON_MINIGUN )
		return "BrightTracer";

	return BaseClass::GetTracerType();
}

//=============================================================================
//
// TFWeaponBase functions (Server specific).
//
#if !defined( CLIENT_DLL )

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBase::CheckRespawn()
{
	// Do not respawn.
	return;
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
CBaseEntity *CTFWeaponBase::Respawn()
{
	// make a copy of this weapon that is invisible and inaccessible to players (no touch function). The weapon spawn/respawn code
	// will decide when to make the weapon visible and touchable.
	CBaseEntity *pNewWeapon = CBaseEntity::Create( GetClassname(), g_pGameRules->VecWeaponRespawnSpot( this ), GetAbsAngles(), GetOwner() );

	if ( pNewWeapon )
	{
		pNewWeapon->AddEffects( EF_NODRAW );// invisible for now
		pNewWeapon->SetTouch( NULL );// no touch
		pNewWeapon->SetThink( &CTFWeaponBase::AttemptToMaterialize );

		UTIL_DropToFloor( this, MASK_SOLID );

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNewWeapon->SetNextThink( gpGlobals->curtime + g_pGameRules->FlWeaponRespawnTime( this ) );
	}
	else
	{
		Msg( "Respawn failed to create %s!\n", GetClassname() );
	}

	return pNewWeapon;
}

// -----------------------------------------------------------------------------
// Purpose: Make a weapon visible and tangible.
// -----------------------------------------------------------------------------
void CTFWeaponBase::Materialize()
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		RemoveEffects( EF_NODRAW );
		DoMuzzleFlash();
	}

	AddSolidFlags( FSOLID_TRIGGER );

	SetThink ( &CTFWeaponBase::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 1 );
}

// -----------------------------------------------------------------------------
// Purpose: The item is trying to materialize, should it do so now or wait longer?
// -----------------------------------------------------------------------------
void CTFWeaponBase::AttemptToMaterialize()
{
	float flTime = g_pGameRules->FlWeaponTryRespawn( this );

	if ( flTime == 0 )
	{
		Materialize();
		return;
	}

	SetNextThink( gpGlobals->curtime + flTime );
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBase::SetDieThink( bool bDie )
{
	if( bDie )
	{
		SetContextThink( &CTFWeaponBase::Die, gpGlobals->curtime + 30.0f, "DieContext" );
	}
	else
	{
		SetContextThink( NULL, gpGlobals->curtime, "DieContext" );
	}
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CTFWeaponBase::Die( void )
{
	UTIL_Remove( this );
}

void CTFWeaponBase::WeaponReset( void )
{
	m_iReloadMode.Set( TF_RELOAD_START );

	m_bResetParity = !m_bResetParity;
}

//-----------------------------------------------------------------------------
// Purpose:
// ----------------------------------------------------------------------------
const Vector &CTFWeaponBase::GetBulletSpread( void )
{
	static Vector cone = VECTOR_CONE_15DEGREES;
	return cone;
}

#else

void TE_DynamicLight( IRecipientFilter& filter, float delay,
					 const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay, int nLightIndex = LIGHT_INDEX_TE_DYNAMIC );

//=============================================================================
//
// TFWeaponBase functions (Client specific).
//

bool CTFWeaponBase::IsFirstPersonView()
{
	C_TFPlayer *pPlayerOwner = GetTFPlayerOwner();
	if ( pPlayerOwner == NULL )
	{
		return false;
	}
	return pPlayerOwner->InFirstPersonView();
}

bool CTFWeaponBase::UsingViewModel()
{
	C_TFPlayer *pPlayerOwner = GetTFPlayerOwner();
	bool bIsFirstPersonView = IsFirstPersonView();
	bool bUsingViewModel = bIsFirstPersonView && ( pPlayerOwner != NULL ) && !pPlayerOwner->ShouldDrawThisPlayer();
	return bUsingViewModel;
}

C_BaseAnimating *CTFWeaponBase::GetAppropriateWorldOrViewModel()
{
	C_TFPlayer *pPlayerOwner = GetTFPlayerOwner();
	if ( pPlayerOwner && UsingViewModel() )
	{
		// Nope - it's a standard viewmodel.
		C_BaseAnimating *pViewModel = pPlayerOwner->GetViewModel();
		if ( pViewModel != NULL )
		{
			return pViewModel;
		}

		// No viewmodel, so just return the normal model.
		return this;
	}
	else
	{
		return this;
	}
}

void CTFWeaponBase::CreateMuzzleFlashEffects( C_BaseEntity *pAttachEnt, int nIndex )
{
	Vector vecOrigin;
	QAngle angAngles;

	int iMuzzleFlashAttachment = pAttachEnt->LookupAttachment( "muzzle" );

	const char *pszMuzzleFlashEffect = NULL;
	const char *pszMuzzleFlashModel = GetMuzzleFlashModel();
	const char *pszMuzzleFlashParticleEffect = GetMuzzleFlashParticleEffect();

	// Pick the right muzzleflash (3rd / 1st person)
	if ( IsFirstPersonView() )
	{
		pszMuzzleFlashEffect = GetMuzzleFlashEffectName_1st();
	}
	else
	{
		pszMuzzleFlashEffect = GetMuzzleFlashEffectName_3rd();
	}

	// If we have an attachment, then stick a light on it.
	if ( iMuzzleFlashAttachment > 0 && (pszMuzzleFlashEffect || pszMuzzleFlashModel || pszMuzzleFlashParticleEffect ) )
	{
		pAttachEnt->GetAttachment( iMuzzleFlashAttachment, vecOrigin, angAngles );

		// Muzzleflash light
/*
		CLocalPlayerFilter filter;
		TE_DynamicLight( filter, 0.0f, &vecOrigin, 255, 192, 64, 5, 70.0f, 0.05f, 70.0f / 0.05f, LIGHT_INDEX_MUZZLEFLASH );
*/

		if ( pszMuzzleFlashEffect )
		{
			// Using an muzzle flash dispatch effect
			CEffectData muzzleFlashData;
			muzzleFlashData.m_vOrigin = vecOrigin;
			muzzleFlashData.m_vAngles = angAngles;
			muzzleFlashData.m_hEntity = pAttachEnt->GetRefEHandle();
			muzzleFlashData.m_nAttachmentIndex = iMuzzleFlashAttachment;
			//muzzleFlashData.m_nHitBox = GetPCWpnData().m_iMuzzleFlashType;
			//muzzleFlashData.m_flMagnitude = GetPCWpnData().m_flMuzzleFlashScale;
			muzzleFlashData.m_flMagnitude = 0.2;
			DispatchEffect( pszMuzzleFlashEffect, muzzleFlashData );
		}

		if ( pszMuzzleFlashModel )
		{
			float flEffectLifetime = GetMuzzleFlashModelLifetime();

			// Using a model as a muzzle flash.
			if ( m_hMuzzleFlashModel[nIndex] )
			{
				// Increase the lifetime of the muzzleflash
				m_hMuzzleFlashModel[nIndex]->SetLifetime( flEffectLifetime );
			}
			else
			{
				m_hMuzzleFlashModel[nIndex] = C_MuzzleFlashModel::CreateMuzzleFlashModel( pszMuzzleFlashModel, pAttachEnt, iMuzzleFlashAttachment, flEffectLifetime );

				// FIXME: This is an incredibly brutal hack to get muzzle flashes positioned correctly for recording
				m_hMuzzleFlashModel[nIndex]->SetIs3rdPersonFlash( nIndex == 1 );
			}
		}

		if ( pszMuzzleFlashParticleEffect ) 
		{
			DispatchParticleEffect( pszMuzzleFlashParticleEffect, PATTACH_POINT_FOLLOW, pAttachEnt, "muzzle" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::ShouldDraw( void )
{
	C_BaseCombatCharacter *pOwner = GetOwner();
	if ( !pOwner )
		return true;

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pLocalPlayer )
		return true;

	if ( pOwner->IsPlayer() )
	{
		CTFPlayer *pTFOwner = ToTFPlayer( GetOwner() );
		if ( !pTFOwner )
			return true;

		if ( pTFOwner->m_Shared.IsControlStunned() )
			return false;

		// Ghosts dont have weapons
		if ( pTFOwner->m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) )
			return false;

		if ( pTFOwner->m_Shared.GetDisguiseWeapon() )
		{
			if ( pTFOwner->m_Shared.InCond( TF_COND_DISGUISED ) )
			{
				int iLocalPlayerTeam = pLocalPlayer->GetTeamNumber();
				// If we are disguised we may want to draw the disguise weapon.
				if ( iLocalPlayerTeam != pOwner->GetTeamNumber() && (iLocalPlayerTeam != TEAM_SPECTATOR) )
				{
					// We are a disguised enemy, so only draw the disguise weapon.
					if ( pTFOwner->m_Shared.GetDisguiseWeapon() != this )
					{
						return false;
					}
				}
				else
				{
					// We are a disguised friendly. Don't draw the disguise weapon.
					if ( m_bDisguiseWeapon )
					{
						return false;
					}
				}
			}
			else
			{
				// We are not disguised. Never draw the disguise weapon.
				if ( m_bDisguiseWeapon )
				{
					return false;
				}
			}
		}
	}

	return BaseClass::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFWeaponBase::InternalDrawModel( int flags )
{
	C_TFPlayer *pOwner = ToTFPlayer( GetOwnerEntity() );
	bool bNotViewModel = ( pOwner->ShouldDrawThisPlayer() );
	bool bUseInvulnMaterial = ( bNotViewModel && pOwner && pOwner->m_Shared.IsInvulnerable() && 
								( !pOwner->m_Shared.InCond( TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED ) || gpGlobals->curtime < pOwner->GetLastDamageTime() + 2.0f ) );

	if ( bUseInvulnMaterial )
	{
		modelrender->ForcedMaterialOverride( *pOwner->GetInvulnMaterialRef() );
	}

	int ret = BaseClass::InternalDrawModel( flags );

	if ( bUseInvulnMaterial )
	{
		modelrender->ForcedMaterialOverride( NULL );
	}

	return ret;
}

void CTFWeaponBase::ProcessMuzzleFlashEvent( void )
{
	C_BaseEntity *pAttachEnt;
	C_TFPlayer *pOwner = ToTFPlayer( GetOwnerEntity() );

	if ( pOwner == NULL )
		return;

	bool bDrawMuzzleFlashOnViewModel = ( pOwner->IsLocalPlayer() && !C_BasePlayer::ShouldDrawLocalPlayer() ) ||
		( IsLocalPlayerSpectator() && GetSpectatorMode() == OBS_MODE_IN_EYE && GetSpectatorTarget() == pOwner->entindex() );

	if ( bDrawMuzzleFlashOnViewModel )
	{
		pAttachEnt = pOwner->GetViewModel();
	}
	else
	{
		pAttachEnt = this;
	}

	{
		CRecordEffectOwner recordOwner( pOwner, bDrawMuzzleFlashOnViewModel );
		CreateMuzzleFlashEffects( pAttachEnt, 0 );
	}

	// Quasi-evil
	int nModelIndex = GetModelIndex();
	int nWorldModelIndex = GetWorldModelIndex();
	bool bInToolRecordingMode = ToolsEnabled() && clienttools->IsInRecordingMode();
	if ( bInToolRecordingMode && nModelIndex != nWorldModelIndex && pOwner->IsLocalPlayer() )
	{
		CRecordEffectOwner recordOwner( pOwner, false );

		SetModelIndex( nWorldModelIndex );
		CreateMuzzleFlashEffects( this, 1 );
		SetModelIndex( nModelIndex );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// ----------------------------------------------------------------------------
bool CTFWeaponBase::ShouldPredict()
{
	if ( GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer() )
	{
		return true;
	}

	return BaseClass::ShouldPredict();
}

//-----------------------------------------------------------------------------
// Purpose:
// ----------------------------------------------------------------------------
void CTFWeaponBase::WeaponReset( void )
{
	UpdateVisibility();
}

//-----------------------------------------------------------------------------
// Purpose:
// ----------------------------------------------------------------------------
void CTFWeaponBase::OnPreDataChanged( DataUpdateType_t type )
{
	BaseClass::OnPreDataChanged( type );

	m_bOldResetParity = m_bResetParity;

}

//-----------------------------------------------------------------------------
// Purpose:
// ----------------------------------------------------------------------------
void CTFWeaponBase::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		ListenForGameEvent( "localplayer_changeteam" );
	}

	if ( GetPredictable() && !ShouldPredict() )
	{
		ShutdownPredictable();
	}

	//If its a world (held or dropped) model then set the correct skin color here.
	if ( m_nModelIndex == GetWorldModelIndex() )
	{
		m_nSkin = GetSkin();
	}

	if ( m_bResetParity != m_bOldResetParity )
	{
		WeaponReset();
	}

	//Here we go...
	//Since we can't get a repro for the invisible weapon thing, I'll fix it right up here:
	CTFPlayer *pOwner = ToTFPlayer( GetOwnerEntity() );
	// Our owner is alive and not a loser.
	if ( pOwner && pOwner->IsAlive() && !pOwner->m_Shared.IsLoser() && !pOwner->m_Shared.InCond( TF_COND_COMPETITIVE_LOSER ) && ( pOwner->GetActiveWeapon() == this ) )
	{
		// And he is NOT taunting or control stunned.
		if ( !pOwner->m_Shared.InCond ( TF_COND_TAUNTING ) && !pOwner->m_Shared.InCond ( TF_COND_HALLOWEEN_KART ) &&
			 ( !pOwner->m_Shared.IsControlStunned() || !HideWhileStunned() ) )
		{
			if ( IsEffectActive( EF_NODRAW ) )
			{
				RemoveEffects( EF_NODRAW );
				UpdateVisibility();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// ----------------------------------------------------------------------------
int CTFWeaponBase::CalcOverrideModelIndex( void )
{
	if ( ShouldDrawUsingViewModel() )
	{
		return m_iViewModelIndex;
	}

	return GetWorldModelIndex();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFWeaponBase::FireGameEvent( IGameEvent *event )
{
	// If we were the active weapon, we need to update our visibility 
	// because we may switch visibility due to Spy disguises.
	const char *pszEventName = event->GetName();
	if ( Q_strcmp( pszEventName, "localplayer_changeteam" ) == 0 )
	{
		UpdateVisibility();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// ----------------------------------------------------------------------------
int CTFWeaponBase::GetWorldModelIndex( void )
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();

	if ( pPlayer )
	{
		// if we're a spy and we're disguised, we also
		// want to disguise our weapon's world model

		CTFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
		if ( !pLocalPlayer )
			return 0;

		int iLocalTeam = pLocalPlayer->GetTeamNumber();

		// We only show disguise weapon to the enemy team when owner is disguised
		bool bUseDisguiseWeapon = ( pPlayer->GetTeamNumber() != iLocalTeam && iLocalTeam > LAST_SHARED_TEAM );

		if ( bUseDisguiseWeapon && pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
		{
			CTFWeaponBase *pDisguiseWeapon = pPlayer->m_Shared.GetDisguiseWeapon();
			if ( !pDisguiseWeapon )
				return BaseClass::GetWorldModelIndex();
			if ( pDisguiseWeapon == this )
				return BaseClass::GetWorldModelIndex();
			else
				return pDisguiseWeapon->GetWorldModelIndex();
		}	
	}

	return BaseClass::GetWorldModelIndex();
}

bool CTFWeaponBase::ShouldDrawCrosshair( void )
{
	const char *crosshairfile = cl_crosshair_file.GetString();
	if ( !crosshairfile || !crosshairfile[0] )
	{
		// Default crosshair.
		return GetPCWpnData().m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_bDrawCrosshair;
	}
	// Custom crosshair.
	return true;
}

void CTFWeaponBase::Redraw()
{
	if ( ShouldDrawCrosshair() && g_pClientMode->ShouldDrawCrosshair() )
	{
		DrawCrosshair();
	}
}

#endif

acttable_t CTFWeaponBase::m_acttablePrimary[] = 
{
	{ ACT_MP_STAND_IDLE,		ACT_MP_STAND_PRIMARY,				false },
	{ ACT_MP_CROUCH_IDLE,		ACT_MP_CROUCH_PRIMARY,				false },
	{ ACT_MP_DEPLOYED,			ACT_MP_DEPLOYED_PRIMARY,			false },
	{ ACT_MP_RUN,				ACT_MP_RUN_PRIMARY,					false },
	{ ACT_MP_WALK,				ACT_MP_WALK_PRIMARY,				false },
	{ ACT_MP_AIRWALK,			ACT_MP_AIRWALK_PRIMARY,				false },
	{ ACT_MP_CROUCHWALK,		ACT_MP_CROUCHWALK_PRIMARY,			false },
	{ ACT_MP_JUMP,				ACT_MP_JUMP_PRIMARY,				false },
	{ ACT_MP_JUMP_START,		ACT_MP_JUMP_START_PRIMARY,			false },
	{ ACT_MP_JUMP_FLOAT,		ACT_MP_JUMP_FLOAT_PRIMARY,			false },
	{ ACT_MP_JUMP_LAND,			ACT_MP_JUMP_LAND_PRIMARY,			false },
	{ ACT_MP_SWIM,				ACT_MP_SWIM_PRIMARY,				false },
	{ ACT_MP_SWIM_DEPLOYED,		ACT_MP_SWIM_DEPLOYED_PRIMARY,		false },

	{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,		ACT_MP_ATTACK_STAND_PRIMARY,	false },
	{ ACT_MP_ATTACK_STAND_PRIMARYFIRE_DEPLOYED,		ACT_MP_ATTACK_STAND_PRIMARY_DEPLOYED, false },
	{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,		ACT_MP_ATTACK_CROUCH_PRIMARY,	false },
	{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE_DEPLOYED,	ACT_MP_ATTACK_CROUCH_PRIMARY_DEPLOYED,	false },
	{ ACT_MP_ATTACK_SWIM_PRIMARYFIRE,		ACT_MP_ATTACK_SWIM_PRIMARY,		false },
	{ ACT_MP_ATTACK_AIRWALK_PRIMARYFIRE,	ACT_MP_ATTACK_AIRWALK_PRIMARY,	false },

	{ ACT_MP_RELOAD_STAND,		ACT_MP_RELOAD_STAND_PRIMARY,		false },
	{ ACT_MP_RELOAD_STAND_LOOP,	ACT_MP_RELOAD_STAND_PRIMARY_LOOP,	false },
	{ ACT_MP_RELOAD_STAND_END,	ACT_MP_RELOAD_STAND_PRIMARY_END,	false },
	{ ACT_MP_RELOAD_CROUCH,		ACT_MP_RELOAD_CROUCH_PRIMARY,		false },
	{ ACT_MP_RELOAD_CROUCH_LOOP,ACT_MP_RELOAD_CROUCH_PRIMARY_LOOP,	false },
	{ ACT_MP_RELOAD_CROUCH_END,	ACT_MP_RELOAD_CROUCH_PRIMARY_END,	false },
	{ ACT_MP_RELOAD_SWIM,		ACT_MP_RELOAD_SWIM_PRIMARY,			false },
	{ ACT_MP_RELOAD_SWIM_LOOP,	ACT_MP_RELOAD_SWIM_PRIMARY_LOOP,	false },
	{ ACT_MP_RELOAD_SWIM_END,	ACT_MP_RELOAD_SWIM_PRIMARY_END,		false },
	{ ACT_MP_RELOAD_AIRWALK,	ACT_MP_RELOAD_AIRWALK_PRIMARY,		false },
	{ ACT_MP_RELOAD_AIRWALK_LOOP,	ACT_MP_RELOAD_AIRWALK_PRIMARY_LOOP,	false },
	{ ACT_MP_RELOAD_AIRWALK_END,ACT_MP_RELOAD_AIRWALK_PRIMARY_END,	false },

	{ ACT_MP_GESTURE_FLINCH,	ACT_MP_GESTURE_FLINCH_PRIMARY, false },

	{ ACT_MP_GRENADE1_DRAW,		ACT_MP_PRIMARY_GRENADE1_DRAW,	false },
	{ ACT_MP_GRENADE1_IDLE,		ACT_MP_PRIMARY_GRENADE1_IDLE,	false },
	{ ACT_MP_GRENADE1_ATTACK,	ACT_MP_PRIMARY_GRENADE1_ATTACK,	false },
	{ ACT_MP_GRENADE2_DRAW,		ACT_MP_PRIMARY_GRENADE2_DRAW,	false },
	{ ACT_MP_GRENADE2_IDLE,		ACT_MP_PRIMARY_GRENADE2_IDLE,	false },
	{ ACT_MP_GRENADE2_ATTACK,	ACT_MP_PRIMARY_GRENADE2_ATTACK,	false },

	{ ACT_MP_ATTACK_STAND_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE,	false },
	{ ACT_MP_ATTACK_CROUCH_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE,	false },
	{ ACT_MP_ATTACK_SWIM_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE,	false },
	{ ACT_MP_ATTACK_AIRWALK_GRENADE,	ACT_MP_ATTACK_STAND_GRENADE,	false },

	{ ACT_MP_GESTURE_VC_HANDMOUTH,	ACT_MP_GESTURE_VC_HANDMOUTH_PRIMARY,	false },
	{ ACT_MP_GESTURE_VC_FINGERPOINT,	ACT_MP_GESTURE_VC_FINGERPOINT_PRIMARY,	false },
	{ ACT_MP_GESTURE_VC_FISTPUMP,	ACT_MP_GESTURE_VC_FISTPUMP_PRIMARY,	false },
	{ ACT_MP_GESTURE_VC_THUMBSUP,	ACT_MP_GESTURE_VC_THUMBSUP_PRIMARY,	false },
	{ ACT_MP_GESTURE_VC_NODYES,	ACT_MP_GESTURE_VC_NODYES_PRIMARY,	false },
	{ ACT_MP_GESTURE_VC_NODNO,	ACT_MP_GESTURE_VC_NODNO_PRIMARY,	false },
};

acttable_t CTFWeaponBase::m_acttableSecondary[] = 
{
	{ ACT_MP_STAND_IDLE,		ACT_MP_STAND_SECONDARY,				false },
	{ ACT_MP_CROUCH_IDLE,		ACT_MP_CROUCH_SECONDARY,			false },
	{ ACT_MP_RUN,				ACT_MP_RUN_SECONDARY,				false },
	{ ACT_MP_WALK,				ACT_MP_WALK_SECONDARY,				false },
	{ ACT_MP_AIRWALK,			ACT_MP_AIRWALK_SECONDARY,			false },
	{ ACT_MP_CROUCHWALK,		ACT_MP_CROUCHWALK_SECONDARY,		false },
	{ ACT_MP_JUMP,				ACT_MP_JUMP_SECONDARY,				false },
	{ ACT_MP_JUMP_START,		ACT_MP_JUMP_START_SECONDARY,		false },
	{ ACT_MP_JUMP_FLOAT,		ACT_MP_JUMP_FLOAT_SECONDARY,		false },
	{ ACT_MP_JUMP_LAND,			ACT_MP_JUMP_LAND_SECONDARY,			false },
	{ ACT_MP_SWIM,				ACT_MP_SWIM_SECONDARY,				false },

	{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,		ACT_MP_ATTACK_STAND_SECONDARY,		false },
	{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,		ACT_MP_ATTACK_CROUCH_SECONDARY,		false },
	{ ACT_MP_ATTACK_SWIM_PRIMARYFIRE,		ACT_MP_ATTACK_SWIM_SECONDARY,		false },
	{ ACT_MP_ATTACK_AIRWALK_PRIMARYFIRE,	ACT_MP_ATTACK_AIRWALK_SECONDARY,	false },

	{ ACT_MP_RELOAD_STAND,		ACT_MP_RELOAD_STAND_SECONDARY,		false },
	{ ACT_MP_RELOAD_STAND_LOOP,	ACT_MP_RELOAD_STAND_SECONDARY_LOOP,	false },
	{ ACT_MP_RELOAD_STAND_END,	ACT_MP_RELOAD_STAND_SECONDARY_END,	false },
	{ ACT_MP_RELOAD_CROUCH,		ACT_MP_RELOAD_CROUCH_SECONDARY,		false },
	{ ACT_MP_RELOAD_CROUCH_LOOP,ACT_MP_RELOAD_CROUCH_SECONDARY_LOOP,false },
	{ ACT_MP_RELOAD_CROUCH_END,	ACT_MP_RELOAD_CROUCH_SECONDARY_END,	false },
	{ ACT_MP_RELOAD_SWIM,		ACT_MP_RELOAD_SWIM_SECONDARY,		false },
	{ ACT_MP_RELOAD_SWIM_LOOP,	ACT_MP_RELOAD_SWIM_SECONDARY_LOOP,	false },
	{ ACT_MP_RELOAD_SWIM_END,	ACT_MP_RELOAD_SWIM_SECONDARY_END,	false },
	{ ACT_MP_RELOAD_AIRWALK,	ACT_MP_RELOAD_AIRWALK_SECONDARY,	false },
	{ ACT_MP_RELOAD_AIRWALK_LOOP,	ACT_MP_RELOAD_AIRWALK_SECONDARY_LOOP,	false },
	{ ACT_MP_RELOAD_AIRWALK_END,ACT_MP_RELOAD_AIRWALK_SECONDARY_END,false },

	{ ACT_MP_GESTURE_FLINCH,	ACT_MP_GESTURE_FLINCH_SECONDARY, false },

	{ ACT_MP_GRENADE1_DRAW,		ACT_MP_SECONDARY_GRENADE1_DRAW,	false },
	{ ACT_MP_GRENADE1_IDLE,		ACT_MP_SECONDARY_GRENADE1_IDLE,	false },
	{ ACT_MP_GRENADE1_ATTACK,	ACT_MP_SECONDARY_GRENADE1_ATTACK,	false },
	{ ACT_MP_GRENADE2_DRAW,		ACT_MP_SECONDARY_GRENADE2_DRAW,	false },
	{ ACT_MP_GRENADE2_IDLE,		ACT_MP_SECONDARY_GRENADE2_IDLE,	false },
	{ ACT_MP_GRENADE2_ATTACK,	ACT_MP_SECONDARY_GRENADE2_ATTACK,	false },

	{ ACT_MP_ATTACK_STAND_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE,	false },
	{ ACT_MP_ATTACK_CROUCH_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE,	false },
	{ ACT_MP_ATTACK_SWIM_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE,	false },
	{ ACT_MP_ATTACK_AIRWALK_GRENADE,	ACT_MP_ATTACK_STAND_GRENADE,	false },

	{ ACT_MP_GESTURE_VC_HANDMOUTH,	ACT_MP_GESTURE_VC_HANDMOUTH_SECONDARY,	false },
	{ ACT_MP_GESTURE_VC_FINGERPOINT,	ACT_MP_GESTURE_VC_FINGERPOINT_SECONDARY,	false },
	{ ACT_MP_GESTURE_VC_FISTPUMP,	ACT_MP_GESTURE_VC_FISTPUMP_SECONDARY,	false },
	{ ACT_MP_GESTURE_VC_THUMBSUP,	ACT_MP_GESTURE_VC_THUMBSUP_SECONDARY,	false },
	{ ACT_MP_GESTURE_VC_NODYES,	ACT_MP_GESTURE_VC_NODYES_SECONDARY,	false },
	{ ACT_MP_GESTURE_VC_NODNO,	ACT_MP_GESTURE_VC_NODNO_SECONDARY,	false },
};

acttable_t CTFWeaponBase::m_acttableMelee[] = 
{
	{ ACT_MP_STAND_IDLE,		ACT_MP_STAND_MELEE,				false },
	{ ACT_MP_CROUCH_IDLE,		ACT_MP_CROUCH_MELEE,			false },
	{ ACT_MP_RUN,				ACT_MP_RUN_MELEE,				false },
	{ ACT_MP_WALK,				ACT_MP_WALK_MELEE,				false },
	{ ACT_MP_AIRWALK,			ACT_MP_AIRWALK_MELEE,			false },
	{ ACT_MP_CROUCHWALK,		ACT_MP_CROUCHWALK_MELEE,		false },
	{ ACT_MP_JUMP,				ACT_MP_JUMP_MELEE,				false },
	{ ACT_MP_JUMP_START,		ACT_MP_JUMP_START_MELEE,		false },
	{ ACT_MP_JUMP_FLOAT,		ACT_MP_JUMP_FLOAT_MELEE,		false },
	{ ACT_MP_JUMP_LAND,			ACT_MP_JUMP_LAND_MELEE,			false },
	{ ACT_MP_SWIM,				ACT_MP_SWIM_MELEE,				false },

	{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,		ACT_MP_ATTACK_STAND_MELEE,		false },
	{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,		ACT_MP_ATTACK_CROUCH_MELEE,		false },
	{ ACT_MP_ATTACK_SWIM_PRIMARYFIRE,		ACT_MP_ATTACK_SWIM_MELEE,		false },
	{ ACT_MP_ATTACK_AIRWALK_PRIMARYFIRE,	ACT_MP_ATTACK_AIRWALK_MELEE,	false },

	{ ACT_MP_ATTACK_STAND_SECONDARYFIRE,	ACT_MP_ATTACK_STAND_MELEE_SECONDARY, false },
	{ ACT_MP_ATTACK_CROUCH_SECONDARYFIRE,	ACT_MP_ATTACK_CROUCH_MELEE_SECONDARY,false },
	{ ACT_MP_ATTACK_SWIM_SECONDARYFIRE,		ACT_MP_ATTACK_SWIM_MELEE,		false },
	{ ACT_MP_ATTACK_AIRWALK_SECONDARYFIRE,	ACT_MP_ATTACK_AIRWALK_MELEE,	false },

	{ ACT_MP_GESTURE_FLINCH,	ACT_MP_GESTURE_FLINCH_MELEE, false },

	{ ACT_MP_GRENADE1_DRAW,		ACT_MP_MELEE_GRENADE1_DRAW,	false },
	{ ACT_MP_GRENADE1_IDLE,		ACT_MP_MELEE_GRENADE1_IDLE,	false },
	{ ACT_MP_GRENADE1_ATTACK,	ACT_MP_MELEE_GRENADE1_ATTACK,	false },
	{ ACT_MP_GRENADE2_DRAW,		ACT_MP_MELEE_GRENADE2_DRAW,	false },
	{ ACT_MP_GRENADE2_IDLE,		ACT_MP_MELEE_GRENADE2_IDLE,	false },
	{ ACT_MP_GRENADE2_ATTACK,	ACT_MP_MELEE_GRENADE2_ATTACK,	false },

	{ ACT_MP_GESTURE_VC_HANDMOUTH,	ACT_MP_GESTURE_VC_HANDMOUTH_MELEE,	false },
	{ ACT_MP_GESTURE_VC_FINGERPOINT,	ACT_MP_GESTURE_VC_FINGERPOINT_MELEE,	false },
	{ ACT_MP_GESTURE_VC_FISTPUMP,	ACT_MP_GESTURE_VC_FISTPUMP_MELEE,	false },
	{ ACT_MP_GESTURE_VC_THUMBSUP,	ACT_MP_GESTURE_VC_THUMBSUP_MELEE,	false },
	{ ACT_MP_GESTURE_VC_NODYES,	ACT_MP_GESTURE_VC_NODYES_MELEE,	false },
	{ ACT_MP_GESTURE_VC_NODNO,	ACT_MP_GESTURE_VC_NODNO_MELEE,	false },
};

acttable_t CTFWeaponBase::m_acttableBuilding[] = 
{
	{ ACT_MP_STAND_IDLE,		ACT_MP_STAND_BUILDING,			false },
	{ ACT_MP_CROUCH_IDLE,		ACT_MP_CROUCH_BUILDING,			false },
	{ ACT_MP_RUN,				ACT_MP_RUN_BUILDING,			false },
	{ ACT_MP_WALK,				ACT_MP_WALK_BUILDING,			false },
	{ ACT_MP_AIRWALK,			ACT_MP_AIRWALK_BUILDING,		false },
	{ ACT_MP_CROUCHWALK,		ACT_MP_CROUCHWALK_BUILDING,		false },
	{ ACT_MP_JUMP,				ACT_MP_JUMP_BUILDING,			false },
	{ ACT_MP_JUMP_START,		ACT_MP_JUMP_START_BUILDING,		false },
	{ ACT_MP_JUMP_FLOAT,		ACT_MP_JUMP_FLOAT_BUILDING,		false },
	{ ACT_MP_JUMP_LAND,			ACT_MP_JUMP_LAND_BUILDING,		false },
	{ ACT_MP_SWIM,				ACT_MP_SWIM_BUILDING,			false },

	{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,		ACT_MP_ATTACK_STAND_BUILDING,		false },
	{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,		ACT_MP_ATTACK_CROUCH_BUILDING,		false },
	{ ACT_MP_ATTACK_SWIM_PRIMARYFIRE,		ACT_MP_ATTACK_SWIM_BUILDING,		false },
	{ ACT_MP_ATTACK_AIRWALK_PRIMARYFIRE,	ACT_MP_ATTACK_AIRWALK_BUILDING,	false },

	{ ACT_MP_ATTACK_STAND_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE_BUILDING,	false },
	{ ACT_MP_ATTACK_CROUCH_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE_BUILDING,	false },
	{ ACT_MP_ATTACK_SWIM_GRENADE,		ACT_MP_ATTACK_STAND_GRENADE_BUILDING,	false },
	{ ACT_MP_ATTACK_AIRWALK_GRENADE,	ACT_MP_ATTACK_STAND_GRENADE_BUILDING,	false },

	{ ACT_MP_GESTURE_VC_HANDMOUTH,	ACT_MP_GESTURE_VC_HANDMOUTH_BUILDING,	false },
	{ ACT_MP_GESTURE_VC_FINGERPOINT,	ACT_MP_GESTURE_VC_FINGERPOINT_BUILDING,	false },
	{ ACT_MP_GESTURE_VC_FISTPUMP,	ACT_MP_GESTURE_VC_FISTPUMP_BUILDING,	false },
	{ ACT_MP_GESTURE_VC_THUMBSUP,	ACT_MP_GESTURE_VC_THUMBSUP_BUILDING,	false },
	{ ACT_MP_GESTURE_VC_NODYES,	ACT_MP_GESTURE_VC_NODYES_BUILDING,	false },
	{ ACT_MP_GESTURE_VC_NODNO,	ACT_MP_GESTURE_VC_NODNO_BUILDING,	false },
};

acttable_t CTFWeaponBase::m_acttablePDA[] = 
{
	{ ACT_MP_STAND_IDLE,		ACT_MP_STAND_PDA,			false },
	{ ACT_MP_CROUCH_IDLE,		ACT_MP_CROUCH_PDA,			false },
	{ ACT_MP_RUN,				ACT_MP_RUN_PDA,				false },
	{ ACT_MP_WALK,				ACT_MP_WALK_PDA,			false },
	{ ACT_MP_AIRWALK,			ACT_MP_AIRWALK_PDA,			false },
	{ ACT_MP_CROUCHWALK,		ACT_MP_CROUCHWALK_PDA,		false },
	{ ACT_MP_JUMP,				ACT_MP_JUMP_PDA,			false },
	{ ACT_MP_JUMP_START,		ACT_MP_JUMP_START_PDA,		false },
	{ ACT_MP_JUMP_FLOAT,		ACT_MP_JUMP_FLOAT_PDA,		false },
	{ ACT_MP_JUMP_LAND,			ACT_MP_JUMP_LAND_PDA,		false },
	{ ACT_MP_SWIM,				ACT_MP_SWIM_PDA,			false },

	{ ACT_MP_ATTACK_STAND_PRIMARYFIRE, ACT_MP_ATTACK_STAND_PDA, false },
	{ ACT_MP_ATTACK_SWIM_PRIMARYFIRE, ACT_MP_ATTACK_SWIM_PDA, false },

	{ ACT_MP_GESTURE_VC_HANDMOUTH,	ACT_MP_GESTURE_VC_HANDMOUTH_PDA,	false },
	{ ACT_MP_GESTURE_VC_FINGERPOINT,	ACT_MP_GESTURE_VC_FINGERPOINT_PDA,	false },
	{ ACT_MP_GESTURE_VC_FISTPUMP,	ACT_MP_GESTURE_VC_FISTPUMP_PDA,	false },
	{ ACT_MP_GESTURE_VC_THUMBSUP,	ACT_MP_GESTURE_VC_THUMBSUP_PDA,	false },
	{ ACT_MP_GESTURE_VC_NODYES,	ACT_MP_GESTURE_VC_NODYES_PDA,	false },
	{ ACT_MP_GESTURE_VC_NODNO,	ACT_MP_GESTURE_VC_NODNO_PDA,	false },
};

ConVar mp_forceactivityset( "mp_forceactivityset", "-1", FCVAR_CHEAT|FCVAR_REPLICATED|FCVAR_DEVELOPMENTONLY );

acttable_t *CTFWeaponBase::ActivityList( int &iActivityCount )
{
	int iWeaponRole = GetPCWpnData().m_ExtraTFWeaponData.m_iWeaponType;

	if ( mp_forceactivityset.GetInt() >= 0 )
	{
		iWeaponRole = mp_forceactivityset.GetInt();
	}

#ifdef CLIENT_DLL
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( pPlayer && pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) && pPlayer->IsEnemyPlayer() )
	{
		CTFWeaponBase *pDisguiseWeapon = pPlayer->m_Shared.GetDisguiseWeapon();
		if ( pDisguiseWeapon && pDisguiseWeapon != this )
		{
			return pDisguiseWeapon->ActivityList( iActivityCount );
		}
	}
#endif

	acttable_t *pTable;

	switch( iWeaponRole )
	{
	case TF_WPN_TYPE_PRIMARY:
	default:
		pTable = m_acttablePrimary;
		iActivityCount = ARRAYSIZE(m_acttablePrimary);
		break;
	case TF_WPN_TYPE_SECONDARY:
		pTable = m_acttableSecondary;
		iActivityCount = ARRAYSIZE(m_acttableSecondary);
		break;
	case TF_WPN_TYPE_MELEE:
		pTable = m_acttableMelee;
		iActivityCount = ARRAYSIZE(m_acttableMelee);
		break;
	case TF_WPN_TYPE_BUILDING:
		pTable = m_acttableBuilding;
		iActivityCount = ARRAYSIZE(m_acttableBuilding);
		break;
	case TF_WPN_TYPE_PDA:
		pTable = m_acttablePDA;
		iActivityCount = ARRAYSIZE(m_acttablePDA);
		break;
	}

	return pTable;
}

#ifdef CLIENT_DLL
// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
C_BaseEntity *CTFWeaponBase::GetWeaponForEffect()
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pLocalPlayer )
		return NULL;

#if 0
	// This causes many problems!
	if ( pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		C_BasePlayer *pObserverTarget = ToBasePlayer( pLocalPlayer->GetObserverTarget() );
		if ( pObserverTarget )
			return pObserverTarget->GetViewModel();
	}
#endif

	if ( pLocalPlayer == GetTFPlayerOwner() )
		return pLocalPlayer->GetViewModel();

	return this;
}
#endif

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
bool CTFWeaponBase::CanAttack( void )
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();

	if ( pPlayer )
		return pPlayer->CanAttack();

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::CanFireRandomCriticalShot( float flCritChance )
{
#ifdef GAME_DLL
	// Todo: Create a version of this in tf_weaponbase_melee

	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return false;

	PlayerStats_t *pPlayerStats = CTF_GameStats.FindPlayerStats( pPlayer );
	if ( pPlayerStats )
	{
		// Compare total damage done against total crit damage done.  If this
		// ratio is out of range for the expected crit chance, deny the crit.
		int nRandomRangedCritDamage = pPlayerStats->statsCurrentRound.m_iStat[TFSTAT_DAMAGE_RANGED_CRIT_RANDOM];
		int nTotalDamage = pPlayerStats->statsCurrentRound.m_iStat[TFSTAT_DAMAGE_RANGED];

		// Early out
		if ( !nTotalDamage )
			return true;

		float flNormalizedDamage = (float)nRandomRangedCritDamage / TF_DAMAGE_CRIT_MULTIPLIER;
		m_flObservedCritChance.Set( flNormalizedDamage / ( flNormalizedDamage + ( nTotalDamage - nRandomRangedCritDamage ) ) );

		// DevMsg ( "SERVER: CritChance: %f Observed: %f\n", flCritChance, m_flObservedCritChance.Get() );
	}
#else
		// DevMsg ( "CLIENT: CritChance: %f Observed: %f\n", flCritChance, m_flObservedCritChance.Get() );
#endif // GAME_DLL
	
	if ( m_flObservedCritChance.Get() > flCritChance + 0.1f )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return the origin & angles for a projectile fired from the player's gun
//-----------------------------------------------------------------------------
void CTFWeaponBase::GetProjectileFireSetup( CTFPlayer *pPlayer, Vector vecOffset, Vector *vecSrc, QAngle *angForward, bool bHitTeammates /* = true */, float flEndDist /* = 2000 */)
{
	// Flip the firing offset if our view model is flipped.
	if ( IsViewModelFlipped() )
	{
		vecOffset.y *= -1;
	}

	int iCenterFireProjectile = 0;
	if ( iCenterFireProjectile == 1 )
	{
		vecOffset.y = 0;
	}

	QAngle angSpread = pPlayer->EyeAngles();
	Vector vecForward, vecRight, vecUp;
	AngleVectors( angSpread, &vecForward, &vecRight, &vecUp );

	Vector vecShootPos = pPlayer->Weapon_ShootPosition();

	// Estimate end point
	Vector endPos = vecShootPos + vecForward * flEndDist;	

	// Trace forward and find what's in front of us, and aim at that
	trace_t tr;

	if ( bHitTeammates )
	{
		CTraceFilterSimple traceFilter( pPlayer, COLLISION_GROUP_NONE );
		ITraceFilter *pFilterChain = NULL;

		CTraceFilterIgnoreFriendlyCombatItems traceFilterCombatItem( pPlayer, COLLISION_GROUP_NONE, GetTeamNumber() );

		CTraceFilterChain traceFilterChain( &traceFilter, pFilterChain );
		UTIL_TraceLine( vecShootPos, endPos, MASK_SOLID, &traceFilterChain, &tr );
	}
	else
	{
		CTraceFilterIgnoreTeammates filter( pPlayer, COLLISION_GROUP_NONE, pPlayer->GetTeamNumber() );
		UTIL_TraceLine( vecShootPos, endPos, MASK_SOLID, &filter, &tr );
	}

	// Offset actual start point
	*vecSrc = vecShootPos + (vecForward * vecOffset.x) + (vecRight * vecOffset.y) + (vecUp * vecOffset.z);

	// Find angles that will get us to our desired end point
	// Only use the trace end if it wasn't too close, which results
	// in visually bizarre forward angles
	if ( tr.fraction > 0.1 )
	{
		VectorAngles( tr.endpos - *vecSrc, *angForward );
	}
	else
	{
		VectorAngles( endPos - *vecSrc, *angForward );
	}
}

#if GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFWeaponBase::DisguiseWeaponThink( void )
{
	// Periodically check to make sure we are valid.
	// Disguise weapons are attached to a player, but not managed through the owned weapons list.
	CTFPlayer *pTFOwner = ToTFPlayer( GetOwner() );
	if ( !pTFOwner )
	{
		// We must have an owner to be valid.
		Drop( Vector( 0,0,0 ) );
		return;
	}

	if ( pTFOwner->m_Shared.GetDisguiseWeapon() != this )
	{
		// The owner's disguise weapon must be us, otherwise we are invalid.
		Drop( Vector( 0,0,0 ) );
		return;
	}

	SetContextThink( &CTFWeaponBase::DisguiseWeaponThink, gpGlobals->curtime + 0.5, "DisguiseWeaponThink" );
}
#endif // GAME_DLL

//-----------------------------------------------------------------------------
// Purpose: Owner is stunned.
//-----------------------------------------------------------------------------
void CTFWeaponBase::OnControlStunned( void )
{
	// Abort reloading.
	AbortReload();

	// Hide the weapon.
	SetWeaponVisible( false );
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::DeflectProjectiles()
{
	CTFPlayer *pOwner = ToTFPlayer( GetPlayerOwner() );
	if ( !pOwner )
		return false;

	if ( pOwner->GetWaterLevel() == WL_Eyes )
		return false;

	lagcompensation->StartLagCompensation( pOwner, pOwner->GetCurrentCommand() );

	Vector vecEye = pOwner->EyePosition();
	Vector vecForward, vecRight, vecUp;
	AngleVectors( pOwner->EyeAngles(), &vecForward, &vecRight, &vecUp );
	Vector vecSize = GetDeflectionSize();
	float flMaxElement = 0.0f;
	for ( int i = 0; i < 3; ++i )
	{
		flMaxElement = MAX( flMaxElement, vecSize[i] );
	}
	Vector vecCenter = vecEye + vecForward * flMaxElement;

	// Get a list of entities in the box defined by vecSize at VecCenter.
	// We will then try to deflect everything in the box.
	const int maxCollectedEntities = 64; // SEALTODO if we allow maxplayers above 64 then change this too
	CBaseEntity	*pObjects[ maxCollectedEntities ];
	int count = UTIL_EntitiesInBox( pObjects, maxCollectedEntities, vecCenter - vecSize, vecCenter + vecSize, FL_CLIENT | FL_GRENADE );

//	NDebugOverlay::Box( vecCenter, -vecSize, vecSize, 0, 255, 0, 40, 3 );

	bool bDeflected = false;
	bool bDeflectedPlayer = false;

	int iEnemyTeam = GetEnemyTeam( pOwner->GetTeamNumber() );
	bool bTruce = TFGameRules() && TFGameRules()->IsTruceActive() && pOwner->IsTruceValidForEnt();

	for ( int i = 0; i < count; i++ )
	{
		if ( pObjects[i] == pOwner )
			continue;

		if ( pObjects[i]->IsPlayer() && pObjects[i]->GetTeamNumber() == TEAM_SPECTATOR )
			continue;

		if ( !pObjects[i]->IsDeflectable() && !FClassnameIs( pObjects[i], "prop_physics" ) )
			continue;

		if ( pOwner->FVisible( pObjects[i], MASK_SOLID ) == false )
			continue;

		if ( bTruce && ( pObjects[i]->GetTeamNumber() == iEnemyTeam ) )
			continue;

		if ( pObjects[i]->IsPlayer() == true )
		{
			CTFPlayer *pTarget = ToTFPlayer( pObjects[i] );
			if ( pTarget )
			{
				bool bRes = DeflectPlayer( pTarget, pOwner, vecForward, vecCenter, vecSize );
				bDeflectedPlayer |= bRes;
				bDeflected |= bRes;
			}
		}
		else
		{
			bDeflected |= DeflectEntity( pObjects[i], pOwner, vecForward, vecCenter, vecSize );
		}
	}

	if ( bDeflected )
	{
		pOwner->SpeakConceptIfAllowed( MP_CONCEPT_DEFLECTED, "victim:0" );
		PlayDeflectionSound( bDeflectedPlayer );
	}

	lagcompensation->FinishLagCompensation( pOwner );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::DeflectPlayer( CTFPlayer *pTarget, CTFPlayer *pOwner, Vector &vecForward, Vector &vecCenter, Vector &vecSize )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFWeaponBase::DeflectEntity( CBaseEntity *pTarget, CTFPlayer *pOwner, Vector &vecForward, Vector &vecCenter, Vector &vecSize )
{
	Assert( pTarget );
	Assert( pOwner );

	Vector vecEye = pOwner->EyePosition();
	Vector vecVel = pTarget->GetAbsVelocity();

	// apply an impulse instead if this is a prop physics object
	if ( FClassnameIs( pTarget, "prop_physics" ) )
	{
		IPhysicsObject *pPhysicsObject = pTarget->VPhysicsGetObject();
		if ( pPhysicsObject && pTarget->CollisionProp() )
		{
			Vector vecDir = pTarget->WorldSpaceCenter() - vecEye;
			VectorNormalize( vecDir );
			float flVel = 50.0f * CTFWeaponBase::DeflectionForce( pTarget->CollisionProp()->OBBSize(), 90, 12.0f );
			pPhysicsObject->ApplyForceOffset( vecDir * flVel, vecEye );
		}
		return true;
	}


	AngularImpulse angularimp;

	CTraceFilterDeflection filter( pOwner, COLLISION_GROUP_NONE, pOwner->GetTeamNumber() );
	trace_t tr;
	UTIL_TraceLine( vecEye, vecEye + vecForward * MAX_TRACE_LENGTH, MASK_SOLID, &filter, &tr );
	Vector vecDir = pTarget->WorldSpaceCenter() - tr.endpos;
	VectorNormalize( vecDir );

	// Send the entity back where it came.
	// If we want per-entity physical deflection behavior this could move into ::Deflected
	IPhysicsObject *pPhysicsObject = pTarget->VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		pPhysicsObject->GetVelocity( &vecVel, &angularimp );
	}
	float flVel = vecVel.Length();
	vecVel = -flVel * vecDir;
	if ( pPhysicsObject )
	{
		if ( pPhysicsObject->IsMotionEnabled() == false )
		{
			vecDir = pOwner->WorldSpaceCenter() - pTarget->WorldSpaceCenter();
			VectorNormalize( vecDir );

			vecVel = -flVel * vecDir;
		}

		pPhysicsObject->EnableMotion( true );
		pPhysicsObject->SetVelocity( &vecVel, &angularimp );
	}
	else
	{
		pTarget->SetAbsVelocity( vecVel );
	}

	// Perform entity specific deflection behavior like team changing.
	pTarget->Deflected( pOwner, vecDir );

	QAngle newAngles;
	VectorAngles( -vecDir, newAngles );
	pTarget->SetAbsAngles( newAngles );

	CDisablePredictionFiltering disabler;
	DispatchParticleEffect( "deflect_fx", PATTACH_ABSORIGIN_FOLLOW, pTarget );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Static deflection helper.
//-----------------------------------------------------------------------------
float CTFWeaponBase::DeflectionForce( const Vector &size, float damage, float scale )
{ 
	float force = damage * ((48 * 48 * 82.0) / (size.x * size.y * size.z)) * scale;

	if ( force > 1000.0) 
	{
		force = 1000.0;
	}

	return force;
}

//-----------------------------------------------------------------------------
// Purpose: Static deflection helper.
//-----------------------------------------------------------------------------
void CTFWeaponBase::SendObjectDeflectedEvent( CTFPlayer *pNewOwner, CTFPlayer *pPrevOwner, int iWeaponID, CBaseAnimating *pObject )
{
	if ( pNewOwner && pPrevOwner )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "object_deflected" );
		if ( event )
		{
			event->SetInt( "userid", pNewOwner->GetUserID() );
			event->SetInt( "ownerid", pPrevOwner->GetUserID() );
			event->SetInt( "weaponid", iWeaponID );

			// Community request. We don't use object_entindex, but some server plugins do.
			event->SetInt( "object_entindex", pObject ? pObject->entindex() : 0 );

			gameeventmanager->FireEvent( event );
		}
	}
}
#endif

#if defined( CLIENT_DLL )

static ConVar	cl_bobcycle( "cl_bobcycle","0.8" );
static ConVar	cl_bobup( "cl_bobup","0.5" );

//-----------------------------------------------------------------------------
// Purpose: Helper function to calculate head bob
//-----------------------------------------------------------------------------
float CalcViewModelBobHelper( CBasePlayer *player, BobState_t *pBobState )
{
	Assert( pBobState );
	if ( !pBobState )
		return 0;

	float	cycle;

	//NOTENOTE: For now, let this cycle continue when in the air, because it snaps badly without it

	if ( ( !gpGlobals->frametime ) || ( player == NULL ) )
	{
		//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
		return 0.0f;// just use old value
	}

	//Find the speed of the player
	float speed = player->GetLocalVelocity().Length2D();
	float flmaxSpeedDelta = MAX( 0, (gpGlobals->curtime - pBobState->m_flLastBobTime ) * 320.0f );

	// don't allow too big speed changes
	speed = clamp( speed, pBobState->m_flLastSpeed-flmaxSpeedDelta, pBobState->m_flLastSpeed+flmaxSpeedDelta );
	speed = clamp( speed, -320, 320 );

	pBobState->m_flLastSpeed = speed;

	//FIXME: This maximum speed value must come from the server.
	//		 MaxSpeed() is not sufficient for dealing with sprinting - jdw

	float bob_offset = RemapVal( speed, 0, 320, 0.0f, 1.0f );

	pBobState->m_flBobTime += ( gpGlobals->curtime - pBobState->m_flLastBobTime ) * bob_offset;
	pBobState->m_flLastBobTime = gpGlobals->curtime;

	//Calculate the vertical bob
	cycle = pBobState->m_flBobTime - (int)(pBobState->m_flBobTime/cl_bobcycle.GetFloat())*cl_bobcycle.GetFloat();
	cycle /= cl_bobcycle.GetFloat();

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = M_PI * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
	}

	pBobState->m_flVerticalBob = speed*0.005f;
	pBobState->m_flVerticalBob = pBobState->m_flVerticalBob*0.3 + pBobState->m_flVerticalBob*0.7*sin(cycle);

	pBobState->m_flVerticalBob = clamp( pBobState->m_flVerticalBob, -7.0f, 4.0f );

	//Calculate the lateral bob
	cycle = pBobState->m_flBobTime - (int)(pBobState->m_flBobTime/cl_bobcycle.GetFloat()*2)*cl_bobcycle.GetFloat()*2;
	cycle /= cl_bobcycle.GetFloat()*2;

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = M_PI * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
	}

	pBobState->m_flLateralBob = speed*0.005f;
	pBobState->m_flLateralBob = pBobState->m_flLateralBob*0.3 + pBobState->m_flLateralBob*0.7*sin(cycle);
	pBobState->m_flLateralBob = clamp( pBobState->m_flLateralBob, -7.0f, 4.0f );

	//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Helper function to add head bob
//-----------------------------------------------------------------------------
void AddViewModelBobHelper( Vector &origin, QAngle &angles, BobState_t *pBobState )
{
	Assert( pBobState );
	if ( !pBobState )
		return;

	Vector	forward, right;
	AngleVectors( angles, &forward, &right, NULL );

	// Apply bob, but scaled down to 40%
	VectorMA( origin, pBobState->m_flVerticalBob * 0.4f, forward, origin );

	// Z bob a bit more
	origin[2] += pBobState->m_flVerticalBob * 0.1f;

	// bob the angles
	angles[ ROLL ]	+= pBobState->m_flVerticalBob * 0.5f;
	angles[ PITCH ]	-= pBobState->m_flVerticalBob * 0.4f;
	angles[ YAW ]	-= pBobState->m_flLateralBob  * 0.3f;

	VectorMA( origin, pBobState->m_flLateralBob * 0.2f, right, origin );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CTFWeaponBase::CalcViewmodelBob( void )
{
	CBasePlayer *player = ToBasePlayer( GetOwner() );
	//Assert( player );
	BobState_t *pBobState = GetBobState();
	if ( pBobState )
		return CalcViewModelBobHelper( player, pBobState );
	else
		return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CTFWeaponBase::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	// call helper functions to do the calculation
	BobState_t *pBobState = GetBobState();
	if ( pBobState )
	{
		CalcViewmodelBob();
		::AddViewModelBobHelper( origin, angles, pBobState );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the head bob state for this weapon, which is stored
//			in the view model.  Note that this this function can return
//			NULL if the player is dead or the view model is otherwise not present.
//-----------------------------------------------------------------------------
BobState_t *CTFWeaponBase::GetBobState()
{
	// get the view model for this weapon
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return NULL;
	CBaseViewModel *baseViewModel = pOwner->GetViewModel( m_nViewModelIndex );
	if ( baseViewModel == NULL )
		return NULL;
	CTFViewModel *viewModel = dynamic_cast<CTFViewModel *>(baseViewModel);
	CDODViewModel *DODviewModel = dynamic_cast<CDODViewModel *>(baseViewModel);
	Assert( viewModel );

	// get the bob state out of the view model
	// Okay so. When we're creating the different types of viewmodels there is a frame where the newly
	// created viewmodel is still using the weapon of the previous class which calls upon the bobstate
	// which crashes due to this specific viewmodel type so lets support it here just so it's fixed. - Seal
	if ( DODviewModel )
		return &( DODviewModel->GetBobState() );
	else
		return &( viewModel->GetBobState() );
}

//-----------------------------------------------------------------------------
// Purpose: Used for spy invisiblity material
//-----------------------------------------------------------------------------
int CTFWeaponBase::GetSkin()
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return 0;

	int iTeamNumber = pPlayer->GetTeamNumber();

#if defined( CLIENT_DLL )
	// Run client-only "is the viewer on the same team as the wielder" logic. Assumed to
	// always be false on the server.
	CTFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pLocalPlayer )
		return 0;

	int iLocalTeam = pLocalPlayer->GetTeamNumber();
	
	// We only show disguise weapon to the enemy team when owner is disguised
	bool bUseDisguiseWeapon = ( iTeamNumber != iLocalTeam && iLocalTeam > LAST_SHARED_TEAM );

	if ( bUseDisguiseWeapon && pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		if ( pLocalPlayer != pPlayer )
		{
			iTeamNumber = pPlayer->m_Shared.GetDisguiseTeam();
		}
	}
#endif // defined( CLIENT_DLL )

	// See if the item wants to override the skin
	int nSkin = GetSkinOverride();						// give custom gameplay code a chance to set whatever

	// If it didn't, fall back to the base skins
	if ( nSkin == -1 )
	{
		switch( iTeamNumber )
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
	}

	return nSkin;
}

bool CTFWeaponBase::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
{
	if( event == 6002 )
	{
		CEffectData data;
		pViewModel->GetAttachment( atoi(options), data.m_vOrigin, data.m_vAngles );
		data.m_nHitBox = GetWeaponID();
		DispatchEffect( "TF_EjectBrass", data );
		return true;
	}
	if ( event == AE_WPN_INCREMENTAMMO )
	{
		CTFPlayer *pPlayer = GetTFPlayerOwner();

		if ( pPlayer && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) > 0 && !m_bReloadedThroughAnimEvent )
		{
			m_iClip1 = min( ( m_iClip1 + 1 ), GetMaxClip1() );
			pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
		}

		m_bReloadedThroughAnimEvent = true;

		return true;
	}

	return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
}

ShadowType_t CTFWeaponBase::ShadowCastType( void )
{
	if ( IsEffectActive( EF_NODRAW | EF_NOSHADOW ) )
		return SHADOWS_NONE;

	if ( m_iState == WEAPON_IS_CARRIED_BY_PLAYER )
		return SHADOWS_NONE;

	return BaseClass::ShadowCastType();
}

//-----------------------------------------------------------------------------
// Purpose: Used for spy invisiblity material
//-----------------------------------------------------------------------------
class CWeaponInvisProxy : public CEntityMaterialProxy
{
public:

	CWeaponInvisProxy( void );
	virtual ~CWeaponInvisProxy( void );
	virtual bool Init( IMaterial *pMaterial, KeyValues* pKeyValues );
	virtual void OnBind( C_BaseEntity *pC_BaseEntity );
	virtual IMaterial * GetMaterial();

private:
	IMaterialVar *m_pPercentInvisible;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponInvisProxy::CWeaponInvisProxy( void )
{
	m_pPercentInvisible = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponInvisProxy::~CWeaponInvisProxy( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: Get pointer to the color value
// Input : *pMaterial - 
//-----------------------------------------------------------------------------
bool CWeaponInvisProxy::Init( IMaterial *pMaterial, KeyValues* pKeyValues )
{
	Assert( pMaterial );

	// Need to get the material var
	bool bFound;
	m_pPercentInvisible = pMaterial->FindVar( "$cloakfactor", &bFound );

	return bFound;
}

extern ConVar tf_teammate_max_invis;
//-----------------------------------------------------------------------------
// Purpose: 
// Input :
//-----------------------------------------------------------------------------
void CWeaponInvisProxy::OnBind( C_BaseEntity *pEnt )
{
	if( !m_pPercentInvisible )
		return;

	if ( !pEnt )
		return;

	C_BaseEntity *pMoveParent = pEnt->GetMoveParent();
	if ( !pMoveParent || !pMoveParent->IsPlayer() )
	{
		m_pPercentInvisible->SetFloatValue( 0.0f );
		return;
	}

	CTFPlayer *pPlayer = ToTFPlayer( pMoveParent );
	Assert( pPlayer );

	m_pPercentInvisible->SetFloatValue( pPlayer->GetEffectiveInvisibilityLevel() );
}

IMaterial *CWeaponInvisProxy::GetMaterial()
{
	if ( !m_pPercentInvisible )
		return NULL;

	return m_pPercentInvisible->GetOwningMaterial();
}

EXPOSE_INTERFACE( CWeaponInvisProxy, IMaterialProxy, "weapon_invis" IMATERIAL_PROXY_INTERFACE_VERSION );

#endif // CLIENT_DLL

bool WeaponID_IsSniperRifle( int iWeaponID )
{
	if ( iWeaponID == TF_WEAPON_SNIPERRIFLE )
		return true;
	else
		return false;
}
