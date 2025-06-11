//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "iclientmode.h"
#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui/ISurface.h>
#include <vgui/IImage.h>
#include <vgui_controls/Label.h>

#include "hud_numericdisplay.h"
#include "c_team.h"
#include "c_tf_player.h"
#include "tf_shareddefs.h"
#include "tf_hud_playerstatus.h"

using namespace vgui;


extern ConVar tf_max_health_boost;

static const char *g_szBlueClassImages[] = 
{ 
	"",
	"../hud/class_scoutblue", 
	"../hud/class_sniperblue",
	"../hud/class_soldierblue",
	"../hud/class_demoblue",
	"../hud/class_medicblue",
	"../hud/class_heavyblue",
	"../hud/class_pyroblue",
	"../hud/class_spyblue",
	"../hud/class_engiblue",
};

static const char *g_szRedClassImages[] = 
{ 
	"",
	"../hud/class_scoutred", 
	"../hud/class_sniperred",
	"../hud/class_soldierred",
	"../hud/class_demored",
	"../hud/class_medicred",
	"../hud/class_heavyred",
	"../hud/class_pyrored",
	"../hud/class_spyred",
	"../hud/class_engired",
};

enum
{
	HUD_HEALTH_NO_ANIM = 0,
	HUD_HEALTH_BONUS_ANIM,
	HUD_HEALTH_DYING_ANIM,
};

DECLARE_BUILD_FACTORY( CTFClassImage );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHudPlayerClass::CTFHudPlayerClass( Panel *parent, const char *name ) : EditablePanel( parent, name )
{
	m_pClassImage = NULL;
	m_pClassImageBG = NULL;
	m_pSpyImage = NULL;
	m_pSpyOutlineImage = NULL;

#ifdef TEMP_CALLS
//	m_pPlayerModelPanel = NULL;
	m_pPlayerModelPanelBG = NULL;
	m_pCarryingWeaponPanel = NULL;
	m_pCarryingLabel = NULL;
	m_pCarryingOwnerLabel = NULL;
	m_pCarryingBG = NULL;
#endif

	m_nTeam = TEAM_UNASSIGNED;
	m_nClass = TF_CLASS_UNDEFINED;
	m_nDisguiseTeam = TEAM_UNASSIGNED;
	m_nDisguiseClass = TF_CLASS_UNDEFINED;
	m_hDisguiseWeapon = NULL;
	m_flNextThink = 0.0f;

	ListenForGameEvent( "localplayer_changedisguise" );

	for ( int i = 0; i < TF_LAST_NORMAL_CLASS; i++ )
	{
		// The materials are given to vgui via the SetImage() function, which prepends 
		// the "vgui/", so we need to precache them with the same.
		if ( g_szBlueClassImages[i] && g_szBlueClassImages[i][0] )
		{
			PrecacheMaterial( VarArgs( "vgui/%s", g_szBlueClassImages[i] ) );
			PrecacheMaterial( VarArgs( "vgui/%s_cloak", g_szBlueClassImages[i] ) );
			PrecacheMaterial( VarArgs( "vgui/%s_halfcloak", g_szBlueClassImages[i] ) );
		}
		if ( g_szRedClassImages[i] && g_szRedClassImages[i][0] )
		{
			PrecacheMaterial( VarArgs( "vgui/%s", g_szRedClassImages[i] ) );
			PrecacheMaterial( VarArgs( "vgui/%s_cloak", g_szRedClassImages[i] ) );
			PrecacheMaterial( VarArgs( "vgui/%s_halfcloak", g_szRedClassImages[i] ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerClass::Reset()
{
	m_flNextThink = gpGlobals->curtime + 0.05f;

	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HudSpyDisguiseHide" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerClass::ApplySchemeSettings( IScheme *pScheme )
{
	// load control settings...
	LoadControlSettings( "resource/UI/HudPlayerClass.res" );

	m_nTeam = TEAM_UNASSIGNED;
	m_nClass = TF_CLASS_UNDEFINED;
	m_nDisguiseTeam = TEAM_UNASSIGNED;
	m_nDisguiseClass = TF_CLASS_UNDEFINED;
	m_hDisguiseWeapon = NULL;
	m_flNextThink = 0.0f;
	m_nCloakLevel = 0;

#ifdef TEMP_CALLS
	m_pClassImage = FindControl<CTFClassImage>( "PlayerStatusClassImage", false );
	m_pClassImageBG = FindControl<CTFImagePanel>( "PlayerStatusClassImageBG", false );
	m_pSpyImage = FindControl<CTFImagePanel>( "PlayerStatusSpyImage", false );
	m_pSpyOutlineImage = FindControl<CTFImagePanel>( "PlayerStatusSpyOutlineImage", false );

//	m_pPlayerModelPanel = FindControl<CTFPlayerModelPanel>( "classmodelpanel", false );
	m_pPlayerModelPanelBG = FindControl<CTFImagePanel>( "classmodelpanelBG", false );

	m_pCarryingWeaponPanel = FindControl< EditablePanel >( "CarryingWeapon", false );
	if ( m_pCarryingWeaponPanel )
	{
		m_pCarryingLabel = m_pCarryingWeaponPanel->FindControl< CExLabel >( "CarryingLabel" );
		m_pCarryingOwnerLabel = m_pCarryingWeaponPanel->FindControl< Label >( "OwnerLabel" );
		m_pCarryingBG = m_pCarryingWeaponPanel->FindControl< CTFImagePanel >( "CarryingBackground" );
	}
#endif

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerClass::OnThink()
{
	if ( m_flNextThink > gpGlobals->curtime )
		return;

	m_flNextThink = gpGlobals->curtime + 0.5f;
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pPlayer )
		return;

	bool bTeamChange = false;

	// set our background colors
	if ( m_nTeam != pPlayer->GetTeamNumber() )
	{
		bTeamChange = true;
		m_nTeam = pPlayer->GetTeamNumber();
	}

	int nCloakLevel = 0;
	bool bCloakChange = false;
	float flInvis = pPlayer->GetPercentInvisible();

	if ( flInvis > 0.9 )
	{
		nCloakLevel = 2;
	}
	else if ( flInvis > 0.1 )
	{
		nCloakLevel = 1;
	}

	if ( nCloakLevel != m_nCloakLevel )
	{
		m_nCloakLevel = nCloakLevel;
		bCloakChange = true;
	}

	// set our class image
	if (	m_nClass != pPlayer->GetPlayerClass()->GetClassIndex() || bTeamChange || bCloakChange ||
			(
				m_nClass == TF_CLASS_SPY &&
				(
					m_nDisguiseClass != pPlayer->m_Shared.GetDisguiseClass() ||
					m_nDisguiseTeam != pPlayer->m_Shared.GetDisguiseTeam() ||
					m_hDisguiseWeapon != pPlayer->m_Shared.GetDisguiseWeapon()
				)
			)
		)
	{
		m_nClass = pPlayer->GetPlayerClass()->GetClassIndex();

		if ( m_nClass == TF_CLASS_SPY && pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) )
		{
			if ( !pPlayer->m_Shared.InCond( TF_COND_DISGUISING ) )
			{
				m_nDisguiseTeam = pPlayer->m_Shared.GetDisguiseTeam();
				m_nDisguiseClass = pPlayer->m_Shared.GetDisguiseClass();
				m_hDisguiseWeapon = pPlayer->m_Shared.GetDisguiseWeapon();
			}
		}
		else
		{
			m_nDisguiseTeam = TEAM_UNASSIGNED;
			m_nDisguiseClass = TF_CLASS_UNDEFINED;
			m_hDisguiseWeapon = NULL;
		}

		if ( m_pClassImage && m_pSpyImage )
		{
			m_pClassImage->SetVisible( true );
			m_pClassImageBG->SetVisible( true );

			int iCloakState = 0;
			if ( pPlayer->IsPlayerClass( TF_CLASS_SPY ) )
			{
				iCloakState = m_nCloakLevel;
			}

			if ( m_nDisguiseTeam != TEAM_UNASSIGNED || m_nDisguiseClass != TF_CLASS_UNDEFINED )
			{
				m_pSpyImage->SetVisible( true );
				m_pClassImage->SetClass( m_nDisguiseTeam, m_nDisguiseClass, iCloakState );
			}
			else
			{
				m_pSpyImage->SetVisible( false );
				m_pClassImage->SetClass( m_nTeam, m_nClass, iCloakState );
			}
		}
	}

#ifdef TEMP_CALLS
	m_pPlayerModelPanelBG->SetVisible( false );
	m_pCarryingWeaponPanel->SetVisible( false );
	m_pCarryingLabel->SetVisible( false );
	m_pCarryingOwnerLabel->SetVisible( false );
	m_pCarryingBG->SetVisible( false );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerClass::FireGameEvent( IGameEvent * event )
{
	if ( FStrEq( "localplayer_changedisguise", event->GetName() ) )
	{
		if ( m_pSpyImage && m_pSpyOutlineImage )
		{
			bool bFadeIn = event->GetBool( "disguised", false );

			if ( bFadeIn )
			{
				m_pSpyImage->SetAlpha( 0 );
			}
			else
			{
				m_pSpyImage->SetAlpha( 255 );
			}

			m_pSpyOutlineImage->SetAlpha( 0 );
			
			m_pSpyImage->SetVisible( true );
			m_pSpyOutlineImage->SetVisible( true );

			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( bFadeIn ? "HudSpyDisguiseFadeIn" : "HudSpyDisguiseFadeOut" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHealthPanel::CTFHealthPanel( Panel *parent, const char *name ) : vgui::Panel( parent, name )
{
	m_flHealth = 1.0f;

	m_iMaterialIndex = surface()->DrawGetTextureId( "hud/health_color" );
	if ( m_iMaterialIndex == -1 ) // we didn't find it, so create a new one
	{
		m_iMaterialIndex = surface()->CreateNewTextureID();	
		surface()->DrawSetTextureFile( m_iMaterialIndex, "hud/health_color", true, false );
	}

	m_iDeadMaterialIndex = surface()->DrawGetTextureId( "hud/health_dead" );
	if ( m_iDeadMaterialIndex == -1 ) // we didn't find it, so create a new one
	{
		m_iDeadMaterialIndex = surface()->CreateNewTextureID();	
		surface()->DrawSetTextureFile( m_iDeadMaterialIndex, "hud/health_dead", true, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHealthPanel::Paint()
{
	BaseClass::Paint();

	int x, y, w, h;
	GetBounds( x, y, w, h );

	Vertex_t vert[4];	
	float uv1 = 0.0f;
	float uv2 = 1.0f;
	int xpos = 0, ypos = 0;

	if ( m_flHealth <= 0 )
	{
		// Draw the dead material
		surface()->DrawSetTexture( m_iDeadMaterialIndex );
		
		vert[0].Init( Vector2D( xpos, ypos ), Vector2D( uv1, uv1 ) );
		vert[1].Init( Vector2D( xpos + w, ypos ), Vector2D( uv2, uv1 ) );
		vert[2].Init( Vector2D( xpos + w, ypos + h ), Vector2D( uv2, uv2 ) );				
		vert[3].Init( Vector2D( xpos, ypos + h ), Vector2D( uv1, uv2 ) );

		surface()->DrawSetColor( Color(255,255,255,255) );
	}
	else
	{
		float flDamageY = h * ( 1.0f - m_flHealth );

		// blend in the red "damage" part
		surface()->DrawSetTexture( m_iMaterialIndex );

		Vector2D uv11( uv1, uv2 - m_flHealth );
		Vector2D uv21( uv2, uv2 - m_flHealth );
		Vector2D uv22( uv2, uv2 );
		Vector2D uv12( uv1, uv2 );

		vert[0].Init( Vector2D( xpos, flDamageY ), uv11 );
		vert[1].Init( Vector2D( xpos + w, flDamageY ), uv21 );
		vert[2].Init( Vector2D( xpos + w, ypos + h ), uv22 );				
		vert[3].Init( Vector2D( xpos, ypos + h ), uv12 );

		surface()->DrawSetColor( GetFgColor() );
	}

	surface()->DrawTexturedPolygon( 4, vert );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHudPlayerHealth::CTFHudPlayerHealth( Panel *parent, const char *name ) : EditablePanel( parent, name )
{
	m_pHealthImage = new CTFHealthPanel( this, "PlayerStatusHealthImage" );	
	m_pHealthImageBG = new ImagePanel( this, "PlayerStatusHealthImageBG" );
	m_pHealthBonusImage = new ImagePanel( this, "PlayerStatusHealthBonusImage" );
	m_pBuildingHealthImageBG = new ImagePanel( this, "BuildingStatusHealthImageBG" );

#ifdef TEMP_CALLS
	m_pBleedImage = new ImagePanel( this, "PlayerStatusBleedImage" );
	m_pHookBleedImage = new ImagePanel( this, "PlayerStatusHookBleedImage" );
	m_pMarkedForDeathImage = new ImagePanel( this, "PlayerStatusMarkedForDeathImage" );
	m_pMarkedForDeathImageSilent = new ImagePanel( this, "PlayerStatusMarkedForDeathSilentImage" );
	m_pMilkImage = new ImagePanel( this, "PlayerStatusMilkImage" );
	m_pWheelOfDoomImage = new ImagePanel( this, "PlayerStatus_WheelOfDoom" );
	m_pGasImage = new ImagePanel( this, "PlayerStatusGasImage" );

	// Vaccinator
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_MEDIGUN_UBER_BULLET_RESIST, BUFF_CLASS_BULLET_RESIST, new ImagePanel( this, "PlayerStatus_MedicUberBulletResistImage" ),	"../HUD/defense_buff_bullet_blue",		"../HUD/defense_buff_bullet_red"  ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_MEDIGUN_UBER_BLAST_RESIST, BUFF_CLASS_BLAST_RESIST, new ImagePanel( this, "PlayerStatus_MedicUberBlastResistImage" ),		"../HUD/defense_buff_explosion_blue",	"../HUD/defense_buff_explosion_red" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_MEDIGUN_UBER_FIRE_RESIST, BUFF_CLASS_FIRE_RESIST, new ImagePanel( this, "PlayerStatus_MedicUberFireResistImage" ),		"../HUD/defense_buff_fire_blue",		"../HUD/defense_buff_fire_red" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_MEDIGUN_SMALL_BULLET_RESIST, BUFF_CLASS_BULLET_RESIST, new ImagePanel( this, "PlayerStatus_MedicSmallBulletResistImage" ),	"../HUD/defense_buff_bullet_blue",		"../HUD/defense_buff_bullet_red"  ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_MEDIGUN_SMALL_BLAST_RESIST, BUFF_CLASS_BLAST_RESIST, new ImagePanel( this, "PlayerStatus_MedicSmallBlastResistImage" ),	"../HUD/defense_buff_explosion_blue",	"../HUD/defense_buff_explosion_red" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_MEDIGUN_SMALL_FIRE_RESIST, BUFF_CLASS_FIRE_RESIST, new ImagePanel( this, "PlayerStatus_MedicSmallFireResistImage" ),		"../HUD/defense_buff_fire_blue",		"../HUD/defense_buff_fire_red" ) );
	// Soldier buffs
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_OFFENSEBUFF, BUFF_CLASS_SOLDIER_OFFENSE, new ImagePanel( this, "PlayerStatus_SoldierOffenseBuff" ),						"../Effects/soldier_buff_offense_blue",		"../Effects/soldier_buff_offense_red" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_DEFENSEBUFF, BUFF_CLASS_SOLDIER_DEFENSE, new ImagePanel( this, "PlayerStatus_SoldierDefenseBuff" ),						"../Effects/soldier_buff_defense_blue",		"../Effects/soldier_buff_defense_red" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_REGENONDAMAGEBUFF, BUFF_CLASS_SOLDIER_HEALTHONHIT, new ImagePanel( this, "PlayerStatus_SoldierHealOnHitBuff" ),			"../Effects/soldier_buff_healonhit_blue",	"../Effects/soldier_buff_healonhit_red" ) );
	// Powerup Rune status
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_STRENGTH, RUNE_CLASS_STRENGTH, new ImagePanel( this, "PlayerStatus_RuneStrength" ), "../Effects/powerup_strength_hud", "../Effects/powerup_strength_hud" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_HASTE, RUNE_CLASS_HASTE, new ImagePanel( this, "PlayerStatus_RuneHaste" ), "../Effects/powerup_haste_hud", "../Effects/powerup_haste_hud" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_REGEN, RUNE_CLASS_REGEN, new ImagePanel( this, "PlayerStatus_RuneRegen" ), "../Effects/powerup_regen_hud", "../Effects/powerup_regen_hud" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_RESIST, RUNE_CLASS_RESIST, new ImagePanel( this, "PlayerStatus_RuneResist" ), "../Effects/powerup_resist_hud", "../Effects/powerup_resist_hud" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_VAMPIRE, RUNE_CLASS_VAMPIRE, new ImagePanel( this, "PlayerStatus_RuneVampire" ), "../Effects/powerup_vampire_hud", "../Effects/powerup_vampire_hud" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_REFLECT, RUNE_CLASS_REFLECT, new ImagePanel( this, "PlayerStatus_RuneReflect" ), "../Effects/powerup_reflect_hud", "../Effects/powerup_reflect_hud" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_PRECISION, RUNE_CLASS_PRECISION, new ImagePanel( this, "PlayerStatus_RunePrecision" ), "../Effects/powerup_precision_hud", "../Effects/powerup_precision_hud" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_AGILITY, RUNE_CLASS_AGILITY, new ImagePanel( this, "PlayerStatus_RuneAgility" ), "../Effects/powerup_agility_hud", "../Effects/powerup_agility_hud" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_KNOCKOUT, RUNE_CLASS_KNOCKOUT, new ImagePanel( this, "PlayerStatus_RuneKnockout" ), "../Effects/powerup_knockout_hud", "../Effects/powerup_knockout_hud" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_KING, RUNE_CLASS_KING, new ImagePanel( this, "PlayerStatus_RuneKing" ), "../Effects/powerup_king_hud", "../Effects/powerup_king_hud" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_PLAGUE, RUNE_CLASS_PLAGUE, new ImagePanel( this, "PlayerStatus_RunePlague" ), "../Effects/powerup_plague_hud", "../Effects/powerup_plague_hud" ) );
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_RUNE_SUPERNOVA, RUNE_CLASS_SUPERNOVA, new ImagePanel( this, "PlayerStatus_RuneSupernova" ), "../Effects/powerup_supernova_hud", "../Effects/powerup_supernova_hud" ) );

	// Parachute
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_PARACHUTE_DEPLOYED, BUFF_CLASS_PARACHUTE, new ImagePanel( this, "PlayerStatus_Parachute" ), "../HUD/hud_parachute_active", "../HUD/hud_parachute_active" ) );

	// Slowed, note the condition prolly isn't right but we don't have slowing so it's fine cause it never draws anyways
	m_vecBuffInfo.AddToTail( new CTFBuffInfo( TF_COND_STUNNED, DEBUFF_CLASS_SLOWED, new ImagePanel( this, "PlayerStatusSlowed" ), "../vgui/slowed", "../vgui/slowed" ) );
#endif

	m_flNextThink = 0.0f;

	m_nBonusHealthOrigX = -1;
	m_nBonusHealthOrigY = -1;
	m_nBonusHealthOrigW = -1;
	m_nBonusHealthOrigH = -1;

	m_iAnimState = HUD_HEALTH_NO_ANIM;
	m_bAnimate = true;
}

CTFHudPlayerHealth::~CTFHudPlayerHealth()
{
	m_vecBuffInfo.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerHealth::Reset()
{
	m_flNextThink = gpGlobals->curtime + 0.05f;
	m_nHealth = -1;
	m_bBuilding = false;

	m_iAnimState = HUD_HEALTH_NO_ANIM;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerHealth::ApplySchemeSettings( IScheme *pScheme )
{
	// load control settings...
	LoadControlSettings( GetResFilename() );

	if ( m_pHealthBonusImage )
	{
		m_pHealthBonusImage->GetBounds( m_nBonusHealthOrigX, m_nBonusHealthOrigY, m_nBonusHealthOrigW, m_nBonusHealthOrigH );
	}

	m_flNextThink = 0.0f;

	BaseClass::ApplySchemeSettings( pScheme );

	m_pBuildingHealthImageBG->SetVisible( m_bBuilding );

#ifdef TEMP_CALLS
	m_pPlayerLevelLabel = dynamic_cast<CExLabel*>( FindChildByName( "PlayerStatusPlayerLevel" ) );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerHealth::SetHealth( int iNewHealth, int iMaxHealth, int	iMaxBuffedHealth )
{
	// set our health
	m_nHealth = iNewHealth;
	m_nMaxHealth = iMaxHealth;
	m_pHealthImage->SetHealth( (float)(m_nHealth) / (float)(m_nMaxHealth) );

	if ( m_pHealthImage )
	{
		m_pHealthImage->SetFgColor( Color( 255, 255, 255, 255 ) );
	}

	if ( m_nHealth <= 0 )
	{
		if ( m_pHealthImageBG->IsVisible() )
		{
			m_pHealthImageBG->SetVisible( false );
		}
		if ( m_pBuildingHealthImageBG->IsVisible() )
		{
			m_pBuildingHealthImageBG->SetVisible( false );
		}
		HideHealthBonusImage();
	}
	else
	{
		if ( !m_pHealthImageBG->IsVisible() )
		{
			m_pHealthImageBG->SetVisible( true );
		}
		m_pBuildingHealthImageBG->SetVisible( m_bBuilding );

		// are we getting a health bonus?
		if ( m_nHealth > m_nMaxHealth )
		{
			if ( m_pHealthBonusImage && m_nBonusHealthOrigW != -1 )
			{
				if ( !m_pHealthBonusImage->IsVisible() )
				{
					m_pHealthBonusImage->SetVisible( true );
				}

				if ( m_bAnimate && m_iAnimState != HUD_HEALTH_BONUS_ANIM )
				{
					g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( this, "HudHealthDyingPulseStop" );
					g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( this, "HudHealthBonusPulse" );

					m_iAnimState = HUD_HEALTH_BONUS_ANIM;
				}

				m_pHealthBonusImage->SetDrawColor( Color( 255, 255, 255, 255 ) );

				// scale the flashing image based on how much health bonus we currently have
				float flBoostMaxAmount = ( iMaxBuffedHealth ) - m_nMaxHealth;
				float flPercent = MIN( ( m_nHealth - m_nMaxHealth ) / flBoostMaxAmount, 1.0f );

				int nPosAdj = RoundFloatToInt( flPercent * m_nHealthBonusPosAdj );
				int nSizeAdj = 2 * nPosAdj;

				m_pHealthBonusImage->SetBounds( m_nBonusHealthOrigX - nPosAdj, 
					m_nBonusHealthOrigY - nPosAdj, 
					m_nBonusHealthOrigW + nSizeAdj,
					m_nBonusHealthOrigH + nSizeAdj );
			}
		}
		// are we close to dying?
		else if ( m_nHealth < m_nMaxHealth * m_flHealthDeathWarning )
		{
			if ( m_pHealthBonusImage && m_nBonusHealthOrigW != -1 )
			{
				if ( !m_pHealthBonusImage->IsVisible() )
				{
					m_pHealthBonusImage->SetVisible( true );
				}

				if ( m_bAnimate && m_iAnimState != HUD_HEALTH_DYING_ANIM )
				{
					g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( this, "HudHealthBonusPulseStop" );
					g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( this, "HudHealthDyingPulse" );

					m_iAnimState = HUD_HEALTH_DYING_ANIM;
				}

				m_pHealthBonusImage->SetDrawColor( m_clrHealthDeathWarningColor );

				// scale the flashing image based on how much health bonus we currently have
				float flBoostMaxAmount = m_nMaxHealth * m_flHealthDeathWarning;
				float flPercent = ( flBoostMaxAmount - m_nHealth ) / flBoostMaxAmount;

				int nPosAdj = RoundFloatToInt( flPercent * m_nHealthBonusPosAdj );
				int nSizeAdj = 2 * nPosAdj;

				m_pHealthBonusImage->SetBounds( m_nBonusHealthOrigX - nPosAdj, 
					m_nBonusHealthOrigY - nPosAdj, 
					m_nBonusHealthOrigW + nSizeAdj,
					m_nBonusHealthOrigH + nSizeAdj );
			}

			if ( m_pHealthImage )
			{
				m_pHealthImage->SetFgColor( m_clrHealthDeathWarningColor );
			}
		}
		// turn it off
		else
		{
			HideHealthBonusImage();
		}
	}

	// set our health display value
	if ( m_nHealth > 0 )
	{
		// same calculation as live tf2
		if ( m_nHealth <= m_nMaxHealth - 5 )
			SetDialogVariable( "MaxHealth", m_nMaxHealth );
		else
			SetDialogVariable( "MaxHealth", "" );

		SetDialogVariable( "Health", m_nHealth );
	}
	else
	{
		SetDialogVariable( "Health", "" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerHealth::HideHealthBonusImage( void )
{
	if ( m_pHealthBonusImage && m_pHealthBonusImage->IsVisible() )
	{
		if ( m_nBonusHealthOrigW != -1 )
		{
			m_pHealthBonusImage->SetBounds( m_nBonusHealthOrigX, m_nBonusHealthOrigY, m_nBonusHealthOrigW, m_nBonusHealthOrigH );
		}
		m_pHealthBonusImage->SetVisible( false );
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( this, "HudHealthBonusPulseStop" );
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( this, "HudHealthDyingPulseStop" );

		m_iAnimState = HUD_HEALTH_NO_ANIM;
	}
}

void CTFBuffInfo::Update( CTFPlayer *pPlayer )
{
	Assert( m_pImagePanel != NULL && pPlayer != NULL );

	if ( pPlayer->m_Shared.InCond( m_eCond ) )
	{
		if( m_pzsBlueImage && m_pzsBlueImage[0] && m_pzsRedImage && m_pzsRedImage[0] )
		{
			if( pPlayer->GetTeamNumber() == TF_TEAM_BLUE )
				m_pImagePanel->SetImage( m_pzsBlueImage );
			else
				m_pImagePanel->SetImage( m_pzsRedImage );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerHealth::OnThink()
{
	if ( m_flNextThink < gpGlobals->curtime )
	{
		C_TFPlayer *pPlayer = ToTFPlayer( C_BasePlayer::GetLocalPlayer() );

		if ( pPlayer )
		{
			SetHealth( pPlayer->GetHealth(), pPlayer->GetPlayerClass()->GetMaxHealth(), pPlayer->m_Shared.GetMaxBuffedHealth() );
		}

#ifdef TEMP_CALLS
		// Turn all the panels off, this sucks ass
		if ( m_pBleedImage )
			m_pBleedImage->SetVisible( false );
		if ( m_pHookBleedImage )
			m_pHookBleedImage->SetVisible( false );
		if ( m_pMilkImage )
			m_pMilkImage->SetVisible( false );
		if ( m_pMarkedForDeathImage )
			m_pMarkedForDeathImage->SetVisible( false );
		if ( m_pMarkedForDeathImageSilent )
			m_pMarkedForDeathImageSilent->SetVisible( false );
		if ( m_pPlayerLevelLabel )
			m_pPlayerLevelLabel->SetVisible( false );
		if ( m_pGasImage )
			m_pGasImage->SetVisible( false );
		if ( m_pWheelOfDoomImage )
			m_pWheelOfDoomImage->SetVisible( false );
#endif

		// Turn all the panels off, and below conditionally turn them on
		FOR_EACH_VEC( m_vecBuffInfo, i )
		{
			m_vecBuffInfo[ i ]->m_pImagePanel->SetVisible( false );
		}

		m_flNextThink = gpGlobals->curtime + 0.05f;
	}
}

DECLARE_HUDELEMENT( CTFHudPlayerStatus );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHudPlayerStatus::CTFHudPlayerStatus( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudPlayerStatus" ) 
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_pHudPlayerClass = new CTFHudPlayerClass( this, "HudPlayerClass" );
	m_pHudPlayerHealth = new CTFHudPlayerHealth( this, "HudPlayerHealth" );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerStatus::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// HACK: Work around the scheme application order failing
	// to reload the player class hud element's scheme in minmode.
	ConVarRef cl_hud_minmode( "cl_hud_minmode", true );
	if ( cl_hud_minmode.IsValid() && cl_hud_minmode.GetBool() )
	{
		m_pHudPlayerClass->InvalidateLayout( false, true );
	}
}

bool CTFHudPlayerStatus::ShouldDraw( void )
{
	CTFPlayer *pTFPlayer = CTFPlayer::GetLocalTFPlayer();
	if ( pTFPlayer && ( !pTFPlayer->IsTF2Class() || pTFPlayer->m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) ) )
		return false;

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudPlayerStatus::Reset()
{
	if ( m_pHudPlayerClass )
	{
		m_pHudPlayerClass->Reset();
	}

	if ( m_pHudPlayerHealth )
	{
		m_pHudPlayerHealth->Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFClassImage::SetClass( int iTeam, int iClass, int iCloakstate )
{
	char szImage[128];
	szImage[0] = '\0';

	// Force image off
	C_TFPlayer *pTFPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( pTFPlayer && ( !pTFPlayer->IsTF2Class() || pTFPlayer->IsSpecialClass() ) )
		return SetImage( szImage );

	if ( iTeam == TF_TEAM_BLUE )
	{
		Q_strncpy( szImage, g_szBlueClassImages[ iClass ], sizeof(szImage) );
	}
	else
	{
		Q_strncpy( szImage, g_szRedClassImages[ iClass ], sizeof(szImage) );
	}

	switch( iCloakstate )
	{
	case 2:
		Q_strncat( szImage, "_cloak", sizeof(szImage), COPY_ALL_CHARACTERS );
		break;
	case 1:
		Q_strncat( szImage, "_halfcloak", sizeof(szImage), COPY_ALL_CHARACTERS );
		break;
	default:
		break;
	}

	if ( Q_strlen( szImage ) > 0 )
	{
		SetImage( szImage );
	}
}
