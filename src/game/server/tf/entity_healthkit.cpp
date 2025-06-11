//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CTF HealthKit.
//
//=============================================================================//
#include "cbase.h"
#include "items.h"
#include "tf_gamerules.h"
#include "tf_shareddefs.h"
#include "tf_player.h"
#include "tf_team.h"
#include "engine/IEngineSound.h"
#include "entity_healthkit.h"

//=============================================================================
//
// CTF HealthKit defines.
//

#define TF_HEALTHKIT_MODEL			"models/items/healthkit.mdl"
#define TF_HEALTHKIT_PICKUP_SOUND	"HealthKit.Touch"

LINK_ENTITY_TO_CLASS( item_healthkit_full, CHealthKit );
LINK_ENTITY_TO_CLASS( item_healthkit_small, CHealthKitSmall );
LINK_ENTITY_TO_CLASS( item_healthkit_medium, CHealthKitMedium );

IMPLEMENT_AUTO_LIST( IHealthKitAutoList );

//=============================================================================
//
// CTF HealthKit functions.
//

//-----------------------------------------------------------------------------
// Purpose: Spawn function for the healthkit
//-----------------------------------------------------------------------------
void CHealthKit::Spawn( void )
{
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Precache function for the healthkit
//-----------------------------------------------------------------------------
void CHealthKit::Precache( void )
{
	PrecacheScriptSound( TF_HEALTHKIT_PICKUP_SOUND );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: MyTouch function for the healthkit
//-----------------------------------------------------------------------------
bool CHealthKit::MyTouch( CBasePlayer *pPlayer )
{
	bool bSuccess = false;

	if ( ValidTouch( pPlayer ) )
	{
		CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );
		Assert( pTFPlayer );

		bool bPerformPickup = false;

		float flHealth = ceil( ( pPlayer->GetMaxHealth() ) * PackRatios[GetPowerupSize()] );

		int nHealthGiven = pPlayer->TakeHealth( flHealth, DMG_GENERIC );

		if ( nHealthGiven > 0 )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "player_healed" );
			if ( event )
			{
				CTFPlayer *pOwner = ToTFPlayer( GetOwnerEntity() );
				int nHealerID = pOwner ? pOwner->GetUserID() : 0;

				event->SetInt( "priority", 1 );	// HLTV event priority
				event->SetInt( "patient", pPlayer->GetUserID() );
				event->SetInt( "healer", nHealerID );
				event->SetInt( "amount", nHealthGiven );
				gameeventmanager->FireEvent( event );
			}
		}

		if ( nHealthGiven > 0 || pTFPlayer->m_Shared.InCond( TF_COND_BURNING ) )
		{
			bPerformPickup = true;
			bSuccess = true;

			// subtract this from the drowndmg in case they're drowning and being healed at the same time
			pPlayer->AdjustDrownDmg( -1.0 * flHealth );

			CSingleUserRecipientFilter user( pPlayer );
			EmitSound( user, entindex(), TF_HEALTHKIT_PICKUP_SOUND );

			pTFPlayer->m_Shared.HealthKitPickupEffects( nHealthGiven );
		}

		if ( bPerformPickup )
		{
			CSingleUserRecipientFilter user( pPlayer );
			user.MakeReliable();
			UserMessageBegin( user, "ItemPickup" );
			WRITE_STRING( GetClassname() );
			MessageEnd();

			IGameEvent * event = gameeventmanager->CreateEvent( "item_pickup" );
			if( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				event->SetString( "item", GetHealthKitName() );
				gameeventmanager->FireEvent( event );
			}
		}
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CHealthKit::GetRespawnDelay( void )
{
	return g_pGameRules->FlItemRespawnTime( this );
}