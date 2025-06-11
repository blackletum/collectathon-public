//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"
#include "fx_cs_shared.h"

#ifdef CLIENT_DLL
	#include "c_tf_player.h"
#else
	#include "tf_player.h"
#endif


IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCSBaseGun, DT_WeaponCSBaseGun )

BEGIN_NETWORK_TABLE( CWeaponCSBaseGun, DT_WeaponCSBaseGun )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCSBaseGun )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_csbase_gun, CWeaponCSBaseGun );



CWeaponCSBaseGun::CWeaponCSBaseGun()
{
}

void CWeaponCSBaseGun::Spawn()
{
	m_flAccuracy = 0.2;
	m_bDelayFire = false;
	m_zoomFullyActiveTime = -1.0f;

	BaseClass::Spawn();
}


bool CWeaponCSBaseGun::Deploy()
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return false;

	m_flAccuracy = 0.2;
	pPlayer->m_iShotsFired = 0;
	m_bDelayFire = false;
	m_zoomFullyActiveTime = -1.0f;

	return BaseClass::Deploy();
}

void CWeaponCSBaseGun::ItemPostFrame()
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();

	if ( !pPlayer )
		return;

	//GOOSEMAN : Return zoom level back to previous zoom level before we fired a shot. This is used only for the AWP.
	// And Scout.
	// SEALTODO CS
#if 0
	if ( (m_flNextPrimaryAttack <= gpGlobals->curtime) && (pPlayer->m_bResumeZoom == TRUE) )
	{
		pPlayer->m_bResumeZoom = false;
		
		if ( m_iClip1 != 0 || ( GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD ) )
		{
			m_weaponMode = TF_WEAPON_SECONDARY_MODE;
			pPlayer->SetFOV( pPlayer, pPlayer->m_iLastZoom, 0.05f );
			m_zoomFullyActiveTime = gpGlobals->curtime + 0.05f;// Make sure we think that we are zooming on the server so we don't get instant acc bonus
		}
	}
#endif

	BaseClass::ItemPostFrame();
}


void CWeaponCSBaseGun::PrimaryAttack()
{
	// Derived classes should implement this and call CSBaseGunFire.
	Assert( false );
}

bool CWeaponCSBaseGun::CSBaseGunFire( float flCycleTime, int weaponMode )
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return false;

	const CTFWeaponInfo &pCSInfo = GetPCWpnData();

	m_bDelayFire = true;

	if ( m_iClip1 > 0 )
	{
		pPlayer->m_iShotsFired++;
	
		// These modifications feed back into flSpread eventually.
		if ( pCSInfo.m_CSWeaponData.m_flAccuracyDivisor != -1 )
		{
			int iShotsFired = pPlayer->m_iShotsFired;

			if ( pCSInfo.m_CSWeaponData.m_bAccuracyQuadratic )
				iShotsFired = iShotsFired * iShotsFired;
			else
				iShotsFired = iShotsFired * iShotsFired * iShotsFired;

			m_flAccuracy = ( iShotsFired / pCSInfo.m_CSWeaponData.m_flAccuracyDivisor ) + pCSInfo.m_CSWeaponData.m_flAccuracyOffset;
			
			if ( m_flAccuracy > pCSInfo.m_CSWeaponData.m_flMaxInaccuracy )
				m_flAccuracy = pCSInfo.m_CSWeaponData.m_flMaxInaccuracy;
		}
	}
	else
	{
		m_flAccuracy = 0;

		if ( m_bFireOnEmpty )
		{
			PlayEmptySound();

			// NOTE[pmf]: we don't want to actually play the dry fire animations, as most seem to depict the weapon actually firing.
			// SendWeaponAnim( ACT_VM_DRYFIRE );

			m_bFireOnEmpty = false;
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.1f;
		}

		return false;
	}

	float flCurAttack = CalculateNextAttackTime( flCycleTime );

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	m_iClip1--;

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	FX_CSFireBullets(
		pPlayer->entindex(),
		pPlayer->Weapon_ShootPosition(),
		pPlayer->EyeAngles() + 2.0f * pPlayer->GetPunchAngle(),
		GetWeaponID(),
		weaponMode,
		CBaseEntity::GetPredictionRandomSeed() & 255,
		GetInaccuracy(),
		GetSpread(), 
		flCurAttack );

	DoFireEffects();

	SetWeaponIdleTime( gpGlobals->curtime + GetPCWpnData().m_CSWeaponData.m_flTimeToIdleAfterFire );

	// update accuracy
	m_fAccuracyPenalty += GetPCWpnData().m_CSWeaponData.m_fInaccuracyImpulseFire[weaponMode];

	return true;
}


void CWeaponCSBaseGun::DoFireEffects()
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	
	if ( pPlayer )
		 pPlayer->DoMuzzleFlash();
}


bool CWeaponCSBaseGun::Reload()
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return false;

	if (pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0)
		return false;

	pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), 0.0f );

	int iResult = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( !iResult )
		return false;

	pPlayer->SetAnimation( PLAYER_RELOAD );

	if ((iResult) && (pPlayer->GetFOV() != pPlayer->GetDefaultFOV()))
	{
		pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV() );
	}

	m_flAccuracy = 0.2;
	pPlayer->m_iShotsFired = 0;
	m_bDelayFire = false;

#if 0
	pPlayer->SetShieldDrawnState( false );
#endif
	return true;
}

void CWeaponCSBaseGun::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	// only idle if the slid isn't back
	if ( m_iClip1 != 0 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + GetPCWpnData().m_CSWeaponData.m_flIdleInterval );
		SendWeaponAnim( ACT_VM_IDLE );
	}
}
