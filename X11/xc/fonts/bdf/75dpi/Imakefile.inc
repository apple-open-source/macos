XCOMM $XFree86: xc/fonts/bdf/75dpi/Imakefile.inc,v 1.6 2001/07/04 16:44:29 tsi Exp $


#if BuildUCSFonts
UCSOBJS1 = \
	FontObj(courB08) FontObj(courB10) FontObj(courB12) FontObj(courB14) \
	FontObj(courB18) FontObj(courB24) \
	FontObj(courBO08) FontObj(courBO10) FontObj(courBO12) \
	FontObj(courBO14) FontObj(courBO18) FontObj(courBO24) \
	FontObj(courO08) FontObj(courO10) FontObj(courO12) FontObj(courO14) \
	FontObj(courO18) FontObj(courO24) \
	FontObj(courR08) FontObj(courR10) FontObj(courR12) FontObj(courR14) \
	FontObj(courR18) FontObj(courR24) \
	FontObj(helvB08) FontObj(helvB10) FontObj(helvB12) FontObj(helvB14) \
	FontObj(helvB18) FontObj(helvB24) \
	FontObj(helvBO08) FontObj(helvBO10) FontObj(helvBO12) \
	FontObj(helvBO14) FontObj(helvBO18) FontObj(helvBO24) \
	FontObj(helvO08) FontObj(helvO10) FontObj(helvO12) FontObj(helvO14) \
	FontObj(helvO18) FontObj(helvO24) \
	FontObj(helvR08) FontObj(helvR10) FontObj(helvR12) FontObj(helvR14) \
	FontObj(helvR18) FontObj(helvR24) \
	FontObj(ncenB08) FontObj(ncenB10) FontObj(ncenB12) FontObj(ncenB14) \
	FontObj(ncenB18) FontObj(ncenB24) \
	FontObj(ncenBI08) FontObj(ncenBI10) FontObj(ncenBI12) \
	FontObj(ncenBI14) FontObj(ncenBI18) FontObj(ncenBI24) \
	FontObj(ncenI08) FontObj(ncenI10) FontObj(ncenI12) FontObj(ncenI14) \
	FontObj(ncenI18) FontObj(ncenI24) \
	FontObj(ncenR08) FontObj(ncenR10) FontObj(ncenR12) FontObj(ncenR14) \
	FontObj(ncenR18) FontObj(ncenR24) \
	FontObj(timB08) FontObj(timB10) FontObj(timB12) FontObj(timB14) \
	FontObj(timB18) FontObj(timB24) \
	FontObj(timBI08) FontObj(timBI10) FontObj(timBI12) FontObj(timBI14) \
	FontObj(timBI18) FontObj(timBI24) \
	FontObj(timI08) FontObj(timI10) FontObj(timI12) FontObj(timI14) \
	FontObj(timI18) FontObj(timI24) \
	FontObj(timR08) FontObj(timR10) FontObj(timR12) FontObj(timR14) \
	FontObj(timR18) FontObj(timR24)

UCSOBJS2 = \
	FontObj(luBIS08) FontObj(luBIS10) FontObj(luBIS12) FontObj(luBIS14) \
	FontObj(luBIS18) FontObj(luBIS19) FontObj(luBIS24) \
	FontObj(luBS08) FontObj(luBS10) FontObj(luBS12) FontObj(luBS14) \
	FontObj(luBS18) FontObj(luBS19) FontObj(luBS24) \
	FontObj(luIS08) FontObj(luIS10) FontObj(luIS12) FontObj(luIS14) \
	FontObj(luIS18) FontObj(luIS19) FontObj(luIS24) \
	FontObj(luRS08) FontObj(luRS10) FontObj(luRS12) FontObj(luRS14) \
	FontObj(luRS18) FontObj(luRS19) FontObj(luRS24) \
	FontObj(lubB08) FontObj(lubB10) FontObj(lubB12) FontObj(lubB14) \
	FontObj(lubB18) FontObj(lubB19) FontObj(lubB24) \
	FontObj(lubBI08) FontObj(lubBI10) FontObj(lubBI12) FontObj(lubBI14) \
	FontObj(lubBI18) FontObj(lubBI19) FontObj(lubBI24) \
	FontObj(lubI08) FontObj(lubI10) FontObj(lubI12) FontObj(lubI14) \
	FontObj(lubI18) FontObj(lubI19) FontObj(lubI24) \
	FontObj(lubR08) FontObj(lubR10) FontObj(lubR12) FontObj(lubR14) \
	FontObj(lubR18) FontObj(lubR19) FontObj(lubR24) \
	FontObj(lutBS08) FontObj(lutBS10) FontObj(lutBS12) FontObj(lutBS14) \
	FontObj(lutBS18) FontObj(lutBS19) FontObj(lutBS24) \
	FontObj(lutRS08) FontObj(lutRS10) FontObj(lutRS12) FontObj(lutRS14) \
	FontObj(lutRS18) FontObj(lutRS19) FontObj(lutRS24) \
	FontObj(UTBI__10) FontObj(UTBI__12) FontObj(UTBI__14) \
	FontObj(UTB___18) FontObj(UTB___24) \
	FontObj(UTB___10) FontObj(UTB___12) FontObj(UTB___14) \
	FontObj(UTBI__18) FontObj(UTBI__24) \
	FontObj(UTI___10) FontObj(UTI___12) FontObj(UTI___14) \
	FontObj(UTI___18) FontObj(UTI___24) \
	FontObj(UTRG__10) FontObj(UTRG__12) FontObj(UTRG__14) \
	FontObj(UTRG__18) FontObj(UTRG__24)

UCSOBJS = $(UCSOBJS1) $(UCSOBJS2)
#endif

MISCOBJS = \
	FontObj(symb08) FontObj(symb10) FontObj(symb12) FontObj(symb14) \
	FontObj(symb18) FontObj(symb24) \
	FontObj(charB08) FontObj(charB10) FontObj(charB12) FontObj(charB14) \
	FontObj(charB18) FontObj(charB24) \
	FontObj(charBI08) FontObj(charBI10) FontObj(charBI12) \
	FontObj(charBI14) FontObj(charBI18) FontObj(charBI24) \
	FontObj(charI08) FontObj(charI10) FontObj(charI12) FontObj(charI14) \
	FontObj(charI18) FontObj(charI24) \
	FontObj(charR08) FontObj(charR10) FontObj(charR12) FontObj(charR14) \
	FontObj(charR18) FontObj(charR24) \
	FontObj(tech14) FontObj(techB14) \
	FontObj(term14) FontObj(termB14)

#if BuildISO8859_1Fonts
/*
 * Latin1 versions are always available, either derived at build time or
 * pre-derived for systems without perl.
 */
DERIVED_8859_1_OBJS1 = \
	FontObj(courB08-ISO8859-1) FontObj(courB10-ISO8859-1) \
	FontObj(courB12-ISO8859-1) FontObj(courB14-ISO8859-1) \
	FontObj(courB18-ISO8859-1) FontObj(courB24-ISO8859-1) \
	FontObj(courBO08-ISO8859-1) FontObj(courBO10-ISO8859-1) \
	FontObj(courBO12-ISO8859-1) FontObj(courBO14-ISO8859-1) \
	FontObj(courBO18-ISO8859-1) FontObj(courBO24-ISO8859-1) \
	FontObj(courO08-ISO8859-1) FontObj(courO10-ISO8859-1) \
	FontObj(courO12-ISO8859-1) FontObj(courO14-ISO8859-1) \
	FontObj(courO18-ISO8859-1) FontObj(courO24-ISO8859-1) \
	FontObj(courR08-ISO8859-1) FontObj(courR10-ISO8859-1) \
	FontObj(courR12-ISO8859-1) FontObj(courR14-ISO8859-1) \
	FontObj(courR18-ISO8859-1) FontObj(courR24-ISO8859-1) \
	FontObj(helvB08-ISO8859-1) FontObj(helvB10-ISO8859-1) \
	FontObj(helvB12-ISO8859-1) FontObj(helvB14-ISO8859-1) \
	FontObj(helvB18-ISO8859-1) FontObj(helvB24-ISO8859-1) \
	FontObj(helvBO08-ISO8859-1) FontObj(helvBO10-ISO8859-1) \
	FontObj(helvBO12-ISO8859-1) FontObj(helvBO14-ISO8859-1) \
	FontObj(helvBO18-ISO8859-1) FontObj(helvBO24-ISO8859-1) \
	FontObj(helvO08-ISO8859-1) FontObj(helvO10-ISO8859-1) \
	FontObj(helvO12-ISO8859-1) FontObj(helvO14-ISO8859-1) \
	FontObj(helvO18-ISO8859-1) FontObj(helvO24-ISO8859-1) \
	FontObj(helvR08-ISO8859-1) FontObj(helvR10-ISO8859-1) \
	FontObj(helvR12-ISO8859-1) FontObj(helvR14-ISO8859-1) \
	FontObj(helvR18-ISO8859-1) FontObj(helvR24-ISO8859-1) \
	FontObj(ncenB08-ISO8859-1) FontObj(ncenB10-ISO8859-1) \
	FontObj(ncenB12-ISO8859-1) FontObj(ncenB14-ISO8859-1) \
	FontObj(ncenB18-ISO8859-1) FontObj(ncenB24-ISO8859-1) \
	FontObj(ncenBI08-ISO8859-1) FontObj(ncenBI10-ISO8859-1) \
	FontObj(ncenBI12-ISO8859-1) FontObj(ncenBI14-ISO8859-1) \
	FontObj(ncenBI18-ISO8859-1) FontObj(ncenBI24-ISO8859-1) \
	FontObj(ncenI08-ISO8859-1) FontObj(ncenI10-ISO8859-1) \
	FontObj(ncenI12-ISO8859-1) FontObj(ncenI14-ISO8859-1) \
	FontObj(ncenI18-ISO8859-1) FontObj(ncenI24-ISO8859-1) \
	FontObj(ncenR08-ISO8859-1) FontObj(ncenR10-ISO8859-1) \
	FontObj(ncenR12-ISO8859-1) FontObj(ncenR14-ISO8859-1) \
	FontObj(ncenR18-ISO8859-1) FontObj(ncenR24-ISO8859-1) \
	FontObj(timB08-ISO8859-1) FontObj(timB10-ISO8859-1) \
	FontObj(timB12-ISO8859-1) FontObj(timB14-ISO8859-1) \
	FontObj(timB18-ISO8859-1) FontObj(timB24-ISO8859-1) \
	FontObj(timBI08-ISO8859-1) FontObj(timBI10-ISO8859-1) \
	FontObj(timBI12-ISO8859-1) FontObj(timBI14-ISO8859-1) \
	FontObj(timBI18-ISO8859-1) FontObj(timBI24-ISO8859-1) \
	FontObj(timI08-ISO8859-1) FontObj(timI10-ISO8859-1) \
	FontObj(timI12-ISO8859-1) FontObj(timI14-ISO8859-1) \
	FontObj(timI18-ISO8859-1) FontObj(timI24-ISO8859-1) \
	FontObj(timR08-ISO8859-1) FontObj(timR10-ISO8859-1) \
	FontObj(timR12-ISO8859-1) FontObj(timR14-ISO8859-1) \
	FontObj(timR18-ISO8859-1) FontObj(timR24-ISO8859-1)

DERIVED_8859_1_OBJS2 = \
	FontObj(luBIS08-ISO8859-1) FontObj(luBIS10-ISO8859-1) \
	FontObj(luBIS12-ISO8859-1) FontObj(luBIS14-ISO8859-1) \
	FontObj(luBIS18-ISO8859-1) FontObj(luBIS19-ISO8859-1) \
	FontObj(luBIS24-ISO8859-1) \
	FontObj(luBS08-ISO8859-1) FontObj(luBS10-ISO8859-1) \
	FontObj(luBS12-ISO8859-1) FontObj(luBS14-ISO8859-1) \
	FontObj(luBS18-ISO8859-1) FontObj(luBS19-ISO8859-1) \
	FontObj(luBS24-ISO8859-1) \
	FontObj(luIS08-ISO8859-1) FontObj(luIS10-ISO8859-1) \
	FontObj(luIS12-ISO8859-1) FontObj(luIS14-ISO8859-1) \
	FontObj(luIS18-ISO8859-1) FontObj(luIS19-ISO8859-1) \
	FontObj(luIS24-ISO8859-1) \
	FontObj(luRS08-ISO8859-1) FontObj(luRS10-ISO8859-1) \
	FontObj(luRS12-ISO8859-1) FontObj(luRS14-ISO8859-1) \
	FontObj(luRS18-ISO8859-1) FontObj(luRS19-ISO8859-1) \
	FontObj(luRS24-ISO8859-1) \
	FontObj(lubB08-ISO8859-1) FontObj(lubB10-ISO8859-1) \
	FontObj(lubB12-ISO8859-1) FontObj(lubB14-ISO8859-1) \
	FontObj(lubB18-ISO8859-1) FontObj(lubR19-ISO8859-1) \
	FontObj(lubB24-ISO8859-1) \
	FontObj(lubBI08-ISO8859-1) FontObj(lubBI10-ISO8859-1) \
	FontObj(lubBI12-ISO8859-1) FontObj(lubBI14-ISO8859-1) \
	FontObj(lubBI18-ISO8859-1) FontObj(lubBI19-ISO8859-1) \
	FontObj(lubBI24-ISO8859-1) \
	FontObj(lubI08-ISO8859-1) FontObj(lubI10-ISO8859-1) \
	FontObj(lubI12-ISO8859-1) FontObj(lubI14-ISO8859-1) \
	FontObj(lubI18-ISO8859-1) FontObj(lubI19-ISO8859-1) \
	FontObj(lubI24-ISO8859-1) \
	FontObj(lubR08-ISO8859-1) FontObj(lubR10-ISO8859-1) \
	FontObj(lubR12-ISO8859-1) FontObj(lubR14-ISO8859-1) \
	FontObj(lubR18-ISO8859-1) FontObj(lubR19-ISO8859-1) \
	FontObj(lubR24-ISO8859-1) \
	FontObj(lutBS08-ISO8859-1) FontObj(lutBS10-ISO8859-1) \
	FontObj(lutBS12-ISO8859-1) FontObj(lutBS14-ISO8859-1) \
	FontObj(lutBS18-ISO8859-1) FontObj(lutBS19-ISO8859-1) \
	FontObj(lutBS24-ISO8859-1) \
	FontObj(lutRS08-ISO8859-1) FontObj(lutRS10-ISO8859-1) \
	FontObj(lutRS12-ISO8859-1) FontObj(lutRS14-ISO8859-1) \
	FontObj(lutRS18-ISO8859-1) FontObj(lutRS19-ISO8859-1) \
	FontObj(lutRS24-ISO8859-1) \
	FontObj(UTBI__10-ISO8859-1) FontObj(UTBI__12-ISO8859-1) \
	FontObj(UTBI__14-ISO8859-1) FontObj(UTBI__18-ISO8859-1) \
	FontObj(UTBI__24-ISO8859-1) \
	FontObj(UTB___10-ISO8859-1) FontObj(UTB___12-ISO8859-1) \
	FontObj(UTB___14-ISO8859-1) FontObj(UTB___18-ISO8859-1) \
	FontObj(UTB___24-ISO8859-1) \
	FontObj(UTI___10-ISO8859-1) FontObj(UTI___12-ISO8859-1) \
	FontObj(UTI___14-ISO8859-1) FontObj(UTI___18-ISO8859-1) \
	FontObj(UTI___24-ISO8859-1) \
	FontObj(UTRG__10-ISO8859-1) FontObj(UTRG__12-ISO8859-1) \
	FontObj(UTRG__14-ISO8859-1) FontObj(UTRG__18-ISO8859-1) \
	FontObj(UTRG__24-ISO8859-1)

DERIVED_8859_1_OBJS = $(DERIVED_8859_1_OBJS1) (DERIVED_8859_1_OBJS2)
#endif

#if DerivedUCSFonts

#if BuildISO8859_2Fonts
DERIVED_8859_2_OBJS1 = \
	FontObj(courB08-ISO8859-2) FontObj(courB10-ISO8859-2) \
	FontObj(courB12-ISO8859-2) FontObj(courB14-ISO8859-2) \
	FontObj(courB18-ISO8859-2) FontObj(courB24-ISO8859-2) \
	FontObj(courBO08-ISO8859-2) FontObj(courBO10-ISO8859-2) \
	FontObj(courBO12-ISO8859-2) FontObj(courBO14-ISO8859-2) \
	FontObj(courBO18-ISO8859-2) FontObj(courBO24-ISO8859-2) \
	FontObj(courO08-ISO8859-2) FontObj(courO10-ISO8859-2) \
	FontObj(courO12-ISO8859-2) FontObj(courO14-ISO8859-2) \
	FontObj(courO18-ISO8859-2) FontObj(courO24-ISO8859-2) \
	FontObj(courR08-ISO8859-2) FontObj(courR10-ISO8859-2) \
	FontObj(courR12-ISO8859-2) FontObj(courR14-ISO8859-2) \
	FontObj(courR18-ISO8859-2) FontObj(courR24-ISO8859-2) \
	FontObj(helvB08-ISO8859-2) FontObj(helvB10-ISO8859-2) \
	FontObj(helvB12-ISO8859-2) FontObj(helvB14-ISO8859-2) \
	FontObj(helvB18-ISO8859-2) FontObj(helvB24-ISO8859-2) \
	FontObj(helvBO08-ISO8859-2) FontObj(helvBO10-ISO8859-2) \
	FontObj(helvBO12-ISO8859-2) FontObj(helvBO14-ISO8859-2) \
	FontObj(helvBO18-ISO8859-2) FontObj(helvBO24-ISO8859-2) \
	FontObj(helvO08-ISO8859-2) FontObj(helvO10-ISO8859-2) \
	FontObj(helvO12-ISO8859-2) FontObj(helvO14-ISO8859-2) \
	FontObj(helvO18-ISO8859-2) FontObj(helvO24-ISO8859-2) \
	FontObj(helvR08-ISO8859-2) FontObj(helvR10-ISO8859-2) \
	FontObj(helvR12-ISO8859-2) FontObj(helvR14-ISO8859-2) \
	FontObj(helvR18-ISO8859-2) FontObj(helvR24-ISO8859-2) \
	FontObj(ncenB08-ISO8859-2) FontObj(ncenB10-ISO8859-2) \
	FontObj(ncenB12-ISO8859-2) FontObj(ncenB14-ISO8859-2) \
	FontObj(ncenB18-ISO8859-2) FontObj(ncenB24-ISO8859-2) \
	FontObj(ncenBI08-ISO8859-2) FontObj(ncenBI10-ISO8859-2) \
	FontObj(ncenBI12-ISO8859-2) FontObj(ncenBI14-ISO8859-2) \
	FontObj(ncenBI18-ISO8859-2) FontObj(ncenBI24-ISO8859-2) \
	FontObj(ncenI08-ISO8859-2) FontObj(ncenI10-ISO8859-2) \
	FontObj(ncenI12-ISO8859-2) FontObj(ncenI14-ISO8859-2) \
	FontObj(ncenI18-ISO8859-2) FontObj(ncenI24-ISO8859-2) \
	FontObj(ncenR08-ISO8859-2) FontObj(ncenR10-ISO8859-2) \
	FontObj(ncenR12-ISO8859-2) FontObj(ncenR14-ISO8859-2) \
	FontObj(ncenR18-ISO8859-2) FontObj(ncenR24-ISO8859-2) \
	FontObj(timB08-ISO8859-2) FontObj(timB10-ISO8859-2) \
	FontObj(timB12-ISO8859-2) FontObj(timB14-ISO8859-2) \
	FontObj(timB18-ISO8859-2) FontObj(timB24-ISO8859-2) \
	FontObj(timBI08-ISO8859-2) FontObj(timBI10-ISO8859-2) \
	FontObj(timBI12-ISO8859-2) FontObj(timBI14-ISO8859-2) \
	FontObj(timBI18-ISO8859-2) FontObj(timBI24-ISO8859-2) \
	FontObj(timI08-ISO8859-2) FontObj(timI10-ISO8859-2) \
	FontObj(timI12-ISO8859-2) FontObj(timI14-ISO8859-2) \
	FontObj(timI18-ISO8859-2) FontObj(timI24-ISO8859-2) \
	FontObj(timR08-ISO8859-2) FontObj(timR10-ISO8859-2) \
	FontObj(timR12-ISO8859-2) FontObj(timR14-ISO8859-2) \
	FontObj(timR18-ISO8859-2) FontObj(timR24-ISO8859-2)

DERIVED_8859_2_OBJS2 = \
	FontObj(luBIS08-ISO8859-2) FontObj(luBIS10-ISO8859-2) \
	FontObj(luBIS12-ISO8859-2) FontObj(luBIS14-ISO8859-2) \
	FontObj(luBIS18-ISO8859-2) FontObj(luBIS19-ISO8859-2) \
	FontObj(luBIS24-ISO8859-2) \
	FontObj(luBS08-ISO8859-2) FontObj(luBS10-ISO8859-2) \
	FontObj(luBS12-ISO8859-2) FontObj(luBS14-ISO8859-2) \
	FontObj(luBS18-ISO8859-2) FontObj(luBS19-ISO8859-2) \
	FontObj(luBS24-ISO8859-2) \
	FontObj(luIS08-ISO8859-2) FontObj(luIS10-ISO8859-2) \
	FontObj(luIS12-ISO8859-2) FontObj(luIS14-ISO8859-2) \
	FontObj(luIS18-ISO8859-2) FontObj(luIS19-ISO8859-2) \
	FontObj(luIS24-ISO8859-2) \
	FontObj(luRS08-ISO8859-2) FontObj(luRS10-ISO8859-2) \
	FontObj(luRS12-ISO8859-2) FontObj(luRS14-ISO8859-2) \
	FontObj(luRS18-ISO8859-2) FontObj(luRS19-ISO8859-2) \
	FontObj(luRS24-ISO8859-2) \
	FontObj(lubB08-ISO8859-2) FontObj(lubB10-ISO8859-2) \
	FontObj(lubB12-ISO8859-2) FontObj(lubB14-ISO8859-2) \
	FontObj(lubB18-ISO8859-2) FontObj(lubR19-ISO8859-2) \
	FontObj(lubB24-ISO8859-2) \
	FontObj(lubBI08-ISO8859-2) FontObj(lubBI10-ISO8859-2) \
	FontObj(lubBI12-ISO8859-2) FontObj(lubBI14-ISO8859-2) \
	FontObj(lubBI18-ISO8859-2) FontObj(lubBI19-ISO8859-2) \
	FontObj(lubBI24-ISO8859-2) \
	FontObj(lubI08-ISO8859-2) FontObj(lubI10-ISO8859-2) \
	FontObj(lubI12-ISO8859-2) FontObj(lubI14-ISO8859-2) \
	FontObj(lubI18-ISO8859-2) FontObj(lubI19-ISO8859-2) \
	FontObj(lubI24-ISO8859-2) \
	FontObj(lubR08-ISO8859-2) FontObj(lubR10-ISO8859-2) \
	FontObj(lubR12-ISO8859-2) FontObj(lubR14-ISO8859-2) \
	FontObj(lubR18-ISO8859-2) FontObj(lubR19-ISO8859-2) \
	FontObj(lubR24-ISO8859-2) \
	FontObj(lutBS08-ISO8859-2) FontObj(lutBS10-ISO8859-2) \
	FontObj(lutBS12-ISO8859-2) FontObj(lutBS14-ISO8859-2) \
	FontObj(lutBS18-ISO8859-2) FontObj(lutBS19-ISO8859-2) \
	FontObj(lutBS24-ISO8859-2) \
	FontObj(lutRS08-ISO8859-2) FontObj(lutRS10-ISO8859-2) \
	FontObj(lutRS12-ISO8859-2) FontObj(lutRS14-ISO8859-2) \
	FontObj(lutRS18-ISO8859-2) FontObj(lutRS19-ISO8859-2) \
	FontObj(lutRS24-ISO8859-2) \
	FontObj(UTBI__10-ISO8859-2) FontObj(UTBI__12-ISO8859-2) \
	FontObj(UTBI__14-ISO8859-2) FontObj(UTBI__18-ISO8859-2) \
	FontObj(UTBI__24-ISO8859-2) \
	FontObj(UTB___10-ISO8859-2) FontObj(UTB___12-ISO8859-2) \
	FontObj(UTB___14-ISO8859-2) FontObj(UTB___18-ISO8859-2) \
	FontObj(UTB___24-ISO8859-2) \
	FontObj(UTI___10-ISO8859-2) FontObj(UTI___12-ISO8859-2) \
	FontObj(UTI___14-ISO8859-2) FontObj(UTI___18-ISO8859-2) \
	FontObj(UTI___24-ISO8859-2) \
	FontObj(UTRG__10-ISO8859-2) FontObj(UTRG__12-ISO8859-2) \
	FontObj(UTRG__14-ISO8859-2) FontObj(UTRG__18-ISO8859-2) \
	FontObj(UTRG__24-ISO8859-2)

DERIVED_8859_2_OBJS = $(DERIVED_8859_2_OBJS1) (DERIVED_8859_2_OBJS2)
#endif

#if BuildISO8859_3Fonts
DERIVED_8859_3_OBJS1 = \
	FontObj(courB08-ISO8859-3) FontObj(courB10-ISO8859-3) \
	FontObj(courB12-ISO8859-3) FontObj(courB14-ISO8859-3) \
	FontObj(courB18-ISO8859-3) FontObj(courB24-ISO8859-3) \
	FontObj(courBO08-ISO8859-3) FontObj(courBO10-ISO8859-3) \
	FontObj(courBO12-ISO8859-3) FontObj(courBO14-ISO8859-3) \
	FontObj(courBO18-ISO8859-3) FontObj(courBO24-ISO8859-3) \
	FontObj(courO08-ISO8859-3) FontObj(courO10-ISO8859-3) \
	FontObj(courO12-ISO8859-3) FontObj(courO14-ISO8859-3) \
	FontObj(courO18-ISO8859-3) FontObj(courO24-ISO8859-3) \
	FontObj(courR08-ISO8859-3) FontObj(courR10-ISO8859-3) \
	FontObj(courR12-ISO8859-3) FontObj(courR14-ISO8859-3) \
	FontObj(courR18-ISO8859-3) FontObj(courR24-ISO8859-3) \
	FontObj(helvB08-ISO8859-3) FontObj(helvB10-ISO8859-3) \
	FontObj(helvB12-ISO8859-3) FontObj(helvB14-ISO8859-3) \
	FontObj(helvB18-ISO8859-3) FontObj(helvB24-ISO8859-3) \
	FontObj(helvBO08-ISO8859-3) FontObj(helvBO10-ISO8859-3) \
	FontObj(helvBO12-ISO8859-3) FontObj(helvBO14-ISO8859-3) \
	FontObj(helvBO18-ISO8859-3) FontObj(helvBO24-ISO8859-3) \
	FontObj(helvO08-ISO8859-3) FontObj(helvO10-ISO8859-3) \
	FontObj(helvO12-ISO8859-3) FontObj(helvO14-ISO8859-3) \
	FontObj(helvO18-ISO8859-3) FontObj(helvO24-ISO8859-3) \
	FontObj(helvR08-ISO8859-3) FontObj(helvR10-ISO8859-3) \
	FontObj(helvR12-ISO8859-3) FontObj(helvR14-ISO8859-3) \
	FontObj(helvR18-ISO8859-3) FontObj(helvR24-ISO8859-3) \
	FontObj(ncenB08-ISO8859-3) FontObj(ncenB10-ISO8859-3) \
	FontObj(ncenB12-ISO8859-3) FontObj(ncenB14-ISO8859-3) \
	FontObj(ncenB18-ISO8859-3) FontObj(ncenB24-ISO8859-3) \
	FontObj(ncenBI08-ISO8859-3) FontObj(ncenBI10-ISO8859-3) \
	FontObj(ncenBI12-ISO8859-3) FontObj(ncenBI14-ISO8859-3) \
	FontObj(ncenBI18-ISO8859-3) FontObj(ncenBI24-ISO8859-3) \
	FontObj(ncenI08-ISO8859-3) FontObj(ncenI10-ISO8859-3) \
	FontObj(ncenI12-ISO8859-3) FontObj(ncenI14-ISO8859-3) \
	FontObj(ncenI18-ISO8859-3) FontObj(ncenI24-ISO8859-3) \
	FontObj(ncenR08-ISO8859-3) FontObj(ncenR10-ISO8859-3) \
	FontObj(ncenR12-ISO8859-3) FontObj(ncenR14-ISO8859-3) \
	FontObj(ncenR18-ISO8859-3) FontObj(ncenR24-ISO8859-3) \
	FontObj(timB08-ISO8859-3) FontObj(timB10-ISO8859-3) \
	FontObj(timB12-ISO8859-3) FontObj(timB14-ISO8859-3) \
	FontObj(timB18-ISO8859-3) FontObj(timB24-ISO8859-3) \
	FontObj(timBI08-ISO8859-3) FontObj(timBI10-ISO8859-3) \
	FontObj(timBI12-ISO8859-3) FontObj(timBI14-ISO8859-3) \
	FontObj(timBI18-ISO8859-3) FontObj(timBI24-ISO8859-3) \
	FontObj(timI08-ISO8859-3) FontObj(timI10-ISO8859-3) \
	FontObj(timI12-ISO8859-3) FontObj(timI14-ISO8859-3) \
	FontObj(timI18-ISO8859-3) FontObj(timI24-ISO8859-3) \
	FontObj(timR08-ISO8859-3) FontObj(timR10-ISO8859-3) \
	FontObj(timR12-ISO8859-3) FontObj(timR14-ISO8859-3) \
	FontObj(timR18-ISO8859-3) FontObj(timR24-ISO8859-3)

DERIVED_8859_3_OBJS2 = \
	FontObj(luBIS08-ISO8859-3) FontObj(luBIS10-ISO8859-3) \
	FontObj(luBIS12-ISO8859-3) FontObj(luBIS14-ISO8859-3) \
	FontObj(luBIS18-ISO8859-3) FontObj(luBIS19-ISO8859-3) \
	FontObj(luBIS24-ISO8859-3) \
	FontObj(luBS08-ISO8859-3) FontObj(luBS10-ISO8859-3) \
	FontObj(luBS12-ISO8859-3) FontObj(luBS14-ISO8859-3) \
	FontObj(luBS18-ISO8859-3) FontObj(luBS19-ISO8859-3) \
	FontObj(luBS24-ISO8859-3) \
	FontObj(luIS08-ISO8859-3) FontObj(luIS10-ISO8859-3) \
	FontObj(luIS12-ISO8859-3) FontObj(luIS14-ISO8859-3) \
	FontObj(luIS18-ISO8859-3) FontObj(luIS19-ISO8859-3) \
	FontObj(luIS24-ISO8859-3) \
	FontObj(luRS08-ISO8859-3) FontObj(luRS10-ISO8859-3) \
	FontObj(luRS12-ISO8859-3) FontObj(luRS14-ISO8859-3) \
	FontObj(luRS18-ISO8859-3) FontObj(luRS19-ISO8859-3) \
	FontObj(luRS24-ISO8859-3) \
	FontObj(lubB08-ISO8859-3) FontObj(lubB10-ISO8859-3) \
	FontObj(lubB12-ISO8859-3) FontObj(lubB14-ISO8859-3) \
	FontObj(lubB18-ISO8859-3) FontObj(lubR19-ISO8859-3) \
	FontObj(lubB24-ISO8859-3) \
	FontObj(lubBI08-ISO8859-3) FontObj(lubBI10-ISO8859-3) \
	FontObj(lubBI12-ISO8859-3) FontObj(lubBI14-ISO8859-3) \
	FontObj(lubBI18-ISO8859-3) FontObj(lubBI19-ISO8859-3) \
	FontObj(lubBI24-ISO8859-3) \
	FontObj(lubI08-ISO8859-3) FontObj(lubI10-ISO8859-3) \
	FontObj(lubI12-ISO8859-3) FontObj(lubI14-ISO8859-3) \
	FontObj(lubI18-ISO8859-3) FontObj(lubI19-ISO8859-3) \
	FontObj(lubI24-ISO8859-3) \
	FontObj(lubR08-ISO8859-3) FontObj(lubR10-ISO8859-3) \
	FontObj(lubR12-ISO8859-3) FontObj(lubR14-ISO8859-3) \
	FontObj(lubR18-ISO8859-3) FontObj(lubR19-ISO8859-3) \
	FontObj(lubR24-ISO8859-3) \
	FontObj(lutBS08-ISO8859-3) FontObj(lutBS10-ISO8859-3) \
	FontObj(lutBS12-ISO8859-3) FontObj(lutBS14-ISO8859-3) \
	FontObj(lutBS18-ISO8859-3) FontObj(lutBS19-ISO8859-3) \
	FontObj(lutBS24-ISO8859-3) \
	FontObj(lutRS08-ISO8859-3) FontObj(lutRS10-ISO8859-3) \
	FontObj(lutRS12-ISO8859-3) FontObj(lutRS14-ISO8859-3) \
	FontObj(lutRS18-ISO8859-3) FontObj(lutRS19-ISO8859-3) \
	FontObj(lutRS24-ISO8859-3) \
	FontObj(UTBI__10-ISO8859-3) FontObj(UTBI__12-ISO8859-3) \
	FontObj(UTBI__14-ISO8859-3) FontObj(UTBI__18-ISO8859-3) \
	FontObj(UTBI__24-ISO8859-3) \
	FontObj(UTB___10-ISO8859-3) FontObj(UTB___12-ISO8859-3) \
	FontObj(UTB___14-ISO8859-3) FontObj(UTB___18-ISO8859-3) \
	FontObj(UTB___24-ISO8859-3) \
	FontObj(UTI___10-ISO8859-3) FontObj(UTI___12-ISO8859-3) \
	FontObj(UTI___14-ISO8859-3) FontObj(UTI___18-ISO8859-3) \
	FontObj(UTI___24-ISO8859-3) \
	FontObj(UTRG__10-ISO8859-3) FontObj(UTRG__12-ISO8859-3) \
	FontObj(UTRG__14-ISO8859-3) FontObj(UTRG__18-ISO8859-3) \
	FontObj(UTRG__24-ISO8859-3)

DERIVED_8859_3_OBJS = $(DERIVED_8859_3_OBJS1) (DERIVED_8859_3_OBJS2)
#endif

#if BuildISO8859_4Fonts
DERIVED_8859_4_OBJS1 = \
	FontObj(courB08-ISO8859-4) FontObj(courB10-ISO8859-4) \
	FontObj(courB12-ISO8859-4) FontObj(courB14-ISO8859-4) \
	FontObj(courB18-ISO8859-4) FontObj(courB24-ISO8859-4) \
	FontObj(courBO08-ISO8859-4) FontObj(courBO10-ISO8859-4) \
	FontObj(courBO12-ISO8859-4) FontObj(courBO14-ISO8859-4) \
	FontObj(courBO18-ISO8859-4) FontObj(courBO24-ISO8859-4) \
	FontObj(courO08-ISO8859-4) FontObj(courO10-ISO8859-4) \
	FontObj(courO12-ISO8859-4) FontObj(courO14-ISO8859-4) \
	FontObj(courO18-ISO8859-4) FontObj(courO24-ISO8859-4) \
	FontObj(courR08-ISO8859-4) FontObj(courR10-ISO8859-4) \
	FontObj(courR12-ISO8859-4) FontObj(courR14-ISO8859-4) \
	FontObj(courR18-ISO8859-4) FontObj(courR24-ISO8859-4) \
	FontObj(helvB08-ISO8859-4) FontObj(helvB10-ISO8859-4) \
	FontObj(helvB12-ISO8859-4) FontObj(helvB14-ISO8859-4) \
	FontObj(helvB18-ISO8859-4) FontObj(helvB24-ISO8859-4) \
	FontObj(helvBO08-ISO8859-4) FontObj(helvBO10-ISO8859-4) \
	FontObj(helvBO12-ISO8859-4) FontObj(helvBO14-ISO8859-4) \
	FontObj(helvBO18-ISO8859-4) FontObj(helvBO24-ISO8859-4) \
	FontObj(helvO08-ISO8859-4) FontObj(helvO10-ISO8859-4) \
	FontObj(helvO12-ISO8859-4) FontObj(helvO14-ISO8859-4) \
	FontObj(helvO18-ISO8859-4) FontObj(helvO24-ISO8859-4) \
	FontObj(helvR08-ISO8859-4) FontObj(helvR10-ISO8859-4) \
	FontObj(helvR12-ISO8859-4) FontObj(helvR14-ISO8859-4) \
	FontObj(helvR18-ISO8859-4) FontObj(helvR24-ISO8859-4) \
	FontObj(ncenB08-ISO8859-4) FontObj(ncenB10-ISO8859-4) \
	FontObj(ncenB12-ISO8859-4) FontObj(ncenB14-ISO8859-4) \
	FontObj(ncenB18-ISO8859-4) FontObj(ncenB24-ISO8859-4) \
	FontObj(ncenBI08-ISO8859-4) FontObj(ncenBI10-ISO8859-4) \
	FontObj(ncenBI12-ISO8859-4) FontObj(ncenBI14-ISO8859-4) \
	FontObj(ncenBI18-ISO8859-4) FontObj(ncenBI24-ISO8859-4) \
	FontObj(ncenI08-ISO8859-4) FontObj(ncenI10-ISO8859-4) \
	FontObj(ncenI12-ISO8859-4) FontObj(ncenI14-ISO8859-4) \
	FontObj(ncenI18-ISO8859-4) FontObj(ncenI24-ISO8859-4) \
	FontObj(ncenR08-ISO8859-4) FontObj(ncenR10-ISO8859-4) \
	FontObj(ncenR12-ISO8859-4) FontObj(ncenR14-ISO8859-4) \
	FontObj(ncenR18-ISO8859-4) FontObj(ncenR24-ISO8859-4) \
	FontObj(timB08-ISO8859-4) FontObj(timB10-ISO8859-4) \
	FontObj(timB12-ISO8859-4) FontObj(timB14-ISO8859-4) \
	FontObj(timB18-ISO8859-4) FontObj(timB24-ISO8859-4) \
	FontObj(timBI08-ISO8859-4) FontObj(timBI10-ISO8859-4) \
	FontObj(timBI12-ISO8859-4) FontObj(timBI14-ISO8859-4) \
	FontObj(timBI18-ISO8859-4) FontObj(timBI24-ISO8859-4) \
	FontObj(timI08-ISO8859-4) FontObj(timI10-ISO8859-4) \
	FontObj(timI12-ISO8859-4) FontObj(timI14-ISO8859-4) \
	FontObj(timI18-ISO8859-4) FontObj(timI24-ISO8859-4) \
	FontObj(timR08-ISO8859-4) FontObj(timR10-ISO8859-4) \
	FontObj(timR12-ISO8859-4) FontObj(timR14-ISO8859-4) \
	FontObj(timR18-ISO8859-4) FontObj(timR24-ISO8859-4)

DERIVED_8859_4_OBJS2 = \
	FontObj(luBIS08-ISO8859-4) FontObj(luBIS10-ISO8859-4) \
	FontObj(luBIS12-ISO8859-4) FontObj(luBIS14-ISO8859-4) \
	FontObj(luBIS18-ISO8859-4) FontObj(luBIS19-ISO8859-4) \
	FontObj(luBIS24-ISO8859-4) \
	FontObj(luBS08-ISO8859-4) FontObj(luBS10-ISO8859-4) \
	FontObj(luBS12-ISO8859-4) FontObj(luBS14-ISO8859-4) \
	FontObj(luBS18-ISO8859-4) FontObj(luBS19-ISO8859-4) \
	FontObj(luBS24-ISO8859-4) \
	FontObj(luIS08-ISO8859-4) FontObj(luIS10-ISO8859-4) \
	FontObj(luIS12-ISO8859-4) FontObj(luIS14-ISO8859-4) \
	FontObj(luIS18-ISO8859-4) FontObj(luIS19-ISO8859-4) \
	FontObj(luIS24-ISO8859-4) \
	FontObj(luRS08-ISO8859-4) FontObj(luRS10-ISO8859-4) \
	FontObj(luRS12-ISO8859-4) FontObj(luRS14-ISO8859-4) \
	FontObj(luRS18-ISO8859-4) FontObj(luRS19-ISO8859-4) \
	FontObj(luRS24-ISO8859-4) \
	FontObj(lubB08-ISO8859-4) FontObj(lubB10-ISO8859-4) \
	FontObj(lubB12-ISO8859-4) FontObj(lubB14-ISO8859-4) \
	FontObj(lubB18-ISO8859-4) FontObj(lubR19-ISO8859-4) \
	FontObj(lubB24-ISO8859-4) \
	FontObj(lubBI08-ISO8859-4) FontObj(lubBI10-ISO8859-4) \
	FontObj(lubBI12-ISO8859-4) FontObj(lubBI14-ISO8859-4) \
	FontObj(lubBI18-ISO8859-4) FontObj(lubBI19-ISO8859-4) \
	FontObj(lubBI24-ISO8859-4) \
	FontObj(lubI08-ISO8859-4) FontObj(lubI10-ISO8859-4) \
	FontObj(lubI12-ISO8859-4) FontObj(lubI14-ISO8859-4) \
	FontObj(lubI18-ISO8859-4) FontObj(lubI19-ISO8859-4) \
	FontObj(lubI24-ISO8859-4) \
	FontObj(lubR08-ISO8859-4) FontObj(lubR10-ISO8859-4) \
	FontObj(lubR12-ISO8859-4) FontObj(lubR14-ISO8859-4) \
	FontObj(lubR18-ISO8859-4) FontObj(lubR19-ISO8859-4) \
	FontObj(lubR24-ISO8859-4) \
	FontObj(lutBS08-ISO8859-4) FontObj(lutBS10-ISO8859-4) \
	FontObj(lutBS12-ISO8859-4) FontObj(lutBS14-ISO8859-4) \
	FontObj(lutBS18-ISO8859-4) FontObj(lutBS19-ISO8859-4) \
	FontObj(lutBS24-ISO8859-4) \
	FontObj(lutRS08-ISO8859-4) FontObj(lutRS10-ISO8859-4) \
	FontObj(lutRS12-ISO8859-4) FontObj(lutRS14-ISO8859-4) \
	FontObj(lutRS18-ISO8859-4) FontObj(lutRS19-ISO8859-4) \
	FontObj(lutRS24-ISO8859-4) \
	FontObj(UTBI__10-ISO8859-4) FontObj(UTBI__12-ISO8859-4) \
	FontObj(UTBI__14-ISO8859-4) FontObj(UTBI__18-ISO8859-4) \
	FontObj(UTBI__24-ISO8859-4) \
	FontObj(UTB___10-ISO8859-4) FontObj(UTB___12-ISO8859-4) \
	FontObj(UTB___14-ISO8859-4) FontObj(UTB___18-ISO8859-4) \
	FontObj(UTB___24-ISO8859-4) \
	FontObj(UTI___10-ISO8859-4) FontObj(UTI___12-ISO8859-4) \
	FontObj(UTI___14-ISO8859-4) FontObj(UTI___18-ISO8859-4) \
	FontObj(UTI___24-ISO8859-4) \
	FontObj(UTRG__10-ISO8859-4) FontObj(UTRG__12-ISO8859-4) \
	FontObj(UTRG__14-ISO8859-4) FontObj(UTRG__18-ISO8859-4) \
	FontObj(UTRG__24-ISO8859-4)

DERIVED_8859_4_OBJS = $(DERIVED_8859_4_OBJS1) (DERIVED_8859_4_OBJS2)
#endif

#if BuildISO8859_9Fonts
DERIVED_8859_9_OBJS1 = \
	FontObj(courB08-ISO8859-9) FontObj(courB10-ISO8859-9) \
	FontObj(courB12-ISO8859-9) FontObj(courB14-ISO8859-9) \
	FontObj(courB18-ISO8859-9) FontObj(courB24-ISO8859-9) \
	FontObj(courBO08-ISO8859-9) FontObj(courBO10-ISO8859-9) \
	FontObj(courBO12-ISO8859-9) FontObj(courBO14-ISO8859-9) \
	FontObj(courBO18-ISO8859-9) FontObj(courBO24-ISO8859-9) \
	FontObj(courO08-ISO8859-9) FontObj(courO10-ISO8859-9) \
	FontObj(courO12-ISO8859-9) FontObj(courO14-ISO8859-9) \
	FontObj(courO18-ISO8859-9) FontObj(courO24-ISO8859-9) \
	FontObj(courR08-ISO8859-9) FontObj(courR10-ISO8859-9) \
	FontObj(courR12-ISO8859-9) FontObj(courR14-ISO8859-9) \
	FontObj(courR18-ISO8859-9) FontObj(courR24-ISO8859-9) \
	FontObj(helvB08-ISO8859-9) FontObj(helvB10-ISO8859-9) \
	FontObj(helvB12-ISO8859-9) FontObj(helvB14-ISO8859-9) \
	FontObj(helvB18-ISO8859-9) FontObj(helvB24-ISO8859-9) \
	FontObj(helvBO08-ISO8859-9) FontObj(helvBO10-ISO8859-9) \
	FontObj(helvBO12-ISO8859-9) FontObj(helvBO14-ISO8859-9) \
	FontObj(helvBO18-ISO8859-9) FontObj(helvBO24-ISO8859-9) \
	FontObj(helvO08-ISO8859-9) FontObj(helvO10-ISO8859-9) \
	FontObj(helvO12-ISO8859-9) FontObj(helvO14-ISO8859-9) \
	FontObj(helvO18-ISO8859-9) FontObj(helvO24-ISO8859-9) \
	FontObj(helvR08-ISO8859-9) FontObj(helvR10-ISO8859-9) \
	FontObj(helvR12-ISO8859-9) FontObj(helvR14-ISO8859-9) \
	FontObj(helvR18-ISO8859-9) FontObj(helvR24-ISO8859-9) \
	FontObj(ncenB08-ISO8859-9) FontObj(ncenB10-ISO8859-9) \
	FontObj(ncenB12-ISO8859-9) FontObj(ncenB14-ISO8859-9) \
	FontObj(ncenB18-ISO8859-9) FontObj(ncenB24-ISO8859-9) \
	FontObj(ncenBI08-ISO8859-9) FontObj(ncenBI10-ISO8859-9) \
	FontObj(ncenBI12-ISO8859-9) FontObj(ncenBI14-ISO8859-9) \
	FontObj(ncenBI18-ISO8859-9) FontObj(ncenBI24-ISO8859-9) \
	FontObj(ncenI08-ISO8859-9) FontObj(ncenI10-ISO8859-9) \
	FontObj(ncenI12-ISO8859-9) FontObj(ncenI14-ISO8859-9) \
	FontObj(ncenI18-ISO8859-9) FontObj(ncenI24-ISO8859-9) \
	FontObj(ncenR08-ISO8859-9) FontObj(ncenR10-ISO8859-9) \
	FontObj(ncenR12-ISO8859-9) FontObj(ncenR14-ISO8859-9) \
	FontObj(ncenR18-ISO8859-9) FontObj(ncenR24-ISO8859-9) \
	FontObj(timB08-ISO8859-9) FontObj(timB10-ISO8859-9) \
	FontObj(timB12-ISO8859-9) FontObj(timB14-ISO8859-9) \
	FontObj(timB18-ISO8859-9) FontObj(timB24-ISO8859-9) \
	FontObj(timBI08-ISO8859-9) FontObj(timBI10-ISO8859-9) \
	FontObj(timBI12-ISO8859-9) FontObj(timBI14-ISO8859-9) \
	FontObj(timBI18-ISO8859-9) FontObj(timBI24-ISO8859-9) \
	FontObj(timI08-ISO8859-9) FontObj(timI10-ISO8859-9) \
	FontObj(timI12-ISO8859-9) FontObj(timI14-ISO8859-9) \
	FontObj(timI18-ISO8859-9) FontObj(timI24-ISO8859-9) \
	FontObj(timR08-ISO8859-9) FontObj(timR10-ISO8859-9) \
	FontObj(timR12-ISO8859-9) FontObj(timR14-ISO8859-9) \
	FontObj(timR18-ISO8859-9) FontObj(timR24-ISO8859-9)

DERIVED_8859_9_OBJS2 = \
	FontObj(luBIS08-ISO8859-9) FontObj(luBIS10-ISO8859-9) \
	FontObj(luBIS12-ISO8859-9) FontObj(luBIS14-ISO8859-9) \
	FontObj(luBIS18-ISO8859-9) FontObj(luBIS19-ISO8859-9) \
	FontObj(luBIS24-ISO8859-9) \
	FontObj(luBS08-ISO8859-9) FontObj(luBS10-ISO8859-9) \
	FontObj(luBS12-ISO8859-9) FontObj(luBS14-ISO8859-9) \
	FontObj(luBS18-ISO8859-9) FontObj(luBS19-ISO8859-9) \
	FontObj(luBS24-ISO8859-9) \
	FontObj(luIS08-ISO8859-9) FontObj(luIS10-ISO8859-9) \
	FontObj(luIS12-ISO8859-9) FontObj(luIS14-ISO8859-9) \
	FontObj(luIS18-ISO8859-9) FontObj(luIS19-ISO8859-9) \
	FontObj(luIS24-ISO8859-9) \
	FontObj(luRS08-ISO8859-9) FontObj(luRS10-ISO8859-9) \
	FontObj(luRS12-ISO8859-9) FontObj(luRS14-ISO8859-9) \
	FontObj(luRS18-ISO8859-9) FontObj(luRS19-ISO8859-9) \
	FontObj(luRS24-ISO8859-9) \
	FontObj(lubB08-ISO8859-9) FontObj(lubB10-ISO8859-9) \
	FontObj(lubB12-ISO8859-9) FontObj(lubB14-ISO8859-9) \
	FontObj(lubB18-ISO8859-9) FontObj(lubR19-ISO8859-9) \
	FontObj(lubB24-ISO8859-9) \
	FontObj(lubBI08-ISO8859-9) FontObj(lubBI10-ISO8859-9) \
	FontObj(lubBI12-ISO8859-9) FontObj(lubBI14-ISO8859-9) \
	FontObj(lubBI18-ISO8859-9) FontObj(lubBI19-ISO8859-9) \
	FontObj(lubBI24-ISO8859-9) \
	FontObj(lubI08-ISO8859-9) FontObj(lubI10-ISO8859-9) \
	FontObj(lubI12-ISO8859-9) FontObj(lubI14-ISO8859-9) \
	FontObj(lubI18-ISO8859-9) FontObj(lubI19-ISO8859-9) \
	FontObj(lubI24-ISO8859-9) \
	FontObj(lubR08-ISO8859-9) FontObj(lubR10-ISO8859-9) \
	FontObj(lubR12-ISO8859-9) FontObj(lubR14-ISO8859-9) \
	FontObj(lubR18-ISO8859-9) FontObj(lubR19-ISO8859-9) \
	FontObj(lubR24-ISO8859-9) \
	FontObj(lutBS08-ISO8859-9) FontObj(lutBS10-ISO8859-9) \
	FontObj(lutBS12-ISO8859-9) FontObj(lutBS14-ISO8859-9) \
	FontObj(lutBS18-ISO8859-9) FontObj(lutBS19-ISO8859-9) \
	FontObj(lutBS24-ISO8859-9) \
	FontObj(lutRS08-ISO8859-9) FontObj(lutRS10-ISO8859-9) \
	FontObj(lutRS12-ISO8859-9) FontObj(lutRS14-ISO8859-9) \
	FontObj(lutRS18-ISO8859-9) FontObj(lutRS19-ISO8859-9) \
	FontObj(lutRS24-ISO8859-9) \
	FontObj(UTBI__10-ISO8859-9) FontObj(UTBI__12-ISO8859-9) \
	FontObj(UTBI__14-ISO8859-9) FontObj(UTBI__18-ISO8859-9) \
	FontObj(UTBI__24-ISO8859-9) \
	FontObj(UTB___10-ISO8859-9) FontObj(UTB___12-ISO8859-9) \
	FontObj(UTB___14-ISO8859-9) FontObj(UTB___18-ISO8859-9) \
	FontObj(UTB___24-ISO8859-9) \
	FontObj(UTI___10-ISO8859-9) FontObj(UTI___12-ISO8859-9) \
	FontObj(UTI___14-ISO8859-9) FontObj(UTI___18-ISO8859-9) \
	FontObj(UTI___24-ISO8859-9) \
	FontObj(UTRG__10-ISO8859-9) FontObj(UTRG__12-ISO8859-9) \
	FontObj(UTRG__14-ISO8859-9) FontObj(UTRG__18-ISO8859-9) \
	FontObj(UTRG__24-ISO8859-9)

DERIVED_8859_9_OBJS = $(DERIVED_8859_9_OBJS1) (DERIVED_8859_9_OBJS2)
#endif

#if BuildISO8859_10Fonts
DERIVED_8859_10_OBJS1 = \
	FontObj(courB08-ISO8859-10) FontObj(courB10-ISO8859-10) \
	FontObj(courB12-ISO8859-10) FontObj(courB14-ISO8859-10) \
	FontObj(courB18-ISO8859-10) FontObj(courB24-ISO8859-10) \
	FontObj(courBO08-ISO8859-10) FontObj(courBO10-ISO8859-10) \
	FontObj(courBO12-ISO8859-10) FontObj(courBO14-ISO8859-10) \
	FontObj(courBO18-ISO8859-10) FontObj(courBO24-ISO8859-10) \
	FontObj(courO08-ISO8859-10) FontObj(courO10-ISO8859-10) \
	FontObj(courO12-ISO8859-10) FontObj(courO14-ISO8859-10) \
	FontObj(courO18-ISO8859-10) FontObj(courO24-ISO8859-10) \
	FontObj(courR08-ISO8859-10) FontObj(courR10-ISO8859-10) \
	FontObj(courR12-ISO8859-10) FontObj(courR14-ISO8859-10) \
	FontObj(courR18-ISO8859-10) FontObj(courR24-ISO8859-10) \
	FontObj(helvB08-ISO8859-10) FontObj(helvB10-ISO8859-10) \
	FontObj(helvB12-ISO8859-10) FontObj(helvB14-ISO8859-10) \
	FontObj(helvB18-ISO8859-10) FontObj(helvB24-ISO8859-10) \
	FontObj(helvBO08-ISO8859-10) FontObj(helvBO10-ISO8859-10) \
	FontObj(helvBO12-ISO8859-10) FontObj(helvBO14-ISO8859-10) \
	FontObj(helvBO18-ISO8859-10) FontObj(helvBO24-ISO8859-10) \
	FontObj(helvO08-ISO8859-10) FontObj(helvO10-ISO8859-10) \
	FontObj(helvO12-ISO8859-10) FontObj(helvO14-ISO8859-10) \
	FontObj(helvO18-ISO8859-10) FontObj(helvO24-ISO8859-10) \
	FontObj(helvR08-ISO8859-10) FontObj(helvR10-ISO8859-10) \
	FontObj(helvR12-ISO8859-10) FontObj(helvR14-ISO8859-10) \
	FontObj(helvR18-ISO8859-10) FontObj(helvR24-ISO8859-10) \
	FontObj(ncenB08-ISO8859-10) FontObj(ncenB10-ISO8859-10) \
	FontObj(ncenB12-ISO8859-10) FontObj(ncenB14-ISO8859-10) \
	FontObj(ncenB18-ISO8859-10) FontObj(ncenB24-ISO8859-10) \
	FontObj(ncenBI08-ISO8859-10) FontObj(ncenBI10-ISO8859-10) \
	FontObj(ncenBI12-ISO8859-10) FontObj(ncenBI14-ISO8859-10) \
	FontObj(ncenBI18-ISO8859-10) FontObj(ncenBI24-ISO8859-10) \
	FontObj(ncenI08-ISO8859-10) FontObj(ncenI10-ISO8859-10) \
	FontObj(ncenI12-ISO8859-10) FontObj(ncenI14-ISO8859-10) \
	FontObj(ncenI18-ISO8859-10) FontObj(ncenI24-ISO8859-10) \
	FontObj(ncenR08-ISO8859-10) FontObj(ncenR10-ISO8859-10) \
	FontObj(ncenR12-ISO8859-10) FontObj(ncenR14-ISO8859-10) \
	FontObj(ncenR18-ISO8859-10) FontObj(ncenR24-ISO8859-10) \
	FontObj(timB08-ISO8859-10) FontObj(timB10-ISO8859-10) \
	FontObj(timB12-ISO8859-10) FontObj(timB14-ISO8859-10) \
	FontObj(timB18-ISO8859-10) FontObj(timB24-ISO8859-10) \
	FontObj(timBI08-ISO8859-10) FontObj(timBI10-ISO8859-10) \
	FontObj(timBI12-ISO8859-10) FontObj(timBI14-ISO8859-10) \
	FontObj(timBI18-ISO8859-10) FontObj(timBI24-ISO8859-10) \
	FontObj(timI08-ISO8859-10) FontObj(timI10-ISO8859-10) \
	FontObj(timI12-ISO8859-10) FontObj(timI14-ISO8859-10) \
	FontObj(timI18-ISO8859-10) FontObj(timI24-ISO8859-10) \
	FontObj(timR08-ISO8859-10) FontObj(timR10-ISO8859-10) \
	FontObj(timR12-ISO8859-10) FontObj(timR14-ISO8859-10) \
	FontObj(timR18-ISO8859-10) FontObj(timR24-ISO8859-10)

DERIVED_8859_10_OBJS2 = \
	FontObj(luBIS08-ISO8859-10) FontObj(luBIS10-ISO8859-10) \
	FontObj(luBIS12-ISO8859-10) FontObj(luBIS14-ISO8859-10) \
	FontObj(luBIS18-ISO8859-10) FontObj(luBIS19-ISO8859-10) \
	FontObj(luBIS24-ISO8859-10) \
	FontObj(luBS08-ISO8859-10) FontObj(luBS10-ISO8859-10) \
	FontObj(luBS12-ISO8859-10) FontObj(luBS14-ISO8859-10) \
	FontObj(luBS18-ISO8859-10) FontObj(luBS19-ISO8859-10) \
	FontObj(luBS24-ISO8859-10) \
	FontObj(luIS08-ISO8859-10) FontObj(luIS10-ISO8859-10) \
	FontObj(luIS12-ISO8859-10) FontObj(luIS14-ISO8859-10) \
	FontObj(luIS18-ISO8859-10) FontObj(luIS19-ISO8859-10) \
	FontObj(luIS24-ISO8859-10) \
	FontObj(luRS08-ISO8859-10) FontObj(luRS10-ISO8859-10) \
	FontObj(luRS12-ISO8859-10) FontObj(luRS14-ISO8859-10) \
	FontObj(luRS18-ISO8859-10) FontObj(luRS19-ISO8859-10) \
	FontObj(luRS24-ISO8859-10) \
	FontObj(lubB08-ISO8859-10) FontObj(lubB10-ISO8859-10) \
	FontObj(lubB12-ISO8859-10) FontObj(lubB14-ISO8859-10) \
	FontObj(lubB18-ISO8859-10) FontObj(lubR19-ISO8859-10) \
	FontObj(lubB24-ISO8859-10) \
	FontObj(lubBI08-ISO8859-10) FontObj(lubBI10-ISO8859-10) \
	FontObj(lubBI12-ISO8859-10) FontObj(lubBI14-ISO8859-10) \
	FontObj(lubBI18-ISO8859-10) FontObj(lubBI19-ISO8859-10) \
	FontObj(lubBI24-ISO8859-10) \
	FontObj(lubI08-ISO8859-10) FontObj(lubI10-ISO8859-10) \
	FontObj(lubI12-ISO8859-10) FontObj(lubI14-ISO8859-10) \
	FontObj(lubI18-ISO8859-10) FontObj(lubI19-ISO8859-10) \
	FontObj(lubI24-ISO8859-10) \
	FontObj(lubR08-ISO8859-10) FontObj(lubR10-ISO8859-10) \
	FontObj(lubR12-ISO8859-10) FontObj(lubR14-ISO8859-10) \
	FontObj(lubR18-ISO8859-10) FontObj(lubR19-ISO8859-10) \
	FontObj(lubR24-ISO8859-10) \
	FontObj(lutBS08-ISO8859-10) FontObj(lutBS10-ISO8859-10) \
	FontObj(lutBS12-ISO8859-10) FontObj(lutBS14-ISO8859-10) \
	FontObj(lutBS18-ISO8859-10) FontObj(lutBS19-ISO8859-10) \
	FontObj(lutBS24-ISO8859-10) \
	FontObj(lutRS08-ISO8859-10) FontObj(lutRS10-ISO8859-10) \
	FontObj(lutRS12-ISO8859-10) FontObj(lutRS14-ISO8859-10) \
	FontObj(lutRS18-ISO8859-10) FontObj(lutRS19-ISO8859-10) \
	FontObj(lutRS24-ISO8859-10) \
	FontObj(UTBI__10-ISO8859-10) FontObj(UTBI__12-ISO8859-10) \
	FontObj(UTBI__14-ISO8859-10) FontObj(UTBI__18-ISO8859-10) \
	FontObj(UTBI__24-ISO8859-10) \
	FontObj(UTB___10-ISO8859-10) FontObj(UTB___12-ISO8859-10) \
	FontObj(UTB___14-ISO8859-10) FontObj(UTB___18-ISO8859-10) \
	FontObj(UTB___24-ISO8859-10) \
	FontObj(UTI___10-ISO8859-10) FontObj(UTI___12-ISO8859-10) \
	FontObj(UTI___14-ISO8859-10) FontObj(UTI___18-ISO8859-10) \
	FontObj(UTI___24-ISO8859-10) \
	FontObj(UTRG__10-ISO8859-10) FontObj(UTRG__12-ISO8859-10) \
	FontObj(UTRG__14-ISO8859-10) FontObj(UTRG__18-ISO8859-10) \
	FontObj(UTRG__24-ISO8859-10)

DERIVED_8859_10_OBJS = $(DERIVED_8859_10_OBJS1) (DERIVED_8859_10_OBJS2)
#endif

#if BuildISO8859_13Fonts
DERIVED_8859_13_OBJS1 = \
	FontObj(courB08-ISO8859-13) FontObj(courB10-ISO8859-13) \
	FontObj(courB12-ISO8859-13) FontObj(courB14-ISO8859-13) \
	FontObj(courB18-ISO8859-13) FontObj(courB24-ISO8859-13) \
	FontObj(courBO08-ISO8859-13) FontObj(courBO10-ISO8859-13) \
	FontObj(courBO12-ISO8859-13) FontObj(courBO14-ISO8859-13) \
	FontObj(courBO18-ISO8859-13) FontObj(courBO24-ISO8859-13) \
	FontObj(courO08-ISO8859-13) FontObj(courO10-ISO8859-13) \
	FontObj(courO12-ISO8859-13) FontObj(courO14-ISO8859-13) \
	FontObj(courO18-ISO8859-13) FontObj(courO24-ISO8859-13) \
	FontObj(courR08-ISO8859-13) FontObj(courR10-ISO8859-13) \
	FontObj(courR12-ISO8859-13) FontObj(courR14-ISO8859-13) \
	FontObj(courR18-ISO8859-13) FontObj(courR24-ISO8859-13) \
	FontObj(helvB08-ISO8859-13) FontObj(helvB10-ISO8859-13) \
	FontObj(helvB12-ISO8859-13) FontObj(helvB14-ISO8859-13) \
	FontObj(helvB18-ISO8859-13) FontObj(helvB24-ISO8859-13) \
	FontObj(helvBO08-ISO8859-13) FontObj(helvBO10-ISO8859-13) \
	FontObj(helvBO12-ISO8859-13) FontObj(helvBO14-ISO8859-13) \
	FontObj(helvBO18-ISO8859-13) FontObj(helvBO24-ISO8859-13) \
	FontObj(helvO08-ISO8859-13) FontObj(helvO10-ISO8859-13) \
	FontObj(helvO12-ISO8859-13) FontObj(helvO14-ISO8859-13) \
	FontObj(helvO18-ISO8859-13) FontObj(helvO24-ISO8859-13) \
	FontObj(helvR08-ISO8859-13) FontObj(helvR10-ISO8859-13) \
	FontObj(helvR12-ISO8859-13) FontObj(helvR14-ISO8859-13) \
	FontObj(helvR18-ISO8859-13) FontObj(helvR24-ISO8859-13) \
	FontObj(ncenB08-ISO8859-13) FontObj(ncenB10-ISO8859-13) \
	FontObj(ncenB12-ISO8859-13) FontObj(ncenB14-ISO8859-13) \
	FontObj(ncenB18-ISO8859-13) FontObj(ncenB24-ISO8859-13) \
	FontObj(ncenBI08-ISO8859-13) FontObj(ncenBI10-ISO8859-13) \
	FontObj(ncenBI12-ISO8859-13) FontObj(ncenBI14-ISO8859-13) \
	FontObj(ncenBI18-ISO8859-13) FontObj(ncenBI24-ISO8859-13) \
	FontObj(ncenI08-ISO8859-13) FontObj(ncenI10-ISO8859-13) \
	FontObj(ncenI12-ISO8859-13) FontObj(ncenI14-ISO8859-13) \
	FontObj(ncenI18-ISO8859-13) FontObj(ncenI24-ISO8859-13) \
	FontObj(ncenR08-ISO8859-13) FontObj(ncenR10-ISO8859-13) \
	FontObj(ncenR12-ISO8859-13) FontObj(ncenR14-ISO8859-13) \
	FontObj(ncenR18-ISO8859-13) FontObj(ncenR24-ISO8859-13) \
	FontObj(timB08-ISO8859-13) FontObj(timB10-ISO8859-13) \
	FontObj(timB12-ISO8859-13) FontObj(timB14-ISO8859-13) \
	FontObj(timB18-ISO8859-13) FontObj(timB24-ISO8859-13) \
	FontObj(timBI08-ISO8859-13) FontObj(timBI10-ISO8859-13) \
	FontObj(timBI12-ISO8859-13) FontObj(timBI14-ISO8859-13) \
	FontObj(timBI18-ISO8859-13) FontObj(timBI24-ISO8859-13) \
	FontObj(timI08-ISO8859-13) FontObj(timI10-ISO8859-13) \
	FontObj(timI12-ISO8859-13) FontObj(timI14-ISO8859-13) \
	FontObj(timI18-ISO8859-13) FontObj(timI24-ISO8859-13) \
	FontObj(timR08-ISO8859-13) FontObj(timR10-ISO8859-13) \
	FontObj(timR12-ISO8859-13) FontObj(timR14-ISO8859-13) \
	FontObj(timR18-ISO8859-13) FontObj(timR24-ISO8859-13)

DERIVED_8859_13_OBJS2 = \
	FontObj(luBIS08-ISO8859-13) FontObj(luBIS10-ISO8859-13) \
	FontObj(luBIS12-ISO8859-13) FontObj(luBIS14-ISO8859-13) \
	FontObj(luBIS18-ISO8859-13) FontObj(luBIS19-ISO8859-13) \
	FontObj(luBIS24-ISO8859-13) \
	FontObj(luBS08-ISO8859-13) FontObj(luBS10-ISO8859-13) \
	FontObj(luBS12-ISO8859-13) FontObj(luBS14-ISO8859-13) \
	FontObj(luBS18-ISO8859-13) FontObj(luBS19-ISO8859-13) \
	FontObj(luBS24-ISO8859-13) \
	FontObj(luIS08-ISO8859-13) FontObj(luIS10-ISO8859-13) \
	FontObj(luIS12-ISO8859-13) FontObj(luIS14-ISO8859-13) \
	FontObj(luIS18-ISO8859-13) FontObj(luIS19-ISO8859-13) \
	FontObj(luIS24-ISO8859-13) \
	FontObj(luRS08-ISO8859-13) FontObj(luRS10-ISO8859-13) \
	FontObj(luRS12-ISO8859-13) FontObj(luRS14-ISO8859-13) \
	FontObj(luRS18-ISO8859-13) FontObj(luRS19-ISO8859-13) \
	FontObj(luRS24-ISO8859-13) \
	FontObj(lubB08-ISO8859-13) FontObj(lubB10-ISO8859-13) \
	FontObj(lubB12-ISO8859-13) FontObj(lubB14-ISO8859-13) \
	FontObj(lubB18-ISO8859-13) FontObj(lubR19-ISO8859-13) \
	FontObj(lubB24-ISO8859-13) \
	FontObj(lubBI08-ISO8859-13) FontObj(lubBI10-ISO8859-13) \
	FontObj(lubBI12-ISO8859-13) FontObj(lubBI14-ISO8859-13) \
	FontObj(lubBI18-ISO8859-13) FontObj(lubBI19-ISO8859-13) \
	FontObj(lubBI24-ISO8859-13) \
	FontObj(lubI08-ISO8859-13) FontObj(lubI10-ISO8859-13) \
	FontObj(lubI12-ISO8859-13) FontObj(lubI14-ISO8859-13) \
	FontObj(lubI18-ISO8859-13) FontObj(lubI19-ISO8859-13) \
	FontObj(lubI24-ISO8859-13) \
	FontObj(lubR08-ISO8859-13) FontObj(lubR10-ISO8859-13) \
	FontObj(lubR12-ISO8859-13) FontObj(lubR14-ISO8859-13) \
	FontObj(lubR18-ISO8859-13) FontObj(lubR19-ISO8859-13) \
	FontObj(lubR24-ISO8859-13) \
	FontObj(lutBS08-ISO8859-13) FontObj(lutBS10-ISO8859-13) \
	FontObj(lutBS12-ISO8859-13) FontObj(lutBS14-ISO8859-13) \
	FontObj(lutBS18-ISO8859-13) FontObj(lutBS19-ISO8859-13) \
	FontObj(lutBS24-ISO8859-13) \
	FontObj(lutRS08-ISO8859-13) FontObj(lutRS10-ISO8859-13) \
	FontObj(lutRS12-ISO8859-13) FontObj(lutRS14-ISO8859-13) \
	FontObj(lutRS18-ISO8859-13) FontObj(lutRS19-ISO8859-13) \
	FontObj(lutRS24-ISO8859-13) \
	FontObj(UTBI__10-ISO8859-13) FontObj(UTBI__12-ISO8859-13) \
	FontObj(UTBI__14-ISO8859-13) FontObj(UTBI__18-ISO8859-13) \
	FontObj(UTBI__24-ISO8859-13) \
	FontObj(UTB___10-ISO8859-13) FontObj(UTB___12-ISO8859-13) \
	FontObj(UTB___14-ISO8859-13) FontObj(UTB___18-ISO8859-13) \
	FontObj(UTB___24-ISO8859-13) \
	FontObj(UTI___10-ISO8859-13) FontObj(UTI___12-ISO8859-13) \
	FontObj(UTI___14-ISO8859-13) FontObj(UTI___18-ISO8859-13) \
	FontObj(UTI___24-ISO8859-13) \
	FontObj(UTRG__10-ISO8859-13) FontObj(UTRG__12-ISO8859-13) \
	FontObj(UTRG__14-ISO8859-13) FontObj(UTRG__18-ISO8859-13) \
	FontObj(UTRG__24-ISO8859-13)

DERIVED_8859_13_OBJS = $(DERIVED_8859_13_OBJS1) (DERIVED_8859_13_OBJS2)
#endif

#if BuildISO8859_14Fonts
DERIVED_8859_14_OBJS1 = \
	FontObj(courB08-ISO8859-14) FontObj(courB10-ISO8859-14) \
	FontObj(courB12-ISO8859-14) FontObj(courB14-ISO8859-14) \
	FontObj(courB18-ISO8859-14) FontObj(courB24-ISO8859-14) \
	FontObj(courBO08-ISO8859-14) FontObj(courBO10-ISO8859-14) \
	FontObj(courBO12-ISO8859-14) FontObj(courBO14-ISO8859-14) \
	FontObj(courBO18-ISO8859-14) FontObj(courBO24-ISO8859-14) \
	FontObj(courO08-ISO8859-14) FontObj(courO10-ISO8859-14) \
	FontObj(courO12-ISO8859-14) FontObj(courO14-ISO8859-14) \
	FontObj(courO18-ISO8859-14) FontObj(courO24-ISO8859-14) \
	FontObj(courR08-ISO8859-14) FontObj(courR10-ISO8859-14) \
	FontObj(courR12-ISO8859-14) FontObj(courR14-ISO8859-14) \
	FontObj(courR18-ISO8859-14) FontObj(courR24-ISO8859-14) \
	FontObj(helvB08-ISO8859-14) FontObj(helvB10-ISO8859-14) \
	FontObj(helvB12-ISO8859-14) FontObj(helvB14-ISO8859-14) \
	FontObj(helvB18-ISO8859-14) FontObj(helvB24-ISO8859-14) \
	FontObj(helvBO08-ISO8859-14) FontObj(helvBO10-ISO8859-14) \
	FontObj(helvBO12-ISO8859-14) FontObj(helvBO14-ISO8859-14) \
	FontObj(helvBO18-ISO8859-14) FontObj(helvBO24-ISO8859-14) \
	FontObj(helvO08-ISO8859-14) FontObj(helvO10-ISO8859-14) \
	FontObj(helvO12-ISO8859-14) FontObj(helvO14-ISO8859-14) \
	FontObj(helvO18-ISO8859-14) FontObj(helvO24-ISO8859-14) \
	FontObj(helvR08-ISO8859-14) FontObj(helvR10-ISO8859-14) \
	FontObj(helvR12-ISO8859-14) FontObj(helvR14-ISO8859-14) \
	FontObj(helvR18-ISO8859-14) FontObj(helvR24-ISO8859-14) \
	FontObj(ncenB08-ISO8859-14) FontObj(ncenB10-ISO8859-14) \
	FontObj(ncenB12-ISO8859-14) FontObj(ncenB14-ISO8859-14) \
	FontObj(ncenB18-ISO8859-14) FontObj(ncenB24-ISO8859-14) \
	FontObj(ncenBI08-ISO8859-14) FontObj(ncenBI10-ISO8859-14) \
	FontObj(ncenBI12-ISO8859-14) FontObj(ncenBI14-ISO8859-14) \
	FontObj(ncenBI18-ISO8859-14) FontObj(ncenBI24-ISO8859-14) \
	FontObj(ncenI08-ISO8859-14) FontObj(ncenI10-ISO8859-14) \
	FontObj(ncenI12-ISO8859-14) FontObj(ncenI14-ISO8859-14) \
	FontObj(ncenI18-ISO8859-14) FontObj(ncenI24-ISO8859-14) \
	FontObj(ncenR08-ISO8859-14) FontObj(ncenR10-ISO8859-14) \
	FontObj(ncenR12-ISO8859-14) FontObj(ncenR14-ISO8859-14) \
	FontObj(ncenR18-ISO8859-14) FontObj(ncenR24-ISO8859-14) \
	FontObj(timB08-ISO8859-14) FontObj(timB10-ISO8859-14) \
	FontObj(timB12-ISO8859-14) FontObj(timB14-ISO8859-14) \
	FontObj(timB18-ISO8859-14) FontObj(timB24-ISO8859-14) \
	FontObj(timBI08-ISO8859-14) FontObj(timBI10-ISO8859-14) \
	FontObj(timBI12-ISO8859-14) FontObj(timBI14-ISO8859-14) \
	FontObj(timBI18-ISO8859-14) FontObj(timBI24-ISO8859-14) \
	FontObj(timI08-ISO8859-14) FontObj(timI10-ISO8859-14) \
	FontObj(timI12-ISO8859-14) FontObj(timI14-ISO8859-14) \
	FontObj(timI18-ISO8859-14) FontObj(timI24-ISO8859-14) \
	FontObj(timR08-ISO8859-14) FontObj(timR10-ISO8859-14) \
	FontObj(timR12-ISO8859-14) FontObj(timR14-ISO8859-14) \
	FontObj(timR18-ISO8859-14) FontObj(timR24-ISO8859-14)

DERIVED_8859_14_OBJS2 = \
	FontObj(luBIS08-ISO8859-14) FontObj(luBIS10-ISO8859-14) \
	FontObj(luBIS12-ISO8859-14) FontObj(luBIS14-ISO8859-14) \
	FontObj(luBIS18-ISO8859-14) FontObj(luBIS19-ISO8859-14) \
	FontObj(luBIS24-ISO8859-14) \
	FontObj(luBS08-ISO8859-14) FontObj(luBS10-ISO8859-14) \
	FontObj(luBS12-ISO8859-14) FontObj(luBS14-ISO8859-14) \
	FontObj(luBS18-ISO8859-14) FontObj(luBS19-ISO8859-14) \
	FontObj(luBS24-ISO8859-14) \
	FontObj(luIS08-ISO8859-14) FontObj(luIS10-ISO8859-14) \
	FontObj(luIS12-ISO8859-14) FontObj(luIS14-ISO8859-14) \
	FontObj(luIS18-ISO8859-14) FontObj(luIS19-ISO8859-14) \
	FontObj(luIS24-ISO8859-14) \
	FontObj(luRS08-ISO8859-14) FontObj(luRS10-ISO8859-14) \
	FontObj(luRS12-ISO8859-14) FontObj(luRS14-ISO8859-14) \
	FontObj(luRS18-ISO8859-14) FontObj(luRS19-ISO8859-14) \
	FontObj(luRS24-ISO8859-14) \
	FontObj(lubB08-ISO8859-14) FontObj(lubB10-ISO8859-14) \
	FontObj(lubB12-ISO8859-14) FontObj(lubB14-ISO8859-14) \
	FontObj(lubB18-ISO8859-14) FontObj(lubR19-ISO8859-14) \
	FontObj(lubB24-ISO8859-14) \
	FontObj(lubBI08-ISO8859-14) FontObj(lubBI10-ISO8859-14) \
	FontObj(lubBI12-ISO8859-14) FontObj(lubBI14-ISO8859-14) \
	FontObj(lubBI18-ISO8859-14) FontObj(lubBI19-ISO8859-14) \
	FontObj(lubBI24-ISO8859-14) \
	FontObj(lubI08-ISO8859-14) FontObj(lubI10-ISO8859-14) \
	FontObj(lubI12-ISO8859-14) FontObj(lubI14-ISO8859-14) \
	FontObj(lubI18-ISO8859-14) FontObj(lubI19-ISO8859-14) \
	FontObj(lubI24-ISO8859-14) \
	FontObj(lubR08-ISO8859-14) FontObj(lubR10-ISO8859-14) \
	FontObj(lubR12-ISO8859-14) FontObj(lubR14-ISO8859-14) \
	FontObj(lubR18-ISO8859-14) FontObj(lubR19-ISO8859-14) \
	FontObj(lubR24-ISO8859-14) \
	FontObj(lutBS08-ISO8859-14) FontObj(lutBS10-ISO8859-14) \
	FontObj(lutBS12-ISO8859-14) FontObj(lutBS14-ISO8859-14) \
	FontObj(lutBS18-ISO8859-14) FontObj(lutBS19-ISO8859-14) \
	FontObj(lutBS24-ISO8859-14) \
	FontObj(lutRS08-ISO8859-14) FontObj(lutRS10-ISO8859-14) \
	FontObj(lutRS12-ISO8859-14) FontObj(lutRS14-ISO8859-14) \
	FontObj(lutRS18-ISO8859-14) FontObj(lutRS19-ISO8859-14) \
	FontObj(lutRS24-ISO8859-14) \
	FontObj(UTBI__10-ISO8859-14) FontObj(UTBI__12-ISO8859-14) \
	FontObj(UTBI__14-ISO8859-14) FontObj(UTBI__18-ISO8859-14) \
	FontObj(UTBI__24-ISO8859-14) \
	FontObj(UTB___10-ISO8859-14) FontObj(UTB___12-ISO8859-14) \
	FontObj(UTB___14-ISO8859-14) FontObj(UTB___18-ISO8859-14) \
	FontObj(UTB___24-ISO8859-14) \
	FontObj(UTI___10-ISO8859-14) FontObj(UTI___12-ISO8859-14) \
	FontObj(UTI___14-ISO8859-14) FontObj(UTI___18-ISO8859-14) \
	FontObj(UTI___24-ISO8859-14) \
	FontObj(UTRG__10-ISO8859-14) FontObj(UTRG__12-ISO8859-14) \
	FontObj(UTRG__14-ISO8859-14) FontObj(UTRG__18-ISO8859-14) \
	FontObj(UTRG__24-ISO8859-14)

DERIVED_8859_14_OBJS = $(DERIVED_8859_14_OBJS1) (DERIVED_8859_14_OBJS2)
#endif

#if BuildISO8859_15Fonts
DERIVED_8859_15_OBJS1 = \
	FontObj(courB08-ISO8859-15) FontObj(courB10-ISO8859-15) \
	FontObj(courB12-ISO8859-15) FontObj(courB14-ISO8859-15) \
	FontObj(courB18-ISO8859-15) FontObj(courB24-ISO8859-15) \
	FontObj(courBO08-ISO8859-15) FontObj(courBO10-ISO8859-15) \
	FontObj(courBO12-ISO8859-15) FontObj(courBO14-ISO8859-15) \
	FontObj(courBO18-ISO8859-15) FontObj(courBO24-ISO8859-15) \
	FontObj(courO08-ISO8859-15) FontObj(courO10-ISO8859-15) \
	FontObj(courO12-ISO8859-15) FontObj(courO14-ISO8859-15) \
	FontObj(courO18-ISO8859-15) FontObj(courO24-ISO8859-15) \
	FontObj(courR08-ISO8859-15) FontObj(courR10-ISO8859-15) \
	FontObj(courR12-ISO8859-15) FontObj(courR14-ISO8859-15) \
	FontObj(courR18-ISO8859-15) FontObj(courR24-ISO8859-15) \
	FontObj(helvB08-ISO8859-15) FontObj(helvB10-ISO8859-15) \
	FontObj(helvB12-ISO8859-15) FontObj(helvB14-ISO8859-15) \
	FontObj(helvB18-ISO8859-15) FontObj(helvB24-ISO8859-15) \
	FontObj(helvBO08-ISO8859-15) FontObj(helvBO10-ISO8859-15) \
	FontObj(helvBO12-ISO8859-15) FontObj(helvBO14-ISO8859-15) \
	FontObj(helvBO18-ISO8859-15) FontObj(helvBO24-ISO8859-15) \
	FontObj(helvO08-ISO8859-15) FontObj(helvO10-ISO8859-15) \
	FontObj(helvO12-ISO8859-15) FontObj(helvO14-ISO8859-15) \
	FontObj(helvO18-ISO8859-15) FontObj(helvO24-ISO8859-15) \
	FontObj(helvR08-ISO8859-15) FontObj(helvR10-ISO8859-15) \
	FontObj(helvR12-ISO8859-15) FontObj(helvR14-ISO8859-15) \
	FontObj(helvR18-ISO8859-15) FontObj(helvR24-ISO8859-15) \
	FontObj(ncenB08-ISO8859-15) FontObj(ncenB10-ISO8859-15) \
	FontObj(ncenB12-ISO8859-15) FontObj(ncenB14-ISO8859-15) \
	FontObj(ncenB18-ISO8859-15) FontObj(ncenB24-ISO8859-15) \
	FontObj(ncenBI08-ISO8859-15) FontObj(ncenBI10-ISO8859-15) \
	FontObj(ncenBI12-ISO8859-15) FontObj(ncenBI14-ISO8859-15) \
	FontObj(ncenBI18-ISO8859-15) FontObj(ncenBI24-ISO8859-15) \
	FontObj(ncenI08-ISO8859-15) FontObj(ncenI10-ISO8859-15) \
	FontObj(ncenI12-ISO8859-15) FontObj(ncenI14-ISO8859-15) \
	FontObj(ncenI18-ISO8859-15) FontObj(ncenI24-ISO8859-15) \
	FontObj(ncenR08-ISO8859-15) FontObj(ncenR10-ISO8859-15) \
	FontObj(ncenR12-ISO8859-15) FontObj(ncenR14-ISO8859-15) \
	FontObj(ncenR18-ISO8859-15) FontObj(ncenR24-ISO8859-15) \
	FontObj(timB08-ISO8859-15) FontObj(timB10-ISO8859-15) \
	FontObj(timB12-ISO8859-15) FontObj(timB14-ISO8859-15) \
	FontObj(timB18-ISO8859-15) FontObj(timB24-ISO8859-15) \
	FontObj(timBI08-ISO8859-15) FontObj(timBI10-ISO8859-15) \
	FontObj(timBI12-ISO8859-15) FontObj(timBI14-ISO8859-15) \
	FontObj(timBI18-ISO8859-15) FontObj(timBI24-ISO8859-15) \
	FontObj(timI08-ISO8859-15) FontObj(timI10-ISO8859-15) \
	FontObj(timI12-ISO8859-15) FontObj(timI14-ISO8859-15) \
	FontObj(timI18-ISO8859-15) FontObj(timI24-ISO8859-15) \
	FontObj(timR08-ISO8859-15) FontObj(timR10-ISO8859-15) \
	FontObj(timR12-ISO8859-15) FontObj(timR14-ISO8859-15) \
	FontObj(timR18-ISO8859-15) FontObj(timR24-ISO8859-15)

DERIVED_8859_15_OBJS2 = \
	FontObj(luBIS08-ISO8859-15) FontObj(luBIS10-ISO8859-15) \
	FontObj(luBIS12-ISO8859-15) FontObj(luBIS14-ISO8859-15) \
	FontObj(luBIS18-ISO8859-15) FontObj(luBIS19-ISO8859-15) \
	FontObj(luBIS24-ISO8859-15) \
	FontObj(luBS08-ISO8859-15) FontObj(luBS10-ISO8859-15) \
	FontObj(luBS12-ISO8859-15) FontObj(luBS14-ISO8859-15) \
	FontObj(luBS18-ISO8859-15) FontObj(luBS19-ISO8859-15) \
	FontObj(luBS24-ISO8859-15) \
	FontObj(luIS08-ISO8859-15) FontObj(luIS10-ISO8859-15) \
	FontObj(luIS12-ISO8859-15) FontObj(luIS14-ISO8859-15) \
	FontObj(luIS18-ISO8859-15) FontObj(luIS19-ISO8859-15) \
	FontObj(luIS24-ISO8859-15) \
	FontObj(luRS08-ISO8859-15) FontObj(luRS10-ISO8859-15) \
	FontObj(luRS12-ISO8859-15) FontObj(luRS14-ISO8859-15) \
	FontObj(luRS18-ISO8859-15) FontObj(luRS19-ISO8859-15) \
	FontObj(luRS24-ISO8859-15) \
	FontObj(lubB08-ISO8859-15) FontObj(lubB10-ISO8859-15) \
	FontObj(lubB12-ISO8859-15) FontObj(lubB14-ISO8859-15) \
	FontObj(lubB18-ISO8859-15) FontObj(lubR19-ISO8859-15) \
	FontObj(lubB24-ISO8859-15) \
	FontObj(lubBI08-ISO8859-15) FontObj(lubBI10-ISO8859-15) \
	FontObj(lubBI12-ISO8859-15) FontObj(lubBI14-ISO8859-15) \
	FontObj(lubBI18-ISO8859-15) FontObj(lubBI19-ISO8859-15) \
	FontObj(lubBI24-ISO8859-15) \
	FontObj(lubI08-ISO8859-15) FontObj(lubI10-ISO8859-15) \
	FontObj(lubI12-ISO8859-15) FontObj(lubI14-ISO8859-15) \
	FontObj(lubI18-ISO8859-15) FontObj(lubI19-ISO8859-15) \
	FontObj(lubI24-ISO8859-15) \
	FontObj(lubR08-ISO8859-15) FontObj(lubR10-ISO8859-15) \
	FontObj(lubR12-ISO8859-15) FontObj(lubR14-ISO8859-15) \
	FontObj(lubR18-ISO8859-15) FontObj(lubR19-ISO8859-15) \
	FontObj(lubR24-ISO8859-15) \
	FontObj(lutBS08-ISO8859-15) FontObj(lutBS10-ISO8859-15) \
	FontObj(lutBS12-ISO8859-15) FontObj(lutBS14-ISO8859-15) \
	FontObj(lutBS18-ISO8859-15) FontObj(lutBS19-ISO8859-15) \
	FontObj(lutBS24-ISO8859-15) \
	FontObj(lutRS08-ISO8859-15) FontObj(lutRS10-ISO8859-15) \
	FontObj(lutRS12-ISO8859-15) FontObj(lutRS14-ISO8859-15) \
	FontObj(lutRS18-ISO8859-15) FontObj(lutRS19-ISO8859-15) \
	FontObj(lutRS24-ISO8859-15) \
	FontObj(UTBI__10-ISO8859-15) FontObj(UTBI__12-ISO8859-15) \
	FontObj(UTBI__14-ISO8859-15) FontObj(UTBI__18-ISO8859-15) \
	FontObj(UTBI__24-ISO8859-15) \
	FontObj(UTB___10-ISO8859-15) FontObj(UTB___12-ISO8859-15) \
	FontObj(UTB___14-ISO8859-15) FontObj(UTB___18-ISO8859-15) \
	FontObj(UTB___24-ISO8859-15) \
	FontObj(UTI___10-ISO8859-15) FontObj(UTI___12-ISO8859-15) \
	FontObj(UTI___14-ISO8859-15) FontObj(UTI___18-ISO8859-15) \
	FontObj(UTI___24-ISO8859-15) \
	FontObj(UTRG__10-ISO8859-15) FontObj(UTRG__12-ISO8859-15) \
	FontObj(UTRG__14-ISO8859-15) FontObj(UTRG__18-ISO8859-15) \
	FontObj(UTRG__24-ISO8859-15)

DERIVED_8859_15_OBJS = $(DERIVED_8859_15_OBJS1) (DERIVED_8859_15_OBJS2)
#endif

DERIVEDOBJS1 = \
	$(DERIVED_8859_1_OBJS1) $(DERIVED_8859_2_OBJS1) \
	$(DERIVED_8859_3_OBJS1) $(DERIVED_8859_4_OBJS1) \
	$(DERIVED_8859_9_OBJS1) $(DERIVED_8859_10_OBJS1) \
	$(DERIVED_8859_13_OBJS1) $(DERIVED_8859_14_OBJS1) \
	$(DERIVED_8859_15_OBJS1)

DERIVEDOBJS2 = \
	$(DERIVED_8859_1_OBJS2) $(DERIVED_8859_2_OBJS2) \
	$(DERIVED_8859_3_OBJS2) $(DERIVED_8859_4_OBJS2) \
	$(DERIVED_8859_9_OBJS2) $(DERIVED_8859_10_OBJS2) \
	$(DERIVED_8859_13_OBJS2) $(DERIVED_8859_14_OBJS2) \
	$(DERIVED_8859_15_OBJS2)

DERIVEDOBJS = $(DERIVEDOBJS1) $(DERIVEDOBJS2)

#else  /* DerivedUCSFonts */
/*
 * PREDERIVEDOBJS is the list of fonts referenced by fonts.alias that would
 * normally be derived from the UCS versions but are not because perl
 * isn't available for the conversion script.  This is hopefully a temporary
 * issue.
 */

PREDERIVEDOBJS1 = $(DERIVED_8859_1_OBJS1)
PREDERIVEDOBJS2 = $(DERIVED_8859_1_OBJS2)
PREDERIVEDOBJS = $(PREDERIVEDOBJS1) $(PREDERIVEDOBJS2)

#endif /* DerivedUCSFonts */

/* Ignore .bdf files when running mkfontdir */
MKFONTDIROPTS = -x bdf

/*
 * There's currently no need to derive truncated UCS subsets for these fonts
 * because they already have glyphs at high encoding values disabled.
 */

#if BuildUCSFonts
FontTarget(courB08)
FontTarget(courB10)
FontTarget(courB12)
FontTarget(courB14)
FontTarget(courB18)
FontTarget(courB24)
FontTarget(courBO08)
FontTarget(courBO10)
FontTarget(courBO12)
FontTarget(courBO14)
FontTarget(courBO18)
FontTarget(courBO24)
FontTarget(courO08)
FontTarget(courO10)
FontTarget(courO12)
FontTarget(courO14)
FontTarget(courO18)
FontTarget(courO24)
FontTarget(courR08)
FontTarget(courR10)
FontTarget(courR12)
FontTarget(courR14)
FontTarget(courR18)
FontTarget(courR24)
FontTarget(helvB08)
FontTarget(helvB10)
FontTarget(helvB12)
FontTarget(helvB14)
FontTarget(helvB18)
FontTarget(helvB24)
FontTarget(helvBO08)
FontTarget(helvBO10)
FontTarget(helvBO12)
FontTarget(helvBO14)
FontTarget(helvBO18)
FontTarget(helvBO24)
FontTarget(helvO08)
FontTarget(helvO10)
FontTarget(helvO12)
FontTarget(helvO14)
FontTarget(helvO18)
FontTarget(helvO24)
FontTarget(helvR08)
FontTarget(helvR10)
FontTarget(helvR12)
FontTarget(helvR14)
FontTarget(helvR18)
FontTarget(helvR24)
FontTarget(ncenB08)
FontTarget(ncenB10)
FontTarget(ncenB12)
FontTarget(ncenB14)
FontTarget(ncenB18)
FontTarget(ncenB24)
FontTarget(ncenBI08)
FontTarget(ncenBI10)
FontTarget(ncenBI12)
FontTarget(ncenBI14)
FontTarget(ncenBI18)
FontTarget(ncenBI24)
FontTarget(ncenI08)
FontTarget(ncenI10)
FontTarget(ncenI12)
FontTarget(ncenI14)
FontTarget(ncenI18)
FontTarget(ncenI24)
FontTarget(ncenR08)
FontTarget(ncenR10)
FontTarget(ncenR12)
FontTarget(ncenR14)
FontTarget(ncenR18)
FontTarget(ncenR24)
FontTarget(timB08)
FontTarget(timB10)
FontTarget(timB12)
FontTarget(timB14)
FontTarget(timB18)
FontTarget(timB24)
FontTarget(timBI08)
FontTarget(timBI10)
FontTarget(timBI12)
FontTarget(timBI14)
FontTarget(timBI18)
FontTarget(timBI24)
FontTarget(timI08)
FontTarget(timI10)
FontTarget(timI12)
FontTarget(timI14)
FontTarget(timI18)
FontTarget(timI24)
FontTarget(timR08)
FontTarget(timR10)
FontTarget(timR12)
FontTarget(timR14)
FontTarget(timR18)
FontTarget(timR24)
FontTarget(luBIS08)
FontTarget(luBIS10)
FontTarget(luBIS12)
FontTarget(luBIS14)
FontTarget(luBIS18)
FontTarget(luBIS19)
FontTarget(luBIS24)
FontTarget(luBS08)
FontTarget(luBS10)
FontTarget(luBS12)
FontTarget(luBS14)
FontTarget(luBS18)
FontTarget(luBS19)
FontTarget(luBS24)
FontTarget(luIS08)
FontTarget(luIS10)
FontTarget(luIS12)
FontTarget(luIS14)
FontTarget(luIS18)
FontTarget(luIS19)
FontTarget(luIS24)
FontTarget(luRS08)
FontTarget(luRS10)
FontTarget(luRS12)
FontTarget(luRS14)
FontTarget(luRS18)
FontTarget(luRS19)
FontTarget(luRS24)
FontTarget(lubB08)
FontTarget(lubB10)
FontTarget(lubB12)
FontTarget(lubB14)
FontTarget(lubB18)
FontTarget(lubB19)
FontTarget(lubB24)
FontTarget(lubBI08)
FontTarget(lubBI10)
FontTarget(lubBI12)
FontTarget(lubBI14)
FontTarget(lubBI18)
FontTarget(lubBI19)
FontTarget(lubBI24)
FontTarget(lubI08)
FontTarget(lubI10)
FontTarget(lubI12)
FontTarget(lubI14)
FontTarget(lubI18)
FontTarget(lubI19)
FontTarget(lubI24)
FontTarget(lubR08)
FontTarget(lubR10)
FontTarget(lubR12)
FontTarget(lubR14)
FontTarget(lubR18)
FontTarget(lubR19)
FontTarget(lubR24)
FontTarget(lutBS08)
FontTarget(lutBS10)
FontTarget(lutBS12)
FontTarget(lutBS14)
FontTarget(lutBS18)
FontTarget(lutBS19)
FontTarget(lutBS24)
FontTarget(lutRS08)
FontTarget(lutRS10)
FontTarget(lutRS12)
FontTarget(lutRS14)
FontTarget(lutRS18)
FontTarget(lutRS19)
FontTarget(lutRS24)
FontTarget(UTBI__10)
FontTarget(UTBI__12)
FontTarget(UTBI__14)
FontTarget(UTBI__18)
FontTarget(UTBI__24)
FontTarget(UTB___10)
FontTarget(UTB___12)
FontTarget(UTB___14)
FontTarget(UTB___18)
FontTarget(UTB___24)
FontTarget(UTI___10)
FontTarget(UTI___12)
FontTarget(UTI___14)
FontTarget(UTI___18)
FontTarget(UTI___24)
FontTarget(UTRG__10)
FontTarget(UTRG__12)
FontTarget(UTRG__14)
FontTarget(UTRG__18)
FontTarget(UTRG__24)
#endif

FontTarget(symb08)
FontTarget(symb10)
FontTarget(symb12)
FontTarget(symb14)
FontTarget(symb18)
FontTarget(symb24)
FontTarget(charB08)
FontTarget(charB10)
FontTarget(charB12)
FontTarget(charB14)
FontTarget(charB18)
FontTarget(charB24)
FontTarget(charBI08)
FontTarget(charBI10)
FontTarget(charBI12)
FontTarget(charBI14)
FontTarget(charBI18)
FontTarget(charBI24)
FontTarget(charI08)
FontTarget(charI10)
FontTarget(charI12)
FontTarget(charI14)
FontTarget(charI18)
FontTarget(charI24)
FontTarget(charR08)
FontTarget(charR10)
FontTarget(charR12)
FontTarget(charR14)
FontTarget(charR18)
FontTarget(charR24)
FontTarget(tech14)
FontTarget(techB14)
FontTarget(term14)
FontTarget(termB14)

#if BuildISO8859_1Fonts
/*
 * Latin1 versions are always available, either derived at build time or
 * pre-derived for systems without perl.
 */
FontTarget(courB08-ISO8859-1)
FontTarget(courB10-ISO8859-1)
FontTarget(courB12-ISO8859-1)
FontTarget(courB14-ISO8859-1)
FontTarget(courB18-ISO8859-1)
FontTarget(courB24-ISO8859-1)
FontTarget(courBO08-ISO8859-1)
FontTarget(courBO10-ISO8859-1)
FontTarget(courBO12-ISO8859-1)
FontTarget(courBO14-ISO8859-1)
FontTarget(courBO18-ISO8859-1)
FontTarget(courBO24-ISO8859-1)
FontTarget(courO08-ISO8859-1)
FontTarget(courO10-ISO8859-1)
FontTarget(courO12-ISO8859-1)
FontTarget(courO14-ISO8859-1)
FontTarget(courO18-ISO8859-1)
FontTarget(courO24-ISO8859-1)
FontTarget(courR08-ISO8859-1)
FontTarget(courR10-ISO8859-1)
FontTarget(courR12-ISO8859-1)
FontTarget(courR14-ISO8859-1)
FontTarget(courR18-ISO8859-1)
FontTarget(courR24-ISO8859-1)
FontTarget(helvB08-ISO8859-1)
FontTarget(helvB10-ISO8859-1)
FontTarget(helvB12-ISO8859-1)
FontTarget(helvB14-ISO8859-1)
FontTarget(helvB18-ISO8859-1)
FontTarget(helvB24-ISO8859-1)
FontTarget(helvBO08-ISO8859-1)
FontTarget(helvBO10-ISO8859-1)
FontTarget(helvBO12-ISO8859-1)
FontTarget(helvBO14-ISO8859-1)
FontTarget(helvBO18-ISO8859-1)
FontTarget(helvBO24-ISO8859-1)
FontTarget(helvO08-ISO8859-1)
FontTarget(helvO10-ISO8859-1)
FontTarget(helvO12-ISO8859-1)
FontTarget(helvO14-ISO8859-1)
FontTarget(helvO18-ISO8859-1)
FontTarget(helvO24-ISO8859-1)
FontTarget(helvR08-ISO8859-1)
FontTarget(helvR10-ISO8859-1)
FontTarget(helvR12-ISO8859-1)
FontTarget(helvR14-ISO8859-1)
FontTarget(helvR18-ISO8859-1)
FontTarget(helvR24-ISO8859-1)
FontTarget(ncenB08-ISO8859-1)
FontTarget(ncenB10-ISO8859-1)
FontTarget(ncenB12-ISO8859-1)
FontTarget(ncenB14-ISO8859-1)
FontTarget(ncenB18-ISO8859-1)
FontTarget(ncenB24-ISO8859-1)
FontTarget(ncenBI08-ISO8859-1)
FontTarget(ncenBI10-ISO8859-1)
FontTarget(ncenBI12-ISO8859-1)
FontTarget(ncenBI14-ISO8859-1)
FontTarget(ncenBI18-ISO8859-1)
FontTarget(ncenBI24-ISO8859-1)
FontTarget(ncenI08-ISO8859-1)
FontTarget(ncenI10-ISO8859-1)
FontTarget(ncenI12-ISO8859-1)
FontTarget(ncenI14-ISO8859-1)
FontTarget(ncenI18-ISO8859-1)
FontTarget(ncenI24-ISO8859-1)
FontTarget(ncenR08-ISO8859-1)
FontTarget(ncenR10-ISO8859-1)
FontTarget(ncenR12-ISO8859-1)
FontTarget(ncenR14-ISO8859-1)
FontTarget(ncenR18-ISO8859-1)
FontTarget(ncenR24-ISO8859-1)
FontTarget(timB08-ISO8859-1)
FontTarget(timB10-ISO8859-1)
FontTarget(timB12-ISO8859-1)
FontTarget(timB14-ISO8859-1)
FontTarget(timB18-ISO8859-1)
FontTarget(timB24-ISO8859-1)
FontTarget(timBI08-ISO8859-1)
FontTarget(timBI10-ISO8859-1)
FontTarget(timBI12-ISO8859-1)
FontTarget(timBI14-ISO8859-1)
FontTarget(timBI18-ISO8859-1)
FontTarget(timBI24-ISO8859-1)
FontTarget(timI08-ISO8859-1)
FontTarget(timI10-ISO8859-1)
FontTarget(timI12-ISO8859-1)
FontTarget(timI14-ISO8859-1)
FontTarget(timI18-ISO8859-1)
FontTarget(timI24-ISO8859-1)
FontTarget(timR08-ISO8859-1)
FontTarget(timR10-ISO8859-1)
FontTarget(timR12-ISO8859-1)
FontTarget(timR14-ISO8859-1)
FontTarget(timR18-ISO8859-1)
FontTarget(timR24-ISO8859-1)
FontTarget(luBIS08-ISO8859-1)
FontTarget(luBIS10-ISO8859-1)
FontTarget(luBIS12-ISO8859-1)
FontTarget(luBIS14-ISO8859-1)
FontTarget(luBIS18-ISO8859-1)
FontTarget(luBIS19-ISO8859-1)
FontTarget(luBIS24-ISO8859-1)
FontTarget(luBS08-ISO8859-1)
FontTarget(luBS10-ISO8859-1)
FontTarget(luBS12-ISO8859-1)
FontTarget(luBS14-ISO8859-1)
FontTarget(luBS18-ISO8859-1)
FontTarget(luBS19-ISO8859-1)
FontTarget(luBS24-ISO8859-1)
FontTarget(luIS08-ISO8859-1)
FontTarget(luIS10-ISO8859-1)
FontTarget(luIS12-ISO8859-1)
FontTarget(luIS14-ISO8859-1)
FontTarget(luIS18-ISO8859-1)
FontTarget(luIS19-ISO8859-1)
FontTarget(luIS24-ISO8859-1)
FontTarget(luRS08-ISO8859-1)
FontTarget(luRS10-ISO8859-1)
FontTarget(luRS12-ISO8859-1)
FontTarget(luRS14-ISO8859-1)
FontTarget(luRS18-ISO8859-1)
FontTarget(luRS19-ISO8859-1)
FontTarget(luRS24-ISO8859-1)
FontTarget(lubB08-ISO8859-1)
FontTarget(lubB10-ISO8859-1)
FontTarget(lubB12-ISO8859-1)
FontTarget(lubB14-ISO8859-1)
FontTarget(lubB18-ISO8859-1)
FontTarget(lubB19-ISO8859-1)
FontTarget(lubB24-ISO8859-1)
FontTarget(lubBI08-ISO8859-1)
FontTarget(lubBI10-ISO8859-1)
FontTarget(lubBI12-ISO8859-1)
FontTarget(lubBI14-ISO8859-1)
FontTarget(lubBI18-ISO8859-1)
FontTarget(lubBI19-ISO8859-1)
FontTarget(lubBI24-ISO8859-1)
FontTarget(lubI08-ISO8859-1)
FontTarget(lubI10-ISO8859-1)
FontTarget(lubI12-ISO8859-1)
FontTarget(lubI14-ISO8859-1)
FontTarget(lubI18-ISO8859-1)
FontTarget(lubI19-ISO8859-1)
FontTarget(lubI24-ISO8859-1)
FontTarget(lubR08-ISO8859-1)
FontTarget(lubR10-ISO8859-1)
FontTarget(lubR12-ISO8859-1)
FontTarget(lubR14-ISO8859-1)
FontTarget(lubR18-ISO8859-1)
FontTarget(lubR19-ISO8859-1)
FontTarget(lubR24-ISO8859-1)
FontTarget(lutBS08-ISO8859-1)
FontTarget(lutBS10-ISO8859-1)
FontTarget(lutBS12-ISO8859-1)
FontTarget(lutBS14-ISO8859-1)
FontTarget(lutBS18-ISO8859-1)
FontTarget(lutBS19-ISO8859-1)
FontTarget(lutBS24-ISO8859-1)
FontTarget(lutRS08-ISO8859-1)
FontTarget(lutRS10-ISO8859-1)
FontTarget(lutRS12-ISO8859-1)
FontTarget(lutRS14-ISO8859-1)
FontTarget(lutRS18-ISO8859-1)
FontTarget(lutRS19-ISO8859-1)
FontTarget(lutRS24-ISO8859-1)
FontTarget(UTBI__10-ISO8859-1)
FontTarget(UTBI__12-ISO8859-1)
FontTarget(UTBI__14-ISO8859-1)
FontTarget(UTBI__18-ISO8859-1)
FontTarget(UTBI__24-ISO8859-1)
FontTarget(UTB___10-ISO8859-1)
FontTarget(UTB___12-ISO8859-1)
FontTarget(UTB___14-ISO8859-1)
FontTarget(UTB___18-ISO8859-1)
FontTarget(UTB___24-ISO8859-1)
FontTarget(UTI___10-ISO8859-1)
FontTarget(UTI___12-ISO8859-1)
FontTarget(UTI___14-ISO8859-1)
FontTarget(UTI___18-ISO8859-1)
FontTarget(UTI___24-ISO8859-1)
FontTarget(UTRG__10-ISO8859-1)
FontTarget(UTRG__12-ISO8859-1)
FontTarget(UTRG__14-ISO8859-1)
FontTarget(UTRG__18-ISO8859-1)
FontTarget(UTRG__24-ISO8859-1)
#endif

/* 8-bit fonts derived from UCS fonts */
#if DerivedUCSFonts
#if BuildISO8859_2Fonts
FontTarget(courB08-ISO8859-2)
FontTarget(courB10-ISO8859-2)
FontTarget(courB12-ISO8859-2)
FontTarget(courB14-ISO8859-2)
FontTarget(courB18-ISO8859-2)
FontTarget(courB24-ISO8859-2)
FontTarget(courBO08-ISO8859-2)
FontTarget(courBO10-ISO8859-2)
FontTarget(courBO12-ISO8859-2)
FontTarget(courBO14-ISO8859-2)
FontTarget(courBO18-ISO8859-2)
FontTarget(courBO24-ISO8859-2)
FontTarget(courO08-ISO8859-2)
FontTarget(courO10-ISO8859-2)
FontTarget(courO12-ISO8859-2)
FontTarget(courO14-ISO8859-2)
FontTarget(courO18-ISO8859-2)
FontTarget(courO24-ISO8859-2)
FontTarget(courR08-ISO8859-2)
FontTarget(courR10-ISO8859-2)
FontTarget(courR12-ISO8859-2)
FontTarget(courR14-ISO8859-2)
FontTarget(courR18-ISO8859-2)
FontTarget(courR24-ISO8859-2)
FontTarget(helvB08-ISO8859-2)
FontTarget(helvB10-ISO8859-2)
FontTarget(helvB12-ISO8859-2)
FontTarget(helvB14-ISO8859-2)
FontTarget(helvB18-ISO8859-2)
FontTarget(helvB24-ISO8859-2)
FontTarget(helvBO08-ISO8859-2)
FontTarget(helvBO10-ISO8859-2)
FontTarget(helvBO12-ISO8859-2)
FontTarget(helvBO14-ISO8859-2)
FontTarget(helvBO18-ISO8859-2)
FontTarget(helvBO24-ISO8859-2)
FontTarget(helvO08-ISO8859-2)
FontTarget(helvO10-ISO8859-2)
FontTarget(helvO12-ISO8859-2)
FontTarget(helvO14-ISO8859-2)
FontTarget(helvO18-ISO8859-2)
FontTarget(helvO24-ISO8859-2)
FontTarget(helvR08-ISO8859-2)
FontTarget(helvR10-ISO8859-2)
FontTarget(helvR12-ISO8859-2)
FontTarget(helvR14-ISO8859-2)
FontTarget(helvR18-ISO8859-2)
FontTarget(helvR24-ISO8859-2)
FontTarget(ncenB08-ISO8859-2)
FontTarget(ncenB10-ISO8859-2)
FontTarget(ncenB12-ISO8859-2)
FontTarget(ncenB14-ISO8859-2)
FontTarget(ncenB18-ISO8859-2)
FontTarget(ncenB24-ISO8859-2)
FontTarget(ncenBI08-ISO8859-2)
FontTarget(ncenBI10-ISO8859-2)
FontTarget(ncenBI12-ISO8859-2)
FontTarget(ncenBI14-ISO8859-2)
FontTarget(ncenBI18-ISO8859-2)
FontTarget(ncenBI24-ISO8859-2)
FontTarget(ncenI08-ISO8859-2)
FontTarget(ncenI10-ISO8859-2)
FontTarget(ncenI12-ISO8859-2)
FontTarget(ncenI14-ISO8859-2)
FontTarget(ncenI18-ISO8859-2)
FontTarget(ncenI24-ISO8859-2)
FontTarget(ncenR08-ISO8859-2)
FontTarget(ncenR10-ISO8859-2)
FontTarget(ncenR12-ISO8859-2)
FontTarget(ncenR14-ISO8859-2)
FontTarget(ncenR18-ISO8859-2)
FontTarget(ncenR24-ISO8859-2)
FontTarget(timB08-ISO8859-2)
FontTarget(timB10-ISO8859-2)
FontTarget(timB12-ISO8859-2)
FontTarget(timB14-ISO8859-2)
FontTarget(timB18-ISO8859-2)
FontTarget(timB24-ISO8859-2)
FontTarget(timBI08-ISO8859-2)
FontTarget(timBI10-ISO8859-2)
FontTarget(timBI12-ISO8859-2)
FontTarget(timBI14-ISO8859-2)
FontTarget(timBI18-ISO8859-2)
FontTarget(timBI24-ISO8859-2)
FontTarget(timI08-ISO8859-2)
FontTarget(timI10-ISO8859-2)
FontTarget(timI12-ISO8859-2)
FontTarget(timI14-ISO8859-2)
FontTarget(timI18-ISO8859-2)
FontTarget(timI24-ISO8859-2)
FontTarget(timR08-ISO8859-2)
FontTarget(timR10-ISO8859-2)
FontTarget(timR12-ISO8859-2)
FontTarget(timR14-ISO8859-2)
FontTarget(timR18-ISO8859-2)
FontTarget(timR24-ISO8859-2)
FontTarget(luBIS08-ISO8859-2)
FontTarget(luBIS10-ISO8859-2)
FontTarget(luBIS12-ISO8859-2)
FontTarget(luBIS14-ISO8859-2)
FontTarget(luBIS18-ISO8859-2)
FontTarget(luBIS19-ISO8859-2)
FontTarget(luBIS24-ISO8859-2)
FontTarget(luBS08-ISO8859-2)
FontTarget(luBS10-ISO8859-2)
FontTarget(luBS12-ISO8859-2)
FontTarget(luBS14-ISO8859-2)
FontTarget(luBS18-ISO8859-2)
FontTarget(luBS19-ISO8859-2)
FontTarget(luBS24-ISO8859-2)
FontTarget(luIS08-ISO8859-2)
FontTarget(luIS10-ISO8859-2)
FontTarget(luIS12-ISO8859-2)
FontTarget(luIS14-ISO8859-2)
FontTarget(luIS18-ISO8859-2)
FontTarget(luIS19-ISO8859-2)
FontTarget(luIS24-ISO8859-2)
FontTarget(luRS08-ISO8859-2)
FontTarget(luRS10-ISO8859-2)
FontTarget(luRS12-ISO8859-2)
FontTarget(luRS14-ISO8859-2)
FontTarget(luRS18-ISO8859-2)
FontTarget(luRS19-ISO8859-2)
FontTarget(luRS24-ISO8859-2)
FontTarget(lubB08-ISO8859-2)
FontTarget(lubB10-ISO8859-2)
FontTarget(lubB12-ISO8859-2)
FontTarget(lubB14-ISO8859-2)
FontTarget(lubB18-ISO8859-2)
FontTarget(lubB19-ISO8859-2)
FontTarget(lubB24-ISO8859-2)
FontTarget(lubBI08-ISO8859-2)
FontTarget(lubBI10-ISO8859-2)
FontTarget(lubBI12-ISO8859-2)
FontTarget(lubBI14-ISO8859-2)
FontTarget(lubBI18-ISO8859-2)
FontTarget(lubBI19-ISO8859-2)
FontTarget(lubBI24-ISO8859-2)
FontTarget(lubI08-ISO8859-2)
FontTarget(lubI10-ISO8859-2)
FontTarget(lubI12-ISO8859-2)
FontTarget(lubI14-ISO8859-2)
FontTarget(lubI18-ISO8859-2)
FontTarget(lubI19-ISO8859-2)
FontTarget(lubI24-ISO8859-2)
FontTarget(lubR08-ISO8859-2)
FontTarget(lubR10-ISO8859-2)
FontTarget(lubR12-ISO8859-2)
FontTarget(lubR14-ISO8859-2)
FontTarget(lubR18-ISO8859-2)
FontTarget(lubR19-ISO8859-2)
FontTarget(lubR24-ISO8859-2)
FontTarget(lutBS08-ISO8859-2)
FontTarget(lutBS10-ISO8859-2)
FontTarget(lutBS12-ISO8859-2)
FontTarget(lutBS14-ISO8859-2)
FontTarget(lutBS18-ISO8859-2)
FontTarget(lutBS19-ISO8859-2)
FontTarget(lutBS24-ISO8859-2)
FontTarget(lutRS08-ISO8859-2)
FontTarget(lutRS10-ISO8859-2)
FontTarget(lutRS12-ISO8859-2)
FontTarget(lutRS14-ISO8859-2)
FontTarget(lutRS18-ISO8859-2)
FontTarget(lutRS19-ISO8859-2)
FontTarget(lutRS24-ISO8859-2)
FontTarget(UTBI__10-ISO8859-2)
FontTarget(UTBI__12-ISO8859-2)
FontTarget(UTBI__14-ISO8859-2)
FontTarget(UTBI__18-ISO8859-2)
FontTarget(UTBI__24-ISO8859-2)
FontTarget(UTB___10-ISO8859-2)
FontTarget(UTB___12-ISO8859-2)
FontTarget(UTB___14-ISO8859-2)
FontTarget(UTB___18-ISO8859-2)
FontTarget(UTB___24-ISO8859-2)
FontTarget(UTI___10-ISO8859-2)
FontTarget(UTI___12-ISO8859-2)
FontTarget(UTI___14-ISO8859-2)
FontTarget(UTI___18-ISO8859-2)
FontTarget(UTI___24-ISO8859-2)
FontTarget(UTRG__10-ISO8859-2)
FontTarget(UTRG__12-ISO8859-2)
FontTarget(UTRG__14-ISO8859-2)
FontTarget(UTRG__18-ISO8859-2)
FontTarget(UTRG__24-ISO8859-2)
#endif

#if BuildISO8859_3Fonts
FontTarget(courB08-ISO8859-3)
FontTarget(courB10-ISO8859-3)
FontTarget(courB12-ISO8859-3)
FontTarget(courB14-ISO8859-3)
FontTarget(courB18-ISO8859-3)
FontTarget(courB24-ISO8859-3)
FontTarget(courBO08-ISO8859-3)
FontTarget(courBO10-ISO8859-3)
FontTarget(courBO12-ISO8859-3)
FontTarget(courBO14-ISO8859-3)
FontTarget(courBO18-ISO8859-3)
FontTarget(courBO24-ISO8859-3)
FontTarget(courO08-ISO8859-3)
FontTarget(courO10-ISO8859-3)
FontTarget(courO12-ISO8859-3)
FontTarget(courO14-ISO8859-3)
FontTarget(courO18-ISO8859-3)
FontTarget(courO24-ISO8859-3)
FontTarget(courR08-ISO8859-3)
FontTarget(courR10-ISO8859-3)
FontTarget(courR12-ISO8859-3)
FontTarget(courR14-ISO8859-3)
FontTarget(courR18-ISO8859-3)
FontTarget(courR24-ISO8859-3)
FontTarget(helvB08-ISO8859-3)
FontTarget(helvB10-ISO8859-3)
FontTarget(helvB12-ISO8859-3)
FontTarget(helvB14-ISO8859-3)
FontTarget(helvB18-ISO8859-3)
FontTarget(helvB24-ISO8859-3)
FontTarget(helvBO08-ISO8859-3)
FontTarget(helvBO10-ISO8859-3)
FontTarget(helvBO12-ISO8859-3)
FontTarget(helvBO14-ISO8859-3)
FontTarget(helvBO18-ISO8859-3)
FontTarget(helvBO24-ISO8859-3)
FontTarget(helvO08-ISO8859-3)
FontTarget(helvO10-ISO8859-3)
FontTarget(helvO12-ISO8859-3)
FontTarget(helvO14-ISO8859-3)
FontTarget(helvO18-ISO8859-3)
FontTarget(helvO24-ISO8859-3)
FontTarget(helvR08-ISO8859-3)
FontTarget(helvR10-ISO8859-3)
FontTarget(helvR12-ISO8859-3)
FontTarget(helvR14-ISO8859-3)
FontTarget(helvR18-ISO8859-3)
FontTarget(helvR24-ISO8859-3)
FontTarget(ncenB08-ISO8859-3)
FontTarget(ncenB10-ISO8859-3)
FontTarget(ncenB12-ISO8859-3)
FontTarget(ncenB14-ISO8859-3)
FontTarget(ncenB18-ISO8859-3)
FontTarget(ncenB24-ISO8859-3)
FontTarget(ncenBI08-ISO8859-3)
FontTarget(ncenBI10-ISO8859-3)
FontTarget(ncenBI12-ISO8859-3)
FontTarget(ncenBI14-ISO8859-3)
FontTarget(ncenBI18-ISO8859-3)
FontTarget(ncenBI24-ISO8859-3)
FontTarget(ncenI08-ISO8859-3)
FontTarget(ncenI10-ISO8859-3)
FontTarget(ncenI12-ISO8859-3)
FontTarget(ncenI14-ISO8859-3)
FontTarget(ncenI18-ISO8859-3)
FontTarget(ncenI24-ISO8859-3)
FontTarget(ncenR08-ISO8859-3)
FontTarget(ncenR10-ISO8859-3)
FontTarget(ncenR12-ISO8859-3)
FontTarget(ncenR14-ISO8859-3)
FontTarget(ncenR18-ISO8859-3)
FontTarget(ncenR24-ISO8859-3)
FontTarget(timB08-ISO8859-3)
FontTarget(timB10-ISO8859-3)
FontTarget(timB12-ISO8859-3)
FontTarget(timB14-ISO8859-3)
FontTarget(timB18-ISO8859-3)
FontTarget(timB24-ISO8859-3)
FontTarget(timBI08-ISO8859-3)
FontTarget(timBI10-ISO8859-3)
FontTarget(timBI12-ISO8859-3)
FontTarget(timBI14-ISO8859-3)
FontTarget(timBI18-ISO8859-3)
FontTarget(timBI24-ISO8859-3)
FontTarget(timI08-ISO8859-3)
FontTarget(timI10-ISO8859-3)
FontTarget(timI12-ISO8859-3)
FontTarget(timI14-ISO8859-3)
FontTarget(timI18-ISO8859-3)
FontTarget(timI24-ISO8859-3)
FontTarget(timR08-ISO8859-3)
FontTarget(timR10-ISO8859-3)
FontTarget(timR12-ISO8859-3)
FontTarget(timR14-ISO8859-3)
FontTarget(timR18-ISO8859-3)
FontTarget(timR24-ISO8859-3)
FontTarget(luBIS08-ISO8859-3)
FontTarget(luBIS10-ISO8859-3)
FontTarget(luBIS12-ISO8859-3)
FontTarget(luBIS14-ISO8859-3)
FontTarget(luBIS18-ISO8859-3)
FontTarget(luBIS19-ISO8859-3)
FontTarget(luBIS24-ISO8859-3)
FontTarget(luBS08-ISO8859-3)
FontTarget(luBS10-ISO8859-3)
FontTarget(luBS12-ISO8859-3)
FontTarget(luBS14-ISO8859-3)
FontTarget(luBS18-ISO8859-3)
FontTarget(luBS19-ISO8859-3)
FontTarget(luBS24-ISO8859-3)
FontTarget(luIS08-ISO8859-3)
FontTarget(luIS10-ISO8859-3)
FontTarget(luIS12-ISO8859-3)
FontTarget(luIS14-ISO8859-3)
FontTarget(luIS18-ISO8859-3)
FontTarget(luIS19-ISO8859-3)
FontTarget(luIS24-ISO8859-3)
FontTarget(luRS08-ISO8859-3)
FontTarget(luRS10-ISO8859-3)
FontTarget(luRS12-ISO8859-3)
FontTarget(luRS14-ISO8859-3)
FontTarget(luRS18-ISO8859-3)
FontTarget(luRS19-ISO8859-3)
FontTarget(luRS24-ISO8859-3)
FontTarget(lubB08-ISO8859-3)
FontTarget(lubB10-ISO8859-3)
FontTarget(lubB12-ISO8859-3)
FontTarget(lubB14-ISO8859-3)
FontTarget(lubB18-ISO8859-3)
FontTarget(lubB19-ISO8859-3)
FontTarget(lubB24-ISO8859-3)
FontTarget(lubBI08-ISO8859-3)
FontTarget(lubBI10-ISO8859-3)
FontTarget(lubBI12-ISO8859-3)
FontTarget(lubBI14-ISO8859-3)
FontTarget(lubBI18-ISO8859-3)
FontTarget(lubBI19-ISO8859-3)
FontTarget(lubBI24-ISO8859-3)
FontTarget(lubI08-ISO8859-3)
FontTarget(lubI10-ISO8859-3)
FontTarget(lubI12-ISO8859-3)
FontTarget(lubI14-ISO8859-3)
FontTarget(lubI18-ISO8859-3)
FontTarget(lubI19-ISO8859-3)
FontTarget(lubI24-ISO8859-3)
FontTarget(lubR08-ISO8859-3)
FontTarget(lubR10-ISO8859-3)
FontTarget(lubR12-ISO8859-3)
FontTarget(lubR14-ISO8859-3)
FontTarget(lubR18-ISO8859-3)
FontTarget(lubR19-ISO8859-3)
FontTarget(lubR24-ISO8859-3)
FontTarget(lutBS08-ISO8859-3)
FontTarget(lutBS10-ISO8859-3)
FontTarget(lutBS12-ISO8859-3)
FontTarget(lutBS14-ISO8859-3)
FontTarget(lutBS18-ISO8859-3)
FontTarget(lutBS19-ISO8859-3)
FontTarget(lutBS24-ISO8859-3)
FontTarget(lutRS08-ISO8859-3)
FontTarget(lutRS10-ISO8859-3)
FontTarget(lutRS12-ISO8859-3)
FontTarget(lutRS14-ISO8859-3)
FontTarget(lutRS18-ISO8859-3)
FontTarget(lutRS19-ISO8859-3)
FontTarget(lutRS24-ISO8859-3)
FontTarget(UTBI__10-ISO8859-3)
FontTarget(UTBI__12-ISO8859-3)
FontTarget(UTBI__14-ISO8859-3)
FontTarget(UTBI__18-ISO8859-3)
FontTarget(UTBI__24-ISO8859-3)
FontTarget(UTB___10-ISO8859-3)
FontTarget(UTB___12-ISO8859-3)
FontTarget(UTB___14-ISO8859-3)
FontTarget(UTB___18-ISO8859-3)
FontTarget(UTB___24-ISO8859-3)
FontTarget(UTI___10-ISO8859-3)
FontTarget(UTI___12-ISO8859-3)
FontTarget(UTI___14-ISO8859-3)
FontTarget(UTI___18-ISO8859-3)
FontTarget(UTI___24-ISO8859-3)
FontTarget(UTRG__10-ISO8859-3)
FontTarget(UTRG__12-ISO8859-3)
FontTarget(UTRG__14-ISO8859-3)
FontTarget(UTRG__18-ISO8859-3)
FontTarget(UTRG__24-ISO8859-3)
#endif

#if BuildISO8859_4Fonts
FontTarget(courB08-ISO8859-4)
FontTarget(courB10-ISO8859-4)
FontTarget(courB12-ISO8859-4)
FontTarget(courB14-ISO8859-4)
FontTarget(courB18-ISO8859-4)
FontTarget(courB24-ISO8859-4)
FontTarget(courBO08-ISO8859-4)
FontTarget(courBO10-ISO8859-4)
FontTarget(courBO12-ISO8859-4)
FontTarget(courBO14-ISO8859-4)
FontTarget(courBO18-ISO8859-4)
FontTarget(courBO24-ISO8859-4)
FontTarget(courO08-ISO8859-4)
FontTarget(courO10-ISO8859-4)
FontTarget(courO12-ISO8859-4)
FontTarget(courO14-ISO8859-4)
FontTarget(courO18-ISO8859-4)
FontTarget(courO24-ISO8859-4)
FontTarget(courR08-ISO8859-4)
FontTarget(courR10-ISO8859-4)
FontTarget(courR12-ISO8859-4)
FontTarget(courR14-ISO8859-4)
FontTarget(courR18-ISO8859-4)
FontTarget(courR24-ISO8859-4)
FontTarget(helvB08-ISO8859-4)
FontTarget(helvB10-ISO8859-4)
FontTarget(helvB12-ISO8859-4)
FontTarget(helvB14-ISO8859-4)
FontTarget(helvB18-ISO8859-4)
FontTarget(helvB24-ISO8859-4)
FontTarget(helvBO08-ISO8859-4)
FontTarget(helvBO10-ISO8859-4)
FontTarget(helvBO12-ISO8859-4)
FontTarget(helvBO14-ISO8859-4)
FontTarget(helvBO18-ISO8859-4)
FontTarget(helvBO24-ISO8859-4)
FontTarget(helvO08-ISO8859-4)
FontTarget(helvO10-ISO8859-4)
FontTarget(helvO12-ISO8859-4)
FontTarget(helvO14-ISO8859-4)
FontTarget(helvO18-ISO8859-4)
FontTarget(helvO24-ISO8859-4)
FontTarget(helvR08-ISO8859-4)
FontTarget(helvR10-ISO8859-4)
FontTarget(helvR12-ISO8859-4)
FontTarget(helvR14-ISO8859-4)
FontTarget(helvR18-ISO8859-4)
FontTarget(helvR24-ISO8859-4)
FontTarget(ncenB08-ISO8859-4)
FontTarget(ncenB10-ISO8859-4)
FontTarget(ncenB12-ISO8859-4)
FontTarget(ncenB14-ISO8859-4)
FontTarget(ncenB18-ISO8859-4)
FontTarget(ncenB24-ISO8859-4)
FontTarget(ncenBI08-ISO8859-4)
FontTarget(ncenBI10-ISO8859-4)
FontTarget(ncenBI12-ISO8859-4)
FontTarget(ncenBI14-ISO8859-4)
FontTarget(ncenBI18-ISO8859-4)
FontTarget(ncenBI24-ISO8859-4)
FontTarget(ncenI08-ISO8859-4)
FontTarget(ncenI10-ISO8859-4)
FontTarget(ncenI12-ISO8859-4)
FontTarget(ncenI14-ISO8859-4)
FontTarget(ncenI18-ISO8859-4)
FontTarget(ncenI24-ISO8859-4)
FontTarget(ncenR08-ISO8859-4)
FontTarget(ncenR10-ISO8859-4)
FontTarget(ncenR12-ISO8859-4)
FontTarget(ncenR14-ISO8859-4)
FontTarget(ncenR18-ISO8859-4)
FontTarget(ncenR24-ISO8859-4)
FontTarget(timB08-ISO8859-4)
FontTarget(timB10-ISO8859-4)
FontTarget(timB12-ISO8859-4)
FontTarget(timB14-ISO8859-4)
FontTarget(timB18-ISO8859-4)
FontTarget(timB24-ISO8859-4)
FontTarget(timBI08-ISO8859-4)
FontTarget(timBI10-ISO8859-4)
FontTarget(timBI12-ISO8859-4)
FontTarget(timBI14-ISO8859-4)
FontTarget(timBI18-ISO8859-4)
FontTarget(timBI24-ISO8859-4)
FontTarget(timI08-ISO8859-4)
FontTarget(timI10-ISO8859-4)
FontTarget(timI12-ISO8859-4)
FontTarget(timI14-ISO8859-4)
FontTarget(timI18-ISO8859-4)
FontTarget(timI24-ISO8859-4)
FontTarget(timR08-ISO8859-4)
FontTarget(timR10-ISO8859-4)
FontTarget(timR12-ISO8859-4)
FontTarget(timR14-ISO8859-4)
FontTarget(timR18-ISO8859-4)
FontTarget(timR24-ISO8859-4)
FontTarget(luBIS08-ISO8859-4)
FontTarget(luBIS10-ISO8859-4)
FontTarget(luBIS12-ISO8859-4)
FontTarget(luBIS14-ISO8859-4)
FontTarget(luBIS18-ISO8859-4)
FontTarget(luBIS19-ISO8859-4)
FontTarget(luBIS24-ISO8859-4)
FontTarget(luBS08-ISO8859-4)
FontTarget(luBS10-ISO8859-4)
FontTarget(luBS12-ISO8859-4)
FontTarget(luBS14-ISO8859-4)
FontTarget(luBS18-ISO8859-4)
FontTarget(luBS19-ISO8859-4)
FontTarget(luBS24-ISO8859-4)
FontTarget(luIS08-ISO8859-4)
FontTarget(luIS10-ISO8859-4)
FontTarget(luIS12-ISO8859-4)
FontTarget(luIS14-ISO8859-4)
FontTarget(luIS18-ISO8859-4)
FontTarget(luIS19-ISO8859-4)
FontTarget(luIS24-ISO8859-4)
FontTarget(luRS08-ISO8859-4)
FontTarget(luRS10-ISO8859-4)
FontTarget(luRS12-ISO8859-4)
FontTarget(luRS14-ISO8859-4)
FontTarget(luRS18-ISO8859-4)
FontTarget(luRS19-ISO8859-4)
FontTarget(luRS24-ISO8859-4)
FontTarget(lubB08-ISO8859-4)
FontTarget(lubB10-ISO8859-4)
FontTarget(lubB12-ISO8859-4)
FontTarget(lubB14-ISO8859-4)
FontTarget(lubB18-ISO8859-4)
FontTarget(lubB19-ISO8859-4)
FontTarget(lubB24-ISO8859-4)
FontTarget(lubBI08-ISO8859-4)
FontTarget(lubBI10-ISO8859-4)
FontTarget(lubBI12-ISO8859-4)
FontTarget(lubBI14-ISO8859-4)
FontTarget(lubBI18-ISO8859-4)
FontTarget(lubBI19-ISO8859-4)
FontTarget(lubBI24-ISO8859-4)
FontTarget(lubI08-ISO8859-4)
FontTarget(lubI10-ISO8859-4)
FontTarget(lubI12-ISO8859-4)
FontTarget(lubI14-ISO8859-4)
FontTarget(lubI18-ISO8859-4)
FontTarget(lubI19-ISO8859-4)
FontTarget(lubI24-ISO8859-4)
FontTarget(lubR08-ISO8859-4)
FontTarget(lubR10-ISO8859-4)
FontTarget(lubR12-ISO8859-4)
FontTarget(lubR14-ISO8859-4)
FontTarget(lubR18-ISO8859-4)
FontTarget(lubR19-ISO8859-4)
FontTarget(lubR24-ISO8859-4)
FontTarget(lutBS08-ISO8859-4)
FontTarget(lutBS10-ISO8859-4)
FontTarget(lutBS12-ISO8859-4)
FontTarget(lutBS14-ISO8859-4)
FontTarget(lutBS18-ISO8859-4)
FontTarget(lutBS19-ISO8859-4)
FontTarget(lutBS24-ISO8859-4)
FontTarget(lutRS08-ISO8859-4)
FontTarget(lutRS10-ISO8859-4)
FontTarget(lutRS12-ISO8859-4)
FontTarget(lutRS14-ISO8859-4)
FontTarget(lutRS18-ISO8859-4)
FontTarget(lutRS19-ISO8859-4)
FontTarget(lutRS24-ISO8859-4)
FontTarget(UTBI__10-ISO8859-4)
FontTarget(UTBI__12-ISO8859-4)
FontTarget(UTBI__14-ISO8859-4)
FontTarget(UTBI__18-ISO8859-4)
FontTarget(UTBI__24-ISO8859-4)
FontTarget(UTB___10-ISO8859-4)
FontTarget(UTB___12-ISO8859-4)
FontTarget(UTB___14-ISO8859-4)
FontTarget(UTB___18-ISO8859-4)
FontTarget(UTB___24-ISO8859-4)
FontTarget(UTI___10-ISO8859-4)
FontTarget(UTI___12-ISO8859-4)
FontTarget(UTI___14-ISO8859-4)
FontTarget(UTI___18-ISO8859-4)
FontTarget(UTI___24-ISO8859-4)
FontTarget(UTRG__10-ISO8859-4)
FontTarget(UTRG__12-ISO8859-4)
FontTarget(UTRG__14-ISO8859-4)
FontTarget(UTRG__18-ISO8859-4)
FontTarget(UTRG__24-ISO8859-4)
#endif

#if BuildISO8859_9Fonts
FontTarget(courB08-ISO8859-9)
FontTarget(courB10-ISO8859-9)
FontTarget(courB12-ISO8859-9)
FontTarget(courB14-ISO8859-9)
FontTarget(courB18-ISO8859-9)
FontTarget(courB24-ISO8859-9)
FontTarget(courBO08-ISO8859-9)
FontTarget(courBO10-ISO8859-9)
FontTarget(courBO12-ISO8859-9)
FontTarget(courBO14-ISO8859-9)
FontTarget(courBO18-ISO8859-9)
FontTarget(courBO24-ISO8859-9)
FontTarget(courO08-ISO8859-9)
FontTarget(courO10-ISO8859-9)
FontTarget(courO12-ISO8859-9)
FontTarget(courO14-ISO8859-9)
FontTarget(courO18-ISO8859-9)
FontTarget(courO24-ISO8859-9)
FontTarget(courR08-ISO8859-9)
FontTarget(courR10-ISO8859-9)
FontTarget(courR12-ISO8859-9)
FontTarget(courR14-ISO8859-9)
FontTarget(courR18-ISO8859-9)
FontTarget(courR24-ISO8859-9)
FontTarget(helvB08-ISO8859-9)
FontTarget(helvB10-ISO8859-9)
FontTarget(helvB12-ISO8859-9)
FontTarget(helvB14-ISO8859-9)
FontTarget(helvB18-ISO8859-9)
FontTarget(helvB24-ISO8859-9)
FontTarget(helvBO08-ISO8859-9)
FontTarget(helvBO10-ISO8859-9)
FontTarget(helvBO12-ISO8859-9)
FontTarget(helvBO14-ISO8859-9)
FontTarget(helvBO18-ISO8859-9)
FontTarget(helvBO24-ISO8859-9)
FontTarget(helvO08-ISO8859-9)
FontTarget(helvO10-ISO8859-9)
FontTarget(helvO12-ISO8859-9)
FontTarget(helvO14-ISO8859-9)
FontTarget(helvO18-ISO8859-9)
FontTarget(helvO24-ISO8859-9)
FontTarget(helvR08-ISO8859-9)
FontTarget(helvR10-ISO8859-9)
FontTarget(helvR12-ISO8859-9)
FontTarget(helvR14-ISO8859-9)
FontTarget(helvR18-ISO8859-9)
FontTarget(helvR24-ISO8859-9)
FontTarget(ncenB08-ISO8859-9)
FontTarget(ncenB10-ISO8859-9)
FontTarget(ncenB12-ISO8859-9)
FontTarget(ncenB14-ISO8859-9)
FontTarget(ncenB18-ISO8859-9)
FontTarget(ncenB24-ISO8859-9)
FontTarget(ncenBI08-ISO8859-9)
FontTarget(ncenBI10-ISO8859-9)
FontTarget(ncenBI12-ISO8859-9)
FontTarget(ncenBI14-ISO8859-9)
FontTarget(ncenBI18-ISO8859-9)
FontTarget(ncenBI24-ISO8859-9)
FontTarget(ncenI08-ISO8859-9)
FontTarget(ncenI10-ISO8859-9)
FontTarget(ncenI12-ISO8859-9)
FontTarget(ncenI14-ISO8859-9)
FontTarget(ncenI18-ISO8859-9)
FontTarget(ncenI24-ISO8859-9)
FontTarget(ncenR08-ISO8859-9)
FontTarget(ncenR10-ISO8859-9)
FontTarget(ncenR12-ISO8859-9)
FontTarget(ncenR14-ISO8859-9)
FontTarget(ncenR18-ISO8859-9)
FontTarget(ncenR24-ISO8859-9)
FontTarget(timB08-ISO8859-9)
FontTarget(timB10-ISO8859-9)
FontTarget(timB12-ISO8859-9)
FontTarget(timB14-ISO8859-9)
FontTarget(timB18-ISO8859-9)
FontTarget(timB24-ISO8859-9)
FontTarget(timBI08-ISO8859-9)
FontTarget(timBI10-ISO8859-9)
FontTarget(timBI12-ISO8859-9)
FontTarget(timBI14-ISO8859-9)
FontTarget(timBI18-ISO8859-9)
FontTarget(timBI24-ISO8859-9)
FontTarget(timI08-ISO8859-9)
FontTarget(timI10-ISO8859-9)
FontTarget(timI12-ISO8859-9)
FontTarget(timI14-ISO8859-9)
FontTarget(timI18-ISO8859-9)
FontTarget(timI24-ISO8859-9)
FontTarget(timR08-ISO8859-9)
FontTarget(timR10-ISO8859-9)
FontTarget(timR12-ISO8859-9)
FontTarget(timR14-ISO8859-9)
FontTarget(timR18-ISO8859-9)
FontTarget(timR24-ISO8859-9)
FontTarget(luBIS08-ISO8859-9)
FontTarget(luBIS10-ISO8859-9)
FontTarget(luBIS12-ISO8859-9)
FontTarget(luBIS14-ISO8859-9)
FontTarget(luBIS18-ISO8859-9)
FontTarget(luBIS19-ISO8859-9)
FontTarget(luBIS24-ISO8859-9)
FontTarget(luBS08-ISO8859-9)
FontTarget(luBS10-ISO8859-9)
FontTarget(luBS12-ISO8859-9)
FontTarget(luBS14-ISO8859-9)
FontTarget(luBS18-ISO8859-9)
FontTarget(luBS19-ISO8859-9)
FontTarget(luBS24-ISO8859-9)
FontTarget(luIS08-ISO8859-9)
FontTarget(luIS10-ISO8859-9)
FontTarget(luIS12-ISO8859-9)
FontTarget(luIS14-ISO8859-9)
FontTarget(luIS18-ISO8859-9)
FontTarget(luIS19-ISO8859-9)
FontTarget(luIS24-ISO8859-9)
FontTarget(luRS08-ISO8859-9)
FontTarget(luRS10-ISO8859-9)
FontTarget(luRS12-ISO8859-9)
FontTarget(luRS14-ISO8859-9)
FontTarget(luRS18-ISO8859-9)
FontTarget(luRS19-ISO8859-9)
FontTarget(luRS24-ISO8859-9)
FontTarget(lubB08-ISO8859-9)
FontTarget(lubB10-ISO8859-9)
FontTarget(lubB12-ISO8859-9)
FontTarget(lubB14-ISO8859-9)
FontTarget(lubB18-ISO8859-9)
FontTarget(lubB19-ISO8859-9)
FontTarget(lubB24-ISO8859-9)
FontTarget(lubBI08-ISO8859-9)
FontTarget(lubBI10-ISO8859-9)
FontTarget(lubBI12-ISO8859-9)
FontTarget(lubBI14-ISO8859-9)
FontTarget(lubBI18-ISO8859-9)
FontTarget(lubBI19-ISO8859-9)
FontTarget(lubBI24-ISO8859-9)
FontTarget(lubI08-ISO8859-9)
FontTarget(lubI10-ISO8859-9)
FontTarget(lubI12-ISO8859-9)
FontTarget(lubI14-ISO8859-9)
FontTarget(lubI18-ISO8859-9)
FontTarget(lubI19-ISO8859-9)
FontTarget(lubI24-ISO8859-9)
FontTarget(lubR08-ISO8859-9)
FontTarget(lubR10-ISO8859-9)
FontTarget(lubR12-ISO8859-9)
FontTarget(lubR14-ISO8859-9)
FontTarget(lubR18-ISO8859-9)
FontTarget(lubR19-ISO8859-9)
FontTarget(lubR24-ISO8859-9)
FontTarget(lutBS08-ISO8859-9)
FontTarget(lutBS10-ISO8859-9)
FontTarget(lutBS12-ISO8859-9)
FontTarget(lutBS14-ISO8859-9)
FontTarget(lutBS18-ISO8859-9)
FontTarget(lutBS19-ISO8859-9)
FontTarget(lutBS24-ISO8859-9)
FontTarget(lutRS08-ISO8859-9)
FontTarget(lutRS10-ISO8859-9)
FontTarget(lutRS12-ISO8859-9)
FontTarget(lutRS14-ISO8859-9)
FontTarget(lutRS18-ISO8859-9)
FontTarget(lutRS19-ISO8859-9)
FontTarget(lutRS24-ISO8859-9)
FontTarget(UTBI__10-ISO8859-9)
FontTarget(UTBI__12-ISO8859-9)
FontTarget(UTBI__14-ISO8859-9)
FontTarget(UTBI__18-ISO8859-9)
FontTarget(UTBI__24-ISO8859-9)
FontTarget(UTB___10-ISO8859-9)
FontTarget(UTB___12-ISO8859-9)
FontTarget(UTB___14-ISO8859-9)
FontTarget(UTB___18-ISO8859-9)
FontTarget(UTB___24-ISO8859-9)
FontTarget(UTI___10-ISO8859-9)
FontTarget(UTI___12-ISO8859-9)
FontTarget(UTI___14-ISO8859-9)
FontTarget(UTI___18-ISO8859-9)
FontTarget(UTI___24-ISO8859-9)
FontTarget(UTRG__10-ISO8859-9)
FontTarget(UTRG__12-ISO8859-9)
FontTarget(UTRG__14-ISO8859-9)
FontTarget(UTRG__18-ISO8859-9)
FontTarget(UTRG__24-ISO8859-9)
#endif

#if BuildISO8859_10Fonts
FontTarget(courB08-ISO8859-10)
FontTarget(courB10-ISO8859-10)
FontTarget(courB12-ISO8859-10)
FontTarget(courB14-ISO8859-10)
FontTarget(courB18-ISO8859-10)
FontTarget(courB24-ISO8859-10)
FontTarget(courBO08-ISO8859-10)
FontTarget(courBO10-ISO8859-10)
FontTarget(courBO12-ISO8859-10)
FontTarget(courBO14-ISO8859-10)
FontTarget(courBO18-ISO8859-10)
FontTarget(courBO24-ISO8859-10)
FontTarget(courO08-ISO8859-10)
FontTarget(courO10-ISO8859-10)
FontTarget(courO12-ISO8859-10)
FontTarget(courO14-ISO8859-10)
FontTarget(courO18-ISO8859-10)
FontTarget(courO24-ISO8859-10)
FontTarget(courR08-ISO8859-10)
FontTarget(courR10-ISO8859-10)
FontTarget(courR12-ISO8859-10)
FontTarget(courR14-ISO8859-10)
FontTarget(courR18-ISO8859-10)
FontTarget(courR24-ISO8859-10)
FontTarget(helvB08-ISO8859-10)
FontTarget(helvB10-ISO8859-10)
FontTarget(helvB12-ISO8859-10)
FontTarget(helvB14-ISO8859-10)
FontTarget(helvB18-ISO8859-10)
FontTarget(helvB24-ISO8859-10)
FontTarget(helvBO08-ISO8859-10)
FontTarget(helvBO10-ISO8859-10)
FontTarget(helvBO12-ISO8859-10)
FontTarget(helvBO14-ISO8859-10)
FontTarget(helvBO18-ISO8859-10)
FontTarget(helvBO24-ISO8859-10)
FontTarget(helvO08-ISO8859-10)
FontTarget(helvO10-ISO8859-10)
FontTarget(helvO12-ISO8859-10)
FontTarget(helvO14-ISO8859-10)
FontTarget(helvO18-ISO8859-10)
FontTarget(helvO24-ISO8859-10)
FontTarget(helvR08-ISO8859-10)
FontTarget(helvR10-ISO8859-10)
FontTarget(helvR12-ISO8859-10)
FontTarget(helvR14-ISO8859-10)
FontTarget(helvR18-ISO8859-10)
FontTarget(helvR24-ISO8859-10)
FontTarget(ncenB08-ISO8859-10)
FontTarget(ncenB10-ISO8859-10)
FontTarget(ncenB12-ISO8859-10)
FontTarget(ncenB14-ISO8859-10)
FontTarget(ncenB18-ISO8859-10)
FontTarget(ncenB24-ISO8859-10)
FontTarget(ncenBI08-ISO8859-10)
FontTarget(ncenBI10-ISO8859-10)
FontTarget(ncenBI12-ISO8859-10)
FontTarget(ncenBI14-ISO8859-10)
FontTarget(ncenBI18-ISO8859-10)
FontTarget(ncenBI24-ISO8859-10)
FontTarget(ncenI08-ISO8859-10)
FontTarget(ncenI10-ISO8859-10)
FontTarget(ncenI12-ISO8859-10)
FontTarget(ncenI14-ISO8859-10)
FontTarget(ncenI18-ISO8859-10)
FontTarget(ncenI24-ISO8859-10)
FontTarget(ncenR08-ISO8859-10)
FontTarget(ncenR10-ISO8859-10)
FontTarget(ncenR12-ISO8859-10)
FontTarget(ncenR14-ISO8859-10)
FontTarget(ncenR18-ISO8859-10)
FontTarget(ncenR24-ISO8859-10)
FontTarget(timB08-ISO8859-10)
FontTarget(timB10-ISO8859-10)
FontTarget(timB12-ISO8859-10)
FontTarget(timB14-ISO8859-10)
FontTarget(timB18-ISO8859-10)
FontTarget(timB24-ISO8859-10)
FontTarget(timBI08-ISO8859-10)
FontTarget(timBI10-ISO8859-10)
FontTarget(timBI12-ISO8859-10)
FontTarget(timBI14-ISO8859-10)
FontTarget(timBI18-ISO8859-10)
FontTarget(timBI24-ISO8859-10)
FontTarget(timI08-ISO8859-10)
FontTarget(timI10-ISO8859-10)
FontTarget(timI12-ISO8859-10)
FontTarget(timI14-ISO8859-10)
FontTarget(timI18-ISO8859-10)
FontTarget(timI24-ISO8859-10)
FontTarget(timR08-ISO8859-10)
FontTarget(timR10-ISO8859-10)
FontTarget(timR12-ISO8859-10)
FontTarget(timR14-ISO8859-10)
FontTarget(timR18-ISO8859-10)
FontTarget(timR24-ISO8859-10)
FontTarget(luBIS08-ISO8859-10)
FontTarget(luBIS10-ISO8859-10)
FontTarget(luBIS12-ISO8859-10)
FontTarget(luBIS14-ISO8859-10)
FontTarget(luBIS18-ISO8859-10)
FontTarget(luBIS19-ISO8859-10)
FontTarget(luBIS24-ISO8859-10)
FontTarget(luBS08-ISO8859-10)
FontTarget(luBS10-ISO8859-10)
FontTarget(luBS12-ISO8859-10)
FontTarget(luBS14-ISO8859-10)
FontTarget(luBS18-ISO8859-10)
FontTarget(luBS19-ISO8859-10)
FontTarget(luBS24-ISO8859-10)
FontTarget(luIS08-ISO8859-10)
FontTarget(luIS10-ISO8859-10)
FontTarget(luIS12-ISO8859-10)
FontTarget(luIS14-ISO8859-10)
FontTarget(luIS18-ISO8859-10)
FontTarget(luIS19-ISO8859-10)
FontTarget(luIS24-ISO8859-10)
FontTarget(luRS08-ISO8859-10)
FontTarget(luRS10-ISO8859-10)
FontTarget(luRS12-ISO8859-10)
FontTarget(luRS14-ISO8859-10)
FontTarget(luRS18-ISO8859-10)
FontTarget(luRS19-ISO8859-10)
FontTarget(luRS24-ISO8859-10)
FontTarget(lubB08-ISO8859-10)
FontTarget(lubB10-ISO8859-10)
FontTarget(lubB12-ISO8859-10)
FontTarget(lubB14-ISO8859-10)
FontTarget(lubB18-ISO8859-10)
FontTarget(lubB19-ISO8859-10)
FontTarget(lubB24-ISO8859-10)
FontTarget(lubBI08-ISO8859-10)
FontTarget(lubBI10-ISO8859-10)
FontTarget(lubBI12-ISO8859-10)
FontTarget(lubBI14-ISO8859-10)
FontTarget(lubBI18-ISO8859-10)
FontTarget(lubBI19-ISO8859-10)
FontTarget(lubBI24-ISO8859-10)
FontTarget(lubI08-ISO8859-10)
FontTarget(lubI10-ISO8859-10)
FontTarget(lubI12-ISO8859-10)
FontTarget(lubI14-ISO8859-10)
FontTarget(lubI18-ISO8859-10)
FontTarget(lubI19-ISO8859-10)
FontTarget(lubI24-ISO8859-10)
FontTarget(lubR08-ISO8859-10)
FontTarget(lubR10-ISO8859-10)
FontTarget(lubR12-ISO8859-10)
FontTarget(lubR14-ISO8859-10)
FontTarget(lubR18-ISO8859-10)
FontTarget(lubR19-ISO8859-10)
FontTarget(lubR24-ISO8859-10)
FontTarget(lutBS08-ISO8859-10)
FontTarget(lutBS10-ISO8859-10)
FontTarget(lutBS12-ISO8859-10)
FontTarget(lutBS14-ISO8859-10)
FontTarget(lutBS18-ISO8859-10)
FontTarget(lutBS19-ISO8859-10)
FontTarget(lutBS24-ISO8859-10)
FontTarget(lutRS08-ISO8859-10)
FontTarget(lutRS10-ISO8859-10)
FontTarget(lutRS12-ISO8859-10)
FontTarget(lutRS14-ISO8859-10)
FontTarget(lutRS18-ISO8859-10)
FontTarget(lutRS19-ISO8859-10)
FontTarget(lutRS24-ISO8859-10)
FontTarget(UTBI__10-ISO8859-10)
FontTarget(UTBI__12-ISO8859-10)
FontTarget(UTBI__14-ISO8859-10)
FontTarget(UTBI__18-ISO8859-10)
FontTarget(UTBI__24-ISO8859-10)
FontTarget(UTB___10-ISO8859-10)
FontTarget(UTB___12-ISO8859-10)
FontTarget(UTB___14-ISO8859-10)
FontTarget(UTB___18-ISO8859-10)
FontTarget(UTB___24-ISO8859-10)
FontTarget(UTI___10-ISO8859-10)
FontTarget(UTI___12-ISO8859-10)
FontTarget(UTI___14-ISO8859-10)
FontTarget(UTI___18-ISO8859-10)
FontTarget(UTI___24-ISO8859-10)
FontTarget(UTRG__10-ISO8859-10)
FontTarget(UTRG__12-ISO8859-10)
FontTarget(UTRG__14-ISO8859-10)
FontTarget(UTRG__18-ISO8859-10)
FontTarget(UTRG__24-ISO8859-10)
#endif

#if BuildISO8859_13Fonts
FontTarget(courB08-ISO8859-13)
FontTarget(courB10-ISO8859-13)
FontTarget(courB12-ISO8859-13)
FontTarget(courB14-ISO8859-13)
FontTarget(courB18-ISO8859-13)
FontTarget(courB24-ISO8859-13)
FontTarget(courBO08-ISO8859-13)
FontTarget(courBO10-ISO8859-13)
FontTarget(courBO12-ISO8859-13)
FontTarget(courBO14-ISO8859-13)
FontTarget(courBO18-ISO8859-13)
FontTarget(courBO24-ISO8859-13)
FontTarget(courO08-ISO8859-13)
FontTarget(courO10-ISO8859-13)
FontTarget(courO12-ISO8859-13)
FontTarget(courO14-ISO8859-13)
FontTarget(courO18-ISO8859-13)
FontTarget(courO24-ISO8859-13)
FontTarget(courR08-ISO8859-13)
FontTarget(courR10-ISO8859-13)
FontTarget(courR12-ISO8859-13)
FontTarget(courR14-ISO8859-13)
FontTarget(courR18-ISO8859-13)
FontTarget(courR24-ISO8859-13)
FontTarget(helvB08-ISO8859-13)
FontTarget(helvB10-ISO8859-13)
FontTarget(helvB12-ISO8859-13)
FontTarget(helvB14-ISO8859-13)
FontTarget(helvB18-ISO8859-13)
FontTarget(helvB24-ISO8859-13)
FontTarget(helvBO08-ISO8859-13)
FontTarget(helvBO10-ISO8859-13)
FontTarget(helvBO12-ISO8859-13)
FontTarget(helvBO14-ISO8859-13)
FontTarget(helvBO18-ISO8859-13)
FontTarget(helvBO24-ISO8859-13)
FontTarget(helvO08-ISO8859-13)
FontTarget(helvO10-ISO8859-13)
FontTarget(helvO12-ISO8859-13)
FontTarget(helvO14-ISO8859-13)
FontTarget(helvO18-ISO8859-13)
FontTarget(helvO24-ISO8859-13)
FontTarget(helvR08-ISO8859-13)
FontTarget(helvR10-ISO8859-13)
FontTarget(helvR12-ISO8859-13)
FontTarget(helvR14-ISO8859-13)
FontTarget(helvR18-ISO8859-13)
FontTarget(helvR24-ISO8859-13)
FontTarget(ncenB08-ISO8859-13)
FontTarget(ncenB10-ISO8859-13)
FontTarget(ncenB12-ISO8859-13)
FontTarget(ncenB14-ISO8859-13)
FontTarget(ncenB18-ISO8859-13)
FontTarget(ncenB24-ISO8859-13)
FontTarget(ncenBI08-ISO8859-13)
FontTarget(ncenBI10-ISO8859-13)
FontTarget(ncenBI12-ISO8859-13)
FontTarget(ncenBI14-ISO8859-13)
FontTarget(ncenBI18-ISO8859-13)
FontTarget(ncenBI24-ISO8859-13)
FontTarget(ncenI08-ISO8859-13)
FontTarget(ncenI10-ISO8859-13)
FontTarget(ncenI12-ISO8859-13)
FontTarget(ncenI14-ISO8859-13)
FontTarget(ncenI18-ISO8859-13)
FontTarget(ncenI24-ISO8859-13)
FontTarget(ncenR08-ISO8859-13)
FontTarget(ncenR10-ISO8859-13)
FontTarget(ncenR12-ISO8859-13)
FontTarget(ncenR14-ISO8859-13)
FontTarget(ncenR18-ISO8859-13)
FontTarget(ncenR24-ISO8859-13)
FontTarget(timB08-ISO8859-13)
FontTarget(timB10-ISO8859-13)
FontTarget(timB12-ISO8859-13)
FontTarget(timB14-ISO8859-13)
FontTarget(timB18-ISO8859-13)
FontTarget(timB24-ISO8859-13)
FontTarget(timBI08-ISO8859-13)
FontTarget(timBI10-ISO8859-13)
FontTarget(timBI12-ISO8859-13)
FontTarget(timBI14-ISO8859-13)
FontTarget(timBI18-ISO8859-13)
FontTarget(timBI24-ISO8859-13)
FontTarget(timI08-ISO8859-13)
FontTarget(timI10-ISO8859-13)
FontTarget(timI12-ISO8859-13)
FontTarget(timI14-ISO8859-13)
FontTarget(timI18-ISO8859-13)
FontTarget(timI24-ISO8859-13)
FontTarget(timR08-ISO8859-13)
FontTarget(timR10-ISO8859-13)
FontTarget(timR12-ISO8859-13)
FontTarget(timR14-ISO8859-13)
FontTarget(timR18-ISO8859-13)
FontTarget(timR24-ISO8859-13)
FontTarget(luBIS08-ISO8859-13)
FontTarget(luBIS10-ISO8859-13)
FontTarget(luBIS12-ISO8859-13)
FontTarget(luBIS14-ISO8859-13)
FontTarget(luBIS18-ISO8859-13)
FontTarget(luBIS19-ISO8859-13)
FontTarget(luBIS24-ISO8859-13)
FontTarget(luBS08-ISO8859-13)
FontTarget(luBS10-ISO8859-13)
FontTarget(luBS12-ISO8859-13)
FontTarget(luBS14-ISO8859-13)
FontTarget(luBS18-ISO8859-13)
FontTarget(luBS19-ISO8859-13)
FontTarget(luBS24-ISO8859-13)
FontTarget(luIS08-ISO8859-13)
FontTarget(luIS10-ISO8859-13)
FontTarget(luIS12-ISO8859-13)
FontTarget(luIS14-ISO8859-13)
FontTarget(luIS18-ISO8859-13)
FontTarget(luIS19-ISO8859-13)
FontTarget(luIS24-ISO8859-13)
FontTarget(luRS08-ISO8859-13)
FontTarget(luRS10-ISO8859-13)
FontTarget(luRS12-ISO8859-13)
FontTarget(luRS14-ISO8859-13)
FontTarget(luRS18-ISO8859-13)
FontTarget(luRS19-ISO8859-13)
FontTarget(luRS24-ISO8859-13)
FontTarget(lubB08-ISO8859-13)
FontTarget(lubB10-ISO8859-13)
FontTarget(lubB12-ISO8859-13)
FontTarget(lubB14-ISO8859-13)
FontTarget(lubB18-ISO8859-13)
FontTarget(lubB19-ISO8859-13)
FontTarget(lubB24-ISO8859-13)
FontTarget(lubBI08-ISO8859-13)
FontTarget(lubBI10-ISO8859-13)
FontTarget(lubBI12-ISO8859-13)
FontTarget(lubBI14-ISO8859-13)
FontTarget(lubBI18-ISO8859-13)
FontTarget(lubBI19-ISO8859-13)
FontTarget(lubBI24-ISO8859-13)
FontTarget(lubI08-ISO8859-13)
FontTarget(lubI10-ISO8859-13)
FontTarget(lubI12-ISO8859-13)
FontTarget(lubI14-ISO8859-13)
FontTarget(lubI18-ISO8859-13)
FontTarget(lubI19-ISO8859-13)
FontTarget(lubI24-ISO8859-13)
FontTarget(lubR08-ISO8859-13)
FontTarget(lubR10-ISO8859-13)
FontTarget(lubR12-ISO8859-13)
FontTarget(lubR14-ISO8859-13)
FontTarget(lubR18-ISO8859-13)
FontTarget(lubR19-ISO8859-13)
FontTarget(lubR24-ISO8859-13)
FontTarget(lutBS08-ISO8859-13)
FontTarget(lutBS10-ISO8859-13)
FontTarget(lutBS12-ISO8859-13)
FontTarget(lutBS14-ISO8859-13)
FontTarget(lutBS18-ISO8859-13)
FontTarget(lutBS19-ISO8859-13)
FontTarget(lutBS24-ISO8859-13)
FontTarget(lutRS08-ISO8859-13)
FontTarget(lutRS10-ISO8859-13)
FontTarget(lutRS12-ISO8859-13)
FontTarget(lutRS14-ISO8859-13)
FontTarget(lutRS18-ISO8859-13)
FontTarget(lutRS19-ISO8859-13)
FontTarget(lutRS24-ISO8859-13)
FontTarget(UTBI__10-ISO8859-13)
FontTarget(UTBI__12-ISO8859-13)
FontTarget(UTBI__14-ISO8859-13)
FontTarget(UTBI__18-ISO8859-13)
FontTarget(UTBI__24-ISO8859-13)
FontTarget(UTB___10-ISO8859-13)
FontTarget(UTB___12-ISO8859-13)
FontTarget(UTB___14-ISO8859-13)
FontTarget(UTB___18-ISO8859-13)
FontTarget(UTB___24-ISO8859-13)
FontTarget(UTI___10-ISO8859-13)
FontTarget(UTI___12-ISO8859-13)
FontTarget(UTI___14-ISO8859-13)
FontTarget(UTI___18-ISO8859-13)
FontTarget(UTI___24-ISO8859-13)
FontTarget(UTRG__10-ISO8859-13)
FontTarget(UTRG__12-ISO8859-13)
FontTarget(UTRG__14-ISO8859-13)
FontTarget(UTRG__18-ISO8859-13)
FontTarget(UTRG__24-ISO8859-13)
#endif

#if BuildISO8859_14Fonts
FontTarget(courB08-ISO8859-14)
FontTarget(courB10-ISO8859-14)
FontTarget(courB12-ISO8859-14)
FontTarget(courB14-ISO8859-14)
FontTarget(courB18-ISO8859-14)
FontTarget(courB24-ISO8859-14)
FontTarget(courBO08-ISO8859-14)
FontTarget(courBO10-ISO8859-14)
FontTarget(courBO12-ISO8859-14)
FontTarget(courBO14-ISO8859-14)
FontTarget(courBO18-ISO8859-14)
FontTarget(courBO24-ISO8859-14)
FontTarget(courO08-ISO8859-14)
FontTarget(courO10-ISO8859-14)
FontTarget(courO12-ISO8859-14)
FontTarget(courO14-ISO8859-14)
FontTarget(courO18-ISO8859-14)
FontTarget(courO24-ISO8859-14)
FontTarget(courR08-ISO8859-14)
FontTarget(courR10-ISO8859-14)
FontTarget(courR12-ISO8859-14)
FontTarget(courR14-ISO8859-14)
FontTarget(courR18-ISO8859-14)
FontTarget(courR24-ISO8859-14)
FontTarget(helvB08-ISO8859-14)
FontTarget(helvB10-ISO8859-14)
FontTarget(helvB12-ISO8859-14)
FontTarget(helvB14-ISO8859-14)
FontTarget(helvB18-ISO8859-14)
FontTarget(helvB24-ISO8859-14)
FontTarget(helvBO08-ISO8859-14)
FontTarget(helvBO10-ISO8859-14)
FontTarget(helvBO12-ISO8859-14)
FontTarget(helvBO14-ISO8859-14)
FontTarget(helvBO18-ISO8859-14)
FontTarget(helvBO24-ISO8859-14)
FontTarget(helvO08-ISO8859-14)
FontTarget(helvO10-ISO8859-14)
FontTarget(helvO12-ISO8859-14)
FontTarget(helvO14-ISO8859-14)
FontTarget(helvO18-ISO8859-14)
FontTarget(helvO24-ISO8859-14)
FontTarget(helvR08-ISO8859-14)
FontTarget(helvR10-ISO8859-14)
FontTarget(helvR12-ISO8859-14)
FontTarget(helvR14-ISO8859-14)
FontTarget(helvR18-ISO8859-14)
FontTarget(helvR24-ISO8859-14)
FontTarget(ncenB08-ISO8859-14)
FontTarget(ncenB10-ISO8859-14)
FontTarget(ncenB12-ISO8859-14)
FontTarget(ncenB14-ISO8859-14)
FontTarget(ncenB18-ISO8859-14)
FontTarget(ncenB24-ISO8859-14)
FontTarget(ncenBI08-ISO8859-14)
FontTarget(ncenBI10-ISO8859-14)
FontTarget(ncenBI12-ISO8859-14)
FontTarget(ncenBI14-ISO8859-14)
FontTarget(ncenBI18-ISO8859-14)
FontTarget(ncenBI24-ISO8859-14)
FontTarget(ncenI08-ISO8859-14)
FontTarget(ncenI10-ISO8859-14)
FontTarget(ncenI12-ISO8859-14)
FontTarget(ncenI14-ISO8859-14)
FontTarget(ncenI18-ISO8859-14)
FontTarget(ncenI24-ISO8859-14)
FontTarget(ncenR08-ISO8859-14)
FontTarget(ncenR10-ISO8859-14)
FontTarget(ncenR12-ISO8859-14)
FontTarget(ncenR14-ISO8859-14)
FontTarget(ncenR18-ISO8859-14)
FontTarget(ncenR24-ISO8859-14)
FontTarget(timB08-ISO8859-14)
FontTarget(timB10-ISO8859-14)
FontTarget(timB12-ISO8859-14)
FontTarget(timB14-ISO8859-14)
FontTarget(timB18-ISO8859-14)
FontTarget(timB24-ISO8859-14)
FontTarget(timBI08-ISO8859-14)
FontTarget(timBI10-ISO8859-14)
FontTarget(timBI12-ISO8859-14)
FontTarget(timBI14-ISO8859-14)
FontTarget(timBI18-ISO8859-14)
FontTarget(timBI24-ISO8859-14)
FontTarget(timI08-ISO8859-14)
FontTarget(timI10-ISO8859-14)
FontTarget(timI12-ISO8859-14)
FontTarget(timI14-ISO8859-14)
FontTarget(timI18-ISO8859-14)
FontTarget(timI24-ISO8859-14)
FontTarget(timR08-ISO8859-14)
FontTarget(timR10-ISO8859-14)
FontTarget(timR12-ISO8859-14)
FontTarget(timR14-ISO8859-14)
FontTarget(timR18-ISO8859-14)
FontTarget(timR24-ISO8859-14)
FontTarget(luBIS08-ISO8859-14)
FontTarget(luBIS10-ISO8859-14)
FontTarget(luBIS12-ISO8859-14)
FontTarget(luBIS14-ISO8859-14)
FontTarget(luBIS18-ISO8859-14)
FontTarget(luBIS19-ISO8859-14)
FontTarget(luBIS24-ISO8859-14)
FontTarget(luBS08-ISO8859-14)
FontTarget(luBS10-ISO8859-14)
FontTarget(luBS12-ISO8859-14)
FontTarget(luBS14-ISO8859-14)
FontTarget(luBS18-ISO8859-14)
FontTarget(luBS19-ISO8859-14)
FontTarget(luBS24-ISO8859-14)
FontTarget(luIS08-ISO8859-14)
FontTarget(luIS10-ISO8859-14)
FontTarget(luIS12-ISO8859-14)
FontTarget(luIS14-ISO8859-14)
FontTarget(luIS18-ISO8859-14)
FontTarget(luIS19-ISO8859-14)
FontTarget(luIS24-ISO8859-14)
FontTarget(luRS08-ISO8859-14)
FontTarget(luRS10-ISO8859-14)
FontTarget(luRS12-ISO8859-14)
FontTarget(luRS14-ISO8859-14)
FontTarget(luRS18-ISO8859-14)
FontTarget(luRS19-ISO8859-14)
FontTarget(luRS24-ISO8859-14)
FontTarget(lubB08-ISO8859-14)
FontTarget(lubB10-ISO8859-14)
FontTarget(lubB12-ISO8859-14)
FontTarget(lubB14-ISO8859-14)
FontTarget(lubB18-ISO8859-14)
FontTarget(lubB19-ISO8859-14)
FontTarget(lubB24-ISO8859-14)
FontTarget(lubBI08-ISO8859-14)
FontTarget(lubBI10-ISO8859-14)
FontTarget(lubBI12-ISO8859-14)
FontTarget(lubBI14-ISO8859-14)
FontTarget(lubBI18-ISO8859-14)
FontTarget(lubBI19-ISO8859-14)
FontTarget(lubBI24-ISO8859-14)
FontTarget(lubI08-ISO8859-14)
FontTarget(lubI10-ISO8859-14)
FontTarget(lubI12-ISO8859-14)
FontTarget(lubI14-ISO8859-14)
FontTarget(lubI18-ISO8859-14)
FontTarget(lubI19-ISO8859-14)
FontTarget(lubI24-ISO8859-14)
FontTarget(lubR08-ISO8859-14)
FontTarget(lubR10-ISO8859-14)
FontTarget(lubR12-ISO8859-14)
FontTarget(lubR14-ISO8859-14)
FontTarget(lubR18-ISO8859-14)
FontTarget(lubR19-ISO8859-14)
FontTarget(lubR24-ISO8859-14)
FontTarget(lutBS08-ISO8859-14)
FontTarget(lutBS10-ISO8859-14)
FontTarget(lutBS12-ISO8859-14)
FontTarget(lutBS14-ISO8859-14)
FontTarget(lutBS18-ISO8859-14)
FontTarget(lutBS19-ISO8859-14)
FontTarget(lutBS24-ISO8859-14)
FontTarget(lutRS08-ISO8859-14)
FontTarget(lutRS10-ISO8859-14)
FontTarget(lutRS12-ISO8859-14)
FontTarget(lutRS14-ISO8859-14)
FontTarget(lutRS18-ISO8859-14)
FontTarget(lutRS19-ISO8859-14)
FontTarget(lutRS24-ISO8859-14)
FontTarget(UTBI__10-ISO8859-14)
FontTarget(UTBI__12-ISO8859-14)
FontTarget(UTBI__14-ISO8859-14)
FontTarget(UTBI__18-ISO8859-14)
FontTarget(UTBI__24-ISO8859-14)
FontTarget(UTB___10-ISO8859-14)
FontTarget(UTB___12-ISO8859-14)
FontTarget(UTB___14-ISO8859-14)
FontTarget(UTB___18-ISO8859-14)
FontTarget(UTB___24-ISO8859-14)
FontTarget(UTI___10-ISO8859-14)
FontTarget(UTI___12-ISO8859-14)
FontTarget(UTI___14-ISO8859-14)
FontTarget(UTI___18-ISO8859-14)
FontTarget(UTI___24-ISO8859-14)
FontTarget(UTRG__10-ISO8859-14)
FontTarget(UTRG__12-ISO8859-14)
FontTarget(UTRG__14-ISO8859-14)
FontTarget(UTRG__18-ISO8859-14)
FontTarget(UTRG__24-ISO8859-14)
#endif

#if BuildISO8859_15Fonts
FontTarget(courB08-ISO8859-15)
FontTarget(courB10-ISO8859-15)
FontTarget(courB12-ISO8859-15)
FontTarget(courB14-ISO8859-15)
FontTarget(courB18-ISO8859-15)
FontTarget(courB24-ISO8859-15)
FontTarget(courBO08-ISO8859-15)
FontTarget(courBO10-ISO8859-15)
FontTarget(courBO12-ISO8859-15)
FontTarget(courBO14-ISO8859-15)
FontTarget(courBO18-ISO8859-15)
FontTarget(courBO24-ISO8859-15)
FontTarget(courO08-ISO8859-15)
FontTarget(courO10-ISO8859-15)
FontTarget(courO12-ISO8859-15)
FontTarget(courO14-ISO8859-15)
FontTarget(courO18-ISO8859-15)
FontTarget(courO24-ISO8859-15)
FontTarget(courR08-ISO8859-15)
FontTarget(courR10-ISO8859-15)
FontTarget(courR12-ISO8859-15)
FontTarget(courR14-ISO8859-15)
FontTarget(courR18-ISO8859-15)
FontTarget(courR24-ISO8859-15)
FontTarget(helvB08-ISO8859-15)
FontTarget(helvB10-ISO8859-15)
FontTarget(helvB12-ISO8859-15)
FontTarget(helvB14-ISO8859-15)
FontTarget(helvB18-ISO8859-15)
FontTarget(helvB24-ISO8859-15)
FontTarget(helvBO08-ISO8859-15)
FontTarget(helvBO10-ISO8859-15)
FontTarget(helvBO12-ISO8859-15)
FontTarget(helvBO14-ISO8859-15)
FontTarget(helvBO18-ISO8859-15)
FontTarget(helvBO24-ISO8859-15)
FontTarget(helvO08-ISO8859-15)
FontTarget(helvO10-ISO8859-15)
FontTarget(helvO12-ISO8859-15)
FontTarget(helvO14-ISO8859-15)
FontTarget(helvO18-ISO8859-15)
FontTarget(helvO24-ISO8859-15)
FontTarget(helvR08-ISO8859-15)
FontTarget(helvR10-ISO8859-15)
FontTarget(helvR12-ISO8859-15)
FontTarget(helvR14-ISO8859-15)
FontTarget(helvR18-ISO8859-15)
FontTarget(helvR24-ISO8859-15)
FontTarget(ncenB08-ISO8859-15)
FontTarget(ncenB10-ISO8859-15)
FontTarget(ncenB12-ISO8859-15)
FontTarget(ncenB14-ISO8859-15)
FontTarget(ncenB18-ISO8859-15)
FontTarget(ncenB24-ISO8859-15)
FontTarget(ncenBI08-ISO8859-15)
FontTarget(ncenBI10-ISO8859-15)
FontTarget(ncenBI12-ISO8859-15)
FontTarget(ncenBI14-ISO8859-15)
FontTarget(ncenBI18-ISO8859-15)
FontTarget(ncenBI24-ISO8859-15)
FontTarget(ncenI08-ISO8859-15)
FontTarget(ncenI10-ISO8859-15)
FontTarget(ncenI12-ISO8859-15)
FontTarget(ncenI14-ISO8859-15)
FontTarget(ncenI18-ISO8859-15)
FontTarget(ncenI24-ISO8859-15)
FontTarget(ncenR08-ISO8859-15)
FontTarget(ncenR10-ISO8859-15)
FontTarget(ncenR12-ISO8859-15)
FontTarget(ncenR14-ISO8859-15)
FontTarget(ncenR18-ISO8859-15)
FontTarget(ncenR24-ISO8859-15)
FontTarget(timB08-ISO8859-15)
FontTarget(timB10-ISO8859-15)
FontTarget(timB12-ISO8859-15)
FontTarget(timB14-ISO8859-15)
FontTarget(timB18-ISO8859-15)
FontTarget(timB24-ISO8859-15)
FontTarget(timBI08-ISO8859-15)
FontTarget(timBI10-ISO8859-15)
FontTarget(timBI12-ISO8859-15)
FontTarget(timBI14-ISO8859-15)
FontTarget(timBI18-ISO8859-15)
FontTarget(timBI24-ISO8859-15)
FontTarget(timI08-ISO8859-15)
FontTarget(timI10-ISO8859-15)
FontTarget(timI12-ISO8859-15)
FontTarget(timI14-ISO8859-15)
FontTarget(timI18-ISO8859-15)
FontTarget(timI24-ISO8859-15)
FontTarget(timR08-ISO8859-15)
FontTarget(timR10-ISO8859-15)
FontTarget(timR12-ISO8859-15)
FontTarget(timR14-ISO8859-15)
FontTarget(timR18-ISO8859-15)
FontTarget(timR24-ISO8859-15)
FontTarget(luBIS08-ISO8859-15)
FontTarget(luBIS10-ISO8859-15)
FontTarget(luBIS12-ISO8859-15)
FontTarget(luBIS14-ISO8859-15)
FontTarget(luBIS18-ISO8859-15)
FontTarget(luBIS19-ISO8859-15)
FontTarget(luBIS24-ISO8859-15)
FontTarget(luBS08-ISO8859-15)
FontTarget(luBS10-ISO8859-15)
FontTarget(luBS12-ISO8859-15)
FontTarget(luBS14-ISO8859-15)
FontTarget(luBS18-ISO8859-15)
FontTarget(luBS19-ISO8859-15)
FontTarget(luBS24-ISO8859-15)
FontTarget(luIS08-ISO8859-15)
FontTarget(luIS10-ISO8859-15)
FontTarget(luIS12-ISO8859-15)
FontTarget(luIS14-ISO8859-15)
FontTarget(luIS18-ISO8859-15)
FontTarget(luIS19-ISO8859-15)
FontTarget(luIS24-ISO8859-15)
FontTarget(luRS08-ISO8859-15)
FontTarget(luRS10-ISO8859-15)
FontTarget(luRS12-ISO8859-15)
FontTarget(luRS14-ISO8859-15)
FontTarget(luRS18-ISO8859-15)
FontTarget(luRS19-ISO8859-15)
FontTarget(luRS24-ISO8859-15)
FontTarget(lubB08-ISO8859-15)
FontTarget(lubB10-ISO8859-15)
FontTarget(lubB12-ISO8859-15)
FontTarget(lubB14-ISO8859-15)
FontTarget(lubB18-ISO8859-15)
FontTarget(lubB19-ISO8859-15)
FontTarget(lubB24-ISO8859-15)
FontTarget(lubBI08-ISO8859-15)
FontTarget(lubBI10-ISO8859-15)
FontTarget(lubBI12-ISO8859-15)
FontTarget(lubBI14-ISO8859-15)
FontTarget(lubBI18-ISO8859-15)
FontTarget(lubBI19-ISO8859-15)
FontTarget(lubBI24-ISO8859-15)
FontTarget(lubI08-ISO8859-15)
FontTarget(lubI10-ISO8859-15)
FontTarget(lubI12-ISO8859-15)
FontTarget(lubI14-ISO8859-15)
FontTarget(lubI18-ISO8859-15)
FontTarget(lubI19-ISO8859-15)
FontTarget(lubI24-ISO8859-15)
FontTarget(lubR08-ISO8859-15)
FontTarget(lubR10-ISO8859-15)
FontTarget(lubR12-ISO8859-15)
FontTarget(lubR14-ISO8859-15)
FontTarget(lubR18-ISO8859-15)
FontTarget(lubR19-ISO8859-15)
FontTarget(lubR24-ISO8859-15)
FontTarget(lutBS08-ISO8859-15)
FontTarget(lutBS10-ISO8859-15)
FontTarget(lutBS12-ISO8859-15)
FontTarget(lutBS14-ISO8859-15)
FontTarget(lutBS18-ISO8859-15)
FontTarget(lutBS19-ISO8859-15)
FontTarget(lutBS24-ISO8859-15)
FontTarget(lutRS08-ISO8859-15)
FontTarget(lutRS10-ISO8859-15)
FontTarget(lutRS12-ISO8859-15)
FontTarget(lutRS14-ISO8859-15)
FontTarget(lutRS18-ISO8859-15)
FontTarget(lutRS19-ISO8859-15)
FontTarget(lutRS24-ISO8859-15)
FontTarget(UTBI__10-ISO8859-15)
FontTarget(UTBI__12-ISO8859-15)
FontTarget(UTBI__14-ISO8859-15)
FontTarget(UTBI__18-ISO8859-15)
FontTarget(UTBI__24-ISO8859-15)
FontTarget(UTB___10-ISO8859-15)
FontTarget(UTB___12-ISO8859-15)
FontTarget(UTB___14-ISO8859-15)
FontTarget(UTB___18-ISO8859-15)
FontTarget(UTB___24-ISO8859-15)
FontTarget(UTI___10-ISO8859-15)
FontTarget(UTI___12-ISO8859-15)
FontTarget(UTI___14-ISO8859-15)
FontTarget(UTI___18-ISO8859-15)
FontTarget(UTI___24-ISO8859-15)
FontTarget(UTRG__10-ISO8859-15)
FontTarget(UTRG__12-ISO8859-15)
FontTarget(UTRG__14-ISO8859-15)
FontTarget(UTRG__18-ISO8859-15)
FontTarget(UTRG__24-ISO8859-15)
#endif
#endif

#if DerivedUCSFonts
#if BuildISO8859_1Fonts
MakeBdfFontFromUCSMaster(courB08,ISO8859-1)
MakeBdfFontFromUCSMaster(courB10,ISO8859-1)
MakeBdfFontFromUCSMaster(courB12,ISO8859-1)
MakeBdfFontFromUCSMaster(courB14,ISO8859-1)
MakeBdfFontFromUCSMaster(courB18,ISO8859-1)
MakeBdfFontFromUCSMaster(courB24,ISO8859-1)
MakeBdfFontFromUCSMaster(courBO08,ISO8859-1)
MakeBdfFontFromUCSMaster(courBO10,ISO8859-1)
MakeBdfFontFromUCSMaster(courBO12,ISO8859-1)
MakeBdfFontFromUCSMaster(courBO14,ISO8859-1)
MakeBdfFontFromUCSMaster(courBO18,ISO8859-1)
MakeBdfFontFromUCSMaster(courBO24,ISO8859-1)
MakeBdfFontFromUCSMaster(courO08,ISO8859-1)
MakeBdfFontFromUCSMaster(courO10,ISO8859-1)
MakeBdfFontFromUCSMaster(courO12,ISO8859-1)
MakeBdfFontFromUCSMaster(courO14,ISO8859-1)
MakeBdfFontFromUCSMaster(courO18,ISO8859-1)
MakeBdfFontFromUCSMaster(courO24,ISO8859-1)
MakeBdfFontFromUCSMaster(courR08,ISO8859-1)
MakeBdfFontFromUCSMaster(courR10,ISO8859-1)
MakeBdfFontFromUCSMaster(courR12,ISO8859-1)
MakeBdfFontFromUCSMaster(courR14,ISO8859-1)
MakeBdfFontFromUCSMaster(courR18,ISO8859-1)
MakeBdfFontFromUCSMaster(courR24,ISO8859-1)
MakeBdfFontFromUCSMaster(helvB08,ISO8859-1)
MakeBdfFontFromUCSMaster(helvB10,ISO8859-1)
MakeBdfFontFromUCSMaster(helvB12,ISO8859-1)
MakeBdfFontFromUCSMaster(helvB14,ISO8859-1)
MakeBdfFontFromUCSMaster(helvB18,ISO8859-1)
MakeBdfFontFromUCSMaster(helvB24,ISO8859-1)
MakeBdfFontFromUCSMaster(helvBO08,ISO8859-1)
MakeBdfFontFromUCSMaster(helvBO10,ISO8859-1)
MakeBdfFontFromUCSMaster(helvBO12,ISO8859-1)
MakeBdfFontFromUCSMaster(helvBO14,ISO8859-1)
MakeBdfFontFromUCSMaster(helvBO18,ISO8859-1)
MakeBdfFontFromUCSMaster(helvBO24,ISO8859-1)
MakeBdfFontFromUCSMaster(helvO08,ISO8859-1)
MakeBdfFontFromUCSMaster(helvO10,ISO8859-1)
MakeBdfFontFromUCSMaster(helvO12,ISO8859-1)
MakeBdfFontFromUCSMaster(helvO14,ISO8859-1)
MakeBdfFontFromUCSMaster(helvO18,ISO8859-1)
MakeBdfFontFromUCSMaster(helvO24,ISO8859-1)
MakeBdfFontFromUCSMaster(helvR08,ISO8859-1)
MakeBdfFontFromUCSMaster(helvR10,ISO8859-1)
MakeBdfFontFromUCSMaster(helvR12,ISO8859-1)
MakeBdfFontFromUCSMaster(helvR14,ISO8859-1)
MakeBdfFontFromUCSMaster(helvR18,ISO8859-1)
MakeBdfFontFromUCSMaster(helvR24,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenB08,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenB10,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenB12,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenB14,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenB18,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenB24,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenBI08,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenBI10,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenBI12,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenBI14,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenBI18,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenBI24,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenI08,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenI10,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenI12,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenI14,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenI18,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenI24,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenR08,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenR10,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenR12,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenR14,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenR18,ISO8859-1)
MakeBdfFontFromUCSMaster(ncenR24,ISO8859-1)
MakeBdfFontFromUCSMaster(timB08,ISO8859-1)
MakeBdfFontFromUCSMaster(timB10,ISO8859-1)
MakeBdfFontFromUCSMaster(timB12,ISO8859-1)
MakeBdfFontFromUCSMaster(timB14,ISO8859-1)
MakeBdfFontFromUCSMaster(timB18,ISO8859-1)
MakeBdfFontFromUCSMaster(timB24,ISO8859-1)
MakeBdfFontFromUCSMaster(timBI08,ISO8859-1)
MakeBdfFontFromUCSMaster(timBI10,ISO8859-1)
MakeBdfFontFromUCSMaster(timBI12,ISO8859-1)
MakeBdfFontFromUCSMaster(timBI14,ISO8859-1)
MakeBdfFontFromUCSMaster(timBI18,ISO8859-1)
MakeBdfFontFromUCSMaster(timBI24,ISO8859-1)
MakeBdfFontFromUCSMaster(timI08,ISO8859-1)
MakeBdfFontFromUCSMaster(timI10,ISO8859-1)
MakeBdfFontFromUCSMaster(timI12,ISO8859-1)
MakeBdfFontFromUCSMaster(timI14,ISO8859-1)
MakeBdfFontFromUCSMaster(timI18,ISO8859-1)
MakeBdfFontFromUCSMaster(timI24,ISO8859-1)
MakeBdfFontFromUCSMaster(timR08,ISO8859-1)
MakeBdfFontFromUCSMaster(timR10,ISO8859-1)
MakeBdfFontFromUCSMaster(timR12,ISO8859-1)
MakeBdfFontFromUCSMaster(timR14,ISO8859-1)
MakeBdfFontFromUCSMaster(timR18,ISO8859-1)
MakeBdfFontFromUCSMaster(timR24,ISO8859-1)
MakeBdfFontFromUCSMaster(luBIS08,ISO8859-1)
MakeBdfFontFromUCSMaster(luBIS10,ISO8859-1)
MakeBdfFontFromUCSMaster(luBIS12,ISO8859-1)
MakeBdfFontFromUCSMaster(luBIS14,ISO8859-1)
MakeBdfFontFromUCSMaster(luBIS18,ISO8859-1)
MakeBdfFontFromUCSMaster(luBIS19,ISO8859-1)
MakeBdfFontFromUCSMaster(luBIS24,ISO8859-1)
MakeBdfFontFromUCSMaster(luBS08,ISO8859-1)
MakeBdfFontFromUCSMaster(luBS10,ISO8859-1)
MakeBdfFontFromUCSMaster(luBS12,ISO8859-1)
MakeBdfFontFromUCSMaster(luBS14,ISO8859-1)
MakeBdfFontFromUCSMaster(luBS18,ISO8859-1)
MakeBdfFontFromUCSMaster(luBS19,ISO8859-1)
MakeBdfFontFromUCSMaster(luBS24,ISO8859-1)
MakeBdfFontFromUCSMaster(luIS08,ISO8859-1)
MakeBdfFontFromUCSMaster(luIS10,ISO8859-1)
MakeBdfFontFromUCSMaster(luIS12,ISO8859-1)
MakeBdfFontFromUCSMaster(luIS14,ISO8859-1)
MakeBdfFontFromUCSMaster(luIS18,ISO8859-1)
MakeBdfFontFromUCSMaster(luIS19,ISO8859-1)
MakeBdfFontFromUCSMaster(luIS24,ISO8859-1)
MakeBdfFontFromUCSMaster(luRS08,ISO8859-1)
MakeBdfFontFromUCSMaster(luRS10,ISO8859-1)
MakeBdfFontFromUCSMaster(luRS12,ISO8859-1)
MakeBdfFontFromUCSMaster(luRS14,ISO8859-1)
MakeBdfFontFromUCSMaster(luRS18,ISO8859-1)
MakeBdfFontFromUCSMaster(luRS19,ISO8859-1)
MakeBdfFontFromUCSMaster(luRS24,ISO8859-1)
MakeBdfFontFromUCSMaster(lubB08,ISO8859-1)
MakeBdfFontFromUCSMaster(lubB10,ISO8859-1)
MakeBdfFontFromUCSMaster(lubB12,ISO8859-1)
MakeBdfFontFromUCSMaster(lubB14,ISO8859-1)
MakeBdfFontFromUCSMaster(lubB18,ISO8859-1)
MakeBdfFontFromUCSMaster(lubB19,ISO8859-1)
MakeBdfFontFromUCSMaster(lubB24,ISO8859-1)
MakeBdfFontFromUCSMaster(lubBI08,ISO8859-1)
MakeBdfFontFromUCSMaster(lubBI10,ISO8859-1)
MakeBdfFontFromUCSMaster(lubBI12,ISO8859-1)
MakeBdfFontFromUCSMaster(lubBI14,ISO8859-1)
MakeBdfFontFromUCSMaster(lubBI18,ISO8859-1)
MakeBdfFontFromUCSMaster(lubBI19,ISO8859-1)
MakeBdfFontFromUCSMaster(lubBI24,ISO8859-1)
MakeBdfFontFromUCSMaster(lubI08,ISO8859-1)
MakeBdfFontFromUCSMaster(lubI10,ISO8859-1)
MakeBdfFontFromUCSMaster(lubI12,ISO8859-1)
MakeBdfFontFromUCSMaster(lubI14,ISO8859-1)
MakeBdfFontFromUCSMaster(lubI18,ISO8859-1)
MakeBdfFontFromUCSMaster(lubI19,ISO8859-1)
MakeBdfFontFromUCSMaster(lubI24,ISO8859-1)
MakeBdfFontFromUCSMaster(lubR08,ISO8859-1)
MakeBdfFontFromUCSMaster(lubR10,ISO8859-1)
MakeBdfFontFromUCSMaster(lubR12,ISO8859-1)
MakeBdfFontFromUCSMaster(lubR14,ISO8859-1)
MakeBdfFontFromUCSMaster(lubR18,ISO8859-1)
MakeBdfFontFromUCSMaster(lubR19,ISO8859-1)
MakeBdfFontFromUCSMaster(lubR24,ISO8859-1)
MakeBdfFontFromUCSMaster(lutBS08,ISO8859-1)
MakeBdfFontFromUCSMaster(lutBS10,ISO8859-1)
MakeBdfFontFromUCSMaster(lutBS12,ISO8859-1)
MakeBdfFontFromUCSMaster(lutBS14,ISO8859-1)
MakeBdfFontFromUCSMaster(lutBS18,ISO8859-1)
MakeBdfFontFromUCSMaster(lutBS19,ISO8859-1)
MakeBdfFontFromUCSMaster(lutBS24,ISO8859-1)
MakeBdfFontFromUCSMaster(lutRS08,ISO8859-1)
MakeBdfFontFromUCSMaster(lutRS10,ISO8859-1)
MakeBdfFontFromUCSMaster(lutRS12,ISO8859-1)
MakeBdfFontFromUCSMaster(lutRS14,ISO8859-1)
MakeBdfFontFromUCSMaster(lutRS18,ISO8859-1)
MakeBdfFontFromUCSMaster(lutRS19,ISO8859-1)
MakeBdfFontFromUCSMaster(lutRS24,ISO8859-1)
MakeBdfFontFromUCSMaster(UTBI__10,ISO8859-1)
MakeBdfFontFromUCSMaster(UTBI__12,ISO8859-1)
MakeBdfFontFromUCSMaster(UTBI__14,ISO8859-1)
MakeBdfFontFromUCSMaster(UTBI__18,ISO8859-1)
MakeBdfFontFromUCSMaster(UTBI__24,ISO8859-1)
MakeBdfFontFromUCSMaster(UTB___10,ISO8859-1)
MakeBdfFontFromUCSMaster(UTB___12,ISO8859-1)
MakeBdfFontFromUCSMaster(UTB___14,ISO8859-1)
MakeBdfFontFromUCSMaster(UTB___18,ISO8859-1)
MakeBdfFontFromUCSMaster(UTB___24,ISO8859-1)
MakeBdfFontFromUCSMaster(UTI___10,ISO8859-1)
MakeBdfFontFromUCSMaster(UTI___12,ISO8859-1)
MakeBdfFontFromUCSMaster(UTI___14,ISO8859-1)
MakeBdfFontFromUCSMaster(UTI___18,ISO8859-1)
MakeBdfFontFromUCSMaster(UTI___24,ISO8859-1)
MakeBdfFontFromUCSMaster(UTRG__10,ISO8859-1)
MakeBdfFontFromUCSMaster(UTRG__12,ISO8859-1)
MakeBdfFontFromUCSMaster(UTRG__14,ISO8859-1)
MakeBdfFontFromUCSMaster(UTRG__18,ISO8859-1)
MakeBdfFontFromUCSMaster(UTRG__24,ISO8859-1)
#endif

#if BuildISO8859_2Fonts
MakeBdfFontFromUCSMaster(courB08,ISO8859-2)
MakeBdfFontFromUCSMaster(courB10,ISO8859-2)
MakeBdfFontFromUCSMaster(courB12,ISO8859-2)
MakeBdfFontFromUCSMaster(courB14,ISO8859-2)
MakeBdfFontFromUCSMaster(courB18,ISO8859-2)
MakeBdfFontFromUCSMaster(courB24,ISO8859-2)
MakeBdfFontFromUCSMaster(courBO08,ISO8859-2)
MakeBdfFontFromUCSMaster(courBO10,ISO8859-2)
MakeBdfFontFromUCSMaster(courBO12,ISO8859-2)
MakeBdfFontFromUCSMaster(courBO14,ISO8859-2)
MakeBdfFontFromUCSMaster(courBO18,ISO8859-2)
MakeBdfFontFromUCSMaster(courBO24,ISO8859-2)
MakeBdfFontFromUCSMaster(courO08,ISO8859-2)
MakeBdfFontFromUCSMaster(courO10,ISO8859-2)
MakeBdfFontFromUCSMaster(courO12,ISO8859-2)
MakeBdfFontFromUCSMaster(courO14,ISO8859-2)
MakeBdfFontFromUCSMaster(courO18,ISO8859-2)
MakeBdfFontFromUCSMaster(courO24,ISO8859-2)
MakeBdfFontFromUCSMaster(courR08,ISO8859-2)
MakeBdfFontFromUCSMaster(courR10,ISO8859-2)
MakeBdfFontFromUCSMaster(courR12,ISO8859-2)
MakeBdfFontFromUCSMaster(courR14,ISO8859-2)
MakeBdfFontFromUCSMaster(courR18,ISO8859-2)
MakeBdfFontFromUCSMaster(courR24,ISO8859-2)
MakeBdfFontFromUCSMaster(helvB08,ISO8859-2)
MakeBdfFontFromUCSMaster(helvB10,ISO8859-2)
MakeBdfFontFromUCSMaster(helvB12,ISO8859-2)
MakeBdfFontFromUCSMaster(helvB14,ISO8859-2)
MakeBdfFontFromUCSMaster(helvB18,ISO8859-2)
MakeBdfFontFromUCSMaster(helvB24,ISO8859-2)
MakeBdfFontFromUCSMaster(helvBO08,ISO8859-2)
MakeBdfFontFromUCSMaster(helvBO10,ISO8859-2)
MakeBdfFontFromUCSMaster(helvBO12,ISO8859-2)
MakeBdfFontFromUCSMaster(helvBO14,ISO8859-2)
MakeBdfFontFromUCSMaster(helvBO18,ISO8859-2)
MakeBdfFontFromUCSMaster(helvBO24,ISO8859-2)
MakeBdfFontFromUCSMaster(helvO08,ISO8859-2)
MakeBdfFontFromUCSMaster(helvO10,ISO8859-2)
MakeBdfFontFromUCSMaster(helvO12,ISO8859-2)
MakeBdfFontFromUCSMaster(helvO14,ISO8859-2)
MakeBdfFontFromUCSMaster(helvO18,ISO8859-2)
MakeBdfFontFromUCSMaster(helvO24,ISO8859-2)
MakeBdfFontFromUCSMaster(helvR08,ISO8859-2)
MakeBdfFontFromUCSMaster(helvR10,ISO8859-2)
MakeBdfFontFromUCSMaster(helvR12,ISO8859-2)
MakeBdfFontFromUCSMaster(helvR14,ISO8859-2)
MakeBdfFontFromUCSMaster(helvR18,ISO8859-2)
MakeBdfFontFromUCSMaster(helvR24,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenB08,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenB10,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenB12,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenB14,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenB18,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenB24,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenBI08,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenBI10,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenBI12,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenBI14,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenBI18,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenBI24,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenI08,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenI10,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenI12,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenI14,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenI18,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenI24,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenR08,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenR10,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenR12,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenR14,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenR18,ISO8859-2)
MakeBdfFontFromUCSMaster(ncenR24,ISO8859-2)
MakeBdfFontFromUCSMaster(timB08,ISO8859-2)
MakeBdfFontFromUCSMaster(timB10,ISO8859-2)
MakeBdfFontFromUCSMaster(timB12,ISO8859-2)
MakeBdfFontFromUCSMaster(timB14,ISO8859-2)
MakeBdfFontFromUCSMaster(timB18,ISO8859-2)
MakeBdfFontFromUCSMaster(timB24,ISO8859-2)
MakeBdfFontFromUCSMaster(timBI08,ISO8859-2)
MakeBdfFontFromUCSMaster(timBI10,ISO8859-2)
MakeBdfFontFromUCSMaster(timBI12,ISO8859-2)
MakeBdfFontFromUCSMaster(timBI14,ISO8859-2)
MakeBdfFontFromUCSMaster(timBI18,ISO8859-2)
MakeBdfFontFromUCSMaster(timBI24,ISO8859-2)
MakeBdfFontFromUCSMaster(timI08,ISO8859-2)
MakeBdfFontFromUCSMaster(timI10,ISO8859-2)
MakeBdfFontFromUCSMaster(timI12,ISO8859-2)
MakeBdfFontFromUCSMaster(timI14,ISO8859-2)
MakeBdfFontFromUCSMaster(timI18,ISO8859-2)
MakeBdfFontFromUCSMaster(timI24,ISO8859-2)
MakeBdfFontFromUCSMaster(timR08,ISO8859-2)
MakeBdfFontFromUCSMaster(timR10,ISO8859-2)
MakeBdfFontFromUCSMaster(timR12,ISO8859-2)
MakeBdfFontFromUCSMaster(timR14,ISO8859-2)
MakeBdfFontFromUCSMaster(timR18,ISO8859-2)
MakeBdfFontFromUCSMaster(timR24,ISO8859-2)
MakeBdfFontFromUCSMaster(luBIS08,ISO8859-2)
MakeBdfFontFromUCSMaster(luBIS10,ISO8859-2)
MakeBdfFontFromUCSMaster(luBIS12,ISO8859-2)
MakeBdfFontFromUCSMaster(luBIS14,ISO8859-2)
MakeBdfFontFromUCSMaster(luBIS18,ISO8859-2)
MakeBdfFontFromUCSMaster(luBIS19,ISO8859-2)
MakeBdfFontFromUCSMaster(luBIS24,ISO8859-2)
MakeBdfFontFromUCSMaster(luBS08,ISO8859-2)
MakeBdfFontFromUCSMaster(luBS10,ISO8859-2)
MakeBdfFontFromUCSMaster(luBS12,ISO8859-2)
MakeBdfFontFromUCSMaster(luBS14,ISO8859-2)
MakeBdfFontFromUCSMaster(luBS18,ISO8859-2)
MakeBdfFontFromUCSMaster(luBS19,ISO8859-2)
MakeBdfFontFromUCSMaster(luBS24,ISO8859-2)
MakeBdfFontFromUCSMaster(luIS08,ISO8859-2)
MakeBdfFontFromUCSMaster(luIS10,ISO8859-2)
MakeBdfFontFromUCSMaster(luIS12,ISO8859-2)
MakeBdfFontFromUCSMaster(luIS14,ISO8859-2)
MakeBdfFontFromUCSMaster(luIS18,ISO8859-2)
MakeBdfFontFromUCSMaster(luIS19,ISO8859-2)
MakeBdfFontFromUCSMaster(luIS24,ISO8859-2)
MakeBdfFontFromUCSMaster(luRS08,ISO8859-2)
MakeBdfFontFromUCSMaster(luRS10,ISO8859-2)
MakeBdfFontFromUCSMaster(luRS12,ISO8859-2)
MakeBdfFontFromUCSMaster(luRS14,ISO8859-2)
MakeBdfFontFromUCSMaster(luRS18,ISO8859-2)
MakeBdfFontFromUCSMaster(luRS19,ISO8859-2)
MakeBdfFontFromUCSMaster(luRS24,ISO8859-2)
MakeBdfFontFromUCSMaster(lubB08,ISO8859-2)
MakeBdfFontFromUCSMaster(lubB10,ISO8859-2)
MakeBdfFontFromUCSMaster(lubB12,ISO8859-2)
MakeBdfFontFromUCSMaster(lubB14,ISO8859-2)
MakeBdfFontFromUCSMaster(lubB18,ISO8859-2)
MakeBdfFontFromUCSMaster(lubB19,ISO8859-2)
MakeBdfFontFromUCSMaster(lubB24,ISO8859-2)
MakeBdfFontFromUCSMaster(lubBI08,ISO8859-2)
MakeBdfFontFromUCSMaster(lubBI10,ISO8859-2)
MakeBdfFontFromUCSMaster(lubBI12,ISO8859-2)
MakeBdfFontFromUCSMaster(lubBI14,ISO8859-2)
MakeBdfFontFromUCSMaster(lubBI18,ISO8859-2)
MakeBdfFontFromUCSMaster(lubBI19,ISO8859-2)
MakeBdfFontFromUCSMaster(lubBI24,ISO8859-2)
MakeBdfFontFromUCSMaster(lubI08,ISO8859-2)
MakeBdfFontFromUCSMaster(lubI10,ISO8859-2)
MakeBdfFontFromUCSMaster(lubI12,ISO8859-2)
MakeBdfFontFromUCSMaster(lubI14,ISO8859-2)
MakeBdfFontFromUCSMaster(lubI18,ISO8859-2)
MakeBdfFontFromUCSMaster(lubI19,ISO8859-2)
MakeBdfFontFromUCSMaster(lubI24,ISO8859-2)
MakeBdfFontFromUCSMaster(lubR08,ISO8859-2)
MakeBdfFontFromUCSMaster(lubR10,ISO8859-2)
MakeBdfFontFromUCSMaster(lubR12,ISO8859-2)
MakeBdfFontFromUCSMaster(lubR14,ISO8859-2)
MakeBdfFontFromUCSMaster(lubR18,ISO8859-2)
MakeBdfFontFromUCSMaster(lubR19,ISO8859-2)
MakeBdfFontFromUCSMaster(lubR24,ISO8859-2)
MakeBdfFontFromUCSMaster(lutBS08,ISO8859-2)
MakeBdfFontFromUCSMaster(lutBS10,ISO8859-2)
MakeBdfFontFromUCSMaster(lutBS12,ISO8859-2)
MakeBdfFontFromUCSMaster(lutBS14,ISO8859-2)
MakeBdfFontFromUCSMaster(lutBS18,ISO8859-2)
MakeBdfFontFromUCSMaster(lutBS19,ISO8859-2)
MakeBdfFontFromUCSMaster(lutBS24,ISO8859-2)
MakeBdfFontFromUCSMaster(lutRS08,ISO8859-2)
MakeBdfFontFromUCSMaster(lutRS10,ISO8859-2)
MakeBdfFontFromUCSMaster(lutRS12,ISO8859-2)
MakeBdfFontFromUCSMaster(lutRS14,ISO8859-2)
MakeBdfFontFromUCSMaster(lutRS18,ISO8859-2)
MakeBdfFontFromUCSMaster(lutRS19,ISO8859-2)
MakeBdfFontFromUCSMaster(lutRS24,ISO8859-2)
MakeBdfFontFromUCSMaster(UTBI__10,ISO8859-2)
MakeBdfFontFromUCSMaster(UTBI__12,ISO8859-2)
MakeBdfFontFromUCSMaster(UTBI__14,ISO8859-2)
MakeBdfFontFromUCSMaster(UTBI__18,ISO8859-2)
MakeBdfFontFromUCSMaster(UTBI__24,ISO8859-2)
MakeBdfFontFromUCSMaster(UTB___10,ISO8859-2)
MakeBdfFontFromUCSMaster(UTB___12,ISO8859-2)
MakeBdfFontFromUCSMaster(UTB___14,ISO8859-2)
MakeBdfFontFromUCSMaster(UTB___18,ISO8859-2)
MakeBdfFontFromUCSMaster(UTB___24,ISO8859-2)
MakeBdfFontFromUCSMaster(UTI___10,ISO8859-2)
MakeBdfFontFromUCSMaster(UTI___12,ISO8859-2)
MakeBdfFontFromUCSMaster(UTI___14,ISO8859-2)
MakeBdfFontFromUCSMaster(UTI___18,ISO8859-2)
MakeBdfFontFromUCSMaster(UTI___24,ISO8859-2)
MakeBdfFontFromUCSMaster(UTRG__10,ISO8859-2)
MakeBdfFontFromUCSMaster(UTRG__12,ISO8859-2)
MakeBdfFontFromUCSMaster(UTRG__14,ISO8859-2)
MakeBdfFontFromUCSMaster(UTRG__18,ISO8859-2)
MakeBdfFontFromUCSMaster(UTRG__24,ISO8859-2)
#endif

#if BuildISO8859_3Fonts
MakeBdfFontFromUCSMaster(courB08,ISO8859-3)
MakeBdfFontFromUCSMaster(courB10,ISO8859-3)
MakeBdfFontFromUCSMaster(courB12,ISO8859-3)
MakeBdfFontFromUCSMaster(courB14,ISO8859-3)
MakeBdfFontFromUCSMaster(courB18,ISO8859-3)
MakeBdfFontFromUCSMaster(courB24,ISO8859-3)
MakeBdfFontFromUCSMaster(courBO08,ISO8859-3)
MakeBdfFontFromUCSMaster(courBO10,ISO8859-3)
MakeBdfFontFromUCSMaster(courBO12,ISO8859-3)
MakeBdfFontFromUCSMaster(courBO14,ISO8859-3)
MakeBdfFontFromUCSMaster(courBO18,ISO8859-3)
MakeBdfFontFromUCSMaster(courBO24,ISO8859-3)
MakeBdfFontFromUCSMaster(courO08,ISO8859-3)
MakeBdfFontFromUCSMaster(courO10,ISO8859-3)
MakeBdfFontFromUCSMaster(courO12,ISO8859-3)
MakeBdfFontFromUCSMaster(courO14,ISO8859-3)
MakeBdfFontFromUCSMaster(courO18,ISO8859-3)
MakeBdfFontFromUCSMaster(courO24,ISO8859-3)
MakeBdfFontFromUCSMaster(courR08,ISO8859-3)
MakeBdfFontFromUCSMaster(courR10,ISO8859-3)
MakeBdfFontFromUCSMaster(courR12,ISO8859-3)
MakeBdfFontFromUCSMaster(courR14,ISO8859-3)
MakeBdfFontFromUCSMaster(courR18,ISO8859-3)
MakeBdfFontFromUCSMaster(courR24,ISO8859-3)
MakeBdfFontFromUCSMaster(helvB08,ISO8859-3)
MakeBdfFontFromUCSMaster(helvB10,ISO8859-3)
MakeBdfFontFromUCSMaster(helvB12,ISO8859-3)
MakeBdfFontFromUCSMaster(helvB14,ISO8859-3)
MakeBdfFontFromUCSMaster(helvB18,ISO8859-3)
MakeBdfFontFromUCSMaster(helvB24,ISO8859-3)
MakeBdfFontFromUCSMaster(helvBO08,ISO8859-3)
MakeBdfFontFromUCSMaster(helvBO10,ISO8859-3)
MakeBdfFontFromUCSMaster(helvBO12,ISO8859-3)
MakeBdfFontFromUCSMaster(helvBO14,ISO8859-3)
MakeBdfFontFromUCSMaster(helvBO18,ISO8859-3)
MakeBdfFontFromUCSMaster(helvBO24,ISO8859-3)
MakeBdfFontFromUCSMaster(helvO08,ISO8859-3)
MakeBdfFontFromUCSMaster(helvO10,ISO8859-3)
MakeBdfFontFromUCSMaster(helvO12,ISO8859-3)
MakeBdfFontFromUCSMaster(helvO14,ISO8859-3)
MakeBdfFontFromUCSMaster(helvO18,ISO8859-3)
MakeBdfFontFromUCSMaster(helvO24,ISO8859-3)
MakeBdfFontFromUCSMaster(helvR08,ISO8859-3)
MakeBdfFontFromUCSMaster(helvR10,ISO8859-3)
MakeBdfFontFromUCSMaster(helvR12,ISO8859-3)
MakeBdfFontFromUCSMaster(helvR14,ISO8859-3)
MakeBdfFontFromUCSMaster(helvR18,ISO8859-3)
MakeBdfFontFromUCSMaster(helvR24,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenB08,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenB10,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenB12,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenB14,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenB18,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenB24,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenBI08,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenBI10,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenBI12,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenBI14,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenBI18,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenBI24,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenI08,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenI10,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenI12,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenI14,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenI18,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenI24,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenR08,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenR10,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenR12,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenR14,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenR18,ISO8859-3)
MakeBdfFontFromUCSMaster(ncenR24,ISO8859-3)
MakeBdfFontFromUCSMaster(timB08,ISO8859-3)
MakeBdfFontFromUCSMaster(timB10,ISO8859-3)
MakeBdfFontFromUCSMaster(timB12,ISO8859-3)
MakeBdfFontFromUCSMaster(timB14,ISO8859-3)
MakeBdfFontFromUCSMaster(timB18,ISO8859-3)
MakeBdfFontFromUCSMaster(timB24,ISO8859-3)
MakeBdfFontFromUCSMaster(timBI08,ISO8859-3)
MakeBdfFontFromUCSMaster(timBI10,ISO8859-3)
MakeBdfFontFromUCSMaster(timBI12,ISO8859-3)
MakeBdfFontFromUCSMaster(timBI14,ISO8859-3)
MakeBdfFontFromUCSMaster(timBI18,ISO8859-3)
MakeBdfFontFromUCSMaster(timBI24,ISO8859-3)
MakeBdfFontFromUCSMaster(timI08,ISO8859-3)
MakeBdfFontFromUCSMaster(timI10,ISO8859-3)
MakeBdfFontFromUCSMaster(timI12,ISO8859-3)
MakeBdfFontFromUCSMaster(timI14,ISO8859-3)
MakeBdfFontFromUCSMaster(timI18,ISO8859-3)
MakeBdfFontFromUCSMaster(timI24,ISO8859-3)
MakeBdfFontFromUCSMaster(timR08,ISO8859-3)
MakeBdfFontFromUCSMaster(timR10,ISO8859-3)
MakeBdfFontFromUCSMaster(timR12,ISO8859-3)
MakeBdfFontFromUCSMaster(timR14,ISO8859-3)
MakeBdfFontFromUCSMaster(timR18,ISO8859-3)
MakeBdfFontFromUCSMaster(timR24,ISO8859-3)
MakeBdfFontFromUCSMaster(luBIS08,ISO8859-3)
MakeBdfFontFromUCSMaster(luBIS10,ISO8859-3)
MakeBdfFontFromUCSMaster(luBIS12,ISO8859-3)
MakeBdfFontFromUCSMaster(luBIS14,ISO8859-3)
MakeBdfFontFromUCSMaster(luBIS18,ISO8859-3)
MakeBdfFontFromUCSMaster(luBIS19,ISO8859-3)
MakeBdfFontFromUCSMaster(luBIS24,ISO8859-3)
MakeBdfFontFromUCSMaster(luBS08,ISO8859-3)
MakeBdfFontFromUCSMaster(luBS10,ISO8859-3)
MakeBdfFontFromUCSMaster(luBS12,ISO8859-3)
MakeBdfFontFromUCSMaster(luBS14,ISO8859-3)
MakeBdfFontFromUCSMaster(luBS18,ISO8859-3)
MakeBdfFontFromUCSMaster(luBS19,ISO8859-3)
MakeBdfFontFromUCSMaster(luBS24,ISO8859-3)
MakeBdfFontFromUCSMaster(luIS08,ISO8859-3)
MakeBdfFontFromUCSMaster(luIS10,ISO8859-3)
MakeBdfFontFromUCSMaster(luIS12,ISO8859-3)
MakeBdfFontFromUCSMaster(luIS14,ISO8859-3)
MakeBdfFontFromUCSMaster(luIS18,ISO8859-3)
MakeBdfFontFromUCSMaster(luIS19,ISO8859-3)
MakeBdfFontFromUCSMaster(luIS24,ISO8859-3)
MakeBdfFontFromUCSMaster(luRS08,ISO8859-3)
MakeBdfFontFromUCSMaster(luRS10,ISO8859-3)
MakeBdfFontFromUCSMaster(luRS12,ISO8859-3)
MakeBdfFontFromUCSMaster(luRS14,ISO8859-3)
MakeBdfFontFromUCSMaster(luRS18,ISO8859-3)
MakeBdfFontFromUCSMaster(luRS19,ISO8859-3)
MakeBdfFontFromUCSMaster(luRS24,ISO8859-3)
MakeBdfFontFromUCSMaster(lubB08,ISO8859-3)
MakeBdfFontFromUCSMaster(lubB10,ISO8859-3)
MakeBdfFontFromUCSMaster(lubB12,ISO8859-3)
MakeBdfFontFromUCSMaster(lubB14,ISO8859-3)
MakeBdfFontFromUCSMaster(lubB18,ISO8859-3)
MakeBdfFontFromUCSMaster(lubB19,ISO8859-3)
MakeBdfFontFromUCSMaster(lubB24,ISO8859-3)
MakeBdfFontFromUCSMaster(lubBI08,ISO8859-3)
MakeBdfFontFromUCSMaster(lubBI10,ISO8859-3)
MakeBdfFontFromUCSMaster(lubBI12,ISO8859-3)
MakeBdfFontFromUCSMaster(lubBI14,ISO8859-3)
MakeBdfFontFromUCSMaster(lubBI18,ISO8859-3)
MakeBdfFontFromUCSMaster(lubBI19,ISO8859-3)
MakeBdfFontFromUCSMaster(lubBI24,ISO8859-3)
MakeBdfFontFromUCSMaster(lubI08,ISO8859-3)
MakeBdfFontFromUCSMaster(lubI10,ISO8859-3)
MakeBdfFontFromUCSMaster(lubI12,ISO8859-3)
MakeBdfFontFromUCSMaster(lubI14,ISO8859-3)
MakeBdfFontFromUCSMaster(lubI18,ISO8859-3)
MakeBdfFontFromUCSMaster(lubI19,ISO8859-3)
MakeBdfFontFromUCSMaster(lubI24,ISO8859-3)
MakeBdfFontFromUCSMaster(lubR08,ISO8859-3)
MakeBdfFontFromUCSMaster(lubR10,ISO8859-3)
MakeBdfFontFromUCSMaster(lubR12,ISO8859-3)
MakeBdfFontFromUCSMaster(lubR14,ISO8859-3)
MakeBdfFontFromUCSMaster(lubR18,ISO8859-3)
MakeBdfFontFromUCSMaster(lubR19,ISO8859-3)
MakeBdfFontFromUCSMaster(lubR24,ISO8859-3)
MakeBdfFontFromUCSMaster(lutBS08,ISO8859-3)
MakeBdfFontFromUCSMaster(lutBS10,ISO8859-3)
MakeBdfFontFromUCSMaster(lutBS12,ISO8859-3)
MakeBdfFontFromUCSMaster(lutBS14,ISO8859-3)
MakeBdfFontFromUCSMaster(lutBS18,ISO8859-3)
MakeBdfFontFromUCSMaster(lutBS19,ISO8859-3)
MakeBdfFontFromUCSMaster(lutBS24,ISO8859-3)
MakeBdfFontFromUCSMaster(lutRS08,ISO8859-3)
MakeBdfFontFromUCSMaster(lutRS10,ISO8859-3)
MakeBdfFontFromUCSMaster(lutRS12,ISO8859-3)
MakeBdfFontFromUCSMaster(lutRS14,ISO8859-3)
MakeBdfFontFromUCSMaster(lutRS18,ISO8859-3)
MakeBdfFontFromUCSMaster(lutRS19,ISO8859-3)
MakeBdfFontFromUCSMaster(lutRS24,ISO8859-3)
MakeBdfFontFromUCSMaster(UTBI__10,ISO8859-3)
MakeBdfFontFromUCSMaster(UTBI__12,ISO8859-3)
MakeBdfFontFromUCSMaster(UTBI__14,ISO8859-3)
MakeBdfFontFromUCSMaster(UTBI__18,ISO8859-3)
MakeBdfFontFromUCSMaster(UTBI__24,ISO8859-3)
MakeBdfFontFromUCSMaster(UTB___10,ISO8859-3)
MakeBdfFontFromUCSMaster(UTB___12,ISO8859-3)
MakeBdfFontFromUCSMaster(UTB___14,ISO8859-3)
MakeBdfFontFromUCSMaster(UTB___18,ISO8859-3)
MakeBdfFontFromUCSMaster(UTB___24,ISO8859-3)
MakeBdfFontFromUCSMaster(UTI___10,ISO8859-3)
MakeBdfFontFromUCSMaster(UTI___12,ISO8859-3)
MakeBdfFontFromUCSMaster(UTI___14,ISO8859-3)
MakeBdfFontFromUCSMaster(UTI___18,ISO8859-3)
MakeBdfFontFromUCSMaster(UTI___24,ISO8859-3)
MakeBdfFontFromUCSMaster(UTRG__10,ISO8859-3)
MakeBdfFontFromUCSMaster(UTRG__12,ISO8859-3)
MakeBdfFontFromUCSMaster(UTRG__14,ISO8859-3)
MakeBdfFontFromUCSMaster(UTRG__18,ISO8859-3)
MakeBdfFontFromUCSMaster(UTRG__24,ISO8859-3)
#endif

#if BuildISO8859_4Fonts
MakeBdfFontFromUCSMaster(courB08,ISO8859-4)
MakeBdfFontFromUCSMaster(courB10,ISO8859-4)
MakeBdfFontFromUCSMaster(courB12,ISO8859-4)
MakeBdfFontFromUCSMaster(courB14,ISO8859-4)
MakeBdfFontFromUCSMaster(courB18,ISO8859-4)
MakeBdfFontFromUCSMaster(courB24,ISO8859-4)
MakeBdfFontFromUCSMaster(courBO08,ISO8859-4)
MakeBdfFontFromUCSMaster(courBO10,ISO8859-4)
MakeBdfFontFromUCSMaster(courBO12,ISO8859-4)
MakeBdfFontFromUCSMaster(courBO14,ISO8859-4)
MakeBdfFontFromUCSMaster(courBO18,ISO8859-4)
MakeBdfFontFromUCSMaster(courBO24,ISO8859-4)
MakeBdfFontFromUCSMaster(courO08,ISO8859-4)
MakeBdfFontFromUCSMaster(courO10,ISO8859-4)
MakeBdfFontFromUCSMaster(courO12,ISO8859-4)
MakeBdfFontFromUCSMaster(courO14,ISO8859-4)
MakeBdfFontFromUCSMaster(courO18,ISO8859-4)
MakeBdfFontFromUCSMaster(courO24,ISO8859-4)
MakeBdfFontFromUCSMaster(courR08,ISO8859-4)
MakeBdfFontFromUCSMaster(courR10,ISO8859-4)
MakeBdfFontFromUCSMaster(courR12,ISO8859-4)
MakeBdfFontFromUCSMaster(courR14,ISO8859-4)
MakeBdfFontFromUCSMaster(courR18,ISO8859-4)
MakeBdfFontFromUCSMaster(courR24,ISO8859-4)
MakeBdfFontFromUCSMaster(helvB08,ISO8859-4)
MakeBdfFontFromUCSMaster(helvB10,ISO8859-4)
MakeBdfFontFromUCSMaster(helvB12,ISO8859-4)
MakeBdfFontFromUCSMaster(helvB14,ISO8859-4)
MakeBdfFontFromUCSMaster(helvB18,ISO8859-4)
MakeBdfFontFromUCSMaster(helvB24,ISO8859-4)
MakeBdfFontFromUCSMaster(helvBO08,ISO8859-4)
MakeBdfFontFromUCSMaster(helvBO10,ISO8859-4)
MakeBdfFontFromUCSMaster(helvBO12,ISO8859-4)
MakeBdfFontFromUCSMaster(helvBO14,ISO8859-4)
MakeBdfFontFromUCSMaster(helvBO18,ISO8859-4)
MakeBdfFontFromUCSMaster(helvBO24,ISO8859-4)
MakeBdfFontFromUCSMaster(helvO08,ISO8859-4)
MakeBdfFontFromUCSMaster(helvO10,ISO8859-4)
MakeBdfFontFromUCSMaster(helvO12,ISO8859-4)
MakeBdfFontFromUCSMaster(helvO14,ISO8859-4)
MakeBdfFontFromUCSMaster(helvO18,ISO8859-4)
MakeBdfFontFromUCSMaster(helvO24,ISO8859-4)
MakeBdfFontFromUCSMaster(helvR08,ISO8859-4)
MakeBdfFontFromUCSMaster(helvR10,ISO8859-4)
MakeBdfFontFromUCSMaster(helvR12,ISO8859-4)
MakeBdfFontFromUCSMaster(helvR14,ISO8859-4)
MakeBdfFontFromUCSMaster(helvR18,ISO8859-4)
MakeBdfFontFromUCSMaster(helvR24,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenB08,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenB10,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenB12,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenB14,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenB18,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenB24,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenBI08,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenBI10,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenBI12,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenBI14,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenBI18,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenBI24,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenI08,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenI10,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenI12,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenI14,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenI18,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenI24,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenR08,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenR10,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenR12,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenR14,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenR18,ISO8859-4)
MakeBdfFontFromUCSMaster(ncenR24,ISO8859-4)
MakeBdfFontFromUCSMaster(timB08,ISO8859-4)
MakeBdfFontFromUCSMaster(timB10,ISO8859-4)
MakeBdfFontFromUCSMaster(timB12,ISO8859-4)
MakeBdfFontFromUCSMaster(timB14,ISO8859-4)
MakeBdfFontFromUCSMaster(timB18,ISO8859-4)
MakeBdfFontFromUCSMaster(timB24,ISO8859-4)
MakeBdfFontFromUCSMaster(timBI08,ISO8859-4)
MakeBdfFontFromUCSMaster(timBI10,ISO8859-4)
MakeBdfFontFromUCSMaster(timBI12,ISO8859-4)
MakeBdfFontFromUCSMaster(timBI14,ISO8859-4)
MakeBdfFontFromUCSMaster(timBI18,ISO8859-4)
MakeBdfFontFromUCSMaster(timBI24,ISO8859-4)
MakeBdfFontFromUCSMaster(timI08,ISO8859-4)
MakeBdfFontFromUCSMaster(timI10,ISO8859-4)
MakeBdfFontFromUCSMaster(timI12,ISO8859-4)
MakeBdfFontFromUCSMaster(timI14,ISO8859-4)
MakeBdfFontFromUCSMaster(timI18,ISO8859-4)
MakeBdfFontFromUCSMaster(timI24,ISO8859-4)
MakeBdfFontFromUCSMaster(timR08,ISO8859-4)
MakeBdfFontFromUCSMaster(timR10,ISO8859-4)
MakeBdfFontFromUCSMaster(timR12,ISO8859-4)
MakeBdfFontFromUCSMaster(timR14,ISO8859-4)
MakeBdfFontFromUCSMaster(timR18,ISO8859-4)
MakeBdfFontFromUCSMaster(timR24,ISO8859-4)
MakeBdfFontFromUCSMaster(luBIS08,ISO8859-4)
MakeBdfFontFromUCSMaster(luBIS10,ISO8859-4)
MakeBdfFontFromUCSMaster(luBIS12,ISO8859-4)
MakeBdfFontFromUCSMaster(luBIS14,ISO8859-4)
MakeBdfFontFromUCSMaster(luBIS18,ISO8859-4)
MakeBdfFontFromUCSMaster(luBIS19,ISO8859-4)
MakeBdfFontFromUCSMaster(luBIS24,ISO8859-4)
MakeBdfFontFromUCSMaster(luBS08,ISO8859-4)
MakeBdfFontFromUCSMaster(luBS10,ISO8859-4)
MakeBdfFontFromUCSMaster(luBS12,ISO8859-4)
MakeBdfFontFromUCSMaster(luBS14,ISO8859-4)
MakeBdfFontFromUCSMaster(luBS18,ISO8859-4)
MakeBdfFontFromUCSMaster(luBS19,ISO8859-4)
MakeBdfFontFromUCSMaster(luBS24,ISO8859-4)
MakeBdfFontFromUCSMaster(luIS08,ISO8859-4)
MakeBdfFontFromUCSMaster(luIS10,ISO8859-4)
MakeBdfFontFromUCSMaster(luIS12,ISO8859-4)
MakeBdfFontFromUCSMaster(luIS14,ISO8859-4)
MakeBdfFontFromUCSMaster(luIS18,ISO8859-4)
MakeBdfFontFromUCSMaster(luIS19,ISO8859-4)
MakeBdfFontFromUCSMaster(luIS24,ISO8859-4)
MakeBdfFontFromUCSMaster(luRS08,ISO8859-4)
MakeBdfFontFromUCSMaster(luRS10,ISO8859-4)
MakeBdfFontFromUCSMaster(luRS12,ISO8859-4)
MakeBdfFontFromUCSMaster(luRS14,ISO8859-4)
MakeBdfFontFromUCSMaster(luRS18,ISO8859-4)
MakeBdfFontFromUCSMaster(luRS19,ISO8859-4)
MakeBdfFontFromUCSMaster(luRS24,ISO8859-4)
MakeBdfFontFromUCSMaster(lubB08,ISO8859-4)
MakeBdfFontFromUCSMaster(lubB10,ISO8859-4)
MakeBdfFontFromUCSMaster(lubB12,ISO8859-4)
MakeBdfFontFromUCSMaster(lubB14,ISO8859-4)
MakeBdfFontFromUCSMaster(lubB18,ISO8859-4)
MakeBdfFontFromUCSMaster(lubB19,ISO8859-4)
MakeBdfFontFromUCSMaster(lubB24,ISO8859-4)
MakeBdfFontFromUCSMaster(lubBI08,ISO8859-4)
MakeBdfFontFromUCSMaster(lubBI10,ISO8859-4)
MakeBdfFontFromUCSMaster(lubBI12,ISO8859-4)
MakeBdfFontFromUCSMaster(lubBI14,ISO8859-4)
MakeBdfFontFromUCSMaster(lubBI18,ISO8859-4)
MakeBdfFontFromUCSMaster(lubBI19,ISO8859-4)
MakeBdfFontFromUCSMaster(lubBI24,ISO8859-4)
MakeBdfFontFromUCSMaster(lubI08,ISO8859-4)
MakeBdfFontFromUCSMaster(lubI10,ISO8859-4)
MakeBdfFontFromUCSMaster(lubI12,ISO8859-4)
MakeBdfFontFromUCSMaster(lubI14,ISO8859-4)
MakeBdfFontFromUCSMaster(lubI18,ISO8859-4)
MakeBdfFontFromUCSMaster(lubI19,ISO8859-4)
MakeBdfFontFromUCSMaster(lubI24,ISO8859-4)
MakeBdfFontFromUCSMaster(lubR08,ISO8859-4)
MakeBdfFontFromUCSMaster(lubR10,ISO8859-4)
MakeBdfFontFromUCSMaster(lubR12,ISO8859-4)
MakeBdfFontFromUCSMaster(lubR14,ISO8859-4)
MakeBdfFontFromUCSMaster(lubR18,ISO8859-4)
MakeBdfFontFromUCSMaster(lubR19,ISO8859-4)
MakeBdfFontFromUCSMaster(lubR24,ISO8859-4)
MakeBdfFontFromUCSMaster(lutBS08,ISO8859-4)
MakeBdfFontFromUCSMaster(lutBS10,ISO8859-4)
MakeBdfFontFromUCSMaster(lutBS12,ISO8859-4)
MakeBdfFontFromUCSMaster(lutBS14,ISO8859-4)
MakeBdfFontFromUCSMaster(lutBS18,ISO8859-4)
MakeBdfFontFromUCSMaster(lutBS19,ISO8859-4)
MakeBdfFontFromUCSMaster(lutBS24,ISO8859-4)
MakeBdfFontFromUCSMaster(lutRS08,ISO8859-4)
MakeBdfFontFromUCSMaster(lutRS10,ISO8859-4)
MakeBdfFontFromUCSMaster(lutRS12,ISO8859-4)
MakeBdfFontFromUCSMaster(lutRS14,ISO8859-4)
MakeBdfFontFromUCSMaster(lutRS18,ISO8859-4)
MakeBdfFontFromUCSMaster(lutRS19,ISO8859-4)
MakeBdfFontFromUCSMaster(lutRS24,ISO8859-4)
MakeBdfFontFromUCSMaster(UTBI__10,ISO8859-4)
MakeBdfFontFromUCSMaster(UTBI__12,ISO8859-4)
MakeBdfFontFromUCSMaster(UTBI__14,ISO8859-4)
MakeBdfFontFromUCSMaster(UTBI__18,ISO8859-4)
MakeBdfFontFromUCSMaster(UTBI__24,ISO8859-4)
MakeBdfFontFromUCSMaster(UTB___10,ISO8859-4)
MakeBdfFontFromUCSMaster(UTB___12,ISO8859-4)
MakeBdfFontFromUCSMaster(UTB___14,ISO8859-4)
MakeBdfFontFromUCSMaster(UTB___18,ISO8859-4)
MakeBdfFontFromUCSMaster(UTB___24,ISO8859-4)
MakeBdfFontFromUCSMaster(UTI___10,ISO8859-4)
MakeBdfFontFromUCSMaster(UTI___12,ISO8859-4)
MakeBdfFontFromUCSMaster(UTI___14,ISO8859-4)
MakeBdfFontFromUCSMaster(UTI___18,ISO8859-4)
MakeBdfFontFromUCSMaster(UTI___24,ISO8859-4)
MakeBdfFontFromUCSMaster(UTRG__10,ISO8859-4)
MakeBdfFontFromUCSMaster(UTRG__12,ISO8859-4)
MakeBdfFontFromUCSMaster(UTRG__14,ISO8859-4)
MakeBdfFontFromUCSMaster(UTRG__18,ISO8859-4)
MakeBdfFontFromUCSMaster(UTRG__24,ISO8859-4)
#endif

#if BuildISO8859_9Fonts
MakeBdfFontFromUCSMaster(courB08,ISO8859-9)
MakeBdfFontFromUCSMaster(courB10,ISO8859-9)
MakeBdfFontFromUCSMaster(courB12,ISO8859-9)
MakeBdfFontFromUCSMaster(courB14,ISO8859-9)
MakeBdfFontFromUCSMaster(courB18,ISO8859-9)
MakeBdfFontFromUCSMaster(courB24,ISO8859-9)
MakeBdfFontFromUCSMaster(courBO08,ISO8859-9)
MakeBdfFontFromUCSMaster(courBO10,ISO8859-9)
MakeBdfFontFromUCSMaster(courBO12,ISO8859-9)
MakeBdfFontFromUCSMaster(courBO14,ISO8859-9)
MakeBdfFontFromUCSMaster(courBO18,ISO8859-9)
MakeBdfFontFromUCSMaster(courBO24,ISO8859-9)
MakeBdfFontFromUCSMaster(courO08,ISO8859-9)
MakeBdfFontFromUCSMaster(courO10,ISO8859-9)
MakeBdfFontFromUCSMaster(courO12,ISO8859-9)
MakeBdfFontFromUCSMaster(courO14,ISO8859-9)
MakeBdfFontFromUCSMaster(courO18,ISO8859-9)
MakeBdfFontFromUCSMaster(courO24,ISO8859-9)
MakeBdfFontFromUCSMaster(courR08,ISO8859-9)
MakeBdfFontFromUCSMaster(courR10,ISO8859-9)
MakeBdfFontFromUCSMaster(courR12,ISO8859-9)
MakeBdfFontFromUCSMaster(courR14,ISO8859-9)
MakeBdfFontFromUCSMaster(courR18,ISO8859-9)
MakeBdfFontFromUCSMaster(courR24,ISO8859-9)
MakeBdfFontFromUCSMaster(helvB08,ISO8859-9)
MakeBdfFontFromUCSMaster(helvB10,ISO8859-9)
MakeBdfFontFromUCSMaster(helvB12,ISO8859-9)
MakeBdfFontFromUCSMaster(helvB14,ISO8859-9)
MakeBdfFontFromUCSMaster(helvB18,ISO8859-9)
MakeBdfFontFromUCSMaster(helvB24,ISO8859-9)
MakeBdfFontFromUCSMaster(helvBO08,ISO8859-9)
MakeBdfFontFromUCSMaster(helvBO10,ISO8859-9)
MakeBdfFontFromUCSMaster(helvBO12,ISO8859-9)
MakeBdfFontFromUCSMaster(helvBO14,ISO8859-9)
MakeBdfFontFromUCSMaster(helvBO18,ISO8859-9)
MakeBdfFontFromUCSMaster(helvBO24,ISO8859-9)
MakeBdfFontFromUCSMaster(helvO08,ISO8859-9)
MakeBdfFontFromUCSMaster(helvO10,ISO8859-9)
MakeBdfFontFromUCSMaster(helvO12,ISO8859-9)
MakeBdfFontFromUCSMaster(helvO14,ISO8859-9)
MakeBdfFontFromUCSMaster(helvO18,ISO8859-9)
MakeBdfFontFromUCSMaster(helvO24,ISO8859-9)
MakeBdfFontFromUCSMaster(helvR08,ISO8859-9)
MakeBdfFontFromUCSMaster(helvR10,ISO8859-9)
MakeBdfFontFromUCSMaster(helvR12,ISO8859-9)
MakeBdfFontFromUCSMaster(helvR14,ISO8859-9)
MakeBdfFontFromUCSMaster(helvR18,ISO8859-9)
MakeBdfFontFromUCSMaster(helvR24,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenB08,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenB10,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenB12,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenB14,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenB18,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenB24,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenBI08,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenBI10,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenBI12,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenBI14,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenBI18,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenBI24,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenI08,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenI10,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenI12,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenI14,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenI18,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenI24,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenR08,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenR10,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenR12,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenR14,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenR18,ISO8859-9)
MakeBdfFontFromUCSMaster(ncenR24,ISO8859-9)
MakeBdfFontFromUCSMaster(timB08,ISO8859-9)
MakeBdfFontFromUCSMaster(timB10,ISO8859-9)
MakeBdfFontFromUCSMaster(timB12,ISO8859-9)
MakeBdfFontFromUCSMaster(timB14,ISO8859-9)
MakeBdfFontFromUCSMaster(timB18,ISO8859-9)
MakeBdfFontFromUCSMaster(timB24,ISO8859-9)
MakeBdfFontFromUCSMaster(timBI08,ISO8859-9)
MakeBdfFontFromUCSMaster(timBI10,ISO8859-9)
MakeBdfFontFromUCSMaster(timBI12,ISO8859-9)
MakeBdfFontFromUCSMaster(timBI14,ISO8859-9)
MakeBdfFontFromUCSMaster(timBI18,ISO8859-9)
MakeBdfFontFromUCSMaster(timBI24,ISO8859-9)
MakeBdfFontFromUCSMaster(timI08,ISO8859-9)
MakeBdfFontFromUCSMaster(timI10,ISO8859-9)
MakeBdfFontFromUCSMaster(timI12,ISO8859-9)
MakeBdfFontFromUCSMaster(timI14,ISO8859-9)
MakeBdfFontFromUCSMaster(timI18,ISO8859-9)
MakeBdfFontFromUCSMaster(timI24,ISO8859-9)
MakeBdfFontFromUCSMaster(timR08,ISO8859-9)
MakeBdfFontFromUCSMaster(timR10,ISO8859-9)
MakeBdfFontFromUCSMaster(timR12,ISO8859-9)
MakeBdfFontFromUCSMaster(timR14,ISO8859-9)
MakeBdfFontFromUCSMaster(timR18,ISO8859-9)
MakeBdfFontFromUCSMaster(timR24,ISO8859-9)
MakeBdfFontFromUCSMaster(luBIS08,ISO8859-9)
MakeBdfFontFromUCSMaster(luBIS10,ISO8859-9)
MakeBdfFontFromUCSMaster(luBIS12,ISO8859-9)
MakeBdfFontFromUCSMaster(luBIS14,ISO8859-9)
MakeBdfFontFromUCSMaster(luBIS18,ISO8859-9)
MakeBdfFontFromUCSMaster(luBIS19,ISO8859-9)
MakeBdfFontFromUCSMaster(luBIS24,ISO8859-9)
MakeBdfFontFromUCSMaster(luBS08,ISO8859-9)
MakeBdfFontFromUCSMaster(luBS10,ISO8859-9)
MakeBdfFontFromUCSMaster(luBS12,ISO8859-9)
MakeBdfFontFromUCSMaster(luBS14,ISO8859-9)
MakeBdfFontFromUCSMaster(luBS18,ISO8859-9)
MakeBdfFontFromUCSMaster(luBS19,ISO8859-9)
MakeBdfFontFromUCSMaster(luBS24,ISO8859-9)
MakeBdfFontFromUCSMaster(luIS08,ISO8859-9)
MakeBdfFontFromUCSMaster(luIS10,ISO8859-9)
MakeBdfFontFromUCSMaster(luIS12,ISO8859-9)
MakeBdfFontFromUCSMaster(luIS14,ISO8859-9)
MakeBdfFontFromUCSMaster(luIS18,ISO8859-9)
MakeBdfFontFromUCSMaster(luIS19,ISO8859-9)
MakeBdfFontFromUCSMaster(luIS24,ISO8859-9)
MakeBdfFontFromUCSMaster(luRS08,ISO8859-9)
MakeBdfFontFromUCSMaster(luRS10,ISO8859-9)
MakeBdfFontFromUCSMaster(luRS12,ISO8859-9)
MakeBdfFontFromUCSMaster(luRS14,ISO8859-9)
MakeBdfFontFromUCSMaster(luRS18,ISO8859-9)
MakeBdfFontFromUCSMaster(luRS19,ISO8859-9)
MakeBdfFontFromUCSMaster(luRS24,ISO8859-9)
MakeBdfFontFromUCSMaster(lubB08,ISO8859-9)
MakeBdfFontFromUCSMaster(lubB10,ISO8859-9)
MakeBdfFontFromUCSMaster(lubB12,ISO8859-9)
MakeBdfFontFromUCSMaster(lubB14,ISO8859-9)
MakeBdfFontFromUCSMaster(lubB18,ISO8859-9)
MakeBdfFontFromUCSMaster(lubB19,ISO8859-9)
MakeBdfFontFromUCSMaster(lubB24,ISO8859-9)
MakeBdfFontFromUCSMaster(lubBI08,ISO8859-9)
MakeBdfFontFromUCSMaster(lubBI10,ISO8859-9)
MakeBdfFontFromUCSMaster(lubBI12,ISO8859-9)
MakeBdfFontFromUCSMaster(lubBI14,ISO8859-9)
MakeBdfFontFromUCSMaster(lubBI18,ISO8859-9)
MakeBdfFontFromUCSMaster(lubBI19,ISO8859-9)
MakeBdfFontFromUCSMaster(lubBI24,ISO8859-9)
MakeBdfFontFromUCSMaster(lubI08,ISO8859-9)
MakeBdfFontFromUCSMaster(lubI10,ISO8859-9)
MakeBdfFontFromUCSMaster(lubI12,ISO8859-9)
MakeBdfFontFromUCSMaster(lubI14,ISO8859-9)
MakeBdfFontFromUCSMaster(lubI18,ISO8859-9)
MakeBdfFontFromUCSMaster(lubI19,ISO8859-9)
MakeBdfFontFromUCSMaster(lubI24,ISO8859-9)
MakeBdfFontFromUCSMaster(lubR08,ISO8859-9)
MakeBdfFontFromUCSMaster(lubR10,ISO8859-9)
MakeBdfFontFromUCSMaster(lubR12,ISO8859-9)
MakeBdfFontFromUCSMaster(lubR14,ISO8859-9)
MakeBdfFontFromUCSMaster(lubR18,ISO8859-9)
MakeBdfFontFromUCSMaster(lubR19,ISO8859-9)
MakeBdfFontFromUCSMaster(lubR24,ISO8859-9)
MakeBdfFontFromUCSMaster(lutBS08,ISO8859-9)
MakeBdfFontFromUCSMaster(lutBS10,ISO8859-9)
MakeBdfFontFromUCSMaster(lutBS12,ISO8859-9)
MakeBdfFontFromUCSMaster(lutBS14,ISO8859-9)
MakeBdfFontFromUCSMaster(lutBS18,ISO8859-9)
MakeBdfFontFromUCSMaster(lutBS19,ISO8859-9)
MakeBdfFontFromUCSMaster(lutBS24,ISO8859-9)
MakeBdfFontFromUCSMaster(lutRS08,ISO8859-9)
MakeBdfFontFromUCSMaster(lutRS10,ISO8859-9)
MakeBdfFontFromUCSMaster(lutRS12,ISO8859-9)
MakeBdfFontFromUCSMaster(lutRS14,ISO8859-9)
MakeBdfFontFromUCSMaster(lutRS18,ISO8859-9)
MakeBdfFontFromUCSMaster(lutRS19,ISO8859-9)
MakeBdfFontFromUCSMaster(lutRS24,ISO8859-9)
MakeBdfFontFromUCSMaster(UTBI__10,ISO8859-9)
MakeBdfFontFromUCSMaster(UTBI__12,ISO8859-9)
MakeBdfFontFromUCSMaster(UTBI__14,ISO8859-9)
MakeBdfFontFromUCSMaster(UTBI__18,ISO8859-9)
MakeBdfFontFromUCSMaster(UTBI__24,ISO8859-9)
MakeBdfFontFromUCSMaster(UTB___10,ISO8859-9)
MakeBdfFontFromUCSMaster(UTB___12,ISO8859-9)
MakeBdfFontFromUCSMaster(UTB___14,ISO8859-9)
MakeBdfFontFromUCSMaster(UTB___18,ISO8859-9)
MakeBdfFontFromUCSMaster(UTB___24,ISO8859-9)
MakeBdfFontFromUCSMaster(UTI___10,ISO8859-9)
MakeBdfFontFromUCSMaster(UTI___12,ISO8859-9)
MakeBdfFontFromUCSMaster(UTI___14,ISO8859-9)
MakeBdfFontFromUCSMaster(UTI___18,ISO8859-9)
MakeBdfFontFromUCSMaster(UTI___24,ISO8859-9)
MakeBdfFontFromUCSMaster(UTRG__10,ISO8859-9)
MakeBdfFontFromUCSMaster(UTRG__12,ISO8859-9)
MakeBdfFontFromUCSMaster(UTRG__14,ISO8859-9)
MakeBdfFontFromUCSMaster(UTRG__18,ISO8859-9)
MakeBdfFontFromUCSMaster(UTRG__24,ISO8859-9)
#endif

#if BuildISO8859_10Fonts
MakeBdfFontFromUCSMaster(courB08,ISO8859-10)
MakeBdfFontFromUCSMaster(courB10,ISO8859-10)
MakeBdfFontFromUCSMaster(courB12,ISO8859-10)
MakeBdfFontFromUCSMaster(courB14,ISO8859-10)
MakeBdfFontFromUCSMaster(courB18,ISO8859-10)
MakeBdfFontFromUCSMaster(courB24,ISO8859-10)
MakeBdfFontFromUCSMaster(courBO08,ISO8859-10)
MakeBdfFontFromUCSMaster(courBO10,ISO8859-10)
MakeBdfFontFromUCSMaster(courBO12,ISO8859-10)
MakeBdfFontFromUCSMaster(courBO14,ISO8859-10)
MakeBdfFontFromUCSMaster(courBO18,ISO8859-10)
MakeBdfFontFromUCSMaster(courBO24,ISO8859-10)
MakeBdfFontFromUCSMaster(courO08,ISO8859-10)
MakeBdfFontFromUCSMaster(courO10,ISO8859-10)
MakeBdfFontFromUCSMaster(courO12,ISO8859-10)
MakeBdfFontFromUCSMaster(courO14,ISO8859-10)
MakeBdfFontFromUCSMaster(courO18,ISO8859-10)
MakeBdfFontFromUCSMaster(courO24,ISO8859-10)
MakeBdfFontFromUCSMaster(courR08,ISO8859-10)
MakeBdfFontFromUCSMaster(courR10,ISO8859-10)
MakeBdfFontFromUCSMaster(courR12,ISO8859-10)
MakeBdfFontFromUCSMaster(courR14,ISO8859-10)
MakeBdfFontFromUCSMaster(courR18,ISO8859-10)
MakeBdfFontFromUCSMaster(courR24,ISO8859-10)
MakeBdfFontFromUCSMaster(helvB08,ISO8859-10)
MakeBdfFontFromUCSMaster(helvB10,ISO8859-10)
MakeBdfFontFromUCSMaster(helvB12,ISO8859-10)
MakeBdfFontFromUCSMaster(helvB14,ISO8859-10)
MakeBdfFontFromUCSMaster(helvB18,ISO8859-10)
MakeBdfFontFromUCSMaster(helvB24,ISO8859-10)
MakeBdfFontFromUCSMaster(helvBO08,ISO8859-10)
MakeBdfFontFromUCSMaster(helvBO10,ISO8859-10)
MakeBdfFontFromUCSMaster(helvBO12,ISO8859-10)
MakeBdfFontFromUCSMaster(helvBO14,ISO8859-10)
MakeBdfFontFromUCSMaster(helvBO18,ISO8859-10)
MakeBdfFontFromUCSMaster(helvBO24,ISO8859-10)
MakeBdfFontFromUCSMaster(helvO08,ISO8859-10)
MakeBdfFontFromUCSMaster(helvO10,ISO8859-10)
MakeBdfFontFromUCSMaster(helvO12,ISO8859-10)
MakeBdfFontFromUCSMaster(helvO14,ISO8859-10)
MakeBdfFontFromUCSMaster(helvO18,ISO8859-10)
MakeBdfFontFromUCSMaster(helvO24,ISO8859-10)
MakeBdfFontFromUCSMaster(helvR08,ISO8859-10)
MakeBdfFontFromUCSMaster(helvR10,ISO8859-10)
MakeBdfFontFromUCSMaster(helvR12,ISO8859-10)
MakeBdfFontFromUCSMaster(helvR14,ISO8859-10)
MakeBdfFontFromUCSMaster(helvR18,ISO8859-10)
MakeBdfFontFromUCSMaster(helvR24,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenB08,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenB10,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenB12,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenB14,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenB18,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenB24,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenBI08,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenBI10,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenBI12,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenBI14,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenBI18,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenBI24,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenI08,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenI10,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenI12,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenI14,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenI18,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenI24,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenR08,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenR10,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenR12,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenR14,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenR18,ISO8859-10)
MakeBdfFontFromUCSMaster(ncenR24,ISO8859-10)
MakeBdfFontFromUCSMaster(timB08,ISO8859-10)
MakeBdfFontFromUCSMaster(timB10,ISO8859-10)
MakeBdfFontFromUCSMaster(timB12,ISO8859-10)
MakeBdfFontFromUCSMaster(timB14,ISO8859-10)
MakeBdfFontFromUCSMaster(timB18,ISO8859-10)
MakeBdfFontFromUCSMaster(timB24,ISO8859-10)
MakeBdfFontFromUCSMaster(timBI08,ISO8859-10)
MakeBdfFontFromUCSMaster(timBI10,ISO8859-10)
MakeBdfFontFromUCSMaster(timBI12,ISO8859-10)
MakeBdfFontFromUCSMaster(timBI14,ISO8859-10)
MakeBdfFontFromUCSMaster(timBI18,ISO8859-10)
MakeBdfFontFromUCSMaster(timBI24,ISO8859-10)
MakeBdfFontFromUCSMaster(timI08,ISO8859-10)
MakeBdfFontFromUCSMaster(timI10,ISO8859-10)
MakeBdfFontFromUCSMaster(timI12,ISO8859-10)
MakeBdfFontFromUCSMaster(timI14,ISO8859-10)
MakeBdfFontFromUCSMaster(timI18,ISO8859-10)
MakeBdfFontFromUCSMaster(timI24,ISO8859-10)
MakeBdfFontFromUCSMaster(timR08,ISO8859-10)
MakeBdfFontFromUCSMaster(timR10,ISO8859-10)
MakeBdfFontFromUCSMaster(timR12,ISO8859-10)
MakeBdfFontFromUCSMaster(timR14,ISO8859-10)
MakeBdfFontFromUCSMaster(timR18,ISO8859-10)
MakeBdfFontFromUCSMaster(timR24,ISO8859-10)
MakeBdfFontFromUCSMaster(luBIS08,ISO8859-10)
MakeBdfFontFromUCSMaster(luBIS10,ISO8859-10)
MakeBdfFontFromUCSMaster(luBIS12,ISO8859-10)
MakeBdfFontFromUCSMaster(luBIS14,ISO8859-10)
MakeBdfFontFromUCSMaster(luBIS18,ISO8859-10)
MakeBdfFontFromUCSMaster(luBIS19,ISO8859-10)
MakeBdfFontFromUCSMaster(luBIS24,ISO8859-10)
MakeBdfFontFromUCSMaster(luBS08,ISO8859-10)
MakeBdfFontFromUCSMaster(luBS10,ISO8859-10)
MakeBdfFontFromUCSMaster(luBS12,ISO8859-10)
MakeBdfFontFromUCSMaster(luBS14,ISO8859-10)
MakeBdfFontFromUCSMaster(luBS18,ISO8859-10)
MakeBdfFontFromUCSMaster(luBS19,ISO8859-10)
MakeBdfFontFromUCSMaster(luBS24,ISO8859-10)
MakeBdfFontFromUCSMaster(luIS08,ISO8859-10)
MakeBdfFontFromUCSMaster(luIS10,ISO8859-10)
MakeBdfFontFromUCSMaster(luIS12,ISO8859-10)
MakeBdfFontFromUCSMaster(luIS14,ISO8859-10)
MakeBdfFontFromUCSMaster(luIS18,ISO8859-10)
MakeBdfFontFromUCSMaster(luIS19,ISO8859-10)
MakeBdfFontFromUCSMaster(luIS24,ISO8859-10)
MakeBdfFontFromUCSMaster(luRS08,ISO8859-10)
MakeBdfFontFromUCSMaster(luRS10,ISO8859-10)
MakeBdfFontFromUCSMaster(luRS12,ISO8859-10)
MakeBdfFontFromUCSMaster(luRS14,ISO8859-10)
MakeBdfFontFromUCSMaster(luRS18,ISO8859-10)
MakeBdfFontFromUCSMaster(luRS19,ISO8859-10)
MakeBdfFontFromUCSMaster(luRS24,ISO8859-10)
MakeBdfFontFromUCSMaster(lubB08,ISO8859-10)
MakeBdfFontFromUCSMaster(lubB10,ISO8859-10)
MakeBdfFontFromUCSMaster(lubB12,ISO8859-10)
MakeBdfFontFromUCSMaster(lubB14,ISO8859-10)
MakeBdfFontFromUCSMaster(lubB18,ISO8859-10)
MakeBdfFontFromUCSMaster(lubB19,ISO8859-10)
MakeBdfFontFromUCSMaster(lubB24,ISO8859-10)
MakeBdfFontFromUCSMaster(lubBI08,ISO8859-10)
MakeBdfFontFromUCSMaster(lubBI10,ISO8859-10)
MakeBdfFontFromUCSMaster(lubBI12,ISO8859-10)
MakeBdfFontFromUCSMaster(lubBI14,ISO8859-10)
MakeBdfFontFromUCSMaster(lubBI18,ISO8859-10)
MakeBdfFontFromUCSMaster(lubBI19,ISO8859-10)
MakeBdfFontFromUCSMaster(lubBI24,ISO8859-10)
MakeBdfFontFromUCSMaster(lubI08,ISO8859-10)
MakeBdfFontFromUCSMaster(lubI10,ISO8859-10)
MakeBdfFontFromUCSMaster(lubI12,ISO8859-10)
MakeBdfFontFromUCSMaster(lubI14,ISO8859-10)
MakeBdfFontFromUCSMaster(lubI18,ISO8859-10)
MakeBdfFontFromUCSMaster(lubI19,ISO8859-10)
MakeBdfFontFromUCSMaster(lubI24,ISO8859-10)
MakeBdfFontFromUCSMaster(lubR08,ISO8859-10)
MakeBdfFontFromUCSMaster(lubR10,ISO8859-10)
MakeBdfFontFromUCSMaster(lubR12,ISO8859-10)
MakeBdfFontFromUCSMaster(lubR14,ISO8859-10)
MakeBdfFontFromUCSMaster(lubR18,ISO8859-10)
MakeBdfFontFromUCSMaster(lubR19,ISO8859-10)
MakeBdfFontFromUCSMaster(lubR24,ISO8859-10)
MakeBdfFontFromUCSMaster(lutBS08,ISO8859-10)
MakeBdfFontFromUCSMaster(lutBS10,ISO8859-10)
MakeBdfFontFromUCSMaster(lutBS12,ISO8859-10)
MakeBdfFontFromUCSMaster(lutBS14,ISO8859-10)
MakeBdfFontFromUCSMaster(lutBS18,ISO8859-10)
MakeBdfFontFromUCSMaster(lutBS19,ISO8859-10)
MakeBdfFontFromUCSMaster(lutBS24,ISO8859-10)
MakeBdfFontFromUCSMaster(lutRS08,ISO8859-10)
MakeBdfFontFromUCSMaster(lutRS10,ISO8859-10)
MakeBdfFontFromUCSMaster(lutRS12,ISO8859-10)
MakeBdfFontFromUCSMaster(lutRS14,ISO8859-10)
MakeBdfFontFromUCSMaster(lutRS18,ISO8859-10)
MakeBdfFontFromUCSMaster(lutRS19,ISO8859-10)
MakeBdfFontFromUCSMaster(lutRS24,ISO8859-10)
MakeBdfFontFromUCSMaster(UTBI__10,ISO8859-10)
MakeBdfFontFromUCSMaster(UTBI__12,ISO8859-10)
MakeBdfFontFromUCSMaster(UTBI__14,ISO8859-10)
MakeBdfFontFromUCSMaster(UTBI__18,ISO8859-10)
MakeBdfFontFromUCSMaster(UTBI__24,ISO8859-10)
MakeBdfFontFromUCSMaster(UTB___10,ISO8859-10)
MakeBdfFontFromUCSMaster(UTB___12,ISO8859-10)
MakeBdfFontFromUCSMaster(UTB___14,ISO8859-10)
MakeBdfFontFromUCSMaster(UTB___18,ISO8859-10)
MakeBdfFontFromUCSMaster(UTB___24,ISO8859-10)
MakeBdfFontFromUCSMaster(UTI___10,ISO8859-10)
MakeBdfFontFromUCSMaster(UTI___12,ISO8859-10)
MakeBdfFontFromUCSMaster(UTI___14,ISO8859-10)
MakeBdfFontFromUCSMaster(UTI___18,ISO8859-10)
MakeBdfFontFromUCSMaster(UTI___24,ISO8859-10)
MakeBdfFontFromUCSMaster(UTRG__10,ISO8859-10)
MakeBdfFontFromUCSMaster(UTRG__12,ISO8859-10)
MakeBdfFontFromUCSMaster(UTRG__14,ISO8859-10)
MakeBdfFontFromUCSMaster(UTRG__18,ISO8859-10)
MakeBdfFontFromUCSMaster(UTRG__24,ISO8859-10)
#endif

#if BuildISO8859_13Fonts
MakeBdfFontFromUCSMaster(courB08,ISO8859-13)
MakeBdfFontFromUCSMaster(courB10,ISO8859-13)
MakeBdfFontFromUCSMaster(courB12,ISO8859-13)
MakeBdfFontFromUCSMaster(courB14,ISO8859-13)
MakeBdfFontFromUCSMaster(courB18,ISO8859-13)
MakeBdfFontFromUCSMaster(courB24,ISO8859-13)
MakeBdfFontFromUCSMaster(courBO08,ISO8859-13)
MakeBdfFontFromUCSMaster(courBO10,ISO8859-13)
MakeBdfFontFromUCSMaster(courBO12,ISO8859-13)
MakeBdfFontFromUCSMaster(courBO14,ISO8859-13)
MakeBdfFontFromUCSMaster(courBO18,ISO8859-13)
MakeBdfFontFromUCSMaster(courBO24,ISO8859-13)
MakeBdfFontFromUCSMaster(courO08,ISO8859-13)
MakeBdfFontFromUCSMaster(courO10,ISO8859-13)
MakeBdfFontFromUCSMaster(courO12,ISO8859-13)
MakeBdfFontFromUCSMaster(courO14,ISO8859-13)
MakeBdfFontFromUCSMaster(courO18,ISO8859-13)
MakeBdfFontFromUCSMaster(courO24,ISO8859-13)
MakeBdfFontFromUCSMaster(courR08,ISO8859-13)
MakeBdfFontFromUCSMaster(courR10,ISO8859-13)
MakeBdfFontFromUCSMaster(courR12,ISO8859-13)
MakeBdfFontFromUCSMaster(courR14,ISO8859-13)
MakeBdfFontFromUCSMaster(courR18,ISO8859-13)
MakeBdfFontFromUCSMaster(courR24,ISO8859-13)
MakeBdfFontFromUCSMaster(helvB08,ISO8859-13)
MakeBdfFontFromUCSMaster(helvB10,ISO8859-13)
MakeBdfFontFromUCSMaster(helvB12,ISO8859-13)
MakeBdfFontFromUCSMaster(helvB14,ISO8859-13)
MakeBdfFontFromUCSMaster(helvB18,ISO8859-13)
MakeBdfFontFromUCSMaster(helvB24,ISO8859-13)
MakeBdfFontFromUCSMaster(helvBO08,ISO8859-13)
MakeBdfFontFromUCSMaster(helvBO10,ISO8859-13)
MakeBdfFontFromUCSMaster(helvBO12,ISO8859-13)
MakeBdfFontFromUCSMaster(helvBO14,ISO8859-13)
MakeBdfFontFromUCSMaster(helvBO18,ISO8859-13)
MakeBdfFontFromUCSMaster(helvBO24,ISO8859-13)
MakeBdfFontFromUCSMaster(helvO08,ISO8859-13)
MakeBdfFontFromUCSMaster(helvO10,ISO8859-13)
MakeBdfFontFromUCSMaster(helvO12,ISO8859-13)
MakeBdfFontFromUCSMaster(helvO14,ISO8859-13)
MakeBdfFontFromUCSMaster(helvO18,ISO8859-13)
MakeBdfFontFromUCSMaster(helvO24,ISO8859-13)
MakeBdfFontFromUCSMaster(helvR08,ISO8859-13)
MakeBdfFontFromUCSMaster(helvR10,ISO8859-13)
MakeBdfFontFromUCSMaster(helvR12,ISO8859-13)
MakeBdfFontFromUCSMaster(helvR14,ISO8859-13)
MakeBdfFontFromUCSMaster(helvR18,ISO8859-13)
MakeBdfFontFromUCSMaster(helvR24,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenB08,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenB10,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenB12,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenB14,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenB18,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenB24,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenBI08,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenBI10,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenBI12,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenBI14,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenBI18,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenBI24,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenI08,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenI10,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenI12,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenI14,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenI18,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenI24,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenR08,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenR10,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenR12,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenR14,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenR18,ISO8859-13)
MakeBdfFontFromUCSMaster(ncenR24,ISO8859-13)
MakeBdfFontFromUCSMaster(timB08,ISO8859-13)
MakeBdfFontFromUCSMaster(timB10,ISO8859-13)
MakeBdfFontFromUCSMaster(timB12,ISO8859-13)
MakeBdfFontFromUCSMaster(timB14,ISO8859-13)
MakeBdfFontFromUCSMaster(timB18,ISO8859-13)
MakeBdfFontFromUCSMaster(timB24,ISO8859-13)
MakeBdfFontFromUCSMaster(timBI08,ISO8859-13)
MakeBdfFontFromUCSMaster(timBI10,ISO8859-13)
MakeBdfFontFromUCSMaster(timBI12,ISO8859-13)
MakeBdfFontFromUCSMaster(timBI14,ISO8859-13)
MakeBdfFontFromUCSMaster(timBI18,ISO8859-13)
MakeBdfFontFromUCSMaster(timBI24,ISO8859-13)
MakeBdfFontFromUCSMaster(timI08,ISO8859-13)
MakeBdfFontFromUCSMaster(timI10,ISO8859-13)
MakeBdfFontFromUCSMaster(timI12,ISO8859-13)
MakeBdfFontFromUCSMaster(timI14,ISO8859-13)
MakeBdfFontFromUCSMaster(timI18,ISO8859-13)
MakeBdfFontFromUCSMaster(timI24,ISO8859-13)
MakeBdfFontFromUCSMaster(timR08,ISO8859-13)
MakeBdfFontFromUCSMaster(timR10,ISO8859-13)
MakeBdfFontFromUCSMaster(timR12,ISO8859-13)
MakeBdfFontFromUCSMaster(timR14,ISO8859-13)
MakeBdfFontFromUCSMaster(timR18,ISO8859-13)
MakeBdfFontFromUCSMaster(timR24,ISO8859-13)
MakeBdfFontFromUCSMaster(luBIS08,ISO8859-13)
MakeBdfFontFromUCSMaster(luBIS10,ISO8859-13)
MakeBdfFontFromUCSMaster(luBIS12,ISO8859-13)
MakeBdfFontFromUCSMaster(luBIS14,ISO8859-13)
MakeBdfFontFromUCSMaster(luBIS18,ISO8859-13)
MakeBdfFontFromUCSMaster(luBIS19,ISO8859-13)
MakeBdfFontFromUCSMaster(luBIS24,ISO8859-13)
MakeBdfFontFromUCSMaster(luBS08,ISO8859-13)
MakeBdfFontFromUCSMaster(luBS10,ISO8859-13)
MakeBdfFontFromUCSMaster(luBS12,ISO8859-13)
MakeBdfFontFromUCSMaster(luBS14,ISO8859-13)
MakeBdfFontFromUCSMaster(luBS18,ISO8859-13)
MakeBdfFontFromUCSMaster(luBS19,ISO8859-13)
MakeBdfFontFromUCSMaster(luBS24,ISO8859-13)
MakeBdfFontFromUCSMaster(luIS08,ISO8859-13)
MakeBdfFontFromUCSMaster(luIS10,ISO8859-13)
MakeBdfFontFromUCSMaster(luIS12,ISO8859-13)
MakeBdfFontFromUCSMaster(luIS14,ISO8859-13)
MakeBdfFontFromUCSMaster(luIS18,ISO8859-13)
MakeBdfFontFromUCSMaster(luIS19,ISO8859-13)
MakeBdfFontFromUCSMaster(luIS24,ISO8859-13)
MakeBdfFontFromUCSMaster(luRS08,ISO8859-13)
MakeBdfFontFromUCSMaster(luRS10,ISO8859-13)
MakeBdfFontFromUCSMaster(luRS12,ISO8859-13)
MakeBdfFontFromUCSMaster(luRS14,ISO8859-13)
MakeBdfFontFromUCSMaster(luRS18,ISO8859-13)
MakeBdfFontFromUCSMaster(luRS19,ISO8859-13)
MakeBdfFontFromUCSMaster(luRS24,ISO8859-13)
MakeBdfFontFromUCSMaster(lubB08,ISO8859-13)
MakeBdfFontFromUCSMaster(lubB10,ISO8859-13)
MakeBdfFontFromUCSMaster(lubB12,ISO8859-13)
MakeBdfFontFromUCSMaster(lubB14,ISO8859-13)
MakeBdfFontFromUCSMaster(lubB18,ISO8859-13)
MakeBdfFontFromUCSMaster(lubB19,ISO8859-13)
MakeBdfFontFromUCSMaster(lubB24,ISO8859-13)
MakeBdfFontFromUCSMaster(lubBI08,ISO8859-13)
MakeBdfFontFromUCSMaster(lubBI10,ISO8859-13)
MakeBdfFontFromUCSMaster(lubBI12,ISO8859-13)
MakeBdfFontFromUCSMaster(lubBI14,ISO8859-13)
MakeBdfFontFromUCSMaster(lubBI18,ISO8859-13)
MakeBdfFontFromUCSMaster(lubBI19,ISO8859-13)
MakeBdfFontFromUCSMaster(lubBI24,ISO8859-13)
MakeBdfFontFromUCSMaster(lubI08,ISO8859-13)
MakeBdfFontFromUCSMaster(lubI10,ISO8859-13)
MakeBdfFontFromUCSMaster(lubI12,ISO8859-13)
MakeBdfFontFromUCSMaster(lubI14,ISO8859-13)
MakeBdfFontFromUCSMaster(lubI18,ISO8859-13)
MakeBdfFontFromUCSMaster(lubI19,ISO8859-13)
MakeBdfFontFromUCSMaster(lubI24,ISO8859-13)
MakeBdfFontFromUCSMaster(lubR08,ISO8859-13)
MakeBdfFontFromUCSMaster(lubR10,ISO8859-13)
MakeBdfFontFromUCSMaster(lubR12,ISO8859-13)
MakeBdfFontFromUCSMaster(lubR14,ISO8859-13)
MakeBdfFontFromUCSMaster(lubR18,ISO8859-13)
MakeBdfFontFromUCSMaster(lubR19,ISO8859-13)
MakeBdfFontFromUCSMaster(lubR24,ISO8859-13)
MakeBdfFontFromUCSMaster(lutBS08,ISO8859-13)
MakeBdfFontFromUCSMaster(lutBS10,ISO8859-13)
MakeBdfFontFromUCSMaster(lutBS12,ISO8859-13)
MakeBdfFontFromUCSMaster(lutBS14,ISO8859-13)
MakeBdfFontFromUCSMaster(lutBS18,ISO8859-13)
MakeBdfFontFromUCSMaster(lutBS19,ISO8859-13)
MakeBdfFontFromUCSMaster(lutBS24,ISO8859-13)
MakeBdfFontFromUCSMaster(lutRS08,ISO8859-13)
MakeBdfFontFromUCSMaster(lutRS10,ISO8859-13)
MakeBdfFontFromUCSMaster(lutRS12,ISO8859-13)
MakeBdfFontFromUCSMaster(lutRS14,ISO8859-13)
MakeBdfFontFromUCSMaster(lutRS18,ISO8859-13)
MakeBdfFontFromUCSMaster(lutRS19,ISO8859-13)
MakeBdfFontFromUCSMaster(lutRS24,ISO8859-13)
MakeBdfFontFromUCSMaster(UTBI__10,ISO8859-13)
MakeBdfFontFromUCSMaster(UTBI__12,ISO8859-13)
MakeBdfFontFromUCSMaster(UTBI__14,ISO8859-13)
MakeBdfFontFromUCSMaster(UTBI__18,ISO8859-13)
MakeBdfFontFromUCSMaster(UTBI__24,ISO8859-13)
MakeBdfFontFromUCSMaster(UTB___10,ISO8859-13)
MakeBdfFontFromUCSMaster(UTB___12,ISO8859-13)
MakeBdfFontFromUCSMaster(UTB___14,ISO8859-13)
MakeBdfFontFromUCSMaster(UTB___18,ISO8859-13)
MakeBdfFontFromUCSMaster(UTB___24,ISO8859-13)
MakeBdfFontFromUCSMaster(UTI___10,ISO8859-13)
MakeBdfFontFromUCSMaster(UTI___12,ISO8859-13)
MakeBdfFontFromUCSMaster(UTI___14,ISO8859-13)
MakeBdfFontFromUCSMaster(UTI___18,ISO8859-13)
MakeBdfFontFromUCSMaster(UTI___24,ISO8859-13)
MakeBdfFontFromUCSMaster(UTRG__10,ISO8859-13)
MakeBdfFontFromUCSMaster(UTRG__12,ISO8859-13)
MakeBdfFontFromUCSMaster(UTRG__14,ISO8859-13)
MakeBdfFontFromUCSMaster(UTRG__18,ISO8859-13)
MakeBdfFontFromUCSMaster(UTRG__24,ISO8859-13)
#endif

#if BuildISO8859_14Fonts
MakeBdfFontFromUCSMaster(courB08,ISO8859-14)
MakeBdfFontFromUCSMaster(courB10,ISO8859-14)
MakeBdfFontFromUCSMaster(courB12,ISO8859-14)
MakeBdfFontFromUCSMaster(courB14,ISO8859-14)
MakeBdfFontFromUCSMaster(courB18,ISO8859-14)
MakeBdfFontFromUCSMaster(courB24,ISO8859-14)
MakeBdfFontFromUCSMaster(courBO08,ISO8859-14)
MakeBdfFontFromUCSMaster(courBO10,ISO8859-14)
MakeBdfFontFromUCSMaster(courBO12,ISO8859-14)
MakeBdfFontFromUCSMaster(courBO14,ISO8859-14)
MakeBdfFontFromUCSMaster(courBO18,ISO8859-14)
MakeBdfFontFromUCSMaster(courBO24,ISO8859-14)
MakeBdfFontFromUCSMaster(courO08,ISO8859-14)
MakeBdfFontFromUCSMaster(courO10,ISO8859-14)
MakeBdfFontFromUCSMaster(courO12,ISO8859-14)
MakeBdfFontFromUCSMaster(courO14,ISO8859-14)
MakeBdfFontFromUCSMaster(courO18,ISO8859-14)
MakeBdfFontFromUCSMaster(courO24,ISO8859-14)
MakeBdfFontFromUCSMaster(courR08,ISO8859-14)
MakeBdfFontFromUCSMaster(courR10,ISO8859-14)
MakeBdfFontFromUCSMaster(courR12,ISO8859-14)
MakeBdfFontFromUCSMaster(courR14,ISO8859-14)
MakeBdfFontFromUCSMaster(courR18,ISO8859-14)
MakeBdfFontFromUCSMaster(courR24,ISO8859-14)
MakeBdfFontFromUCSMaster(helvB08,ISO8859-14)
MakeBdfFontFromUCSMaster(helvB10,ISO8859-14)
MakeBdfFontFromUCSMaster(helvB12,ISO8859-14)
MakeBdfFontFromUCSMaster(helvB14,ISO8859-14)
MakeBdfFontFromUCSMaster(helvB18,ISO8859-14)
MakeBdfFontFromUCSMaster(helvB24,ISO8859-14)
MakeBdfFontFromUCSMaster(helvBO08,ISO8859-14)
MakeBdfFontFromUCSMaster(helvBO10,ISO8859-14)
MakeBdfFontFromUCSMaster(helvBO12,ISO8859-14)
MakeBdfFontFromUCSMaster(helvBO14,ISO8859-14)
MakeBdfFontFromUCSMaster(helvBO18,ISO8859-14)
MakeBdfFontFromUCSMaster(helvBO24,ISO8859-14)
MakeBdfFontFromUCSMaster(helvO08,ISO8859-14)
MakeBdfFontFromUCSMaster(helvO10,ISO8859-14)
MakeBdfFontFromUCSMaster(helvO12,ISO8859-14)
MakeBdfFontFromUCSMaster(helvO14,ISO8859-14)
MakeBdfFontFromUCSMaster(helvO18,ISO8859-14)
MakeBdfFontFromUCSMaster(helvO24,ISO8859-14)
MakeBdfFontFromUCSMaster(helvR08,ISO8859-14)
MakeBdfFontFromUCSMaster(helvR10,ISO8859-14)
MakeBdfFontFromUCSMaster(helvR12,ISO8859-14)
MakeBdfFontFromUCSMaster(helvR14,ISO8859-14)
MakeBdfFontFromUCSMaster(helvR18,ISO8859-14)
MakeBdfFontFromUCSMaster(helvR24,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenB08,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenB10,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenB12,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenB14,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenB18,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenB24,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenBI08,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenBI10,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenBI12,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenBI14,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenBI18,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenBI24,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenI08,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenI10,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenI12,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenI14,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenI18,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenI24,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenR08,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenR10,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenR12,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenR14,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenR18,ISO8859-14)
MakeBdfFontFromUCSMaster(ncenR24,ISO8859-14)
MakeBdfFontFromUCSMaster(timB08,ISO8859-14)
MakeBdfFontFromUCSMaster(timB10,ISO8859-14)
MakeBdfFontFromUCSMaster(timB12,ISO8859-14)
MakeBdfFontFromUCSMaster(timB14,ISO8859-14)
MakeBdfFontFromUCSMaster(timB18,ISO8859-14)
MakeBdfFontFromUCSMaster(timB24,ISO8859-14)
MakeBdfFontFromUCSMaster(timBI08,ISO8859-14)
MakeBdfFontFromUCSMaster(timBI10,ISO8859-14)
MakeBdfFontFromUCSMaster(timBI12,ISO8859-14)
MakeBdfFontFromUCSMaster(timBI14,ISO8859-14)
MakeBdfFontFromUCSMaster(timBI18,ISO8859-14)
MakeBdfFontFromUCSMaster(timBI24,ISO8859-14)
MakeBdfFontFromUCSMaster(timI08,ISO8859-14)
MakeBdfFontFromUCSMaster(timI10,ISO8859-14)
MakeBdfFontFromUCSMaster(timI12,ISO8859-14)
MakeBdfFontFromUCSMaster(timI14,ISO8859-14)
MakeBdfFontFromUCSMaster(timI18,ISO8859-14)
MakeBdfFontFromUCSMaster(timI24,ISO8859-14)
MakeBdfFontFromUCSMaster(timR08,ISO8859-14)
MakeBdfFontFromUCSMaster(timR10,ISO8859-14)
MakeBdfFontFromUCSMaster(timR12,ISO8859-14)
MakeBdfFontFromUCSMaster(timR14,ISO8859-14)
MakeBdfFontFromUCSMaster(timR18,ISO8859-14)
MakeBdfFontFromUCSMaster(timR24,ISO8859-14)
MakeBdfFontFromUCSMaster(luBIS08,ISO8859-14)
MakeBdfFontFromUCSMaster(luBIS10,ISO8859-14)
MakeBdfFontFromUCSMaster(luBIS12,ISO8859-14)
MakeBdfFontFromUCSMaster(luBIS14,ISO8859-14)
MakeBdfFontFromUCSMaster(luBIS18,ISO8859-14)
MakeBdfFontFromUCSMaster(luBIS19,ISO8859-14)
MakeBdfFontFromUCSMaster(luBIS24,ISO8859-14)
MakeBdfFontFromUCSMaster(luBS08,ISO8859-14)
MakeBdfFontFromUCSMaster(luBS10,ISO8859-14)
MakeBdfFontFromUCSMaster(luBS12,ISO8859-14)
MakeBdfFontFromUCSMaster(luBS14,ISO8859-14)
MakeBdfFontFromUCSMaster(luBS18,ISO8859-14)
MakeBdfFontFromUCSMaster(luBS19,ISO8859-14)
MakeBdfFontFromUCSMaster(luBS24,ISO8859-14)
MakeBdfFontFromUCSMaster(luIS08,ISO8859-14)
MakeBdfFontFromUCSMaster(luIS10,ISO8859-14)
MakeBdfFontFromUCSMaster(luIS12,ISO8859-14)
MakeBdfFontFromUCSMaster(luIS14,ISO8859-14)
MakeBdfFontFromUCSMaster(luIS18,ISO8859-14)
MakeBdfFontFromUCSMaster(luIS19,ISO8859-14)
MakeBdfFontFromUCSMaster(luIS24,ISO8859-14)
MakeBdfFontFromUCSMaster(luRS08,ISO8859-14)
MakeBdfFontFromUCSMaster(luRS10,ISO8859-14)
MakeBdfFontFromUCSMaster(luRS12,ISO8859-14)
MakeBdfFontFromUCSMaster(luRS14,ISO8859-14)
MakeBdfFontFromUCSMaster(luRS18,ISO8859-14)
MakeBdfFontFromUCSMaster(luRS19,ISO8859-14)
MakeBdfFontFromUCSMaster(luRS24,ISO8859-14)
MakeBdfFontFromUCSMaster(lubB08,ISO8859-14)
MakeBdfFontFromUCSMaster(lubB10,ISO8859-14)
MakeBdfFontFromUCSMaster(lubB12,ISO8859-14)
MakeBdfFontFromUCSMaster(lubB14,ISO8859-14)
MakeBdfFontFromUCSMaster(lubB18,ISO8859-14)
MakeBdfFontFromUCSMaster(lubB19,ISO8859-14)
MakeBdfFontFromUCSMaster(lubB24,ISO8859-14)
MakeBdfFontFromUCSMaster(lubBI08,ISO8859-14)
MakeBdfFontFromUCSMaster(lubBI10,ISO8859-14)
MakeBdfFontFromUCSMaster(lubBI12,ISO8859-14)
MakeBdfFontFromUCSMaster(lubBI14,ISO8859-14)
MakeBdfFontFromUCSMaster(lubBI18,ISO8859-14)
MakeBdfFontFromUCSMaster(lubBI19,ISO8859-14)
MakeBdfFontFromUCSMaster(lubBI24,ISO8859-14)
MakeBdfFontFromUCSMaster(lubI08,ISO8859-14)
MakeBdfFontFromUCSMaster(lubI10,ISO8859-14)
MakeBdfFontFromUCSMaster(lubI12,ISO8859-14)
MakeBdfFontFromUCSMaster(lubI14,ISO8859-14)
MakeBdfFontFromUCSMaster(lubI18,ISO8859-14)
MakeBdfFontFromUCSMaster(lubI19,ISO8859-14)
MakeBdfFontFromUCSMaster(lubI24,ISO8859-14)
MakeBdfFontFromUCSMaster(lubR08,ISO8859-14)
MakeBdfFontFromUCSMaster(lubR10,ISO8859-14)
MakeBdfFontFromUCSMaster(lubR12,ISO8859-14)
MakeBdfFontFromUCSMaster(lubR14,ISO8859-14)
MakeBdfFontFromUCSMaster(lubR18,ISO8859-14)
MakeBdfFontFromUCSMaster(lubR19,ISO8859-14)
MakeBdfFontFromUCSMaster(lubR24,ISO8859-14)
MakeBdfFontFromUCSMaster(lutBS08,ISO8859-14)
MakeBdfFontFromUCSMaster(lutBS10,ISO8859-14)
MakeBdfFontFromUCSMaster(lutBS12,ISO8859-14)
MakeBdfFontFromUCSMaster(lutBS14,ISO8859-14)
MakeBdfFontFromUCSMaster(lutBS18,ISO8859-14)
MakeBdfFontFromUCSMaster(lutBS19,ISO8859-14)
MakeBdfFontFromUCSMaster(lutBS24,ISO8859-14)
MakeBdfFontFromUCSMaster(lutRS08,ISO8859-14)
MakeBdfFontFromUCSMaster(lutRS10,ISO8859-14)
MakeBdfFontFromUCSMaster(lutRS12,ISO8859-14)
MakeBdfFontFromUCSMaster(lutRS14,ISO8859-14)
MakeBdfFontFromUCSMaster(lutRS18,ISO8859-14)
MakeBdfFontFromUCSMaster(lutRS19,ISO8859-14)
MakeBdfFontFromUCSMaster(lutRS24,ISO8859-14)
MakeBdfFontFromUCSMaster(UTBI__10,ISO8859-14)
MakeBdfFontFromUCSMaster(UTBI__12,ISO8859-14)
MakeBdfFontFromUCSMaster(UTBI__14,ISO8859-14)
MakeBdfFontFromUCSMaster(UTBI__18,ISO8859-14)
MakeBdfFontFromUCSMaster(UTBI__24,ISO8859-14)
MakeBdfFontFromUCSMaster(UTB___10,ISO8859-14)
MakeBdfFontFromUCSMaster(UTB___12,ISO8859-14)
MakeBdfFontFromUCSMaster(UTB___14,ISO8859-14)
MakeBdfFontFromUCSMaster(UTB___18,ISO8859-14)
MakeBdfFontFromUCSMaster(UTB___24,ISO8859-14)
MakeBdfFontFromUCSMaster(UTI___10,ISO8859-14)
MakeBdfFontFromUCSMaster(UTI___12,ISO8859-14)
MakeBdfFontFromUCSMaster(UTI___14,ISO8859-14)
MakeBdfFontFromUCSMaster(UTI___18,ISO8859-14)
MakeBdfFontFromUCSMaster(UTI___24,ISO8859-14)
MakeBdfFontFromUCSMaster(UTRG__10,ISO8859-14)
MakeBdfFontFromUCSMaster(UTRG__12,ISO8859-14)
MakeBdfFontFromUCSMaster(UTRG__14,ISO8859-14)
MakeBdfFontFromUCSMaster(UTRG__18,ISO8859-14)
MakeBdfFontFromUCSMaster(UTRG__24,ISO8859-14)
#endif

#if BuildISO8859_15Fonts
MakeBdfFontFromUCSMaster(courB08,ISO8859-15)
MakeBdfFontFromUCSMaster(courB10,ISO8859-15)
MakeBdfFontFromUCSMaster(courB12,ISO8859-15)
MakeBdfFontFromUCSMaster(courB14,ISO8859-15)
MakeBdfFontFromUCSMaster(courB18,ISO8859-15)
MakeBdfFontFromUCSMaster(courB24,ISO8859-15)
MakeBdfFontFromUCSMaster(courBO08,ISO8859-15)
MakeBdfFontFromUCSMaster(courBO10,ISO8859-15)
MakeBdfFontFromUCSMaster(courBO12,ISO8859-15)
MakeBdfFontFromUCSMaster(courBO14,ISO8859-15)
MakeBdfFontFromUCSMaster(courBO18,ISO8859-15)
MakeBdfFontFromUCSMaster(courBO24,ISO8859-15)
MakeBdfFontFromUCSMaster(courO08,ISO8859-15)
MakeBdfFontFromUCSMaster(courO10,ISO8859-15)
MakeBdfFontFromUCSMaster(courO12,ISO8859-15)
MakeBdfFontFromUCSMaster(courO14,ISO8859-15)
MakeBdfFontFromUCSMaster(courO18,ISO8859-15)
MakeBdfFontFromUCSMaster(courO24,ISO8859-15)
MakeBdfFontFromUCSMaster(courR08,ISO8859-15)
MakeBdfFontFromUCSMaster(courR10,ISO8859-15)
MakeBdfFontFromUCSMaster(courR12,ISO8859-15)
MakeBdfFontFromUCSMaster(courR14,ISO8859-15)
MakeBdfFontFromUCSMaster(courR18,ISO8859-15)
MakeBdfFontFromUCSMaster(courR24,ISO8859-15)
MakeBdfFontFromUCSMaster(helvB08,ISO8859-15)
MakeBdfFontFromUCSMaster(helvB10,ISO8859-15)
MakeBdfFontFromUCSMaster(helvB12,ISO8859-15)
MakeBdfFontFromUCSMaster(helvB14,ISO8859-15)
MakeBdfFontFromUCSMaster(helvB18,ISO8859-15)
MakeBdfFontFromUCSMaster(helvB24,ISO8859-15)
MakeBdfFontFromUCSMaster(helvBO08,ISO8859-15)
MakeBdfFontFromUCSMaster(helvBO10,ISO8859-15)
MakeBdfFontFromUCSMaster(helvBO12,ISO8859-15)
MakeBdfFontFromUCSMaster(helvBO14,ISO8859-15)
MakeBdfFontFromUCSMaster(helvBO18,ISO8859-15)
MakeBdfFontFromUCSMaster(helvBO24,ISO8859-15)
MakeBdfFontFromUCSMaster(helvO08,ISO8859-15)
MakeBdfFontFromUCSMaster(helvO10,ISO8859-15)
MakeBdfFontFromUCSMaster(helvO12,ISO8859-15)
MakeBdfFontFromUCSMaster(helvO14,ISO8859-15)
MakeBdfFontFromUCSMaster(helvO18,ISO8859-15)
MakeBdfFontFromUCSMaster(helvO24,ISO8859-15)
MakeBdfFontFromUCSMaster(helvR08,ISO8859-15)
MakeBdfFontFromUCSMaster(helvR10,ISO8859-15)
MakeBdfFontFromUCSMaster(helvR12,ISO8859-15)
MakeBdfFontFromUCSMaster(helvR14,ISO8859-15)
MakeBdfFontFromUCSMaster(helvR18,ISO8859-15)
MakeBdfFontFromUCSMaster(helvR24,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenB08,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenB10,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenB12,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenB14,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenB18,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenB24,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenBI08,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenBI10,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenBI12,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenBI14,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenBI18,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenBI24,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenI08,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenI10,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenI12,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenI14,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenI18,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenI24,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenR08,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenR10,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenR12,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenR14,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenR18,ISO8859-15)
MakeBdfFontFromUCSMaster(ncenR24,ISO8859-15)
MakeBdfFontFromUCSMaster(timB08,ISO8859-15)
MakeBdfFontFromUCSMaster(timB10,ISO8859-15)
MakeBdfFontFromUCSMaster(timB12,ISO8859-15)
MakeBdfFontFromUCSMaster(timB14,ISO8859-15)
MakeBdfFontFromUCSMaster(timB18,ISO8859-15)
MakeBdfFontFromUCSMaster(timB24,ISO8859-15)
MakeBdfFontFromUCSMaster(timBI08,ISO8859-15)
MakeBdfFontFromUCSMaster(timBI10,ISO8859-15)
MakeBdfFontFromUCSMaster(timBI12,ISO8859-15)
MakeBdfFontFromUCSMaster(timBI14,ISO8859-15)
MakeBdfFontFromUCSMaster(timBI18,ISO8859-15)
MakeBdfFontFromUCSMaster(timBI24,ISO8859-15)
MakeBdfFontFromUCSMaster(timI08,ISO8859-15)
MakeBdfFontFromUCSMaster(timI10,ISO8859-15)
MakeBdfFontFromUCSMaster(timI12,ISO8859-15)
MakeBdfFontFromUCSMaster(timI14,ISO8859-15)
MakeBdfFontFromUCSMaster(timI18,ISO8859-15)
MakeBdfFontFromUCSMaster(timI24,ISO8859-15)
MakeBdfFontFromUCSMaster(timR08,ISO8859-15)
MakeBdfFontFromUCSMaster(timR10,ISO8859-15)
MakeBdfFontFromUCSMaster(timR12,ISO8859-15)
MakeBdfFontFromUCSMaster(timR14,ISO8859-15)
MakeBdfFontFromUCSMaster(timR18,ISO8859-15)
MakeBdfFontFromUCSMaster(timR24,ISO8859-15)
MakeBdfFontFromUCSMaster(luBIS08,ISO8859-15)
MakeBdfFontFromUCSMaster(luBIS10,ISO8859-15)
MakeBdfFontFromUCSMaster(luBIS12,ISO8859-15)
MakeBdfFontFromUCSMaster(luBIS14,ISO8859-15)
MakeBdfFontFromUCSMaster(luBIS18,ISO8859-15)
MakeBdfFontFromUCSMaster(luBIS19,ISO8859-15)
MakeBdfFontFromUCSMaster(luBIS24,ISO8859-15)
MakeBdfFontFromUCSMaster(luBS08,ISO8859-15)
MakeBdfFontFromUCSMaster(luBS10,ISO8859-15)
MakeBdfFontFromUCSMaster(luBS12,ISO8859-15)
MakeBdfFontFromUCSMaster(luBS14,ISO8859-15)
MakeBdfFontFromUCSMaster(luBS18,ISO8859-15)
MakeBdfFontFromUCSMaster(luBS19,ISO8859-15)
MakeBdfFontFromUCSMaster(luBS24,ISO8859-15)
MakeBdfFontFromUCSMaster(luIS08,ISO8859-15)
MakeBdfFontFromUCSMaster(luIS10,ISO8859-15)
MakeBdfFontFromUCSMaster(luIS12,ISO8859-15)
MakeBdfFontFromUCSMaster(luIS14,ISO8859-15)
MakeBdfFontFromUCSMaster(luIS18,ISO8859-15)
MakeBdfFontFromUCSMaster(luIS19,ISO8859-15)
MakeBdfFontFromUCSMaster(luIS24,ISO8859-15)
MakeBdfFontFromUCSMaster(luRS08,ISO8859-15)
MakeBdfFontFromUCSMaster(luRS10,ISO8859-15)
MakeBdfFontFromUCSMaster(luRS12,ISO8859-15)
MakeBdfFontFromUCSMaster(luRS14,ISO8859-15)
MakeBdfFontFromUCSMaster(luRS18,ISO8859-15)
MakeBdfFontFromUCSMaster(luRS19,ISO8859-15)
MakeBdfFontFromUCSMaster(luRS24,ISO8859-15)
MakeBdfFontFromUCSMaster(lubB08,ISO8859-15)
MakeBdfFontFromUCSMaster(lubB10,ISO8859-15)
MakeBdfFontFromUCSMaster(lubB12,ISO8859-15)
MakeBdfFontFromUCSMaster(lubB14,ISO8859-15)
MakeBdfFontFromUCSMaster(lubB18,ISO8859-15)
MakeBdfFontFromUCSMaster(lubB19,ISO8859-15)
MakeBdfFontFromUCSMaster(lubB24,ISO8859-15)
MakeBdfFontFromUCSMaster(lubBI08,ISO8859-15)
MakeBdfFontFromUCSMaster(lubBI10,ISO8859-15)
MakeBdfFontFromUCSMaster(lubBI12,ISO8859-15)
MakeBdfFontFromUCSMaster(lubBI14,ISO8859-15)
MakeBdfFontFromUCSMaster(lubBI18,ISO8859-15)
MakeBdfFontFromUCSMaster(lubBI19,ISO8859-15)
MakeBdfFontFromUCSMaster(lubBI24,ISO8859-15)
MakeBdfFontFromUCSMaster(lubI08,ISO8859-15)
MakeBdfFontFromUCSMaster(lubI10,ISO8859-15)
MakeBdfFontFromUCSMaster(lubI12,ISO8859-15)
MakeBdfFontFromUCSMaster(lubI14,ISO8859-15)
MakeBdfFontFromUCSMaster(lubI18,ISO8859-15)
MakeBdfFontFromUCSMaster(lubI19,ISO8859-15)
MakeBdfFontFromUCSMaster(lubI24,ISO8859-15)
MakeBdfFontFromUCSMaster(lubR08,ISO8859-15)
MakeBdfFontFromUCSMaster(lubR10,ISO8859-15)
MakeBdfFontFromUCSMaster(lubR12,ISO8859-15)
MakeBdfFontFromUCSMaster(lubR14,ISO8859-15)
MakeBdfFontFromUCSMaster(lubR18,ISO8859-15)
MakeBdfFontFromUCSMaster(lubR19,ISO8859-15)
MakeBdfFontFromUCSMaster(lubR24,ISO8859-15)
MakeBdfFontFromUCSMaster(lutBS08,ISO8859-15)
MakeBdfFontFromUCSMaster(lutBS10,ISO8859-15)
MakeBdfFontFromUCSMaster(lutBS12,ISO8859-15)
MakeBdfFontFromUCSMaster(lutBS14,ISO8859-15)
MakeBdfFontFromUCSMaster(lutBS18,ISO8859-15)
MakeBdfFontFromUCSMaster(lutBS19,ISO8859-15)
MakeBdfFontFromUCSMaster(lutBS24,ISO8859-15)
MakeBdfFontFromUCSMaster(lutRS08,ISO8859-15)
MakeBdfFontFromUCSMaster(lutRS10,ISO8859-15)
MakeBdfFontFromUCSMaster(lutRS12,ISO8859-15)
MakeBdfFontFromUCSMaster(lutRS14,ISO8859-15)
MakeBdfFontFromUCSMaster(lutRS18,ISO8859-15)
MakeBdfFontFromUCSMaster(lutRS19,ISO8859-15)
MakeBdfFontFromUCSMaster(lutRS24,ISO8859-15)
MakeBdfFontFromUCSMaster(UTBI__10,ISO8859-15)
MakeBdfFontFromUCSMaster(UTBI__12,ISO8859-15)
MakeBdfFontFromUCSMaster(UTBI__14,ISO8859-15)
MakeBdfFontFromUCSMaster(UTBI__18,ISO8859-15)
MakeBdfFontFromUCSMaster(UTBI__24,ISO8859-15)
MakeBdfFontFromUCSMaster(UTB___10,ISO8859-15)
MakeBdfFontFromUCSMaster(UTB___12,ISO8859-15)
MakeBdfFontFromUCSMaster(UTB___14,ISO8859-15)
MakeBdfFontFromUCSMaster(UTB___18,ISO8859-15)
MakeBdfFontFromUCSMaster(UTB___24,ISO8859-15)
MakeBdfFontFromUCSMaster(UTI___10,ISO8859-15)
MakeBdfFontFromUCSMaster(UTI___12,ISO8859-15)
MakeBdfFontFromUCSMaster(UTI___14,ISO8859-15)
MakeBdfFontFromUCSMaster(UTI___18,ISO8859-15)
MakeBdfFontFromUCSMaster(UTI___24,ISO8859-15)
MakeBdfFontFromUCSMaster(UTRG__10,ISO8859-15)
MakeBdfFontFromUCSMaster(UTRG__12,ISO8859-15)
MakeBdfFontFromUCSMaster(UTRG__14,ISO8859-15)
MakeBdfFontFromUCSMaster(UTRG__18,ISO8859-15)
MakeBdfFontFromUCSMaster(UTRG__24,ISO8859-15)
#endif

#else /* DerivedUCSFonts */

#if BuildISO8859_1Fonts
LinkFile(courB08-ISO8859-1.bdf,courB08-L1.bdf)
LinkFile(courB10-ISO8859-1.bdf,courB10-L1.bdf)
LinkFile(courB12-ISO8859-1.bdf,courB12-L1.bdf)
LinkFile(courB14-ISO8859-1.bdf,courB14-L1.bdf)
LinkFile(courB18-ISO8859-1.bdf,courB18-L1.bdf)
LinkFile(courB24-ISO8859-1.bdf,courB24-L1.bdf)
LinkFile(courBO08-ISO8859-1.bdf,courBO08-L1.bdf)
LinkFile(courBO10-ISO8859-1.bdf,courBO10-L1.bdf)
LinkFile(courBO12-ISO8859-1.bdf,courBO12-L1.bdf)
LinkFile(courBO14-ISO8859-1.bdf,courBO14-L1.bdf)
LinkFile(courBO18-ISO8859-1.bdf,courBO18-L1.bdf)
LinkFile(courBO24-ISO8859-1.bdf,courBO24-L1.bdf)
LinkFile(courO08-ISO8859-1.bdf,courO08-L1.bdf)
LinkFile(courO10-ISO8859-1.bdf,courO10-L1.bdf)
LinkFile(courO12-ISO8859-1.bdf,courO12-L1.bdf)
LinkFile(courO14-ISO8859-1.bdf,courO14-L1.bdf)
LinkFile(courO18-ISO8859-1.bdf,courO18-L1.bdf)
LinkFile(courO24-ISO8859-1.bdf,courO24-L1.bdf)
LinkFile(courR08-ISO8859-1.bdf,courR08-L1.bdf)
LinkFile(courR10-ISO8859-1.bdf,courR10-L1.bdf)
LinkFile(courR12-ISO8859-1.bdf,courR12-L1.bdf)
LinkFile(courR14-ISO8859-1.bdf,courR14-L1.bdf)
LinkFile(courR18-ISO8859-1.bdf,courR18-L1.bdf)
LinkFile(courR24-ISO8859-1.bdf,courR24-L1.bdf)
LinkFile(helvB08-ISO8859-1.bdf,helvB08-L1.bdf)
LinkFile(helvB10-ISO8859-1.bdf,helvB10-L1.bdf)
LinkFile(helvB12-ISO8859-1.bdf,helvB12-L1.bdf)
LinkFile(helvB14-ISO8859-1.bdf,helvB14-L1.bdf)
LinkFile(helvB18-ISO8859-1.bdf,helvB18-L1.bdf)
LinkFile(helvB24-ISO8859-1.bdf,helvB24-L1.bdf)
LinkFile(helvBO08-ISO8859-1.bdf,helvBO08-L1.bdf)
LinkFile(helvBO10-ISO8859-1.bdf,helvBO10-L1.bdf)
LinkFile(helvBO12-ISO8859-1.bdf,helvBO12-L1.bdf)
LinkFile(helvBO14-ISO8859-1.bdf,helvBO14-L1.bdf)
LinkFile(helvBO18-ISO8859-1.bdf,helvBO18-L1.bdf)
LinkFile(helvBO24-ISO8859-1.bdf,helvBO24-L1.bdf)
LinkFile(helvO08-ISO8859-1.bdf,helvO08-L1.bdf)
LinkFile(helvO10-ISO8859-1.bdf,helvO10-L1.bdf)
LinkFile(helvO12-ISO8859-1.bdf,helvO12-L1.bdf)
LinkFile(helvO14-ISO8859-1.bdf,helvO14-L1.bdf)
LinkFile(helvO18-ISO8859-1.bdf,helvO18-L1.bdf)
LinkFile(helvO24-ISO8859-1.bdf,helvO24-L1.bdf)
LinkFile(helvR08-ISO8859-1.bdf,helvR08-L1.bdf)
LinkFile(helvR10-ISO8859-1.bdf,helvR10-L1.bdf)
LinkFile(helvR12-ISO8859-1.bdf,helvR12-L1.bdf)
LinkFile(helvR14-ISO8859-1.bdf,helvR14-L1.bdf)
LinkFile(helvR18-ISO8859-1.bdf,helvR18-L1.bdf)
LinkFile(helvR24-ISO8859-1.bdf,helvR24-L1.bdf)
LinkFile(ncenB08-ISO8859-1.bdf,ncenB08-L1.bdf)
LinkFile(ncenB10-ISO8859-1.bdf,ncenB10-L1.bdf)
LinkFile(ncenB12-ISO8859-1.bdf,ncenB12-L1.bdf)
LinkFile(ncenB14-ISO8859-1.bdf,ncenB14-L1.bdf)
LinkFile(ncenB18-ISO8859-1.bdf,ncenB18-L1.bdf)
LinkFile(ncenB24-ISO8859-1.bdf,ncenB24-L1.bdf)
LinkFile(ncenBI08-ISO8859-1.bdf,ncenBI08-L1.bdf)
LinkFile(ncenBI10-ISO8859-1.bdf,ncenBI10-L1.bdf)
LinkFile(ncenBI12-ISO8859-1.bdf,ncenBI12-L1.bdf)
LinkFile(ncenBI14-ISO8859-1.bdf,ncenBI14-L1.bdf)
LinkFile(ncenBI18-ISO8859-1.bdf,ncenBI18-L1.bdf)
LinkFile(ncenBI24-ISO8859-1.bdf,ncenBI24-L1.bdf)
LinkFile(ncenI08-ISO8859-1.bdf,ncenI08-L1.bdf)
LinkFile(ncenI10-ISO8859-1.bdf,ncenI10-L1.bdf)
LinkFile(ncenI12-ISO8859-1.bdf,ncenI12-L1.bdf)
LinkFile(ncenI14-ISO8859-1.bdf,ncenI14-L1.bdf)
LinkFile(ncenI18-ISO8859-1.bdf,ncenI18-L1.bdf)
LinkFile(ncenI24-ISO8859-1.bdf,ncenI24-L1.bdf)
LinkFile(ncenR08-ISO8859-1.bdf,ncenR08-L1.bdf)
LinkFile(ncenR10-ISO8859-1.bdf,ncenR10-L1.bdf)
LinkFile(ncenR12-ISO8859-1.bdf,ncenR12-L1.bdf)
LinkFile(ncenR14-ISO8859-1.bdf,ncenR14-L1.bdf)
LinkFile(ncenR18-ISO8859-1.bdf,ncenR18-L1.bdf)
LinkFile(ncenR24-ISO8859-1.bdf,ncenR24-L1.bdf)
LinkFile(timB08-ISO8859-1.bdf,timB08-L1.bdf)
LinkFile(timB10-ISO8859-1.bdf,timB10-L1.bdf)
LinkFile(timB12-ISO8859-1.bdf,timB12-L1.bdf)
LinkFile(timB14-ISO8859-1.bdf,timB14-L1.bdf)
LinkFile(timB18-ISO8859-1.bdf,timB18-L1.bdf)
LinkFile(timB24-ISO8859-1.bdf,timB24-L1.bdf)
LinkFile(timBI08-ISO8859-1.bdf,timBI08-L1.bdf)
LinkFile(timBI10-ISO8859-1.bdf,timBI10-L1.bdf)
LinkFile(timBI12-ISO8859-1.bdf,timBI12-L1.bdf)
LinkFile(timBI14-ISO8859-1.bdf,timBI14-L1.bdf)
LinkFile(timBI18-ISO8859-1.bdf,timBI18-L1.bdf)
LinkFile(timBI24-ISO8859-1.bdf,timBI24-L1.bdf)
LinkFile(timI08-ISO8859-1.bdf,timI08-L1.bdf)
LinkFile(timI10-ISO8859-1.bdf,timI10-L1.bdf)
LinkFile(timI12-ISO8859-1.bdf,timI12-L1.bdf)
LinkFile(timI14-ISO8859-1.bdf,timI14-L1.bdf)
LinkFile(timI18-ISO8859-1.bdf,timI18-L1.bdf)
LinkFile(timI24-ISO8859-1.bdf,timI24-L1.bdf)
LinkFile(timR08-ISO8859-1.bdf,timR08-L1.bdf)
LinkFile(timR10-ISO8859-1.bdf,timR10-L1.bdf)
LinkFile(timR12-ISO8859-1.bdf,timR12-L1.bdf)
LinkFile(timR14-ISO8859-1.bdf,timR14-L1.bdf)
LinkFile(timR18-ISO8859-1.bdf,timR18-L1.bdf)
LinkFile(timR24-ISO8859-1.bdf,timR24-L1.bdf)
LinkFile(luBIS08-ISO8859-1.bdf,luBIS08-L1.bdf)
LinkFile(luBIS10-ISO8859-1.bdf,luBIS10-L1.bdf)
LinkFile(luBIS12-ISO8859-1.bdf,luBIS12-L1.bdf)
LinkFile(luBIS14-ISO8859-1.bdf,luBIS14-L1.bdf)
LinkFile(luBIS18-ISO8859-1.bdf,luBIS18-L1.bdf)
LinkFile(luBIS19-ISO8859-1.bdf,luBIS19-L1.bdf)
LinkFile(luBIS24-ISO8859-1.bdf,luBIS24-L1.bdf)
LinkFile(luBS08-ISO8859-1.bdf,luBS08-L1.bdf)
LinkFile(luBS10-ISO8859-1.bdf,luBS10-L1.bdf)
LinkFile(luBS12-ISO8859-1.bdf,luBS12-L1.bdf)
LinkFile(luBS14-ISO8859-1.bdf,luBS14-L1.bdf)
LinkFile(luBS18-ISO8859-1.bdf,luBS18-L1.bdf)
LinkFile(luBS19-ISO8859-1.bdf,luBS19-L1.bdf)
LinkFile(luBS24-ISO8859-1.bdf,luBS24-L1.bdf)
LinkFile(luIS08-ISO8859-1.bdf,luIS08-L1.bdf)
LinkFile(luIS10-ISO8859-1.bdf,luIS10-L1.bdf)
LinkFile(luIS12-ISO8859-1.bdf,luIS12-L1.bdf)
LinkFile(luIS14-ISO8859-1.bdf,luIS14-L1.bdf)
LinkFile(luIS18-ISO8859-1.bdf,luIS18-L1.bdf)
LinkFile(luIS19-ISO8859-1.bdf,luIS19-L1.bdf)
LinkFile(luIS24-ISO8859-1.bdf,luIS24-L1.bdf)
LinkFile(luRS08-ISO8859-1.bdf,luRS08-L1.bdf)
LinkFile(luRS10-ISO8859-1.bdf,luRS10-L1.bdf)
LinkFile(luRS12-ISO8859-1.bdf,luRS12-L1.bdf)
LinkFile(luRS14-ISO8859-1.bdf,luRS14-L1.bdf)
LinkFile(luRS18-ISO8859-1.bdf,luRS18-L1.bdf)
LinkFile(luRS19-ISO8859-1.bdf,luRS19-L1.bdf)
LinkFile(luRS24-ISO8859-1.bdf,luRS24-L1.bdf)
LinkFile(lubB08-ISO8859-1.bdf,lubB08-L1.bdf)
LinkFile(lubB10-ISO8859-1.bdf,lubB10-L1.bdf)
LinkFile(lubB12-ISO8859-1.bdf,lubB12-L1.bdf)
LinkFile(lubB14-ISO8859-1.bdf,lubB14-L1.bdf)
LinkFile(lubB18-ISO8859-1.bdf,lubB18-L1.bdf)
LinkFile(lubB19-ISO8859-1.bdf,lubB19-L1.bdf)
LinkFile(lubB24-ISO8859-1.bdf,lubB24-L1.bdf)
LinkFile(lubBI08-ISO8859-1.bdf,lubBI08-L1.bdf)
LinkFile(lubBI10-ISO8859-1.bdf,lubBI10-L1.bdf)
LinkFile(lubBI12-ISO8859-1.bdf,lubBI12-L1.bdf)
LinkFile(lubBI14-ISO8859-1.bdf,lubBI14-L1.bdf)
LinkFile(lubBI18-ISO8859-1.bdf,lubBI18-L1.bdf)
LinkFile(lubBI19-ISO8859-1.bdf,lubBI19-L1.bdf)
LinkFile(lubBI24-ISO8859-1.bdf,lubBI24-L1.bdf)
LinkFile(lubI08-ISO8859-1.bdf,lubI08-L1.bdf)
LinkFile(lubI10-ISO8859-1.bdf,lubI10-L1.bdf)
LinkFile(lubI12-ISO8859-1.bdf,lubI12-L1.bdf)
LinkFile(lubI14-ISO8859-1.bdf,lubI14-L1.bdf)
LinkFile(lubI18-ISO8859-1.bdf,lubI18-L1.bdf)
LinkFile(lubI19-ISO8859-1.bdf,lubI19-L1.bdf)
LinkFile(lubI24-ISO8859-1.bdf,lubI24-L1.bdf)
LinkFile(lubR08-ISO8859-1.bdf,lubR08-L1.bdf)
LinkFile(lubR10-ISO8859-1.bdf,lubR10-L1.bdf)
LinkFile(lubR12-ISO8859-1.bdf,lubR12-L1.bdf)
LinkFile(lubR14-ISO8859-1.bdf,lubR14-L1.bdf)
LinkFile(lubR18-ISO8859-1.bdf,lubR18-L1.bdf)
LinkFile(lubR19-ISO8859-1.bdf,lubR19-L1.bdf)
LinkFile(lubR24-ISO8859-1.bdf,lubR24-L1.bdf)
LinkFile(lutBS08-ISO8859-1.bdf,lutBS08-L1.bdf)
LinkFile(lutBS10-ISO8859-1.bdf,lutBS10-L1.bdf)
LinkFile(lutBS12-ISO8859-1.bdf,lutBS12-L1.bdf)
LinkFile(lutBS14-ISO8859-1.bdf,lutBS14-L1.bdf)
LinkFile(lutBS18-ISO8859-1.bdf,lutBS18-L1.bdf)
LinkFile(lutBS19-ISO8859-1.bdf,lutBS19-L1.bdf)
LinkFile(lutBS24-ISO8859-1.bdf,lutBS24-L1.bdf)
LinkFile(lutRS08-ISO8859-1.bdf,lutRS08-L1.bdf)
LinkFile(lutRS10-ISO8859-1.bdf,lutRS10-L1.bdf)
LinkFile(lutRS12-ISO8859-1.bdf,lutRS12-L1.bdf)
LinkFile(lutRS14-ISO8859-1.bdf,lutRS14-L1.bdf)
LinkFile(lutRS18-ISO8859-1.bdf,lutRS18-L1.bdf)
LinkFile(lutRS19-ISO8859-1.bdf,lutRS19-L1.bdf)
LinkFile(lutRS24-ISO8859-1.bdf,lutRS24-L1.bdf)
LinkFile(UTBI__10-ISO8859-1.bdf,UTBI__10-L1.bdf)
LinkFile(UTBI__12-ISO8859-1.bdf,UTBI__12-L1.bdf)
LinkFile(UTBI__14-ISO8859-1.bdf,UTBI__14-L1.bdf)
LinkFile(UTBI__18-ISO8859-1.bdf,UTBI__18-L1.bdf)
LinkFile(UTBI__24-ISO8859-1.bdf,UTBI__24-L1.bdf)
LinkFile(UTB___10-ISO8859-1.bdf,UTB___10-L1.bdf)
LinkFile(UTB___12-ISO8859-1.bdf,UTB___12-L1.bdf)
LinkFile(UTB___14-ISO8859-1.bdf,UTB___14-L1.bdf)
LinkFile(UTB___18-ISO8859-1.bdf,UTB___18-L1.bdf)
LinkFile(UTB___24-ISO8859-1.bdf,UTB___24-L1.bdf)
LinkFile(UTI___10-ISO8859-1.bdf,UTI___10-L1.bdf)
LinkFile(UTI___12-ISO8859-1.bdf,UTI___12-L1.bdf)
LinkFile(UTI___14-ISO8859-1.bdf,UTI___14-L1.bdf)
LinkFile(UTI___18-ISO8859-1.bdf,UTI___18-L1.bdf)
LinkFile(UTI___24-ISO8859-1.bdf,UTI___24-L1.bdf)
LinkFile(UTRG__10-ISO8859-1.bdf,UTRG__10-L1.bdf)
LinkFile(UTRG__12-ISO8859-1.bdf,UTRG__12-L1.bdf)
LinkFile(UTRG__14-ISO8859-1.bdf,UTRG__14-L1.bdf)
LinkFile(UTRG__18-ISO8859-1.bdf,UTRG__18-L1.bdf)
LinkFile(UTRG__24-ISO8859-1.bdf,UTRG__24-L1.bdf)
#endif

#endif /* DerivedUCSFonts */

UpdateDerivedFont(courB08-ISO8859-1.bdf,courB08-L1.bdf)
UpdateDerivedFont(courB10-ISO8859-1.bdf,courB10-L1.bdf)
UpdateDerivedFont(courB12-ISO8859-1.bdf,courB12-L1.bdf)
UpdateDerivedFont(courB14-ISO8859-1.bdf,courB14-L1.bdf)
UpdateDerivedFont(courB18-ISO8859-1.bdf,courB18-L1.bdf)
UpdateDerivedFont(courB24-ISO8859-1.bdf,courB24-L1.bdf)
UpdateDerivedFont(courBO08-ISO8859-1.bdf,courBO08-L1.bdf)
UpdateDerivedFont(courBO10-ISO8859-1.bdf,courBO10-L1.bdf)
UpdateDerivedFont(courBO12-ISO8859-1.bdf,courBO12-L1.bdf)
UpdateDerivedFont(courBO14-ISO8859-1.bdf,courBO14-L1.bdf)
UpdateDerivedFont(courBO18-ISO8859-1.bdf,courBO18-L1.bdf)
UpdateDerivedFont(courBO24-ISO8859-1.bdf,courBO24-L1.bdf)
UpdateDerivedFont(courO08-ISO8859-1.bdf,courO08-L1.bdf)
UpdateDerivedFont(courO10-ISO8859-1.bdf,courO10-L1.bdf)
UpdateDerivedFont(courO12-ISO8859-1.bdf,courO12-L1.bdf)
UpdateDerivedFont(courO14-ISO8859-1.bdf,courO14-L1.bdf)
UpdateDerivedFont(courO18-ISO8859-1.bdf,courO18-L1.bdf)
UpdateDerivedFont(courO24-ISO8859-1.bdf,courO24-L1.bdf)
UpdateDerivedFont(courR08-ISO8859-1.bdf,courR08-L1.bdf)
UpdateDerivedFont(courR10-ISO8859-1.bdf,courR10-L1.bdf)
UpdateDerivedFont(courR12-ISO8859-1.bdf,courR12-L1.bdf)
UpdateDerivedFont(courR14-ISO8859-1.bdf,courR14-L1.bdf)
UpdateDerivedFont(courR18-ISO8859-1.bdf,courR18-L1.bdf)
UpdateDerivedFont(courR24-ISO8859-1.bdf,courR24-L1.bdf)
UpdateDerivedFont(helvB08-ISO8859-1.bdf,helvB08-L1.bdf)
UpdateDerivedFont(helvB10-ISO8859-1.bdf,helvB10-L1.bdf)
UpdateDerivedFont(helvB12-ISO8859-1.bdf,helvB12-L1.bdf)
UpdateDerivedFont(helvB14-ISO8859-1.bdf,helvB14-L1.bdf)
UpdateDerivedFont(helvB18-ISO8859-1.bdf,helvB18-L1.bdf)
UpdateDerivedFont(helvB24-ISO8859-1.bdf,helvB24-L1.bdf)
UpdateDerivedFont(helvBO08-ISO8859-1.bdf,helvBO08-L1.bdf)
UpdateDerivedFont(helvBO10-ISO8859-1.bdf,helvBO10-L1.bdf)
UpdateDerivedFont(helvBO12-ISO8859-1.bdf,helvBO12-L1.bdf)
UpdateDerivedFont(helvBO14-ISO8859-1.bdf,helvBO14-L1.bdf)
UpdateDerivedFont(helvBO18-ISO8859-1.bdf,helvBO18-L1.bdf)
UpdateDerivedFont(helvBO24-ISO8859-1.bdf,helvBO24-L1.bdf)
UpdateDerivedFont(helvO08-ISO8859-1.bdf,helvO08-L1.bdf)
UpdateDerivedFont(helvO10-ISO8859-1.bdf,helvO10-L1.bdf)
UpdateDerivedFont(helvO12-ISO8859-1.bdf,helvO12-L1.bdf)
UpdateDerivedFont(helvO14-ISO8859-1.bdf,helvO14-L1.bdf)
UpdateDerivedFont(helvO18-ISO8859-1.bdf,helvO18-L1.bdf)
UpdateDerivedFont(helvO24-ISO8859-1.bdf,helvO24-L1.bdf)
UpdateDerivedFont(helvR08-ISO8859-1.bdf,helvR08-L1.bdf)
UpdateDerivedFont(helvR10-ISO8859-1.bdf,helvR10-L1.bdf)
UpdateDerivedFont(helvR12-ISO8859-1.bdf,helvR12-L1.bdf)
UpdateDerivedFont(helvR14-ISO8859-1.bdf,helvR14-L1.bdf)
UpdateDerivedFont(helvR18-ISO8859-1.bdf,helvR18-L1.bdf)
UpdateDerivedFont(helvR24-ISO8859-1.bdf,helvR24-L1.bdf)
UpdateDerivedFont(ncenB08-ISO8859-1.bdf,ncenB08-L1.bdf)
UpdateDerivedFont(ncenB10-ISO8859-1.bdf,ncenB10-L1.bdf)
UpdateDerivedFont(ncenB12-ISO8859-1.bdf,ncenB12-L1.bdf)
UpdateDerivedFont(ncenB14-ISO8859-1.bdf,ncenB14-L1.bdf)
UpdateDerivedFont(ncenB18-ISO8859-1.bdf,ncenB18-L1.bdf)
UpdateDerivedFont(ncenB24-ISO8859-1.bdf,ncenB24-L1.bdf)
UpdateDerivedFont(ncenBI08-ISO8859-1.bdf,ncenBI08-L1.bdf)
UpdateDerivedFont(ncenBI10-ISO8859-1.bdf,ncenBI10-L1.bdf)
UpdateDerivedFont(ncenBI12-ISO8859-1.bdf,ncenBI12-L1.bdf)
UpdateDerivedFont(ncenBI14-ISO8859-1.bdf,ncenBI14-L1.bdf)
UpdateDerivedFont(ncenBI18-ISO8859-1.bdf,ncenBI18-L1.bdf)
UpdateDerivedFont(ncenBI24-ISO8859-1.bdf,ncenBI24-L1.bdf)
UpdateDerivedFont(ncenI08-ISO8859-1.bdf,ncenI08-L1.bdf)
UpdateDerivedFont(ncenI10-ISO8859-1.bdf,ncenI10-L1.bdf)
UpdateDerivedFont(ncenI12-ISO8859-1.bdf,ncenI12-L1.bdf)
UpdateDerivedFont(ncenI14-ISO8859-1.bdf,ncenI14-L1.bdf)
UpdateDerivedFont(ncenI18-ISO8859-1.bdf,ncenI18-L1.bdf)
UpdateDerivedFont(ncenI24-ISO8859-1.bdf,ncenI24-L1.bdf)
UpdateDerivedFont(ncenR08-ISO8859-1.bdf,ncenR08-L1.bdf)
UpdateDerivedFont(ncenR10-ISO8859-1.bdf,ncenR10-L1.bdf)
UpdateDerivedFont(ncenR12-ISO8859-1.bdf,ncenR12-L1.bdf)
UpdateDerivedFont(ncenR14-ISO8859-1.bdf,ncenR14-L1.bdf)
UpdateDerivedFont(ncenR18-ISO8859-1.bdf,ncenR18-L1.bdf)
UpdateDerivedFont(ncenR24-ISO8859-1.bdf,ncenR24-L1.bdf)
UpdateDerivedFont(timB08-ISO8859-1.bdf,timB08-L1.bdf)
UpdateDerivedFont(timB10-ISO8859-1.bdf,timB10-L1.bdf)
UpdateDerivedFont(timB12-ISO8859-1.bdf,timB12-L1.bdf)
UpdateDerivedFont(timB14-ISO8859-1.bdf,timB14-L1.bdf)
UpdateDerivedFont(timB18-ISO8859-1.bdf,timB18-L1.bdf)
UpdateDerivedFont(timB24-ISO8859-1.bdf,timB24-L1.bdf)
UpdateDerivedFont(timBI08-ISO8859-1.bdf,timBI08-L1.bdf)
UpdateDerivedFont(timBI10-ISO8859-1.bdf,timBI10-L1.bdf)
UpdateDerivedFont(timBI12-ISO8859-1.bdf,timBI12-L1.bdf)
UpdateDerivedFont(timBI14-ISO8859-1.bdf,timBI14-L1.bdf)
UpdateDerivedFont(timBI18-ISO8859-1.bdf,timBI18-L1.bdf)
UpdateDerivedFont(timBI24-ISO8859-1.bdf,timBI24-L1.bdf)
UpdateDerivedFont(timI08-ISO8859-1.bdf,timI08-L1.bdf)
UpdateDerivedFont(timI10-ISO8859-1.bdf,timI10-L1.bdf)
UpdateDerivedFont(timI12-ISO8859-1.bdf,timI12-L1.bdf)
UpdateDerivedFont(timI14-ISO8859-1.bdf,timI14-L1.bdf)
UpdateDerivedFont(timI18-ISO8859-1.bdf,timI18-L1.bdf)
UpdateDerivedFont(timI24-ISO8859-1.bdf,timI24-L1.bdf)
UpdateDerivedFont(timR08-ISO8859-1.bdf,timR08-L1.bdf)
UpdateDerivedFont(timR10-ISO8859-1.bdf,timR10-L1.bdf)
UpdateDerivedFont(timR12-ISO8859-1.bdf,timR12-L1.bdf)
UpdateDerivedFont(timR14-ISO8859-1.bdf,timR14-L1.bdf)
UpdateDerivedFont(timR18-ISO8859-1.bdf,timR18-L1.bdf)
UpdateDerivedFont(timR24-ISO8859-1.bdf,timR24-L1.bdf)
UpdateDerivedFont(luBIS08-ISO8859-1.bdf,luBIS08-L1.bdf)
UpdateDerivedFont(luBIS10-ISO8859-1.bdf,luBIS10-L1.bdf)
UpdateDerivedFont(luBIS12-ISO8859-1.bdf,luBIS12-L1.bdf)
UpdateDerivedFont(luBIS14-ISO8859-1.bdf,luBIS14-L1.bdf)
UpdateDerivedFont(luBIS18-ISO8859-1.bdf,luBIS18-L1.bdf)
UpdateDerivedFont(luBIS19-ISO8859-1.bdf,luBIS19-L1.bdf)
UpdateDerivedFont(luBIS24-ISO8859-1.bdf,luBIS24-L1.bdf)
UpdateDerivedFont(luBS08-ISO8859-1.bdf,luBS08-L1.bdf)
UpdateDerivedFont(luBS10-ISO8859-1.bdf,luBS10-L1.bdf)
UpdateDerivedFont(luBS12-ISO8859-1.bdf,luBS12-L1.bdf)
UpdateDerivedFont(luBS14-ISO8859-1.bdf,luBS14-L1.bdf)
UpdateDerivedFont(luBS18-ISO8859-1.bdf,luBS18-L1.bdf)
UpdateDerivedFont(luBS19-ISO8859-1.bdf,luBS19-L1.bdf)
UpdateDerivedFont(luBS24-ISO8859-1.bdf,luBS24-L1.bdf)
UpdateDerivedFont(luIS08-ISO8859-1.bdf,luIS08-L1.bdf)
UpdateDerivedFont(luIS10-ISO8859-1.bdf,luIS10-L1.bdf)
UpdateDerivedFont(luIS12-ISO8859-1.bdf,luIS12-L1.bdf)
UpdateDerivedFont(luIS14-ISO8859-1.bdf,luIS14-L1.bdf)
UpdateDerivedFont(luIS18-ISO8859-1.bdf,luIS18-L1.bdf)
UpdateDerivedFont(luIS19-ISO8859-1.bdf,luIS19-L1.bdf)
UpdateDerivedFont(luIS24-ISO8859-1.bdf,luIS24-L1.bdf)
UpdateDerivedFont(luRS08-ISO8859-1.bdf,luRS08-L1.bdf)
UpdateDerivedFont(luRS10-ISO8859-1.bdf,luRS10-L1.bdf)
UpdateDerivedFont(luRS12-ISO8859-1.bdf,luRS12-L1.bdf)
UpdateDerivedFont(luRS14-ISO8859-1.bdf,luRS14-L1.bdf)
UpdateDerivedFont(luRS18-ISO8859-1.bdf,luRS18-L1.bdf)
UpdateDerivedFont(luRS19-ISO8859-1.bdf,luRS19-L1.bdf)
UpdateDerivedFont(luRS24-ISO8859-1.bdf,luRS24-L1.bdf)
UpdateDerivedFont(lubB08-ISO8859-1.bdf,lubB08-L1.bdf)
UpdateDerivedFont(lubB10-ISO8859-1.bdf,lubB10-L1.bdf)
UpdateDerivedFont(lubB12-ISO8859-1.bdf,lubB12-L1.bdf)
UpdateDerivedFont(lubB14-ISO8859-1.bdf,lubB14-L1.bdf)
UpdateDerivedFont(lubB18-ISO8859-1.bdf,lubB18-L1.bdf)
UpdateDerivedFont(lubB19-ISO8859-1.bdf,lubB19-L1.bdf)
UpdateDerivedFont(lubB24-ISO8859-1.bdf,lubB24-L1.bdf)
UpdateDerivedFont(lubBI08-ISO8859-1.bdf,lubBI08-L1.bdf)
UpdateDerivedFont(lubBI10-ISO8859-1.bdf,lubBI10-L1.bdf)
UpdateDerivedFont(lubBI12-ISO8859-1.bdf,lubBI12-L1.bdf)
UpdateDerivedFont(lubBI14-ISO8859-1.bdf,lubBI14-L1.bdf)
UpdateDerivedFont(lubBI18-ISO8859-1.bdf,lubBI18-L1.bdf)
UpdateDerivedFont(lubBI19-ISO8859-1.bdf,lubBI19-L1.bdf)
UpdateDerivedFont(lubBI24-ISO8859-1.bdf,lubBI24-L1.bdf)
UpdateDerivedFont(lubI08-ISO8859-1.bdf,lubI08-L1.bdf)
UpdateDerivedFont(lubI10-ISO8859-1.bdf,lubI10-L1.bdf)
UpdateDerivedFont(lubI12-ISO8859-1.bdf,lubI12-L1.bdf)
UpdateDerivedFont(lubI14-ISO8859-1.bdf,lubI14-L1.bdf)
UpdateDerivedFont(lubI18-ISO8859-1.bdf,lubI18-L1.bdf)
UpdateDerivedFont(lubI19-ISO8859-1.bdf,lubI19-L1.bdf)
UpdateDerivedFont(lubI24-ISO8859-1.bdf,lubI24-L1.bdf)
UpdateDerivedFont(lubR08-ISO8859-1.bdf,lubR08-L1.bdf)
UpdateDerivedFont(lubR10-ISO8859-1.bdf,lubR10-L1.bdf)
UpdateDerivedFont(lubR12-ISO8859-1.bdf,lubR12-L1.bdf)
UpdateDerivedFont(lubR14-ISO8859-1.bdf,lubR14-L1.bdf)
UpdateDerivedFont(lubR18-ISO8859-1.bdf,lubR18-L1.bdf)
UpdateDerivedFont(lubR19-ISO8859-1.bdf,lubR19-L1.bdf)
UpdateDerivedFont(lubR24-ISO8859-1.bdf,lubR24-L1.bdf)
UpdateDerivedFont(lutBS08-ISO8859-1.bdf,lutBS08-L1.bdf)
UpdateDerivedFont(lutBS10-ISO8859-1.bdf,lutBS10-L1.bdf)
UpdateDerivedFont(lutBS12-ISO8859-1.bdf,lutBS12-L1.bdf)
UpdateDerivedFont(lutBS14-ISO8859-1.bdf,lutBS14-L1.bdf)
UpdateDerivedFont(lutBS18-ISO8859-1.bdf,lutBS18-L1.bdf)
UpdateDerivedFont(lutBS19-ISO8859-1.bdf,lutBS19-L1.bdf)
UpdateDerivedFont(lutBS24-ISO8859-1.bdf,lutBS24-L1.bdf)
UpdateDerivedFont(lutRS08-ISO8859-1.bdf,lutRS08-L1.bdf)
UpdateDerivedFont(lutRS10-ISO8859-1.bdf,lutRS10-L1.bdf)
UpdateDerivedFont(lutRS12-ISO8859-1.bdf,lutRS12-L1.bdf)
UpdateDerivedFont(lutRS14-ISO8859-1.bdf,lutRS14-L1.bdf)
UpdateDerivedFont(lutRS18-ISO8859-1.bdf,lutRS18-L1.bdf)
UpdateDerivedFont(lutRS19-ISO8859-1.bdf,lutRS19-L1.bdf)
UpdateDerivedFont(lutRS24-ISO8859-1.bdf,lutRS24-L1.bdf)
UpdateDerivedFont(UTBI__10-ISO8859-1.bdf,UTBI__10-L1.bdf)
UpdateDerivedFont(UTBI__12-ISO8859-1.bdf,UTBI__12-L1.bdf)
UpdateDerivedFont(UTBI__14-ISO8859-1.bdf,UTBI__14-L1.bdf)
UpdateDerivedFont(UTBI__18-ISO8859-1.bdf,UTBI__18-L1.bdf)
UpdateDerivedFont(UTBI__24-ISO8859-1.bdf,UTBI__24-L1.bdf)
UpdateDerivedFont(UTB___10-ISO8859-1.bdf,UTB___10-L1.bdf)
UpdateDerivedFont(UTB___12-ISO8859-1.bdf,UTB___12-L1.bdf)
UpdateDerivedFont(UTB___14-ISO8859-1.bdf,UTB___14-L1.bdf)
UpdateDerivedFont(UTB___18-ISO8859-1.bdf,UTB___18-L1.bdf)
UpdateDerivedFont(UTB___24-ISO8859-1.bdf,UTB___24-L1.bdf)
UpdateDerivedFont(UTI___10-ISO8859-1.bdf,UTI___10-L1.bdf)
UpdateDerivedFont(UTI___12-ISO8859-1.bdf,UTI___12-L1.bdf)
UpdateDerivedFont(UTI___14-ISO8859-1.bdf,UTI___14-L1.bdf)
UpdateDerivedFont(UTI___18-ISO8859-1.bdf,UTI___18-L1.bdf)
UpdateDerivedFont(UTI___24-ISO8859-1.bdf,UTI___24-L1.bdf)
UpdateDerivedFont(UTRG__10-ISO8859-1.bdf,UTRG__10-L1.bdf)
UpdateDerivedFont(UTRG__12-ISO8859-1.bdf,UTRG__12-L1.bdf)
UpdateDerivedFont(UTRG__14-ISO8859-1.bdf,UTRG__14-L1.bdf)
UpdateDerivedFont(UTRG__18-ISO8859-1.bdf,UTRG__18-L1.bdf)
UpdateDerivedFont(UTRG__24-ISO8859-1.bdf,UTRG__24-L1.bdf)

