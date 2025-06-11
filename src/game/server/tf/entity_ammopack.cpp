//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: CTF AmmoPack.
//
//=============================================================================//
#include "cbase.h"
#include "items.h"
#include "tf_gamerules.h"
#include "tf_shareddefs.h"
#include "tf_player.h"
#include "tf_team.h"
#include "engine/IEngineSound.h"
#include "entity_ammopack.h"

//=============================================================================
//
// CTF AmmoPack defines.
//

#define TF_AMMOPACK_PICKUP_SOUND	"AmmoPack.Touch"

LINK_ENTITY_TO_CLASS( item_ammopack_full, CAmmoPack );
LINK_ENTITY_TO_CLASS( item_ammopack_small, CAmmoPackSmall );
LINK_ENTITY_TO_CLASS( item_ammopack_medium, CAmmoPackMedium );

//=============================================================================
//
// CTF AmmoPack functions.
//

//-----------------------------------------------------------------------------
// Purpose: Spawn function for the ammopack
//-----------------------------------------------------------------------------
void CAmmoPack::Spawn( void )
{
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Precache function for the ammopack
//-----------------------------------------------------------------------------
void CAmmoPack::Precache( void )
{
	PrecacheScriptSound( TF_AMMOPACK_PICKUP_SOUND );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: MyTouch function for the ammopack
//-----------------------------------------------------------------------------
bool CAmmoPack::MyTouch( CBasePlayer *pPlayer )
{
	bool bSuccess = false;

	if ( ValidTouch( pPlayer ) )
	{
		CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );
		if ( !pTFPlayer )
			return false;

		int iMax;
		for ( int i = 1; i < GAME_AMMO_COUNT; i++ )
		{
			iMax = pTFPlayer->GetMaxAmmo( i );

			// Check if we have already succeeded, if so, only give ammo
			if ( bSuccess )
			{
				GivePlayerAmmo( pPlayer, iMax, i );
				continue;
			}

			bSuccess = ( GivePlayerAmmo( pPlayer, iMax, i ) > 0 );
		}

		float flPackRatio = PackRatios[GetPowerupSize()];

		if ( pTFPlayer->IsTF2Class() )
		{
			if ( pTFPlayer->m_Shared.AddToSpyCloakMeter( 100.0f * flPackRatio ) )
				bSuccess = true;
		}

		// did we give them anything?
		if ( bSuccess )
		{
			CSingleUserRecipientFilter filter( pPlayer );
			EmitSound( filter, entindex(), TF_AMMOPACK_PICKUP_SOUND );
		}
	}

	return bSuccess;
}

int CAmmoPack::GivePlayerAmmo( CBasePlayer *pPlayer, int iAmmo, int iAmmoIndex )
{
	float flPackRatio = PackRatios[GetPowerupSize()];
	return pPlayer->GiveAmmo( ceil( iAmmo * flPackRatio ), iAmmoIndex, true );
}