#!/bin/sh

#
# This script is used to generate the FONT property given correct information
# in the various other fields.
#

awk 'BEGIN {
	fontname_registry = ""; 
	foundry = "";
	family_name = "";
	weight_name = "";
	slant = "";
	setwidth_name = "";
	add_style_name = "";
	pixel_size = "";
	point_size = "";
	resolution_x = "";
	resolution_y = "";
	spacing = "";
	average_width = "";
	charset_registry = "";
	charset_encoding = "";
}

/^FONTNAME_REGISTRY/	{ fontname_registry = $2; }
/^FOUNDRY/		{ foundry = $2; }
/^FAMILY_NAME/		{ family_name = $2; }
/^WEIGHT_NAME/		{ weight_name = $2; }
/^SLANT/		{ slant = $2; }
/^SETWIDTH_NAME/	{ setwidth_name = $2; }
/^ADD_STYLE_NAME/	{ add_style_name = $2; }
/^PIXEL_SIZE/		{ pixel_size = $2; }
/^POINT_SIZE/		{ point_size = $2; }
/^RESOLUTION_X/		{ resolution_x = $2; }
/^RESOLUTION_Y/		{ resolution_y = $2; }
/^SPACING/		{ spacing = $2; }
/^AVERAGE_WIDTH/	{ average_width = $2; }
/^CHARSET_REGISTRY/	{ charset_registry = $2; }
/^CHARSET_ENCODING/	{ charset_encoding = $2; }
/^ENDPROPERTIES/	{ exit; }

END {
	printf "%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s\n", \
		fontname_registry, foundry, family_name, weight_name, \
		slant, setwidth_name, add_style_name, \
		pixel_size, point_size, resolution_x, resolution_y, \
		spacing, average_width, charset_registry, charset_encoding;
}' $* | sed 's/"//g'
