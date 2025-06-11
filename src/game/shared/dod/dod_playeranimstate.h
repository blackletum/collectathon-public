//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_PLAYERANIMSTATE_H
#define DOD_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif


#include "convar.h"
#include "iplayeranimstate.h"
#include "multiplayer_animstate.h"

#if defined( CLIENT_DLL )
	class C_TFPlayer;
	#define CTFPlayer C_TFPlayer
#else
	class CTFPlayer;
#endif

// If this is set, then the game code needs to make sure to send player animation events
// to the local player if he's the one being watched.
extern ConVar cl_showanimstate;

class CDODPlayerAnimState : public CMultiPlayerAnimState
{
public:

	DECLARE_CLASS( CDODPlayerAnimState, CMultiPlayerAnimState );

	CDODPlayerAnimState();
	CDODPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData );
	~CDODPlayerAnimState();

	// This is called by both the client and the server in the same way to trigger events for
	// players firing, jumping, throwing grenades, etc.
	virtual void DoAnimationEvent( PlayerAnimEvent_t event, int nData );
	virtual void ClearAnimationState();
	virtual Activity CalcMainActivity();	
	virtual void Update( float eyeYaw, float eyePitch );

	virtual int CalcAimLayerSequence( float *flCyle, float *flAimSequenceWeight, bool bForceIdle ) { return 0; }

	virtual float GetCurrentMaxGroundSpeed();
	virtual void ComputeSequences( CStudioHdr *pStudioHdr );
//	virtual void ClearAnimationLayers();

	virtual void RestartMainSequence();
	virtual float CalcMovementPlaybackRate( bool *bIsMoving );

	Activity TranslateActivity( Activity actDesired );
	void CancelGestures( void );

	void InitDOD( CTFPlayer *pPlayer );

protected:

	// Pose paramters.
	virtual bool		SetupPoseParameters( CStudioHdr *pStudioHdr );
	virtual void		ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr );
	virtual void		ComputePoseParam_AimPitch( CStudioHdr *pStudioHdr );
	virtual void		ComputePoseParam_AimYaw( CStudioHdr *pStudioHdr );
	void				ComputePoseParam_BodyHeight( CStudioHdr *pStudioHdr );
	virtual void		EstimateYaw( void );
	void				ConvergeYawAngles( float flGoalYaw, float flYawRate, float flDeltaTime, float &flCurrentYaw );

	void ComputeFireSequence();
	void ComputeDeployedSequence();

//	void ComputeGestureSequence( CStudioHdr *pStudioHdr );

//	void RestartGesture( int iGestureType, Activity act, bool bAutoKill = true );

	void UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd, float flWeight = 1.0 );

private:	
	bool HandleJumping( Activity *idealActivity );
	bool HandleProne( Activity *idealActivity );
	bool HandleProneDown( CTFPlayer *pPlayer, Activity *idealActivity );
	bool HandleProneUp( CTFPlayer *pPlayer, Activity *idealActivity );
	bool HandleDucked( Activity *idealActivity );

	bool IsGettingDown( CTFPlayer *pPlayer );
	bool IsGettingUp( CTFPlayer *pPlayer );

	CTFPlayer* GetOuterDOD() const;

	//bool IsPlayingGesture( int type )
	//{
	//	return ( m_bPlayingGesture && m_iGestureType == type );
	//}

private:
	// Current state variables.
	bool m_bJumping;			// Set on a jump event.
	float m_flJumpStartTime;	
	bool m_bFirstJumpFrame;

	// These control the prone state _achine.
	bool m_bGettingDown;
	bool m_bGettingUp;
	bool m_bWasGoingProne;
	bool m_bWasGettingUp;

	// The single Gesture layer
	//bool m_bPlayingGesture;
	//bool m_bAutokillGesture;
	//int m_iGestureSequence;
	//float m_flGestureCycle;

	//int m_iGestureType;

	// Pose parameters.
	float		m_flEstimateVelocity;
	float		m_flLastBodyHeight;
	Vector2D	m_vecLastMoveYaw;

	float m_flFireIdleTime;				// Time that we drop our gun

	bool m_bLastDeployState;		// true = last was deployed, false = last was not deployed

	int m_iLastWeaponID;			// remember the weapon we were last using

	// Our DOD player pointer.
	CTFPlayer *m_pOuterDOD;
};

CDODPlayerAnimState* CreateDODPlayerAnimState( CTFPlayer *pPlayer );

#endif // DOD_PLAYERANIMSTATE_H
