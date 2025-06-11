//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_MOUSE_OVER_BUTTON_H
#define DOD_MOUSE_OVER_BUTTON_H

#include "dodbutton.h"
#include "mouseoverpanelbutton.h"

template <class T>
class CDODMouseOverButton : public MouseOverButton<T>, public CDODButtonShape
{
private:
	//DECLARE_CLASS_SIMPLE( CDODMouseOverButton, MouseOverButton );
	
public:
	CDODMouseOverButton(vgui::Panel *parent, const char *panelName, T *templatePanel ) :
					MouseOverButton<T>( parent, panelName, templatePanel )
	{
	}

	virtual int  GetButtonTypeReference( void ) { return BUTTON_DOD; }

protected:
	virtual void PaintBackground();
	virtual void PaintBorder();

public:
	virtual void ShowPage( void );
	virtual void HidePage( void );

	virtual const char *GetClassPage( const char *className )
	{
		static char classPanel[ _MAX_PATH ];
		Q_snprintf( classPanel, sizeof( classPanel ), "classes/%s.res", className);

		if ( g_pFullFileSystem->FileExists( classPanel, IsX360() ? "MOD" : "GAME" ) )
		{
		}
		else if (g_pFullFileSystem->FileExists( "classes/default_dod.res", IsX360() ? "MOD" : "GAME" ) )
		{
			Q_snprintf ( classPanel, sizeof( classPanel ), "classes/default_dod.res" );
		}
		else
		{
			return NULL;
		}

		return classPanel;
	}
};

//===============================================
// CDODMouseOverButton - shaped mouseover button
//===============================================
template <class T>
void CDODMouseOverButton<T>::PaintBackground()
{
	int wide, tall;
	this->GetSize(wide,tall);
	DrawShapedBackground( 0, 0, wide, tall, this->GetBgColor() );
}

template <class T>
void CDODMouseOverButton<T>::PaintBorder()
{
	int wide, tall;
	this->GetSize(wide,tall);
	DrawShapedBorder( 0, 0, wide, tall, this->GetFgColor() );
}

template <class T>
void CDODMouseOverButton<T>::ShowPage( void )
{
	MouseOverButton<T>::ShowPage();

	// send message to parent that we triggered something
	this->PostActionSignal( new KeyValues("ShowPage", "page", this->GetName() ) );
}

template <class T>
void CDODMouseOverButton<T>::HidePage( void )
{
	MouseOverButton<T>::HidePage();

	// send message to parent that we triggered something
	this->PostActionSignal( new KeyValues("ShowPage", "page", this->GetName() ) );
}

#endif // DOD_MOUSE_OVER_BUTTON_H