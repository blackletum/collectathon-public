//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasegrenade.h"

#ifdef CLIENT_DLL
	#define CWeaponExplodingHandGrenade C_WeaponExplodingHandGrenade
#else 
	#include "dod_handgrenade.h"	//the thing that we throw
#endif


class CWeaponExplodingHandGrenade : public CWeaponDODBaseGrenade
{
public:
	DECLARE_CLASS( CWeaponExplodingHandGrenade, CWeaponDODBaseGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponExplodingHandGrenade() {}

	virtual int GetWeaponID( void ) const		{ return DOD_WEAPON_FRAG_US_LIVE; }

#ifndef CLIENT_DLL

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime = GRENADE_FUSE_LENGTH )
	{
		CDODHandGrenade::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, flLifeTime, GetWeaponID() );
	}

#endif

	virtual bool IsArmed( void ) { return true; }

private:
	CWeaponExplodingHandGrenade( const CWeaponExplodingHandGrenade & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponExplodingHandGrenade, DT_WeaponExplodingHandGrenade )

BEGIN_NETWORK_TABLE(CWeaponExplodingHandGrenade, DT_WeaponExplodingHandGrenade)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponExplodingHandGrenade )	//MATTTODO: are these necessary?
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( dod_weapon_frag_us_live, CWeaponExplodingHandGrenade );
PRECACHE_WEAPON_REGISTER( dod_weapon_frag_us_live );