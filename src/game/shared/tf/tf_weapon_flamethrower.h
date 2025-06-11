//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================
#ifndef TF_WEAPON_FLAMETHROWER_H
#define TF_WEAPON_FLAMETHROWER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_gun.h"
#include "tf_weaponbase_rocket.h"

// Client specific.
#ifdef CLIENT_DLL
	#include "particlemgr.h"
	#include "particle_util.h"
	#include "particles_simple.h"
	#include "c_tf_projectile_rocket.h"

	#define CTFFlameThrower C_TFFlameThrower
#else
	#include "tf_projectile_rocket.h"
	#include "baseentity.h"
	#include "iscorer.h"
#endif

enum FlameThrowerState_t
{
	// Firing states.
	FT_STATE_IDLE = 0,
	FT_STATE_STARTFIRING,
	FT_STATE_FIRING,
	FT_STATE_SECONDARY,
};

enum EFlameThrowerAirblastFunction
{
	TF_FUNCTION_AIRBLAST_PUSHBACK					= 0x01,
	TF_FUNCTION_AIRBLAST_PUT_OUT_TEAMMATES			= 0x02,
	TF_FUNCTION_AIRBLAST_REFLECT_PROJECTILES		= 0x04,

	TF_FUNCTION_AIRBLAST_PUSHBACK__STUN				= 0x08,			// dependent on TF_FUNCTION_AIRBLAST_PUSHBACK
	TF_FUNCTION_AIRBLAST_PUSHBACK__VIEW_PUNCH		= 0x10,			// dependent on TF_FUNCTION_AIRBLAST_PUSHBACK
};

//=========================================================
// Flamethrower Weapon
//=========================================================
#ifdef GAME_DLL
class CTFFlameThrower : public CTFWeaponBaseGun
#else
class CTFFlameThrower : public CTFWeaponBaseGun
#endif // GAME_DLL
{
	DECLARE_CLASS( CTFFlameThrower, CTFWeaponBaseGun );
public:
	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CTFFlameThrower();
	~CTFFlameThrower();

	virtual void	Spawn( void );

	virtual int		GetWeaponID( void ) const { return TF_WEAPON_FLAMETHROWER; }

	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack();
	virtual void	SecondaryAttack();
	virtual bool	Lower( void );
	virtual void	WeaponReset( void );

	virtual void	DestroySounds( void );
	virtual void	Precache( void );

	bool			CanAirBlast() const;

	bool			SupportsAirBlastFunction( EFlameThrowerAirblastFunction eFunction ) const;
	void			FireAirBlast( int iAmmoPerShot );

	float			GetSpinUpTime( void ) const;
	virtual bool	IsFiring( void ) const OVERRIDE { return m_iWeaponState == FT_STATE_FIRING; }
	void			SetWeaponState( int nWeaponState );

	Vector GetVisualMuzzlePos();
	Vector GetFlameOriginPos();

	void			IncrementFlameDamageCount( void );
	void			DecrementFlameDamageCount( void );
	void			IncrementActiveFlameCount( void );
	void			DecrementActiveFlameCount( void );
	void			ResetFlameHitCount( void );

	virtual bool	Deploy( void );

#if defined( CLIENT_DLL )

	virtual void	OnDataChanged(DataUpdateType_t updateType);
	virtual void	UpdateOnRemove( void );
	virtual void	SetDormant( bool bDormant );

	//	Start/stop flame sound and particle effects
	void			StartFlame();
	void			StopFlame( bool bAbrupt = false );

	virtual void		RestartParticleEffect();
	virtual const char* FlameEffectName( bool bIsFirstPersonView );	
	virtual const char* FlameCritEffectName( bool bIsFirstPersonView );

	// constant pilot light sound
	void 			StartPilotLight();
	void 			StopPilotLight();

	// burning sound when you hit a player/building
	void			StopHitSound();

	float			GetFlameHitRatio( void );

#else
	void			SetHitTarget( void );
	void			HitTargetThink( void );

	virtual Vector	GetDeflectionSize();
	virtual bool	DeflectPlayer( CTFPlayer *pTarget, CTFPlayer *pOwner, Vector &vecForward, Vector &vecCenter, Vector &vecSize );
	virtual bool	DeflectEntity( CBaseEntity *pTarget, CTFPlayer *pOwner, Vector &vecForward, Vector &vecCenter, Vector &vecSize );
	virtual void	PlayDeflectionSound( bool bPlayer );
#endif

private:
	Vector GetMuzzlePosHelper( bool bVisualPos );
	CNetworkVar( int, m_iWeaponState );
	CNetworkVar( bool, m_bCritFire );
	CNetworkVar( bool, m_bHitTarget );
	CNetworkVar( int, m_iActiveFlames );		// Number of flames we have in the world
	CNetworkVar( int, m_iDamagingFlames );		// Number of flames that have done damage
	CNetworkVar( float, m_flSpinupBeginTime );

	float m_flStartFiringTime;
	float m_flNextPrimaryAttackAnim;

	int			m_iParticleWaterLevel;
	float		m_flAmmoUseRemainder;
	float		m_flResetBurstEffect;
	bool		m_bFiredSecondary;
	bool		m_bFiredBothAttacks;

#if defined( CLIENT_DLL )
	CSoundPatch	*m_pFiringStartSound;
	CSoundPatch	*m_pFiringLoop;
	CSoundPatch	*m_pFiringAccuracyLoop;
	CSoundPatch	*m_pFiringHitLoop;
	bool		m_bFiringLoopCritical;
	bool		m_bFiringHitTarget;
	CSoundPatch *m_pPilotLightSound;
	CSoundPatch *m_pSpinUpSound;
	float		m_flFlameHitRatio;
	float		m_flPrevFlameHitRatio;
	const char *m_szAccuracySound;

	class FlameEffect_t
	{
	public:
		FlameEffect_t( CTFWeaponBase* pOwner ) : m_pFlameEffect(NULL), m_pOwner(pOwner), m_hEffectWeapon(NULL)
		{}

		void StartEffects( const char* pszEffectName);
		bool StopEffects();

	private:
		CNewParticleEffect*	m_pFlameEffect;
		CTFWeaponBase*		m_pOwner;
		EHANDLE			m_hEffectWeapon;
	};

	FlameEffect_t m_FlameEffects;
#else
	float		m_flTimeToStopHitSound;
#endif

	CTFFlameThrower( const CTFFlameThrower & );
};

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
DECLARE_AUTO_LIST( ITFFlameEntityAutoList );
class CTFFlameEntity : public CBaseEntity, public ITFFlameEntityAutoList
{
	DECLARE_CLASS( CTFFlameEntity, CBaseEntity );
public:

	CTFFlameEntity();
	virtual void Spawn( void );

public:
	static CTFFlameEntity *Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flSpeed, int iDmgType, float m_flDmgAmount, bool bAlwaysCritFromBehind, bool bRandomize=true );

	void FlameThink( void );
	bool IsEntityAttacker( CBaseEntity *pEnt ) { return m_hAttacker.Get() == pEnt; }

private:
	void RemoveFlame();
	void OnCollide( CBaseEntity *pOther );
	void SetHitTarget( void );
	float DotProductToTarget( CBaseEntity *pTarget );
	void UpdateFlameThrowerHitRatio( void );
	float GetFlameFloat( void );
	float GetFlameDrag( void );

	Vector					m_vecInitialPos;		// position the flame was fired from
	Vector					m_vecPrevPos;			// position from previous frame
	Vector					m_vecBaseVelocity;		// base velocity vector of the flame (ignoring rise effect)
	Vector					m_vecAttackerVelocity;	// velocity of attacking player at time flame was fired
	float					m_flTimeRemove;			// time at which the flame should be removed
	int						m_iDmgType;				// damage type
	float					m_flDmgAmount;			// amount of base damage
	CUtlVector<EHANDLE>		m_hEntitiesBurnt;		// list of entities this flame has burnt
	EHANDLE					m_hAttacker;			// attacking player
	int						m_iAttackerTeam;		// team of attacking player
	bool					m_bBurnedEnemy;			// We track hitting to calculate hit/miss ratio in the Flamethrower

	CHandle< CTFFlameThrower > m_hFlameThrower;
};
#endif // GAME_DLL

#endif // TF_WEAPON_FLAMETHROWER_H
