//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HUD Target ID element
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "c_tf_player.h"
#include "c_playerresource.h"
#include "vgui_entitypanel.h"
#include "iclientmode.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PLAYER_HINT_DISTANCE	150
#define PLAYER_HINT_DISTANCE_SQ	(PLAYER_HINT_DISTANCE*PLAYER_HINT_DISTANCE)

//static ConVar hud_centerid( "hud_centerid", "1", FCVAR_ARCHIVE );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDODTargetID : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CDODTargetID, vgui::Panel );

public:
	CDODTargetID( const char *pElementName );
	void Init( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint( void );
	void VidInit( void );
	virtual bool	ShouldDraw( void );

private:
	vgui::HFont		m_hFont;
	int				m_iLastEntIndex;
	float			m_flLastChangeTime;
};

DECLARE_HUDELEMENT( CDODTargetID );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDODTargetID::CDODTargetID( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "DODTargetID" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_hFont = g_hFontTrebuchet24;
	m_flLastChangeTime = 0;
	m_iLastEntIndex = 0;

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	RegisterForRenderGroup( "winpanel" );
}

//-----------------------------------------------------------------------------
// Purpose: Setup
//-----------------------------------------------------------------------------
void CDODTargetID::Init( void )
{
};

void CDODTargetID::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_hFont = scheme->GetFont( "DOD_TargetID", IsProportional() );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: clear out string etc between levels
//-----------------------------------------------------------------------------
void CDODTargetID::VidInit()
{
	CHudElement::VidInit();

	m_flLastChangeTime = 0;
	m_iLastEntIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDODTargetID::ShouldDraw( void )
{
	if ( !CHudElement::ShouldDraw() )
		return false;

	C_TFPlayer *pLocalTFPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pLocalTFPlayer )
		return false;

	if ( !pLocalTFPlayer->IsDODClass() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Draw function for the element
//-----------------------------------------------------------------------------
void CDODTargetID::Paint()
{
#define MAX_ID_STRING 256
	wchar_t sIDString[ MAX_ID_STRING ];
	wchar_t sExtraIDString[ MAX_ID_STRING ];
	sExtraIDString[0] = 0;
	sIDString[0] = 0;

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pPlayer )
		return;

	// Get our target's ent index
	int iEntIndex = pPlayer->GetIDTarget();
	// Didn't find one?
	if ( !iEntIndex )
	{
		// Check to see if we should clear our ID
		if ( m_flLastChangeTime && (gpGlobals->curtime > (m_flLastChangeTime + 0.5)) )
		{
			m_flLastChangeTime = 0;
			sIDString[0] = 0;
			sExtraIDString[0] = 0;
			m_iLastEntIndex = 0;
		}
		else
		{
			// Keep re-using the old one
			iEntIndex = m_iLastEntIndex;
		}
	}
	else
	{
		m_flLastChangeTime = gpGlobals->curtime;
	}

	// Is this an entindex sent by the server?
	if ( iEntIndex )
	{
		C_BasePlayer *pPlayer = static_cast<C_BasePlayer*>(cl_entitylist->GetEnt( iEntIndex ));
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		//C_TFPlayer *pLocalDODPlayer = C_TFPlayer::GetLocalTFPlayer();

		wchar_t wszPlayerName[ MAX_PLAYER_NAME_LENGTH ];
		wchar_t wszHealthText[ 10 ];

		// Some entities we always want to check, cause the text may change
		// even while we're looking at it
		if ( IsPlayerIndex( iEntIndex ) && pPlayer->InSameTeam(pLocalPlayer) )
		{
			// Construct the wide char name string
			g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(),  wszPlayerName, sizeof(wszPlayerName) );
			
			_snwprintf( wszHealthText, ARRAYSIZE(wszHealthText) - 1, L"%.0f", (float)pPlayer->GetHealth() );
			wszHealthText[ ARRAYSIZE(wszHealthText)-1 ] = '\0';

			// Construct the string to display
			g_pVGuiLocalize->ConstructString( sIDString, sizeof(sIDString), L"%s1 (%s2)", 2, wszPlayerName, wszHealthText );

			CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );

			if ( pTFPlayer && pTFPlayer->IsPlayerClass( TF_CLASS_MEDIC ) )
			{
				wchar_t wszChargeLevel[ 10 ];
				_snwprintf( wszChargeLevel, ARRAYSIZE(wszChargeLevel) - 1, L"%.0f", pTFPlayer->MedicGetChargeLevel() * 100 );
				wszChargeLevel[ ARRAYSIZE(wszChargeLevel)-1 ] = '\0';
				g_pVGuiLocalize->ConstructString( sExtraIDString, sizeof(sExtraIDString), g_pVGuiLocalize->Find( "#TF_playerid_mediccharge" ), 1, wszChargeLevel );
			}

			if ( sIDString[0] )
			{
				int wide, tall;
				int ypos = YRES(260);
				int xpos = XRES(10);

				vgui::surface()->GetTextSize( m_hFont, sIDString, wide, tall );

				// Seal: This doesnt even work on base dod lol
				//if( hud_centerid.GetInt() == 0 )
				//{
				//	ypos = YRES(420);
				//}
				//else
				{
					xpos = (ScreenWidth() - wide) / 2;
				}

				vgui::surface()->DrawSetTextFont( m_hFont );

				// draw a black dropshadow ( the default one looks horrible )
				vgui::surface()->DrawSetTextPos( xpos+1, ypos+1 );
				vgui::surface()->DrawSetTextColor( Color(0,0,0,255) );
				vgui::surface()->DrawPrintText( sIDString, wcslen(sIDString) );		

				vgui::surface()->DrawSetTextPos( xpos, ypos );
				vgui::surface()->DrawSetTextColor( g_PR->GetTeamColor( pPlayer->GetTeamNumber() ) );
				vgui::surface()->DrawPrintText( sIDString, wcslen(sIDString) );

				if ( sExtraIDString[0] )
				{
					 xpos = XRES(275);
					 ypos = YRES(280);

					// draw a black dropshadow ( the default one looks horrible )
					vgui::surface()->DrawSetTextPos( xpos+1, ypos+1 );
					vgui::surface()->DrawSetTextColor( Color(0,0,0,255) );
					vgui::surface()->DrawPrintText( sExtraIDString, wcslen(sExtraIDString) );

					vgui::surface()->DrawSetTextPos( xpos, ypos );
					vgui::surface()->DrawSetTextColor( g_PR->GetTeamColor( pPlayer->GetTeamNumber() ) );
					vgui::surface()->DrawPrintText( sExtraIDString, wcslen(sExtraIDString) );
				}
			}

			// SEALTODO
			//pLocalDODPlayer->HintMessage( HINT_FRIEND_SEEN );
		}		
		else if( IsPlayerIndex( iEntIndex ) &&
			pPlayer->GetTeamNumber() != TEAM_SPECTATOR &&
			pPlayer->GetTeamNumber() != TEAM_UNASSIGNED &&
			pPlayer->IsAlive() )
		{
			// must not be in the same team, enemy
			// SEALTODO
			//pLocalDODPlayer->HintMessage( HINT_ENEMY_SEEN );
		}
	}
}
