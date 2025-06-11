//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_BASEROCKET_H
#define DOD_BASEROCKET_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "baseanimating.h"
#include "smoke_trail.h"
#include "weapon_dodbase.h"

class RocketTrail;
 
//================================================
// CDODBaseRocket	
//================================================
class CDODBaseRocket : public CBaseAnimating
{
	DECLARE_CLASS( CDODBaseRocket, CBaseAnimating );

public:
	CDODBaseRocket();
	~CDODBaseRocket();
	
	void	Spawn( void );
	void	Precache( void );
	void	RocketTouch( CBaseEntity *pOther );
	void	Explode( void );
	void	Fire( void );
	
	virtual float	GetDamage() { return m_flDamage; }
	virtual void	SetDamage(float flDamage) { m_flDamage = flDamage; }

	unsigned int PhysicsSolidMaskForEntity( void ) const;

	static CDODBaseRocket *Create( const char *szClassname, const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner );	

	void SetupInitialTransmittedGrenadeVelocity( const Vector &velocity );

	virtual int GetEmitterWeaponID() { return TF_WEAPON_NONE; Assert(0); }

	virtual bool	IsDeflectable( void ) { return true; }
	virtual void	Deflected( CBaseEntity *pDeflectedBy, Vector& vecDir );

	virtual void	IncrementDeflected( void ) { m_iDeflected++; }
	void			ResetDeflected( void ) { m_iDeflected = 0; }
	int				GetDeflected( void ) { return m_iDeflected; }

protected:
	virtual void DoExplosion( trace_t *pTrace );

	void FlyThink( void );

	float					m_flDamage;

	CNetworkVector( m_vInitialVelocity );
	CNetworkVar( int, m_iDeflected );

private:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	float m_flCollideWithTeammatesTime;
	bool m_bCollideWithTeammates;
};

#endif // DOD_BASEROCKET_H