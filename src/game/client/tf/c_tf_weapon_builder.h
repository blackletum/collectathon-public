//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_WEAPON_BUILDER_H
#define C_TF_WEAPON_BUILDER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase.h"
#include "c_baseobject.h"

#define CTFWeaponBuilder C_TFWeaponBuilder
#define CTFWeaponSapper C_TFWeaponSapper

//=============================================================================
// Purpose: Client version of CWeaponBuiler
//=============================================================================
class C_TFWeaponBuilder : public C_TFWeaponBase
{
	DECLARE_CLASS( C_TFWeaponBuilder, C_TFWeaponBase );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_TFWeaponBuilder();
	~C_TFWeaponBuilder();

	virtual void Redraw();

	virtual void SecondaryAttack();

	virtual bool	IsPlacingObject( void );

	virtual const char *GetCurrentSelectionObjectName( void );

	virtual const char *GetViewModel( int iViewModel ) const;
	virtual const char *GetWorldModel( void ) const;

	virtual bool Deploy( void );

	C_BaseObject	*GetPlacementModel( void ) { return m_hObjectBeingBuilt.Get(); }

	virtual int GetSlot( void ) const;
	virtual int GetPosition( void ) const;

	void SetupObjectSelectionSprite( void );

	virtual CHudTexture const *GetSpriteActive( void ) const;
	virtual CHudTexture const *GetSpriteInactive( void ) const;

	virtual const char *GetPrintName( void ) const;

	virtual int	GetSubType( void );

	virtual bool CanBeSelected( void );
	virtual bool VisibleInWeaponSelection( void );

	virtual bool HasAmmo( void );

	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_BUILDER; }

	int GetType( void ) { return m_iObjectType; }
	bool CanBuildObjectType( int iObjectType );

	virtual Activity GetDrawActivity( void );

	virtual CStudioHdr *OnNewModel( void );

public:
	// Builder Data
	int			m_iBuildState;
	unsigned int m_iObjectType;
	unsigned int m_iObjectMode;
	float		m_flStartTime;
	float		m_flTotalTime;

	CHudTexture *m_pSelectionTextureActive;
	CHudTexture *m_pSelectionTextureInactive;

	// Our placement model
	CHandle<C_BaseObject>	m_hObjectBeingBuilt;

	int m_iValidBuildPoseParam;

private:
	C_TFWeaponBuilder( const C_TFWeaponBuilder & );
	bool m_aBuildableObjectTypes[OBJ_LAST];
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_TFWeaponSapper : public C_TFWeaponBuilder
{
	DECLARE_CLASS( C_TFWeaponSapper, C_TFWeaponBuilder );
public:
	DECLARE_NETWORKCLASS();
	//DECLARE_PREDICTABLE();

	//virtual const char *GetViewModel( int iViewModel ) const;
	//virtual const char *GetWorldModel( void ) const;
};

#endif // C_TF_WEAPON_BUILDER_H
