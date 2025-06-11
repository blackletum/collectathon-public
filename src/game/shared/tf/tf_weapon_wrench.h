//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef TF_WEAPON_WRENCH_H
#define TF_WEAPON_WRENCH_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_melee.h"

#ifdef CLIENT_DLL
#define CTFWrench C_TFWrench
#endif

extern ConVar tf_construction_build_rate_multiplier;

//=============================================================================
//
// Wrench class.
//
class CTFWrench : public CTFWeaponBaseMelee
{
public:

	DECLARE_CLASS( CTFWrench, CTFWeaponBaseMelee );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CTFWrench();
	virtual int			GetWeaponID( void ) const			{ return TF_WEAPON_WRENCH; }
	virtual void		Smack( void );

	bool				IsPDQ( void ) { return false; }
	float				GetConstructionValue( void ) { return tf_construction_build_rate_multiplier.GetFloat(); }
	float				GetRepairValue( void ) { return 1.0; }

#ifdef GAME_DLL
	void				OnFriendlyBuildingHit( CBaseObject *pObject, CTFPlayer *pPlayer, Vector hitLoc );
#endif

private:

	CTFWrench( const CTFWrench & ) {}
};

#endif // TF_WEAPON_WRENCH_H
