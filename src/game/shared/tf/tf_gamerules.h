//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: The TF Game rules object
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================

#ifndef TF_GAMERULES_H
#define TF_GAMERULES_H

#ifdef _WIN32
#pragma once
#endif


#include "teamplayroundbased_gamerules.h"
#include "convar.h"
#include "gamevars_shared.h"
#include "GameEventListener.h"
#include "tf_gamestats_shared.h"

#ifdef CLIENT_DLL
#include "c_tf_player.h"
#else
#include "tf_player.h"
#endif

#ifdef CLIENT_DLL
	
	#define CTFGameRules C_TFGameRules
	#define CTFGameRulesProxy C_TFGameRulesProxy

#else

	extern BOOL no_cease_fire_text;
	extern BOOL cease_fire;

	class CHealthKit;

#endif

extern ConVar	tf_spec_xray;
extern ConVar	tf_avoidteammates;
extern ConVar	tf_spawn_glows_duration;

extern Vector g_TFClassViewVectors[];

class CDODViewVectors : public CViewVectors
{
public:
	CDODViewVectors( 
		Vector vView,
		Vector vHullMin,
		Vector vHullMax,
		Vector vDuckHullMin,
		Vector vDuckHullMax,
		Vector vDuckView,
		Vector vObsHullMin,
		Vector vObsHullMax,
		Vector vDeadViewHeight,
		Vector vProneHullMin,
		Vector vProneHullMax ) :
			CViewVectors( 
				vView,
				vHullMin,
				vHullMax,
				vDuckHullMin,
				vDuckHullMax,
				vDuckView,
				vObsHullMin,
				vObsHullMax,
				vDeadViewHeight )
	{
		m_vProneHullMin = vProneHullMin;
		m_vProneHullMax = vProneHullMax;
	}

	Vector m_vProneHullMin;
	Vector m_vProneHullMax;	
};

class CTFGameRulesProxy : public CTeamplayRoundBasedRulesProxy
{
public:
	DECLARE_CLASS( CTFGameRulesProxy, CTeamplayRoundBasedRulesProxy );
	DECLARE_NETWORKCLASS();

#ifdef GAME_DLL
	DECLARE_DATADESC();
	void	InputSetRedTeamRespawnWaveTime( inputdata_t &inputdata );
	void	InputSetBlueTeamRespawnWaveTime( inputdata_t &inputdata );
	void	InputAddRedTeamRespawnWaveTime( inputdata_t &inputdata );
	void	InputAddBlueTeamRespawnWaveTime( inputdata_t &inputdata );
	void	InputSetRedTeamGoalString( inputdata_t &inputdata );
	void	InputSetBlueTeamGoalString( inputdata_t &inputdata );
	void	InputSetRedTeamRole( inputdata_t &inputdata );
	void	InputSetBlueTeamRole( inputdata_t &inputdata );

	virtual void Activate();
#endif
};

class CTFRadiusDamageInfo
{
	DECLARE_CLASS_NOBASE( CTFRadiusDamageInfo );
public:
	CTFRadiusDamageInfo( CTakeDamageInfo *pInfo, const Vector &vecSrcIn, float flRadiusIn, CBaseEntity *pIgnore = NULL, float flRJRadiusIn = 0, float flForceScaleIn = 1.0f )
	{
		dmgInfo = pInfo;
		vecSrc = vecSrcIn;
		flRadius = flRadiusIn;
		pEntityIgnore = pIgnore;
		flRJRadius = flRJRadiusIn;
		flFalloff = 0;
		m_flForceScale = flForceScaleIn;
		m_pEntityTarget = NULL;

		CalculateFalloff();
	}

	void CalculateFalloff( void );
	int ApplyToEntity( CBaseEntity *pEntity );

public:
	// Fill these in & call RadiusDamage()
	CTakeDamageInfo	*dmgInfo;
	Vector			vecSrc;
	float			flRadius;
	CBaseEntity		*pEntityIgnore;
	float			flRJRadius;	// Radius to use to calculate RJ, to maintain RJs when damage/radius changes on a RL
	float			m_flForceScale;
	CBaseEntity		*m_pEntityTarget;		// Target being direct hit if any
private:
	// These are used during the application of the RadiusDamage 
	float			flFalloff;
};

struct PlayerRoundScore_t
{
	int iPlayerIndex;	// player index
	int iRoundScore;	// how many points scored this round
	int	iTotalScore;	// total points scored across all rounds
};

#define MAX_TEAMGOAL_STRING		256

class CTFGameRules : public CTeamplayRoundBasedRules
{
public:
	DECLARE_CLASS( CTFGameRules, CTeamplayRoundBasedRules );

	CTFGameRules();

	// Damage Queries.
	virtual bool	Damage_IsTimeBased( int iDmgType );			// Damage types that are time-based.
	virtual bool	Damage_ShowOnHUD( int iDmgType );				// Damage types that have client HUD art.
	virtual bool	Damage_ShouldNotBleed( int iDmgType );			// Damage types that don't make the player bleed.
	// TEMP:
	virtual int		Damage_GetTimeBased( void );		
	virtual int		Damage_GetShowOnHud( void );
	virtual int		Damage_GetShouldNotBleed( void );

	int				GetFarthestOwnedControlPoint( int iTeam, bool bWithSpawnpoints );
	virtual bool	TeamMayCapturePoint( int iTeam, int iPointIndex );
	virtual bool	PlayerMayCapturePoint( CBasePlayer *pPlayer, int iPointIndex, char *pszReason = NULL, int iMaxReasonLength = 0 );
	virtual bool	PlayerMayBlockPoint( CBasePlayer *pPlayer, int iPointIndex, char *pszReason = NULL, int iMaxReasonLength = 0 );
	
	static int		CalcPlayerScore( RoundStats_t *pRoundStats );

	bool			IsBirthday( void );

	virtual const unsigned char *GetEncryptionKey( void ) { return (unsigned char *)"E2NcUkG2"; }

#ifdef GAME_DLL
public:
	// Override this to prevent removal of game specific entities that need to persist
	virtual bool	RoundCleanupShouldIgnore( CBaseEntity *pEnt );
	virtual bool	ShouldCreateEntity( const char *pszClassName );
	virtual void	CleanUpMap( void );

	virtual void	FrameUpdatePostEntityThink();

	// Called when a new round is being initialized
	virtual void	SetupOnRoundStart( void );

	// Called when a new round is off and running
	virtual void	SetupOnRoundRunning( void );

	// Called before a new round is started (so the previous round can end)
	virtual void	PreviousRoundEnd( void );

	// Send the team scores down to the client
	virtual void	SendTeamScoresEvent( void ) { return; }

	// Send the end of round info displayed in the win panel
	virtual void	SendWinPanelInfo( bool bGameOver ) OVERRIDE;

	// Setup spawn points for the current round before it starts
	virtual void	SetupSpawnPointsForRound( void );

	// Called when a round has entered stalemate mode (timer has run out)
	virtual void	SetupOnStalemateStart( void );
	virtual void	SetupOnStalemateEnd( void );

	void			RecalculateControlPointState( void );

	virtual void	HandleSwitchTeams( void );
	virtual void	HandleScrambleTeams( void );
	bool			CanChangeClassInStalemate( void );

	virtual void	SetRoundOverlayDetails( void );	
	virtual void	ShowRoundInfoPanel( CTFPlayer *pPlayer = NULL ); // NULL pPlayer means show the panel to everyone

	virtual bool	TimerMayExpire( void );

	virtual void	Activate();

	virtual bool	AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info );

	void			SetTeamGoalString( int iTeam, const char *pszGoal );

	// Speaking, vcds, voice commands.
	virtual void	InitCustomResponseRulesDicts();
	virtual void	ShutdownCustomResponseRulesDicts();

	virtual bool	HasPassedMinRespawnTime( CBasePlayer *pPlayer );

	bool			ShouldScorePerRound( void );

	bool			CanFlagBeCaptured( CBaseEntity *pOther );
	bool			PowerupModeFlagStandoffActive( void );

protected:
	virtual void	InitTeams( void );

	virtual void	RoundRespawn( void );

	virtual void	InternalHandleTeamWin( int iWinningTeam );
	
	static int		PlayerRoundScoreSortFunc( const PlayerRoundScore_t *pRoundScore1, const PlayerRoundScore_t *pRoundScore2 );

	virtual void FillOutTeamplayRoundWinEvent( IGameEvent *event );

	virtual bool CanChangelevelBecauseOfTimeLimit( void );
	virtual bool CanGoToStalemate( void );
#endif // GAME_DLL

public:
	// Return the value of this player towards capturing a point
	virtual int		GetCaptureValueForPlayer( CBasePlayer *pPlayer );

	// Collision and Damage rules.
	virtual bool	ShouldCollide( int collisionGroup0, int collisionGroup1 );
	
	int GetTimeLeft( void );

	// Get the view vectors for this mod.
	virtual const CViewVectors *GetViewVectors() const;
	virtual const CDODViewVectors *GetDODViewVectors() const;

	virtual void FireGameEvent( IGameEvent *event );

	virtual const char *GetGameTypeName( void ){ return g_aGameTypeNames[m_nGameType]; }
	virtual int GetGameType( void ){ return m_nGameType; }

	virtual bool FlagsMayBeCapped( void );

	void	RunPlayerConditionThink ( void );

	const char *GetTeamGoalString( int iTeam );

	virtual bool	IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer ) { return true; }

	// SEALTODO
	// ADD THESE
	bool IsTruceActive( void ) const { return false; }
	bool IsInMedievalMode( void ) const { return false; }
	bool IsMannVsMachineMode( void ) const { return false; }
	bool IsPlayingHybrid_CTF_CP( void ) const { return false; }
	bool IsPlayingSpecialDeliveryMode( void ) const { return false; }
	bool IsPowerupMode( void ) const { return false; }
	bool IsPlayingRobotDestructionMode( void ) const { return false; }
	bool IsQuickBuildTime( void ) { return false; }
	bool IsPVEModeActive( void ) { return false; }
	bool GameModeUsesUpgrades( void ) { return false; }

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes data tables able to access our private vars.

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	HandleOvertimeBegin();

	bool			ShouldShowTeamGoal( void );

	const char *GetVideoFileForMap( bool bWithExtension = true );

	virtual void	GetTeamGlowColor( int nTeam, float &r, float &g, float &b );

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes data tables able to access our private vars.
	
	virtual ~CTFGameRules();

	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	void RadiusStun( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius );
	virtual void Think();

	bool CheckTimeLimit();
	bool CheckWinLimit();
	bool CheckCapsPerRound();

	virtual bool FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker, const CTakeDamageInfo &info );

	// Spawing rules.
	CBaseEntity *GetPlayerSpawnSpot( CBasePlayer *pPlayer );
	bool IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer, bool bIgnorePlayers );

	virtual float FlItemRespawnTime( CItem *pItem );
	virtual Vector VecItemRespawnSpot( CItem *pItem );
	virtual QAngle VecItemRespawnAngles( CItem *pItem );

	virtual const char *GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer );
	void ClientSettingsChanged( CBasePlayer *pPlayer );
	void ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues );
	void ChangePlayerName( CTFPlayer *pPlayer, const char *pszNewName );

	virtual VoiceCommandMenuItem_t *VoiceCommand( CBaseMultiplayerPlayer *pPlayer, int iMenu, int iItem ); 

	bool IsInPreMatch() const;
	float GetPreMatchEndTime() const;	// Returns the time at which the prematch will be over.
	void GoToIntermission( void );

	virtual int GetAutoAimMode()	{ return AUTOAIM_NONE; }

	bool CanHaveAmmo( CBaseCombatCharacter *pPlayer, int iAmmoIndex );

	virtual const char *GetGameDescription( void ){ return "Team Fortress"; }

	// Sets up g_pPlayerResource.
	virtual void CreateStandardEntities();

	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info, const char* eventName );
	virtual CBasePlayer *GetDeathScorer( CBaseEntity *pKiller, CBaseEntity *pInflictor, CBaseEntity *pVictim );

	void CalcDominationAndRevenge( CTFPlayer *pAttacker, CTFPlayer *pVictim, bool bIsAssist, int *piDeathFlags );

	const char *GetKillingWeaponName( const CTakeDamageInfo &info, CTFPlayer *pVictim, int *iWeaponID );
	CBasePlayer *GetAssister( CBasePlayer *pVictim, CBasePlayer *pScorer, CBaseEntity *pInflictor );
	CTFPlayer *GetRecentDamager( CTFPlayer *pVictim, int iDamager, float flMaxElapsed );

	virtual void ClientDisconnected( edict_t *pClient );

	virtual void  RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore );
	void	RadiusDamage( CTFRadiusDamageInfo &info );

	bool ApplyOnDamageModifyRules( CTakeDamageInfo &info, CBaseEntity *pVictimBaseEntity, bool bAllowDamage );

	struct DamageModifyExtras_t
	{
		bool bIgniting;
		bool bSelfBlastDmg;
		bool bSendPreFeignDamage;
		bool bPlayDamageReductionSound;
	};
	float ApplyOnDamageAliveModifyRules( const CTakeDamageInfo &info, CBaseEntity *pVictimBaseEntity, DamageModifyExtras_t& outParams );

	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );

	virtual bool  FlPlayerFallDeathDoesScreenFade( CBasePlayer *pl ) { return false; }

	virtual bool UseSuicidePenalty() { return false; }

	int		GetPreviousRoundWinners( void ) { return m_iPreviousRoundWinners; }

	void	SendHudNotification( IRecipientFilter &filter, HudNotification_t iType );
	void	SendHudNotification( IRecipientFilter &filter, const char *pszText, const char *pszIcon, int iTeam = TEAM_UNASSIGNED );

	void	OnDispenserBuilt( CBaseEntity *dispenser );
	void	OnDispenserDestroyed( CBaseEntity *dispenser );

	const CUtlVector< CHandle< CBaseEntity > > &GetHealthEntityVector( void );		// return vector of health entities 
	const CUtlVector< CHandle< CBaseEntity > > &GetAmmoEntityVector( void );		// return vector of ammo entities 

private:

	int DefaultFOV( void ) { return 75; }

#endif

private:

#ifdef GAME_DLL

	Vector2D	m_vecPlayerPositions[MAX_PLAYERS];

	CUtlVector<CHandle<CHealthKit> > m_hDisabledHealthKits;	
	
	char	m_szMostRecentCappers[MAX_PLAYERS+1];	// list of players who made most recent capture.  Stored as string so it can be passed in events.
	int		m_iNumCaps[TF_TEAM_COUNT];				// # of captures ever by each team during a round

	int SetCurrentRoundStateBitString();
	void SetMiniRoundBitMask( int iMask );
	int m_iPrevRoundState;	// bit string representing the state of the points at the start of the previous miniround
	int m_iCurrentRoundState;
	int m_iCurrentMiniRoundMask;
	float m_flTimerMayExpireAt;

#endif

	CNetworkVar( int, m_nGameType ); // Type of game this map is (CTF, CP)
	CNetworkString( m_pszTeamGoalStringRed, MAX_TEAMGOAL_STRING );
	CNetworkString( m_pszTeamGoalStringBlue, MAX_TEAMGOAL_STRING );

	void ComputeHealthAndAmmoVectors( void );		// compute internal vectors of health and ammo locations
	bool m_areHealthAndAmmoVectorsReady;

public:

	bool m_bControlSpawnsPerTeam[ MAX_TEAMS ][ MAX_CONTROL_POINTS ];
	int	 m_iPreviousRoundWinners;

	int		m_iBirthdayMode;

#ifdef GAME_DLL
	// remove all projectiles in the world
	void RemoveAllProjectiles();

	// remove all buildings in the world
	void RemoveAllBuildings( bool bExplodeBuildings = false );

	// remove all sentry's ammo
	void RemoveAllSentriesAmmo();

	// remove all projectiles and objects
	void RemoveAllProjectilesAndBuildings( bool bExplodeBuildings = false );
#endif

	CUtlVector< CHandle< CBaseEntity > > m_ammoVector;			// vector of active ammo entities
	bool m_isAmmoVectorReady;									// for lazy evaluation

	CUtlVector< CHandle< CBaseEntity > > m_healthVector;		// vector of active health entities
	bool m_isHealthVectorReady;									// for lazy evaluation

public:
	//////////////////////////////// Day Of Defeat ////////////////////////////////
#ifdef CLIENT_DLL
#else
	virtual bool CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );

	virtual CBaseCombatWeapon *GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon );
#endif

protected:
	void CheckPlayerPositions( void );
};

//-----------------------------------------------------------------------------
// Gets us at the team fortress game rules
//-----------------------------------------------------------------------------

inline CTFGameRules* TFGameRules()
{
	return static_cast<CTFGameRules*>(g_pGameRules);
}

#ifdef GAME_DLL
	bool EntityPlacementTest( CBaseEntity *pMainEnt, const Vector &vOrigin, Vector &outPos, bool bDropToGround );
#endif

#endif // TF_GAMERULES_H
