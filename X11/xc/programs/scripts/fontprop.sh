#!/bin/sh

#
# This script is used to generate the various XLFD font properties given an
# XLFD-style font name:
#
#    -FOUNDRY-FAMILY_NAME-WEIGHT_NAME-SLANT-SETWIDTH_NAME-ADD_STYLE_NAME- ...
#        ... PIXEL_SIZE-POINT_SIZE-RESOLUTION_X-RESOLUTION_Y-SPACING- ...
#        ... AVERAGE_WIDTH-CHARSET_REGISTRY-CHARSET_ENCODING
#

awk -F- '
{
    printf "FONTNAME_REGISTRY \"%s\"\n", $1;
    printf "FOUNDRY \"%s\"\n", $2;
    printf "FAMILY_NAME \"%s\"\n", $3;
    printf "WEIGHT_NAME \"%s\"\n", $4;
    printf "SLANT \"%s\"\n", $5;
    printf "SETWIDTH_NAME \"%s\"\n", $6;
    printf "ADD_STYLE_NAME \"%s\"\n", $7;
    printf "PIXEL_SIZE %d\n", $8;
    printf "POINT_SIZE %d\n", $9;
    printf "RESOLUTION_X %d\n", $10;
    printf "RESOLUTION_Y %d\n", $11;
    printf "SPACING \"%s\"\n", $12;
    printf "AVERAGE_WIDTH %d\n", $13;
    printf "CHARSET_REGISTRY \"%s\"\n", $14;
    printf "CHARSET_ENCODING \"%s\"\n", $15;
}' $*
