//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: TF's custom CPlayerResource
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_PLAYER_RESOURCE_H
#define TF_PLAYER_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

class CTFPlayerResource : public CPlayerResource
{
	DECLARE_CLASS( CTFPlayerResource, CPlayerResource );
	
public:
	DECLARE_SERVERCLASS();

	CTFPlayerResource();

	virtual void UpdatePlayerData( void );
	virtual void Spawn( void );
	virtual void Init( int iIndex ) OVERRIDE;

	int	GetTotalScore( int iIndex );

protected:
	virtual void UpdateConnectedPlayer( int iIndex, CBasePlayer *pPlayer ) OVERRIDE;
	virtual void UpdateDisconnectedPlayer( int iIndex ) OVERRIDE;

	CNetworkArray( int,	m_iTotalScore, MAX_PLAYERS+1 );
	CNetworkArray( int, m_iMaxHealth, MAX_PLAYERS+1 );
	CNetworkArray( int, m_iMaxBuffedHealth, MAX_PLAYERS+1 );
	CNetworkArray( int, m_iPlayerClass, MAX_PLAYERS+1 );
	CNetworkArray( int,	m_iActiveDominations, MAX_PLAYERS+1 );
};

#endif // TF_PLAYER_RESOURCE_H
