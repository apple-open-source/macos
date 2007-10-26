/*
 * Copyright (c) 2003, 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#if defined (__ppc__) || defined(ppc)

	.section	__TEXT, __VLib_Container, regular, no_dead_strip

	.align	2

VLib_Origin:

	.long	0xF04D6163
	.long	0x564C6962
	.long	1
	.long	VLib_Strings - VLib_Origin
	.long	VLib_HashTable - VLib_Origin
	.long	VLib_HashKeys - VLib_Origin
	.long	VLib_ExportSymbols - VLib_Origin
	.long	VLib_ExportNames - VLib_Origin
	.long	5
	.long	205
	.long	0
	.long	14
	.long	15
	.long	50
	.long	0x70777063
	.long	0x00000000
	.long	0xC1DAAAB5
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000

VLib_Strings:

	.ascii	"CFMPriv_System"
	.byte	0
	.ascii	"/System/Library/Frameworks/System.framework/System"
	.byte	0

	.align	2

VLib_HashTable:

	.long	0x00100000
	.long	0x00140004
	.long	0x00200009
	.long	0x001C0011
	.long	0x00240018
	.long	0x00180021
	.long	0x00240027
	.long	0x00200030
	.long	0x00180038
	.long	0x0020003E
	.long	0x00140046
	.long	0x0018004B
	.long	0x001C0051
	.long	0x00100058
	.long	0x000C005C
	.long	0x0024005F
	.long	0x00100068
	.long	0x0018006C
	.long	0x000C0072
	.long	0x001C0075
	.long	0x000C007C
	.long	0x0008007F
	.long	0x00140081
	.long	0x00200086
	.long	0x0028008E
	.long	0x00240098
	.long	0x001800A1
	.long	0x001C00A7
	.long	0x002000AE
	.long	0x002C00B6
	.long	0x001C00C1
	.long	0x001400C8

VLib_HashKeys:

	.long	0x000B82F7
	.long	0x00071021
	.long	0x00060A94
	.long	0x000717DE
	.long	0x00060909
	.long	0x00040253
	.long	0x00030109
	.long	0x0006098D
	.long	0x00060AB4
	.long	0x000B82F5
	.long	0x0003010A
	.long	0x00040233
	.long	0x00030169
	.long	0x0007179E
	.long	0x00060B9E
	.long	0x00071548
	.long	0x00060A33
	.long	0x00040232
	.long	0x00050441
	.long	0x000504E4
	.long	0x00050422
	.long	0x00094B1B
	.long	0x00040270
	.long	0x0007173A
	.long	0x00050480
	.long	0x000B13B9
	.long	0x000B07B9
	.long	0x000504C2
	.long	0x00071188
	.long	0x00060B5E
	.long	0x000828C2
	.long	0x00060BDA
	.long	0x00040290
	.long	0x00050447
	.long	0x00040257
	.long	0x00060866
	.long	0x000609EA
	.long	0x000717FA
	.long	0x00071589
	.long	0x00050616
	.long	0x0006090E
	.long	0x0008214C
	.long	0x00050444
	.long	0x0007137D
	.long	0x00040237
	.long	0x00040254
	.long	0x000A971E
	.long	0x000609AB
	.long	0x00040274
	.long	0x00060864
	.long	0x000A8274
	.long	0x000504E0
	.long	0x000504E0
	.long	0x00060B1F
	.long	0x00060A74
	.long	0x000505C9
	.long	0x000712DE
	.long	0x00030121
	.long	0x000712DE
	.long	0x00060A39
	.long	0x0004029C
	.long	0x000716DE
	.long	0x000820EE
	.long	0x00040238
	.long	0x00071028
	.long	0x00082372
	.long	0x0004027A
	.long	0x000402DF
	.long	0x000505A4
	.long	0x00060A5B
	.long	0x0004021A
	.long	0x000608AF
	.long	0x000504AF
	.long	0x000710CC
	.long	0x00060B96
	.long	0x00040278
	.long	0x000504CD
	.long	0x0007123A
	.long	0x00071770
	.long	0x00082E1B
	.long	0x00060BD5
	.long	0x0005046F
	.long	0x0004021C
	.long	0x000B0F77
	.long	0x0007106F
	.long	0x0004021C
	.long	0x00060BD2
	.long	0x000717B1
	.long	0x000ACAFA
	.long	0x0005042C
	.long	0x00060B76
	.long	0x00060BF2
	.long	0x00071182
	.long	0x00071582
	.long	0x000402BB
	.long	0x000ACAF8
	.long	0x0005048B
	.long	0x0004021F
	.long	0x000D02F8
	.long	0x0004023E
	.long	0x0004023E
	.long	0x0006082E
	.long	0x0006088B
	.long	0x00071736
	.long	0x00050410
	.long	0x0007138C
	.long	0x0005059C
	.long	0x000829DE
	.long	0x000711BC
	.long	0x00040220
	.long	0x000A90F6
	.long	0x00060AA4
	.long	0x00095328
	.long	0x00060AC7
	.long	0x000711FD
	.long	0x00071012
	.long	0x000504D4
	.long	0x00040260
	.long	0x00040203
	.long	0x000504F4
	.long	0x000609DD
	.long	0x000609BE
	.long	0x000A90F4
	.long	0x0007172A
	.long	0x000C3D3D
	.long	0x00050477
	.long	0x00060A67
	.long	0x00050457
	.long	0x00050476
	.long	0x00040227
	.long	0x0007136D
	.long	0x00050492
	.long	0x00060B8A
	.long	0x000505BB
	.long	0x000822A2
	.long	0x000A8874
	.long	0x00050436
	.long	0x00094E64
	.long	0x000717E8
	.long	0x000402A2
	.long	0x00082E64
	.long	0x0003017C
	.long	0x000D3629
	.long	0x00040229
	.long	0x0006089C
	.long	0x000C04BD
	.long	0x000609D6
	.long	0x000609F7
	.long	0x00082531
	.long	0x00071594
	.long	0x00060BA5
	.long	0x000717E7
	.long	0x0004026A
	.long	0x0008205B
	.long	0x000D5C38
	.long	0x00071038
	.long	0x00060911
	.long	0x000402CF
	.long	0x000402AC
	.long	0x00060BA4
	.long	0x00060BE6
	.long	0x000D362B
	.long	0x00030133
	.long	0x000504BF
	.long	0x000711B7
	.long	0x000402CC
	.long	0x00082C9E
	.long	0x0005043A
	.long	0x0004022A
	.long	0x0007109F
	.long	0x000710FC
	.long	0x000609B6
	.long	0x00030170
	.long	0x0008281B
	.long	0x0008301C
	.long	0x00050498
	.long	0x000504DA
	.long	0x0004026F
	.long	0x00030114
	.long	0x00030114
	.long	0x0004024E
	.long	0x00040288
	.long	0x000B8191
	.long	0x0006081D
	.long	0x00030134
	.long	0x0004024F
	.long	0x0004026E
	.long	0x0008226E
	.long	0x000402CB
	.long	0x00050534
	.long	0x00071705
	.long	0x0007145F
	.long	0x0007143C
	.long	0x0008301E
	.long	0x000712AB
	.long	0x00030116
	.long	0x000504BB
	.long	0x0006089A
	.long	0x00050516
	.long	0x00050516
	.long	0x000B8193
	.long	0x0005047C
	.long	0x0007145D
	.long	0x00060A4D
	.long	0x000955D1

VLib_ExportNames:

	.ascii	"writewctombwcstombsvsprintfvprintfvfprintfunlinkungetctrunctouppertolowertoasciitmpnamtmpfiletimetanhtansystemstrxfrmstrtoulstrtolstrtokstrtodstrstrstrspnstrrchrstrpbrkstrncpystrncmpstrncatstrlenstrft"
	.ascii	"imestrerrorstrcspnstrcpystrcollstrcmpstrchrstrcatsscanfsrandsqrtsprintfsinhsinsetvbufsetlocalesetbufscanfscalbroundtolroundrinttolrintrewindrenameremquoremoveremainderreallocreadrandqsortputwputsputch"
	.ascii	"arputcprintfpowperroropennextafterfnextafterdnearbyintnanfnanmodffmodfmktimememsetmemmovememcpymemcmpmemchrmbtowcmbstowcsmblenmalloclongjmplogblog2log1plog10loglocaltimelocaleconvlgammaldivldexplabsis"
	.ascii	"xdigitisupperisspaceispunctisprintislowerisgraphisdigitiscntrlisasciiisalphaisalnumioctlhypotgmtimegetwgetsgetpidgetenvgetchargetcgammafwriteftellfsetposfseekfscanffrexpfreopenfreefreadfputsfputcfprin"
	.ascii	"tffopenfmodfminfmaxfloorfgetsfgetposfgetcfflushfeupdateenvfetestexceptfesetroundfesetexceptfesetenvferrorferaiseexceptfeoffeholdexceptfegetroundfegetexceptfegetenvfeclearexceptfdopenfdimfcntlfclosefab"
	.ascii	"sexpm1exp2expexiterfcerfdupdivdifftimectimecoshcoscopysigncloseclockclearerrceilcallocbsearchatolatoiatofatexitatanhatan2atanasinhasinasctimeacoshacosabsabort__signbitf__signbitd__isnormalf__isnormald"
	.ascii	"__isnanf__isnand__isfinitef__isfinited__inf__fpclassifyf__fpclassifyd"

	.section	__TEXT, __VLib_Exports, regular, no_dead_strip

	.align	2

VLib_ExportSymbols:

	.long	0x020004CB
	.long	__isfinited_bp - VLib_Origin

	.long	0x0200029D
	.long	isalpha_bp - VLib_Origin

	.long	0x02000126
	.long	setbuf_bp - VLib_Origin

	.long	0x0200009A
	.long	strrchr_bp - VLib_Origin

	.long	0x020002DF
	.long	fwrite_bp - VLib_Origin

	.long	0x02000252
	.long	labs_bp - VLib_Origin

	.long	0x0200022D
	.long	log_bp - VLib_Origin

	.long	0x020001E9
	.long	memcpy_bp - VLib_Origin

	.long	0x0200014E
	.long	rewind_bp - VLib_Origin

	.long	0x020004C0
	.long	__isfinitef_bp - VLib_Origin

	.long	0x02000400
	.long	dup_bp - VLib_Origin

	.long	0x020002BB
	.long	getw_bp - VLib_Origin

	.long	0x0200019C
	.long	pow_bp - VLib_Origin

	.long	0x02000108
	.long	sprintf_bp - VLib_Origin

	.long	0x020000ED
	.long	strchr_bp - VLib_Origin

	.long	0x0200003B
	.long	toupper_bp - VLib_Origin

	.long	0x0200002A
	.long	unlink_bp - VLib_Origin

	.long	0x02000434
	.long	ceil_bp - VLib_Origin

	.long	0x02000427
	.long	clock_bp - VLib_Origin

	.long	0x020002FC
	.long	frexp_bp - VLib_Origin

	.long	0x020002B0
	.long	hypot_bp - VLib_Origin

	.long	0x02000230
	.long	localtime_bp - VLib_Origin

	.long	0x0200021B
	.long	logb_bp - VLib_Origin

	.long	0x020000D3
	.long	strcspn_bp - VLib_Origin

	.long	0x02000457
	.long	atanh_bp - VLib_Origin

	.long	0x020003B0
	.long	fegetexcept_bp - VLib_Origin

	.long	0x02000370
	.long	fesetexcept_bp - VLib_Origin

	.long	0x0200030C
	.long	fread_bp - VLib_Origin

	.long	0x0200025E
	.long	isupper_bp - VLib_Origin

	.long	0x02000196
	.long	printf_bp - VLib_Origin

	.long	0x02000136
	.long	roundtol_bp - VLib_Origin

	.long	0x02000088
	.long	strtod_bp - VLib_Origin

	.long	0x02000061
	.long	tanh_bp - VLib_Origin

	.long	0x02000344
	.long	fgetc_bp - VLib_Origin

	.long	0x02000308
	.long	free_bp - VLib_Origin

	.long	0x020002C3
	.long	getpid_bp - VLib_Origin

	.long	0x020001DC
	.long	memset_bp - VLib_Origin

	.long	0x02000075
	.long	strtoul_bp - VLib_Origin

	.long	0x02000056
	.long	tmpfile_bp - VLib_Origin

	.long	0x020004D6
	.long	__inf_bp - VLib_Origin

	.long	0x02000451
	.long	atexit_bp - VLib_Origin

	.long	0x0200041A
	.long	copysign_bp - VLib_Origin

	.long	0x020003DA
	.long	fcntl_bp - VLib_Origin

	.long	0x020002EA
	.long	fsetpos_bp - VLib_Origin

	.long	0x020002BF
	.long	gets_bp - VLib_Origin

	.long	0x02000249
	.long	ldiv_bp - VLib_Origin

	.long	0x02000239
	.long	localeconv_bp - VLib_Origin

	.long	0x020001D6
	.long	mktime_bp - VLib_Origin

	.long	0x02000461
	.long	atan_bp - VLib_Origin

	.long	0x02000383
	.long	ferror_bp - VLib_Origin

	.long	0x02000366
	.long	fesetround_bp - VLib_Origin

	.long	0x020002E5
	.long	ftell_bp - VLib_Origin

	.long	0x020002AB
	.long	ioctl_bp - VLib_Origin

	.long	0x02000068
	.long	system_bp - VLib_Origin

	.long	0x02000005
	.long	wctomb_bp - VLib_Origin

	.long	0x02000000
	.long	write_bp - VLib_Origin

	.long	0x0200043E
	.long	bsearch_bp - VLib_Origin

	.long	0x02000417
	.long	cos_bp - VLib_Origin

	.long	0x0200031B
	.long	fprintf_bp - VLib_Origin

	.long	0x0200015A
	.long	remquo_bp - VLib_Origin

	.long	0x0200014A
	.long	rint_bp - VLib_Origin

	.long	0x0200001B
	.long	vprintf_bp - VLib_Origin

	.long	0x0200037B
	.long	fesetenv_bp - VLib_Origin

	.long	0x0200032B
	.long	fmin_bp - VLib_Origin

	.long	0x020002CF
	.long	getchar_bp - VLib_Origin

	.long	0x02000256
	.long	isxdigit_bp - VLib_Origin

	.long	0x020001D2
	.long	modf_bp - VLib_Origin

	.long	0x02000192
	.long	putc_bp - VLib_Origin

	.long	0x0200017E
	.long	qsort_bp - VLib_Origin

	.long	0x02000154
	.long	rename_bp - VLib_Origin

	.long	0x020003EE
	.long	exp2_bp - VLib_Origin

	.long	0x020003DF
	.long	fclose_bp - VLib_Origin

	.long	0x02000311
	.long	fputs_bp - VLib_Origin

	.long	0x02000281
	.long	isgraph_bp - VLib_Origin

	.long	0x020000E7
	.long	strcmp_bp - VLib_Origin

	.long	0x0200046A
	.long	asin_bp - VLib_Origin

	.long	0x020002F1
	.long	fseek_bp - VLib_Origin

	.long	0x02000214
	.long	longjmp_bp - VLib_Origin

	.long	0x020000E0
	.long	strcoll_bp - VLib_Origin

	.long	0x020000C3
	.long	strftime_bp - VLib_Origin

	.long	0x02000082
	.long	strtok_bp - VLib_Origin

	.long	0x02000422
	.long	close_bp - VLib_Origin

	.long	0x02000396
	.long	feof_bp - VLib_Origin

	.long	0x0200034F
	.long	feupdateenv_bp - VLib_Origin

	.long	0x020002A4
	.long	isalnum_bp - VLib_Origin

	.long	0x020001A5
	.long	open_bp - VLib_Origin

	.long	0x0200007C
	.long	strtol_bp - VLib_Origin

	.long	0x0200006E
	.long	strxfrm_bp - VLib_Origin

	.long	0x02000486
	.long	__signbitf_bp - VLib_Origin

	.long	0x02000481
	.long	abort_bp - VLib_Origin

	.long	0x020000F9
	.long	sscanf_bp - VLib_Origin

	.long	0x02000094
	.long	strspn_bp - VLib_Origin

	.long	0x0200026C
	.long	ispunct_bp - VLib_Origin

	.long	0x02000143
	.long	rinttol_bp - VLib_Origin

	.long	0x0200005D
	.long	time_bp - VLib_Origin

	.long	0x02000490
	.long	__signbitd_bp - VLib_Origin

	.long	0x0200040E
	.long	ctime_bp - VLib_Origin

	.long	0x020003D6
	.long	fdim_bp - VLib_Origin

	.long	0x02000389
	.long	feraiseexcept_bp - VLib_Origin

	.long	0x0200032F
	.long	fmax_bp - VLib_Origin

	.long	0x02000327
	.long	fmod_bp - VLib_Origin

	.long	0x020002C9
	.long	getenv_bp - VLib_Origin

	.long	0x020002B5
	.long	gmtime_bp - VLib_Origin

	.long	0x020000AF
	.long	strncmp_bp - VLib_Origin

	.long	0x02000333
	.long	floor_bp - VLib_Origin

	.long	0x02000301
	.long	freopen_bp - VLib_Origin

	.long	0x020000FF
	.long	srand_bp - VLib_Origin

	.long	0x02000022
	.long	vfprintf_bp - VLib_Origin

	.long	0x02000273
	.long	isprint_bp - VLib_Origin

	.long	0x0200021F
	.long	log2_bp - VLib_Origin

	.long	0x020001A9
	.long	nextafterf_bp - VLib_Origin

	.long	0x0200019F
	.long	perror_bp - VLib_Origin

	.long	0x02000166
	.long	remainder_bp - VLib_Origin

	.long	0x02000050
	.long	tmpnam_bp - VLib_Origin

	.long	0x0200033D
	.long	fgetpos_bp - VLib_Origin

	.long	0x02000288
	.long	isdigit_bp - VLib_Origin

	.long	0x02000209
	.long	mblen_bp - VLib_Origin

	.long	0x0200044D
	.long	atof_bp - VLib_Origin

	.long	0x020003E5
	.long	fabs_bp - VLib_Origin

	.long	0x0200024D
	.long	ldexp_bp - VLib_Origin

	.long	0x0200020E
	.long	malloc_bp - VLib_Origin

	.long	0x020001EF
	.long	memcmp_bp - VLib_Origin

	.long	0x020001B3
	.long	nextafterd_bp - VLib_Origin

	.long	0x020000B6
	.long	strncat_bp - VLib_Origin

	.long	0x0200039A
	.long	feholdexcept_bp - VLib_Origin

	.long	0x020002DA
	.long	gamma_bp - VLib_Origin

	.long	0x02000030
	.long	ungetc_bp - VLib_Origin

	.long	0x02000338
	.long	fgets_bp - VLib_Origin

	.long	0x02000228
	.long	log10_bp - VLib_Origin

	.long	0x020002D6
	.long	getc_bp - VLib_Origin

	.long	0x020001E2
	.long	memmove_bp - VLib_Origin

	.long	0x020001CD
	.long	modff_bp - VLib_Origin

	.long	0x020000F3
	.long	strcat_bp - VLib_Origin

	.long	0x02000036
	.long	trunc_bp - VLib_Origin

	.long	0x0200042C
	.long	clearerr_bp - VLib_Origin

	.long	0x020003A6
	.long	fegetround_bp - VLib_Origin

	.long	0x02000223
	.long	log1p_bp - VLib_Origin

	.long	0x020001BD
	.long	nearbyint_bp - VLib_Origin

	.long	0x0200018B
	.long	putchar_bp - VLib_Origin

	.long	0x02000176
	.long	read_bp - VLib_Origin

	.long	0x020000CB
	.long	strerror_bp - VLib_Origin

	.long	0x02000065
	.long	tan_bp - VLib_Origin

	.long	0x020004DB
	.long	__fpclassifyf_bp - VLib_Origin

	.long	0x0200047A
	.long	acos_bp - VLib_Origin

	.long	0x020003D0
	.long	fdopen_bp - VLib_Origin

	.long	0x0200035A
	.long	fetestexcept_bp - VLib_Origin

	.long	0x020002F6
	.long	fscanf_bp - VLib_Origin

	.long	0x02000243
	.long	lgamma_bp - VLib_Origin

	.long	0x02000201
	.long	mbstowcs_bp - VLib_Origin

	.long	0x02000116
	.long	setvbuf_bp - VLib_Origin

	.long	0x020000DA
	.long	strcpy_bp - VLib_Origin

	.long	0x020000A1
	.long	strpbrk_bp - VLib_Origin

	.long	0x02000445
	.long	atol_bp - VLib_Origin

	.long	0x02000406
	.long	difftime_bp - VLib_Origin

	.long	0x020003C3
	.long	feclearexcept_bp - VLib_Origin

	.long	0x0200028F
	.long	iscntrl_bp - VLib_Origin

	.long	0x020001FB
	.long	mbtowc_bp - VLib_Origin

	.long	0x02000187
	.long	puts_bp - VLib_Origin

	.long	0x0200017A
	.long	rand_bp - VLib_Origin

	.long	0x020000BD
	.long	strlen_bp - VLib_Origin

	.long	0x0200008E
	.long	strstr_bp - VLib_Origin

	.long	0x020004E8
	.long	__fpclassifyd_bp - VLib_Origin

	.long	0x0200047E
	.long	abs_bp - VLib_Origin

	.long	0x02000316
	.long	fputc_bp - VLib_Origin

	.long	0x02000265
	.long	isspace_bp - VLib_Origin

	.long	0x02000104
	.long	sqrt_bp - VLib_Origin

	.long	0x02000013
	.long	vsprintf_bp - VLib_Origin

	.long	0x02000475
	.long	acosh_bp - VLib_Origin

	.long	0x02000413
	.long	cosh_bp - VLib_Origin

	.long	0x02000296
	.long	isascii_bp - VLib_Origin

	.long	0x0200027A
	.long	islower_bp - VLib_Origin

	.long	0x020001F5
	.long	memchr_bp - VLib_Origin

	.long	0x02000113
	.long	sin_bp - VLib_Origin

	.long	0x0200000B
	.long	wcstombs_bp - VLib_Origin

	.long	0x020004B8
	.long	__isnand_bp - VLib_Origin

	.long	0x02000465
	.long	asinh_bp - VLib_Origin

	.long	0x0200045C
	.long	atan2_bp - VLib_Origin

	.long	0x02000449
	.long	atoi_bp - VLib_Origin

	.long	0x020003F2
	.long	exp_bp - VLib_Origin

	.long	0x020001CA
	.long	nan_bp - VLib_Origin

	.long	0x020001C6
	.long	nanf_bp - VLib_Origin

	.long	0x0200010F
	.long	sinh_bp - VLib_Origin

	.long	0x020004A5
	.long	__isnormald_bp - VLib_Origin

	.long	0x02000438
	.long	calloc_bp - VLib_Origin

	.long	0x02000403
	.long	div_bp - VLib_Origin

	.long	0x020003F9
	.long	erfc_bp - VLib_Origin

	.long	0x020003F5
	.long	exit_bp - VLib_Origin

	.long	0x020003BB
	.long	fegetenv_bp - VLib_Origin

	.long	0x02000183
	.long	putw_bp - VLib_Origin

	.long	0x0200013E
	.long	round_bp - VLib_Origin

	.long	0x020000A8
	.long	strncpy_bp - VLib_Origin

	.long	0x02000049
	.long	toascii_bp - VLib_Origin

	.long	0x02000042
	.long	tolower_bp - VLib_Origin

	.long	0x020004B0
	.long	__isnanf_bp - VLib_Origin

	.long	0x0200046E
	.long	asctime_bp - VLib_Origin

	.long	0x020003FD
	.long	erf_bp - VLib_Origin

	.long	0x020003E9
	.long	expm1_bp - VLib_Origin

	.long	0x02000349
	.long	fflush_bp - VLib_Origin

	.long	0x02000131
	.long	scalb_bp - VLib_Origin

	.long	0x0200012C
	.long	scanf_bp - VLib_Origin

	.long	0x0200049A
	.long	__isnormalf_bp - VLib_Origin

	.long	0x02000322
	.long	fopen_bp - VLib_Origin

	.long	0x0200016F
	.long	realloc_bp - VLib_Origin

	.long	0x02000160
	.long	remove_bp - VLib_Origin

	.long	0x0200011D
	.long	setlocale_bp - VLib_Origin


	.globl	cfm_stub_binding_helper

	.section	__DATA, __VLib_Func_BPs, regular, no_dead_strip

	.align	2

write_bp:
	.long	_write

wctomb_bp:
	.long	_wctomb

wcstombs_bp:
	.long	_wcstombs

vsprintf_bp:
	.long	_vsprintf

vprintf_bp:
	.long	_vprintf

vfprintf_bp:
	.long	_vfprintf

unlink_bp:
	.long	_unlink

ungetc_bp:
	.long	_ungetc

trunc_bp:
	.long	_trunc

toupper_bp:
	.long	_toupper

tolower_bp:
	.long	_tolower

toascii_bp:
	.long	_toascii

tmpnam_bp:
	.long	_tmpnam

tmpfile_bp:
	.long	_tmpfile

time_bp:
	.long	_time

tanh_bp:
	.long	_tanh

tan_bp:
	.long	_tan

system_bp:
	.long	_system

strxfrm_bp:
	.long	_strxfrm

strtoul_bp:
	.long	_strtoul

strtol_bp:
	.long	_strtol

strtok_bp:
	.long	_strtok

strtod_bp:
	.long	_strtod

strstr_bp:
	.long	_strstr

strspn_bp:
	.long	_strspn

strrchr_bp:
	.long	_strrchr

strpbrk_bp:
	.long	_strpbrk

strncpy_bp:
	.long	_strncpy

strncmp_bp:
	.long	_strncmp

strncat_bp:
	.long	_strncat

strlen_bp:
	.long	_strlen

strftime_bp:
	.long	_strftime

strerror_bp:
	.long	_strerror

strcspn_bp:
	.long	_strcspn

strcpy_bp:
	.long	_strcpy

strcoll_bp:
	.long	_strcoll

strcmp_bp:
	.long	_strcmp

strchr_bp:
	.long	_strchr

strcat_bp:
	.long	_strcat

sscanf_bp:
	.long	_sscanf

srand_bp:
	.long	_srand

sqrt_bp:
	.long	_sqrt

sprintf_bp:
	.long	_sprintf

sinh_bp:
	.long	_sinh

sin_bp:
	.long	_sin

setvbuf_bp:
	.long	_setvbuf

setlocale_bp:
	.long	_setlocale

setbuf_bp:
	.long	_setbuf

scanf_bp:
	.long	_scanf

scalb_bp:
	.long	_scalb

roundtol_bp:
	.long	_roundtol

round_bp:
	.long	_round

rinttol_bp:
	.long	_rinttol

rint_bp:
	.long	_rint

rewind_bp:
	.long	_rewind

rename_bp:
	.long	_rename

remquo_bp:
	.long	_remquo

remove_bp:
	.long	_remove

remainder_bp:
	.long	_remainder

realloc_bp:
	.long	_realloc

read_bp:
	.long	_read

rand_bp:
	.long	_rand

qsort_bp:
	.long	_qsort

putw_bp:
	.long	_putw

puts_bp:
	.long	_puts

putchar_bp:
	.long	_putchar

putc_bp:
	.long	_putc

printf_bp:
	.long	_printf

pow_bp:
	.long	_pow

perror_bp:
	.long	_perror

open_bp:
	.long	_open

nextafterf_bp:
	.long	_nextafterf

nextafterd_bp:
	.long	_nextafterd

nearbyint_bp:
	.long	_nearbyint

nanf_bp:
	.long	_nanf

nan_bp:
	.long	_nan

modff_bp:
	.long	_modff

modf_bp:
	.long	_modf

mktime_bp:
	.long	_mktime

memset_bp:
	.long	_memset

memmove_bp:
	.long	_memmove

memcpy_bp:
	.long	_memcpy

memcmp_bp:
	.long	_memcmp

memchr_bp:
	.long	_memchr

mbtowc_bp:
	.long	_mbtowc

mbstowcs_bp:
	.long	_mbstowcs

mblen_bp:
	.long	_mblen

malloc_bp:
	.long	_malloc

longjmp_bp:
	.long	_longjmp

logb_bp:
	.long	_logb

log2_bp:
	.long	_log2

log1p_bp:
	.long	_log1p

log10_bp:
	.long	_log10

log_bp:
	.long	_log

localtime_bp:
	.long	_localtime

localeconv_bp:
	.long	_localeconv

lgamma_bp:
	.long	_lgamma

ldiv_bp:
	.long	_ldiv

ldexp_bp:
	.long	_ldexp

labs_bp:
	.long	_labs

isxdigit_bp:
	.long	_isxdigit

isupper_bp:
	.long	_isupper

isspace_bp:
	.long	_isspace

ispunct_bp:
	.long	_ispunct

isprint_bp:
	.long	_isprint

islower_bp:
	.long	_islower

isgraph_bp:
	.long	_isgraph

isdigit_bp:
	.long	_isdigit

iscntrl_bp:
	.long	_iscntrl

isascii_bp:
	.long	_isascii

isalpha_bp:
	.long	_isalpha

isalnum_bp:
	.long	_isalnum

ioctl_bp:
	.long	_ioctl

hypot_bp:
	.long	_hypot

gmtime_bp:
	.long	_gmtime

getw_bp:
	.long	_getw

gets_bp:
	.long	_gets

getpid_bp:
	.long	_getpid

getenv_bp:
	.long	_getenv

getchar_bp:
	.long	_getchar

getc_bp:
	.long	_getc

gamma_bp:
	.long	_gamma

fwrite_bp:
	.long	_fwrite

ftell_bp:
	.long	_ftell

fsetpos_bp:
	.long	_fsetpos

fseek_bp:
	.long	_fseek

fscanf_bp:
	.long	_fscanf

frexp_bp:
	.long	_frexp

freopen_bp:
	.long	_freopen

free_bp:
	.long	_free

fread_bp:
	.long	_fread

fputs_bp:
	.long	_fputs

fputc_bp:
	.long	_fputc

fprintf_bp:
	.long	_fprintf

fopen_bp:
	.long	_fopen

fmod_bp:
	.long	_fmod

fmin_bp:
	.long	_fmin

fmax_bp:
	.long	_fmax

floor_bp:
	.long	_floor

fgets_bp:
	.long	_fgets

fgetpos_bp:
	.long	_fgetpos

fgetc_bp:
	.long	_fgetc

fflush_bp:
	.long	_fflush

feupdateenv_bp:
	.long	_feupdateenv

fetestexcept_bp:
	.long	_fetestexcept

fesetround_bp:
	.long	_fesetround

fesetexcept_bp:
	.long	_fesetexcept

fesetenv_bp:
	.long	_fesetenv

ferror_bp:
	.long	_ferror

feraiseexcept_bp:
	.long	_feraiseexcept

feof_bp:
	.long	_feof

feholdexcept_bp:
	.long	_feholdexcept

fegetround_bp:
	.long	_fegetround

fegetexcept_bp:
	.long	_fegetexcept

fegetenv_bp:
	.long	_fegetenv

feclearexcept_bp:
	.long	_feclearexcept

fdopen_bp:
	.long	_fdopen

fdim_bp:
	.long	_fdim

fcntl_bp:
	.long	_fcntl

fclose_bp:
	.long	_fclose

fabs_bp:
	.long	_fabs

expm1_bp:
	.long	_expm1

exp2_bp:
	.long	_exp2

exp_bp:
	.long	_exp

exit_bp:
	.long	_exit

erfc_bp:
	.long	_erfc

erf_bp:
	.long	_erf

dup_bp:
	.long	_dup

div_bp:
	.long	_div

difftime_bp:
	.long	_difftime

ctime_bp:
	.long	_ctime

cosh_bp:
	.long	_cosh

cos_bp:
	.long	_cos

copysign_bp:
	.long	_copysign

close_bp:
	.long	_close

clock_bp:
	.long	_clock

clearerr_bp:
	.long	_clearerr

ceil_bp:
	.long	_ceil

calloc_bp:
	.long	_calloc

bsearch_bp:
	.long	_bsearch

atol_bp:
	.long	_atol

atoi_bp:
	.long	_atoi

atof_bp:
	.long	_atof

atexit_bp:
	.long	_atexit

atanh_bp:
	.long	_atanh

atan2_bp:
	.long	_atan2

atan_bp:
	.long	_atan

asinh_bp:
	.long	_asinh

asin_bp:
	.long	_asin

asctime_bp:
	.long	_asctime

acosh_bp:
	.long	_acosh

acos_bp:
	.long	_acos

abs_bp:
	.long	_abs

abort_bp:
	.long	_abort

__signbitf_bp:
	.long	___signbitf

__signbitd_bp:
	.long	___signbitd

__isnormalf_bp:
	.long	___isnormalf

__isnormald_bp:
	.long	___isnormald

__isnanf_bp:
	.long	___isnanf

__isnand_bp:
	.long	___isnand

__isfinitef_bp:
	.long	___isfinitef

__isfinited_bp:
	.long	___isfinited

__inf_bp:
	.long	___inf

__fpclassifyf_bp:
	.long	___fpclassifyf

__fpclassifyd_bp:
	.long	___fpclassifyd

	.section	__DATA, __VLib_Data_BPs, regular, no_dead_strip

	.align	2

#else
#endif

