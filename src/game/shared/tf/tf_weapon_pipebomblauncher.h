//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef TF_WEAPON_PIPEBOMBLAUNCHER_H
#define TF_WEAPON_PIPEBOMBLAUNCHER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_gun.h"
#include "tf_weapon_grenade_pipebomb.h"

// Client specific.
#ifdef CLIENT_DLL
#define CTFPipebombLauncher C_TFPipebombLauncher
#endif

#define TF_PIPEBOMB_MAX_CHARGE_TIME	 4.0f

#define TF_DETONATE_MODE_STANDARD	0
#define TF_DETONATE_MODE_DOT		1
#define TF_DETONATE_MODE_AIR		2

// hard code these eventually
#define TF_PIPEBOMB_MIN_CHARGE_VEL 900
#define TF_PIPEBOMB_MAX_CHARGE_VEL 2400


//=============================================================================
//
// TF Weapon Pipebomb Launcher.
//
#ifdef GAME_DLL
	class CTFPipebombLauncher : public CTFWeaponBaseGun, public ITFChargeUpWeapon, public IEntityListener
#else
	class CTFPipebombLauncher : public CTFWeaponBaseGun, public ITFChargeUpWeapon
#endif
{
public:

	DECLARE_CLASS( CTFPipebombLauncher, CTFWeaponBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	// Server specific.
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CTFPipebombLauncher();
	~CTFPipebombLauncher();

	virtual void	Spawn( void );
	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_PIPEBOMBLAUNCHER; }
	virtual CBaseEntity *FireProjectile( CTFPlayer *pPlayer );
	virtual void	ItemBusyFrame( void );
	virtual void	ItemPostFrame( void );
	virtual void	SecondaryAttack();

	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual bool	Deploy( void );
	virtual void	PrimaryAttack( void );
	virtual void	WeaponIdle( void );
	virtual float	GetProjectileSpeed( void );
	virtual bool	Reload( void );
	virtual void	WeaponReset( void );
	virtual bool	CanPerformSecondaryAttack(void) const;

public:
	// ITFChargeUpWeapon
	virtual bool CanCharge() { return true; }
	virtual float GetChargeBeginTime( void ) { return m_flChargeBeginTime; }
	virtual float GetChargeMaxTime( void ) { float flChargeTime = TF_PIPEBOMB_MAX_CHARGE_TIME; return flChargeTime; }
	virtual float GetChargeForceReleaseTime( void ) { return GetChargeMaxTime(); }
	int	GetPipeBombCount( void ) { return m_iPipebombCount; }
	const CUtlVector< CHandle< CTFGrenadePipebombProjectile > > &GetPipeBombVector( void ) const;
	int			  GetDetonateMode( void ) const { int iMode = 0; return iMode; };
	bool		  CanDestroyStickies( void ) const { int iMode = 0; return (iMode == 1); };

	virtual void LaunchGrenade( void );
	virtual void ForceLaunchGrenade( void ) { LaunchGrenade(); }
	virtual bool DetonateRemotePipebombs( bool bFizzle );
	virtual void AddPipeBomb( CTFGrenadePipebombProjectile *pBomb );
	void			DeathNotice( CBaseEntity *pVictim );

#ifdef GAME_DLL
	void			UpdateOnRemove( void );

protected:

	// This is here so we can network the pipebomb count for prediction purposes
	CNetworkVar( int,				m_iPipebombCount );	
#endif

#ifdef CLIENT_DLL
	int				m_iPipebombCount;
	float			m_flNextBombCheckTime;
	bool			m_bBombThinking;
#endif

	// List of active pipebombs
	typedef CHandle<CTFGrenadePipebombProjectile>	PipebombHandle;
	CUtlVector<PipebombHandle>		m_Pipebombs;

	CNetworkVar( float, m_flChargeBeginTime );
	float	m_flLastDenySoundTime;
	bool	m_bNoAutoRelease;
	bool	m_bWantsToShoot;

private:

	CTFPipebombLauncher( const CTFPipebombLauncher & ) {}
};


inline const CUtlVector< CHandle< CTFGrenadePipebombProjectile > > &CTFPipebombLauncher::GetPipeBombVector( void ) const
{
	return m_Pipebombs;
}


#endif // TF_WEAPON_PIPEBOMBLAUNCHER_H
