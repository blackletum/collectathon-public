//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_WEAPON_BUILDER_H
#define TF_WEAPON_BUILDER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase.h"

class CBaseObject;

//=========================================================
// Builder Weapon
//=========================================================
class CTFWeaponBuilder : public CTFWeaponBase
{
	DECLARE_CLASS( CTFWeaponBuilder, CTFWeaponBase );
public:
	CTFWeaponBuilder();
	~CTFWeaponBuilder();

	DECLARE_SERVERCLASS();

	virtual void	SetSubType( int iSubType );
	virtual void	SetObjectMode( int iMode ) { m_iObjectMode = iMode; }
	virtual void	Precache( void );
	virtual bool	CanDeploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual void	SecondaryAttack( void );
	virtual void	WeaponIdle( void );
	virtual bool	Deploy( void );	
	virtual Activity GetDrawActivity( void );
	virtual const char *GetViewModel( int iViewModel ) const;
	virtual const char *GetWorldModel( void ) const;

	virtual bool	AllowsAutoSwitchTo( void ) const;

	virtual int		GetType( void ) { return m_iObjectType; }

	void	SetCurrentState( int iState );
	void	SwitchOwnersWeaponToLast();

	// Placement
	void	StartPlacement( void );
	void	StopPlacement( void );
	void	UpdatePlacementState( void );		// do a check for valid placement
	bool	IsValidPlacement( void );			// is this a valid placement pos?

	// Building
	void	StartBuilding( void );

	// Selection
	bool	HasAmmo( void );
	int		GetSlot( void ) const;
	int		GetPosition( void ) const;
	const char *GetPrintName( void ) const;
	bool	CanBuildObjectType( int iObjectType );
	void	SetObjectTypeAsBuildable( int iObjectType );

	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_BUILDER; }

	virtual void	WeaponReset( void );

public:
	CNetworkVar( int, m_iBuildState );
	CNetworkVar( unsigned int, m_iObjectType );
	CNetworkVar( unsigned int, m_iObjectMode );
	CNetworkArray( bool, m_aBuildableObjectTypes, OBJ_LAST );

	CNetworkHandle( CBaseObject, m_hObjectBeingBuilt );

	int m_iValidBuildPoseParam;

	float m_flNextDenySound;

	EHANDLE     m_hLastSappedBuilding;
	Vector		m_vLastKnownSapPos;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTFWeaponSapper : public CTFWeaponBuilder
{
public:
	DECLARE_CLASS( CTFWeaponSapper, CTFWeaponBuilder );
	DECLARE_SERVERCLASS();
	//DECLARE_PREDICTABLE();

	CTFWeaponSapper();

	virtual void		ItemPostFrame( void );

	virtual const char *GetViewModel( int iViewModel ) const;
	virtual const char *GetWorldModel( void ) const;
};

#endif // TF_WEAPON_BUILDER_H
