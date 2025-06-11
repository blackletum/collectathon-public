//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
//
//=============================================================================
#ifndef PC_WEAPON_AIGIS_ARMS_H
#define PC_WEAPON_AIGIS_ARMS_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_gun.h"

// Client specific.
#ifdef CLIENT_DLL
#define CPCAigisArms C_PCAigisArms
#endif

//=============================================================================
//
// PC Weapon Aigis Arms
//
class CPCAigisArms : public CTFWeaponBaseGun
{
public:

	DECLARE_CLASS( CPCAigisArms, CTFWeaponBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

// Server specific.
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CPCAigisArms() {}
	~CPCAigisArms() {}

	virtual int		GetWeaponID( void ) const			{ return PC_WEAPON_AIGIS_ARMS; }

private:

	CPCAigisArms( const CPCAigisArms & ) {}
};

#endif // PC_WEAPON_AIGIS_ARMS_H