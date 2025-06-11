//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Weapon Knife Class
//
//=============================================================================
#ifndef TF_WEAPON_KNIFE_H
#define TF_WEAPON_KNIFE_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_melee.h"

#ifdef CLIENT_DLL
#define CTFKnife C_TFKnife
#endif

//=============================================================================
//
// Knife class.
//
class CTFKnife : public CTFWeaponBaseMelee
{
public:

	DECLARE_CLASS( CTFKnife, CTFWeaponBaseMelee );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CTFKnife();
	virtual void		PrimaryAttack( void );
	virtual int			GetWeaponID( void ) const			{ return TF_WEAPON_KNIFE; }

	virtual float		GetMeleeDamage( CBaseEntity *pTarget, int* piCustomDamage );

	virtual void		SendPlayerAnimEvent( CTFPlayer *pPlayer );

	virtual bool		Deploy( void ) OVERRIDE;

	void				BackstabVMThink( void );

	bool				SendWeaponAnim( int iActivity );

	bool				CanPerformBackstabAgainstTarget( CTFPlayer *pTarget );		// "backstab" sometimes means "frontstab"
	bool				IsBehindAndFacingTarget( CTFPlayer *pTarget );
	bool				IsBackstab( void ) { return (m_hBackstabVictim.Get() != NULL); }
	void				ProcessDisguiseImpulse();

	virtual void		ItemPostFrame( void ) OVERRIDE;
	virtual void		ItemPreFrame( void ) OVERRIDE;
	virtual void		ItemBusyFrame( void ) OVERRIDE;
	virtual void		ItemHolsterFrame( void ) OVERRIDE;

	virtual bool		CalcIsAttackCriticalHelper( void ) OVERRIDE;
	virtual bool		CalcIsAttackCriticalHelperNoCrits( void ) OVERRIDE;
	virtual bool		DoSwingTrace( trace_t &trace ) OVERRIDE;

private:
	CHandle<CTFPlayer>	m_hBackstabVictim;
	CNetworkVar( bool,	m_bReadyToBackstab );

	bool m_bWasTaunting;

	CTFKnife( const CTFKnife & ) {}
};

#endif // TF_WEAPON_KNIFE_H
