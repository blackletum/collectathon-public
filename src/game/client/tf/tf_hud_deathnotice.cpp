//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Draws CSPort's death notices
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "c_playerresource.h"
#include "iclientmode.h"
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <KeyValues.h>
#include "c_baseplayer.h"
#include "c_team.h"

#include "hud_basedeathnotice.h"

#include "tf_shareddefs.h"
#include "clientmode_tf.h"
#include "c_tf_player.h"
#include "c_tf_playerresource.h"
#include "tf_hud_freezepanel.h"
#include "engine/IEngineSound.h"
#include "tf_controls.h"
#include "tf_gamerules.h"
#include "tf_weapon_medigun.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Must match resource/tf_objects.txt!!!
const char *szLocalizedObjectNames[OBJ_LAST] =
{
	"#TF_Object_Dispenser",
	"#TF_Object_Tele",
	"#TF_Object_Sentry",
	"#TF_object_Sapper"
};

class CTFHudDeathNotice : public CHudBaseDeathNotice
{
	DECLARE_CLASS_SIMPLE( CTFHudDeathNotice, CHudBaseDeathNotice );
public:
	CTFHudDeathNotice( const char *pElementName ) : CHudBaseDeathNotice( pElementName ) {};
	virtual void Init( void );
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual bool IsVisible( void );
	virtual bool ShouldDraw( void );

	virtual void FireGameEvent( IGameEvent *event );
	void PlayRivalrySounds( int iKillerIndex, int iVictimIndex, int iType  );
	virtual bool ShouldShowDeathNotice( IGameEvent *event );

protected:	
	virtual void OnGameEvent( IGameEvent *event, int iDeathNoticeMsg );
	virtual Color GetTeamColor( int iTeamNumber, bool bLocalPlayerInvolved = false );
	virtual Color GetInfoTextColor( int iDeathNoticeMsg );
	virtual Color GetBackgroundColor ( int iDeathNoticeMsg );
	virtual bool EventIsPlayerDeath( const char *eventName );

private:
	void AddAdditionalMsg( int iKillerID, int iVictimID, const char *pMsgKey );

	CHudTexture		*m_iconDomination;

	CPanelAnimationVar( Color, m_clrBlueText, "TeamBlue", "153 204 255 255" );
	CPanelAnimationVar( Color, m_clrRedText, "TeamRed", "255 64 64 255" );
	CPanelAnimationVar( Color, m_clrPurpleText, "PurpleText", "134 80 172 255" );
	CPanelAnimationVar( Color, m_clrGreenText, "GreenText", "112 176 74 255" );
	CPanelAnimationVar( Color, m_clrLocalPlayerText, "LocalPlayerColor", "65 65 65 255" );

};

DECLARE_HUDELEMENT( CTFHudDeathNotice );

void CTFHudDeathNotice::Init()
{
	BaseClass::Init();
}

void CTFHudDeathNotice::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_iconDomination = gHUD.GetIcon( "leaderboard_dominated" );
}

bool CTFHudDeathNotice::IsVisible( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return BaseClass::IsVisible();
}

bool CTFHudDeathNotice::ShouldDraw( void )
{
	return true;
}

bool CTFHudDeathNotice::ShouldShowDeathNotice( IGameEvent *event )
{ 
	if ( event->GetBool( "silent_kill" ) )
	{
		// Don't show a kill event for the team of the silent kill victim.
		int iVictimID = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
		C_TFPlayer* pVictim = ToTFPlayer( UTIL_PlayerByIndex( iVictimID ) );
		if ( pVictim && pVictim->GetTeamNumber() == GetLocalPlayerTeam() && iVictimID != GetLocalPlayerIndex() )
		{
			return false;
		}
	}

	if ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() && ( event->GetInt( "death_flags" ) & TF_DEATH_MINIBOSS ) == 0 )
	{
		int iLocalPlayerIndex = GetLocalPlayerIndex();

		if ( iLocalPlayerIndex != engine->GetPlayerForUserID( event->GetInt( "attacker" ) ) && 
			 iLocalPlayerIndex != engine->GetPlayerForUserID( event->GetInt( "assister" ) ) )
		{
			C_TFPlayer* pVictim = ToTFPlayer( UTIL_PlayerByIndex( engine->GetPlayerForUserID( event->GetInt( "userid" ) ) ) );
			if ( pVictim && pVictim->GetTeamNumber() == TF_TEAM_PVE_INVADERS )
			{
				bool bImportantClass = false;
				// If they have greater max health than their class usually does they're most likely something more important like a steel gauntlet
				if (pVictim->GetMaxHealth() > pVictim->GetPlayerClass()->GetMaxHealth() * 1.5)
					bImportantClass = true;

				// check for important support classes
				if (!bImportantClass)
				{
					C_WeaponMedigun* pMedigun = static_cast<C_WeaponMedigun*>(pVictim->Weapon_OwnsThisID(TF_WEAPON_MEDIGUN));
					switch (pVictim->GetPlayerClass()->GetClassIndex())
					{
					case TF_CLASS_ENGINEER:
					case TF_CLASS_SPY:
						bImportantClass = true;
						break;
					case TF_CLASS_SNIPER:
						// HACK: don't wanna display something like bowmen unless they're giant
						if (pVictim->Weapon_OwnsThisID(TF_WEAPON_SNIPERRIFLE))
							bImportantClass = true;
						break;
					case TF_CLASS_MEDIC:
						// Display medics that are healing giants
						if (pMedigun)
						{
							C_TFPlayer* pHealTarget = ToTFPlayer(pMedigun->GetHealTarget());
							if (pHealTarget && pHealTarget->IsMiniBoss())
								bImportantClass = true;
						}
						break;
					default:
						break;
					}
				}

				if (!bImportantClass)
					return false;
			}
		}
	}

	return true;
}

void CTFHudDeathNotice::PlayRivalrySounds( int iKillerIndex, int iVictimIndex, int iType )
{
	int iLocalPlayerIndex = GetLocalPlayerIndex();

	//We're not involved in this kill
	if ( iKillerIndex != iLocalPlayerIndex && iVictimIndex != iLocalPlayerIndex )
		return;

	CTFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( iLocalPlayerIndex ) );

	const char *pszSoundName = NULL;

	if ( iType == TF_DEATH_DOMINATION )
	{
		if ( iKillerIndex == iLocalPlayerIndex )
		{
			pszSoundName = "Game.Domination";

			if ( pPlayer && pPlayer->IsDODClass() )
				pszSoundName = "DOD.Domination";
		}
		else if ( iVictimIndex == iLocalPlayerIndex )
		{
			pszSoundName = "Game.Nemesis";

			if ( pPlayer && pPlayer->IsDODClass() )
				pszSoundName = "DOD.Nemesis";
		}
	}
	else if ( iType == TF_DEATH_REVENGE )
	{
		pszSoundName = "Game.Revenge";

		if ( pPlayer && pPlayer->IsDODClass() )
			pszSoundName = "DOD.Revenge";
	}

	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, pszSoundName );
}

//-----------------------------------------------------------------------------
// Purpose: Server's told us that someone's died
//-----------------------------------------------------------------------------
void CTFHudDeathNotice::FireGameEvent( IGameEvent *event )
{
	BaseClass::FireGameEvent( event );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFHudDeathNotice::EventIsPlayerDeath( const char* eventName )
{
	return BaseClass::EventIsPlayerDeath( eventName );
}

//-----------------------------------------------------------------------------
// Purpose: Called when a game event happens and a death notice is about to be 
//			displayed.  This method can examine the event and death notice and
//			make game-specific tweaks to it before it is displayed
//-----------------------------------------------------------------------------
void CTFHudDeathNotice::OnGameEvent( IGameEvent *event, int iDeathNoticeMsg )
{
	const char *pszEventName = event->GetName();

	if ( FStrEq( pszEventName, "player_death" ) || FStrEq( pszEventName, "object_destroyed" ) )
	{
		bool bIsObjectDestroyed = FStrEq( pszEventName, "object_destroyed" );
		int iCustomDamage = event->GetInt( "customkill" );
		int iLocalPlayerIndex = GetLocalPlayerIndex();

		const int iKillerID = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
		const int iVictimID = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
		// if there was an assister, put both the killer's and assister's names in the death message
		int iAssisterID = engine->GetPlayerForUserID( event->GetInt( "assister" ) );

		const char *assister_name = ( iAssisterID > 0 ? g_PR->GetPlayerName( iAssisterID ) : NULL );

		bool bMultipleKillers = false;

		if ( assister_name )
		{
			DeathNoticeItem &msg = m_DeathNotices[ iDeathNoticeMsg ];
			const char *pszKillerName = msg.Killer.szName;
			const char *pszAssisterName = assister_name;

			char szKillerBuf[MAX_PLAYER_NAME_LENGTH*2];
			Q_snprintf( szKillerBuf, ARRAYSIZE(szKillerBuf), "%s + %s", pszKillerName, pszAssisterName );
			Q_strncpy( msg.Killer.szName, szKillerBuf, ARRAYSIZE( msg.Killer.szName ) );
			if ( iLocalPlayerIndex == iAssisterID )
			{
				msg.bLocalPlayerInvolved = true;
			}

			bMultipleKillers = true;
		}

		int deathFlags = event->GetInt( "death_flags" );

		if ( !bIsObjectDestroyed )
		{
			// if this death involved a player dominating another player or getting revenge on another player, add an additional message
			// mentioning that
			
			// WARNING: AddAdditionalMsg will grow and potentially realloc the m_DeathNotices array. So be careful
			//	using pointers to m_DeathNotices elements...
		
			if ( deathFlags & TF_DEATH_DOMINATION )
			{
				AddAdditionalMsg( iKillerID, iVictimID, "#Msg_Dominating" );
				PlayRivalrySounds( iKillerID, iVictimID, TF_DEATH_DOMINATION );
			}
			if ( deathFlags & TF_DEATH_ASSISTER_DOMINATION && ( iAssisterID > 0 ) )
			{
				AddAdditionalMsg( iAssisterID, iVictimID, "#Msg_Dominating" );
				PlayRivalrySounds( iAssisterID, iVictimID, TF_DEATH_DOMINATION );
			}
			if ( deathFlags & TF_DEATH_REVENGE ) 
			{
				AddAdditionalMsg( iKillerID, iVictimID, "#Msg_Revenge" );
				PlayRivalrySounds( iKillerID, iVictimID, TF_DEATH_REVENGE );
			}
			if ( deathFlags & TF_DEATH_ASSISTER_REVENGE && ( iAssisterID > 0 ) ) 
			{
				AddAdditionalMsg( iAssisterID, iVictimID, "#Msg_Revenge" );
				PlayRivalrySounds( iAssisterID, iVictimID, TF_DEATH_REVENGE );
			}
		}
		else
		{
			// if this is an object destroyed message, set the victim name to "<object type> (<owner>)"
			int iObjectType = event->GetInt( "objecttype" );
			if ( iObjectType >= 0 && iObjectType < OBJ_LAST )
			{
				// get the localized name for the object
				char szLocalizedObjectName[MAX_PLAYER_NAME_LENGTH];
				szLocalizedObjectName[ 0 ] = 0;
				const wchar_t *wszLocalizedObjectName = g_pVGuiLocalize->Find( szLocalizedObjectNames[iObjectType] );
				if ( wszLocalizedObjectName )
				{
					g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalizedObjectName, szLocalizedObjectName, ARRAYSIZE( szLocalizedObjectName ) );
				}
				else
				{
					Warning( "Couldn't find localized object name for '%s'\n", szLocalizedObjectNames[iObjectType] );
					Q_strncpy( szLocalizedObjectName, szLocalizedObjectNames[iObjectType], sizeof( szLocalizedObjectName ) );
				}

				// compose the string
				DeathNoticeItem &msg = m_DeathNotices[ iDeathNoticeMsg ];
				if ( msg.Victim.szName[0] )
				{
					char szVictimBuf[MAX_PLAYER_NAME_LENGTH*2];
					Q_snprintf( szVictimBuf, ARRAYSIZE(szVictimBuf), "%s (%s)", szLocalizedObjectName, msg.Victim.szName );
					Q_strncpy( msg.Victim.szName, szVictimBuf, ARRAYSIZE( msg.Victim.szName ) );
				}
				else
				{
					Q_strncpy( msg.Victim.szName, szLocalizedObjectName, ARRAYSIZE( msg.Victim.szName ) );
				}
				
			}
			else
			{
				Assert( false ); // invalid object type
			}
		}

		const wchar_t *pMsg = NULL;
		DeathNoticeItem &msg = m_DeathNotices[ iDeathNoticeMsg ];

		switch ( iCustomDamage )
		{
		case TF_DMG_CUSTOM_BACKSTAB:
			Q_strncpy( msg.szIcon, "d_backstab", ARRAYSIZE( msg.szIcon ) );
			break;
		case TF_DMG_CUSTOM_HEADSHOT_DECAPITATION:
		case TF_DMG_CUSTOM_HEADSHOT:
			Q_strncpy( msg.szIcon, "d_headshot", ARRAYSIZE( msg.szIcon ) );
			break;
		case TF_DMG_CUSTOM_BURNING:
			if ( event->GetInt( "attacker" ) == event->GetInt( "userid" ) )
			{
				// suicide by fire
				Q_strncpy( msg.szIcon, "d_firedeath", ARRAYSIZE( msg.szIcon ) );
				msg.wzInfoText[0] = 0;
			}
			break;
		case TF_DMG_CUSTOM_PUMPKIN_BOMB:
			// special-case if the player is killed by a pumpkin bomb
			Q_strncpy( msg.szIcon, "d_pumpkindeath", ARRAYSIZE( msg.szIcon ) );
			msg.wzInfoText[0] = 0;
			break;
		case TF_DMG_CUSTOM_SUICIDE:
			{
				// display a different message if this was suicide, or assisted suicide (suicide w/recent damage, kill awarded to damager)
				bool bAssistedSuicide = event->GetInt( "userid" ) != event->GetInt( "attacker" );
				pMsg = g_pVGuiLocalize->Find( ( bAssistedSuicide ) ? ( bMultipleKillers ? "#DeathMsg_AssistedSuicide_Multiple" : "#DeathMsg_AssistedSuicide" ) : ( "#DeathMsg_Suicide" ) );
				if ( pMsg )
				{
					V_wcsncpy( msg.wzInfoText, pMsg, sizeof( msg.wzInfoText ) );
				}			
				break;
			}
		case TF_DMG_CUSTOM_EYEBALL_ROCKET:
			{
				if ( msg.Killer.iTeam == TEAM_UNASSIGNED )
				{
					char szLocalizedName[MAX_PLAYER_NAME_LENGTH];
					szLocalizedName[ 0 ] = 0;
					const wchar_t *wszLocalizedName = g_pVGuiLocalize->Find( "#TF_HALLOWEEN_EYEBALL_BOSS_DEATHCAM_NAME" );
					if ( wszLocalizedName )
					{
						g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalizedName, szLocalizedName, ARRAYSIZE( szLocalizedName ) );
						Q_strncpy( msg.Killer.szName, szLocalizedName, ARRAYSIZE( msg.Killer.szName ) );
						msg.Killer.iTeam = TF_TEAM_HALLOWEEN; // This will set the name to purple for MONOCULUS!
					}
				}
				break;
			}
		case TF_DMG_CUSTOM_MERASMUS_ZAP:
		case TF_DMG_CUSTOM_MERASMUS_GRENADE:
		case TF_DMG_CUSTOM_MERASMUS_DECAPITATION:
			{
				if ( msg.Killer.iTeam == TEAM_UNASSIGNED )
				{
					char szLocalizedName[MAX_PLAYER_NAME_LENGTH];
					szLocalizedName[ 0 ] = 0;
					const wchar_t *wszLocalizedName = g_pVGuiLocalize->Find( "#TF_HALLOWEEN_MERASMUS_DEATHCAM_NAME" );
					if ( wszLocalizedName )
					{
						g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalizedName, szLocalizedName, ARRAYSIZE( szLocalizedName ) );
						Q_strncpy( msg.Killer.szName, szLocalizedName, ARRAYSIZE( msg.Killer.szName ) );
						msg.Killer.iTeam = TF_TEAM_HALLOWEEN; // This will set the name to green for MERASMUS!
					}
				}
				break;
			}
		case TF_DMG_CUSTOM_SPELL_SKELETON:
			{
				if ( msg.Killer.iTeam == TEAM_UNASSIGNED )
				{
					char szLocalizedName[MAX_PLAYER_NAME_LENGTH];
					szLocalizedName[ 0 ] = 0;
					const wchar_t *wszLocalizedName = g_pVGuiLocalize->Find( "#TF_HALLOWEEN_SKELETON_DEATHCAM_NAME" );
					if ( wszLocalizedName )
					{
						g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalizedName, szLocalizedName, ARRAYSIZE( szLocalizedName ) );
						Q_strncpy( msg.Killer.szName, szLocalizedName, ARRAYSIZE( msg.Killer.szName ) );
						msg.Killer.iTeam = TF_TEAM_HALLOWEEN; // This will set the name to green for THE UNDEAD!
					}
				}
				break;
			}

		case TF_DMG_CUSTOM_KART:
			// special-case if the player is pushed by kart
			Q_strncpy( msg.szIcon, "d_bumper_kart", ARRAYSIZE( msg.szIcon ) );
			msg.wzInfoText[0] = 0;
			break;
		case TF_DMG_CUSTOM_GIANT_HAMMER:
			// special-case Giant hammer
			Q_strncpy( msg.szIcon, "d_necro_smasher", ARRAYSIZE( msg.szIcon ) );
			msg.wzInfoText[0] = 0;
			break;
		default:
			break;
		}

		if ( ( event->GetInt( "damagebits" ) & DMG_NERVEGAS )  )
		{
			// special case icon for hit-by-vehicle death
			Q_strncpy( msg.szIcon, "d_saw_kill", ARRAYSIZE( msg.szIcon ) );
		}
	} 
	else if ( FStrEq( "teamplay_point_captured", pszEventName ) ||
			  FStrEq( "teamplay_capture_blocked", pszEventName ) || 
			  FStrEq( "teamplay_flag_event", pszEventName ) )
	{
		bool bDefense = ( FStrEq( "teamplay_capture_blocked", pszEventName ) || ( FStrEq( "teamplay_flag_event", pszEventName ) &&
			TF_FLAGEVENT_DEFEND == event->GetInt( "eventtype" ) ) );

		DeathNoticeItem &msg = m_DeathNotices[ iDeathNoticeMsg ];
		const char *szCaptureIcons[] = { "d_redcapture", "d_bluecapture" };
		const char *szDefenseIcons[] = { "d_reddefend", "d_bluedefend" };
		
		int iTeam = msg.Killer.iTeam;
		Assert( iTeam >= FIRST_GAME_TEAM );
		Assert( iTeam < FIRST_GAME_TEAM + TF_TEAM_COUNT );
		if ( iTeam < FIRST_GAME_TEAM || iTeam >= FIRST_GAME_TEAM + TF_TEAM_COUNT )
			return;

		int iIndex = msg.Killer.iTeam - FIRST_GAME_TEAM;
		Assert( iIndex < ARRAYSIZE( szCaptureIcons ) );

		Q_strncpy( msg.szIcon, bDefense ? szDefenseIcons[iIndex] : szCaptureIcons[iIndex], ARRAYSIZE( msg.szIcon ) );
	}
	else if ( FStrEq( "rd_robot_killed", pszEventName ) )
	{
		DeathNoticeItem &msg = m_DeathNotices[ iDeathNoticeMsg ];

		int killer = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
		const char *killedwith = event->GetString( "weapon" );

		msg.Killer.iTeam = g_PR->GetTeam( killer );
		Q_strncpy( msg.Killer.szName, g_PR->GetPlayerName( killer ), ARRAYSIZE( msg.Killer.szName ) );

		Q_strncpy( msg.Victim.szName, g_PR->GetTeam( killer ) == TF_TEAM_RED ? "BLUE ROBOT" : "RED ROBOT", ARRAYSIZE( msg.Victim.szName ) );
		msg.Victim.iTeam = g_PR->GetTeam( killer ) == TF_TEAM_RED ? TF_TEAM_BLUE : TF_TEAM_RED;

		Q_snprintf( msg.szIcon, sizeof(msg.szIcon), "d_%s", killedwith );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds an additional death message
//-----------------------------------------------------------------------------
void CTFHudDeathNotice::AddAdditionalMsg( int iKillerID, int iVictimID, const char *pMsgKey )
{
	DeathNoticeItem &msg2 = m_DeathNotices[AddDeathNoticeItem()];
	Q_strncpy( msg2.Killer.szName, g_PR->GetPlayerName( iKillerID ), ARRAYSIZE( msg2.Killer.szName ) );
	msg2.Killer.iTeam = g_PR->GetTeam( iKillerID );
	Q_strncpy( msg2.Victim.szName, g_PR->GetPlayerName( iVictimID ), ARRAYSIZE( msg2.Victim.szName ) );
	msg2.Victim.iTeam = g_PR->GetTeam( iVictimID );
	const wchar_t *wzMsg =  g_pVGuiLocalize->Find( pMsgKey );
	if ( wzMsg )
	{
		V_wcsncpy( msg2.wzInfoText, wzMsg, sizeof( msg2.wzInfoText ) );
	}
	msg2.iconDeath = m_iconDomination;
	int iLocalPlayerIndex = GetLocalPlayerIndex();
	if ( iLocalPlayerIndex == iVictimID || iLocalPlayerIndex == iKillerID )
	{
		msg2.bLocalPlayerInvolved = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the color to draw text in for this team.  
//-----------------------------------------------------------------------------
Color CTFHudDeathNotice::GetTeamColor( int iTeamNumber, bool bLocalPlayerInvolved /* = false */ )
{
	switch ( iTeamNumber )
	{
	case TF_TEAM_BLUE:
		return m_clrBlueText;
		break;
	case TF_TEAM_RED:
		return m_clrRedText;
		break;
	case TEAM_UNASSIGNED:
		if ( bLocalPlayerInvolved )
			return m_clrLocalPlayerText;
		else
			return Color( 255, 255, 255, 255 );
		break;
	default:
		AssertOnce( false );	// invalid team
		return Color( 255, 255, 255, 255 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Color CTFHudDeathNotice::GetInfoTextColor( int iDeathNoticeMsg )
{ 
	DeathNoticeItem &msg = m_DeathNotices[ iDeathNoticeMsg ];

	if ( msg.bLocalPlayerInvolved )
		return m_clrLocalPlayerText;

	return Color( 255, 255, 255, 255 );
}

//-----------------------------------------------------------------------------
Color CTFHudDeathNotice::GetBackgroundColor ( int iDeathNoticeMsg )
{ 
	DeathNoticeItem &msg = m_DeathNotices[ iDeathNoticeMsg ];

	return msg.bLocalPlayerInvolved ? m_clrLocalBGColor : m_clrBaseBGColor; 
}