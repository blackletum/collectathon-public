//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#ifndef TF_WEAPON_PARSE_H
#define TF_WEAPON_PARSE_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_parse.h"
#include "networkvar.h"
#include "tf_shareddefs.h"

//=============================================================================
//
// TF Weapon Info
//

struct TFWeaponData_t
{
	int		m_nDamage;
	int		m_nBulletsPerShot;
	float	m_flRange;
	float	m_flSpread;
	float	m_flPunchAngle;
	float	m_flTimeFireDelay;				// Time to delay between firing
	float	m_flTimeIdle;					// Time to idle after firing
	float	m_flTimeIdleEmpty;				// Time to idle after firing last bullet in clip
	float	m_flTimeReloadStart;			// Time to start into a reload (ie. shotgun)
	float	m_flTimeReload;					// Time to reload
	bool	m_bDrawCrosshair;				// Should the weapon draw a crosshair
	int		m_iProjectile;					// The type of projectile this mode fires
	int		m_iAmmoPerShot;					// How much ammo each shot consumes
	float	m_flProjectileSpeed;			// Start speed for projectiles (nail, etc.); NOTE: union with something non-projectile
	float	m_flSmackDelay;					// how long after swing should damage happen for melee weapons
	bool	m_bUseRapidFireCrits;

	void Init( void )
	{
		m_nDamage = 0;
		m_nBulletsPerShot = 0;
		m_flRange = 0.0f;
		m_flSpread = 0.0f;
		m_flPunchAngle = 0.0f;
		m_flTimeFireDelay = 0.0f;
		m_flTimeIdle = 0.0f;
		m_flTimeIdleEmpty = 0.0f;
		m_flTimeReloadStart = 0.0f;
		m_flTimeReload = 0.0f;
		m_iProjectile = TF_PROJECTILE_NONE;
		m_iAmmoPerShot = 0;
		m_flProjectileSpeed = 0.0f;
		m_flSmackDelay = 0.0f;
		m_bUseRapidFireCrits = false;
	};
};

struct ExtraTFWeaponData_t
{
	int		m_iWeaponType;
	
	// Grenade.
	bool	m_bGrenade;
	float	m_flDamageRadius;
	float	m_flPrimerTime;
	bool	m_bLowerWeapon;
	bool	m_bSuppressGrenTimer;

	// Skins
	bool	m_bHasTeamSkins_Viewmodel;
	bool	m_bHasTeamSkins_Worldmodel;

	// Muzzle flash
	char	m_szMuzzleFlashModel[128];
	float	m_flMuzzleFlashModelDuration;
	char	m_szMuzzleFlashParticleEffect[128];

	// Tracer
	char	m_szTracerEffect[128];

	// Eject Brass
	bool	m_bDoInstantEjectBrass;
	char	m_szBrassModel[128];

	// Explosion Effect
	char	m_szExplosionSound[128];
	char	m_szExplosionEffect[128];
	char	m_szExplosionPlayerEffect[128];
	char	m_szExplosionWaterEffect[128];

	bool	m_bDontDrop;

	void Init( void )
	{
		m_bGrenade = false;
		m_flDamageRadius = 0.0f;
		m_flPrimerTime = 0.0f;
		m_bSuppressGrenTimer = false;
		m_bLowerWeapon = false;

		m_bHasTeamSkins_Viewmodel = false;
		m_bHasTeamSkins_Worldmodel = false;

		m_szMuzzleFlashModel[0] = '\0';
		m_flMuzzleFlashModelDuration = 0;
		m_szMuzzleFlashParticleEffect[0] = '\0';

		m_szTracerEffect[0] = '\0';

		m_szBrassModel[0] = '\0';
		m_bDoInstantEjectBrass = true;

		m_szExplosionSound[0] = '\0';
		m_szExplosionEffect[0] = '\0';
		m_szExplosionPlayerEffect[0] = '\0';
		m_szExplosionWaterEffect[0] = '\0';

		m_iWeaponType = TF_WPN_TYPE_PRIMARY;
	};
};

#define WPN_TYPE_MELEE			(1<<0)
#define WPN_TYPE_GRENADE		(1<<1)			
//#define WPN_TYPE_GRENADE_LIVE	(1<<2)	//exploding grenades, unused
#define WPN_TYPE_PISTOL			(1<<3)			
#define WPN_TYPE_RIFLE			(1<<4)			
#define WPN_TYPE_SNIPER			(1<<5)		
#define WPN_TYPE_SUBMG			(1<<6)			
#define WPN_TYPE_MG				(1<<7)	//mg42, 30cal
#define WPN_TYPE_BAZOOKA		(1<<8)
#define WPN_TYPE_BANDAGE		(1<<9)
#define WPN_TYPE_SIDEARM		(1<<10)	//carbine - secondary weapons
#define WPN_TYPE_RIFLEGRENADE	(1<<11)
#define WPN_TYPE_BOMB			(1<<12)
#define WPN_TYPE_UNKNOWN		(1<<13)
#define WPN_TYPE_CAMERA			(1<<12)

#define WPN_MASK_GUN	( WPN_TYPE_PISTOL | WPN_TYPE_RIFLE | WPN_TYPE_SNIPER | WPN_TYPE_SUBMG | WPN_TYPE_MG | WPN_TYPE_SIDEARM )

struct DODWeaponData_t
{
	int		m_iDamage;
	int		m_flPenetration;
	int		m_iBulletsPerShot;
	int		m_iMuzzleFlashType;
	float   m_flMuzzleFlashScale;

	bool	m_bCanDrop;

	float	m_flRecoil;

	float	m_flRange;
	float	m_flRangeModifier;

	float	m_flAccuracy;
	float	m_flSecondaryAccuracy;
	float	m_flAccuracyMovePenalty;

	float	m_flFireDelay;
	float	m_flSecondaryFireDelay;

	int		m_iCrosshairMinDistance;
	int		m_iCrosshairDeltaDistance;

	int		m_WeaponType;

	float	m_flBotAudibleRange;

	char	m_szReloadModel[MAX_WEAPON_STRING];
	char	m_szDeployedModel[MAX_WEAPON_STRING];
	char	m_szDeployedReloadModel[MAX_WEAPON_STRING];
	char	m_szProneDeployedReloadModel[MAX_WEAPON_STRING];

	//timers
	float	m_flTimeToIdleAfterFire;	//wait this long until idling after fire
	float	m_flIdleInterval;			//wait this long after idling to idle again

	//ammo
	int		m_iDefaultAmmoClips;
	int		m_iAmmoPickupClips;

	int		m_iHudClipHeight;
	int		m_iHudClipBaseHeight;
	int		m_iHudClipBulletHeight;

	int		m_iTracerType;

	float	m_flViewModelFOV;

	int		m_iAltWpnCriteria;

	Vector	m_vecViewNormalOffset;
	Vector	m_vecViewProneOffset;
	Vector  m_vecIronSightOffset;

	int		m_iDefaultTeam;

	bool	m_bIsDOD;

	void Init( void )
	{
		m_szReloadModel[0] = '\0';
		m_szDeployedModel[0] = '\0';
		m_szDeployedReloadModel[0] = '\0';
		m_szProneDeployedReloadModel[0] = '\0';
	};
};

// CSS Start
//--------------------------------------------------------------------------------------------------------
enum CSWeaponType
{
	WEAPONTYPE_KNIFE = 0,
	WEAPONTYPE_PISTOL,
	WEAPONTYPE_SUBMACHINEGUN,
	WEAPONTYPE_RIFLE,
	WEAPONTYPE_SHOTGUN,
	WEAPONTYPE_SNIPER_RIFLE,
	WEAPONTYPE_MACHINEGUN,
	WEAPONTYPE_C4,
	WEAPONTYPE_GRENADE,
	WEAPONTYPE_UNKNOWN

};

#define MAX_EQUIPMENT (CS_WEAPON_COUNT - CS_WEAPON_KEVLAR)

void PrepareEquipmentInfo( void );
//--------------------------------------------------------------------------------------------------------
const char * WeaponClassAsString( CSWeaponType weaponType );

//--------------------------------------------------------------------------------------------------------
CSWeaponType WeaponClassFromString( const char* weaponType );

struct CSWeaponData_t
{
	float m_flMaxSpeed;			// How fast the player can run while this is his primary weapon.

	CSWeaponType m_WeaponType;

	bool	m_bFullAuto;		// is this a fully automatic weapon?

	int m_iTeam;				// Which team can have this weapon. TEAM_UNASSIGNED if both can have it.
	float m_flBotAudibleRange;	// How far away a bot can hear this weapon.
	float m_flArmorRatio;

	int	  m_iCrosshairMinDistance;
	int	  m_iCrosshairDeltaDistance;
	
	bool  m_bCanUseWithShield;
	
	char m_WrongTeamMsg[32];	// Reference to a string describing the error if someone tries to buy
								// this weapon but they're on the wrong team to have it.
								// Zero-length if no specific message for this weapon.

	char m_szAnimExtension[16];
	char m_szShieldViewModel[64];

	char m_szAddonModel[MAX_WEAPON_STRING];		// If this is set, it is used as the addon model. Otherwise, szWorldModel is used.
	char m_szDroppedModel[MAX_WEAPON_STRING];	// Alternate dropped model, if different from the szWorldModel the player holds
	char m_szSilencerModel[MAX_WEAPON_STRING];	// Alternate model with silencer attached

	int	  m_iMuzzleFlashStyle;
	float m_flMuzzleScale;
	
	// Parameters for FX_FireBullets:
	int		m_iPenetration;
	int		m_iDamage;
	float	m_flRange;
	float	m_flRangeModifier;
	int		m_iBullets;
	float	m_flCycleTime;

	// Variables that control how fast the weapon's accuracy changes as it is fired.
	bool	m_bAccuracyQuadratic;
	float	m_flAccuracyDivisor;
	float	m_flAccuracyOffset;
	float	m_flMaxInaccuracy;

	// variables for new accuracy model
	float m_fSpread[2];
	float m_fInaccuracyCrouch[2];
	float m_fInaccuracyStand[2];
	float m_fInaccuracyJump[2];
	float m_fInaccuracyLand[2];
	float m_fInaccuracyLadder[2];
	float m_fInaccuracyImpulseFire[2];
	float m_fInaccuracyMove[2];
	float m_fRecoveryTimeStand;
	float m_fRecoveryTimeCrouch;
	float m_fInaccuracyReload;
	float m_fInaccuracyAltSwitch;

	// Delay until the next idle animation after shooting.
	float	m_flTimeToIdleAfterFire;
	float	m_flIdleInterval;
   
	int		GetWeaponPrice( void ) const { return m_iWeaponPrice; }
	int		GetDefaultPrice( void ) { return m_iDefaultPrice; }
	int		GetPrevousPrice( void ) { return m_iPreviousPrice; }
	void	SetWeaponPrice( int iPrice ) { m_iWeaponPrice = iPrice; }
	void	SetDefaultPrice( int iPrice ) { m_iDefaultPrice = iPrice; }
	void	SetPreviousPrice( int iPrice ) { m_iPreviousPrice = iPrice; }

	int		GetRealWeaponPrice( void ) { return m_iWeaponPrice; }

	int m_iWeaponPrice;
	int m_iDefaultPrice;
	int m_iPreviousPrice;

	void Init( void )
	{
		m_flMaxSpeed = 1; // This should always be set in the script.
		m_szAddonModel[0] = 0;
	}
};
// CSS End

struct HL2WeaponData_t
{
	int m_iPlayerDamage;

	void Init( void )
	{
		m_iPlayerDamage = 0;
	}
};

class CTFWeaponInfo : public FileWeaponInfo_t
{
public:

	DECLARE_CLASS_GAMEROOT( CTFWeaponInfo, FileWeaponInfo_t );
	
	CTFWeaponInfo();
	~CTFWeaponInfo();
	
	void ConstructTF( void );
	void ConstructDOD( void );
	void ConstructCS( void );
	void ConstructHL2();

	virtual void Parse( ::KeyValues *pKeyValuesData, const char *szWeaponName );
	void TFParse( ::KeyValues *pKeyValuesData, const char *szWeaponName );
	void DODParse( ::KeyValues *pKeyValuesData, const char *szWeaponName );
	void HL2Parse( KeyValues *pKeyValuesData, const char *szWeaponName );
	void CSParse( ::KeyValues *pKeyValuesData, const char *szWeaponName );

	TFWeaponData_t const &GetWeaponData( int iWeapon ) const	{ return m_TFWeaponData[iWeapon]; }
	DODWeaponData_t const &GetDODWeaponData( void ) const		{ return m_DODWeaponData; }
	CSWeaponData_t const &GetCSWeaponData( void ) const			{ return m_CSWeaponData; }

	void GetWeaponFaction( const char *szWeaponName );
	void SetWeaponParseTypeReference( int pParseType ) { m_iParseType = pParseType; }
	virtual int  GetWeaponParseTypeReference( void ) { return m_iParseType; }
	unsigned short int	m_iParseType;

public:

	TFWeaponData_t	m_TFWeaponData[2];
	ExtraTFWeaponData_t m_ExtraTFWeaponData;
	DODWeaponData_t	m_DODWeaponData;
	CSWeaponData_t	m_CSWeaponData;
	HL2WeaponData_t	m_HL2WeaponData;
};

#endif // TF_WEAPON_PARSE_H
