//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	Weapons.
//
//=============================================================================
#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "pc_weaponbase.h"
#include "ammodef.h"
#include "tf_gamerules.h"
#include "eventlist.h"
#include "activitylist.h"

// Server specific.
#if !defined( CLIENT_DLL )
#include "tf_player.h"
// Client specific.
#else
#include "c_tf_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
extern ConVar cl_flipviewmodels;
#endif

Vector head_hull_mins( -16, -16, -18 );
Vector head_hull_maxs( 16, 16, 18 );

//=============================================================================
//
// Global functions.
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool IsAmmoType( int iAmmoType, const char *pAmmoName )
{
	return GetAmmoDef()->Index( pAmmoName ) == iAmmoType;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void FindHullIntersection( const Vector &vecSrc, trace_t &tr, const Vector &mins, const Vector &maxs, CBaseEntity *pEntity )
{
	int	i, j, k;
	trace_t tmpTrace;
	Vector vecEnd;
	float distance = 1e6f;
	Vector minmaxs[2] = {mins, maxs};
	Vector vecHullEnd = tr.endpos;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, MASK_SOLID, pEntity, COLLISION_GROUP_NONE, &tmpTrace );
	if ( tmpTrace.fraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, pEntity, COLLISION_GROUP_NONE, &tmpTrace );
				if ( tmpTrace.fraction < 1.0 )
				{
					float thisDistance = (tmpTrace.endpos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}

//=============================================================================
//
// PCWeaponBase tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( PCWeaponBase, DT_PCWeaponBase )

#ifdef GAME_DLL
void* SendProxy_SendActiveLocalWeaponDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );
void* SendProxy_SendNonLocalWeaponDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID );
#endif

//-----------------------------------------------------------------------------
// Purpose: Only sent to the player holding it.
//-----------------------------------------------------------------------------
BEGIN_NETWORK_TABLE_NOBASE( CPCWeaponBase, DT_LocalPCWeaponData )
#if defined( CLIENT_DLL )
#else
#endif
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Variables sent at low precision to non-holding observers.
//-----------------------------------------------------------------------------
BEGIN_NETWORK_TABLE_NOBASE( CPCWeaponBase, DT_PCWeaponDataNonLocal )
END_NETWORK_TABLE()

BEGIN_NETWORK_TABLE( CPCWeaponBase, DT_PCWeaponBase )
// Client specific.
#ifdef CLIENT_DLL
	RecvPropDataTable("LocalActivePCWeaponData", 0, 0, &REFERENCE_RECV_TABLE(DT_LocalPCWeaponData)),
	RecvPropDataTable("NonLocalPCWeaponData", 0, 0, &REFERENCE_RECV_TABLE(DT_PCWeaponDataNonLocal)),
#else
// Server specific.
	SendPropDataTable("LocalActivePCWeaponData", 0, &REFERENCE_SEND_TABLE(DT_LocalPCWeaponData), SendProxy_SendActiveLocalWeaponDataTable ),
	SendPropDataTable("NonLocalPCWeaponData", 0, &REFERENCE_SEND_TABLE(DT_PCWeaponDataNonLocal), SendProxy_SendNonLocalWeaponDataTable ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CPCWeaponBase ) 
#ifdef CLIENT_DLL
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( pc_weapon_base, CPCWeaponBase );

// Server specific.
#if !defined( CLIENT_DLL )

BEGIN_DATADESC( CPCWeaponBase )
END_DATADESC()

// Client specific
#else
#endif

//=============================================================================
//
// PCWeaponBase shared functions.
//

// -----------------------------------------------------------------------------
// Purpose: Constructor.
// -----------------------------------------------------------------------------
CPCWeaponBase::CPCWeaponBase()
{
}

CPCWeaponBase::~CPCWeaponBase()
{
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
void CPCWeaponBase::Spawn()
{
	BaseClass::Spawn();

	// Set this here to allow players to shoot dropped weapons
	SetCollisionGroup( COLLISION_GROUP_WEAPON );

	if ( GetPlayerOwner() )
	{
		ChangeTeam( GetPlayerOwner()->GetTeamNumber() );
	}
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
CBasePlayer *CPCWeaponBase::GetPlayerOwner() const
{
	return dynamic_cast<CBasePlayer*>( GetOwner() );
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
CTFPlayer *CPCWeaponBase::GetTFPlayerOwner() const
{
	return dynamic_cast<CTFPlayer*>( GetOwner() );
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
const CTFWeaponInfo &CPCWeaponBase::GetPCWpnData() const
{
	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CTFWeaponInfo *pTFInfo = dynamic_cast< const CTFWeaponInfo* >( pWeaponInfo );
	Assert( pTFInfo );
	return *pTFInfo;
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
int CPCWeaponBase::GetWeaponID( void ) const
{
	Assert( false ); 
	return TF_WEAPON_NONE; 
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
bool CPCWeaponBase::IsWeapon( int iWeapon ) const
{ 
	return GetWeaponID() == iWeapon; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPCWeaponBase::IsViewModelFlipped( void )
{
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return false;

#ifdef GAME_DLL
	if ( m_bFlipViewModel != pPlayer->m_bFlipViewModels )
	{
		return true;
	}
#else
	if ( m_bFlipViewModel != cl_flipviewmodels.GetBool() )
	{
		return true;
	}
#endif

	return false;
}

// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
const char *CPCWeaponBase::GetViewModel( int iViewModel ) const
{
	if ( GetPlayerOwner() == NULL )
	{
		return BaseClass::GetViewModel();
	}

	return GetPCWpnData().szViewModel;
}