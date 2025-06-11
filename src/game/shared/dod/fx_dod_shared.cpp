//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "fx_dod_shared.h"
#include "tf_shareddefs.h"
#include "engine/ivdebugoverlay.h"

#include "tf_fx_shared.h"

#ifndef CLIENT_DLL
	#include "ilagcompensationmanager.h"
#endif

#ifndef CLIENT_DLL

//=============================================================================
//
// Explosions.
//
class CTEDODExplosion : public CBaseTempEntity
{
public:

	DECLARE_CLASS( CTEDODExplosion, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTEDODExplosion( const char *name );

public:

	Vector m_vecOrigin;
	Vector m_vecNormal;
};

// Singleton to fire explosion objects
static CTEDODExplosion g_TEExplosion( "DODExplosion" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEDODExplosion::CTEDODExplosion( const char *name ) : CBaseTempEntity( name )
{
	m_vecOrigin.Init();
	m_vecNormal.Init();
}

IMPLEMENT_SERVERCLASS_ST( CTEDODExplosion, DT_TEDODExplosion )
	SendPropFloat( SENDINFO_NOCHECK( m_vecOrigin[0] ), -1, SPROP_COORD_MP_INTEGRAL ),
	SendPropFloat( SENDINFO_NOCHECK( m_vecOrigin[1] ), -1, SPROP_COORD_MP_INTEGRAL ),
	SendPropFloat( SENDINFO_NOCHECK( m_vecOrigin[2] ), -1, SPROP_COORD_MP_INTEGRAL ),
	SendPropVector( SENDINFO_NOCHECK( m_vecNormal ), 6, 0, -1.0f, 1.0f ),
END_SEND_TABLE()

void TE_DODExplosion( IRecipientFilter &filter, float flDelay, const Vector &vecOrigin, const Vector &vecNormal )
{
	VectorCopy( vecOrigin, g_TEExplosion.m_vecOrigin );
	VectorCopy( vecNormal, g_TEExplosion.m_vecNormal );

	// Send it over the wire
	g_TEExplosion.Create( filter, flDelay );
}

#endif

#ifdef CLIENT_DLL

	#include "fx_impact.h"

	extern void FX_TracerSound( const Vector &start, const Vector &end, int iTracerType );

	// this is a cheap ripoff from CBaseCombatWeapon::WeaponSound():
	void FX_DODWeaponSound(
		int iPlayerIndex,
		WeaponSound_t sound_type,
		const Vector &vOrigin,
		CTFWeaponInfo *pWeaponInfo )
	{

		// If we have some sounds from the weapon classname.txt file, play a random one of them
		const char *shootsound = pWeaponInfo->aShootSounds[ sound_type ]; 
		if ( !shootsound || !shootsound[0] )
			return;

		CBroadcastRecipientFilter filter; // this is client side only

		if ( !te->CanPredict() )
			return;
				
		CBaseEntity::EmitSound( filter, iPlayerIndex, shootsound, &vOrigin ); 
	}

#else

	#include "te_firebullets.h"

#endif



// This runs on both the client and the server.
// On the server, it only does the damage calculations.
// On the client, it does all the effects.
void FX_FireBullets( 
	int	iPlayerIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	int	iWeaponID,
	int	iMode,
	int iSeed,
	float flSpread
	)
{
	bool bDoEffects = true;

#ifdef CLIENT_DLL
	C_TFPlayer *pPlayer = ToTFPlayer( ClientEntityList().GetBaseEntity( iPlayerIndex ) );
#else
	CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( iPlayerIndex) );
#endif

	const char *weaponAlias = WeaponIdToAlias( iWeaponID );

	if ( !weaponAlias )
	{
		DevMsg("FX_FireBullets: weapon alias for ID %i not found\n", iWeaponID );
		return;
	}

	//MATTTODO: Why are we looking up the weapon info again when every weapon
	// stores its own m_pWeaponInfo pointer?

	char wpnName[128];
	Q_snprintf( wpnName, sizeof( wpnName ), "%s", weaponAlias );
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( wpnName );

	if ( hWpnInfo == GetInvalidWeaponInfoHandle() )
	{
		DevMsg("FX_FireBullets: LookupWeaponInfoSlot failed for weapon %s\n", wpnName );
		return;
	}

	CTFWeaponInfo *pWeaponInfo = static_cast< CTFWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );

#ifdef CLIENT_DLL
	if( pPlayer && !pPlayer->IsDormant() )
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN );
#else
	if( pPlayer )
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN );
#endif	

#ifndef CLIENT_DLL
	// if this is server code, send the effect over to client as temp entity
	// Dispatch one message for all the bullet impacts and sounds.
	TE_DODFireBullets( 
		iPlayerIndex,
		vOrigin, 
		vAngles, 
		iWeaponID,
		iMode,
		iSeed,
		flSpread
	);

	bDoEffects = false; // no effects on server

	// Let the player remember the usercmd he fired a weapon on. Assists in making decisions about lag compensation.
	pPlayer->NoteWeaponFired();
#endif

	if ( bDoEffects )
	{
		FX_WeaponSound( iPlayerIndex, SINGLE, vOrigin, pWeaponInfo );
	}

	// Fire bullets, calculate impacts & effects
	if ( !pPlayer )
		return;
	
	StartGroupingSounds();

#if !defined (CLIENT_DLL)
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, pPlayer->GetCurrentCommand() );
#endif

	RandomSeed( iSeed );

	float x, y;
	do
	{
		x = random->RandomFloat( -0.5, 0.5 ) + random->RandomFloat( -0.5, 0.5 );
		y = random->RandomFloat( -0.5, 0.5 ) + random->RandomFloat( -0.5, 0.5 );
	} while ( (x * x + y * y) > 1.0f );

	Vector vecForward, vecRight, vecUp;
	AngleVectors( vAngles, &vecForward, &vecRight, &vecUp );

	Vector vecDirShooting = vecForward +
					x * flSpread * vecRight +
					y * flSpread * vecUp;

	vecDirShooting.NormalizeInPlace();

	FireBulletsInfo_t info( 1 /*shots*/, vOrigin, vecDirShooting, Vector( flSpread, flSpread, FLOAT32_NAN), MAX_COORD_RANGE, pWeaponInfo->iAmmoType );
	info.m_flDamage = pWeaponInfo->GetDODWeaponData().m_iDamage;
	info.m_pAttacker = pPlayer;

#if 0
	DevMsg("Damage = %i \n", pWeaponInfo->GetDODWeaponData().m_iDamage );
#endif

	pPlayer->DODFireBullets( info );

#ifdef CLIENT_DLL
	
	{
		trace_t tr;		
		UTIL_TraceLine( vOrigin, vOrigin + vecDirShooting * MAX_COORD_RANGE, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

		// if this is a local player, start at attachment on view model
		// else start on attachment on weapon model

		int iEntIndex = pPlayer->entindex();
		int iAttachment = 1;

		Vector vecStart = tr.startpos; 
		QAngle angAttachment;

		C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

		bool bInToolRecordingMode = clienttools->IsInRecordingMode();

		// try to align tracers to actual weapon barrel if possible
		if ( pPlayer->IsLocalPlayer() && !bInToolRecordingMode )
		{
			C_BaseViewModel *pViewModel = pPlayer->GetViewModel(0);

			if ( pViewModel )
			{
				iEntIndex = pViewModel->entindex();
				pViewModel->GetAttachment( iAttachment, vecStart, angAttachment );
			}
		}
		else if ( pLocalPlayer &&
					pLocalPlayer->GetObserverTarget() == pPlayer &&
					pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
		{	
			// get our observer target's view model

			C_BaseViewModel *pViewModel = pLocalPlayer->GetViewModel(0);

			if ( pViewModel )
			{
				iEntIndex = pViewModel->entindex();
				pViewModel->GetAttachment( iAttachment, vecStart, angAttachment );
			}
		}
		else if ( !pPlayer->IsDormant() )
		{
			// fill in with third person weapon model index
			C_BaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();

			if( pWeapon )
			{
            	iEntIndex = pWeapon->entindex();

				int nModelIndex = pWeapon->GetModelIndex();
				int nWorldModelIndex = pWeapon->GetWorldModelIndex();
				if ( bInToolRecordingMode && nModelIndex != nWorldModelIndex )
				{
					pWeapon->SetModelIndex( nWorldModelIndex );
				}

				pWeapon->GetAttachment( iAttachment, vecStart, angAttachment );

				if ( bInToolRecordingMode && nModelIndex != nWorldModelIndex )
				{
					pWeapon->SetModelIndex( nModelIndex );
				}
			}
		}

		switch( pWeaponInfo->m_DODWeaponData.m_iTracerType )
		{
		case 1:		// Machine gun, heavy tracer
			UTIL_Tracer( vecStart, tr.endpos, iEntIndex, TRACER_DONT_USE_ATTACHMENT, 5000.0, true, "DODBrightTracer" );
			break;

		case 2:		// rifle, smg, light tracer
			vecStart += vecDirShooting * 150;
			UTIL_Tracer( vecStart, tr.endpos, iEntIndex, TRACER_DONT_USE_ATTACHMENT, 5000.0, true, "DODFaintTracer" );
			break;

		case 0:	// pistols etc, just do the sound
			{
				FX_TracerSound( vecStart, tr.endpos, TRACER_TYPE_DEFAULT );
			}
		default:
			break;
		}
	}
#endif

#if !defined (CLIENT_DLL)
	lagcompensation->FinishLagCompensation( pPlayer );
#endif

	EndGroupingSounds();
}

