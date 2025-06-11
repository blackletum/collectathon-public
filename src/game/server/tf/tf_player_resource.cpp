//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: TF's custom CPlayerResource
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "player_resource.h"
#include "tf_player_resource.h"
#include "tf_gamestats.h"
#include "tf_gamerules.h"
#include <coordsize.h>

// Datatable
IMPLEMENT_SERVERCLASS_ST( CTFPlayerResource, DT_TFPlayerResource )
	SendPropArray3( SENDINFO_ARRAY3( m_iTotalScore ), SendPropInt( SENDINFO_ARRAY( m_iTotalScore ), 12, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iMaxHealth ), SendPropInt( SENDINFO_ARRAY( m_iMaxHealth ), 10, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iMaxBuffedHealth ), SendPropInt( SENDINFO_ARRAY( m_iMaxBuffedHealth ), -1, SPROP_UNSIGNED | SPROP_VARINT ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iPlayerClass ), SendPropInt( SENDINFO_ARRAY( m_iPlayerClass ), 5, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iActiveDominations ), SendPropInt( SENDINFO_ARRAY( m_iActiveDominations ), 6, SPROP_UNSIGNED ) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( tf_player_manager, CTFPlayerResource );

CTFPlayerResource::CTFPlayerResource( void )
{
	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerResource::UpdatePlayerData( void )
{
	BaseClass::UpdatePlayerData();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerResource::UpdateConnectedPlayer( int iIndex, CBasePlayer *pPlayer )
{
	BaseClass::UpdateConnectedPlayer( iIndex, pPlayer );

	CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );

	m_iMaxHealth.Set( iIndex, pTFPlayer->GetMaxHealth() );

	// m_iMaxBuffedHealth is misnamed -- it should be m_iMaxHealthForBuffing, but we don't want to change it now due to demos.
	m_iMaxBuffedHealth.Set( iIndex, pTFPlayer->GetMaxHealthForBuffing() );
	m_iPlayerClass.Set( iIndex, pTFPlayer->GetPlayerClass()->GetClassIndex() );

	m_iActiveDominations.Set( iIndex, pTFPlayer->GetNumberofDominations() );

	PlayerStats_t *pTFPlayerStats = CTF_GameStats.FindPlayerStats( pTFPlayer );
	if ( pTFPlayerStats )
	{
		int iTotalScore = CTFGameRules::CalcPlayerScore( &pTFPlayerStats->statsAccumulated );
		m_iTotalScore.Set( iIndex, iTotalScore );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerResource::UpdateDisconnectedPlayer( int iIndex )
{
	// cache accountID to see if we should preserve this account
//	uint32 unAccountID = m_iAccountID[iIndex];

	BaseClass::UpdateDisconnectedPlayer( iIndex );
}

void CTFPlayerResource::Spawn( void )
{
	BaseClass::Spawn();
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerResource::Init( int iIndex )
{
	BaseClass::Init( iIndex );

	m_iTotalScore.Set( iIndex, 0 );
	m_iMaxHealth.Set( iIndex, TF_HEALTH_UNDEFINED );
	m_iMaxBuffedHealth.Set( iIndex, TF_HEALTH_UNDEFINED );
	m_iPlayerClass.Set( iIndex, TF_CLASS_UNDEFINED );
	m_iActiveDominations.Set( iIndex, 0 );
	//m_iPlayerClassWhenKilled.Set( iIndex, TF_CLASS_UNDEFINED );
	//m_iConnectionState.Set( iIndex, MM_DISCONNECTED );
	m_bValid.Set( iIndex, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Gets a value from an array member
//-----------------------------------------------------------------------------
int CTFPlayerResource::GetTotalScore( int iIndex )
{
	Assert( iIndex >= 0 && iIndex <= MAX_PLAYERS );

	CTFPlayer *pPlayer = (CTFPlayer*)UTIL_PlayerByIndex( iIndex );

	if ( pPlayer && pPlayer->IsConnected() )
	{	
		return m_iTotalScore[iIndex];
	}

	return 0;
}