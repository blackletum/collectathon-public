//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//
//
// Health.cpp
//
// implementation of CCSHudHealth class
//
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#define PAIN_NAME "sprites/%d_pain.vmt"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>

using namespace vgui;

#include "hudelement.h"
#include "hud_numericdisplay.h"
//#include "cs_gamerules.h"

#include "convar.h"

#include "c_tf_player.h"

//-----------------------------------------------------------------------------
// Purpose: Health panel
//-----------------------------------------------------------------------------
class CCSHudHealth : public CHudElement, public CHudNumericDisplay
{
	DECLARE_CLASS_SIMPLE( CCSHudHealth, CHudNumericDisplay );

public:
	CCSHudHealth( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();

	virtual void Paint( void );
	virtual void ApplySchemeSettings( IScheme *scheme );

	virtual int  GetPanelTypeReference( void ) { return PANEL_CS; } // TF is considered default

private:
	// old variables
	int		m_iHealth;

	int		m_bitsDamage;

	CHudTexture *m_pHealthIcon;

	CPanelAnimationVarAliasType( float, icon_xpos, "icon_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, icon_ypos, "icon_ypos", "0", "proportional_float" );

//	CPanelAnimationVar( Color, m_LowHealthColor, "LowHealthColor", "255 0 0 255" );

	float icon_tall;
	float icon_wide;

};

DECLARE_HUDELEMENT( CCSHudHealth );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSHudHealth::CCSHudHealth( const char *pElementName ) : CHudElement( pElementName ), CHudNumericDisplay(NULL, "CSHudHealth")
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSHudHealth::Init()
{
	m_iHealth		= 100;
	m_bitsDamage	= 0;
	icon_tall		= 0;
	icon_wide		= 0;
	SetIndent(true);
	SetDisplayValue(m_iHealth);
}

void CCSHudHealth::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	if( !m_pHealthIcon )
	{
		m_pHealthIcon = gHUD.GetIcon( "cs_health_icon" );
	}

	if( m_pHealthIcon )
	{

		icon_tall = GetTall() - YRES(2);
		float scale = icon_tall / (float)m_pHealthIcon->Height();
		icon_wide = ( scale ) * (float)m_pHealthIcon->Width();
	}
}

//-----------------------------------------------------------------------------
// Purpose: reset health to normal color at round restart
//-----------------------------------------------------------------------------
void CCSHudHealth::Reset()
{
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("CSHealthRestored");
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSHudHealth::VidInit()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSHudHealth::OnThink()
{
	int realHealth = 0;
	C_BasePlayer *local = C_BasePlayer::GetLocalPlayer();

	if ( local && !ToTFPlayer( local )->IsCSClass() )
	{
		SetPaintEnabled( false );
		SetPaintBackgroundEnabled( false );
		return;
	}

	SetPaintEnabled( true );
	SetPaintBackgroundEnabled( true );

	if ( local )
	{
		// Never below zero
		realHealth = MAX( local->GetHealth(), 0 );
	}

	// Only update the fade if we've changed health
	if ( realHealth == m_iHealth )
	{
		return;
	}

	if( realHealth > m_iHealth && realHealth > 25 )
	{
		// Health above threshold
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("CSHealthRestored");
	}
	else if ( realHealth <= 25 )
	{
		// we are badly injured
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("CSHealthLow");
	}
	else if( realHealth < m_iHealth && ToTFPlayer( local )->GetMaxHealth() > realHealth )
	{
		// took a hit
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("CSHealthTookDamage");
	}

	m_iHealth = realHealth;

	SetDisplayValue(m_iHealth);
}

void CCSHudHealth::Paint( void )
{
	if( m_pHealthIcon )
	{
		m_pHealthIcon->DrawSelf( icon_xpos, icon_ypos, icon_wide, icon_tall, GetFgColor() );
	}

	//draw the health icon
	BaseClass::Paint();
}