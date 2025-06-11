//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
//	Weapons.
//
//	CTFWeaponBase
//	|
//	|--> CTFWeaponBaseMelee
//	|		|
//	|		|--> CTFWeaponCrowbar
//	|		|--> CTFWeaponKnife
//	|		|--> CTFWeaponMedikit
//	|		|--> CTFWeaponWrench
//	|
//	|--> CTFWeaponBaseGrenade
//	|		|
//	|		|--> CTFWeapon
//	|		|--> CTFWeapon
//	|
//	|--> CTFWeaponBaseGun
//
//=============================================================================
#ifndef TF_WEAPONBASE_H
#define TF_WEAPONBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "pc_weaponbase.h"
#include "tf_playeranimstate.h"
#include "tf_weapon_parse.h"
#include "npcevent.h"
#include "econ/ihasowner.h"

// Client specific.
#if defined( CLIENT_DLL )
#define CTFWeaponBase C_TFWeaponBase
#define CTFWeaponBaseGrenadeProj C_TFWeaponBaseGrenadeProj
#include "tf_fx_muzzleflash.h"
#include "GameEventListener.h"
#endif

#define MAX_TRACER_NAME		128

class CBaseObject;
class CTFWeaponBaseGrenadeProj;

extern ConVar tf_weapon_criticals;

// Reloading singly.
enum
{
	TF_RELOAD_START = 0,
	TF_RELOADING,
	TF_RELOADING_CONTINUE,
	TF_RELOAD_FINISH
};

// structure to encapsulate state of head bob
struct BobState_t
{
	BobState_t() 
	{ 
		m_flBobTime = 0; 
		m_flLastBobTime = 0;
		m_flLastSpeed = 0;
		m_flVerticalBob = 0;
		m_flLateralBob = 0;
	}

	float m_flBobTime;
	float m_flLastBobTime;
	float m_flLastSpeed;
	float m_flVerticalBob;
	float m_flLateralBob;
};

#ifdef CLIENT_DLL
float CalcViewModelBobHelper( CBasePlayer *player, BobState_t *pBobState );
void AddViewModelBobHelper( Vector &origin, QAngle &angles, BobState_t *pBobState );
#endif

#define TF_PARTICLE_WEAPON_BLUE_1	Vector( 0.345, 0.52, 0.635 )
#define TF_PARTICLE_WEAPON_BLUE_2	Vector( 0.145, 0.427, 0.55 )
#define TF_PARTICLE_WEAPON_RED_1	Vector( 0.72, 0.22, 0.23 )
#define TF_PARTICLE_WEAPON_RED_2	Vector( 0.5, 0.18, 0.125 )

//=============================================================================
//
// Base TF Weapon Class
//
#if defined( CLIENT_DLL )
class CTFWeaponBase : public CPCWeaponBase, public CGameEventListener
#else
class CTFWeaponBase : public CPCWeaponBase
#endif
{
	DECLARE_CLASS( CTFWeaponBase, CPCWeaponBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
#if !defined ( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	// Setup.
	CTFWeaponBase();

	virtual void Spawn();
	virtual void Precache();
	virtual void FallInit( void );

	// Weapon Data.
	virtual int	GetDamageType() const { return g_aWeaponDamageTypes[ GetWeaponID() ]; }
	virtual int GetCustomDamageType() const { return TF_DMG_CUSTOM_NONE; }

	virtual void Drop( const Vector &vecVelocity );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual bool Deploy( void );

	virtual bool CanBeCritBoosted( void ) { return true; };

	// Attacks.
	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	void CalcIsAttackCritical( void );
	virtual bool CalcIsAttackCriticalHelper();
	virtual bool CalcIsAttackCriticalHelperNoCrits();
	bool IsCurrentAttackACrit() { return m_bCurrentAttackIsCrit; }
	bool IsCurrentAttackARandomCrit() const { return m_bCurrentAttackIsCrit && m_bCurrentCritIsRandom; }
	virtual void GetProjectileFireSetup( CTFPlayer *pPlayer, Vector vecOffset, Vector *vecSrc, QAngle *angForward, bool bHitTeammates = true, float flEndDist = 2000.f );
	float GetLastPrimaryAttackTime( void ) const { return m_flLastPrimaryAttackTime; }
	virtual bool IsFiring( void ) const { return false; }
	virtual bool AreRandomCritsEnabled( void ) { return tf_weapon_criticals.GetBool(); }

	// Reloads.
	virtual bool Reload( void );
	virtual void AbortReload( void );
	virtual bool DefaultReload( int iClipSize1, int iClipSize2, int iActivity );
	void SendReloadEvents();
	virtual bool IsReloading() const;			// is the weapon reloading right now?

	virtual bool CanDrop( void ) { return false; }

	// Sound.
	bool PlayEmptySound();

	// Activities.
	virtual void ItemBusyFrame( void );
	virtual void ItemPostFrame( void );

	virtual void SetWeaponVisible( bool visible );

	virtual acttable_t *ActivityList( int &iActivityCount );
	static acttable_t m_acttablePrimary[];
	static acttable_t m_acttableSecondary[];
	static acttable_t m_acttableMelee[];
	static acttable_t m_acttableBuilding[];
	static acttable_t m_acttablePDA[];

#ifdef GAME_DLL
	virtual void	AddAssociatedObject( CBaseObject *pObject ) { }
	virtual void	RemoveAssociatedObject( CBaseObject *pObject ) { }

	// Deliberately disabled to prevent players picking up fallen weapons.
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) { return; }

	virtual bool	DeflectProjectiles();
	virtual bool	DeflectPlayer( CTFPlayer *pTarget, CTFPlayer *pOwner, Vector &vecForward, Vector &vecCenter, Vector &vecSize );
	virtual bool	DeflectEntity( CBaseEntity *pTarget, CTFPlayer *pOwner, Vector &vecForward, Vector &vecCenter, Vector &vecSize );
	static void		SendObjectDeflectedEvent( CTFPlayer *pNewOwner, CTFPlayer *pPrevOwner, int iWeaponID, CBaseAnimating *pObject );
	static float	DeflectionForce( const Vector &size, float damage, float scale );
	virtual void	PlayDeflectionSound( bool bPlayer ) {}
	virtual Vector	GetDeflectionSize() { return Vector( 128, 128, 64 ); }
#endif

#ifdef CLIENT_DLL
	C_BaseEntity *GetWeaponForEffect();
#endif

	bool CanAttack( void );

	// Raising & Lowering for grenade throws
	bool			WeaponShouldBeLowered( void );
	virtual bool	Ready( void );
	virtual bool	Lower( void );

	virtual void	WeaponIdle( void );

	virtual void	WeaponReset( void );

	// Muzzleflashes
	virtual const char *GetMuzzleFlashEffectName_3rd( void ) { return NULL; }
	virtual const char *GetMuzzleFlashEffectName_1st( void ) { return NULL; }
	virtual const char *GetMuzzleFlashModel( void );
	virtual float	GetMuzzleFlashModelLifetime( void );
	virtual const char *GetMuzzleFlashParticleEffect( void );

	virtual const char	*GetTracerType( void );

	virtual void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	virtual bool		CanFireCriticalShot( bool bIsHeadshot = false ){ return true; }
	virtual bool		CanFireRandomCriticalShot( float flCritChance );

	virtual void		OnControlStunned( void );

	virtual bool		HideWhileStunned( void ) { return true; }

	virtual float		GetLastDeployTime( void ) { return m_flLastDeployTime; }

	virtual bool		HasLastShotCritical( void ) { return false; }

	virtual bool		UseServerRandomSeed( void ) const { return true; }

// Server specific.
#if !defined( CLIENT_DLL )

	// Spawning.
	virtual void CheckRespawn();
	virtual CBaseEntity* Respawn();
	void Materialize();
	void AttemptToMaterialize();

	// Death.
	void Die( void );
	void SetDieThink( bool bDie );

	// Disguise weapon.
	void DisguiseWeaponThink( void );

	// Ammo.
	virtual const Vector& GetBulletSpread();

// Client specific.
#else

	bool			IsFirstPersonView();
	bool			UsingViewModel();
	C_BaseAnimating *GetAppropriateWorldOrViewModel();

	virtual bool	ShouldDraw( void ) OVERRIDE;

	virtual void	ProcessMuzzleFlashEvent( void );
	virtual int		InternalDrawModel( int flags );

	virtual bool	ShouldPredict();
	virtual void	OnDataChanged( DataUpdateType_t type );
	virtual void	OnPreDataChanged( DataUpdateType_t updateType );
	virtual int		CalcOverrideModelIndex( void );
	virtual int		GetWorldModelIndex( void );
	virtual bool	ShouldDrawCrosshair( void );
	virtual void	Redraw( void );
	virtual void	FireGameEvent( IGameEvent *event );

	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );
	virtual ShadowType_t	ShadowCastType( void );
	virtual int		GetSkin();
	BobState_t		*GetBobState();

	bool OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );

	// Model muzzleflashes
	CHandle<C_MuzzleFlashModel>		m_hMuzzleFlashModel[2];

#endif

	CNetworkVar(	bool, m_bDisguiseWeapon );

	CNetworkVar( float, m_flLastFireTime );

	CNetworkVar( float, m_flObservedCritChance );

	virtual int  GetWeaponTypeReference( void ) { return WEAPON_TF; }

protected:
#ifdef CLIENT_DLL
	virtual void CreateMuzzleFlashEffects( C_BaseEntity *pAttachEnt, int nIndex );
#endif // CLIENT_DLL

	// Reloads.
	void UpdateReloadTimers( bool bStart );
	void SetReloadTimer( float flReloadTime );
	bool ReloadSingly( void );
	void ReloadSinglyPostFrame( void );

	bool NeedsReloadForAmmo1( int iClipSize1 ) const;
	bool NeedsReloadForAmmo2( int iClipSize2 ) const;

protected:

	int				m_iWeaponMode;
	CNetworkVar(	int,	m_iReloadMode );
	CNetworkVar( float, m_flReloadPriorNextFire );
	CTFWeaponInfo	*m_pWeaponInfo;
	bool			m_bInAttack;
	bool			m_bInAttack2;
	bool			m_bCurrentAttackIsCrit;
	bool			m_bCurrentCritIsRandom;

	CNetworkVar(	bool,	m_bLowered );

	int				m_iAltFireHint;

	int				m_iReloadStartClipAmount;

	float			m_flCritTime;
	CNetworkVar( float, m_flLastCritCheckTime );	// Deprecated
	int				m_iLastCritCheckFrame;
	int				m_iCurrentSeed;
	float			m_flLastRapidFireCritCheckTime;

	float			m_flLastDeployTime;

	char			m_szTracerName[MAX_TRACER_NAME];

	CNetworkVar(	bool, m_bResetParity );

	float			m_flLastPrimaryAttackTime;

#ifdef CLIENT_DLL
	bool m_bOldResetParity;
#endif

	CNetworkVar(	bool,	m_bReloadedThroughAnimEvent );
private:
	CTFWeaponBase( const CTFWeaponBase & );
};

bool WeaponID_IsSniperRifle( int iWeaponID );

#define WEAPON_RANDOM_RANGE 10000

#endif // TF_WEAPONBASE_H
