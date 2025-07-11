//========= Copyright � 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_HUD_PLAYERSTATUS_H
#define TF_HUD_PLAYERSTATUS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/ImagePanel.h>
#include "tf_controls.h"
#include "tf_imagepanel.h"
#include "GameEventListener.h"

class C_TFPlayer;
//class CTFPlayerModelPanel;
namespace vgui
{
	class Label;
}

// Initilise but hide Live hud related objects we currently don't support
#define TEMP_CALLS

//-----------------------------------------------------------------------------
// Purpose:  
//-----------------------------------------------------------------------------
class CTFClassImage : public vgui::ImagePanel
{
public:
	DECLARE_CLASS_SIMPLE( CTFClassImage, vgui::ImagePanel );

	CTFClassImage( vgui::Panel *parent, const char *name ) : ImagePanel( parent, name )
	{
	}

	void SetClass( int iTeam, int iClass, int iCloakstate );
};

//-----------------------------------------------------------------------------
// Purpose:  Displays player class data
//-----------------------------------------------------------------------------
class CTFHudPlayerClass : public vgui::EditablePanel, public CGameEventListener
{
	DECLARE_CLASS_SIMPLE( CTFHudPlayerClass, EditablePanel );

public:

	CTFHudPlayerClass( Panel *parent, const char *name );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Reset();

public: // IGameEventListener Interface
	virtual void FireGameEvent( IGameEvent * event );

protected:

	virtual void OnThink();

private:

	float				m_flNextThink;

	CTFClassImage		*m_pClassImage;
	CTFImagePanel		*m_pClassImageBG;
	CTFImagePanel		*m_pSpyImage; // used when spies are disguised
	CTFImagePanel		*m_pSpyOutlineImage;

#ifdef TEMP_CALLS
//	CTFPlayerModelPanel	*m_pPlayerModelPanel;
	CTFImagePanel		*m_pPlayerModelPanelBG;
	EditablePanel		*m_pCarryingWeaponPanel;
	CExLabel			*m_pCarryingLabel;
	vgui::Label			*m_pCarryingOwnerLabel;
	CTFImagePanel		*m_pCarryingBG;
#endif

	int					m_nTeam;
	int					m_nClass;
	int					m_nDisguiseTeam;
	int					m_nDisguiseClass;
	EHANDLE				m_hDisguiseWeapon;
	int					m_nCloakLevel;
};

//-----------------------------------------------------------------------------
// Purpose:  Clips the health image to the appropriate percentage
//-----------------------------------------------------------------------------
class CTFHealthPanel : public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CTFHealthPanel, vgui::Panel );

	CTFHealthPanel( vgui::Panel *parent, const char *name );
	virtual void Paint();
	void SetHealth( float flHealth ){ m_flHealth = ( flHealth <= 1.0 ) ? flHealth : 1.0f; }

private:

	float	m_flHealth; // percentage from 0.0 -> 1.0
	int		m_iMaterialIndex;
	int		m_iDeadMaterialIndex;
};

enum BuffClass_t
{
	BUFF_CLASS_BULLET_RESIST,
	BUFF_CLASS_BLAST_RESIST,
	BUFF_CLASS_FIRE_RESIST,
	BUFF_CLASS_SOLDIER_OFFENSE,
	BUFF_CLASS_SOLDIER_DEFENSE,
	BUFF_CLASS_SOLDIER_HEALTHONHIT,
	DEBUFF_CLASS_STUNNED,
	DEBUFF_CLASS_SPY_MARKED,
	BUFF_CLASS_PARACHUTE,
	RUNE_CLASS_STRENGTH,
	RUNE_CLASS_HASTE,
	RUNE_CLASS_REGEN,
	RUNE_CLASS_RESIST,
	RUNE_CLASS_VAMPIRE,
	RUNE_CLASS_REFLECT,
	RUNE_CLASS_PRECISION,
	RUNE_CLASS_AGILITY,
	RUNE_CLASS_KNOCKOUT,
	RUNE_CLASS_KING,
	RUNE_CLASS_PLAGUE,
	RUNE_CLASS_SUPERNOVA,
	DEBUFF_CLASS_SLOWED,
};

struct CTFBuffInfo
{
public:
	CTFBuffInfo( ETFCond eCond, BuffClass_t eClass, vgui::ImagePanel* pPanel, const char* pzsBlueImage = NULL, const char* pzsRedImage = NULL )
	{
		m_eCond = eCond;
		m_eClass = eClass;
		m_pImagePanel = pPanel;
		m_pzsRedImage = pzsRedImage;
		m_pzsBlueImage = pzsBlueImage;
	}

	void Update( C_TFPlayer *pPlayer );

	ETFCond	m_eCond;
	BuffClass_t m_eClass;
	vgui::ImagePanel	*m_pImagePanel;
	const char* m_pzsRedImage;
	const char* m_pzsBlueImage;
};

//-----------------------------------------------------------------------------
// Purpose:  Displays player health data
//-----------------------------------------------------------------------------
class CTFHudPlayerHealth : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTFHudPlayerHealth, EditablePanel );

public:

	CTFHudPlayerHealth( Panel *parent, const char *name );
	~CTFHudPlayerHealth();

	virtual const char *GetResFilename( void ) { return "resource/UI/HudPlayerHealth.res"; }
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Reset();

	void	SetHealth( int iNewHealth, int iMaxHealth, int iMaxBuffedHealth );
	void	HideHealthBonusImage( void );
	void	SetBuilding( bool bBuilding ) { m_bBuilding = bBuilding; }
	void	SetAllowAnimations( bool bValue ) { m_bAnimate = bValue; }

protected:

	virtual void OnThink();

protected:
	float				m_flNextThink;

private:
	CTFHealthPanel		*m_pHealthImage;
	vgui::ImagePanel	*m_pHealthBonusImage;
	vgui::ImagePanel	*m_pHealthImageBG;
	vgui::ImagePanel	*m_pBuildingHealthImageBG;

#ifdef TEMP_CALLS
	vgui::ImagePanel	*m_pBleedImage;
	vgui::ImagePanel	*m_pHookBleedImage;
	vgui::ImagePanel	*m_pMilkImage;
	vgui::ImagePanel	*m_pMarkedForDeathImage;
	vgui::ImagePanel	*m_pMarkedForDeathImageSilent;
	vgui::ImagePanel	*m_pWheelOfDoomImage;
	vgui::ImagePanel	*m_pGasImage;
	CExLabel			*m_pPlayerLevelLabel;
#endif

	CUtlVector<CTFBuffInfo*> m_vecBuffInfo;

	int					m_nHealth;
	int					m_nMaxHealth;

	int					m_nBonusHealthOrigX;
	int					m_nBonusHealthOrigY;
	int					m_nBonusHealthOrigW;
	int					m_nBonusHealthOrigH;

	bool				m_bBuilding;
	int					m_iAnimState;
	bool				m_bAnimate;

	CPanelAnimationVar( int, m_nHealthBonusPosAdj, "HealthBonusPosAdj", "25" );
	CPanelAnimationVar( float, m_flHealthDeathWarning, "HealthDeathWarning", "0.49" );
	CPanelAnimationVar( Color, m_clrHealthDeathWarningColor, "HealthDeathWarningColor", "HUDDeathWarning" );
};

//-----------------------------------------------------------------------------
// Purpose:  Parent panel for the player class/health displays
//-----------------------------------------------------------------------------
class CTFHudPlayerStatus : public CHudElement, public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTFHudPlayerStatus, vgui::EditablePanel );

public:
	CTFHudPlayerStatus( const char *pElementName );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Reset();
	virtual bool ShouldDraw( void ) OVERRIDE;
private:

	CTFHudPlayerClass	*m_pHudPlayerClass;
	CTFHudPlayerHealth	*m_pHudPlayerHealth;
};

#endif	// TF_HUD_PLAYERSTATUS_H