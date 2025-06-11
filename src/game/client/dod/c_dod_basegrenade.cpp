//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "c_dod_basegrenade.h"


#include "c_tf_player.h"
#include "dod_shareddefs.h"
#include "dodoverview.h"

IMPLEMENT_NETWORKCLASS_ALIASED( DODBaseGrenade, DT_DODBaseGrenade )

BEGIN_NETWORK_TABLE(C_DODBaseGrenade, DT_DODBaseGrenade )
	RecvPropVector( RECVINFO( m_vInitialVelocity ) ),
	RecvPropInt( RECVINFO( m_iDeflected ) ),
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_DODBaseGrenade::~C_DODBaseGrenade()
{
	GetDODOverview()->RemoveGrenade( this );
	ParticleProp()->StopEmission();
}

void C_DODBaseGrenade::PostDataUpdate( DataUpdateType_t type )
{
	BaseClass::PostDataUpdate( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		// Now stick our initial velocity into the interpolation history 
		CInterpolatedVar< Vector > &interpolator = GetOriginInterpolator();

		interpolator.ClearHistory();
		float changeTime = GetLastChangeTime( LATCH_SIMULATION_VAR );

		// Add a sample 1 second back.
		Vector vCurOrigin = GetLocalOrigin() - m_vInitialVelocity;
		interpolator.AddToHead( changeTime - 1.0, &vCurOrigin, false );

		// Add the current sample.
		vCurOrigin = GetLocalOrigin();
		interpolator.AddToHead( changeTime, &vCurOrigin, false );

		// BUG ? this may call multiple times
		GetDODOverview()->AddGrenade( this );

		const char *pszParticleTrail = GetParticleTrailName();
		if ( pszParticleTrail )
		{
			ParticleProp()->Create( pszParticleTrail, PATTACH_ABSORIGIN_FOLLOW );
		}
	}
}

int C_DODBaseGrenade::DrawModel( int flags )
{
	if( m_flSpawnTime + 0.15 > gpGlobals->curtime )
		return 0;

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pPlayer && GetAbsVelocity().Length() < 30 )
	{
		//SEALTODO
		//pPlayer->CheckGrenadeHint( GetAbsOrigin() );
	}

	return BaseClass::DrawModel( flags );
}

void C_DODBaseGrenade::Spawn()
{
	m_flSpawnTime = gpGlobals->curtime;
	BaseClass::Spawn();
}

const char *C_DODBaseGrenade::GetOverviewSpriteName( void )
{
	const char *pszSprite = "";

	switch( GetTeamNumber() )
	{
	case TF_TEAM_RED:
		pszSprite = "sprites/minimap_icons/grenade_hltv";
		break;
	case TF_TEAM_BLUE:
		pszSprite = "sprites/minimap_icons/stick_hltv";
		break;
	default:
		break;
	}

	return pszSprite;
}

IMPLEMENT_NETWORKCLASS_ALIASED( DODRifleGrenadeUS, DT_DODRifleGrenadeUS )

BEGIN_NETWORK_TABLE(C_DODRifleGrenadeUS, DT_DODRifleGrenadeUS )
END_NETWORK_TABLE()


IMPLEMENT_CLIENTCLASS_DT(C_DODRifleGrenadeGER, DT_DODRifleGrenadeGER, CDODRifleGrenadeGER)
END_RECV_TABLE()