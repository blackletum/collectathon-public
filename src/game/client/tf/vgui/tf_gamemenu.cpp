//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"

#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Frame.h>
#include <vgui/IScheme.h>
#include <game/client/iviewport.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>
#include <filesystem.h>

#include "vguicenterprint.h"
#include "tf_controls.h"
#include "basemodelpanel.h"
#include "tf_gamemenu.h"
#include <convar.h>
#include "IGameUIFuncs.h" // for key bindings
#include "hud.h" // for gEngfuncs
#include "c_tf_player.h"
#include "tf_gamerules.h"
#include "c_team.h"
#include "tf_hud_notification_panel.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFGameButton::CTFGameButton( vgui::Panel *parent, const char *panelName ) : CExButton( parent, panelName, "" )
{
	m_szModelPanel[0] = '\0';
	m_flHoverTimeToWait = -1;
	m_flHoverTime = -1;
	m_bMouseEntered = false;
	m_bTeamDisabled = false;

	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameButton::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	Q_strncpy( m_szModelPanel, inResourceData->GetString( "associated_model", "" ), sizeof( m_szModelPanel ) );
	m_flHoverTimeToWait = inResourceData->GetFloat( "hover", -1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameButton::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetDefaultColor( GetFgColor(), Color( 0, 0, 0, 0 ) );
	SetArmedColor( GetButtonFgColor(), Color( 0, 0, 0, 0 ) );
	SetDepressedColor( GetButtonFgColor(), Color( 0, 0, 0, 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameButton::SendAnimation( const char *pszAnimation )
{
	Panel *pParent = GetParent();
	if ( pParent )
	{
		CModelPanel *pModel = dynamic_cast< CModelPanel* >( pParent->FindChildByName( m_szModelPanel ) );
		if ( pModel )
		{
			KeyValues *kvParms = new KeyValues( "SetAnimation" );
			if ( kvParms )
			{
				kvParms->SetString( "animation", pszAnimation );
				PostMessage( pModel, kvParms );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameButton::SetDefaultAnimation( const char *pszName )
{
	Panel *pParent = GetParent();
	if ( pParent )
	{
		CModelPanel *pModel = dynamic_cast< CModelPanel* >( pParent->FindChildByName( m_szModelPanel ) );
		if ( pModel )
		{
			pModel->SetDefaultAnimation( pszName );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameButton::OnCursorEntered()
{
	BaseClass::OnCursorEntered();

	SetMouseEnteredState( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameButton::OnCursorExited()
{
	BaseClass::OnCursorExited();

	SetMouseEnteredState( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameButton::SetMouseEnteredState( bool state )
{
	if ( state )
	{
		m_bMouseEntered = true;

		if ( m_flHoverTimeToWait > 0 )
		{
			m_flHoverTime = gpGlobals->curtime + m_flHoverTimeToWait;
		}
		else
		{
			m_flHoverTime = -1;
		}

		if ( m_bTeamDisabled )
		{
			SendAnimation( "enter_disabled" );
		}
		else
		{
			SendAnimation( "enter_enabled" );
		}
	}
	else
	{
		m_bMouseEntered = false;
		m_flHoverTime = -1;

		if ( m_bTeamDisabled )
		{
			SendAnimation( "exit_disabled" );
		}
		else
		{
			SendAnimation( "exit_enabled" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameButton::OnTick()
{
	if ( ( m_flHoverTime > 0 ) && ( m_flHoverTime < gpGlobals->curtime ) )
	{
		m_flHoverTime = -1;

		SendAnimation( "hover_enabled" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFGameMenu::CTFGameMenu( IViewPort *pViewPort ) : CTeamMenu( pViewPort )
{
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetVisible( false );
	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( true );

	m_iGameMenuKey = BUTTON_CODE_INVALID;

	m_pDODButton = new CTFGameButton( this, "teambutton0" );
	m_pTFButton = new CTFGameButton( this, "teambutton1" );
//	m_pAutoTeamButton = new CTFGameButton( this, "teambutton2" );

	m_pCancelButton = new CExButton( this, "CancelButton", "#TF_Cancel" );

	vgui::ivgui()->AddTickSignal( GetVPanel() );

	LoadControlSettings( "Resource/UI/Gamemenu.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTFGameMenu::~CTFGameMenu()
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
void CTFGameMenu::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/Gamemenu.res" );

	Update();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameMenu::ShowPanel( bool bShow )
{
	if ( BaseClass::IsVisible() == bShow )
		return;

	if ( !C_TFPlayer::GetLocalTFPlayer() )
		return;

	if ( !gameuifuncs || !gViewPortInterface || !engine )
		return;

	if ( bShow )
	{
		if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN && 
			 C_TFPlayer::GetLocalTFPlayer() && 
			 C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() != TFGameRules()->GetWinningTeam()
			 && C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() == TEAM_SPECTATOR 
	  		 && C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() == TEAM_UNASSIGNED )
		{
			SetVisible( false );
			SetMouseInputEnabled( false );

			CHudNotificationPanel *pNotifyPanel = GET_HUDELEMENT( CHudNotificationPanel );
			if ( pNotifyPanel )
			{
				pNotifyPanel->SetupNotifyCustom( "#TF_CantChangeGameNow", "ico_notify_flag_moving", C_TFPlayer::GetLocalTFPlayer()->GetTeamNumber() );
			}

			return;
		}

		gViewPortInterface->ShowPanel( PANEL_CLASS_RED, false );
		gViewPortInterface->ShowPanel( PANEL_CLASS_BLUE, false );
		gViewPortInterface->ShowPanel( PANEL_TEAM, false );

		// Close DOD menus
		gViewPortInterface->ShowPanel( PANEL_CLASS_ALLIES, false );
		gViewPortInterface->ShowPanel( PANEL_CLASS_AXIS, false );

		// SEALTODO: Find out what this does
		engine->CheckPoint( "GameMenu" );

		Activate();
		SetMouseInputEnabled( true );

		// get key bindings if shown
		m_iGameMenuKey = gameuifuncs->GetButtonCodeForBind( "changegame" );
		m_iScoreBoardKey = gameuifuncs->GetButtonCodeForBind( "showscores" );
	}
	else
	{
		SetVisible( false );
		SetMouseInputEnabled( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: called to update the menu with new information
//-----------------------------------------------------------------------------
void CTFGameMenu::Update( void )
{
	BaseClass::Update();

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( pLocalPlayer && ( pLocalPlayer->GetPlayerClass()->GetClassIndex() != TF_CLASS_UNDEFINED ) )
	{
		if ( m_pCancelButton )
		{
			m_pCancelButton->SetVisible( true );
		}
	}
	else
	{
		if ( m_pCancelButton && m_pCancelButton->IsVisible() )
		{
			m_pCancelButton->SetVisible( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: chooses and loads the text page to display that describes mapName map
//-----------------------------------------------------------------------------
void CTFGameMenu::LoadMapPage( const char *mapName )
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGameMenu::OnKeyCodePressed( KeyCode code )
{
	if ( ( m_iGameMenuKey != BUTTON_CODE_INVALID && m_iGameMenuKey == code ) ||
		   code == KEY_XBUTTON_BACK || 
		   code == KEY_XBUTTON_B )
	{
		//C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

		//if ( pLocalPlayer && ( pLocalPlayer->GetPlayerClass()->GetClassIndex() != TF_CLASS_UNDEFINED ) )
		//{
		//	ShowPanel( false );
		//}
	}
	else if( code == KEY_SPACE )
	{
		// SEALTODO auto game
		//engine->ClientCmd( "jointeam auto" );

		ShowPanel( false );
		OnClose();
	}
	else if( code == KEY_XBUTTON_A || code == KEY_XBUTTON_RTRIGGER )
	{
		// select the active focus
		if ( GetFocusNavGroup().GetCurrentFocus() )
		{
			ipanel()->SendMessage( GetFocusNavGroup().GetCurrentFocus()->GetVPanel(), new KeyValues( "PressButton" ), GetVPanel() );
		}
	}
	else if( code == KEY_XBUTTON_RIGHT || code == KEY_XSTICK1_RIGHT )
	{
		CTFGameButton *pButton;
			
		pButton = dynamic_cast< CTFGameButton *> ( GetFocusNavGroup().GetCurrentFocus() );
		if ( pButton )
		{
			pButton->OnCursorExited();
			GetFocusNavGroup().RequestFocusNext( pButton->GetVPanel() );
		}
		else
		{
			GetFocusNavGroup().RequestFocusNext( NULL );
		}

		pButton = dynamic_cast< CTFGameButton * > ( GetFocusNavGroup().GetCurrentFocus() );
		if ( pButton )
		{
			pButton->OnCursorEntered();
		}
	}
	else if( code == KEY_XBUTTON_LEFT || code == KEY_XSTICK1_LEFT )
	{
		CTFGameButton *pButton;

		pButton = dynamic_cast< CTFGameButton *> ( GetFocusNavGroup().GetCurrentFocus() );
		if ( pButton )
		{
			pButton->OnCursorExited();
			GetFocusNavGroup().RequestFocusPrev( pButton->GetVPanel() );
		}
		else
		{
			GetFocusNavGroup().RequestFocusPrev( NULL );
		}

		pButton = dynamic_cast< CTFGameButton * > ( GetFocusNavGroup().GetCurrentFocus() );
		if ( pButton )
		{
			pButton->OnCursorEntered();
		}
	}
	else if ( m_iScoreBoardKey != BUTTON_CODE_INVALID && m_iScoreBoardKey == code )
	{
		gViewPortInterface->ShowPanel( PANEL_SCOREBOARD, true );
		gViewPortInterface->PostMessageToPanel( PANEL_SCOREBOARD, new KeyValues( "PollHideCode", "code", code ) );
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the user picks a team
//-----------------------------------------------------------------------------
void CTFGameMenu::OnCommand( const char *command )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( Q_stricmp( command, "vguicancel" ) )
	{
		// we're selecting a team, so make sure it's not the team we're already on before sending to the server
		if ( pLocalPlayer && ( Q_strstr( command, "joingame " ) ) )
		{
			const char *pGame = command + Q_strlen( "joingame " );

			if ( Q_stricmp( pGame, "tf2" ) == 0 )
			{
				if ( pLocalPlayer->GetTeamNumber() == TF_TEAM_RED )
					gViewPortInterface->ShowPanel( PANEL_CLASS_RED, true );
				else
					gViewPortInterface->ShowPanel( PANEL_CLASS_BLUE, true );
			}
			else if ( Q_stricmp( pGame, "dod" ) == 0 )
			{
				if ( pLocalPlayer->GetTeamNumber() == TF_TEAM_RED )
					gViewPortInterface->ShowPanel( PANEL_CLASS_ALLIES, true );
				else
					gViewPortInterface->ShowPanel( PANEL_CLASS_AXIS, true );
			}
		}
	}

	BaseClass::OnCommand( command );
	ShowPanel( false );
	OnClose();
}

//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CTFGameMenu::OnTick()
{
	// update the number of players on each team

	// enable or disable buttons based on team limit

	C_Team *pRed = GetGlobalTeam( TF_TEAM_RED );
	C_Team *pBlue = GetGlobalTeam( TF_TEAM_BLUE );

	if ( !pRed || !pBlue )
		return;

	// set our team counts
	//SetDialogVariable( "bluecount", pBlue->Get_Number_Players() );
	//SetDialogVariable( "redcount", pRed->Get_Number_Players() );

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pLocalPlayer )
		return;

	CTFGameRules *pRules = TFGameRules();

	if ( !pRules )
		return;

	//if ( m_pSpecTeamButton && m_pSpecLabel )
	//{
	//	ConVarRef mp_allowspectators( "mp_allowspectators" );
	//	if ( mp_allowspectators.IsValid() )
	//	{
	//		if ( mp_allowspectators.GetBool() )
	//		{
	//			if ( !m_pSpecTeamButton->IsVisible() )
	//			{
	//				m_pSpecTeamButton->SetVisible( true );
	//				m_pSpecLabel->SetVisible( true );
	//			}
	//		}
	//		else
	//		{
	//			if ( m_pSpecTeamButton->IsVisible() )
	//			{
	//				m_pSpecTeamButton->SetVisible( false );
	//				m_pSpecLabel->SetVisible( false );
	//			}
	//		}
	//	}
	//}
}