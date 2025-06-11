"Resource/HudLayout.res"
{
	CSHudWeaponSelection
	{
		"fieldName" "CSHudWeaponSelection"
		"xpos"	"r640"
		"wide"	"640"
		"ypos" 	"16"
		"visible" "1"
		"enabled" "1"
		"SmallBoxSize" "60"
		"LargeBoxWide" "108"
		"LargeBoxTall" "80"
		"BoxGap" "8"
		"SelectionNumberXPos" "4"
		"SelectionNumberYPos" "4"
		"SelectionGrowTime"	"0.4"
		"IconXPos" "8"
		"IconYPos" "0"
		"TextYPos" "68"	
		"TextColor" "CS_SelectionTextFg"
		"MaxSlots"	"5"
		"PlaySelectSounds"	"0"
	}
	
	CSHudHealth
	{
		"fieldName"		"CSHudHealth"
		"xpos"	"8"
		"ypos"	"446"
		"zpos"	"0"
 		"wide"	"80"
		"tall"  "25"
		"visible" "1"
		"enabled" "1"

		"PaintBackgroundType"	"2"
		"bgcolor_override"		"0 0 0 96"
		
		"icon_xpos"	"8"
		"icon_ypos"	"-4"
		"digit_xpos" "35"
		"digit_ypos" "-4"
		"LowHealthColor"	"CS_HudIcon_Red"
	}
	
	CSHudArmor
	{
		"fieldName"		"CSHudArmor"
		"xpos"	"148"
		"ypos"	"446"
		"zpos"	"1"
		"wide"	"80"
		"tall"  "25"
		"visible" "1"
		"enabled" "1"

		"PaintBackgroundType"	"2"
 		"bgcolor_override"		"0 0 0 96"
		
		"icon_xpos"	"8"
		"icon_ypos"	"-4"
		"digit_xpos" "34"
		"digit_ypos" "-4"
	}
	
	CSHudAmmo
	{
		"fieldName" "CSHudAmmo"
		"xpos"	"r157"
		"ypos"	"446"
		"zpos"	"1"
		"wide"	"142"
		"tall"  "25"
		"visible" "1"
		"enabled" "1"

		"PaintBackgroundType"	"2"
 		"bgcolor_override"		"0 0 0 96"

		"digit_xpos" "8"
		"digit_ypos" "-4"
		"digit2_xpos" "63"
		"digit2_ypos" "-4"
	
		"bar_xpos"		"53"
		"bar_ypos"		"3"
		"bar_height"	"20"
		"bar_width"		"2"

		"icon_xpos"		"110"
		"icon_ypos"		"2"
	}
}
