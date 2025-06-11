//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "gamemovement.h"
#include "tf_gamerules.h"
#include "tf_shareddefs.h"
#include "in_buttons.h"
#include "movevars_shared.h"
#include "collisionutils.h"
#include "debugoverlay_shared.h"
#include "baseobject_shared.h"
#include "particle_parse.h"
#include "baseobject_shared.h"
#include "coordsize.h"

#ifdef CLIENT_DLL
	#include "c_tf_player.h"
	#include "c_world.h"
	#include "c_team.h"

	#define CTeam C_Team

#else
	#include "tf_player.h"
	#include "team.h"
#endif

class CTFGameMovement : public CGameMovement
{
public:
	DECLARE_CLASS( CTFGameMovement, CGameMovement );

	CTFGameMovement( CTFPlayer *pTFPlayer );

	virtual void PlayerMove();
	virtual unsigned int PlayerSolidMask( bool brushOnly = false );
	virtual void ProcessMovement( CBasePlayer *pBasePlayer, CMoveData *pMove );
	virtual bool CanAccelerate();
	virtual bool CheckJumpButton();
	virtual bool CheckWater( void );
	virtual void WaterMove( void );
	virtual void FullWalkMove();
	virtual void WalkMove( void );
	virtual void AirMove( void );
	virtual void FullTossMove( void );
	virtual void CategorizePosition( void );
	virtual void CheckFalling( void );
	virtual void Duck( void );
	virtual Vector GetPlayerViewOffset( bool ducked ) const;

	virtual void	TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm );
	virtual CBaseHandle	TestPlayerPosition( const Vector& pos, int collisionGroup, trace_t& pm );
	virtual void	StepMove( Vector &vecDestination, trace_t &trace );
	virtual bool	GameHasLadders() const;
	virtual void SetGroundEntity( trace_t *pm );
	virtual void PlayerRoughLandingEffects( float fvol );

	virtual void HandleDuckingSpeedCrop( void );
protected:

	virtual void CheckWaterJump(void );
	void		 FullWalkMoveUnderwater();

private:

	bool		CheckWaterJumpButton( void );
	void		AirDash( void );
	void		PreventBunnyJumping();

	// Ducking.
	void DuckOverrides();
	void OnDuck( int nButtonsPressed );
	void OnUnDuck( int nButtonsReleased );

private:

	Vector		m_vecWaterPoint;
	CTFPlayer  *m_pTFPlayer;
};