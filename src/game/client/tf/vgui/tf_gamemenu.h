//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef TF_GAMEMENU_H
#define TF_GAMEMENU_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_controls.h"
#include <teammenu.h>

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTFGameButton : public CExButton
{
private:
	DECLARE_CLASS_SIMPLE( CTFGameButton, CExButton );

public:
	CTFGameButton( vgui::Panel *parent, const char *panelName );

	void ApplySettings( KeyValues *inResourceData );
	void ApplySchemeSettings( vgui::IScheme *pScheme );

	void OnCursorExited();
	void OnCursorEntered();

	void OnTick( void );

	void SetDefaultAnimation( const char *pszName );

private:
	void SendAnimation( const char *pszAnimation );
	void SetMouseEnteredState( bool state );

private:
	char	m_szModelPanel[64];		// the panel we'll send messages to

	float	m_flHoverTimeToWait;	// length of time to wait before reporting a "hover" message (-1 = no hover)
	float	m_flHoverTime;			// when should a "hover" message be sent?
	bool	m_bMouseEntered;		// used to track when the mouse is over a button
	bool	m_bTeamDisabled;		// used to keep track of whether our team is a valid team for selection
};

//-----------------------------------------------------------------------------
// Purpose: Displays the team menu
//-----------------------------------------------------------------------------
class CTFGameMenu : public CTeamMenu
{
private:
	DECLARE_CLASS_SIMPLE( CTFGameMenu, CTeamMenu );
		
public:
	CTFGameMenu( IViewPort *pViewPort );
	~CTFGameMenu();

	virtual const char *GetName( void ) { return PANEL_GAMEMENU; }
	void Update();
	void ShowPanel( bool bShow );

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnKeyCodePressed( vgui::KeyCode code );

	// command callbacks
	virtual void OnCommand( const char *command );

	virtual void LoadMapPage( const char *mapName );

	virtual void OnTick( void );

private:

	CTFGameButton	*m_pDODButton;
	CTFGameButton	*m_pTFButton;
//	CTFGameButton	*m_pAutoTeamButton;

	CExButton		*m_pCancelButton;

private:
	enum { NUM_GAMES = 2 };

	ButtonCode_t m_iGameMenuKey;
};

#endif // TF_TEAMMENU_H
