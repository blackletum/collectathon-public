//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_CSBASE_H
#define WEAPON_CSBASE_H
#ifdef _WIN32
#pragma once
#endif

#if defined( CLIENT_DLL )
#include "c_tf_player.h"
#else
#include "tf_player.h"
#endif

//#include "cs_playeranimstate.h"
//#include "cs_weapon_parse.h"
#include "tf_weapon_parse.h"

extern CTFWeaponInfo *GetCSWeaponInfo( int iWeapon );


#if defined( CLIENT_DLL )
	#define CWeaponCSBase C_WeaponCSBase
#endif

extern const char *GetTranslatedWeaponAlias( const char *alias);
extern const char * GetWeaponAliasFromTranslated(const char *translatedAlias);
extern bool	IsPrimaryWeapon( int id );
extern bool IsSecondaryWeapon( int  id );
extern int GetShellForAmmoType( const char *ammoname );

#define SHIELD_VIEW_MODEL "models/weapons/v_shield.mdl"
#define SHIELD_WORLD_MODEL "models/weapons/w_shield.mdl"

#define CROSSHAIR_CONTRACT_PIXELS_PER_SECOND	7.0f

#if defined( CLIENT_DLL )

	//--------------------------------------------------------------------------------------------------------------
	/**
	*  Returns the client's ID_* value for the currently owned weapon, or ID_NONE if no weapon is owned
	*/
	int GetClientWeaponID( bool primary );

#endif


class CWeaponCSBase : public CPCWeaponBase
{
public:
	DECLARE_CLASS( CWeaponCSBase, CPCWeaponBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponCSBase();

	#ifdef GAME_DLL
		DECLARE_DATADESC();

		virtual void CheckRespawn();
		virtual CBaseEntity* Respawn();
		
		virtual const Vector& GetBulletSpread();
		virtual float	GetDefaultAnimSpeed();

		virtual void	BulletWasFired( const Vector &vecStart, const Vector &vecEnd );
		virtual bool	ShouldRemoveOnRoundRestart();

        //=============================================================================
        // HPE_BEGIN:
        // [dwenger] Handle round restart processing for the weapon.
        //=============================================================================

        virtual void    OnRoundRestart();

        //=============================================================================
        // HPE_END
        //=============================================================================

        virtual bool	DefaultReload( int iClipSize1, int iClipSize2, int iActivity );

		void SendReloadEvents();

		void Materialize();
		void AttemptToMaterialize();
		virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

		virtual bool IsRemoveable();
		
	#endif

	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );

	// Pistols reset m_iShotsFired to 0 when the attack button is released.
	bool			IsPistol() const;

	virtual bool IsFullAuto() const;

	virtual float GetMaxSpeed() const;	// What's the player's max speed while holding this weapon.

	// return true if this weapon is a kinf of the given weapon type (ie: "IsKindOf" WEAPONTYPE_RIFLE )
	bool IsKindOf( CSWeaponType type ) const			{ return GetPCWpnData().m_CSWeaponData.m_WeaponType == type; }

	// return true if this weapon has a silencer equipped
	virtual bool IsSilenced( void ) const				{ return false; }

	virtual void SetWeaponModelIndex( const char *pName );
	virtual void OnPickedUp( CBaseCombatCharacter *pNewOwner );

	virtual void OnJump( float fImpulse );
	virtual void OnLand( float fVelocity );

public:
	#if defined( CLIENT_DLL )

		virtual void	ProcessMuzzleFlashEvent();
		virtual bool	OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
		virtual bool	ShouldPredict();
		// SEALTODO
//		virtual void	DrawCrosshair();
		virtual void	OnDataChanged( DataUpdateType_t type );

		virtual int		GetMuzzleAttachment( void );
		virtual bool	HideViewModelWhenZoomed( void ) { return true; }

		float			m_flCrosshairDistance;
		int				m_iAmmoLastCheck;
		int				m_iAlpha;
		int				m_iScopeTextureID;
		int				m_iCrosshairTextureID; // for white additive texture

		virtual int GetMuzzleFlashStyle( void );

	#else

		virtual bool	Reload();
		virtual void	Spawn();
		virtual bool	KeyValue( const char *szKeyName, const char *szValue );

		virtual bool PhysicsSplash( const Vector &centerPoint, const Vector &normal, float rawSpeed, float scaledSpeed );

	#endif

	bool IsUseable();
	virtual bool	CanDeploy( void );
	virtual void	UpdateShieldState( void );
	virtual bool	SendWeaponAnim( int iActivity );
	virtual void	SecondaryAttack( void );
	virtual void	Precache( void );
	virtual bool	CanBeSelected( void );
	virtual Activity GetDeployActivity( void );
	virtual bool	DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt );
	virtual void 	DefaultTouch( CBaseEntity *pOther );	// default weapon touch
	virtual bool	DefaultPistolReload();

	virtual bool	Deploy();
	virtual void	Drop( const Vector &vecVelocity );
	bool PlayEmptySound();
	virtual void	ItemPostFrame();
	virtual void	ItemBusyFrame();


	bool	m_bDelayFire;			// This variable is used to delay the time between subsequent button pressing.
	float	m_flAccuracy;

	//=============================================================================
	// HPE_BEGIN:
	// [pfreese] new accuracy model
	//=============================================================================

	CNetworkVar( int, m_weaponMode );

	virtual float GetInaccuracy() const;
	virtual float GetSpread() const;

	virtual void UpdateAccuracyPenalty();

	CNetworkVar( float, m_fAccuracyPenalty );

	//=============================================================================
	// HPE_END
	//=============================================================================
	
	void SetExtraAmmoCount( int count ) { m_iExtraPrimaryAmmo = count; }
	int GetExtraAmmoCount( void ) { return m_iExtraPrimaryAmmo; }

	//=============================================================================
	// HPE_BEGIN:	
	//=============================================================================

    // [tj] Accessors for the previous owner of the gun
	void SetPreviousOwner(CTFPlayer* player) { m_prevOwner = player; }
	CTFPlayer* GetPreviousOwner() { return m_prevOwner; }

    // [tj] Accessors for the donor system
    void SetDonor(CTFPlayer* player) { m_donor = player; }
    CTFPlayer* GetDonor() { return m_donor; }
    void SetDonated(bool donated) { m_donated = true;}
    bool GetDonated() { return m_donated; }

    //[dwenger] Accessors for the prior owner list
    void AddToPriorOwnerList(CTFPlayer* pPlayer);
    bool IsAPriorOwner(CTFPlayer* pPlayer);

	//=============================================================================
	// HPE_END
	//=============================================================================

	virtual int  GetWeaponTypeReference( void ) { return WEAPON_CS; }

protected:

	float	CalculateNextAttackTime( float flCycleTime );

private:

	float	m_flDecreaseShotsFired;

	CWeaponCSBase( const CWeaponCSBase & );

	int		m_iExtraPrimaryAmmo;

	float	m_nextPrevOwnerTouchTime;
	CTFPlayer *m_prevOwner;

	int m_iDefaultExtraAmmo;

    //=============================================================================
    // HPE_BEGIN:
    //=============================================================================

    // [dwenger] track all prior owners of this weapon
    CUtlVector< CTFPlayer* >    m_PriorOwners;

    // [tj] To keep track of people who drop weapons for teammates during the buy round
    CHandle<CTFPlayer> m_donor;
    bool m_donated;

    //=============================================================================
    // HPE_END
    //=============================================================================
};

extern ConVar weapon_accuracy_model;

#endif // WEAPON_CSBASE_H
