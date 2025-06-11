//========= Copyright © 1996-2005, Valve LLC, All rights reserved. ============
//
//=============================================================================
#ifndef TF_PLAYER_H
#define TF_PLAYER_H
#pragma once

#include "basemultiplayerplayer.h"
#include "server_class.h"
#include "tf_playeranimstate.h"
#include "tf_shareddefs.h"
#include "tf_player_shared.h"
#include "tf_playerclass.h"
#include "entity_tfstart.h"
#include "tf_weapon_medigun.h"

//DOD
#include "dod_shareddefs.h"

class CTFPlayer;
class CTFTeam;
class CTFGoal;
class CTFGoalItem;
class CTFItem;
class CTFWeaponBuilder;
//class CBaseObject;
class CTFWeaponBase;
class CIntroViewpoint;
class CTriggerAreaCapture;
class CTFWeaponBaseGun;
class CCaptureZone;

//=============================================================================
//
// Player State Information
//
class CPlayerStateInfo
{
public:

	int				m_nPlayerState;
	const char		*m_pStateName;

	// Enter/Leave state.
	void ( CTFPlayer::*pfnEnterState )();	
	void ( CTFPlayer::*pfnLeaveState )();

	// Think (called every frame).
	void ( CTFPlayer::*pfnThink )();
};

struct DamagerHistory_t
{
	DamagerHistory_t()
	{
		Reset();
	}
	void Reset()
	{
		hDamager = NULL;
		flTimeDamage = 0;
	}
	EHANDLE hDamager;
	float	flTimeDamage;
};
#define MAX_DAMAGER_HISTORY 2

//=============================================================================
//
// TF Player
//
class CTFPlayer : public CBaseMultiplayerPlayer
{
public:
	DECLARE_CLASS( CTFPlayer, CBaseMultiplayerPlayer );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CTFPlayer();
	~CTFPlayer();

	// Creation/Destruction.
	static CTFPlayer	*CreatePlayer( const char *className, edict_t *ed );
	static CTFPlayer	*Instance( int iEnt );

	virtual void		Spawn();
	virtual void		ForceRespawn();
	int					TranslateClassIndex( int iClass, int iTeam );
	virtual CBaseEntity	*EntSelectSpawnPoint( void );
	virtual void		InitialSpawn();
	virtual void		Precache();
	virtual bool		IsReadyToPlay( void );
	virtual bool		IsReadyToSpawn( void );
	virtual bool		ShouldGainInstantSpawn( void );
	virtual void		ResetScores( void );

	void				DestroyViewModel( int iViewModel = 0 );
	void				CreateViewModel( int iViewModel = 0 );
	CBaseViewModel		*GetOffHandViewModel();
	void				SendOffHandViewModelActivity( Activity activity );

	virtual void		CheatImpulseCommands( int iImpulse );
	virtual void		PlayerRunCommand( CUserCmd *ucmd, IMoveHelper *moveHelper );

	virtual void		CommitSuicide( bool bExplode = false, bool bForce = false );

	// Combats
	virtual void		TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator);
	virtual int			TakeHealth( float flHealth, int bitsDamageType );
	virtual	void		Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info );
	virtual void		Event_Killed( const CTakeDamageInfo &info );
	virtual void		PlayerDeathThink( void );
	virtual void		DetermineAssistForKill( const CTakeDamageInfo &info );
	virtual void		SetNumberofDominations( int iDominations )
	{
		// Check for some bogus values, which are sneaking in somehow
		if ( iDominations < 0 )
		{
			Assert( iDominations >= 0 );
			iDominations = 0;
		}
		else if ( iDominations >= MAX_PLAYERS )
		{
			Assert( iDominations < MAX_PLAYERS );
			iDominations = MAX_PLAYERS-1;
		}
		m_iNumberofDominations = iDominations;
	}
	virtual int			GetNumberofDominations( void ) { return m_iNumberofDominations; }

	virtual int			OnTakeDamage( const CTakeDamageInfo &inputInfo );
	virtual int			OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void		DamageEffect(float flDamage, int fDamageType);

	void				OnDealtDamage( CBaseCombatCharacter *pVictim, const CTakeDamageInfo &info );		// invoked when we deal damage to another victim
	int					GetDamagePerSecond( void ) const;
	void				ResetDamagePerSecond( void );

	virtual	bool		ShouldCollide( int collisionGroup, int contentsMask ) const;
	void				ApplyPushFromDamage( const CTakeDamageInfo &info, Vector vecDir );
	void				PlayDamageResistSound( float flStartDamage, float flModifiedDamage );

	virtual bool		Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon );
	bool				Weapon_CanSwitchTo_TF( CBaseCombatWeapon *pWeapon );
	bool				Weapon_CanSwitchTo_DOD( CBaseCombatWeapon *pWeapon );
	bool				Weapon_CanSwitchTo_CS( CBaseCombatWeapon *pWeapon );

	void				SetHealthBuffTime( float flTime )		{ m_flHealthBuffTime = flTime; }

	CPCWeaponBase		*GetActivePCWeapon( void ) const;

	virtual void		RemoveAllWeapons();

	void				SaveMe( void );

	void				FireBullet( CTFWeaponBase *pWpn, const FireBulletsInfo_t &info, bool bDoEffects, int nDamageType, int nCustomDamageType = TF_DMG_CUSTOM_NONE );

	void				ImpactWaterTrace( trace_t &trace, const Vector &vecStart );
	void				NoteWeaponFired();

	bool				HasItem( void ) const;					// Currently can have only one item at a time.
	void				SetItem( CTFItem *pItem );
	CTFItem				*GetItem( void ) const;

	void				Regenerate( void );
	float				GetNextRegenTime( void ){ return m_flNextRegenerateTime; }
	void				SetNextRegenTime( float flTime ){ m_flNextRegenerateTime = flTime; }

	float				GetNextChangeClassTime( void ){ return m_flNextChangeClassTime; }
	void				SetNextChangeClassTime( float flTime ){ m_flNextChangeClassTime = flTime; }

	virtual	void		RemoveAllItems( bool removeSuit );

	bool				DropCurrentWeapon( void );
	void				DropFlag( bool bSilent = false );
	void				TFWeaponRemove( int iWeaponID );
	bool				TFWeaponDrop( CTFWeaponBase *pWeapon, bool bThrowForward );

	// Class.
	CTFPlayerClass		*GetPlayerClass( void ) 					{ return &m_PlayerClass; }
	int					GetDesiredPlayerClassIndex( void )			{ return m_Shared.m_iDesiredPlayerClass; }
	void				SetDesiredPlayerClassIndex( int iClass )	{ m_Shared.m_iDesiredPlayerClass = iClass; }

	// Team.
	void				ForceChangeTeam( int iTeamNum, bool bFullTeamSwitch = false );
	virtual void		ChangeTeam( int iTeamNum );

	// mp_fadetoblack
	void				HandleFadeToBlack( void );

	// Flashlight controls for SFM - JasonM
	virtual int FlashlightIsOn( void );
	virtual void FlashlightTurnOn( void );
	virtual void FlashlightTurnOff( void );

	// Think.
	virtual void		PreThink();
	virtual void		PostThink();

	virtual void		ItemPostFrame();
	virtual void		Weapon_FrameUpdate( void );
	virtual void		Weapon_HandleAnimEvent( animevent_t *pEvent );
	virtual bool		Weapon_ShouldSetLast( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon );

	virtual void		GetStepSoundVelocities( float *velwalk, float *velrun );
	virtual void		SetStepSoundTime( stepsoundtimes_t iStepSoundTime, bool bWalking );

	// Utility.
	void				RemoveOwnedEnt( char *pEntName, bool bGrenade = false );
	void				UpdateModel( void );
	void				UpdateSkin( int iTeam );

	virtual int			GiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound = false );

	virtual int			GetMaxHealth()  const OVERRIDE;
	int					GetMaxHealthForBuffing()  const;

	//-----------------------------------------------------------------------------------------------------
	// Return true if we are a "mini boss" in Mann Vs Machine mode
	// SEALTODO
	bool				IsMiniBoss( void ) const { return false; }

	bool				CanAttack( int iCanAttackFlags = 0 );

	// This passes the event to the client's and server's CPlayerAnimState.
	void				DoAnimationEvent( PlayerAnimEvent_t event, int mData = 0 );

	virtual bool		ClientCommand( const CCommand &args );
	void				ClientHearVox( const char *pSentence );
	void				DisplayLocalItemStatus( CTFGoal *pGoal );

	int					BuildObservableEntityList( void );
	virtual int			GetNextObserverSearchStartPoint( bool bReverse ); // Where we should start looping the player list in a FindNextObserverTarget call
	virtual CBaseEntity *FindNextObserverTarget(bool bReverse);
	virtual bool		IsValidObserverTarget(CBaseEntity * target); // true, if player is allowed to see this target
	virtual bool		SetObserverTarget(CBaseEntity * target);
	virtual bool		ModeWantsSpectatorGUI( int iMode ) { return (iMode != OBS_MODE_FREEZECAM && iMode != OBS_MODE_DEATHCAM); }
	void				FindInitialObserverTarget( void );
	CBaseEntity		    *FindNearestObservableTarget( Vector vecOrigin, float flMaxDist );
	virtual void		ValidateCurrentObserverTarget( void );

	void CheckUncoveringSpies( CTFPlayer *pTouchedPlayer );
	void Touch( CBaseEntity *pOther );

	bool CanPlayerMove() const;
	void TeamFortress_SetSpeed();
	EHANDLE TeamFortress_GetDisguiseTarget( int nTeam, int nClass );

	void TeamFortress_ClientDisconnected();
	void RemoveAllOwnedEntitiesFromWorld( bool bExplodeBuildings = false );
	void RemoveOwnedProjectiles();
	int GetNumActivePipebombs( void );

	CTFTeamSpawn *GetSpawnPoint( void ){ return m_pSpawnPoint; }
		
	void SetAnimation( PLAYER_ANIM playerAnim );

	bool IsPlayerClass( int iClass ) const;

	void PlayFlinch( const CTakeDamageInfo &info );

	float PlayCritReceivedSound( void );

	void StunSound( CTFPlayer* pAttacker, int iStunFlags, int iOldStunFlags=0 );

	void PainSound( const CTakeDamageInfo &info );
	void DeathSound( const CTakeDamageInfo &info );

	void TFPainSound( const CTakeDamageInfo &info );
	void TFDeathSound( const CTakeDamageInfo &info );

	void DODPainSound( const CTakeDamageInfo &info );
	void DODDeathSound( const CTakeDamageInfo &info );

	void SetSeeCrit( bool bAllSeeCrit, bool bMiniCrit, bool bShowDisguisedCrit ) { m_bAllSeeCrit = bAllSeeCrit; m_bMiniCrit = bMiniCrit; m_bShowDisguisedCrit = bShowDisguisedCrit;  }
	void SetAttackBonusEffect( EAttackBonusEffects_t effect ) { m_eBonusAttackEffect = effect; }
	EAttackBonusEffects_t GetAttackBonusEffect( void ) { return m_eBonusAttackEffect; }

	// TF doesn't want the explosion ringing sound
	virtual void			OnDamagedByExplosion( const CTakeDamageInfo &info ) { return; }

	void	OnBurnOther( CTFPlayer *pTFPlayerVictim );

	// Buildables
	void SetWeaponBuilder( CTFWeaponBuilder *pBuilder );
	CTFWeaponBuilder *GetWeaponBuilder( void );

	int GetBuildResources( void );
	void RemoveBuildResources( int iAmount );
	void AddBuildResources( int iAmount );

	bool IsBuilding( void );
	int CanBuild( int iObjectType, int iObjectMode = 0 );

	CBaseObject	*GetObject( int index ) const;
	CBaseObject	*GetObjectOfType( int iObjectType, int iObjectMode = 0 ) const;
	int	GetObjectCount( void ) const;
	int GetNumObjects( int iObjectType, int iObjectMode = 0 );
	void RemoveAllObjects( bool bExplodeBuildings = false );
	void StopPlacement( void );
	int	StartedBuildingObject( int iObjectType );
	void StoppedBuilding( int iObjectType );
	void FinishedObject( CBaseObject *pObject );
	void AddObject( CBaseObject *pObject );
	void OwnedObjectDestroyed( CBaseObject *pObject );
	void RemoveObject( CBaseObject *pObject );
	bool PlayerOwnsObject( CBaseObject *pObject );
	void DetonateObjectOfType( int iObjectType, int iObjectMode = 0, bool bIgnoreSapperState = false );
	void StartBuildingObjectOfType( int iType, int iObjectMode = 0 );
	float GetObjectBuildSpeedMultiplier( int iObjectType, bool bIsRedeploy ) const;

	void OnSapperPlaced( CBaseEntity *sappedObject );			// invoked when we place a sapper on an enemy building
	bool IsPlacingSapper( void ) const;							// return true if we are a spy who placed a sapper on a building in the last few moments
	void OnSapperStarted( float flStartTime );
	void OnSapperFinished( float flStartTime );
	bool IsSapping( void ) const;
	int GetSappingEvent( void) const;
	void ClearSappingEvent( void );
	void ClearSappingTracking( void );

	CTFTeam *GetTFTeam( void );
	CTFTeam *GetOpposingTFTeam( void );

	void TeleportEffect( void );
	void RemoveTeleportEffect( void );
	bool HasTheFlag( ETFFlagType exceptionTypes[] = NULL, int nNumExceptions = 0 ) const;
	virtual bool IsAllowedToPickUpFlag( void ) const;

	// Death & Ragdolls.
	virtual void CreateRagdollEntity( void );
	void CreateRagdollEntity( bool bGib, bool bBurning, bool bElectrocuted, bool bOnGround, bool bCloakedCorpse, bool bGoldRagdoll, bool bIceRagdoll, bool bBecomeAsh, int iDamageCustom = 0, bool bCritOnHardHit = false );
	void DestroyRagdoll( void );
	CNetworkHandle( CBaseEntity, m_hRagdoll );	// networked entity handle 
	virtual bool ShouldGib( const CTakeDamageInfo &info ) OVERRIDE;

	// Dropping Ammo
	void DropAmmoPack( void );

	bool CanDisguise( void );
	bool CanGoInvisible( void );
	void RemoveInvisibility( void );

	void RemoveDisguise( void );
	void PrintTargetWeaponInfo( void );

	bool DoClassSpecialSkill( void );

	bool	CanPickupBuilding( CBaseObject *pPickupObject );
	bool TryToPickupBuilding( void );

	float GetLastDamageReceivedTime( void ) { return m_flLastDamageTime; }
	float GetLastEntityDamagedTime( void ) { return m_flLastDamageDoneTime; }
	void SetLastEntityDamagedTime( float flTime ) { m_flLastDamageDoneTime = flTime; }
	CBaseEntity *GetLastEntityDamaged( void ) { return m_hLastDamageDoneEntity; }
	void SetLastEntityDamaged( CBaseEntity *pEnt ) { m_hLastDamageDoneEntity = pEnt; }

	void SetClassMenuOpen( bool bIsOpen );
	bool IsClassMenuOpen( void );

	float GetCritMult( void ) { return m_Shared.GetCritMult(); }
	void  RecordDamageEvent( const CTakeDamageInfo &info, bool bKill, int nVictimPrevHealth ) { m_Shared.RecordDamageEvent(info,bKill,nVictimPrevHealth); }

	bool GetHudClassAutoKill( void ){ return m_bHudClassAutoKill; }
	void SetHudClassAutoKill( bool bAutoKill ){ m_bHudClassAutoKill = bAutoKill; }

	bool GetMedigunAutoHeal( void ){ return m_bMedigunAutoHeal; }
	void SetMedigunAutoHeal( bool bMedigunAutoHeal ){ m_bMedigunAutoHeal = bMedigunAutoHeal; }
	CBaseEntity		*MedicGetHealTarget( void );

	bool ShouldAutoReload( void ) { return m_bAutoReload; }
	void SetAutoReload( bool bAutoReload ) { m_bAutoReload = bAutoReload; }

	bool ShouldAutoRezoom( void ) { return m_bAutoRezoom; }
	void SetAutoRezoom( bool bAutoRezoom ) { m_bAutoRezoom = bAutoRezoom; }

	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet );

	virtual bool CanHearAndReadChatFrom( CBasePlayer *pPlayer );

	const Vector& 	GetClassEyeHeight( void );

	void	UpdateExpression( void );
	void	ClearExpression( void );

	virtual IResponseSystem *GetResponseSystem();
	virtual bool			SpeakConceptIfAllowed( int iConcept, const char *modifiers = NULL, char *pszOutResponseChosen = NULL, size_t bufsize = 0, IRecipientFilter *filter = NULL );

	virtual bool CanSpeakVoiceCommand( void );
	virtual bool ShouldShowVoiceSubtitleToEnemy( void );
	virtual void NoteSpokeVoiceCommand( const char *pszScenePlayed );
	void	SpeakWeaponFire( int iCustomConcept = MP_CONCEPT_NONE );
	void	ClearWeaponFireScene( void );

	virtual int DrawDebugTextOverlays( void );

	float m_flNextVoiceCommandTime;
	float m_flNextSpeakWeaponFire;

	virtual int	CalculateTeamBalanceScore( void );

	bool ShouldAnnouceAchievement( void );

	CTriggerAreaCapture *GetControlPointStandingOn( void );
	CCaptureZone *GetCaptureZoneStandingOn( void );
	CCaptureZone *GetClosestCaptureZone( void );

	bool CanAirDash( void ) const;

	virtual bool IsDeflectable() { return true; }

	bool IsInCombat( void ) const;								// return true if we are engaged in active combat

	void PlayerUse( bool bPushAway = true );

	bool InAirDueToExplosion( void ) { return (!(GetFlags() & FL_ONGROUND) && (GetWaterLevel() == WL_NotInWater) && (m_iBlastJumpState != 0) ); }
	bool InAirDueToKnockback( void ) { return (!(GetFlags() & FL_ONGROUND) && (GetWaterLevel() == WL_NotInWater) && ( (m_iBlastJumpState != 0) || m_Shared.InCond( TF_COND_KNOCKED_INTO_AIR ) || m_Shared.InCond( TF_COND_GRAPPLINGHOOK ) || m_Shared.InCond( TF_COND_GRAPPLINGHOOK_SAFEFALL ) ) ); }

	CBaseEntity *GetEntityForLoadoutSlot( int iLoadoutSlot );			//Gets whatever entity is associated with the loadout slot (wearable or weapon)

	void ApplyAbsVelocityImpulse ( const Vector &vecImpulse );
	bool ApplyPunchImpulseX ( float flImpulse );
	void ApplyAirBlastImpulse( const Vector &vecImpulse );

	void GameMovementInit( void );
	void GameAnimStateInit( void );

public:

	CTFPlayerShared m_Shared;

	int	    item_list;			// Used to keep track of which goalitems are 
								// affecting the player at any time.
								// GoalItems use it to keep track of their own 
								// mask to apply to a player's item_list

	float invincible_finished;
	float invisible_finished;
	float super_damage_finished;
	float radsuit_finished;

	int m_flNextTimeCheck;		// Next time the player can execute a "timeleft" command

	// TEAMFORTRESS VARIABLES
	int		no_sentry_message;
	int		no_dispenser_message;
	
	CNetworkVar( bool, m_bSaveMeParity );

	// teleporter variables
	int		no_entry_teleporter_message;
	int		no_exit_teleporter_message;

	float	m_flNextNameChangeTime;

	int					StateGet( void ) const;

	void				SetOffHandWeapon( CTFWeaponBase *pWeapon );
	void				HolsterOffHandWeapon( void );

	float				GetSpawnTime() { return m_flSpawnTime; }

	virtual bool Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );
	virtual void Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget , const Vector *pVelocity );

	void				ManageRegularWeapons( TFPlayerClassData_t *pData );
	void				ManageBuilderWeapons( TFPlayerClassData_t *pData );

	// Taunts.
	void				Taunt( void );
	bool				IsTaunting( void ) { return m_Shared.InCond( TF_COND_TAUNTING ); }
	bool				IsAllowedToTaunt( void );
	bool				ShouldStopTaunting();
	float				GetTauntYaw( void )				{ return m_flTauntYaw; }
	float				GetPrevTauntYaw( void )		{ return m_flPrevTauntYaw; }
	void				SetTauntYaw( float flTauntYaw );
	void				CancelTaunt( void );
	void				StopTaunt( void );
	float				GetTauntRemoveTime( void ) const { return m_flTauntRemoveTime; }
	bool				IsAllowedToRemoveTaunt() const { return m_bAllowedToRemoveTaunt; }
	QAngle				m_angTauntCamera;

	virtual float		PlayScene( const char *pszScene, float flDelay = 0.0f, AI_Response *response = NULL, IRecipientFilter *filter = NULL );
	void				ResetTauntHandle( void )				{ m_hTauntScene = NULL; }
	void				SetDeathFlags( int iDeathFlags ) { m_iDeathFlags = iDeathFlags; }
	int					GetDeathFlags() { return m_iDeathFlags; }
	void				SetMaxSentryKills( int iMaxSentryKills ) { m_iMaxSentryKills = iMaxSentryKills; }
	int					GetMaxSentryKills() { return m_iMaxSentryKills; }

	CNetworkVar( bool, m_iSpawnCounter );
	
	void				CheckForIdle( void );
	void				PickWelcomeObserverPoint();

	void				StopRandomExpressions( void ) { m_flNextRandomExpressionTime = -1; }
	void				StartRandomExpressions( void ) { m_flNextRandomExpressionTime = gpGlobals->curtime; }

	virtual bool			WantsLagCompensationOnEntity( const CBasePlayer	*pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const;

	float				MedicGetChargeLevel( CTFWeaponBase **pRetMedigun = NULL );

	CTFWeaponBase		*Weapon_OwnsThisID( int iWeaponID );
	CTFWeaponBase		*Weapon_GetWeaponByType( int iType );

	int					RocketJumped( void ) { return m_iBlastJumpState & TF_PLAYER_ROCKET_JUMPED; }
	int					StickyJumped( void ) { return m_iBlastJumpState & TF_PLAYER_STICKY_JUMPED; }
	void				SetBlastJumpState( int iState, bool bPlaySound = false );
	void				ClearBlastJumpState( void );

	bool				IsCapturingPoint( void );

	int					m_iOldStunFlags;

	bool				m_bFlipViewModels;
	int					m_iBlastJumpState;
	float				m_flBlastJumpLandTime;
	bool				m_bTakenBlastDamageSinceLastMovement;

	void				SetTargetDummy( void ){ m_bIsTargetDummy = true; }

	int					GetHealthBefore( void ) { return m_iHealthBefore; }

	void				AccumulateSentryGunDamageDealt( float damage );
	void				ResetAccumulatedSentryGunDamageDealt();
	float				GetAccumulatedSentryGunDamageDealt();

	void				IncrementSentryGunKillCount( void );
	void				ResetAccumulatedSentryGunKillCount();
	int					GetAccumulatedSentryGunKillCount();

	void				SetWaterExitTime( float flTime ){ m_flWaterExitTime = flTime; }
	float				GetWaterExitTime( void ){ return m_flWaterExitTime; }

	void SetMovementTypeReference( int pMoveType ) { m_iMoveType = pMoveType; }
	int  GetMovementTypeReference( void ) { return m_iMoveType; }

	void SetGameAnimStateTypeReference( int pAnimStateType ) { m_iAnimState = pAnimStateType; }
	int	 GetGameAnimStateTypeReference( void ) { return m_iAnimState; }

	bool DoesMovementTypeMatch();
	bool DoesAnimStateTypeMatch();

	int GetClassType() { return GetPlayerClass()->GetData()->m_iClassType; }

	// Legacy
	bool IsTF2Class() { return GetClassType() == FACTION_TF; }
	bool IsDODClass() { return GetClassType() == FACTION_DOD; }
	bool IsSpecialClass() { return GetClassType() == FACTION_PC; }
	bool IsCSClass() { return GetClassType() == FACTION_CS; }
	bool IsHL2Class() { return GetClassType() == FACTION_HL2; }
	bool IsHL1Class() { return GetClassType() == FACTION_HL1; }

	// Client commands.
	void				HandleCommand_JoinTeam( const char *pTeamName );
	void				HandleCommand_JoinClass( const char *pClassName );
	void				HandleCommand_JoinTeam_NoMenus( const char *pTeamName );

	float				m_flCommentOnCarrying;

	float				GetTimeSinceLastThink( void ) const { return ( m_flLastThinkTime >= 0.f ) ? gpGlobals->curtime - m_flLastThinkTime : -1.f; }

	void SetSecondaryLastWeapon( CBaseCombatWeapon *pSecondaryLastWeapon ) { m_hSecondaryLastWeapon = pSecondaryLastWeapon; }
	CBaseCombatWeapon* GetSecondaryLastWeapon() const { return m_hSecondaryLastWeapon; }

	void CreateDisguiseWeaponList( CTFPlayer *pDisguiseTarget );
	void ClearDisguiseWeaponList();

	virtual bool ShouldForceTransmitsForTeam( int iTeam ) OVERRIDE;

private:
	// Taunt.
	EHANDLE				m_hTauntScene;
	bool				m_bInitTaunt;
	CNetworkVar( int, m_nForceTauntCam );
	CNetworkVar( float, m_flTauntYaw );
	float				m_flPrevTauntYaw;

	bool				m_bAllowedToRemoveTaunt;
	float				m_flTauntStartTime;
	float				m_flTauntRemoveTime;
	Vector				m_vecTauntStartPosition;

	int					GetAutoTeam( void );

	// Creation/Destruction.
	void				InitClass( void );
	void				GiveDefaultItems();
	bool				SelectSpawnSpot( const char *pEntClassName, CBaseEntity* &pSpot );
	void				PrecachePlayerModels( void );
	void				PrecacheTFExtras( void );
	void				RemoveNemesisRelationships();

	// Think.
	void				TFPlayerThink();
	void				RegenThink();
	void				UpdateTimers( void );

	float				m_flAccumulatedHealthRegen;	// Regeneration can be in small amounts, so we accumulate it and apply when it's > 1
	float				m_flNextAmmoRegenAt;
	float				m_flLastHealthRegenAt;

	// Bots.
	friend void			Bot_Think( CTFPlayer *pBot );

	// Physics.
	void				PhysObjectSleep();
	void				PhysObjectWake();

	// Ammo pack.
	bool CalculateAmmoPackPositionAndAngles( CTFWeaponBase *pWeapon, Vector &vecOrigin, QAngle &vecAngles );
	void AmmoPackCleanUp( void );

	// State.
	CPlayerStateInfo	*StateLookupInfo( int nState );
	void				StateEnter( int nState );
	void				StateLeave( void );
	void				StateTransition( int nState );
	void				StateEnterWELCOME( void );
	void				StateThinkWELCOME( void );
	void				StateEnterPICKINGTEAM( void );
	void				StateEnterACTIVE( void );
	void				StateEnterOBSERVER( void );
	void				StateThinkOBSERVER( void );
	void				StateEnterDYING( void );
	void				StateThinkDYING( void );

	virtual bool		SetObserverMode(int mode);
	virtual void		AttemptToExitFreezeCam( void );

	bool				PlayGesture( const char *pGestureName );
	bool				PlaySpecificSequence( const char *pSequenceName );

	bool				GetResponseSceneFromConcept( int iConcept, char *chSceneBuffer, int numSceneBufferBytes );

	unsigned short int	m_iMoveType;
	unsigned short int	m_iAnimState;

	float				GetDesiredHeadScale() const { return 1.f; }
	float				GetHeadScaleSpeed() const;
	float				GetDesiredTorsoScale() const { return 1.f; }
	float				GetTorsoScaleSpeed() const;
	float				GetDesiredHandScale() const { return 1.f; }
	float				GetHandScaleSpeed() const;

	CNetworkHandle( CBaseCombatWeapon, m_hSecondaryLastWeapon );
public:
	// Achievement data storage
	CAchievementData	m_AchievementData;

private:
	// Map introductions
	int					m_iIntroStep;
	CHandle<CIntroViewpoint> m_hIntroView;
	float				m_flIntroShowHintAt;
	float				m_flIntroShowEventAt;
	bool				m_bHintShown;
	bool				m_bAbortFreezeCam;
	bool				m_bSeenRoundInfo;
	bool				m_bRegenerating;

	// Items.
	CNetworkHandle( CTFItem, m_hItem );

	// Combat.
	CNetworkHandle( CTFWeaponBase, m_hOffHandWeapon );

	float					m_flHealthBuffTime;
	int						m_iHealthBefore;

	bool					m_bAllSeeCrit;
	bool					m_bMiniCrit;
	bool					m_bShowDisguisedCrit;
	EAttackBonusEffects_t	m_eBonusAttackEffect;

	float					m_flLastDamageResistSoundTime;

	CNetworkVar( float, m_flHeadScale );
	CNetworkVar( float, m_flTorsoScale );
	CNetworkVar( float, m_flHandScale );

	float				m_accumulatedSentryGunDamageDealt;	// for Sentry Buster missions in MvM
	int					m_accumulatedSentryGunKillCount;	// for Sentry Buster missions in MvM

	static const int		DPS_Period = 90;					// The duration of the sliding window for calculating DPS, in seconds
	int						*m_damageRateArray;					// One array element per second, for accumulating damage done during that time
	int						m_lastDamageRateIndex;
	int						m_peakDamagePerSecond;

	CNetworkVar( uint16, m_nActiveWpnClip );
	uint16				m_nActiveWpnClipPrev;
	float				m_flNextClipSendTime;

	float					m_flWaterExitTime;

	bool					m_bIsSapping;
	int						m_iSappingEvent;
	float					m_flSapStartTime;
	float					m_flLastThinkTime;

	float					m_flNextRegenerateTime;
	float					m_flNextChangeClassTime;

	// Ragdolls.
	Vector					m_vecTotalBulletForce;

	// State.
	CPlayerStateInfo		*m_pStateInfo;

	// Spawn Point
	CTFTeamSpawn			*m_pSpawnPoint;

	// Networked.
	CNetworkQAngle( m_angEyeAngles );					// Copied from EyeAngles() so we can send it to the client.

	CTFPlayerClass		m_PlayerClass;
	int					m_iLastWeaponFireUsercmd;				// Firing a weapon.  Last usercmd we shot a bullet on.
	int					m_iLastSkin;
	CNetworkVar( float, m_flLastDamageTime );
	float				m_flLastDamageDoneTime;
	CHandle< CBaseEntity > m_hLastDamageDoneEntity;
	float				m_flLastHealedTime;
	float				m_flNextPainSoundTime;
	int					m_LastDamageType;
	int					m_iDeathFlags;				// TF_DEATH_* flags with additional death info
	int					m_iMaxSentryKills;			// most kills by a single sentry
	int					m_iNumberofDominations;		// number of active dominations for this player

	bool				m_bPlayedFreezeCamSound;

	CHandle< CTFWeaponBuilder > m_hWeaponBuilder;

	CUtlVector<EHANDLE>	m_aObjects;			// List of player objects

	bool m_bIsClassMenuOpen;

	Vector m_vecLastDeathPosition;

	float				m_flSpawnTime;

	float				m_flLastAction;
	bool				m_bIsIdle;

	CUtlVector<EHANDLE>	m_hObservableEntities;
	DamagerHistory_t m_DamagerHistory[MAX_DAMAGER_HISTORY];	// history of who has damaged this player
	CUtlVector<float>	m_aBurnOtherTimes;					// vector of times this player has burned others

	bool m_bHudClassAutoKill;

	// Background expressions
	string_t			m_iszExpressionScene;
	EHANDLE				m_hExpressionSceneEnt;
	float				m_flNextRandomExpressionTime;
	EHANDLE				m_hWeaponFireSceneEnt;

	bool				m_bSpeakingConceptAsDisguisedSpy;

	bool 				m_bMedigunAutoHeal;
	bool				m_bAutoReload;	// does the player want to autoreload their weapon when empty
	bool				m_bAutoRezoom;	// does the player want to re-zoom after each shot for sniper rifles

public:
	// Send ForcePlayerViewAngles user message. Handled in __MsgFunc_ForcePlayerViewAngles in
	// clientmode_tf.cpp. Sets Local and Abs angles, along with TauntYaw and VehicleMovingAngles.
	void ForcePlayerViewAngles( const QAngle& qTeleportAngles );

	bool				SetPowerplayEnabled( bool bOn );
	bool				PlayerHasPowerplay( void );
	void				PowerplayThink( void );
	float				m_flPowerPlayTime;

	CUtlVector< CHandle< CTFWeaponBase > > m_hDisguiseWeaponList; // copy disguise target weapons to this list

private:
	int					m_iLeftGroundHealth;	// health we were at the last time we left the ground

	bool				m_bCreatedRocketJumpParticles;

	bool				m_bIsTargetDummy;

	bool				m_bCollideWithSentry;
	CountdownTimer		m_placedSapperTimer;

//////////////////////////////// Day Of Defeat ////////////////////////////////
public:
	virtual void	SharedSpawn();

	void	DODFireBullets( const FireBulletsInfo_t& info );

	void	SetBazookaDeployed( bool bDeployed ) { m_bBazookaDeployed = bDeployed; }

	Activity TranslateActivity( Activity baseAct, bool *pRequired = NULL );

	void	PickUpWeapon( CWeaponDODBase *pWeapon );

	virtual void	TraceAttackDOD( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );

	void PopHelmet( Vector vecDir, Vector vecForceOrigin );

	bool	DropActiveWeapon( void );
	bool	DropPrimaryWeapon( void );
	bool	DODWeaponDrop( CBaseCombatWeapon *pWeapon, bool bThrowForward );
	bool	BumpWeapon( CBaseCombatWeapon *pBaseWeapon );

	void	ManageDODWeapons( TFPlayerClassData_t *pData );
	void	ManageDODGrenades( TFPlayerClassData_t *pData );

	CBaseEntity	*	GiveNamedItem( const char *pszName, int iSubType = 0 );

	Vector m_lastStandingPos; // used by the gamemovement code for finding ladders

	void	SetSprinting( bool bIsSprinting );

	//void SetDefusing( CDODBombTarget *pTarget );
	//bool m_bIsDefusing;
	//CHandle<CDODBombTarget> m_pDefuseTarget;

	//void SetPlanting( CDODBombTarget *pTarget );
	//bool m_bIsPlanting;
	//CHandle<CDODBombTarget> m_pPlantTarget;

	void	CheckProneMoveSound( int groundspeed, bool onground );
	virtual void UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  );
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );

	CWeaponDODBase	*DODWeapon_OwnsThisID( int iWeaponID );
	CWeaponDODBase	*DODWeapon_GetWeaponByType( int iType );

	int				GetMaxAmmo( int iAmmoIndex, int iClassIndex = -1 );

	void			OnDamagedByExplosionDOD( const CTakeDamageInfo &info );
	void			OnDamageByStun( const CTakeDamageInfo &info );
	void			DeafenThink( void );

	CNetworkVar( float, m_flStunDuration );
	CNetworkVar( float, m_flStunMaxAlpha );
private:
	bool	SetSpeed( int speed );

	void	InitProne( void );

	void	InitSprinting( void );
	bool	IsSprinting( void );
	bool	CanSprint( void );

	float	m_flNextStaminaThink;	//time to do next stamina gain

	bool	m_bBazookaDeployed;

	bool	m_bSlowedByHit;
	float	m_flUnslowTime;
	int		m_iPlayerSpeed;	//last updated player max speed

	float m_flMinNextStepSoundTime;

	int m_LastHitGroup;			// the last body region that took damage

	bool m_bPlayingProneMoveSound;

	EHANDLE m_hLastDroppedWeapon;

//////////////////////////////// END OF	///////////////////////////////////////

//////////////////////////////// Counter-Strike ////////////////////////////////
public:
	void	ManageCSWeapons( TFPlayerClassData_t *pData );
	CWeaponCSBase	*CSWeapon_OwnsThisID( int iWeaponID );

	void GetBulletTypeParameters( 
		int iBulletType, 
		float &fPenetrationPower, 
		float &flPenetrationDistance );

	void CSFireBullet( 
		Vector vecSrc, 
		const QAngle &shootAngles, 
		float flDistance, 
		int iPenetration, 
		int iBulletType, 
		int iDamage, 
		float flRangeModifier, 
		CBaseEntity *pevAttacker,
		bool bDoEffects,
		float xSpread, float ySpread );

//	virtual void TraceAttackCS( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );

	CNetworkVar( int, m_iShotsFired );	// number of shots fired recently

	int m_LastHitBox;			// the last body hitbox that took damage
	Vector m_vLastHitLocationObjectSpace; //position where last hit occured in space of the bone associated with the hitbox
//////////////////////////////// END OF	///////////////////////////////////////
};

//-----------------------------------------------------------------------------
// Purpose: Utility function to convert an entity into a tf player.
//   Input: pEntity - the entity to convert into a player
//-----------------------------------------------------------------------------
inline CTFPlayer *ToTFPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	Assert( dynamic_cast<CTFPlayer*>( pEntity ) != 0 );
	return static_cast< CTFPlayer* >( pEntity );
}

inline void CTFPlayer::OnSapperPlaced( CBaseEntity *sappedObject )
{
	m_placedSapperTimer.Start( 3.0f );
}
inline void CTFPlayer::OnSapperStarted( float flStartTime )
{
	if (m_iSappingEvent == TF_SAPEVENT_NONE && m_flSapStartTime == 0.00 )
	{
		m_flSapStartTime = flStartTime;
		m_bIsSapping = true;
		m_iSappingEvent = TF_SAPEVENT_PLACED;
	}
}
inline void CTFPlayer::OnSapperFinished( float flStartTime )
{
	if (m_iSappingEvent == TF_SAPEVENT_NONE && flStartTime == m_flSapStartTime )
	{
		m_bIsSapping = false;
		m_flSapStartTime = 0.00;
		m_iSappingEvent = TF_SAPEVENT_DONE;
	}
}
inline bool CTFPlayer::IsSapping( void ) const
{
	return m_bIsSapping;
}

inline int CTFPlayer::GetSappingEvent( void ) const
{
	return m_iSappingEvent;
}

inline void CTFPlayer::ClearSappingEvent( void )
{
	m_iSappingEvent = TF_SAPEVENT_NONE;
}

inline void CTFPlayer::ClearSappingTracking( void )
{
	ClearSappingEvent();
	m_bIsSapping = false;
	m_flSapStartTime = 0.00;
}

inline bool CTFPlayer::IsPlacingSapper( void ) const
{
	return !m_placedSapperTimer.IsElapsed();
}

inline int CTFPlayer::StateGet( void ) const
{
	return m_Shared.m_nPlayerState;
}

inline bool CTFPlayer::IsInCombat( void ) const
{
	// the simplest condition is whether we've been firing our weapon very recently
	return GetTimeSinceWeaponFired() < 2.0f;
}

inline void CTFPlayer::AccumulateSentryGunDamageDealt( float damage )
{
	m_accumulatedSentryGunDamageDealt += damage;
}

inline void	CTFPlayer::ResetAccumulatedSentryGunDamageDealt()
{
	m_accumulatedSentryGunDamageDealt = 0.0f;
}

inline float CTFPlayer::GetAccumulatedSentryGunDamageDealt()
{
	return m_accumulatedSentryGunDamageDealt;
}

inline void CTFPlayer::IncrementSentryGunKillCount( void )
{
	++m_accumulatedSentryGunKillCount;
}

inline void	CTFPlayer::ResetAccumulatedSentryGunKillCount()
{
	m_accumulatedSentryGunKillCount = 0;
}

inline int CTFPlayer::GetAccumulatedSentryGunKillCount()
{
	return m_accumulatedSentryGunKillCount;
}

inline int CTFPlayer::GetDamagePerSecond( void ) const
{
	return m_peakDamagePerSecond;
}

inline void CTFPlayer::ResetDamagePerSecond( void )
{
	for( int i=0; i<DPS_Period; ++i )
	{
		m_damageRateArray[i] = 0;
	}

	m_lastDamageRateIndex = -1;
	m_peakDamagePerSecond = 0;
}

#endif	// TF_PLAYER_H
