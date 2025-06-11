"Resource/UI/HudHealth.res"
{
	"HealthClassImage"	// draws the class image and the red "damage taken" part
	{
		"ControlName"	"ImagePanel"
		"fieldName"		"HealthClassImage"
		"xpos"			"0"
		"ypos"			"5"
		"wide"			"36"
		"tall"			"72"
		"visible"		"1"
		"enabled"		"1"
		"scaleImage"	"1"	
	}
	"HealthClassImageBG"	// draws the class image outline
	{
		"ControlName"	"ImagePanel"
		"fieldName"		"HealthClassImageBG"
		"xpos"			"0"
		"ypos"			"5"
		"wide"			"36"
		"tall"			"72"
		"visible"		"1"
		"enabled"		"1"
		"scaleImage"	"1"	
	}
	"HealthBar"
	{
		"ControlName"	"ImagePanel"
		"fieldName"		"HealthBar"
		"xpos"			"36"
		"ypos"			"20"
		"wide"			"8"
		"tall"			"54"
		"visible"		"1"
		"enabled"		"1"
		"FirstWarning"	"0.50"
		"SecondWarning"	"0.25"
		"fillcolor"		"White"
	}
}
