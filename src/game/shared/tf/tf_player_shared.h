//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: Shared player code.
//
//=============================================================================
#ifndef TF_PLAYER_SHARED_H
#define TF_PLAYER_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "networkvar.h"
#include "tf_shareddefs.h"
#include "tf_weaponbase.h"
#include "basegrenade_shared.h"
#include "SpriteTrail.h"
#include "tf_condition.h"

// DOD Includes
#include "weapon_dodbase.h"
#include "dod_shareddefs.h"

class CWeaponDODBase;

// CS
#include "weapon_csbase.h"

class CWeaponCSBase;

// Client specific.
#ifdef CLIENT_DLL
#include "baseobject_shared.h"
#include "c_tf_weapon_builder.h"
class C_TFPlayer;
// Server specific.
#else
#include "tf_weapon_builder.h"
#include "tf_obj.h"
class CTFPlayer;
#endif

class CViewOffsetAnimation
{
public:
	CViewOffsetAnimation( CBasePlayer *pPlayer )
	{
		m_pPlayer = pPlayer;
		m_vecStart = vec3_origin;
		m_vecDest = vec3_origin;
		m_flLength = 0.0;
		m_flEndTime = 0.0;
		m_eViewAnimType = VIEW_ANIM_LINEAR_Z_ONLY;
		m_bActive = false;
	}

	static CViewOffsetAnimation *CreateViewOffsetAnim( CBasePlayer *pPlayer )
	{
		CViewOffsetAnimation *p = new CViewOffsetAnimation( pPlayer );

		Assert( p );

		return p;
	}

	void StartAnimation( Vector vecStart, Vector vecDest, float flTime, ViewAnimationType type )
	{
		m_vecStart = vecStart;
		m_vecDest = vecDest;
		m_flLength = flTime;
		m_flEndTime = gpGlobals->curtime + flTime;
		m_eViewAnimType = type;
		m_bActive = true;
	}

	void Reset( void ) 
	{
		m_bActive = false;
	}

	void Think( void )
	{
		if ( !m_bActive )
			return;

		if ( IsFinished() )
		{
			m_bActive = false;
			return;
		}

		float flFraction = ( m_flEndTime - gpGlobals->curtime ) / m_flLength;

		Assert( m_pPlayer );

		if ( m_pPlayer )
		{
			Vector vecCurrentView = m_pPlayer->GetViewOffset();

			switch ( m_eViewAnimType )
			{
			case VIEW_ANIM_LINEAR_Z_ONLY:
				vecCurrentView.z = flFraction * m_vecStart.z + ( 1.0 - flFraction ) * m_vecDest.z;
				break;

			case VIEW_ANIM_SPLINE_Z_ONLY:
				vecCurrentView.z = SimpleSplineRemapVal( flFraction, 1.0, 0.0, m_vecStart.z, m_vecDest.z );
				break;

			case VIEW_ANIM_EXPONENTIAL_Z_ONLY:
				{
					float flBias = Bias( flFraction, 0.2 );
					vecCurrentView.z = flBias * m_vecStart.z + ( 1.0 - flBias ) * m_vecDest.z;
				}
				break;
			}

			m_pPlayer->SetViewOffset( vecCurrentView );
		}		
	}

	bool IsFinished( void )
	{
		return ( gpGlobals->curtime > m_flEndTime || m_pPlayer == NULL );
	}

private:
	CBasePlayer *m_pPlayer;
	Vector		m_vecStart;
	Vector		m_vecDest;
	float		m_flEndTime;
	float		m_flLength;
	ViewAnimationType m_eViewAnimType;
	bool		m_bActive;
};

//=============================================================================
//
// Tables.
//

// Client specific.
#ifdef CLIENT_DLL

	EXTERN_RECV_TABLE( DT_TFPlayerShared );

// Server specific.
#else

	EXTERN_SEND_TABLE( DT_TFPlayerShared );

#endif

enum
{
	MELEE_NOCRIT = 0,
	MELEE_MINICRIT = 1,
	MELEE_CRIT = 2,
};

struct stun_struct_t
{
	CHandle<CTFPlayer> hPlayer;
	float flDuration;
	float flExpireTime;
	float flStartFadeTime;
	float flStunAmount;
	int	iStunFlags;
};

//=============================================================================

#define PERMANENT_CONDITION		-1

#define MOVEMENTSTUN_PARITY_BITS	2

// Damage storage for crit multiplier calculation
class CTFDamageEvent
{
#ifdef CLIENT_DLL
	// This redundantly declares friendship which leads to gcc warnings.
	//DECLARE_CLIENTCLASS_NOBASE();
#else
public:
	// This redundantly declares friendship which leads to gcc warnings.
	//DECLARE_SERVERCLASS_NOBASE();
#endif
	DECLARE_EMBEDDED_NETWORKVAR();

public:
	float	flDamage;
	float	flDamageCritScaleMultiplier;		// scale the damage by this amount when taking it into consideration for "should I crit?" calculations
	float	flTime;
	int		nDamageType;
	byte	nKills;
};

// Condition Provider
struct condition_source_t
{
	DECLARE_EMBEDDED_NETWORKVAR();
	DECLARE_CLASS_NOBASE( condition_source_t );

	condition_source_t()
	{
		m_nPreventedDamageFromCondition = 0;
		m_flExpireTime = 0.f;
		m_pProvider = NULL;
		m_bPrevActive = false;
	}

	int	m_nPreventedDamageFromCondition;
	float	m_flExpireTime;
	CNetworkHandle( CBaseEntity, m_pProvider );
	bool	m_bPrevActive;
};

//=============================================================================
//
// Shared player class.
//
class CTFPlayerShared
{
public:

// Client specific.
#ifdef CLIENT_DLL

	friend class C_TFPlayer;
	typedef C_TFPlayer OuterClass;
	DECLARE_PREDICTABLE();

// Server specific.
#else

	friend class CTFPlayer;
	typedef CTFPlayer OuterClass;

#endif
	
	DECLARE_EMBEDDED_NETWORKVAR()
	DECLARE_CLASS_NOBASE( CTFPlayerShared );

private:
	// Condition Provider tracking
	CUtlVector< condition_source_t > m_ConditionData;

public:

	// Initialization.
	CTFPlayerShared();
	~CTFPlayerShared();
	void Init( OuterClass *pOuter );
	void Spawn( void );

	// State (TF_STATE_*).
	int		GetState() const					{ return m_nPlayerState; }
	void	SetState( int nState )				{ m_nPlayerState = nState; }
	bool	InState( int nState )				{ return ( m_nPlayerState == nState ); }

	// Condition (TF_COND_*).
	void	AddCond( ETFCond eCond, float flDuration = PERMANENT_CONDITION, CBaseEntity *pProvider = NULL );
	void	RemoveCond( ETFCond eCond, bool ignore_duration=false );
	bool	InCond( ETFCond eCond ) const;
	bool	WasInCond( ETFCond eCond ) const;
	void	ForceRecondNextSync( ETFCond eCond );
	void	RemoveAllCond();
	void	OnConditionAdded( ETFCond eCond );
	void	OnConditionRemoved( ETFCond eCond );
	void	ConditionThink( void );
	float	GetConditionDuration( ETFCond eCond ) const;
	void	SetConditionDuration( ETFCond eCond, float flNewDur )
	{
		Assert( eCond < m_ConditionData.Count() );
		m_ConditionData[eCond].m_flExpireTime = flNewDur;
	}

	CBaseEntity *GetConditionProvider( ETFCond eCond ) const;
	CBaseEntity *GetConditionAssistFromVictim( void );
	CBaseEntity *GetConditionAssistFromAttacker( void );

	void	GetConditionsBits( CBitVec< TF_COND_LAST >& vbConditions ) const;

	void	ConditionGameRulesThink( void );

	void	InvisibilityThink( void );

	void	CheckDisguiseTimer( void );

	int		GetMaxBuffedHealth( bool bIgnoreAttributes = false, bool bIgnoreHealthOverMax = false );

	bool	IsAiming( void );

#ifdef CLIENT_DLL
	// This class only receives calls for these from C_TFPlayer, not
	// natively from the networking system
	virtual void OnPreDataChanged( void );
	virtual void OnDataChanged( void );

	// check the newly networked conditions for changes
	void	SyncConditions( int nPreviousConditions, int nNewConditions, int nForceConditionBits, int nBaseCondBit );

	enum ECritBoostUpdateType { kCritBoost_Ignore, kCritBoost_ForceRefresh };
	void	UpdateCritBoostEffect( ECritBoostUpdateType eUpdateType = kCritBoost_Ignore );
#endif

	bool	IsCritBoosted( void ) const;
	bool	IsInvulnerable( void ) const;
	bool	IsStealthed( void ) const;
	bool	CanBeDebuffed( void ) const;

	void	Disguise( int nTeam, int nClass, CTFPlayer* pDesiredTarget=NULL );
	void	CompleteDisguise( void );
	void	RemoveDisguise( void );
	void	RemoveDisguiseWeapon( void );
	void	FindDisguiseTarget( void );
	int		GetDisguiseTeam( void ) const;
	int		GetDisguiseClass( void ) const			{ return InCond( TF_COND_DISGUISED_AS_DISPENSER ) ? (int)TF_CLASS_ENGINEER : m_nDisguiseClass; }
	int		GetDisguisedSkinOverride( void ) const	{ return m_nDisguiseSkinOverride; }
	int		GetDisguiseMask( void )	const			{ return InCond( TF_COND_DISGUISED_AS_DISPENSER ) ? (int)TF_CLASS_ENGINEER : m_nMaskClass; }
	int		GetDesiredDisguiseClass( void )		{ return m_nDesiredDisguiseClass; }
	int		GetDesiredDisguiseTeam( void )		{ return m_nDesiredDisguiseTeam; }
	bool	WasLastDisguiseAsOwnTeam( void ) const	{ return m_bLastDisguisedAsOwnTeam; }
	int		GetDisguiseTargetIndex( void ) const	{ return m_iDisguiseTargetIndex; }
	EHANDLE GetDisguiseTarget( void ) const
	{
#ifdef CLIENT_DLL
		if ( m_iDisguiseTargetIndex == TF_DISGUISE_TARGET_INDEX_NONE )
			return NULL;
		return cl_entitylist->GetNetworkableHandle( m_iDisguiseTargetIndex );
#else
		return m_hDisguiseTarget.Get();
#endif
	}
	CTFWeaponBase *GetDisguiseWeapon( void )			{ return m_hDisguiseWeapon; }
	int		GetDisguiseHealth( void )			{ return m_iDisguiseHealth; }
	void	SetDisguiseHealth( int iDisguiseHealth );
	int		GetDisguiseMaxHealth( void );
	int		GetDisguiseMaxBuffedHealth( bool bIgnoreAttributes = false, bool bIgnoreHealthOverMax = false );
	void	ProcessDisguiseImpulse( CTFPlayer *pPlayer );
	int		GetDisguiseAmmoCount( void ) { return m_iDisguiseAmmo; }
	void	SetDisguiseAmmoCount( int nValue ) { m_iDisguiseAmmo = nValue; }

#ifdef CLIENT_DLL
	int		GetDisplayedTeam( void ) const;
	void	OnDisguiseChanged( void );

	int		GetTeamTeleporterUsed( void ) { return m_nTeamTeleporterUsed; }
#endif

	int		CalculateObjectCost( CTFPlayer* pBuilder, int iObjectType );

	// Pickup effects, including putting out fires, updating HUD, etc.
	void	HealthKitPickupEffects( int iHealthGiven = 0 );

	bool IsLoser( void );

#ifdef GAME_DLL
	void	DetermineDisguiseWeapon( bool bForcePrimary = false );
	void	Heal( CBaseEntity *pHealer, float flAmount, float flOverhealBonus, float flOverhealDecayMult, bool bDispenserHeal = false, CTFPlayer *pHealScorer = NULL );
	float	StopHealing( CBaseEntity *pHealer );
	void	RecalculateInvuln( bool bInstantRemove = false );
	int		FindHealerIndex( CBaseEntity *pPlayer );
	EHANDLE	GetFirstHealer();

	bool	AddToSpyCloakMeter( float val, bool bForce=false );

	void	AddTmpDamageBonus( float flBonus, float flExpiration );
	float	GetTmpDamageBonus( void ) { return (InCond(TF_COND_TMPDAMAGEBONUS)) ? m_flTmpDamageBonusAmount : 1.0; }

	void	SetTeamTeleporterUsed( int nTeam ){ m_nTeamTeleporterUsed.Set( nTeam ); }
#endif

	CBaseEntity *GetHealerByIndex( int index );
	bool HealerIsDispenser( int index );
	int		GetNumHealers( void ) { return m_nNumHealers; }

	void	Burn( CTFPlayer *pPlayer, float flBurningTime = -1.0f );

	// Weapons.
	CPCWeaponBase *GetActivePCWeapon() const;

	// Utility.
	bool	IsAlly( CBaseEntity *pEntity );

	// Separation force
	bool	IsSeparationEnabled( void ) const	{ return m_bEnableSeparation; }
	void	SetSeparation( bool bEnable )		{ m_bEnableSeparation = bEnable; }
	const Vector &GetSeparationVelocity( void ) const { return m_vSeparationVelocity; }
	void	SetSeparationVelocity( const Vector &vSeparationVelocity ) { m_vSeparationVelocity = vSeparationVelocity; }

	void	FadeInvis( float fAdditionalRateScale );
	float	GetPercentInvisible( void ) const;
	float	GetPercentInvisiblePrevious( void ) { return m_flPrevInvisibility; }
	void	NoteLastDamageTime( int nDamage );
	void	OnSpyTouchedByEnemy( void );
	float	GetLastStealthExposedTime( void ) { return m_flLastStealthExposeTime; }
	void	SetNextStealthTime( float flTime ) { m_flStealthNextChangeTime = flTime; }
	bool	IsFullyInvisible( void ) { return ( GetPercentInvisible() == 1.f ); }

	bool	IsEnteringOrExitingFullyInvisible( void );

	int		GetDesiredPlayerClassIndex( void );

	void	UpdateCloakMeter( void );
	float	GetSpyCloakMeter() const		{ return m_flCloakMeter; }
	void	SetSpyCloakMeter( float val ) { m_flCloakMeter = val; }

	bool	IsJumping( void ) const			{ return m_bJumping; }
	void	SetJumping( bool bJumping );
	bool    IsAirDashing( void ) const		{ return (m_iAirDash > 0); }
	int		GetAirDash( void ) const		{ return m_iAirDash; }
	void    SetAirDash( int iAirDash );
	void	SetAirDucked( int nAirDucked )	{ m_nAirDucked = nAirDucked; }
	int		AirDuckedCount( void )			{ return m_nAirDucked; }
	void	SetDuckTimer( float flTime )	{ m_flDuckTimer = flTime; }
	float	GetDuckTimer( void ) const		{ return m_flDuckTimer; }

	void	DebugPrintConditions( void );
	void	InstantlySniperUnzoom( void );

	float	GetStealthNoAttackExpireTime( void );

	void	SetPlayerDominated( CTFPlayer *pPlayer, bool bDominated );
	bool	IsPlayerDominated( int iPlayerIndex );
	bool	IsPlayerDominatingMe( int iPlayerIndex );
	void	SetPlayerDominatingMe( CTFPlayer *pPlayer, bool bDominated );

	float	GetDisguiseCompleteTime( void ) { return m_flDisguiseCompleteTime; }
	bool	IsSpyDisguisedAsMyTeam( CTFPlayer *pPlayer );

	// Stuns
	stun_struct_t *GetActiveStunInfo( void ) const;
#ifdef GAME_DLL
	void	StunPlayer( float flTime, float flReductionAmount, int iStunFlags = TF_STUN_MOVEMENT, CTFPlayer* pAttacker = NULL );
#endif // GAME_DLL
	float	GetAmountStunned( int iStunFlags );
	bool	IsLoserStateStunned( void ) const;
	bool	IsControlStunned( void );
	bool	IsSnared( void );
	CTFPlayer *GetStunner( void );
	void	ControlStunFading( void );
	int		GetStunFlags( void ) const		{ return GetActiveStunInfo() ? GetActiveStunInfo()->iStunFlags : 0; }
	float	GetStunExpireTime( void ) const { return GetActiveStunInfo() ? GetActiveStunInfo()->flExpireTime : 0; }
	void	SetStunExpireTime( float flTime );
	void	UpdateLegacyStunSystem( void );

	CTFPlayer *GetAssist( void ) const			{ return m_hAssist; }
	void	SetAssist( CTFPlayer* newAssist )	{ m_hAssist = newAssist; }

	void SetCloakConsumeRate( float newCloakConsumeRate ) { m_fCloakConsumeRate = newCloakConsumeRate; }
	void SetCloakRegenRate( float newCloakRegenRate ) { m_fCloakRegenRate = newCloakRegenRate; }

	int		GetDisguiseBody( void ) const	{ return m_iDisguiseBody; }
	void	SetDisguiseBody( int iVal )		{ m_iDisguiseBody = iVal; }

	bool	IsCarryingObject( void )		const { return m_bCarryingObject; }
	CBaseObject* GetCarriedObject( void )	const { return m_hCarriedObject.Get(); }
	void	SetCarriedObject( CBaseObject* pObj );
	void	StartBuildingObjectOfType( int iType, int iObjectMode=0 );

	int GetSequenceForDeath( CBaseAnimating* pRagdoll, bool bBurning, int nCustomDeath );

	void IncrementRespawnTouchCount() { ++m_iSpawnRoomTouchCount; }
	void DecrementRespawnTouchCount() { m_iSpawnRoomTouchCount = Max( m_iSpawnRoomTouchCount - 1, 0 ); }
	int GetRespawnTouchCount() const { return m_iSpawnRoomTouchCount; }
private:

	void ImpactWaterTrace( trace_t &trace, const Vector &vecStart );

	void OnAddStealthed( void );
	void OnAddInvulnerable( void );
	void OnAddTeleported( void );
	void OnAddBurning( void );
	void OnAddDisguising( void );
	void OnAddDisguised( void );
	void OnAddDemoCharge( void );
	void OnAddCritBoost( void );
	void OnAddSodaPopperHype( void );
	void OnAddOverhealed( void );
	void OnAddFeignDeath( void );
	void OnAddStunned( void );
	void OnAddPhase( void );
	void OnAddUrine( void );
	void OnAddMarkedForDeath( void );
	void OnAddBleeding( void );
	void OnAddDefenseBuff( void );
	void OnAddOffenseBuff( void );
	void OnAddOffenseHealthRegenBuff( void );
	const char* GetSoldierBuffEffectName( void );
	void OnAddSoldierOffensiveBuff( void );
	void OnAddSoldierDefensiveBuff( void );
	void OnAddSoldierOffensiveHealthRegenBuff( void );
	void OnAddSoldierNoHealingDamageBuff( void );
	void OnAddShieldCharge( void );
	void OnAddDemoBuff( void );
	void OnAddEnergyDrinkBuff( void	);
	void OnAddRadiusHeal( void );
	void OnAddMegaHeal( void );
	void OnAddMadMilk( void );
	void OnAddTaunting( void );
	void OnAddNoHealingDamageBuff( void );
	void OnAddSpeedBoost( bool IsNonCombat );
	void OnAddSapped( void );
	void OnAddReprogrammed( void );
	void OnAddMarkedForDeathSilent( void );
	void OnAddDisguisedAsDispenser( void );
	void OnAddHalloweenBombHead( void );
	void OnAddHalloweenThriller( void );
	void OnAddRadiusHealOnDamage( void );
	void OnAddMedEffectUberBulletResist( void );
	void OnAddMedEffectUberBlastResist( void );
	void OnAddMedEffectUberFireResist( void );
	void OnAddMedEffectSmallBulletResist( void );
	void OnAddMedEffectSmallBlastResist( void );
	void OnAddMedEffectSmallFireResist( void );
	void OnAddStealthedUserBuffFade( void );
	void OnAddBulletImmune( void );
	void OnAddBlastImmune( void );
	void OnAddFireImmune( void );
	void OnAddMVMBotRadiowave( void );
	void OnAddHalloweenSpeedBoost( void );
	void OnAddHalloweenQuickHeal( void );
	void OnAddHalloweenGiant( void );
	void OnAddHalloweenTiny( void );
	void OnAddHalloweenGhostMode( void );
	void OnAddHalloweenKartDash( void );
	void OnAddHalloweenKart( void );
	void OnAddBalloonHead( void );
	void OnAddMeleeOnly( void );
	void OnAddSwimmingCurse( void );
	void OnAddHalloweenKartCage( void );
	void OnAddRuneResist( void );
	void OnAddGrapplingHookLatched( void );
	void OnAddPasstimeInterception( void );
	void OnAddRunePlague( void );
	void OnAddPlague( void );
	void OnAddKingBuff( void );
	void OnAddInPurgatory( void );
	void OnAddCompetitiveWinner( void );
	void OnAddCompetitiveLoser( void );

	void OnRemoveZoomed( void );
	void OnRemoveBurning( void );
	void OnRemoveStealthed( void );
	void OnRemoveDisguised( void );
	void OnRemoveDisguising( void );
	void OnRemoveInvulnerable( void );
	void OnRemoveTeleported( void );
	void OnRemoveDemoCharge( void );
	void OnRemoveCritBoost( void );
	void OnRemoveSodaPopperHype( void );
	void OnRemoveTmpDamageBonus( void );
	void OnRemoveOverhealed( void );
	void OnRemoveFeignDeath( void );
	void OnRemoveStunned( void );
	void OnRemovePhase( void );
	void OnRemoveUrine( void );
	void OnRemoveMarkedForDeath( void );
	void OnRemoveBleeding( void );
	void OnRemoveInvulnerableWearingOff( void );
	void OnRemoveDefenseBuff( void );
	void OnRemoveOffenseBuff( void );
	void OnRemoveOffenseHealthRegenBuff( void );
	void OnRemoveSoldierOffensiveBuff( void );
	void OnRemoveSoldierDefensiveBuff( void );
	void OnRemoveSoldierOffensiveHealthRegenBuff( void );
	void OnRemoveSoldierNoHealingDamageBuff( void );
	void OnRemoveShieldCharge( void );
	void OnRemoveDemoBuff( void );
	void OnRemoveEnergyDrinkBuff( void );
	void OnRemoveRadiusHeal( void );
	void OnRemoveMegaHeal( void );
	void OnRemoveMadMilk( void );
	void OnRemoveTaunting( void );
	void OnRemoveNoHealingDamageBuff( void );
	void OnRemoveSpeedBoost( bool IsNonCombat );
	void OnRemoveSapped( void );
	void OnRemoveReprogrammed( void );
	void OnRemoveMarkedForDeathSilent( void );
	void OnRemoveDisguisedAsDispenser( void );
	void OnRemoveHalloweenBombHead( void );
	void OnRemoveHalloweenThriller( void );
	void OnRemoveRadiusHealOnDamage( void );
	void OnRemoveMedEffectUberBulletResist( void );
	void OnRemoveMedEffectUberBlastResist( void );
	void OnRemoveMedEffectUberFireResist( void );
	void OnRemoveMedEffectSmallBulletResist( void );
	void OnRemoveMedEffectSmallBlastResist( void );
	void OnRemoveMedEffectSmallFireResist( void );
	void OnRemoveStealthedUserBuffFade( void );
	void OnRemoveBulletImmune( void );
	void OnRemoveBlastImmune( void );
	void OnRemoveFireImmune( void );
	void OnRemoveMVMBotRadiowave( void );
	void OnRemoveHalloweenSpeedBoost( void );
	void OnRemoveHalloweenQuickHeal( void );
	void OnRemoveHalloweenGiant( void );
	void OnRemoveHalloweenTiny( void );
	void OnRemoveHalloweenGhostMode( void );
	void OnRemoveHalloweenKartDash( void );
	void OnRemoveHalloweenKart( void );
	void OnRemoveBalloonHead( void );
	void OnRemoveMeleeOnly( void );
	void OnRemoveSwimmingCurse( void );
	void OnRemoveHalloweenKartCage( void );
	void OnRemoveRuneResist( void );
	void OnRemoveGrapplingHookLatched( void );
	void OnRemovePasstimeInterception( void );
	void OnRemoveRunePlague( void );
	void OnRemovePlague( void );
	void OnRemoveRuneKing( void );
	void OnRemoveKingBuff( void );
	void OnRemoveRuneSupernova( void );
	void OnRemoveInPurgatory( void );
	void OnRemoveCompetitiveWinner( void );
	void OnRemoveCompetitiveLoser( void );

	// Starting a new trend, putting Add and Remove next to each other
	void OnAddCondParachute( void );
	void OnRemoveCondParachute( void );

	float GetCritMult( void );

#ifdef GAME_DLL
	void  UpdateCritMult( void );
	void  RecordDamageEvent( const CTakeDamageInfo &info, bool bKill, int nVictimPrevHealth );
	void  AddTempCritBonus( float flAmount );
	void  ClearDamageEvents( void ) { m_DamageEvents.Purge(); }
	int	  GetNumKillsInTime( float flTime );

	// Invulnerable.
	bool  IsProvidingInvuln( CTFPlayer *pPlayer );
	void  SetInvulnerable( bool bState, bool bInstant = false );
#endif

private:

	// Vars that are networked.
	CNetworkVar( int, m_nPlayerState );			// Player state.
	CNetworkVar( int, m_nPlayerCond );			// Player condition flags.
	CNetworkVar( int, m_nPlayerCondEx );		// Player condition flags (extended -- we overflowed 32 bits).
	CNetworkVar( int, m_nPlayerCondEx2 );		// Player condition flags (extended -- we overflowed 64 bits).
	CNetworkVar( int, m_nPlayerCondEx3 );		// Player condition flags (extended -- we overflowed 96 bits).

	CNetworkVarEmbedded( CTFConditionList, m_ConditionList );

//TFTODO: What if the player we're disguised as leaves the server?
//...maybe store the name instead of the index?
	CNetworkVar( int, m_nDisguiseTeam );		// Team spy is disguised as.
	CNetworkVar( int, m_nDisguiseClass );		// Class spy is disguised as.
	CNetworkVar( int, m_nDisguiseSkinOverride ); // skin override value of the player spy disguised as.
	CNetworkVar( int, m_nMaskClass );
#ifdef GAME_DLL
	EHANDLE m_hDisguiseTarget;					// Playing the spy is using for name disguise.
#endif // GAME_DLL
	CNetworkVar( int, m_iDisguiseTargetIndex );
	CNetworkVar( int, m_iDisguiseHealth );		// Health to show our enemies in player id
	CNetworkVar( int, m_nDesiredDisguiseClass );
	CNetworkVar( int, m_nDesiredDisguiseTeam );
	CNetworkHandle( CTFWeaponBase, m_hDisguiseWeapon );
	CNetworkVar( int, m_nTeamTeleporterUsed ); // for disguised spies using enemy teleporters
	CHandle<CTFPlayer>	m_hDesiredDisguiseTarget;
	int m_iDisguiseAmmo;

	bool m_bEnableSeparation;		// Keeps separation forces on when player stops moving, but still penetrating
	Vector m_vSeparationVelocity;	// Velocity used to keep player seperate from teammates

	float m_flInvisibility;
	float m_flPrevInvisibility;
	CNetworkVar( float, m_flInvisChangeCompleteTime );		// when uncloaking, must be done by this time
	float m_flLastStealthExposeTime;
	float m_fCloakConsumeRate;
	float m_fCloakRegenRate;

	CNetworkVar( int, m_nNumHealers );

	// Vars that are not networked.
	OuterClass			*m_pOuter;					// C_TFPlayer or CTFPlayer (client/server).

#ifdef GAME_DLL
	// Healer handling
	struct healers_t
	{
		EHANDLE	pHealer;
		float	flAmount;
		float   flHealAccum;
		float	flOverhealBonus;
		float	flOverhealDecayMult;
		bool	bDispenserHeal;
		EHANDLE pHealScorer;
		int		iKillsWhileBeingHealed; // for engineer achievement ACHIEVEMENT_TF_ENGINEER_TANK_DAMAGE
		float	flHealedLastSecond;
	};
	CUtlVector< healers_t >	m_aHealers;
	float					m_flHealFraction;	// Store fractional health amounts
	float					m_flDisguiseHealFraction;	// Same for disguised healing
	float					m_flBestOverhealDecayMult;
	float					m_flHealedPerSecondTimer;

	float m_flInvulnerableOffTime;
#endif

	CNetworkVar( bool, m_bLastDisguisedAsOwnTeam );

	// Burn handling
	CHandle<CTFPlayer>		m_hBurnAttacker;
	CNetworkVar( int,		m_nNumFlames );
	float					m_flFlameBurnTime;
	float					m_flFlameRemoveTime;


	float					m_flDisguiseCompleteTime;
	float					m_flTmpDamageBonusAmount;

#ifdef CLIENT_DLL
	bool m_bSyncingConditions;
#endif
	int	m_nOldConditions;
	int m_nOldConditionsEx;
	int m_nOldConditionsEx2;
	int m_nOldConditionsEx3;
	int	m_nOldDisguiseClass;
	int	m_nOldDisguiseTeam;

	int	m_nForceConditions;
	int m_nForceConditionsEx;
	int m_nForceConditionsEx2;
	int m_nForceConditionsEx3;

	CNetworkVar( int, m_iDesiredPlayerClass );

	float m_flNextBurningSound;

	CNetworkVar( float, m_flCloakMeter );	// [0,100]

	// Movement.
	CNetworkVar( bool, m_bJumping );
	CNetworkVar( int,  m_iAirDash );
	CNetworkVar( int, m_nAirDucked );
	CNetworkVar( float, m_flDuckTimer );

	CNetworkVar( float, m_flStealthNoAttackExpire );
	CNetworkVar( float, m_flStealthNextChangeTime );

	CNetworkVar( int, m_iCritMult );

	CNetworkArray( bool, m_bPlayerDominated, MAX_PLAYERS+1 );		// array of state per other player whether player is dominating other players
	CNetworkArray( bool, m_bPlayerDominatingMe, MAX_PLAYERS+1 );	// array of state per other player whether other players are dominating this player

	CNetworkVar( float, m_flMovementStunTime );
	CNetworkVar( int, m_iMovementStunAmount );
	CNetworkVar( unsigned char, m_iMovementStunParity );
	CNetworkHandle( CTFPlayer, m_hStunner );
	CNetworkVar( int, m_iStunFlags );
	CNetworkVar( int, m_iStunIndex );

	CNetworkHandle( CBaseObject, m_hCarriedObject );
	CNetworkVar( bool, m_bCarryingObject );

	CHandle<CTFPlayer>	m_hAssist;

	CNetworkVar( int,  m_iDisguiseBody );

	CNetworkVar( int,  m_iSpawnRoomTouchCount );

#ifdef GAME_DLL
	float	m_flNextCritUpdate;
	CUtlVector<CTFDamageEvent> m_DamageEvents;
#else
	unsigned char m_iOldMovementStunParity;
	CSoundPatch	*m_pCritBoostSoundLoop;
#endif

public:
	float	m_flStunFade;
	float	m_flStunEnd;
	float	m_flStunMid;
	int		m_iStunAnimState;
	
	// Movement stun state.
	bool		m_bStunNeedsFadeOut;
	float		m_flStunLerpTarget;
	float		m_flLastMovementStunChange;
#ifdef GAME_DLL
	CUtlVector <stun_struct_t> m_PlayerStuns;
#else
	stun_struct_t m_ActiveStunInfo;
#endif // CLIENT_DLL

	float	m_flLastNoMovementTime;

	//////////////////////////////// Day Of Defeat ////////////////////////////////
public:
	void	SetStamina( float stamina );
	float	GetStamina( void ) { return m_flStamina; }

	bool	IsProne() const;
	bool	IsGettingUpFromProne() const;	
	bool	IsGoingProne() const;
	void	SetProne( bool bProne, bool bNoAnimation = false );

	bool	IsBazookaDeployed( void ) const;
	bool	IsBazookaOnlyDeployed( void ) const;
	bool	IsSniperZoomed( void ) const;
	bool	IsInMGDeploy( void ) const;
	bool	IsProneDeployed( void ) const;
	bool	IsSandbagDeployed( void ) const;
	bool	IsDucking( void ) const;

	void	SetDeployed( bool bDeployed, float flHeight = -1 );

	QAngle	GetDeployedAngles( void ) const;
	float	GetDeployedHeight( void ) const;

	void	SetDeployedYawLimits( float flLeftYaw, float flRightYaw );
	void	ClampDeployedAngles( QAngle *vecTestAngles );

	void	SetSlowedTime( float t );
	float	GetSlowedTime( void ) const;

	void	StartGoingProne( void );
	void	StandUpFromProne( void );

	bool	CanChangePosition( void );

	bool	IsSprinting( void ) { return m_bIsSprinting; }

	void	ForceUnzoom( void );

	void	SetSprinting( bool bSprinting );
	void	StartSprinting( void );
	void	StopSprinting( void );

	void	SetLastViewAnimTime( float flTime );
	float	GetLastViewAnimTime( void );

	void	ViewAnimThink( void );

	void	ResetViewOffsetAnimation( void );
	void	ViewOffsetAnimation( Vector vecDest, float flTime, ViewAnimationType type );

	void	ResetSprintPenalty( void );

	void	ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs );

	float m_flNextProneCheck; // Prevent it switching their prone state constantly.

	QAngle m_vecDeployedAngles;
	//float m_flDeployedHeight;
	CNetworkVar( float, m_flDeployedHeight );

	CNetworkVar( float, m_flUnProneTime );
	CNetworkVar( float, m_flGoProneTime );

	CNetworkVar( float, m_flDeployChangeTime );
	
	CNetworkVar( bool, m_bForceProneChange );

	float m_flLastViewAnimationTime;

	CViewOffsetAnimation *m_pViewOffsetAnim;
private:
	CNetworkVar( bool, m_bProne );

	CNetworkVar( float, m_flStamina );

	CNetworkVar( float, m_flSlowedUntilTime );

	CNetworkVar( bool, m_bIsSprinting );

	CNetworkVar( float, m_flDeployedYawLimitLeft );
	CNetworkVar( float, m_flDeployedYawLimitRight );

	CNetworkVar( bool, m_bPlanting );
	CNetworkVar( bool, m_bDefusing );

	bool m_bGaveSprintPenalty;
};

// Entity Messages
#define DOD_PLAYER_POP_HELMET		1
#define DOD_PLAYER_REMOVE_DECALS	2

	//////////////////////////////// END OF	///////////////////////////////////////

#define TF_DEATH_DOMINATION				0x0001	// killer is dominating victim
#define TF_DEATH_ASSISTER_DOMINATION	0x0002	// assister is dominating victim
#define TF_DEATH_REVENGE				0x0004	// killer got revenge on victim
#define TF_DEATH_ASSISTER_REVENGE		0x0008	// assister got revenge on victim

#define CONTROL_STUN_ANIM_TIME	1.5f
#define STUN_ANIM_NONE	0
#define STUN_ANIM_LOOP	1
#define STUN_ANIM_END	2

extern const char *g_pszBDayGibs[22];

class CTraceFilterIgnoreTeammatesAndTeamObjects : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterIgnoreTeammatesAndTeamObjects, CTraceFilterSimple );

	CTraceFilterIgnoreTeammatesAndTeamObjects( const IHandleEntity *passentity, int collisionGroup, int iIgnoreTeam )
		: CTraceFilterSimple( passentity, collisionGroup ), m_iIgnoreTeam( iIgnoreTeam ) {}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask );

private:
	int m_iIgnoreTeam;
};

enum { kSoldierBuffCount = 6 };
extern ETFCond g_SoldierBuffAttributeIDToConditionMap[kSoldierBuffCount + 1];

class CTFPlayerSharedUtils
{
public:
	static CTFWeaponBuilder *GetBuilderForObjectType( CTFPlayer *pTFPlayer, int iObjectType );
};

class CTargetOnlyFilter : public CTraceFilterSimple
{
public:
	CTargetOnlyFilter( CBaseEntity *pShooter, CBaseEntity *pTarget );
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
	CBaseEntity	*m_pShooter;
	CBaseEntity	*m_pTarget;
};

#endif // TF_PLAYER_SHARED_H
