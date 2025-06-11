//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DODCLASSMENU_H
#define DODCLASSMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <classmenu.h>
#include <vgui_controls/EditablePanel.h>
#include <filesystem.h>
#include <dod_shareddefs.h>
#include "cbase.h"
#include "tf_gamerules.h"
#include "dodmenubackground.h"
#include "dodbutton.h"
#include "imagemouseoverbutton.h"
#include "IconPanel.h"
#include <vgui_controls/CheckButton.h>

#include "c_tf_playerresource.h"
#include "tf_controls.h"

using namespace vgui;

#define NUM_CLASSES	6

static const char *g_sDODDialogVariables[] = {
	"numTommy",
	"numSpring",
	"numGarand",
	"numBazooka",
	"numBar",
	"num30Cal",
	"numK98",
	"numK98s",
	"numMG42",
	"numMP40",
	"numMP44",
	"numPschreck",
};

static int g_sDODClassDefines[] = {
	DOD_CLASS_TOMMY,
	DOD_CLASS_SPRING,
	DOD_CLASS_GARAND,
	DOD_CLASS_BAZOOKA,
	DOD_CLASS_BAR,
	DOD_CLASS_30CAL,
	DOD_CLASS_K98,
	DOD_CLASS_K98s,
	DOD_CLASS_MG42,
	DOD_CLASS_MP40,
	DOD_CLASS_MP44,
	DOD_CLASS_PSCHRECK,
};

class CDODClassMenu : public CClassMenu
{
private:
	DECLARE_CLASS_SIMPLE( CDODClassMenu, CClassMenu );

public:
	CDODClassMenu(IViewPort *pViewPort);

	virtual void Update( void );
	virtual Panel *CreateControlByName( const char *controlName );
	virtual void OnTick( void );
	virtual void PaintBackground( void );
	virtual void OnKeyCodePressed(KeyCode code);
	virtual void SetVisible( bool state );

	MESSAGE_FUNC_CHARPTR( OnShowPage, "ShowPage", page );

	virtual void ShowPanel(bool bShow);
	virtual void UpdateClassCounts( void ){}

	virtual void UpdateNumClassLabel( int iTeam );

	virtual int GetTeamNumber( void ) = 0;

#ifdef REFRESH_CLASSMENU_TOOL
	MESSAGE_FUNC( OnRefreshClassMenu, "refresh_classes" );
#endif

	MESSAGE_FUNC_PTR( OnSuicideOptionChanged, "CheckButtonChecked", panel );

private:
	CDODClassInfoPanel *m_pClassInfoPanel;
	CDODMenuBackground *m_pBackground;
	CheckButton *m_pSuicideOption;

	CImageMouseOverButton<CDODClassInfoPanel> *m_pInitialButton;
	int m_iActivePlayerClass;
	int m_iLastPlayerClassCount;
	int	m_iLastClassLimit;

	ButtonCode_t m_iClassMenuKey;
};

//-----------------------------------------------------------------------------
// Purpose: Draws the U.S. class menu
//-----------------------------------------------------------------------------

class CDODClassMenu_Allies : public CDODClassMenu
{
private:
	DECLARE_CLASS_SIMPLE( CDODClassMenu_Allies, CDODClassMenu );
	
public:
	CDODClassMenu_Allies(IViewPort *pViewPort) : BaseClass(pViewPort)
	{
		LoadControlSettings( "Resource/UI/ClassMenu_Allies.res" );
	}
	
	virtual const char *GetName( void )
	{ 
		return PANEL_CLASS_ALLIES; 
	}

	virtual int GetTeamNumber( void )
	{
		return TF_TEAM_RED;
	}

	virtual void UpdateClassCounts( void ){ UpdateNumClassLabel( TF_TEAM_RED ); }
};


//-----------------------------------------------------------------------------
// Purpose: Draws the Wermacht class menu
//-----------------------------------------------------------------------------

class CDODClassMenu_Axis : public CDODClassMenu
{
private:
	DECLARE_CLASS_SIMPLE( CDODClassMenu_Axis, CDODClassMenu );
	
public:
	CDODClassMenu_Axis(IViewPort *pViewPort) : BaseClass(pViewPort)
	{
		LoadControlSettings( "Resource/UI/ClassMenu_Axis.res" );
	}

	virtual const char *GetName( void )
	{ 
		return PANEL_CLASS_AXIS;
	}

	virtual int GetTeamNumber( void )
	{
		return TF_TEAM_BLUE;
	}

	virtual void UpdateClassCounts( void ){ UpdateNumClassLabel( TF_TEAM_BLUE ); }
};

#endif // DODCLASSMENU_H
