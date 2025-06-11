//========= Copyright © 1996-2005, Valve LLC, All rights reserved. ============
//
// Purpose:
//
//=============================================================================
#ifndef TF_PLAYERCLASS_SHARED_H
#define TF_PLAYERCLASS_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_shareddefs.h"
#include "dod_shareddefs.h"

#define TF_NAME_LENGTH		128

// Client specific.
#ifdef CLIENT_DLL

EXTERN_RECV_TABLE( DT_TFPlayerClassShared );

// Server specific.
#else

EXTERN_SEND_TABLE( DT_TFPlayerClassShared );

#endif


//-----------------------------------------------------------------------------
// Cache structure for the TF player class data (includes citizen). 
//-----------------------------------------------------------------------------

#define MAX_PLAYERCLASS_SOUND_LENGTH	128

struct TFPlayerClassData_t
{
	char		m_szClassName[TF_NAME_LENGTH];
	char		m_szModelName[TF_NAME_LENGTH];
	char		m_szHWMModelName[TF_NAME_LENGTH];
	char		m_szLocalizableName[TF_NAME_LENGTH];
	float		m_flMaxSpeed;
	int			m_nMaxHealth;
	int			m_nMaxArmor;
	int			m_aWeapons[TF_PLAYER_WEAPON_COUNT];
	int			m_aGrenades[TF_PLAYER_GRENADE_COUNT];
	int			m_aAmmoMax[TF_AMMO_COUNT];
	int			m_aBuildable[TF_PLAYER_BLUEPRINT_COUNT];

	bool		m_bDontDoAirwalk;
	bool		m_bDontDoNewJump;

	int			m_iClassType;

	bool		m_bParsed;

#ifdef GAME_DLL
	// sounds
	char		m_szDeathSound[MAX_PLAYERCLASS_SOUND_LENGTH];
	char		m_szCritDeathSound[MAX_PLAYERCLASS_SOUND_LENGTH];
	char		m_szMeleeDeathSound[MAX_PLAYERCLASS_SOUND_LENGTH];
	char		m_szExplosionDeathSound[MAX_PLAYERCLASS_SOUND_LENGTH];
#endif

	TFPlayerClassData_t();
	const char *GetModelName() const;
	void Parse( const char *pszClassName );

	int			m_iNumGrensType1;
	int			m_iGrenType1;
	int			m_iNumGrensType2;
	int			m_iGrenType2;

	int			m_iHelmetGroup;
	int			m_iHairGroup;	//what helmet group to switch to when the helmet comes off

	int			m_iDropHelmet;

	char m_szClassHealthImage[DOD_HUD_HEALTH_IMAGE_LENGTH];
	char m_szClassHealthImageBG[DOD_HUD_HEALTH_IMAGE_LENGTH];
private:

	// Parser for the class data.
	void ParseData( KeyValues *pKeyValuesData );

	// TF Parser
	void ParseDataTF( KeyValues *pKeyValuesData );

	// DOD Parser
	void ParseDataDOD( KeyValues *pKeyValuesData );

	// CS Parser
	void ParseDataCS( KeyValues *pKeyValuesData );
};

void InitPlayerClasses( void );
TFPlayerClassData_t *GetPlayerClassData( int iClass );

//-----------------------------------------------------------------------------
// TF Player Class Shared
//-----------------------------------------------------------------------------
class CTFPlayerClassShared
{
public:

	CTFPlayerClassShared();

	DECLARE_EMBEDDED_NETWORKVAR()
	DECLARE_CLASS_NOBASE( CTFPlayerClassShared );

	bool		Init( int iClass );
	bool		IsClass( int iClass ) const						{ return ( m_iClass == iClass ); }
	int			GetClassIndex( void ) const						{ return m_iClass; }

	const char	*GetName( void ) const							{ return GetPlayerClassData( m_iClass )->m_szClassName; }
	const char	*GetModelName( void ) const						{ return GetPlayerClassData( m_iClass )->GetModelName(); }		
	float		GetMaxSpeed( void )								{ return GetPlayerClassData( m_iClass )->m_flMaxSpeed; }
	int			GetMaxHealth( void ) const						{ return GetPlayerClassData( m_iClass )->m_nMaxHealth; }
	int			GetMaxArmor( void )	const						{ return GetPlayerClassData( m_iClass )->m_nMaxArmor; }

	TFPlayerClassData_t  *GetData( void )						{ return GetPlayerClassData( m_iClass ); }

	// If needed, put this into playerclass scripts
	bool CanBuildObject( int iObjectType );

protected:

	CNetworkVar( int,	m_iClass );
};

#endif // TF_PLAYERCLASS_SHARED_H