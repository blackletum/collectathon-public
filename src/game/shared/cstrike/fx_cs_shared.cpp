//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "fx_cs_shared.h"
#include "weapon_csbase.h"

#ifndef CLIENT_DLL
	#include "ilagcompensationmanager.h"
#endif

#include "tf_fx_shared.h"

ConVar weapon_accuracy_logging( "weapon_accuracy_logging", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_ARCHIVE );

#ifdef CLIENT_DLL
	#include "fx_impact.h"
#else
	#include "te_shotgun_shot.h"
#endif


// This runs on both the client and the server.
// On the server, it only does the damage calculations.
// On the client, it does all the effects.
void FX_CSFireBullets( 
	int	iPlayerIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	int	iWeaponID,
	int	iMode,
	int iSeed,
	float fInaccuracy,
	float fSpread,
	float flSoundTime
	)
{
	// Get the weapon information.
	const char *pszWeaponAlias = WeaponIdToAlias( iWeaponID );
	if ( !pszWeaponAlias )
	{
		DevMsg( 1, "FX_FireBullets: weapon alias for ID %i not found\n", iWeaponID );
		return;
	}

	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( pszWeaponAlias );
	if ( hWpnInfo == GetInvalidWeaponInfoHandle() )
	{
		DevMsg( 1, "FX_FireBullets: LookupWeaponInfoSlot failed for weapon %s\n", pszWeaponAlias );
		return;
	}

	CTFWeaponInfo *pWeaponInfo = static_cast<CTFWeaponInfo*>( GetFileWeaponInfoFromHandle( hWpnInfo ) );
	if( !pWeaponInfo )
		return;

	bool bDoEffects = true;

#ifdef CLIENT_DLL
	C_TFPlayer *pPlayer = ToTFPlayer( ClientEntityList().GetBaseEntity( iPlayerIndex ) );
#else
	CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( iPlayerIndex) );
#endif

#if !defined(CLIENT_DLL)
	if ( weapon_accuracy_logging.GetBool() )
	{
		char szFlags[256];

		V_strcpy(szFlags, " ");

// #if defined(CLIENT_DLL)
// 		V_strcat(szFlags, "CLIENT ", sizeof(szFlags));
// #else
// 		V_strcat(szFlags, "SERVER ", sizeof(szFlags));
// #endif
// 
		if ( pPlayer->GetMoveType() == MOVETYPE_LADDER )
			V_strcat(szFlags, "LADDER ", sizeof(szFlags));

		if ( FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			V_strcat(szFlags, "GROUND ", sizeof(szFlags));

		if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING) )
			V_strcat(szFlags, "DUCKING ", sizeof(szFlags));

		float fVelocity = pPlayer->GetAbsVelocity().Length2D();

		Msg("FireBullets @ %10f [ %s ]: inaccuracy=%f  spread=%f  max dispersion=%f  mode=%2i  vel=%10f  seed=%3i  %s\n", 
			gpGlobals->curtime, pszWeaponAlias, fInaccuracy, fSpread, fInaccuracy + fSpread, iMode, fVelocity, iSeed, szFlags);
	}
#endif

	// Do the firing animation event.
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// SEALTODO
		//if ( iMode == TF_WEAPON_PRIMARY_MODE )
		//	pPlayer->GetPlayerAnimState()->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );
		//else
		//	pPlayer->GetPlayerAnimState()->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_SECONDARY );
	}

#ifndef CLIENT_DLL
	// if this is server code, send the effect over to client as temp entity
	// Dispatch one message for all the bullet impacts and sounds.
	TE_CSFireBullets( 
		iPlayerIndex,
		vOrigin, 
		vAngles, 
		iWeaponID,
		iMode,
		iSeed,
		fInaccuracy,
		fSpread
		);


	// Let the player remember the usercmd he fired a weapon on. Assists in making decisions about lag compensation.
	pPlayer->NoteWeaponFired();

	bDoEffects = false; // no effects on server
#endif

	iSeed++;

	int		iDamage = pWeaponInfo->m_CSWeaponData.m_iDamage;
	float	flRange = pWeaponInfo->m_CSWeaponData.m_flRange;
	int		iPenetration = pWeaponInfo->m_CSWeaponData.m_iPenetration;
	float	flRangeModifier = pWeaponInfo->m_CSWeaponData.m_flRangeModifier;
	int		iAmmoType = pWeaponInfo->iAmmoType;

	WeaponSound_t sound_type = SINGLE;

	// CS HACK, tweak some weapon values based on primary/secondary mode

	if ( iWeaponID == CS_WEAPON_GLOCK )
	{
		if ( iMode == TF_WEAPON_SECONDARY_MODE )
		{
			iDamage = 18;	// reduced power for burst shots
			flRangeModifier = 0.9f;
		}
	}
	else if ( iWeaponID == CS_WEAPON_M4A1 )
	{
		if ( iMode == TF_WEAPON_SECONDARY_MODE )
		{
			flRangeModifier = 0.95f; // slower bullets in silenced mode
			sound_type = SPECIAL1;
		}
	}
	else if ( iWeaponID == CS_WEAPON_USP )
	{
		if ( iMode == TF_WEAPON_SECONDARY_MODE )
		{
			iDamage = 30; // reduced damage in silenced mode
			sound_type = SPECIAL1;
		}
	}

	if ( bDoEffects)
	{
		FX_WeaponSound( iPlayerIndex, sound_type, vOrigin, pWeaponInfo );
	}


	// Fire bullets, calculate impacts & effects

	if ( !pPlayer )
		return;
	
	StartGroupingSounds();

#ifdef GAME_DLL
	// SEALTODO
//	pPlayer->StartNewBulletGroup();
#endif

#if !defined (CLIENT_DLL)
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, pPlayer->GetCurrentCommand() );
#endif

	RandomSeed( iSeed );	// init random system with this seed

	// Get accuracy displacement
	float fTheta0 = RandomFloat(0.0f, 2.0f * M_PI);
	float fRadius0 = RandomFloat(0.0f, fInaccuracy);
	float x0 = fRadius0 * cosf(fTheta0);
	float y0 = fRadius0 * sinf(fTheta0);

	const int kMaxBullets = 16;
	float x1[kMaxBullets], y1[kMaxBullets];
	Assert(pWeaponInfo->m_CSWeaponData.m_iBullets <= kMaxBullets);

	// the RNG can be desynchronized by FireBullet(), so pre-generate all spread offsets
	for ( int iBullet=0; iBullet < pWeaponInfo->m_CSWeaponData.m_iBullets; iBullet++ )
	{
		float fTheta1 = RandomFloat(0.0f, 2.0f * M_PI);
		float fRadius1 = RandomFloat(0.0f, fSpread);
		x1[iBullet] = fRadius1 * cosf(fTheta1);
		y1[iBullet] = fRadius1 * sinf(fTheta1);
	}

	for ( int iBullet=0; iBullet < pWeaponInfo->m_CSWeaponData.m_iBullets; iBullet++ )
	{
		pPlayer->CSFireBullet(
			vOrigin,
			vAngles,
			flRange,
			iPenetration,
			iAmmoType,
			iDamage,
			flRangeModifier,
			pPlayer,
			bDoEffects,
			x0 + x1[iBullet], y0 + y1[iBullet] );
	}

#if !defined (CLIENT_DLL)
	lagcompensation->FinishLagCompensation( pPlayer );
#endif

	EndGroupingSounds();
}

// This runs on both the client and the server.
// On the server, it dispatches a TE_PlantBomb to visible clients.
// On the client, it plays the planting animation.
void FX_PlantBomb( int iPlayerIndex, const Vector &vOrigin, PlantBombOption_t option )
{
#ifdef CLIENT_DLL
	C_TFPlayer *pPlayer = ToTFPlayer( ClientEntityList().GetBaseEntity( iPlayerIndex ) );
#else
	CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( iPlayerIndex) );
#endif

	// Do the firing animation event.
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		switch ( option )
		{
		case PLANTBOMB_PLANT:
			{
				// SEALTODO
//				pPlayer->GetPlayerAnimState()->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );
			}
			break;

		case PLANTBOMB_ABORT:
			{
				// SEALTODO
//				pPlayer->GetPlayerAnimState()->DoAnimationEvent( PLAYERANIMEVENT_CLEAR_FIRING );
			}
			break;
		}
	}

#ifndef CLIENT_DLL
	// if this is server code, send the effect over to client as temp entity
	// Dispatch one message for all the bullet impacts and sounds.
	TE_PlantBomb( iPlayerIndex, vOrigin, option );
#endif
}

