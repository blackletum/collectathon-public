//========= Copyright � 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================
#ifndef TF_PLAYERANIMSTATE_H
#define TF_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif

#include "convar.h"
#include "multiplayer_animstate.h"

#if defined( CLIENT_DLL )
class C_TFPlayer;
#define CTFPlayer C_TFPlayer
#else
class CTFPlayer;
#endif

// ------------------------------------------------------------------------------------------------ //
// CPlayerAnimState declaration.
// ------------------------------------------------------------------------------------------------ //
class CTFPlayerAnimState : public CMultiPlayerAnimState
{
public:
	
	DECLARE_CLASS( CTFPlayerAnimState, CMultiPlayerAnimState );

	CTFPlayerAnimState();
	CTFPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData );
	~CTFPlayerAnimState();

	void InitTF( CTFPlayer *pPlayer );
	CTFPlayer *GetTFPlayer( void )							{ return m_pTFPlayer; }

	virtual void ClearAnimationState();
	virtual Activity TranslateActivity( Activity actDesired );
	Activity ActivityOverride( Activity baseAct, bool *pRequired );
	virtual void Update( float eyeYaw, float eyePitch );

	void	DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );
	virtual void CheckStunAnimation();
	virtual Activity CalcMainActivity();

	virtual float GetCurrentMaxGroundSpeed();
	virtual float GetGesturePlaybackRate( void );

	bool	HandleMoving( Activity &idealActivity );
	bool	HandleJumping( Activity &idealActivity );
	bool	HandleDucking( Activity &idealActivity );
	bool	HandleSwimming( Activity &idealActivity );


private:
	void Taunt_ComputePoseParam_MoveX( CStudioHdr *pStudioHdr );
	void Taunt_ComputePoseParam_MoveY( CStudioHdr *pStudioHdr );

	CTFPlayer   *m_pTFPlayer;
	bool		m_bInAirWalk;
	float		m_flHoldDeployedPoseUntilTime;
	float		m_flTauntMoveX;
	float		m_flTauntMoveY;
	Vector		m_vecSmoothedUp;
};

CTFPlayerAnimState *CreateTFPlayerAnimState( CTFPlayer *pPlayer );

#endif // TF_PLAYERANIMSTATE_H
