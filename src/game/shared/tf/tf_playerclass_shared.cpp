//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
//=============================================================================

#include "cbase.h"
#include "KeyValues.h"
#include "tf_playerclass_shared.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier2/tier2.h"

#include "weapon_dodbase.h"

#ifdef CLIENT_DLL
bool UseHWMorphModels();
#endif


#define TF_CLASS_UNDEFINED_FILE			""
#define TF_CLASS_SCOUT_FILE				"scripts/playerclasses/scout"
#define TF_CLASS_SNIPER_FILE			"scripts/playerclasses/sniper"
#define TF_CLASS_SOLDIER_FILE			"scripts/playerclasses/soldier"
#define TF_CLASS_DEMOMAN_FILE			"scripts/playerclasses/demoman"
#define TF_CLASS_MEDIC_FILE				"scripts/playerclasses/medic"
#define TF_CLASS_HEAVYWEAPONS_FILE		"scripts/playerclasses/heavyweapons"
#define TF_CLASS_PYRO_FILE				"scripts/playerclasses/pyro"
#define TF_CLASS_SPY_FILE				"scripts/playerclasses/spy"
#define TF_CLASS_ENGINEER_FILE			"scripts/playerclasses/engineer"
#define TF_CLASS_RANDOM_FILE			""

#define DOD_CLASS_TOMMY_FILE			"scripts/playerclasses/tommy"
#define DOD_CLASS_BAR_FILE				"scripts/playerclasses/bar"
#define DOD_CLASS_GARAND_FILE			"scripts/playerclasses/garand"
#define DOD_CLASS_SPRING_FILE			"scripts/playerclasses/spring"
#define DOD_CLASS_BAZOOKA_FILE			"scripts/playerclasses/bazooka"
#define DOD_CLASS_30CAL_FILE			"scripts/playerclasses/30cal"

#define DOD_CLASS_MP40_FILE				"scripts/playerclasses/mp40"
#define DOD_CLASS_MP44_FILE				"scripts/playerclasses/mp44"
#define DOD_CLASS_K98_FILE				"scripts/playerclasses/k98"
#define DOD_CLASS_K98S_FILE				"scripts/playerclasses/k98s"
#define DOD_CLASS_PSCHRECK_FILE			"scripts/playerclasses/pschreck"
#define DOD_CLASS_MG42_FILE				"scripts/playerclasses/mg42"

#define PC_CLASS_AIGIS_FILE				"scripts/playerclasses/aigis"

#define CS_CLASS_CT						"scripts/playerclasses/ct"
#define CS_CLASS_T						"scripts/playerclasses/t"

#define HL2_CLASS_REBEL					"scripts/playerclasses/rebel"
#define HL2_CLASS_COMBINE				"scripts/playerclasses/combine"
#define HL2_CLASS_DEATHMATCH			"scripts/playerclasses/hl2dm"

#define HL1_CLASS_DEATHMATCH			"scripts/playerclasses/hl1dm"

const char* s_aPlayerClassFiles[] =
{
	TF_CLASS_UNDEFINED_FILE,
	TF_CLASS_SCOUT_FILE,
	TF_CLASS_SNIPER_FILE,
	TF_CLASS_SOLDIER_FILE,
	TF_CLASS_DEMOMAN_FILE,
	TF_CLASS_MEDIC_FILE,
	TF_CLASS_HEAVYWEAPONS_FILE,
	TF_CLASS_PYRO_FILE,
	TF_CLASS_SPY_FILE,
	TF_CLASS_ENGINEER_FILE,
	TF_CLASS_RANDOM_FILE,
	DOD_CLASS_TOMMY_FILE,
	DOD_CLASS_BAR_FILE,
	DOD_CLASS_GARAND_FILE,
	DOD_CLASS_SPRING_FILE,
	DOD_CLASS_BAZOOKA_FILE,
	DOD_CLASS_30CAL_FILE,
	DOD_CLASS_MP40_FILE,
	DOD_CLASS_MP44_FILE,
	DOD_CLASS_K98_FILE,
	DOD_CLASS_K98S_FILE,
	DOD_CLASS_PSCHRECK_FILE,
	DOD_CLASS_MG42_FILE,
	PC_CLASS_AIGIS_FILE,
	CS_CLASS_CT,
	CS_CLASS_T,
	HL2_CLASS_REBEL,
	HL2_CLASS_COMBINE,
	HL2_CLASS_DEATHMATCH,
	HL1_CLASS_DEATHMATCH,
};

TFPlayerClassData_t s_aTFPlayerClassData[GAME_CLASS_COUNT];

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TFPlayerClassData_t::TFPlayerClassData_t()
{
	m_szClassName[0] = '\0';
	m_szModelName[0] = '\0';
	m_szHWMModelName[0] = '\0';
	m_szLocalizableName[0] = '\0';
	m_flMaxSpeed = 0.0f;
	m_nMaxHealth = 0;
	m_nMaxArmor = 0;

#ifdef GAME_DLL
	m_szDeathSound[0] = '\0';
	m_szCritDeathSound[0] = '\0';
	m_szMeleeDeathSound[0] = '\0';
	m_szExplosionDeathSound[0] = '\0';
#endif

	for ( int iWeapon = 0; iWeapon < TF_PLAYER_WEAPON_COUNT; ++iWeapon )
	{
		m_aWeapons[iWeapon] = TF_WEAPON_NONE;
	}

	for ( int iGrenade = 0; iGrenade < TF_PLAYER_GRENADE_COUNT; ++iGrenade )
	{
		m_aGrenades[iGrenade] = TF_WEAPON_NONE;
	}

	for ( int iAmmo = 0; iAmmo < TF_AMMO_COUNT; ++iAmmo )
	{
		m_aAmmoMax[iAmmo] = TF_AMMO_DUMMY;
	}

	for ( int iBuildable = 0; iBuildable < TF_PLAYER_BLUEPRINT_COUNT; ++iBuildable )
	{
		m_aBuildable[iBuildable] = OBJ_LAST;
	}
	
	m_iNumGrensType1 = 0;
	m_iGrenType1 = TF_WEAPON_NONE;

	m_iNumGrensType2 = 0;
	m_iGrenType2 = TF_WEAPON_NONE;

	m_iHelmetGroup = HELMET_GROUP_0;
	m_iHairGroup = HELMET_GROUP_0;

	m_iDropHelmet = HELMET_ALLIES;

	m_iClassType = FACTION_TF;

	m_bParsed = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TFPlayerClassData_t::Parse( const char *szName )
{
	// Have we parsed this file already?
	if ( m_bParsed )
		return;

	// No filesystem at this point????  Hmmmm......

	// Parse class file.

	const unsigned char *pKey = NULL;

	if ( g_pGameRules )
	{
		pKey = g_pGameRules->GetEncryptionKey();
	}

	KeyValues *pKV = ReadEncryptedKVFile( filesystem, szName, pKey );
	if ( pKV )
	{
		ParseData( pKV );
		pKV->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *TFPlayerClassData_t::GetModelName() const
{
#ifdef CLIENT_DLL
	if ( UseHWMorphModels() )
	{
		if ( m_szHWMModelName[0] != '\0' )
		{
			return m_szHWMModelName;
		}
	}

	return m_szModelName;
#else
	return m_szModelName;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TFPlayerClassData_t::ParseData( KeyValues *pKeyValuesData )
{
	m_iClassType = pKeyValuesData->GetInt( "ClassType", 0 );

	switch( m_iClassType )
	{
		case FACTION_DOD:
			return ParseDataDOD( pKeyValuesData );
		case FACTION_CS:
			return ParseDataCS( pKeyValuesData );
		case FACTION_PC:
		default:
			return ParseDataTF( pKeyValuesData );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TFPlayerClassData_t::ParseDataTF( KeyValues *pKeyValuesData )
{
	// Attributes.
	Q_strncpy( m_szClassName, pKeyValuesData->GetString( "name" ), TF_NAME_LENGTH );

	// Load the high res model or the lower res model.
	Q_strncpy( m_szHWMModelName, pKeyValuesData->GetString( "model_hwm" ), TF_NAME_LENGTH );
	Q_strncpy( m_szModelName, pKeyValuesData->GetString( "model" ), TF_NAME_LENGTH );
	Q_strncpy( m_szLocalizableName, pKeyValuesData->GetString( "localize_name" ), TF_NAME_LENGTH );

	m_flMaxSpeed = pKeyValuesData->GetFloat( "speed_max" );
	m_nMaxHealth = pKeyValuesData->GetInt( "health_max" );
	m_nMaxArmor = pKeyValuesData->GetInt( "armor_max" );

	// Weapons.
	int i;
	char buf[32];
	for ( i=0;i<TF_PLAYER_WEAPON_COUNT;i++ )
	{
		Q_snprintf( buf, sizeof(buf), "weapon%d", i+1 );
		m_aWeapons[i] = GetWeaponId( pKeyValuesData->GetString( buf ) );
	}

	// Grenades.
	m_aGrenades[0] = GetWeaponId( pKeyValuesData->GetString( "grenade1" ) );
	m_aGrenades[1] = GetWeaponId( pKeyValuesData->GetString( "grenade2" ) );

	// Ammo Max.
	KeyValues *pAmmoKeyValuesData = pKeyValuesData->FindKey( "AmmoMax" );
	if ( pAmmoKeyValuesData )
	{
		for ( int iAmmo = 1; iAmmo < TF_AMMO_COUNT; ++iAmmo )
		{
			m_aAmmoMax[iAmmo] = pAmmoKeyValuesData->GetInt( g_aAmmoNames[iAmmo], 0 );
		}
	}

	// Buildables
	for ( i=0;i<TF_PLAYER_BLUEPRINT_COUNT;i++ )
	{
		Q_snprintf( buf, sizeof(buf), "buildable%d", i+1 );		
		m_aBuildable[i] = GetBuildableId( pKeyValuesData->GetString( buf ) );		
	}

	// Temp animation flags
	m_bDontDoAirwalk = ( pKeyValuesData->GetInt( "DontDoAirwalk", 0 ) > 0 );
	m_bDontDoNewJump = ( pKeyValuesData->GetInt( "DontDoNewJump", 0 ) > 0 );

#ifdef GAME_DLL		// right now we only emit these sounds from server. if that changes we can do this in both dlls

	// Death Sounds
	Q_strncpy( m_szDeathSound, pKeyValuesData->GetString( "sound_death", "Player.Death" ), MAX_PLAYERCLASS_SOUND_LENGTH );
	Q_strncpy( m_szCritDeathSound, pKeyValuesData->GetString( "sound_crit_death", "TFPlayer.CritDeath" ), MAX_PLAYERCLASS_SOUND_LENGTH );
	Q_strncpy( m_szMeleeDeathSound, pKeyValuesData->GetString( "sound_melee_death", "Player.MeleeDeath" ), MAX_PLAYERCLASS_SOUND_LENGTH );
	Q_strncpy( m_szExplosionDeathSound, pKeyValuesData->GetString( "sound_explosion_death", "Player.ExplosionDeath" ), MAX_PLAYERCLASS_SOUND_LENGTH );

#endif

	// The file has been parsed.
	m_bParsed = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TFPlayerClassData_t::ParseDataDOD( KeyValues *pKeyValuesData )
{
	// Attributes.
	Q_strncpy( m_szClassName, pKeyValuesData->GetString( "name" ), TF_NAME_LENGTH );
	Q_strncpy( m_szModelName, pKeyValuesData->GetString( "model" ), TF_NAME_LENGTH );
	Q_strncpy( m_szLocalizableName, pKeyValuesData->GetString( "localize_name" ), TF_NAME_LENGTH );

	m_nMaxHealth = pKeyValuesData->GetInt( "health_max" );

	// Only account for weapons, we handle melees differently
	int i;
	char buf[32];
	for ( i=0;i<DOD_PLAYER_WEAPON_COUNT;i++ )
	{
		Q_snprintf( buf, sizeof(buf), "weapon%d", i+1 );
		m_aWeapons[i] = AliasToWeaponId( pKeyValuesData->GetString( buf ) );
	}

	m_iNumGrensType1 = pKeyValuesData->GetInt( "numgrens", 0 );
	if ( m_iNumGrensType1 > 0 )
	{
		const char *pszGrenType1 = pKeyValuesData->GetString( "grenadetype", NULL );
		m_iGrenType1 = AliasToWeaponId( pszGrenType1 );
		Assert( m_iGrenType1 != TF_WEAPON_NONE );
	}

	m_iNumGrensType2 = pKeyValuesData->GetInt( "numgrens2", 0 );
	if ( m_iNumGrensType2 > 0 )
	{
		const char *pszGrenType2 = pKeyValuesData->GetString( "grenadetype2", NULL );
		m_iGrenType2 = AliasToWeaponId( pszGrenType2 );
		Assert( m_iGrenType2 != TF_WEAPON_NONE );
	}

	m_iHelmetGroup	= pKeyValuesData->GetInt( "helmetgroup", 0 );
	m_iHairGroup	= pKeyValuesData->GetInt( "hairgroup", 0 );

	// Which helmet model to generate 
	const char *pszHelmetModel = pKeyValuesData->GetString( "drophelmet", "HELMET_ALLIES" );

	if( pszHelmetModel )
	{
		if ( Q_stricmp( pszHelmetModel, "HELMET_ALLIES" ) == 0 )
		{
			m_iDropHelmet = HELMET_ALLIES;
		}
		else if ( Q_stricmp( pszHelmetModel, "HELMET_AXIS" ) == 0 )
		{
			m_iDropHelmet = HELMET_AXIS;
		}
		else
		{
			Assert( false );
		}
	}
	else
	{
		Assert( false );
	}

#ifdef GAME_DLL		// right now we only emit these sounds from server. if that changes we can do this in both dlls

	// Death Sounds
	Q_strncpy( m_szDeathSound, pKeyValuesData->GetString( "sound_death", "Player.Death" ), MAX_PLAYERCLASS_SOUND_LENGTH );
	Q_strncpy( m_szCritDeathSound, pKeyValuesData->GetString( "sound_crit_death", "TFPlayer.CritDeath" ), MAX_PLAYERCLASS_SOUND_LENGTH );
	Q_strncpy( m_szMeleeDeathSound, pKeyValuesData->GetString( "sound_melee_death", "Player.MeleeDeath" ), MAX_PLAYERCLASS_SOUND_LENGTH );
	Q_strncpy( m_szExplosionDeathSound, pKeyValuesData->GetString( "sound_explosion_death", "Player.ExplosionDeath" ), MAX_PLAYERCLASS_SOUND_LENGTH );

#endif

	// HUD player status health images (when the player is hurt)
	Q_strncpy( m_szClassHealthImage, pKeyValuesData->GetString( "healthimage", "white" ), sizeof( m_szClassHealthImage ) );
	Q_strncpy( m_szClassHealthImageBG, pKeyValuesData->GetString( "healthimagebg", "white" ), sizeof( m_szClassHealthImageBG ) );

	// The file has been parsed.
	m_bParsed = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TFPlayerClassData_t::ParseDataCS( KeyValues *pKeyValuesData )
{
	// Attributes.
	Q_strncpy( m_szClassName, pKeyValuesData->GetString( "name" ), TF_NAME_LENGTH );
	Q_strncpy( m_szModelName, pKeyValuesData->GetString( "model" ), TF_NAME_LENGTH );
	Q_strncpy( m_szLocalizableName, pKeyValuesData->GetString( "localize_name" ), TF_NAME_LENGTH );

	m_nMaxHealth = pKeyValuesData->GetInt( "health_max" );

	// Only account for weapons, we handle melees differently
	int i;
	char buf[32];
	for ( i=0;i<DOD_PLAYER_WEAPON_COUNT;i++ )
	{
		Q_snprintf( buf, sizeof(buf), "weapon%d", i+1 );
		m_aWeapons[i] = AliasToWeaponId( pKeyValuesData->GetString( buf ) );
	}

	// The file has been parsed.
	m_bParsed = true;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the player class data (keep a cache).
//-----------------------------------------------------------------------------
void InitPlayerClasses( void )
{
	// Special case the undefined class.
	TFPlayerClassData_t *pClassData = &s_aTFPlayerClassData[TF_CLASS_UNDEFINED];
	Assert( pClassData );
	Q_strncpy( pClassData->m_szClassName, "undefined", TF_NAME_LENGTH );
	Q_strncpy( pClassData->m_szModelName, "models/player/scout.mdl", TF_NAME_LENGTH );	// Undefined players still need a model
	Q_strncpy( pClassData->m_szLocalizableName, "undefined", TF_NAME_LENGTH );

	// Initialize the classes.
	for ( int iClass = 1; iClass < GAME_CLASS_COUNT; ++iClass )
	{
		TFPlayerClassData_t *pClassData = &s_aTFPlayerClassData[iClass];
		Assert( pClassData );
		pClassData->Parse( s_aPlayerClassFiles[iClass] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Helper function to get player class data.
//-----------------------------------------------------------------------------
TFPlayerClassData_t *GetPlayerClassData( int iClass )
{
	Assert ( ( iClass >= 0 ) && ( iClass < GAME_CLASS_COUNT ) );
	return &s_aTFPlayerClassData[iClass];
}

//=============================================================================
//
// Shared player class data.
//

//=============================================================================
//
// Tables.
//

// Client specific.
#ifdef CLIENT_DLL

BEGIN_RECV_TABLE_NOBASE( CTFPlayerClassShared, DT_TFPlayerClassShared )
	RecvPropInt( RECVINFO( m_iClass ) ),
END_RECV_TABLE()

// Server specific.
#else

BEGIN_SEND_TABLE_NOBASE( CTFPlayerClassShared, DT_TFPlayerClassShared )
	SendPropInt( SENDINFO( m_iClass ), Q_log2( GAME_CLASS_COUNT  )+1, SPROP_UNSIGNED ),
END_SEND_TABLE()

#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFPlayerClassShared::CTFPlayerClassShared()
{
	m_iClass.Set( TF_CLASS_UNDEFINED );
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the player class.
//-----------------------------------------------------------------------------
bool CTFPlayerClassShared::Init( int iClass )
{
	Assert ( ( iClass >= GAME_FIRST_CLASS ) && ( iClass <= GAME_LAST_CLASS ) );
	m_iClass = iClass;
	return true;
}

// If needed, put this into playerclass scripts
bool CTFPlayerClassShared::CanBuildObject( int iObjectType )
{
	bool bFound = false;

	TFPlayerClassData_t  *pData = GetData();

	int i;
	for ( i=0;i<TF_PLAYER_BLUEPRINT_COUNT;i++ )
	{
		if ( iObjectType == pData->m_aBuildable[i] )
		{
			bFound = true;
			break;
		}
	}

	return bFound;
}
