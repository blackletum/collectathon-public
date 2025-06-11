//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DOD_VIEWMODEL_H
#define DOD_VIEWMODEL_H
#ifdef _WIN32
#pragma once
#endif

#include "predictable_entity.h"
#include "utlvector.h"
#include "baseplayer_shared.h"
#include "shared_classnames.h"

#include "tf_weaponbase.h"

#if defined( CLIENT_DLL )
#define CDODViewModel C_DODViewModel
#endif

class CDODViewModel : public CBaseViewModel
{
	DECLARE_CLASS( CDODViewModel, CBaseViewModel );
public:

	DECLARE_NETWORKCLASS();

	CDODViewModel( void );
	~CDODViewModel( void );

	virtual void CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& original_angles );
	virtual void CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles );

#if defined( CLIENT_DLL )
	virtual bool ShouldPredict( void )
	{
		if ( GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	BobState_t	&GetBobState() { return m_BobState; }
#endif

private:

#if defined( CLIENT_DLL )

	// This is used to lag the angles.
	CInterpolatedVar<QAngle> m_LagAnglesHistory;
	QAngle m_vLagAngles;

	CDODViewModel( const CDODViewModel & ); // not defined, not accessible

	QAngle m_vLoweredWeaponOffset;

	BobState_t		m_BobState;		// view model head bob state
#endif
};

#endif // DOD_VIEWMODEL_H
