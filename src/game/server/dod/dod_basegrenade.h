//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_BASEGRENADE_H
#define DOD_BASEGRENADE_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_dodbase.h"
#include "basegrenade_shared.h"

class CGrenadeTrail;

class CGrenadeController : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();
public:
	virtual simresult_e	Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
};

class CDODBaseGrenade : public CBaseGrenade
{
public:
	DECLARE_CLASS( CDODBaseGrenade, CBaseGrenade );

	DECLARE_DATADESC();

	DECLARE_SERVERCLASS();

	CDODBaseGrenade();
	virtual ~CDODBaseGrenade();

	virtual void Spawn();
	virtual void Precache( void );

	static inline float GetGrenadeGravity() { return 0.4f; }
	static inline const float GetGrenadeFriction() { return 0.5f; }
	static inline const float GetGrenadeElasticity() { return 0.2f; }

	void DetonateThink( void );

	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual char *GetExplodingClassname( void );

	virtual int	    ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE | FCAP_USE_IN_RADIUS; }

	virtual	int		OnTakeDamage( const CTakeDamageInfo &info );

	virtual void	Detonate();

	bool			CreateVPhysics();
	virtual void	VPhysicsUpdate( IPhysicsObject *pPhysics );
	virtual void VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );

	virtual void Explode( trace_t *pTrace, int bitsDamageType );

	void SetupInitialTransmittedGrenadeVelocity( const Vector &velocity )
	{
		m_vInitialVelocity = velocity;
	}

	CGrenadeController			m_GrenadeController;
	IPhysicsMotionController	*m_pMotionController;

	virtual bool CanBePickedUp( void ) { return true; }

	virtual void OnPickedUp( void ) {}

	virtual float GetElasticity();

	virtual int GetEmitterWeaponID() { return m_EmitterWeaponID; }

	virtual bool	IsDeflectable( void ) { return true; }
	virtual void	Deflected( CBaseEntity *pDeflectedBy, Vector& vecDir );

	virtual void		IncrementDeflected( void ) { m_iDeflected++; }
	void				ResetDeflected( void ) { m_iDeflected = 0; }
	int					GetDeflected( void ) { return m_iDeflected; }

protected:

	//Set the time to detonate ( now + timer )
	virtual void SetDetonateTimerLength( float timer );
	float m_flDetonateTime;

	//Custom collision to allow for constant elasticity on hit surfaces
	virtual void ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity );

	bool	m_bUseVPhysics;

	int m_EmitterWeaponID;

private:		

	CDODBaseGrenade( const CDODBaseGrenade & );

	bool	m_bInSolid;

	// This gets sent to the client and placed in the client's interpolation history
	// so the projectile starts out moving right off the bat.
	CNetworkVector( m_vInitialVelocity );

	float m_flCollideWithTeammatesTime;
	bool m_bCollideWithTeammates;

	CNetworkVar( int,	m_iDeflected );
};


#endif // DOD_BASEGRENADE_H
