//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "cbase.h"
#include <KeyValues.h>
#include "tf_weapon_parse.h"
#include "tf_shareddefs.h"
#include "tf_playerclass_shared.h"

#include "dod_shareddefs.h"

#include "tf_gamerules.h"

//#include "cs_blackmarket.h"

enum
{
	KEVLAR_PRICE = 650,
	HELMET_PRICE = 350,
	ASSAULTSUIT_PRICE = 1000,
	DEFUSEKIT_PRICE = 200,
	NVG_PRICE = 1250,
	SHIELD_PRICE = 2200
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
FileWeaponInfo_t *CreateWeaponInfo()
{
	return new CTFWeaponInfo;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFWeaponInfo::CTFWeaponInfo()
{
	ConstructTF();
	ConstructDOD();
	ConstructCS();
	ConstructHL2();
}

CTFWeaponInfo::~CTFWeaponInfo()
{
}

CTFWeaponInfo *GetTFWeaponInfo( int iWeapon )
{
	// Get the weapon information.
	const char *pszWeaponAlias = WeaponIdToAlias( iWeapon );
	if ( !pszWeaponAlias )
	{
		return NULL;
	}

	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( pszWeaponAlias );
	if ( hWpnInfo == GetInvalidWeaponInfoHandle() )
	{
		return NULL;
	}

	CTFWeaponInfo *pWeaponInfo = static_cast<CTFWeaponInfo*>( GetFileWeaponInfoFromHandle( hWpnInfo ) );
	return pWeaponInfo;
}

void CTFWeaponInfo::ConstructTF()
{
	m_TFWeaponData[0].Init(); // Primary mode
	m_TFWeaponData[1].Init(); // Secondary mode
	m_ExtraTFWeaponData.Init();
}

void CTFWeaponInfo::ConstructDOD()
{
	m_DODWeaponData.Init();
}

void CTFWeaponInfo::ConstructCS()
{
	m_CSWeaponData.Init();
}

void CTFWeaponInfo::ConstructHL2()
{
	m_HL2WeaponData.Init();
}

void CTFWeaponInfo::GetWeaponFaction( const char *szWeaponName )
{
	for( int i = 0; i < FACTION_COUNT; ++i )
	{
		if ( strncmp( szWeaponName, s_aFactionPath[i], strlen( s_aFactionPath[i] ) ) == 0 )
		{
			// Set our parse type to reflect our assigned faction
			SetWeaponParseTypeReference( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFWeaponInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

	// Set default parse type
	SetWeaponParseTypeReference( FACTION_TF );

	// Figure out our faction the weapon belongs to
	GetWeaponFaction( szWeaponName );

	switch ( GetWeaponParseTypeReference() )
	{
		case FACTION_DOD:
		{
			DODParse( pKeyValuesData, szWeaponName );
			break;
		}
		case FACTION_CS:
		{
			CSParse( pKeyValuesData, szWeaponName );
			break;
		}
		// SEALTODO: HL1 WEAPON PARSING
		case FACTION_HL1:
		case FACTION_HL2:
		{
			HL2Parse( pKeyValuesData, szWeaponName );
			break;
		}
		default:
		{
			TFParse( pKeyValuesData, szWeaponName );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFWeaponInfo::TFParse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	// Primary fire mode.
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_nDamage				= pKeyValuesData->GetInt( "Damage", 0 );
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flRange				= pKeyValuesData->GetFloat( "Range", 8192.0f );
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_nBulletsPerShot		= pKeyValuesData->GetInt( "BulletsPerShot", 0 );
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flSpread				= pKeyValuesData->GetFloat( "Spread", 0.0f );
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flPunchAngle			= pKeyValuesData->GetFloat( "PunchAngle", 0.0f );
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeFireDelay		= pKeyValuesData->GetFloat( "TimeFireDelay", 0.0f );
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeIdle			= pKeyValuesData->GetFloat( "TimeIdle", 0.0f );
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeIdleEmpty		= pKeyValuesData->GetFloat( "TimeIdleEmpy", 0.0f );
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeReloadStart	= pKeyValuesData->GetFloat( "TimeReloadStart", 0.0f );
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeReload			= pKeyValuesData->GetFloat( "TimeReload", 0.0f );
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_bDrawCrosshair		= pKeyValuesData->GetInt( "DrawCrosshair", 1 ) > 0;
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_iAmmoPerShot			= pKeyValuesData->GetInt( "AmmoPerShot", 1 );
	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_bUseRapidFireCrits	= ( pKeyValuesData->GetInt( "UseRapidFireCrits", 0 ) != 0 );

	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_iProjectile = TF_PROJECTILE_NONE;
	const char *pszProjectileType = pKeyValuesData->GetString( "ProjectileType", "projectile_none" );

	int i;
	for ( i=0;i<TF_NUM_PROJECTILES;i++ )
	{
		if ( FStrEq( pszProjectileType, g_szProjectileNames[i] ) )
		{
			m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_iProjectile = i;
			break;
		}
	}	 

	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flProjectileSpeed	= pKeyValuesData->GetFloat( "ProjectileSpeed", 0.0f );

	m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flSmackDelay			= pKeyValuesData->GetFloat( "SmackDelay", 0.2f );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_flSmackDelay		= pKeyValuesData->GetFloat( "Secondary_SmackDelay", 0.2f );

	m_ExtraTFWeaponData.m_bDoInstantEjectBrass = ( pKeyValuesData->GetInt( "DoInstantEjectBrass", 1 ) != 0 );
	const char *pszBrassModel = pKeyValuesData->GetString( "BrassModel", NULL );
	if ( pszBrassModel )
	{
		Q_strncpy( m_ExtraTFWeaponData.m_szBrassModel, pszBrassModel, sizeof( m_ExtraTFWeaponData.m_szBrassModel ) );
	}

	// Secondary fire mode.
	// Inherit from primary fire mode
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_nDamage			= pKeyValuesData->GetInt( "Secondary_Damage", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_nDamage );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_flRange			= pKeyValuesData->GetFloat( "Secondary_Range", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flRange );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_nBulletsPerShot	= pKeyValuesData->GetInt( "Secondary_BulletsPerShot", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_nBulletsPerShot );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_flSpread			= pKeyValuesData->GetFloat( "Secondary_Spread", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flSpread );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_flPunchAngle		= pKeyValuesData->GetFloat( "Secondary_PunchAngle", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flPunchAngle );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_flTimeFireDelay	= pKeyValuesData->GetFloat( "Secondary_TimeFireDelay", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeFireDelay );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_flTimeIdle			= pKeyValuesData->GetFloat( "Secondary_TimeIdle", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeIdle );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_flTimeIdleEmpty	= pKeyValuesData->GetFloat( "Secondary_TimeIdleEmpy", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeIdleEmpty );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_flTimeReloadStart	= pKeyValuesData->GetFloat( "Secondary_TimeReloadStart", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeReloadStart );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_flTimeReload		= pKeyValuesData->GetFloat( "Secondary_TimeReload", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_flTimeReload );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_bDrawCrosshair		= pKeyValuesData->GetInt( "Secondary_DrawCrosshair", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_bDrawCrosshair ) > 0;
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_iAmmoPerShot		= pKeyValuesData->GetInt( "Secondary_AmmoPerShot", m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_iAmmoPerShot );
	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_bUseRapidFireCrits	= ( pKeyValuesData->GetInt( "Secondary_UseRapidFireCrits", 0 ) != 0 );

	m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_iProjectile = m_TFWeaponData[TF_WEAPON_PRIMARY_MODE].m_iProjectile;
	pszProjectileType = pKeyValuesData->GetString( "Secondary_ProjectileType", "projectile_none" );

	for ( i=0;i<TF_NUM_PROJECTILES;i++ )
	{
		if ( FStrEq( pszProjectileType, g_szProjectileNames[i] ) )
		{
			m_TFWeaponData[TF_WEAPON_SECONDARY_MODE].m_iProjectile = i;
			break;
		}
	}	

	const char *pszWeaponType = pKeyValuesData->GetString( "WeaponType" );

	if ( !Q_strcmp( pszWeaponType, "primary" ) )
	{
		m_ExtraTFWeaponData.m_iWeaponType = TF_WPN_TYPE_PRIMARY;
	}
	else if ( !Q_strcmp( pszWeaponType, "secondary" ) )
	{
		m_ExtraTFWeaponData.m_iWeaponType = TF_WPN_TYPE_SECONDARY;
	}
	else if ( !Q_strcmp( pszWeaponType, "melee" ) )
	{
		m_ExtraTFWeaponData.m_iWeaponType = TF_WPN_TYPE_MELEE;
	}
	else if ( !Q_strcmp( pszWeaponType, "grenade" ) )
	{
		m_ExtraTFWeaponData.m_iWeaponType = TF_WPN_TYPE_GRENADE;
	}
	else if ( !Q_strcmp( pszWeaponType, "building" ) )
	{
		m_ExtraTFWeaponData.m_iWeaponType = TF_WPN_TYPE_BUILDING;
	}
	else if ( !Q_strcmp( pszWeaponType, "pda" ) )
	{
		m_ExtraTFWeaponData.m_iWeaponType = TF_WPN_TYPE_PDA;
	}

	// Grenade data.
	m_ExtraTFWeaponData.m_bGrenade				= ( pKeyValuesData->GetInt( "Grenade", 0 ) != 0 );
	m_ExtraTFWeaponData.m_flDamageRadius		= pKeyValuesData->GetFloat( "DamageRadius", 0.0f );
	m_ExtraTFWeaponData.m_flPrimerTime			= pKeyValuesData->GetFloat( "PrimerTime", 0.0f );
	m_ExtraTFWeaponData.m_bSuppressGrenTimer	= ( pKeyValuesData->GetInt( "PlayGrenTimer", 1 ) <= 0 );

	m_ExtraTFWeaponData.m_bLowerWeapon			= ( pKeyValuesData->GetInt( "LowerMainWeapon", 0 ) != 0 );
	m_ExtraTFWeaponData.m_bHasTeamSkins_Viewmodel	= ( pKeyValuesData->GetInt( "HasTeamSkins_Viewmodel", 0 ) != 0 );
	m_ExtraTFWeaponData.m_bHasTeamSkins_Worldmodel	= ( pKeyValuesData->GetInt( "HasTeamSkins_Worldmodel", 0 ) != 0 );

	// Model muzzleflash
	const char *pszMuzzleFlashModel = pKeyValuesData->GetString( "MuzzleFlashModel", NULL );

	if ( pszMuzzleFlashModel )
	{
		Q_strncpy( m_ExtraTFWeaponData.m_szMuzzleFlashModel, pszMuzzleFlashModel, sizeof( m_ExtraTFWeaponData.m_szMuzzleFlashModel ) );
	}

	m_ExtraTFWeaponData.m_flMuzzleFlashModelDuration = pKeyValuesData->GetFloat( "MuzzleFlashModelDuration", 0.2 );

	const char *pszMuzzleFlashParticleEffect = pKeyValuesData->GetString( "MuzzleFlashParticleEffect", NULL );

	if ( pszMuzzleFlashParticleEffect )
	{
		Q_strncpy( m_ExtraTFWeaponData.m_szMuzzleFlashParticleEffect, pszMuzzleFlashParticleEffect, sizeof( m_ExtraTFWeaponData.m_szMuzzleFlashParticleEffect ) );
	}

	// Tracer particle effect
	const char *pszTracerEffect = pKeyValuesData->GetString( "TracerEffect", NULL );

	if ( pszTracerEffect )
	{
		Q_strncpy( m_ExtraTFWeaponData.m_szTracerEffect, pszTracerEffect, sizeof( m_ExtraTFWeaponData.m_szTracerEffect ) );
	}


	// Explosion effects (used for grenades)
	const char *pszSound = pKeyValuesData->GetString( "ExplosionSound", NULL );
	if ( pszSound )
	{
		Q_strncpy( m_ExtraTFWeaponData.m_szExplosionSound, pszSound, sizeof( m_ExtraTFWeaponData.m_szExplosionSound ) );
	}

	const char *pszEffect = pKeyValuesData->GetString( "ExplosionEffect", NULL );
	if ( pszEffect )
	{
		Q_strncpy( m_ExtraTFWeaponData.m_szExplosionEffect, pszEffect, sizeof( m_ExtraTFWeaponData.m_szExplosionEffect ) );
	}

	pszEffect = pKeyValuesData->GetString( "ExplosionPlayerEffect", NULL );
	if ( pszEffect )
	{
		Q_strncpy( m_ExtraTFWeaponData.m_szExplosionPlayerEffect, pszEffect, sizeof( m_ExtraTFWeaponData.m_szExplosionPlayerEffect ) );
	}

	pszEffect = pKeyValuesData->GetString( "ExplosionWaterEffect", NULL );
	if ( pszEffect )
	{
		Q_strncpy( m_ExtraTFWeaponData.m_szExplosionWaterEffect, pszEffect, sizeof( m_ExtraTFWeaponData.m_szExplosionWaterEffect ) );
	}

	m_ExtraTFWeaponData.m_bDontDrop = ( pKeyValuesData->GetInt( "DontDrop", 0 ) > 0 );
}

// criteria that we parse out of the file.
// tells us which player animation states
// should use the alternate wpn p model
static struct
{
	const char *m_pCriteriaName;
	int m_iFlagValue;
} g_AltWpnCritera[] =
{
	{ "ALTWPN_CRITERIA_FIRING",		ALTWPN_CRITERIA_FIRING },
	{ "ALTWPN_CRITERIA_RELOADING",	ALTWPN_CRITERIA_RELOADING },
	{ "ALTWPN_CRITERIA_DEPLOYED",	ALTWPN_CRITERIA_DEPLOYED },
	{ "ALTWPN_CRITERIA_DEPLOYED_RELOAD", ALTWPN_CRITERIA_DEPLOYED_RELOAD },
	{ "ALTWPN_CRITERIA_PRONE_DEPLOYED_RELOAD", ALTWPN_CRITERIA_PRONE_DEPLOYED_RELOAD }
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFWeaponInfo::DODParse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	m_DODWeaponData.m_iCrosshairMinDistance		= pKeyValuesData->GetInt( "CrosshairMinDistance", 4 );
	m_DODWeaponData.m_iCrosshairDeltaDistance	= pKeyValuesData->GetInt( "CrosshairDeltaDistance", 3 );
	m_DODWeaponData.m_iMuzzleFlashType			= pKeyValuesData->GetFloat( "MuzzleFlashType", 0 );
	m_DODWeaponData.m_flMuzzleFlashScale		= pKeyValuesData->GetFloat( "MuzzleFlashScale", 0.5 );

	m_DODWeaponData.m_iDamage				= pKeyValuesData->GetInt( "Damage", 1 );
	m_DODWeaponData.m_flAccuracy			= pKeyValuesData->GetFloat( "Accuracy", 1.0 );
	m_DODWeaponData.m_flSecondaryAccuracy	= pKeyValuesData->GetFloat( "SecondaryAccuracy", 1.0 );
	m_DODWeaponData.m_flAccuracyMovePenalty = pKeyValuesData->GetFloat( "AccuracyMovePenalty", 0.1 );
	m_DODWeaponData.m_flRecoil				= pKeyValuesData->GetFloat( "Recoil", 99.0 );
	m_DODWeaponData.m_flPenetration			= pKeyValuesData->GetFloat( "Penetration", 1.0 );
	m_DODWeaponData.m_flFireDelay			= pKeyValuesData->GetFloat( "FireDelay", 0.1 );
	m_DODWeaponData.m_flSecondaryFireDelay	= pKeyValuesData->GetFloat( "SecondaryFireDelay", 0.1 );
	m_DODWeaponData.m_flTimeToIdleAfterFire = pKeyValuesData->GetFloat( "IdleTimeAfterFire", 1.0 );
	m_DODWeaponData.m_flIdleInterval		= pKeyValuesData->GetFloat( "IdleInterval", 1.0 );
	m_DODWeaponData.m_bCanDrop				= ( pKeyValuesData->GetInt( "CanDrop", 1 ) > 0 );
	m_DODWeaponData.m_iBulletsPerShot		= pKeyValuesData->GetInt( "BulletsPerShot", 1 );
	
	m_DODWeaponData.m_iHudClipHeight		= pKeyValuesData->GetInt( "HudClipHeight", 0 );
	m_DODWeaponData.m_iHudClipBaseHeight	= pKeyValuesData->GetInt( "HudClipBaseHeight", 0 );
	m_DODWeaponData.m_iHudClipBulletHeight	= pKeyValuesData->GetInt( "HudClipBulletHeight", 0 );

	m_DODWeaponData.m_iAmmoPickupClips		= pKeyValuesData->GetInt( "AmmoPickupClips", 2 );
	
	m_DODWeaponData.m_iDefaultAmmoClips		= pKeyValuesData->GetInt( "DefaultAmmoClips", 0 );

	m_DODWeaponData.m_flViewModelFOV		= pKeyValuesData->GetFloat( "ViewModelFOV", 90.0f );

//	const char *pAnimEx = pKeyValuesData->GetString( "PlayerAnimationExtension", "error" );
//	Q_strncpy( m_szAnimExtension, pAnimEx, sizeof( m_szAnimExtension ) );

	// if this key exists, use this for reload animations instead of anim_prefix
//	Q_strncpy( m_szReloadAnimPrefix, pKeyValuesData->GetString( "reload_anim_prefix", "" ), MAX_WEAPON_PREFIX );

	m_DODWeaponData.m_flBotAudibleRange = pKeyValuesData->GetFloat( "BotAudibleRange", 2000.0f );
	
	m_DODWeaponData.m_iTracerType = pKeyValuesData->GetInt( "Tracer", 0 );

	m_DODWeaponData.m_bIsDOD = ( pKeyValuesData->GetInt( "IsDOD", 1 ) > 0 );

	//Weapon Type
	const char *pTypeString = pKeyValuesData->GetString( "WeaponType", NULL );
	
	m_DODWeaponData.m_WeaponType = WPN_TYPE_UNKNOWN;
	if ( !pTypeString )
	{
		Assert( false );
	}
	else if ( Q_stricmp( pTypeString, "Melee" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_MELEE;
	}
	else if ( Q_stricmp( pTypeString, "Camera" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_CAMERA;
	}
	else if ( Q_stricmp( pTypeString, "Grenade" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_GRENADE;
	}
	else if ( Q_stricmp( pTypeString, "Pistol" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_PISTOL;
	}
	else if ( Q_stricmp( pTypeString, "Rifle" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_RIFLE;
	}
	else if ( Q_stricmp( pTypeString, "Sniper" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_SNIPER;
	}
	else if ( Q_stricmp( pTypeString, "SubMG" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_SUBMG;
	}
	else if ( Q_stricmp( pTypeString, "MG" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_MG;
	}
	else if ( Q_stricmp( pTypeString, "Bazooka" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_BAZOOKA;
	}
	else if ( Q_stricmp( pTypeString, "Bandage" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_BANDAGE;
	}
	else if ( Q_stricmp( pTypeString, "Sidearm" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_SIDEARM;
	}
	else if ( Q_stricmp( pTypeString, "RifleGrenade" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_RIFLEGRENADE;
	}
	else if ( Q_stricmp( pTypeString, "Bomb" ) == 0 )
	{
		m_DODWeaponData.m_WeaponType = WPN_TYPE_BOMB;
	}
	else
	{
		Assert( false );
	}

	Q_strncpy( m_DODWeaponData.m_szReloadModel, pKeyValuesData->GetString( "reloadmodel" ), sizeof( m_DODWeaponData.m_szReloadModel ) );
	Q_strncpy( m_DODWeaponData.m_szDeployedModel, pKeyValuesData->GetString( "deployedmodel" ), sizeof( m_DODWeaponData.m_szDeployedModel ) );
	Q_strncpy( m_DODWeaponData.m_szDeployedReloadModel, pKeyValuesData->GetString( "deployedreloadmodel" ), sizeof( m_DODWeaponData.m_szDeployedReloadModel ) );
	Q_strncpy( m_DODWeaponData.m_szProneDeployedReloadModel, pKeyValuesData->GetString( "pronedeployedreloadmodel" ), sizeof( m_DODWeaponData.m_szProneDeployedReloadModel ) );


	m_DODWeaponData.m_iAltWpnCriteria = ALTWPN_CRITERIA_NONE;

	for ( int i=0; i < ARRAYSIZE( g_AltWpnCritera ); i++ )
	{
		int iVal = pKeyValuesData->GetInt( g_AltWpnCritera[i].m_pCriteriaName, 0 );
		if ( iVal == 1 )
		{
			m_DODWeaponData.m_iAltWpnCriteria |= g_AltWpnCritera[i].m_iFlagValue;
		}
	}

	const char *szNormalOffset = pKeyValuesData->GetString( "vm_normal_offset", "0 0 0" );
	const char *szProneOffset = pKeyValuesData->GetString( "vm_prone_offset", "0 0 0" );
	const char *szIronSightOffset = pKeyValuesData->GetString( "vm_ironsight_offset", "0 0 0" );

	sscanf( szNormalOffset, "%f %f %f", 
		&m_DODWeaponData.m_vecViewNormalOffset[0], 
		&m_DODWeaponData.m_vecViewNormalOffset[1], 
		&m_DODWeaponData.m_vecViewNormalOffset[2]);
	sscanf( szProneOffset, "%f %f %f", 
		&m_DODWeaponData.m_vecViewProneOffset[0], 
		&m_DODWeaponData.m_vecViewProneOffset[1], 
		&m_DODWeaponData.m_vecViewProneOffset[2]);
	sscanf( szIronSightOffset, "%f %f %f", 
		&m_DODWeaponData.m_vecIronSightOffset[0], 
		&m_DODWeaponData.m_vecIronSightOffset[1], 
		&m_DODWeaponData.m_vecIronSightOffset[2]);

	m_DODWeaponData.m_iDefaultTeam = TF_TEAM_RED;

	const char *pDefaultTeam = pKeyValuesData->GetString( "default_team", NULL );

	if ( pDefaultTeam )
	{
		if ( FStrEq( pDefaultTeam, "Axis" ) )
		{
			m_DODWeaponData.m_iDefaultTeam = TF_TEAM_BLUE;
		}
		else if ( FStrEq( pDefaultTeam, "Allies" ) )
		{
			m_DODWeaponData.m_iDefaultTeam = TF_TEAM_RED;
		}
		else
		{
			Assert( !"invalid param to \"default_team\" in weapon scripts\n" );
		}
	}
}

void CTFWeaponInfo::HL2Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

	m_HL2WeaponData.m_iPlayerDamage = pKeyValuesData->GetInt( "damage", 0 );
}

//--------------------------------------------------------------------------------------------------------
struct WeaponTypeInfo
{
	CSWeaponType type;
	const char * name;
};


//--------------------------------------------------------------------------------------------------------
WeaponTypeInfo s_weaponTypeInfo[] =
{
	{ WEAPONTYPE_KNIFE,			"Knife" },
	{ WEAPONTYPE_PISTOL,		"Pistol" },
	{ WEAPONTYPE_SUBMACHINEGUN,	"Submachine Gun" },	// First match is printable
	{ WEAPONTYPE_SUBMACHINEGUN,	"submachinegun" },
	{ WEAPONTYPE_SUBMACHINEGUN,	"smg" },
	{ WEAPONTYPE_RIFLE,			"Rifle" },
	{ WEAPONTYPE_SHOTGUN,		"Shotgun" },
	{ WEAPONTYPE_SNIPER_RIFLE,	"Sniper Rifle" },	// First match is printable
	{ WEAPONTYPE_SNIPER_RIFLE,	"SniperRifle" },
	{ WEAPONTYPE_MACHINEGUN,	"Machine Gun" },		// First match is printable
	{ WEAPONTYPE_MACHINEGUN,	"machinegun" },
	{ WEAPONTYPE_MACHINEGUN,	"mg" },
	{ WEAPONTYPE_C4,			"C4" },
	{ WEAPONTYPE_GRENADE,		"Grenade" },
};


struct WeaponNameInfo
{
	int id;
	const char *name;
};

WeaponNameInfo s_weaponNameInfo[] =
{
	{ CS_WEAPON_P228,				"cs_weapon_p228" },
	{ CS_WEAPON_GLOCK,				"cs_weapon_glock" },
	{ CS_WEAPON_SCOUT,				"cs_weapon_scout" },
	{ CS_WEAPON_HEGRENADE,			"cs_weapon_hegrenade" },
	{ CS_WEAPON_XM1014,				"cs_weapon_xm1014" },
	{ CS_WEAPON_C4,					"cs_weapon_c4" },
	{ CS_WEAPON_MAC10,				"cs_weapon_mac10" },
	{ CS_WEAPON_AUG,				"cs_weapon_aug" },
	{ CS_WEAPON_SMOKEGRENADE,		"cs_weapon_smokegrenade" },
	{ CS_WEAPON_ELITE,				"cs_weapon_elite" },
	{ CS_WEAPON_FIVESEVEN,			"cs_weapon_fiveseven" },
	{ CS_WEAPON_UMP45,				"cs_weapon_ump45" },
	{ CS_WEAPON_SG550,				"cs_weapon_sg550" },

	{ CS_WEAPON_GALIL,				"cs_weapon_galil" },
	{ CS_WEAPON_FAMAS,				"cs_weapon_famas" },
	{ CS_WEAPON_USP,				"cs_weapon_usp" },
	{ CS_WEAPON_AWP,				"cs_weapon_awp" },
	{ CS_WEAPON_MP5NAVY,			"cs_weapon_mp5navy" },
	{ CS_WEAPON_M249,				"cs_weapon_m249" },
	{ CS_WEAPON_M3,					"cs_weapon_m3" },
	{ CS_WEAPON_M4A1,				"cs_weapon_m4a1" },
	{ CS_WEAPON_TMP,				"cs_weapon_tmp" },
	{ CS_WEAPON_G3SG1,				"cs_weapon_g3sg1" },
	{ CS_WEAPON_FLASHBANG,			"cs_weapon_flashbang" },
	{ CS_WEAPON_DEAGLE,				"cs_weapon_deagle" },
	{ CS_WEAPON_SG552,				"cs_weapon_sg552" },
	{ CS_WEAPON_AK47,				"cs_weapon_ak47" },
	{ CS_WEAPON_KNIFE,				"cs_weapon_knife" },
	{ CS_WEAPON_P90,				"cs_weapon_p90" },

	// not sure any of these are needed
	{ CS_WEAPON_SHIELDGUN,			"cs_weapon_shieldgun" },
	{ CS_WEAPON_KEVLAR,				"cs_weapon_kevlar" },
	{ CS_WEAPON_ASSAULTSUIT,		"cs_weapon_assaultsuit" },
	{ CS_WEAPON_NVG,				"cs_weapon_nvg" },

	{ TF_WEAPON_NONE,				"tf_weapon_none" },
};



//--------------------------------------------------------------------------------------------------------------


CTFWeaponInfo g_EquipmentInfo[MAX_EQUIPMENT];

void PrepareEquipmentInfo( void )
{
	memset( g_EquipmentInfo, 0, ARRAYSIZE( g_EquipmentInfo ) );

	// SEALTODO BLACKMARKET
#if 0
	g_EquipmentInfo[2].m_CSWeaponData.SetWeaponPrice( TFGameRules()->GetBlackMarketPriceForWeapon( CS_WEAPON_KEVLAR ) );
	g_EquipmentInfo[2].m_CSWeaponData.SetDefaultPrice( KEVLAR_PRICE );
	g_EquipmentInfo[2].m_CSWeaponData.SetPreviousPrice( TFGameRules()->GetBlackMarketPreviousPriceForWeapon( CS_WEAPON_KEVLAR ) );
#else
	g_EquipmentInfo[2].m_CSWeaponData.SetWeaponPrice( KEVLAR_PRICE );
	g_EquipmentInfo[2].m_CSWeaponData.SetDefaultPrice( KEVLAR_PRICE );
	g_EquipmentInfo[2].m_CSWeaponData.SetPreviousPrice( KEVLAR_PRICE );
#endif
	g_EquipmentInfo[2].m_CSWeaponData.m_iTeam = TEAM_UNASSIGNED;
	Q_strcpy( g_EquipmentInfo[2].szClassName, "cs_weapon_vest" );

#ifdef CLIENT_DLL
	g_EquipmentInfo[2].iconActive = new CHudTexture;
	g_EquipmentInfo[2].iconActive->cCharacterInFont = 't';
#endif

#if 0
	g_EquipmentInfo[1].m_CSWeaponData.SetWeaponPrice( TFGameRules()->GetBlackMarketPriceForWeapon( CS_WEAPON_ASSAULTSUIT ) );
	g_EquipmentInfo[1].m_CSWeaponData.SetDefaultPrice( ASSAULTSUIT_PRICE );
	g_EquipmentInfo[1].m_CSWeaponData.SetPreviousPrice( TFGameRules()->GetBlackMarketPreviousPriceForWeapon( CS_WEAPON_ASSAULTSUIT ) );
#else
	g_EquipmentInfo[1].m_CSWeaponData.SetWeaponPrice( ASSAULTSUIT_PRICE );
	g_EquipmentInfo[1].m_CSWeaponData.SetDefaultPrice( ASSAULTSUIT_PRICE );
	g_EquipmentInfo[1].m_CSWeaponData.SetPreviousPrice( ASSAULTSUIT_PRICE );
#endif
	g_EquipmentInfo[1].m_CSWeaponData.m_iTeam = TEAM_UNASSIGNED;
	Q_strcpy( g_EquipmentInfo[1].szClassName, "cs_weapon_vesthelm" );

#ifdef CLIENT_DLL
	g_EquipmentInfo[1].iconActive = new CHudTexture;
	g_EquipmentInfo[1].iconActive->cCharacterInFont = 'u';
#endif

#if 0
	g_EquipmentInfo[0].m_CSWeaponData.SetWeaponPrice( TFGameRules()->GetBlackMarketPriceForWeapon( CS_WEAPON_NVG ) );
	g_EquipmentInfo[0].m_CSWeaponData.SetPreviousPrice( TFGameRules()->GetBlackMarketPreviousPriceForWeapon( CS_WEAPON_NVG ) );
#else
	g_EquipmentInfo[0].m_CSWeaponData.SetWeaponPrice( NVG_PRICE );
	g_EquipmentInfo[0].m_CSWeaponData.SetPreviousPrice( NVG_PRICE );
#endif
	g_EquipmentInfo[0].m_CSWeaponData.SetDefaultPrice( NVG_PRICE );
	g_EquipmentInfo[0].m_CSWeaponData.m_iTeam = TEAM_UNASSIGNED;
	Q_strcpy( g_EquipmentInfo[0].szClassName, "cs_weapon_nvgs" );

#ifdef CLIENT_DLL
	g_EquipmentInfo[0].iconActive = new CHudTexture;
	g_EquipmentInfo[0].iconActive->cCharacterInFont = 's';
#endif

}

CTFWeaponInfo *GetCSWeaponInfo( int iWeapon )
{
	if ( iWeapon == TF_WEAPON_NONE )
		return NULL;

	if ( iWeapon >= CS_WEAPON_KEVLAR )
	{
		int iIndex = ( CS_WEAPON_COUNT - iWeapon ) - 1;

		return &g_EquipmentInfo[iIndex];

	}

	return GetTFWeaponInfo( iWeapon );
}

//--------------------------------------------------------------------------------------------------------
const char* WeaponClassAsString( CSWeaponType weaponType )
{
	for ( int i = 0; i < ARRAYSIZE(s_weaponTypeInfo); ++i )
	{
		if ( s_weaponTypeInfo[i].type == weaponType )
		{
			return s_weaponTypeInfo[i].name;
		}
	}

	return NULL;
}


//--------------------------------------------------------------------------------------------------------
CSWeaponType WeaponClassFromString( const char* weaponType )
{
	for ( int i = 0; i < ARRAYSIZE(s_weaponTypeInfo); ++i )
	{
		if ( !Q_stricmp( s_weaponTypeInfo[i].name, weaponType ) )
		{
			return s_weaponTypeInfo[i].type;
		}
	}

	return WEAPONTYPE_UNKNOWN;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFWeaponInfo::CSParse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

	m_CSWeaponData.m_flMaxSpeed = (float)pKeyValuesData->GetInt( "MaxPlayerSpeed", 1 );

	m_CSWeaponData.m_iDefaultPrice = m_CSWeaponData.m_iWeaponPrice = pKeyValuesData->GetInt( "WeaponPrice", -1 );
	if ( m_CSWeaponData.m_iWeaponPrice == -1 )
	{
		// This weapon should have the price in its script.
		Assert( false );
	}

	// SEALTODO
#if 0
	if ( TFGameRules()->IsBlackMarket() )
	{
		int iWeaponID = AliasToWeaponId( GetTranslatedWeaponAlias ( szWeaponName ) );

		m_CSWeaponData.m_iDefaultPrice = m_CSWeaponData.m_iWeaponPrice;
		m_CSWeaponData.m_iPreviousPrice = TFGameRules()->GetBlackMarketPreviousPriceForWeapon( iWeaponID );
		m_CSWeaponData.m_iWeaponPrice = TFGameRules()->GetBlackMarketPriceForWeapon( iWeaponID );
	}
#endif
		
	m_CSWeaponData.m_flArmorRatio				= pKeyValuesData->GetFloat( "WeaponArmorRatio", 1 );
	m_CSWeaponData.m_iCrosshairMinDistance		= pKeyValuesData->GetInt( "CrosshairMinDistance", 4 );
	m_CSWeaponData.m_iCrosshairDeltaDistance	= pKeyValuesData->GetInt( "CrosshairDeltaDistance", 3 );
	m_CSWeaponData.m_bCanUseWithShield			= !!pKeyValuesData->GetInt( "CanEquipWithShield", false );
	m_CSWeaponData.m_flMuzzleScale				= pKeyValuesData->GetFloat( "MuzzleFlashScale", 1 );

	const char *pMuzzleFlashStyle = pKeyValuesData->GetString( "MuzzleFlashStyle", "CS_MUZZLEFLASH_NORM" );
	
	if( pMuzzleFlashStyle )
	{
		//if ( Q_stricmp( pMuzzleFlashStyle, "CS_MUZZLEFLASH_X" ) == 0 )
		//{
		//	m_CSWeaponData.m_iMuzzleFlashStyle = CS_MUZZLEFLASH_X;
		//}
		//else if ( Q_stricmp( pMuzzleFlashStyle, "CS_MUZZLEFLASH_NONE" ) == 0 )
		//{
		//	m_CSWeaponData.m_iMuzzleFlashStyle = CS_MUZZLEFLASH_NONE;
		//}
		//else
		//{
		//	m_CSWeaponData.m_iMuzzleFlashStyle = CS_MUZZLEFLASH_NORM;
		//}
	}
	else
	{
		Assert( false );
	}

	m_CSWeaponData.m_iPenetration		= pKeyValuesData->GetInt( "Penetration", 1 );
	m_CSWeaponData.m_iDamage			= pKeyValuesData->GetInt( "Damage", 42 ); // Douglas Adams 1952 - 2001
	m_CSWeaponData.m_flRange			= pKeyValuesData->GetFloat( "Range", 8192.0f );
	m_CSWeaponData.m_flRangeModifier	= pKeyValuesData->GetFloat( "RangeModifier", 0.98f );
	m_CSWeaponData.m_iBullets			= pKeyValuesData->GetInt( "Bullets", 1 );
	m_CSWeaponData.m_flCycleTime		= pKeyValuesData->GetFloat( "CycleTime", 0.15 );
	m_CSWeaponData.m_bAccuracyQuadratic = pKeyValuesData->GetBool ("AccuracyQuadratic", 0);
	m_CSWeaponData.m_flAccuracyDivisor	= pKeyValuesData->GetFloat( "AccuracyDivisor", -1 ); // -1 = off
	m_CSWeaponData.m_flAccuracyOffset	= pKeyValuesData->GetFloat( "AccuracyOffset", 0 );
	m_CSWeaponData.m_flMaxInaccuracy	= pKeyValuesData->GetFloat( "MaxInaccuracy", 0 );

	// new accuracy model parameters
	m_CSWeaponData.m_fSpread[0]				= pKeyValuesData->GetFloat("Spread", 0.0f);
	m_CSWeaponData.m_fInaccuracyCrouch[0]		= pKeyValuesData->GetFloat("InaccuracyCrouch", 0.0f);
	m_CSWeaponData.m_fInaccuracyStand[0]		= pKeyValuesData->GetFloat("InaccuracyStand", 0.0f);
	m_CSWeaponData.m_fInaccuracyJump[0]		= pKeyValuesData->GetFloat("InaccuracyJump", 0.0f);
	m_CSWeaponData.m_fInaccuracyLand[0]		= pKeyValuesData->GetFloat("InaccuracyLand", 0.0f);
	m_CSWeaponData.m_fInaccuracyLadder[0]		= pKeyValuesData->GetFloat("InaccuracyLadder", 0.0f);
	m_CSWeaponData.m_fInaccuracyImpulseFire[0]	= pKeyValuesData->GetFloat("InaccuracyFire", 0.0f);
	m_CSWeaponData.m_fInaccuracyMove[0]		= pKeyValuesData->GetFloat("InaccuracyMove", 0.0f);

	m_CSWeaponData.m_fSpread[1]				= pKeyValuesData->GetFloat("SpreadAlt", 0.0f);
	m_CSWeaponData.m_fInaccuracyCrouch[1]		= pKeyValuesData->GetFloat("InaccuracyCrouchAlt", 0.0f);
	m_CSWeaponData.m_fInaccuracyStand[1]		= pKeyValuesData->GetFloat("InaccuracyStandAlt", 0.0f);
	m_CSWeaponData.m_fInaccuracyJump[1]		= pKeyValuesData->GetFloat("InaccuracyJumpAlt", 0.0f);
	m_CSWeaponData.m_fInaccuracyLand[1]		= pKeyValuesData->GetFloat("InaccuracyLandAlt", 0.0f);
	m_CSWeaponData.m_fInaccuracyLadder[1]		= pKeyValuesData->GetFloat("InaccuracyLadderAlt", 0.0f);
	m_CSWeaponData.m_fInaccuracyImpulseFire[1]	= pKeyValuesData->GetFloat("InaccuracyFireAlt", 0.0f);
	m_CSWeaponData.m_fInaccuracyMove[1]		= pKeyValuesData->GetFloat("InaccuracyMoveAlt", 0.0f);

	m_CSWeaponData.m_fInaccuracyReload			= pKeyValuesData->GetFloat("InaccuracyReload", 0.0f);
	m_CSWeaponData.m_fInaccuracyAltSwitch		= pKeyValuesData->GetFloat("InaccuracyAltSwitch", 0.0f);

	m_CSWeaponData.m_fRecoveryTimeCrouch		= pKeyValuesData->GetFloat("RecoveryTimeCrouch", 1.0f);
	m_CSWeaponData.m_fRecoveryTimeStand		= pKeyValuesData->GetFloat("RecoveryTimeStand", 1.0f);

	m_CSWeaponData.m_flTimeToIdleAfterFire	= pKeyValuesData->GetFloat( "TimeToIdle", 2 );
	m_CSWeaponData.m_flIdleInterval	= pKeyValuesData->GetFloat( "IdleInterval", 20 );

	// Figure out what team can have this weapon.
	m_CSWeaponData.m_iTeam = TEAM_UNASSIGNED;
	const char *pTeam = pKeyValuesData->GetString( "Team", NULL );
	if ( pTeam )
	{
		if ( Q_stricmp( pTeam, "CT" ) == 0 )
		{
			m_CSWeaponData.m_iTeam = TF_TEAM_RED;
		}
		else if ( Q_stricmp( pTeam, "TERRORIST" ) == 0 )
		{
			m_CSWeaponData.m_iTeam = TF_TEAM_BLUE;
		}
		else if ( Q_stricmp( pTeam, "ANY" ) == 0 )
		{
			m_CSWeaponData.m_iTeam = TEAM_UNASSIGNED;
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

	
	const char *pWrongTeamMsg = pKeyValuesData->GetString( "WrongTeamMsg", "" );
	Q_strncpy( m_CSWeaponData.m_WrongTeamMsg, pWrongTeamMsg, sizeof( m_CSWeaponData.m_WrongTeamMsg ) );

	const char *pShieldViewModel = pKeyValuesData->GetString( "shieldviewmodel", "" );
	Q_strncpy( m_CSWeaponData.m_szShieldViewModel, pShieldViewModel, sizeof( m_CSWeaponData.m_szShieldViewModel ) );
	
	const char *pAnimEx = pKeyValuesData->GetString( "PlayerAnimationExtension", "m4" );
	Q_strncpy( m_CSWeaponData.m_szAnimExtension, pAnimEx, sizeof( m_CSWeaponData.m_szAnimExtension ) );

	// Default is 2000.
	m_CSWeaponData.m_flBotAudibleRange = pKeyValuesData->GetFloat( "BotAudibleRange", 2000.0f );
	
	const char *pTypeString = pKeyValuesData->GetString( "WeaponType", "" );
	m_CSWeaponData.m_WeaponType = WeaponClassFromString( pTypeString );

	m_CSWeaponData.m_bFullAuto = pKeyValuesData->GetBool("FullAuto");

	// Read the addon model.
	Q_strncpy( m_CSWeaponData.m_szAddonModel, pKeyValuesData->GetString( "AddonModel" ), sizeof( m_CSWeaponData.m_szAddonModel ) );

	// Read the dropped model.
	Q_strncpy( m_CSWeaponData.m_szDroppedModel, pKeyValuesData->GetString( "DroppedModel" ), sizeof( m_CSWeaponData.m_szDroppedModel ) );

	// Read the silencer model.
	Q_strncpy( m_CSWeaponData.m_szSilencerModel, pKeyValuesData->GetString( "SilencerModel" ), sizeof( m_CSWeaponData.m_szSilencerModel ) );

#ifndef CLIENT_DLL
	// Enforce consistency for the weapon here, since that way we don't need to save off the model bounds
	// for all time.
	// Moved to pure_server_minimal.txt
//	engine->ForceExactFile( UTIL_VarArgs("scripts/%s.ctx", szWeaponName ) );

	// Model bounds are rounded to the nearest integer, then extended by 1
	engine->ForceModelBounds( szWorldModel, Vector( -15, -12, -18 ), Vector( 44, 16, 19 ) );
	if ( m_CSWeaponData.m_szAddonModel[0] )
	{
		engine->ForceModelBounds( m_CSWeaponData.m_szAddonModel, Vector( -5, -5, -6 ), Vector( 13, 5, 7 ) );
	}
	if ( m_CSWeaponData.m_szSilencerModel[0] )
	{
		engine->ForceModelBounds( m_CSWeaponData.m_szSilencerModel, Vector( -15, -12, -18 ), Vector( 44, 16, 19 ) );
	}
#endif // !CLIENT_DLL
}