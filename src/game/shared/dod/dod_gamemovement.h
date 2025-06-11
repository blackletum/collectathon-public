//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "gamemovement.h"
#include "tf_gamerules.h"
#include "dod_shareddefs.h"
#include "in_buttons.h"
#include "movevars_shared.h"

#include "weapon_dodsniper.h"
#include "weapon_dodbaserpg.h"
#include "weapon_dodsemiauto.h"


#ifdef CLIENT_DLL
	#include "c_tf_player.h"
#else
	#include "tf_player.h"
#endif

extern bool g_bMovementOptimizations;

class CDODGameMovement : public CGameMovement
{
public:
	DECLARE_CLASS( CDODGameMovement, CGameMovement );

	CDODGameMovement( CTFPlayer *pTFPlayer );
	virtual ~CDODGameMovement();

	void SetPlayerSpeed( void );

	virtual void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );
	virtual bool CanAccelerate();
	virtual bool CheckJumpButton( void );
	virtual void ReduceTimers( void );
	virtual void WalkMove( void );
	virtual void AirMove( void );
	virtual void CheckParameters( void );
	virtual void CheckFalling( void );

	// Ducking
	virtual void Duck( void );
	virtual void FinishUnDuck( void );
	virtual void FinishDuck( void );
	virtual void HandleDuckingSpeedCrop();
	void SetDODDuckedEyeOffset( float duckFraction );
	void SetDeployedEyeOffset( void );

	// Prone
	void SetProneEyeOffset( float proneFraction );
	void FinishProne( void );
	void FinishUnProne( void );
	bool CanUnprone();

	virtual Vector	GetPlayerMins( void ) const; // uses local player
	virtual Vector	GetPlayerMaxs( void ) const; // uses local player

	// IGameMovement interface
	virtual Vector	GetPlayerMins( bool ducked ) const { return BaseClass::GetPlayerMins(ducked); }
	virtual Vector	GetPlayerMaxs( bool ducked ) const { return BaseClass::GetPlayerMaxs(ducked); }
	virtual Vector	GetPlayerViewOffset( bool ducked ) const { return ducked ? VEC_DUCK_VIEW_SCALED_DOD( player ) : VEC_VIEW_SCALED_DOD( player );; }

	void ViewOffsetAnimation( Vector vecDest, float flTime, ViewAnimationType type );

	void SetViewOffset( Vector vecViewOffset );

	virtual unsigned int PlayerSolidMask( bool brushOnly = false );

protected:
	virtual void PlayerMove();

	void CheckForLadders( bool wasOnGround );
	bool ResolveStanding( void );
	void TracePlayerBBoxWithStep( const Vector &vStart, const Vector &vEnd, unsigned int fMask, int collisionGroup, trace_t &trace );

public:
	CTFPlayer *m_pDODPlayer;
	bool m_bUnProneToDuck;
};