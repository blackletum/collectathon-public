//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef C_TF_PLAYER_H
#define C_TF_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_playeranimstate.h"
#include "c_baseplayer.h"
#include "tf_shareddefs.h"
#include "baseparticleentity.h"
#include "tf_player_shared.h"
#include "c_tf_playerclass.h"
#include "tf_item.h"
#include "props_shared.h"
#include "hintsystem.h"
#include "c_playerattachedmodel.h"
#include "iinput.h"
#include "tf_weapon_medigun.h"
#include "GameEventListener.h"

#include "clientsteamcontext.h"

class C_MuzzleFlashModel;
class C_BaseObject;
class C_TFRagdoll;
class C_CaptureZone;

extern ConVar tf_medigun_autoheal;
extern ConVar cl_autoreload;
extern ConVar cl_autorezoom;

enum EBonusEffectFilter_t
{
	kEffectFilter_AttackerOnly,
	kEffectFilter_AttackerTeam,
	kEffectFilter_VictimOnly,
	kEffectFilter_VictimTeam,
	kEffectFilter_AttackerAndVictimOnly,
	kEffectFilter_BothTeams,
};

struct BonusEffect_t
{
	BonusEffect_t( const char* pszSoundName, const char* pszParticleName, EBonusEffectFilter_t eParticleFilter, EBonusEffectFilter_t eSoundFilter, bool bPlaySoundInAttackersEars )
		: m_pszSoundName( pszSoundName )
		, m_pszParticleName( pszParticleName )
		, m_eParticleFilter( eParticleFilter )
		, m_eSoundFilter( eSoundFilter )
		, m_bPlaySoundInAttackersEars( bPlaySoundInAttackersEars )

	{}

	const char* m_pszSoundName;
	const char* m_pszParticleName;
	EBonusEffectFilter_t m_eParticleFilter;
	EBonusEffectFilter_t m_eSoundFilter;
	bool m_bPlaySoundInAttackersEars;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_TFPlayer : public C_BasePlayer
{
public:

	DECLARE_CLASS( C_TFPlayer, C_BasePlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();

	C_TFPlayer();
	~C_TFPlayer();

	static C_TFPlayer* GetLocalTFPlayer();

	virtual void UpdateOnRemove( void );

	virtual const QAngle& GetRenderAngles();
	virtual void UpdateClientSideAnimation();
	virtual void SetDormant( bool bDormant );
	virtual void OnPreDataChanged( DataUpdateType_t updateType );
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ProcessMuzzleFlashEvent();
	virtual void ValidateModelIndex( void );

	virtual Vector GetObserverCamOrigin( void );
	virtual int DrawModel( int flags );

	virtual void BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );

	virtual bool CreateMove( float flInputSampleTime, CUserCmd *pCmd );

	virtual bool				IsAllowedToSwitchWeapons( void );

	virtual void ClientThink();

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

	// Deal with recording
	virtual void GetToolRecordingState( KeyValues *msg );

	CPCWeaponBase *GetActivePCWeapon( void ) const;

	virtual void Simulate( void );
	virtual void FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );

	void FireBullet( CTFWeaponBase *pWpn, const FireBulletsInfo_t &info, bool bDoEffects, int nDamageType, int nCustomDamageType = TF_DMG_CUSTOM_NONE );

	void ImpactWaterTrace( trace_t &trace, const Vector &vecStart );

	bool CanAttack( int iCanAttackFlags = 0 );

	const C_TFPlayerClass *GetPlayerClass( void ) const { return &m_PlayerClass; }
	C_TFPlayerClass *GetPlayerClass( void )				{ return &m_PlayerClass; }
	bool IsPlayerClass( int iClass ) const;
	virtual int GetMaxHealth( void ) const;
	int			GetMaxHealthForBuffing()  const;

	virtual int GetRenderTeamNumber( void );

	bool IsWeaponLowered( void );

	void	AvoidPlayers( CUserCmd *pCmd );

	// Get the ID target entity index. The ID target is the player that is behind our crosshairs, used to
	// display the player's name.
	void UpdateIDTarget();
	int GetIDTarget() const;
	void SetForcedIDTarget( int iTarget );

	void SetAnimation( PLAYER_ANIM playerAnim );

	virtual float GetMinFOV() const;

	virtual const QAngle& EyeAngles();

	bool	ShouldDrawSpyAsDisguised();
	virtual int GetBody( void );

	int GetBuildResources( void );

	// MATTTODO: object selection if necessary
	void SetSelectedObject( C_BaseObject *pObject ) {}

	void GetTeamColor( Color &color );
	bool InSameDisguisedTeam( CBaseEntity *pEnt );

	virtual void ComputeFxBlend( void );

	// Taunts/VCDs
	virtual bool	StartSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, C_BaseEntity *pTarget );
	virtual void	CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov );
	bool			StartGestureSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );
	void			TurnOnTauntCam( void );
	void			TurnOnTauntCam_Finish( void );
	void			TurnOffTauntCam( void );
	void			TurnOffTauntCam_Finish( void );
	bool			IsTaunting( void ) const { return m_Shared.InCond( TF_COND_TAUNTING ); }

	virtual void	InitPhonemeMappings();

	// Gibs.
	void InitPlayerGibs( void );
	void CheckAndUpdateGibType( void );
	void CreatePlayerGibs( const Vector &vecOrigin, const Vector &vecVelocity, float flImpactScale, bool bBurning, bool bOnlyHead=false, bool bDisguiseGibs=false );
	void DropPartyHat( breakablepropparams_t &breakParams, Vector &vecBreakVelocity );

	int	GetObjectCount( void );
	C_BaseObject *GetObject( int index );
	C_BaseObject *GetObjectOfType( int iObjectType, int iObjectMode=0 ) const;
	int GetNumObjects( int iObjectType, int iObjectMode=0 );

	virtual bool ShouldCollide( int collisionGroup, int contentsMask ) const;

	float GetPercentInvisible( void );
	float GetEffectiveInvisibilityLevel( void );	// takes viewer into account

	virtual void AddDecal( const Vector& rayStart, const Vector& rayEnd,
		const Vector& decalCenter, int hitbox, int decalIndex, bool doTrace, trace_t& tr, int maxLODToDecal = ADDDECAL_TO_ALL_LODS );

	virtual void CalcDeathCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void		 CalcChaseCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	virtual Vector GetChaseCamViewOffset( CBaseEntity *target );
	virtual Vector GetDeathViewPosition();

	void ClientPlayerRespawn( void );

	void CreateSaveMeEffect( void );

	bool			IsAllowedToTaunt( void );
	bool			ShouldStopTaunting();
	float			GetTauntYaw( void )				{ return m_flTauntYaw; }
	float			GetPrevTauntYaw( void )		{ return m_flPrevTauntYaw; }
	void			SetTauntYaw( float flTauntYaw );

	virtual bool	IsOverridingViewmodel( void );
	virtual int		DrawOverriddenViewmodel( C_BaseViewModel *pViewmodel, int flags );

	void			SetHealer( C_TFPlayer *pHealer, float flChargeLevel );
	void			GetHealer( C_TFPlayer **pHealer, float *flChargeLevel ) { *pHealer = m_hHealer; *flChargeLevel = m_flHealerChargeLevel; }
	float			MedicGetChargeLevel( CTFWeaponBase **pRetMedigun = NULL );
	CBaseEntity		*MedicGetHealTarget( void );

	void			StartBurningSound( void );
	void			StopBurningSound( void );
	void			OnAddTeleported( void );
	void			OnRemoveTeleported( void );

	void			UpdateSpyStateChange( void );

	bool			CanShowClassMenu( void );

	void			InitializePoseParams( void );
	void			UpdateLookAt( void );

	bool			IsEnemyPlayer( void );
	void			ShowNemesisIcon( bool bShow );

	CUtlVector<EHANDLE>		*GetSpawnedGibs( void ) { return &m_hSpawnedGibs; }

	const Vector& 	GetClassEyeHeight( void );

	void			ForceUpdateObjectHudState( void );

	bool			GetMedigunAutoHeal( void ){ return tf_medigun_autoheal.GetBool(); }
	bool			ShouldAutoReload( void ){ return cl_autoreload.GetBool(); }
	bool			ShouldAutoRezoom( void ){ return cl_autorezoom.GetBool(); }

	void			GetTargetIDDataString( bool bIsDisguised, OUT_Z_BYTECAP(iMaxLenInBytes) wchar_t *sDataString, int iMaxLenInBytes, bool &bIsAmmoData );

	void			RemoveDisguise( void );
	bool			CanDisguise( void );

	bool			CanAirDash( void ) const;

	bool			CanUseFirstPersonCommand( void );

	bool			IsEffectRateLimited( EBonusEffectFilter_t effect, const C_TFPlayer* pAttacker ) const;
	bool			ShouldPlayEffect( EBonusEffectFilter_t filter, const C_TFPlayer* pAttacker, const C_TFPlayer* pVictim ) const;
	virtual void	FireGameEvent( IGameEvent *event );

	// Set the distances the camera should use. 
	void			SetTauntCameraTargets( float back, float up );

	// TF-specific color values for GlowEffect
	virtual void	GetGlowEffectColor( float *r, float *g, float *b );
	void UpdateGlowColor( void );

public:
	// Shared functions
	bool			CanPlayerMove() const;
	void			TeamFortress_SetSpeed();
	bool			HasItem( void ) const;				// Currently can have only one item at a time.
	void			SetItem( C_TFItem *pItem );
	C_TFItem		*GetItem( void ) const;
	bool			HasTheFlag( ETFFlagType exceptionTypes[] = NULL, int nNumExceptions = 0 ) const;
	virtual bool	IsAllowedToPickUpFlag( void ) const;
	float			GetCritMult( void ) { return m_Shared.GetCritMult(); }

	virtual void	ItemPostFrame( void );

	void			SetOffHandWeapon( CTFWeaponBase *pWeapon );
	void			HolsterOffHandWeapon( void );

	virtual int GetSkin();

	float GetLastDamageTime( void ) const { return m_flLastDamageTime; }

	virtual bool		Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon );
	bool				Weapon_CanSwitchTo_TF( CBaseCombatWeapon *pWeapon );
	bool				Weapon_CanSwitchTo_DOD( CBaseCombatWeapon *pWeapon );
	bool				Weapon_CanSwitchTo_CS( CBaseCombatWeapon *pWeapon );

	virtual bool		Weapon_ShouldSetLast( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon );
	virtual	bool		Weapon_Switch( C_BaseCombatWeapon *pWeapon, int viewmodelindex = 0 );

	CBaseEntity			*GetEntityForLoadoutSlot( int iLoadoutSlot );			//Gets whatever entity is associated with the loadout slot (wearable or weapon)

	CTFWeaponBase		*Weapon_OwnsThisID( int iWeaponID );
	CTFWeaponBase		*Weapon_GetWeaponByType( int iType );

	virtual void		GetStepSoundVelocities( float *velwalk, float *velrun );
	virtual void		SetStepSoundTime( stepsoundtimes_t iStepSoundTime, bool bWalking );

	bool	DoClassSpecialSkill( void );
	bool	CanGoInvisible( void );

	//-----------------------------------------------------------------------------------------------------
	// Return true if we are a "mini boss" in Mann Vs Machine mode
	// SEALTODO
	bool IsMiniBoss( void ) const { return false; }

	bool	CanPickupBuilding( CBaseObject *pPickupObject );
	bool	TryToPickupBuilding( void );
	void	StartBuildingObjectOfType( int iType, int iObjectMode=0 );

	C_CaptureZone *GetCaptureZoneStandingOn( void );
	C_CaptureZone *GetClosestCaptureZone( void );

	void	GameMovementInit( void );
	void	GameAnimStateInit( void );

	void SetMovementTypeReference( int pMoveType ) { m_iMoveType = pMoveType; }
	int  GetMovementTypeReference( void ) { return m_iMoveType; }

	void SetGameAnimStateTypeReference( int pAnimStateType ) { m_iAnimState = pAnimStateType; }
	int	 GetGameAnimStateTypeReference( void ) { return m_iAnimState; }

public:
	float			GetHeadScale() const { return m_flHeadScale; }
	float			GetTorsoScale() const { return m_flTorsoScale; }
	float			GetHandScale() const { return m_flHandScale; }

	// Ragdolls.
	virtual C_BaseAnimating *BecomeRagdollOnClient();
	virtual IRagdoll		*GetRepresentativeRagdoll() const;
	EHANDLE	m_hRagdoll;
	Vector m_vecRagdollVelocity;

	// Objects
	int CanBuild( int iObjectType, int iObjectMode=0 );
	CUtlVector< CHandle<C_BaseObject> > m_aObjects;

	virtual CStudioHdr *OnNewModel( void );

	void				DisplaysHintsForTarget( C_BaseEntity *pTarget );

	// Shadows
	virtual ShadowType_t ShadowCastType( void ) ;
	virtual void GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType );
	virtual void GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual bool GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const;

	CMaterialReference *GetInvulnMaterialRef( void ) { return &m_InvulnerableMaterial; }
	bool IsNemesisOfLocalPlayer();
	bool ShouldShowNemesisIcon();

	virtual	IMaterial *GetHeadLabelMaterial( void );

	bool	IsPlayerOnSteamFriendsList( C_BasePlayer *pPlayer );

protected:

	void ResetFlexWeights( CStudioHdr *pStudioHdr );

	virtual void UpdateGlowEffect( void );
	virtual void DestroyGlowEffect( void );

private:
	bool ShouldShowPowerupGlowEffect();
	void GetPowerupGlowEffectColor( float *r, float *g, float *b );

	void HandleTaunting( void );
	void TauntCamInterpolation( void );

	void OnPlayerClassChange( void );
	void UpdatePartyHat( void );

	void InitInvulnerableMaterial( void );

	bool				m_bWasTaunting;
	bool				m_bTauntInterpolating;
	CameraThirdData_t	m_TauntCameraData;
	float				m_flTauntCamCurrentDist;
	float				m_flTauntCamTargetDist;
	float				m_flTauntCamCurrentDistUp;
	float				m_flTauntCamTargetDistUp;

	QAngle				m_angTauntPredViewAngles;
	QAngle				m_angTauntEngViewAngles;

	unsigned short int	m_iMoveType;
	unsigned short int	m_iAnimState;
private:

	C_TFPlayerClass		m_PlayerClass;

	// ID Target
	int					m_iIDEntIndex;
	int					m_iForcedIDTarget;

	CNewParticleEffect	*m_pTeleporterEffect;
	bool				m_bToolRecordingVisibility;

	int					m_iOldState;
	int					m_iOldSpawnCounter;

	// Healer
	CHandle<C_TFPlayer>	m_hHealer;
	float				m_flHealerChargeLevel;
	int					m_iOldHealth;

	CNetworkVar( int, m_iPlayerModelIndex );

	// Look At
	/*
	int m_headYawPoseParam;
	int m_headPitchPoseParam;
	float m_headYawMin;
	float m_headYawMax;
	float m_headPitchMin;
	float m_headPitchMax;
	float m_flLastBodyYaw;
	float m_flCurrentHeadYaw;
	float m_flCurrentHeadPitch;
	*/

	// Spy cigarette smoke
	bool m_bCigaretteSmokeActive;

	// Medic callout particle effect
	CNewParticleEffect	*m_pSaveMeEffect;

	bool m_bUpdateObjectHudState;

	int				m_nForceTauntCam;
	float			m_flTauntYaw;
	float			m_flPrevTauntYaw;

public:

	CTFPlayerShared m_Shared;

// Called by shared code.
public:

	void DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );

	QAngle	m_angEyeAngles;
	CInterpolatedVar< QAngle >	m_iv_angEyeAngles;

	CNetworkHandle( C_TFItem, m_hItem );

	CNetworkHandle( C_TFWeaponBase, m_hOffHandWeapon );

	CGlowObject		*m_pPowerupGlowEffect;

	int				m_iOldPlayerClass;	// Used to detect player class changes
	bool			m_bIsDisplayingNemesisIcon;

	float			m_flLastDamageTime;

	int				m_iSpawnCounter;

	bool			m_bSaveMeParity;
	bool			m_bOldSaveMeParity;

	int				m_nOldWaterLevel;
	float			m_flWaterEntryTime;
	bool			m_bWaterExitEffectActive;

	bool			m_bDuckJumpInterp;
	float			m_flFirstDuckJumpInterp;
	float			m_flLastDuckJumpInterp;
	float			m_flDuckJumpInterp;

	CMaterialReference	m_InvulnerableMaterial;


	// Burning
	CSoundPatch			*m_pBurningSound;
	CNewParticleEffect	*m_pBurningEffect;
	float				m_flBurnEffectStartTime;
	float				m_flBurnEffectEndTime;

	// Temp HACK for crit boost
	HPARTICLEFFECT m_pCritBoostEffect;

	HPARTICLEFFECT m_pStunnedEffect;

	CNewParticleEffect	*m_pDisguisingEffect;
	float m_flDisguiseEffectStartTime;
	float m_flDisguiseEndEffectStartTime;

	EHANDLE					m_hFirstGib;
	EHANDLE					m_hHeadGib;
	CUtlVector<EHANDLE>		m_hSpawnedGibs;

	int				m_iOldTeam;
	int				m_iOldClass;
	int				m_iOldDisguiseTeam;
	int				m_iOldDisguiseClass;

	bool			m_bDisguised;
	int				m_iPreviousMetal;

	int GetNumActivePipebombs( void );

	int				m_iSpyMaskBodygroup;

	bool			m_bUpdatePartyHat;
	CHandle<C_PlayerAttachedModel>	m_hPartyHat;

	bool		CanDisplayAllSeeEffect( EAttackBonusEffects_t effect ) const;
	void		SetNextAllSeeEffectTime( EAttackBonusEffects_t effect, float flTime );

	void SetSecondaryLastWeapon( CBaseCombatWeapon *pSecondaryLastWeapon ) { m_hSecondaryLastWeapon = pSecondaryLastWeapon; }
	CBaseCombatWeapon* GetSecondaryLastWeapon() const { return m_hSecondaryLastWeapon; }

private:

	float m_flWaterImpactTime;

	// Gibs.
	CUtlVector< int > m_aSillyGibs;
	CUtlVector< char* > m_aNormalGibs;
	CUtlVector<breakmodel_t>	m_aGibs;

	C_TFPlayer( const C_TFPlayer & );

	// Medic healtarget active weapon ammo/clip count
	uint16	m_nActiveWpnClip;

	CNetworkVar( float, m_flHeadScale );
	CNetworkVar( float, m_flTorsoScale );
	CNetworkVar( float, m_flHandScale );

	// Allseecrit throttle - other clients ask us if we can be the source of another particle+sound
	float	m_flNextMiniCritEffectTime[ kBonusEffect_Count ];

	CNetworkHandle( CBaseCombatWeapon, m_hSecondaryLastWeapon );

//////////////////////////////// Day Of Defeat ////////////////////////////////
#define PRONE_MAX_SWAY		3.0
#define PRONE_SWAY_AMOUNT	4.0
#define RECOIL_DURATION 0.1
public:
	virtual void	SharedSpawn();

	void DODFireBullets( const FireBulletsInfo_t &info );

	void GetWeaponRecoilAmount( int weaponId, float &flPitchRecoil, float &flYawRecoil );
	void DoRecoil( int iWpnID, float flWpnRecoil );
	void SetRecoilAmount( float flPitchRecoil, float flYawRecoil );
	void GetRecoilToAddThisFrame( float &flPitchRecoil, float &flYawRecoil );

	void SetBazookaDeployed( bool bDeployed ) {}

	Activity TranslateActivity( Activity baseAct, bool *pRequired = NULL );

	virtual void ReceiveMessage( int classID, bf_read &msg );
	void PopHelmet( Vector vecDir, Vector vecForceOffset, int model );

	void SetSprinting( bool bIsSprinting );
	bool IsSprinting( void );

	bool IsDODWeaponLowered( void );

	void	CheckProneMoveSound( int groundspeed, bool onground );
	virtual void UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  );
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );

	float m_flRecoilTimeRemaining;
	float m_flPitchRecoilAccumulator;
	float m_flYawRecoilAccumulator;

	Vector m_lastStandingPos; // used by the gamemovement code for finding ladders

	// for stun effect
	float m_flStunEffectTime;
	float m_flStunAlpha;
	CNetworkVar( float, m_flStunMaxAlpha );
	CNetworkVar( float, m_flStunDuration );	

	bool	m_bWeaponLowered;	// should our weapon be lowered right now

	CWeaponDODBase	*DODWeapon_OwnsThisID( int iWeaponID );
	CWeaponDODBase	*DODWeapon_GetWeaponByType( int iType );
private:
	float	m_flProneViewOffset;
	bool	m_bProneSwayingRight;

	float m_flMinNextStepSoundTime;

	bool m_bPlayingProneMoveSound;

	void StaminaSoundThink( void );
	CSoundPatch		*m_pStaminaSound;
	bool m_bPlayingLowStaminaSound;

	// Cold Breath
	bool			CreateColdBreathEmitter( void );
	void			DestroyColdBreathEmitter( void );
	void			UpdateColdBreath( void );
	void			EmitColdBreathParticles( void );

	virtual void	ProcessMuzzleFlashEventDOD();

	bool						m_bColdBreathOn;
	float						m_flColdBreathTimeStart;
	float						m_flColdBreathTimeEnd;
	CSmartPtr<CSimpleEmitter>	m_hColdBreathEmitter;	
	PMaterialHandle				m_hColdBreathMaterial;
	int							m_iHeadAttach;

//////////////////////////////// END OF	///////////////////////////////////////

//////////////////////////////// Counter-Strike ////////////////////////////////
public:
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

	CNetworkVar( int, m_iShotsFired );	// number of shots fired recently
//////////////////////////////// END OF	///////////////////////////////////////
};

inline C_TFPlayer* ToTFPlayer( C_BaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	Assert( dynamic_cast<C_TFPlayer*>( pEntity ) != 0 );
	return static_cast< C_TFPlayer* >( pEntity );
}

void SetAppropriateCamera( C_TFPlayer *pPlayer );

#endif // C_TF_PLAYER_H
