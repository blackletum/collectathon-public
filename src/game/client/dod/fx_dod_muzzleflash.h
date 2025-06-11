//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "particles_simple.h"
#include "particles_localspace.h"
#include "fx.h"
#include "c_te_effect_dispatch.h"

extern ConVar cl_muzzleflash_dlight_1st;

class CDODMuzzleFlashEmitter_1stPerson : public CLocalSpaceEmitter
{
public:
	DECLARE_CLASS( CDODMuzzleFlashEmitter_1stPerson, CLocalSpaceEmitter );

	static CSmartPtr<CDODMuzzleFlashEmitter_1stPerson> Create( const char *pDebugName, int entIndex, int nAttachment, int fFlags = 0 );

	virtual void Update( float t ) { BaseClass::Update(t); }

protected:
	CDODMuzzleFlashEmitter_1stPerson( const char *pDebugName );

private:
	CDODMuzzleFlashEmitter_1stPerson( const CDODMuzzleFlashEmitter_1stPerson & );

	int m_iMuzzleFlashType;
};