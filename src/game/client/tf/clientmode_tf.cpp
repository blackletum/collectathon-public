//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================
#include "cbase.h"
#include "hud.h"
#include "clientmode_tf.h"
#include "cdll_client_int.h"
#include "iinput.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "GameUI/IGameUI.h"
#include <vgui_controls/AnimationController.h>
#include "ivmodemanager.h"
#include "buymenu.h" 
#include "filesystem.h"
#include "vgui/IVGui.h"
#include "hud_chat.h"
#include "view_shared.h"
#include "view.h"
#include "ivrenderview.h"
#include "model_types.h"
#include "iefx.h"
#include "dlight.h"
#include <imapoverview.h>
#include "c_playerresource.h"
#include <KeyValues.h>
#include "text_message.h"
#include "panelmetaclassmgr.h"
#include "c_tf_player.h"
#include "ienginevgui.h"
#include "in_buttons.h"
#include "voice_status.h"
#include "tf_hud_menu_engy_build.h"
#include "tf_hud_menu_engy_destroy.h"
#include "tf_hud_menu_spy_disguise.h"
#include "tf_statsummary.h"
#include "tf_hud_freezepanel.h"
#include "cam_thirdperson.h"
#include "hud_macros.h"

#if defined( _X360 )
#include "tf_clientscoreboard.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void __MsgFunc_PlayerExtinguished( bf_read &msg );
void __MsgFunc_BreakModel( bf_read &msg );
void __MsgFunc_CheapBreakModel( bf_read &msg );
void __MsgFunc_ForcePlayerViewAngles( bf_read &msg );

ConVar default_fov( "default_fov", "90", FCVAR_CHEAT );
ConVar fov_desired( "fov_desired", "90", FCVAR_ARCHIVE | FCVAR_USERINFO, "Sets the base field-of-view.", true, 75.0, true, 90.0 );

void HUDMinModeChangedCallBack( IConVar *var, const char *pOldString, float flOldValue )
{
	engine->ExecuteClientCmd( "hud_reloadscheme" );
}
ConVar cl_hud_minmode( "cl_hud_minmode", "0", FCVAR_ARCHIVE, "Set to 1 to turn on the advanced minimalist HUD mode.", HUDMinModeChangedCallBack );

IClientMode *g_pClientMode = NULL;

// --------------------------------------------------------------------------------- //
// CTFModeManager.
// --------------------------------------------------------------------------------- //

class CTFModeManager : public IVModeManager
{
public:
	virtual void	Init();
	virtual void	SwitchMode( bool commander, bool force ) {}
	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );
	virtual void	ActivateMouse( bool isactive ) {}
};

static CTFModeManager g_ModeManager;
IVModeManager *modemanager = ( IVModeManager * )&g_ModeManager;


// --------------------------------------------------------------------------------- //
// CTFModeManager implementation.
// --------------------------------------------------------------------------------- //

#define SCREEN_FILE		"scripts/vgui_screens.txt"

void CTFModeManager::Init()
{
	g_pClientMode = GetClientModeNormal();
	
	PanelMetaClassMgr()->LoadMetaClassDefinitionFile( SCREEN_FILE );

	// Load the objects.txt file.
	LoadObjectInfos( ::filesystem );

	GetClientVoiceMgr()->SetHeadLabelOffset( 40 );
}

void CTFModeManager::LevelInit( const char *newmap )
{
	g_pClientMode->LevelInit( newmap );

	ConVarRef voice_steal( "voice_steal" );

	if ( voice_steal.IsValid() )
	{
		voice_steal.SetValue( 1 );
	}

	g_ThirdPersonManager.Init();
}

void CTFModeManager::LevelShutdown( void )
{
	g_pClientMode->LevelShutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeTFNormal::ClientModeTFNormal()
{
	m_pMenuEngyBuild = NULL;
	m_pMenuEngyDestroy = NULL;
	m_pMenuSpyDisguise = NULL;
	m_pGameUI = NULL;
	m_pFreezePanel = NULL;

	// Hook global message handlers
	HOOK_MESSAGE( PlayerExtinguished );
	HOOK_MESSAGE( BreakModel );
	HOOK_MESSAGE( CheapBreakModel );
	HOOK_MESSAGE( ForcePlayerViewAngles );

#if defined( _X360 )
	m_pScoreboard = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: If you don't know what a destructor is by now, you are probably going to get fired
//-----------------------------------------------------------------------------
ClientModeTFNormal::~ClientModeTFNormal()
{
}

// See interface.h/.cpp for specifics:  basically this ensures that we actually Sys_UnloadModule the dll and that we don't call Sys_LoadModule 
//  over and over again.
static CDllDemandLoader g_GameUI( "gameui" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeTFNormal::Init()
{
	m_pMenuEngyBuild = ( CHudMenuEngyBuild * )GET_HUDELEMENT( CHudMenuEngyBuild );
	Assert( m_pMenuEngyBuild );

	m_pMenuEngyDestroy = ( CHudMenuEngyDestroy * )GET_HUDELEMENT( CHudMenuEngyDestroy );
	Assert( m_pMenuEngyDestroy );

	m_pMenuSpyDisguise = ( CHudMenuSpyDisguise * )GET_HUDELEMENT( CHudMenuSpyDisguise );
	Assert( m_pMenuSpyDisguise );

	m_pFreezePanel = ( CTFFreezePanel * )GET_HUDELEMENT( CTFFreezePanel );
	Assert( m_pFreezePanel );

	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		m_pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
		if ( NULL != m_pGameUI )
		{
			// insert stats summary panel as the loading background dialog
			CTFStatsSummaryPanel *pPanel = GStatsSummaryPanel();
			pPanel->InvalidateLayout( false, true );
			pPanel->SetVisible( false );
			pPanel->MakePopup( false );
			m_pGameUI->SetLoadingBackgroundDialog( pPanel->GetVPanel() );
		}		
	}

#if defined( _X360 )
	m_pScoreboard = (CTFClientScoreBoardDialog *)( gViewPortInterface->FindPanelByName( PANEL_SCOREBOARD ) );
	Assert( m_pScoreboard );
#endif

	BaseClass::Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeTFNormal::Shutdown()
{
	DestroyStatsSummaryPanel();
}

void ClientModeTFNormal::InitViewport()
{
	m_pViewport = new TFViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}

ClientModeTFNormal g_ClientModeNormal;

IClientMode *GetClientModeNormal()
{
	return &g_ClientModeNormal;
}


ClientModeTFNormal* GetClientModeTFNormal()
{
	Assert( dynamic_cast< ClientModeTFNormal* >( GetClientModeNormal() ) );

	return static_cast< ClientModeTFNormal* >( GetClientModeNormal() );
}

//-----------------------------------------------------------------------------
// Purpose: Fixes some bugs from base class.
//-----------------------------------------------------------------------------
void ClientModeTFNormal::OverrideView( CViewSetup *pSetup )
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pPlayer )
		return;

	// Let the player override the view.
	pPlayer->OverrideView( pSetup );

	if ( ::input->CAM_IsThirdPerson() )
	{
		int iObserverMode = pPlayer->GetObserverMode();
		if ( iObserverMode == OBS_MODE_NONE || iObserverMode == OBS_MODE_IN_EYE )
		{
			QAngle camAngles;
			if ( g_ThirdPersonManager.IsOverridingThirdPerson() == false )
			{
				VectorCopy( pSetup->angles, camAngles );
			}
			else
			{
				const Vector& cam_ofs = g_ThirdPersonManager.GetCameraOffsetAngles();
				camAngles[PITCH] = cam_ofs[PITCH];
				camAngles[YAW] = cam_ofs[YAW];
				camAngles[ROLL] = 0;

				// Override angles from third person camera
				VectorCopy( camAngles, pSetup->angles );
			}

			Vector camForward, camRight, camUp, cam_ofs_distance;

			// get the forward vector
			AngleVectors( camAngles, &camForward, &camRight, &camUp );

			if ( g_ThirdPersonManager.IsOverridingThirdPerson() == false )
			{
				cam_ofs_distance = g_ThirdPersonManager.GetFinalCameraOffset();
			}
			else
			{
				cam_ofs_distance = g_ThirdPersonManager.GetDesiredCameraOffset();
			}

			cam_ofs_distance *= g_ThirdPersonManager.GetDistanceFraction();

			VectorMA( pSetup->origin, -cam_ofs_distance[0], camForward, pSetup->origin );
			VectorMA( pSetup->origin, cam_ofs_distance[1], camRight, pSetup->origin );
			VectorMA( pSetup->origin, cam_ofs_distance[2], camUp, pSetup->origin );
		}
	}
	else if ( ::input->CAM_IsOrthographic() )
	{
		pSetup->m_bOrtho = true;
		float w, h;
		::input->CAM_OrthographicSize( w, h );
		w *= 0.5f;
		h *= 0.5f;
		pSetup->m_OrthoLeft = -w;
		pSetup->m_OrthoTop = -h;
		pSetup->m_OrthoRight = w;
		pSetup->m_OrthoBottom = h;
	}
}

ConVar v_viewmodel_fov_dod( "viewmodel_dod_fov", "54", FCVAR_ARCHIVE, "Sets the field-of-view for the viewmodel.", true, 0.1, true, 179.9, true, 54, true, 70, NULL );
ConVar v_viewmodel_fov_cs( "viewmodel_cs_fov", "74", FCVAR_ARCHIVE, "Sets the field-of-view for the viewmodel.", true, 0.1, true, 179.9, true, 54, true, 70, NULL );
ConVar viewmodel_fov_override( "viewmodel_fov_override", "0", FCVAR_ARCHIVE, "Overrides default field-of-view for the viewmodel." );

extern ConVar v_viewmodel_fov;
float ClientModeTFNormal::GetViewModelFOV( void )
{
	float flFov = 54.0f;

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pPlayer )
		return flFov;

	if ( pPlayer->IsDODClass() )
	{
		if ( viewmodel_fov_override.GetBool() )
			return v_viewmodel_fov_dod.GetFloat();

		flFov = 90.0f;

		CPCWeaponBase *pWpn = pPlayer->GetActivePCWeapon();

		if( pWpn )
		{
			flFov = pWpn->GetPCWpnData().m_DODWeaponData.m_flViewModelFOV;
		}
	}
	else if ( pPlayer->IsCSClass() )
	{
		viewmodel_fov_override.GetBool() ? flFov = v_viewmodel_fov_cs.GetFloat() : flFov = 74.0f;
	}
	else
	{
		flFov = v_viewmodel_fov.GetFloat();
	}

	return flFov;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ClientModeTFNormal::ShouldDrawViewModel()
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( pPlayer )
	{
		if ( pPlayer->m_Shared.InCond( TF_COND_ZOOMED ) )
			return false;

		if ( pPlayer->IsDODClass() )
		{
			CWeaponDODBase *pDODWeapon = dynamic_cast<CWeaponDODBase*>( pPlayer->GetActivePCWeapon() );
			if ( pDODWeapon && !pDODWeapon->ShouldDrawViewModel() )
				return false;
		}
	}

	return true;
}

int ClientModeTFNormal::GetDeathMessageStartHeight( void )
{
	return m_pViewport->GetDeathMessageStartHeight();
}

void ClientModeTFNormal::FireGameEvent( IGameEvent *event )
{
	const char *eventname = event->GetName();

	if ( !eventname || !eventname[0] )
		return;

	if ( Q_strcmp( "player_changename", eventname ) == 0 )
	{
		return; // server sends a colorized text string for this
	}

	BaseClass::FireGameEvent( event );
}


void ClientModeTFNormal::PostRenderVGui()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ClientModeTFNormal::CreateMove( float flInputSampleTime, CUserCmd *cmd )
{
	return BaseClass::CreateMove( flInputSampleTime, cmd );
}

//-----------------------------------------------------------------------------
// Purpose: See if hud elements want key input. Return 0 if the key is swallowed
//-----------------------------------------------------------------------------
int	ClientModeTFNormal::HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	// Let scoreboard handle input first because on X360 we need gamertags and
	// gamercards accessible at all times when gamertag is visible.
#if defined( _X360 )
	if ( m_pScoreboard )
	{
		if ( !m_pScoreboard->HudElementKeyInput( down, keynum, pszCurrentBinding ) )
		{
			return 0;
		}
	}
#endif

	// check for hud menus
	if ( m_pMenuEngyBuild )
	{
		if ( !m_pMenuEngyBuild->HudElementKeyInput( down, keynum, pszCurrentBinding ) )
		{
			return 0;
		}
	}

	if ( m_pMenuEngyDestroy )
	{
		if ( !m_pMenuEngyDestroy->HudElementKeyInput( down, keynum, pszCurrentBinding ) )
		{
			return 0;
		}
	}

	if ( m_pMenuSpyDisguise )
	{
		if ( !m_pMenuSpyDisguise->HudElementKeyInput( down, keynum, pszCurrentBinding ) )
		{
			return 0;
		}
	}

	if ( m_pFreezePanel )
	{
		m_pFreezePanel->HudElementKeyInput( down, keynum, pszCurrentBinding );
	}

	return BaseClass::HudElementKeyInput( down, keynum, pszCurrentBinding );
}

//-----------------------------------------------------------------------------
// Purpose: See if spectator input occurred. Return 0 if the key is swallowed.
//-----------------------------------------------------------------------------
int ClientModeTFNormal::HandleSpectatorKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
#if defined( _X360 )
	// On X360 when we have scoreboard up in spectator menu we cannot
	// steal any input because gamertags must be selectable and gamercards
	// must be accessible.
	// We cannot rely on any keybindings in this case since user could have
	// remapped everything.
	if ( m_pScoreboard && m_pScoreboard->IsVisible() )
	{
		return 1;
	}
#endif

	return BaseClass::HandleSpectatorKeyInput( down, keynum, pszCurrentBinding );
}

//----------------------------------------------------------------------------
bool ClientModeTFNormal::IsSpyDisguiseVisible() const
{
	return m_pMenuSpyDisguise && m_pMenuSpyDisguise->IsVisible();
}

//-----------------------------------------------------------------------------
// Receive the BreakModel user message
//-----------------------------------------------------------------------------
void HandleBreakModel( bf_read &msg, bool bCheap )
{
	int nModelIndex = (int)msg.ReadShort();
	CUtlVector<breakmodel_t>	aGibs;
	BuildGibList( aGibs, nModelIndex, 1.0f, COLLISION_GROUP_NONE );
	if ( !aGibs.Count() )
		return;

	// Get the origin & angles
	Vector vecOrigin;
	QAngle vecAngles;
	int nSkin = 0;
	msg.ReadBitVec3Coord( vecOrigin );
	if ( !bCheap )
	{
		msg.ReadBitAngles( vecAngles );
		nSkin = (int)msg.ReadShort();
	}
	else
	{
		vecAngles = vec3_angle;
	}

	// Launch it straight up with some random spread
	Vector vecBreakVelocity = Vector(0,0,200);
	AngularImpulse angularImpulse( RandomFloat( 0.0f, 120.0f ), RandomFloat( 0.0f, 120.0f ), 0.0 );
	breakablepropparams_t breakParams( vecOrigin, vecAngles, vecBreakVelocity, angularImpulse );
	breakParams.impactEnergyScale = 1.0f;
	breakParams.nDefaultSkin = nSkin;

	CreateGibsFromList( aGibs, nModelIndex, NULL, breakParams, NULL, -1 , false, true );
}

// Receive the PlayerExtinguished user message and send out a clientside event for achievements to hook.
void __MsgFunc_PlayerExtinguished( bf_read &msg )
{
	int iMedicEntIndex = (int) msg.ReadByte();
	int iVictimEntIndex = (int) msg.ReadByte();

	IGameEvent *event = gameeventmanager->CreateEvent( "player_extinguished" );
	if ( event )
	{
		event->SetInt( "victim", iVictimEntIndex );
		event->SetInt( "healer", iMedicEntIndex );

		gameeventmanager->FireEventClientSide( event );
	}
}

//-----------------------------------------------------------------------------
// Receive the BreakModel user message
//-----------------------------------------------------------------------------
void __MsgFunc_BreakModel( bf_read &msg )
{
	HandleBreakModel( msg, false );
}

//-----------------------------------------------------------------------------
// Receive the CheapBreakModel user message
//-----------------------------------------------------------------------------
void __MsgFunc_CheapBreakModel( bf_read &msg )
{
	HandleBreakModel( msg, true );
}

void __MsgFunc_ForcePlayerViewAngles( bf_read &msg )
{
	// Read flag byte. Should be 1 right now.
	int iFlags = (int)msg.ReadByte();
	NOTE_UNUSED( iFlags );
	Assert( iFlags == 0x01 );

	int iPlayerEntIndex = (int)msg.ReadByte();
	C_TFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( iPlayerEntIndex ) );
	if ( pPlayer )
	{
		QAngle viewangles;
		msg.ReadBitAngles( viewangles );

		// Set the magic angles here!
		pPlayer->SetLocalAngles( viewangles );
		pPlayer->SetAbsAngles( viewangles );
		pPlayer->SetTauntYaw( viewangles[YAW] );
	}
}