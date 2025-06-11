//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
//
//=============================================================================
#include "cbase.h"
#include "pc_weapon_aigis_arms.h"

//=============================================================================
//
// Weapon Aigis Arms tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( PCAigisArms, DT_WeaponAigisArms )

BEGIN_NETWORK_TABLE( CPCAigisArms, DT_WeaponAigisArms )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CPCAigisArms )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( pc_weapon_aigis_arms, CPCAigisArms );
PRECACHE_WEAPON_REGISTER( pc_weapon_aigis_arms );

// Server specific.
#ifndef CLIENT_DLL
BEGIN_DATADESC( CPCAigisArms )
END_DATADESC()
#endif

//=============================================================================
//
// Weapon Aigis Arms functions.
//