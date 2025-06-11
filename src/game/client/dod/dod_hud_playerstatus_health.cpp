//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//

#include "cbase.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>

#include "convar.h"
#include "c_tf_team.h"
#include "c_tf_playerresource.h"
#include "c_tf_player.h"

#include "dod_hud_playerstatus_health.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDoDHudHealthBar::CDoDHudHealthBar( vgui::Panel *parent, const char *name ) : vgui::ImagePanel( parent, name )
{
	m_flPercentage = 1.0f;

	m_iMaterialIndex = vgui::surface()->DrawGetTextureId( "vgui/white" );
	if ( m_iMaterialIndex == -1 ) // we didn't find it, so create a new one
	{
		m_iMaterialIndex = vgui::surface()->CreateNewTextureID();	
	}

	vgui::surface()->DrawSetTextureFile( m_iMaterialIndex, "vgui/white", true, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudHealthBar::OnThink()
{
	BaseClass::OnThink();

	C_TFPlayer *pPlayer = GetHealthDelegatePlayer();
	if ( pPlayer )
	{
		// m_nHealth >= 0 
		int nHealth = MAX( pPlayer->GetHealth(), 0 );
		m_flPercentage = nHealth / 100.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudHealthBar::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_clrHealthHigh = pScheme->GetColor( "DOD_HudHealthGreen", GetFgColor() );
	m_clrHealthMed = pScheme->GetColor( "DOD_HudHealthYellow", GetFgColor() );
	m_clrHealthLow = pScheme->GetColor( "DOD_HudHealthRed", GetFgColor() );
	m_clrHealthOverHeal = pScheme->GetColor( "DOD_HudHealthOverHeal", GetFgColor() );
	m_clrBackground = pScheme->GetColor( "DOD_HudHealthBG", GetBgColor() );
	m_clrBorder = pScheme->GetColor( "DOD_HudHealthBorder", GetBgColor() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudHealthBar::Paint( void )
{
	BaseClass::Paint();

	int x, y, w, h;
	GetBounds( x, y, w, h );

	int xpos = 0, ypos = 0;
	float flDamageY = h * ( 1.0f - m_flPercentage );
	float flOverHealY = h * ( 1.0f - 2.0f * ( m_flPercentage - 1.0f ) );

	Color *pclrHealth;

	if ( m_flPercentage > m_flFirstWarningLevel )
	{
		pclrHealth = &m_clrHealthHigh; 
	}
	else if ( m_flPercentage > m_flSecondWarningLevel )
	{
		pclrHealth = &m_clrHealthMed; 
	}
	else
	{
		pclrHealth = &m_clrHealthLow;
	}

	// blend in the red "damage" part
	float uv1 = 0.0f;
	float uv2 = 1.0f;

	vgui::surface()->DrawSetTexture( m_iMaterialIndex );

	Vector2D uv11( uv1, uv1 );
	Vector2D uv21( uv2, uv1 );
	Vector2D uv22( uv2, uv2 );
	Vector2D uv12( uv1, uv2 );

	vgui::Vertex_t vert[4];	

	// background
	vert[0].Init( Vector2D( xpos, ypos ), uv11 );
	vert[1].Init( Vector2D( xpos + w, ypos ), uv21 );
	vert[2].Init( Vector2D( xpos + w, ypos + h ), uv22 );				
	vert[3].Init( Vector2D( xpos, ypos + h ), uv12 );

	if ( m_flPercentage <= 0.0f )
	{
		vgui::surface()->DrawSetColor( m_clrHealthLow );
	}
	else if ( m_flPercentage > 1.0f ) // Overheal
	{
		vgui::surface()->DrawSetColor( m_clrHealthHigh );
	}
	else
	{
		vgui::surface()->DrawSetColor( m_clrBackground );
	}
	vgui::surface()->DrawTexturedPolygon( 4, vert );

	if ( m_flPercentage > 1.0f ) // Overheal
	{
		vert[0].Init( Vector2D( xpos, flOverHealY ), uv11 );
		vert[1].Init( Vector2D( xpos + w, flOverHealY ), uv21 );
		vert[2].Init( Vector2D( xpos + w, ypos + h ), uv22 );				
		vert[3].Init( Vector2D( xpos, ypos + h ), uv12 );

		vgui::surface()->DrawSetColor( m_clrHealthOverHeal );
	}
	else // damage part
	{
		vert[0].Init( Vector2D( xpos, flDamageY ), uv11 );
		vert[1].Init( Vector2D( xpos + w, flDamageY ), uv21 );
		vert[2].Init( Vector2D( xpos + w, ypos + h ), uv22 );				
		vert[3].Init( Vector2D( xpos, ypos + h ), uv12 );

		vgui::surface()->DrawSetColor( *pclrHealth );
	}

	vgui::surface()->DrawTexturedPolygon( 4, vert );

	// outline
	vert[0].Init( Vector2D( xpos, ypos ), uv11 );
	vert[1].Init( Vector2D( xpos + w - 1, ypos ), uv21 );
	vert[2].Init( Vector2D( xpos + w - 1, ypos + h - 1 ), uv22 );				
	vert[3].Init( Vector2D( xpos, ypos + h - 1 ), uv12 );

	vgui::surface()->DrawSetColor( m_clrBorder );
	vgui::surface()->DrawTexturedPolyLine( vert, 4 );
}

// Show the health / class for a player other than the local player
void CDoDHudHealthBar::SetHealthDelegatePlayer( C_TFPlayer *pPlayer )
{
	m_hHealthDelegatePlayer = pPlayer;
}

C_TFPlayer *CDoDHudHealthBar::GetHealthDelegatePlayer( void )
{
	if ( m_hHealthDelegatePlayer.Get() )
	{
		C_TFPlayer *pPlayer = dynamic_cast<C_TFPlayer *>( m_hHealthDelegatePlayer.Get() );
		if ( pPlayer )
			return pPlayer;
	}

	return C_TFPlayer::GetLocalTFPlayer();
}

DECLARE_BUILD_FACTORY( CDoDHudHealth );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDoDHudHealth::CDoDHudHealth( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	SetProportional( true );

	m_pHealthBar = new CDoDHudHealthBar( this, "HealthBar" );
	m_pClassImageBG = new vgui::ImagePanel( this, "HealthClassImageBG" );
	m_pClassImage = new vgui::ImagePanel( this, "HealthClassImage" );
	
	m_nPrevClass = PLAYERCLASS_UNDEFINED;
	m_nPrevTeam = TEAM_INVALID;

	// load control settings...
	LoadControlSettings( "resource/UI/HudPlayerStatusHealth.res" );

	m_hHealthDelegatePlayer = NULL;
}

void CDoDHudHealth::OnScreenSizeChanged(int iOldWide, int iOldTall)
{
	LoadControlSettings( "resource/UI/HudPlayerStatusHealth.res" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudHealth::OnThink()
{
	BaseClass::OnThink();

	C_TFPlayer *pPlayer = GetHealthDelegatePlayer();
	if ( pPlayer )
	{
		int nTeam = pPlayer->GetTeamNumber();

		if ( nTeam == TF_TEAM_RED || nTeam == TF_TEAM_BLUE )
		{
			int nClass = g_TF_PR->GetPlayerClass( pPlayer->entindex() );

			if ( nClass != PLAYERCLASS_UNDEFINED )
			{
				if ( ( nClass != m_nPrevClass ) ||
					( nTeam != TEAM_INVALID && ( nTeam == TF_TEAM_BLUE || nTeam == TF_TEAM_RED ) && nTeam != m_nPrevTeam ) )
				{
					m_nPrevClass = nClass;
					m_nPrevTeam = nTeam;

					if ( m_pClassImage )
					{
						m_pClassImage->SetImage( pPlayer->GetPlayerClass()->GetData()->m_szClassHealthImage );
					}

					if ( m_pClassImageBG )
					{
						m_pClassImage->SetImage( pPlayer->GetPlayerClass()->GetData()->m_szClassHealthImageBG );
					}
				}
			}
		}
	}
}

// Show the health / class for a player other than the local player
void CDoDHudHealth::SetHealthDelegatePlayer( C_TFPlayer *pPlayer )
{
	m_hHealthDelegatePlayer = pPlayer;

	m_pHealthBar->SetHealthDelegatePlayer( pPlayer );
}

C_TFPlayer *CDoDHudHealth::GetHealthDelegatePlayer( void )
{
	if ( m_hHealthDelegatePlayer.Get() )
	{
		C_TFPlayer *pPlayer = dynamic_cast<C_TFPlayer *>( m_hHealthDelegatePlayer.Get() );
		if ( pPlayer )
			return pPlayer;
	}

	return C_TFPlayer::GetLocalTFPlayer();
}