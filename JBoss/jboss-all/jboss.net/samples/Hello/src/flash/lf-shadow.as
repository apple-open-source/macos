function shadowControls( textColor, textBackground )
{
	COLOR_HIGHLIGHT = 0xC0CCD8;
	COLOR_FACE = 0x8098B0;
	COLOR_BORDER = 0x4E657C;
	COLOR_DISABLED = 0xff0000;

	globalStyleFormat.face = textBackground;
	globalStyleFormat.highlight = COLOR_HIGHLIGHT;
	globalStyleFormat.highlight3D = COLOR_HIGHLIGHT;
	globalStyleFormat.darkshadow = 0x000000;
	globalStyleFormat.shadow = COLOR_BORDER;
	globalStyleFormat.arrow = 0x333333;
	globalStyleFormat.foregroundDisabled = COLOR_HIGHLIGHT;
	globalStyleFormat.scrollTrack = COLOR_HIGHLIGHT;
	globalStyleFormat.focusRectInner = 0xDBDCE5;
	globalStyleFormat.focusRectOuter = 0x9495A2;
	globalStyleFormat.background = COLOR_HIGHLIGHT;
	globalStyleFormat.selectionUnfocused = COLOR_DISABLED;
	globalStyleFormat.border = COLOR_BORDER;
	globalStyleFormat.textColor = textColor;
	globalStyleFormat.textDisabled = COLOR_DISABLED;
	globalStyleFormat.textSize = 12;
	globalStyleFormat.emptyDateBackground = COLOR_BORDER;
	globalStyleFormat.dateBackground = COLOR_HIGHLIGHT;
	globalStyleFormat.dateBorder= COLOR_BORDER;
	globalStyleFormat.applyChanges();
}