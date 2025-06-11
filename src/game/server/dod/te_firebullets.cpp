//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetempentity.h"


#define NUM_BULLET_SEED_BITS 8


//-----------------------------------------------------------------------------
// Purpose: Display's a blood sprite
//-----------------------------------------------------------------------------
class CTEDODFireBullets : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEDODFireBullets, CBaseTempEntity );
	DECLARE_SERVERCLASS();

					CTEDODFireBullets( const char *name );
	virtual			~CTEDODFireBullets( void );

	virtual	void	Create( IRecipientFilter& filter, float delay = 0.0f );


public:
	CNetworkVar( int, m_iPlayer );
	CNetworkVector( m_vecOrigin );
	CNetworkQAngle( m_vecAngles );
	CNetworkVar( int, m_iWeaponID );
	CNetworkVar( int, m_iMode );
	CNetworkVar( int, m_iSeed );
	CNetworkVar( float, m_flSpread );
	
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEDODFireBullets::CTEDODFireBullets( const char *name ) :
	CBaseTempEntity( name )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEDODFireBullets::~CTEDODFireBullets( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//-----------------------------------------------------------------------------
void CTEDODFireBullets::Create( IRecipientFilter& filter, float delay )
{
	engine->PlaybackTempEntity( filter, delay, 
		(void *)this, GetServerClass()->m_pTable, GetServerClass()->m_ClassID );
}

IMPLEMENT_SERVERCLASS_ST_NOBASE(CTEDODFireBullets, DT_TEDODFireBullets)
	SendPropVector( SENDINFO(m_vecOrigin), -1, SPROP_COORD ),
	SendPropAngle( SENDINFO_VECTORELEM( m_vecAngles, 0 ), 13, 0 ),
	SendPropAngle( SENDINFO_VECTORELEM( m_vecAngles, 1 ), 13, 0 ),
	SendPropInt( SENDINFO( m_iWeaponID ), 5, SPROP_UNSIGNED ), // max 31 weapons
	SendPropInt( SENDINFO( m_iMode ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iSeed ), NUM_BULLET_SEED_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iPlayer ), 6, SPROP_UNSIGNED ), 	// max 64 players, see MAX_PLAYERS
	SendPropFloat( SENDINFO( m_flSpread ), 10, 0, 0, 1 ),	
END_SEND_TABLE()


// Singleton
static CTEDODFireBullets g_TEDODFireBullets( "DODFireBullets" );


void TE_DODFireBullets( 
	int	iPlayerIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	int	iWeaponID,
	int	iMode,
	int iSeed,
	float flSpread )
{
	CPASFilter filter( vOrigin );
	filter.UsePredictionRules();

	g_TEDODFireBullets.m_iPlayer = iPlayerIndex-1;
	g_TEDODFireBullets.m_vecOrigin = vOrigin;
	g_TEDODFireBullets.m_vecAngles = vAngles;
	g_TEDODFireBullets.m_iSeed = iSeed;
	g_TEDODFireBullets.m_flSpread = flSpread;
	g_TEDODFireBullets.m_iMode = iMode;
	g_TEDODFireBullets.m_iWeaponID = iWeaponID;

	Assert( iSeed < (1 << NUM_BULLET_SEED_BITS) );
	
	g_TEDODFireBullets.Create( filter, 0 );
}
