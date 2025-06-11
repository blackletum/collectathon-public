//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_DODBASE_H
#define WEAPON_DODBASE_H
#ifdef _WIN32
#pragma once
#endif

//#include "dod_playeranimstate.h"
//#include "dod_weapon_parse.h"

#if defined( CLIENT_DLL )
#include "c_tf_player.h"
#else
#include "tf_player.h"
#endif

#if defined( CLIENT_DLL )
	#define CWeaponDODBase C_WeaponDODBase
#endif

extern bool	IsPrimaryWeapon( int id );
extern bool IsSecondaryWeapon( int id );

class CTFPlayer;

// These are the names of the ammo types that go in the CAmmoDefs and that the 
// weapon script files reference.

#define CROSSHAIR_CONTRACT_PIXELS_PER_SECOND	7.0f

//Class Heirarchy for dod weapons

/*

  CWeaponDODBase
	|
	|
	|--> CWeaponDODBaseMelee
	|		|
	|		|--> CWeaponSpade
	|		|--> CWeaponUSKnife
	|
	|--> CWeaponDODBaseGrenade
	|		|
	|		|--> CWeaponHandgrenade
	|		|--> CWeaponStickGrenade
	|		|--> CWeaponSmokeGrenadeUS
	|		|--> CWeaponSmokeGrenadeGER
	|
	|--> CWeaponBaseRifleGrenade
	|		|
	|		|--> CWeaponRifleGrenadeUS
	|		|--> CWeaponRifleGrenadeGER
	|
	|--> CDODBaseRocketWeapon
	|		|
	|		|--> CWeaponBazooka
	|		|--> CWeaponPschreck
	|
	|--> CWeaponDODBaseGun
			|
			|--> CDODFullAutoWeapon
			|		|
			|		|--> CWeaponC96
			|		|
			|		|--> CDODFullAutoPunchWeapon
			|		|		|
			|		|		|--> CWeaponThompson
			|		|		|--> CWeaponMP40
			|		|
			|		|--> CDODBipodWeapon
			|				|
			|				|->	CWeapon30Cal
			|				|->	CWeaponMG42
			|
			|--> CDODFireSelectWeapon
			|		|
			|		|--> CWeaponMP44
			|		|--> CWeaponBAR
			|
			|
			|--> CDODSemiAutoWeapon
					|
					|--> CWeaponColt
					|--> CWeaponP38
					|--> CWeaponM1Carbine
					|--> CDODSniperWeapon
						|
						|--> CWeaponSpring
						|--> CWeaponScopedK98
						|--> CWeaponGarand
						|--> CWeaponK98

*/

class CWeaponDODBase : public CPCWeaponBase
{
public:
	DECLARE_CLASS( CWeaponDODBase, CPCWeaponBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponDODBase();

	#ifdef GAME_DLL
		DECLARE_DATADESC();

		virtual void CheckRespawn();
		virtual CBaseEntity* Respawn();
		
		virtual const Vector& GetBulletSpread();
		virtual float	GetDefaultAnimSpeed();

		virtual void	ItemBusyFrame();
		virtual bool	ShouldRemoveOnRoundRestart();

		void Materialize();
		void AttemptToMaterialize();
		
	#else

		void PlayWorldReloadSound( CTFPlayer *pPlayer );

	#endif

	virtual bool	DefaultReload( int iClipSize1, int iClipSize2, int iActivity );

	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	Drop( const Vector &vecVelocity );
	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );
	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted() const;

	virtual int	    ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_USE_IN_RADIUS; }

	virtual void WeaponIdle( void );
	virtual Activity GetIdleActivity( void );

	// Get specific DOD weapon ID (ie: DOD_WEAPON_GARAND, etc)
	virtual int GetStatsWeaponID( void )		{ return GetWeaponID(); }
	virtual int GetAltWeaponID( void ) const	{ return TF_WEAPON_NONE; }

	// return true if this weapon has a silencer equipped
	virtual bool IsSilenced( void ) const				{ return false; }

	void KickBack( float up_base, float lateral_base, float up_modifier, float lateral_modifier, float up_max, float lateral_max, int direction_change );

	virtual void SetWeaponModelIndex( const char *pName );

	virtual bool CanDrop( void ) { return false; }

	virtual bool ShouldDrawCrosshair( void ) { return true; }
	virtual bool ShouldDrawViewModel( void ) { return true; }
	virtual bool ShouldDrawMuzzleFlash( void ) { return true; }

	virtual float GetWeaponAccuracy( float flPlayerSpeed ) { return 0; }

	virtual bool HideViewModelWhenZoomed( void ) { return false; }

	virtual bool CanAttack( void );
	virtual bool ShouldAutoReload( void );

	CNetworkVar( int, m_iReloadModelIndex );
	CNetworkVector( m_vInitialDropVelocity );

	virtual void FinishReload( void	) {}

public:
	#if defined( CLIENT_DLL )

		virtual void	ProcessMuzzleFlashEvent();
		virtual bool	ShouldPredict();


		virtual void	PostDataUpdate( DataUpdateType_t type );
		virtual void	OnDataChanged( DataUpdateType_t type );

		virtual bool	OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );

		virtual bool ShouldAutoEjectBrass( void );
		virtual bool GetEjectBrassShellType( void );

		void SetUseAltModel( bool bUseAlt );
		virtual int GetWorldModelIndex( void );
		virtual void CheckForAltWeapon( int iCurrentState );

		virtual Vector GetDesiredViewModelOffset( C_TFPlayer *pOwner );
		virtual float GetViewModelSwayScale( void ) { return 1.0; }

		virtual void OnWeaponDropped( void ) {}

		virtual bool ShouldDraw( void );

		virtual int		InternalDrawModel( int flags );

		float			m_flCrosshairDistance;
		int				m_iAmmoLastCheck;
		int				m_iAlpha;
		int				m_iScopeTextureID;

		bool			m_bUseAltWeaponModel;	//use alternate left handed world model? reset on new sequence
	#else

		virtual bool	Reload();
		virtual void	Spawn();

		void			SetDieThink( bool bDie );
		void			Die( void );

		void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	#endif

	virtual void OnPickedUp( CBaseCombatCharacter *pNewOwner );

	bool			IsUseable();
	virtual bool	CanDeploy( void );
	virtual bool	CanHolster( void );
	virtual bool	SendWeaponAnim( int iActivity );
	virtual void	Precache( void );
	virtual bool	CanBeSelected( void );
	virtual bool	DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt );
	virtual bool	Deploy();
	bool			PlayEmptySound();
	virtual void	ItemPostFrame();

	bool	m_bInAttack;		//True after a semi-auto weapon fires - will not fire a second time on the same button press

	void	SetExtraAmmoCount( int count ) { m_iExtraPrimaryAmmo = count; }
	int		GetExtraAmmoCount( void ) { return m_iExtraPrimaryAmmo; }

	virtual const char *GetSecondaryDeathNoticeName( void ) { return "world"; }

	virtual CBaseEntity *MeleeAttack( int iDamageAmount, int iDamageType, float flDmgDelay, float flAttackDelay );
	void EXPORT Smack( void );
	//Secondary Attacks
	void RifleButt( void );
	void Bayonet( void );
	void Punch( void );

	virtual Activity GetMeleeActivity( void ) { return ACT_VM_SECONDARYATTACK; }
	virtual Activity GetStrongMeleeActivity( void ) { return ACT_VM_SECONDARYATTACK; }

	virtual float GetRecoil( void ) { return 0.0f; }

	virtual int  GetWeaponTypeReference( void ) { return WEAPON_DOD; }

protected:
	CNetworkVar( float, m_flSmackTime );
	int m_iSmackDamage;
	int m_iSmackDamageType;
	EHANDLE m_pTraceHitEnt;
	trace_t m_trHit;

	int		m_iAltFireHint;

private:

	void EjectBrassLate();

	float	m_flDecreaseShotsFired;

	CWeaponDODBase( const CWeaponDODBase & );

	int		m_iExtraPrimaryAmmo;

#ifdef CLIENT_DLL
	int m_iCrosshairTexture;
#endif
};


#endif // WEAPON_DODBASE_H
