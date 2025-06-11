//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//
//=============================================================================
#ifndef PC_WEAPONBASE_H
#define PC_WEAPONBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weapon_parse.h"
#include "tf_playeranimstate.h"
#include "npcevent.h"
#include "econ/ihasowner.h"

// Client specific.
#if defined( CLIENT_DLL )
#define CPCWeaponBase C_PCWeaponBase
#endif

class CTFPlayer;

// Used by CSS and DOD
extern Vector head_hull_mins;
extern Vector head_hull_maxs;

// Given an ammo type (like from a weapon's GetPrimaryAmmoType()), this compares it
// against the ammo name you specify.
// TFTODO: this should use indexing instead of searching and strcmp()'ing all the time.
bool IsAmmoType( int iAmmoType, const char *pAmmoName );
void FindHullIntersection( const Vector &vecSrc, trace_t &tr, const Vector &mins, const Vector &maxs, CBaseEntity *pEntity );

// Interface for weapons that have a charge time
class ITFChargeUpWeapon 
{
public:
	virtual bool CanCharge( void ) = 0;

	virtual float GetChargeBeginTime( void ) = 0;

	virtual float GetChargeMaxTime( void ) = 0;

	virtual float GetCurrentCharge( void )
	{ 
		return ( gpGlobals->curtime - GetChargeBeginTime() ) / GetChargeMaxTime();
	}
};

class CTraceFilterIgnoreTeammates : public CTraceFilterSimple
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS( CTraceFilterIgnoreTeammates, CTraceFilterSimple );

	CTraceFilterIgnoreTeammates( const IHandleEntity *passentity, int collisionGroup, int iIgnoreTeam )
		: CTraceFilterSimple( passentity, collisionGroup ), m_iIgnoreTeam( iIgnoreTeam )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );

		if ( ( pEntity->IsPlayer() || pEntity->IsCombatItem() ) && ( pEntity->GetTeamNumber() == m_iIgnoreTeam || m_iIgnoreTeam == TEAM_ANY ) )
		{
			return false;
		}

		return BaseClass::ShouldHitEntity( pServerEntity, contentsMask );
	}

	int m_iIgnoreTeam;
};

class CTraceFilterIgnorePlayers : public CTraceFilterSimple
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS( CTraceFilterIgnorePlayers, CTraceFilterSimple );

	CTraceFilterIgnorePlayers( const IHandleEntity *passentity, int collisionGroup )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );
		if ( pEntity && pEntity->IsPlayer() )
			return false;

		return BaseClass::ShouldHitEntity( pServerEntity, contentsMask );
	}
};

class CTraceFilterIgnoreFriendlyCombatItems : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterIgnoreFriendlyCombatItems, CTraceFilterSimple );

	CTraceFilterIgnoreFriendlyCombatItems( const IHandleEntity *passentity, int collisionGroup, int iIgnoreTeam, bool bIsProjectile = false )
		: CTraceFilterSimple( passentity, collisionGroup ), m_iIgnoreTeam( iIgnoreTeam )
	{
		m_bCallerIsProjectile = bIsProjectile;
	}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );

// 		if ( ( pEntity->MyCombatCharacterPointer() || pEntity->MyCombatWeaponPointer() ) && pEntity->GetTeamNumber() == m_iIgnoreTeam )
// 			return false;
// 
// 		if ( pEntity->IsPlayer() && pEntity->GetTeamNumber() == m_iIgnoreTeam )
// 			return false;

		if ( pEntity->IsCombatItem() )
		{
			if ( pEntity->GetTeamNumber() == m_iIgnoreTeam )
				return false;

			// If source is a enemy projectile, be explicit, otherwise we fail a "IsTransparent" test downstream
			if ( m_bCallerIsProjectile )
				return true;
		}

		return BaseClass::ShouldHitEntity( pServerEntity, contentsMask );
	}

	int m_iIgnoreTeam;
	bool m_bCallerIsProjectile;
};

//-----------------------------------------------------------------------------
// This filter checks against friendly players, buildings, shields
//-----------------------------------------------------------------------------
class CTraceFilterDeflection : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterDeflection, CTraceFilterSimple );
	
	CTraceFilterDeflection( const IHandleEntity *passentity, int collisionGroup, int iIgnoreTeam ) 
		: CTraceFilterSimple( passentity, collisionGroup ), m_iIgnoreTeam( iIgnoreTeam )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *passentity, int contentsMask ) OVERRIDE
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( passentity );
		if ( !pEntity )
			return false;

		if ( pEntity->IsPlayer() )
			return false;

		if ( pEntity->IsBaseObject() )
			return false;

		if ( pEntity->IsCombatItem() )
			return false;

		return BaseClass::ShouldHitEntity( passentity, contentsMask );
	}

	int m_iIgnoreTeam;
};

//=============================================================================
//
// Base PC Weapon Class
//
class CPCWeaponBase : public CBaseCombatWeapon, public IHasOwner
{
	DECLARE_CLASS( CPCWeaponBase, CBaseCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
#if !defined ( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	// Setup.
	CPCWeaponBase();
	~CPCWeaponBase();

	virtual void	Spawn();
	virtual bool	IsPredicted() const			{ return true; }

	// Weapon Data.
	CTFWeaponInfo const	&GetPCWpnData() const;
	virtual int GetWeaponID( void ) const;
	bool IsWeapon( int iWeapon ) const;

	// View model.
	virtual const char *GetViewModel( int iViewModel = 0 ) const;

	virtual CBaseEntity	*GetOwnerViaInterface( void ) { return GetOwner(); }

	// Utility.
	CBasePlayer		*GetPlayerOwner() const;
	CTFPlayer		*GetTFPlayerOwner() const;

	virtual int		GetWeaponTypeReference( void ) { return WEAPON_PC; }

	virtual bool	IsViewModelFlipped( void );

protected:

private:
	CPCWeaponBase( const CPCWeaponBase & );
};

#endif // PC_WEAPONBASE_H
