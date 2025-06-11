//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CTF HealthKit.
//
//=============================================================================//
#ifndef ENTITY_HEALTHKIT_H
#define ENTITY_HEALTHKIT_H

#ifdef _WIN32
#pragma once
#endif

#include "tf_powerup.h"
#include "tf_gamerules.h"

//=============================================================================
//
// CTF HealthKit class.
//

DECLARE_AUTO_LIST( IHealthKitAutoList );

class CHealthKit : public CTFPowerup, public IHealthKitAutoList
{
public:
	DECLARE_CLASS( CHealthKit, CTFPowerup );

	void	Spawn( void );
	virtual void Precache( void );
	virtual bool MyTouch( CBasePlayer *pPlayer );

	virtual const char *GetDefaultPowerupModel( void )
	{ 
		return "models/items/medkit_large.mdl";
	}

	powerupsize_t	GetPowerupSize( void ) { return POWERUP_FULL; }
	virtual const char *GetHealthKitName( void ) { return "medkit_large"; }

	virtual float	GetRespawnDelay( void );
};

class CHealthKitSmall : public CHealthKit
{
public:
	DECLARE_CLASS( CHealthKitSmall, CHealthKit );
	powerupsize_t	GetPowerupSize( void ) { return POWERUP_SMALL; }
	virtual const char *GetHealthKitName( void ) { return "medkit_small"; }

	virtual void Precache( void )
	{
		BaseClass::Precache();
	}

	virtual const char *GetDefaultPowerupModel( void )
	{
		return "models/items/medkit_small.mdl"; 
	}
};

class CHealthKitMedium : public CHealthKit
{
public:
	DECLARE_CLASS( CHealthKitMedium, CHealthKit );
	powerupsize_t	GetPowerupSize( void ) { return POWERUP_MEDIUM; }
	virtual const char *GetHealthKitName( void ) { return "medkit_medium"; }

	virtual void Precache( void )
	{
		BaseClass::Precache();
	}

	virtual const char *GetDefaultPowerupModel( void )
	{
		return "models/items/medkit_medium.mdl"; 
	}
};

#endif // ENTITY_HEALTHKIT_H


