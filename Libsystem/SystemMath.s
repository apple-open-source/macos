#if defined (__ppc__) || defined(ppc)

	.section	__TEXT, __VLib_Container, regular

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
	.long	167
	.long	0
	.long	11
	.long	12
	.long	50
	.long	0x70777063
	.long	0x00000000
	.long	0x00000001
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000

VLib_Strings:

	.ascii	"System.vlib"
	.byte	0
	.ascii	"/System/Library/Frameworks/System.framework/System"
	.byte	0

	.align	2

VLib_HashTable:

	.long	0x000C0000
	.long	0x00140003
	.long	0x001C0008
	.long	0x001C000F
	.long	0x00180016
	.long	0x0018001C
	.long	0x00200022
	.long	0x001C002A
	.long	0x00140031
	.long	0x00180036
	.long	0x0010003C
	.long	0x00180040
	.long	0x00180046
	.long	0x000C004C
	.long	0x0008004F
	.long	0x00140051
	.long	0x00100056
	.long	0x0010005A
	.long	0x000C005E
	.long	0x00180061
	.long	0x00080067
	.long	0x00080069
	.long	0x000C006B
	.long	0x0018006E
	.long	0x00200074
	.long	0x0020007C
	.long	0x00140084
	.long	0x001C0089
	.long	0x00140090
	.long	0x00200095
	.long	0x0018009D
	.long	0x001000A3

VLib_HashKeys:

	.long	0x000717DE
	.long	0x00071021
	.long	0x00060A94
	.long	0x00030109
	.long	0x00040253
	.long	0x00060AB4
	.long	0x0006098D
	.long	0x00060909
	.long	0x00030169
	.long	0x0007179E
	.long	0x0003010A
	.long	0x00060A33
	.long	0x00071548
	.long	0x00060B9E
	.long	0x00040233
	.long	0x00040232
	.long	0x000504E4
	.long	0x00050422
	.long	0x00040270
	.long	0x00050441
	.long	0x0007173A
	.long	0x00094B1B
	.long	0x00050480
	.long	0x00040290
	.long	0x00071188
	.long	0x000504C2
	.long	0x00060B5E
	.long	0x00060BDA
	.long	0x00060866
	.long	0x000609EA
	.long	0x00050447
	.long	0x00040257
	.long	0x00071589
	.long	0x000717FA
	.long	0x0008214C
	.long	0x000A971E
	.long	0x000609AB
	.long	0x00050444
	.long	0x00040254
	.long	0x0006090E
	.long	0x0007137D
	.long	0x00040237
	.long	0x00040274
	.long	0x00060864
	.long	0x00060A74
	.long	0x000505C9
	.long	0x00060B1F
	.long	0x000504E0
	.long	0x000504E0
	.long	0x00030121
	.long	0x0004029C
	.long	0x000716DE
	.long	0x000712DE
	.long	0x000712DE
	.long	0x0004027A
	.long	0x000402DF
	.long	0x00071028
	.long	0x00060A5B
	.long	0x000505A4
	.long	0x00082372
	.long	0x000608AF
	.long	0x000710CC
	.long	0x00060B96
	.long	0x000504AF
	.long	0x00040278
	.long	0x000504CD
	.long	0x00082E1B
	.long	0x00071770
	.long	0x0007123A
	.long	0x00060BD5
	.long	0x0004021C
	.long	0x0004021C
	.long	0x0007106F
	.long	0x000717B1
	.long	0x00060BD2
	.long	0x0005046F
	.long	0x00060B76
	.long	0x00060BF2
	.long	0x0005042C
	.long	0x00071182
	.long	0x000402BB
	.long	0x0004023E
	.long	0x0006082E
	.long	0x0005048B
	.long	0x0006088B
	.long	0x00071736
	.long	0x00050410
	.long	0x0007138C
	.long	0x0005059C
	.long	0x000829DE
	.long	0x00095328
	.long	0x00060AA4
	.long	0x00060AC7
	.long	0x000711BC
	.long	0x000504D4
	.long	0x00071012
	.long	0x000711FD
	.long	0x00040203
	.long	0x000504F4
	.long	0x000609BE
	.long	0x00040260
	.long	0x0007172A
	.long	0x000609DD
	.long	0x00050477
	.long	0x00060A67
	.long	0x00050476
	.long	0x00050457
	.long	0x0007136D
	.long	0x00060B8A
	.long	0x00040227
	.long	0x00050436
	.long	0x0003017C
	.long	0x00082E64
	.long	0x000402A2
	.long	0x000822A2
	.long	0x000717E8
	.long	0x00040229
	.long	0x000609F7
	.long	0x00071594
	.long	0x00060BA5
	.long	0x00082531
	.long	0x000609D6
	.long	0x000717E7
	.long	0x0006089C
	.long	0x000402CF
	.long	0x00060BE6
	.long	0x00060BA4
	.long	0x0008205B
	.long	0x0004026A
	.long	0x000402AC
	.long	0x00060911
	.long	0x00071038
	.long	0x000402CC
	.long	0x00082C9E
	.long	0x000504BF
	.long	0x000711B7
	.long	0x00030133
	.long	0x0005043A
	.long	0x0004022A
	.long	0x00030170
	.long	0x000609B6
	.long	0x0007109F
	.long	0x0008281B
	.long	0x000710FC
	.long	0x00050498
	.long	0x000504DA
	.long	0x00030114
	.long	0x00040288
	.long	0x0004026F
	.long	0x0004024F
	.long	0x000402CB
	.long	0x0007145F
	.long	0x00071705
	.long	0x0004026E
	.long	0x0007143C
	.long	0x00030134
	.long	0x0006081D
	.long	0x00030116
	.long	0x000504BB
	.long	0x00050516
	.long	0x00050516
	.long	0x000712AB
	.long	0x0006089A
	.long	0x000955D1
	.long	0x0007145D
	.long	0x00060A4D
	.long	0x0005047C

VLib_ExportNames:

	.long	0x67657473, 0x69736C6F, 0x77657269, 0x73786469
	.long	0x67697463, 0x6C6F7365, 0x66646F70, 0x656E7365
	.long	0x74627566, 0x63616C6C, 0x6F636670, 0x75747373
	.long	0x7472746F, 0x6C61626F, 0x72746765, 0x74777763
	.long	0x73746F6D, 0x62736667, 0x6574706F, 0x73697370
	.long	0x72696E74, 0x6D616C6C, 0x6F637374, 0x72746F6B
	.long	0x76667072, 0x696E7466, 0x66666C75, 0x73686973
	.long	0x636E7472, 0x6C737472, 0x636D7073, 0x74727062
	.long	0x726B6162, 0x736C6F63, 0x616C7469, 0x6D657075
	.long	0x74636861, 0x72737472, 0x6E636174, 0x6D62746F
	.long	0x77637374, 0x72746F64, 0x6674656C, 0x6C696F63
	.long	0x746C7072, 0x696E7466, 0x73797374, 0x656D6C6F
	.long	0x6E676A6D, 0x7071736F, 0x72747265, 0x6E616D65
	.long	0x66736361, 0x6E666973, 0x73706163, 0x6574696D
	.long	0x65646976, 0x66736574, 0x706F7373, 0x74726368
	.long	0x72676574, 0x63737472, 0x7866726D, 0x6973616C
	.long	0x6E756D66, 0x6F70656E, 0x66707574, 0x63667772
	.long	0x69746573, 0x7472636F, 0x6C6C746D, 0x706E616D
	.long	0x746F6C6F, 0x77657276, 0x73707269, 0x6E74666D
	.long	0x656D6370, 0x79617465, 0x78697473, 0x74726E63
	.long	0x6D706174, 0x6F66676D, 0x74696D65, 0x636C6561
	.long	0x72657272, 0x67657463, 0x68617272, 0x65616465
	.long	0x78697461, 0x746F6972, 0x616E6473, 0x74726373
	.long	0x706E6D62, 0x73746F77, 0x63737374, 0x72746F75
	.long	0x6C72656D, 0x6F766573, 0x74726361, 0x7461746F
	.long	0x6C737472, 0x6674696D, 0x65617363, 0x74696D65
	.long	0x6973616C, 0x7068616D, 0x656D6D6F, 0x7665746D
	.long	0x7066696C, 0x65667265, 0x65667365, 0x656B7265
	.long	0x77696E64, 0x77726974, 0x65697361, 0x73636969
	.long	0x6C646976, 0x73747265, 0x72726F72, 0x73747273
	.long	0x706E746F, 0x75707065, 0x72776374, 0x6F6D6273
	.long	0x7363616E, 0x66756E6C, 0x696E6B64, 0x69666674
	.long	0x696D656C, 0x61627363, 0x74696D65, 0x7374726E
	.long	0x6370796D, 0x656D636D, 0x70666765, 0x74636273
	.long	0x65617263, 0x6866636E, 0x746C6670, 0x72696E74
	.long	0x6666656F, 0x666F7065, 0x6E707574, 0x63697364
	.long	0x69676974, 0x66726561, 0x64636C6F, 0x636B6D65
	.long	0x6D636872, 0x69736772, 0x61706864, 0x75706765
	.long	0x74656E76, 0x69737570, 0x70657270, 0x6572726F
	.long	0x72746F61, 0x73636969, 0x756E6765, 0x74636663
	.long	0x6C6F7365, 0x6D656D73, 0x65746D6B, 0x74696D65
	.long	0x7265616C, 0x6C6F6373, 0x74726370, 0x79737261
	.long	0x6E647374, 0x726C656E, 0x73747273, 0x74726765
	.long	0x74706964, 0x6D626C65, 0x6E666765, 0x74737365
	.long	0x746C6F63, 0x616C6566, 0x6572726F, 0x72767072
	.long	0x696E7466, 0x69737075, 0x6E637470, 0x75747366
	.long	0x72656F70, 0x656E6C6F, 0x63616C65, 0x636F6E76
	.long	0x7363616E, 0x66736574, 0x76627566, 0x73747272
	.long	0x63687270, 0x75747773, 0x7072696E, 0x74667461
	.long	0x6E687461, 0x6E737172, 0x7473696E, 0x6873696E
	.long	0x7363616C, 0x6272696E, 0x7472656D, 0x61696E64
	.long	0x6572706F, 0x776D6F64, 0x666C6F67, 0x626C6F67
	.long	0x31706C6F, 0x6731306C, 0x6F676C67, 0x616D6D61
	.long	0x6C646578, 0x70687970, 0x6F746761, 0x6D6D6166
	.long	0x72657870, 0x666D6F64, 0x666C6F6F, 0x72666162
	.long	0x73657870, 0x6D316578, 0x70657266, 0x63657266
	.long	0x636F7368, 0x636F7363, 0x6F707973, 0x69676E63
	.long	0x65696C61, 0x74616E68, 0x6174616E, 0x32617461
	.long	0x6E617369, 0x6E686173, 0x696E6163, 0x6F736861
	.long	0x636F7300

	.section	__TEXT, __VLib_Exports, symbol_stubs, none, 8

	.align	2

VLib_ExportSymbols:

	.indirect_symbol	_strrchr
	.long	0x020002FC
	.long	strrchr_bp - VLib_Origin

	.indirect_symbol	_isalpha
	.long	0x020001A0
	.long	isalpha_bp - VLib_Origin

	.indirect_symbol	_setbuf
	.long	0x0200001E
	.long	setbuf_bp - VLib_Origin

	.indirect_symbol	_log
	.long	0x02000347
	.long	log_bp - VLib_Origin

	.indirect_symbol	_labs
	.long	0x02000203
	.long	labs_bp - VLib_Origin

	.indirect_symbol	_rewind
	.long	0x020001BE
	.long	rewind_bp - VLib_Origin

	.indirect_symbol	_memcpy
	.long	0x0200012F
	.long	memcpy_bp - VLib_Origin

	.indirect_symbol	_fwrite
	.long	0x0200010D
	.long	fwrite_bp - VLib_Origin

	.indirect_symbol	_pow
	.long	0x02000332
	.long	pow_bp - VLib_Origin

	.indirect_symbol	_sprintf
	.long	0x02000307
	.long	sprintf_bp - VLib_Origin

	.indirect_symbol	_dup
	.long	0x0200025B
	.long	dup_bp - VLib_Origin

	.indirect_symbol	_unlink
	.long	0x020001F5
	.long	unlink_bp - VLib_Origin

	.indirect_symbol	_toupper
	.long	0x020001E2
	.long	toupper_bp - VLib_Origin

	.indirect_symbol	_strchr
	.long	0x020000EB
	.long	strchr_bp - VLib_Origin

	.indirect_symbol	_getw
	.long	0x0200003A
	.long	getw_bp - VLib_Origin

	.indirect_symbol	_ceil
	.long	0x0200038F
	.long	ceil_bp - VLib_Origin

	.indirect_symbol	_frexp
	.long	0x0200035F
	.long	frexp_bp - VLib_Origin

	.indirect_symbol	_hypot
	.long	0x02000355
	.long	hypot_bp - VLib_Origin

	.indirect_symbol	_logb
	.long	0x02000339
	.long	logb_bp - VLib_Origin

	.indirect_symbol	_clock
	.long	0x02000249
	.long	clock_bp - VLib_Origin

	.indirect_symbol	_strcspn
	.long	0x0200016B
	.long	strcspn_bp - VLib_Origin

	.indirect_symbol	_localtime
	.long	0x02000085
	.long	localtime_bp - VLib_Origin

	.indirect_symbol	_atanh
	.long	0x02000393
	.long	atanh_bp - VLib_Origin

	.indirect_symbol	_tanh
	.long	0x0200030E
	.long	tanh_bp - VLib_Origin

	.indirect_symbol	_isupper
	.long	0x02000264
	.long	isupper_bp - VLib_Origin

	.indirect_symbol	_fread
	.long	0x02000244
	.long	fread_bp - VLib_Origin

	.indirect_symbol	_printf
	.long	0x020000B2
	.long	printf_bp - VLib_Origin

	.indirect_symbol	_strtod
	.long	0x020000A2
	.long	strtod_bp - VLib_Origin

	.indirect_symbol	_getpid
	.long	0x020002AE
	.long	getpid_bp - VLib_Origin

	.indirect_symbol	_memset
	.long	0x02000284
	.long	memset_bp - VLib_Origin

	.indirect_symbol	_fgetc
	.long	0x02000219
	.long	fgetc_bp - VLib_Origin

	.indirect_symbol	_free
	.long	0x020001B5
	.long	free_bp - VLib_Origin

	.indirect_symbol	_tmpfile
	.long	0x020001AE
	.long	tmpfile_bp - VLib_Origin

	.indirect_symbol	_strtoul
	.long	0x0200017A
	.long	strtoul_bp - VLib_Origin

	.indirect_symbol	_copysign
	.long	0x02000387
	.long	copysign_bp - VLib_Origin

	.indirect_symbol	_localeconv
	.long	0x020002E6
	.long	localeconv_bp - VLib_Origin

	.indirect_symbol	_mktime
	.long	0x0200028A
	.long	mktime_bp - VLib_Origin

	.indirect_symbol	_fcntl
	.long	0x02000225
	.long	fcntl_bp - VLib_Origin

	.indirect_symbol	_ldiv
	.long	0x020001D0
	.long	ldiv_bp - VLib_Origin

	.indirect_symbol	_atexit
	.long	0x02000135
	.long	atexit_bp - VLib_Origin

	.indirect_symbol	_fsetpos
	.long	0x020000E4
	.long	fsetpos_bp - VLib_Origin

	.indirect_symbol	_gets
	.long	0x02000000
	.long	gets_bp - VLib_Origin

	.indirect_symbol	_atan
	.long	0x0200039D
	.long	atan_bp - VLib_Origin

	.indirect_symbol	_ferror
	.long	0x020002C7
	.long	ferror_bp - VLib_Origin

	.indirect_symbol	_wctomb
	.long	0x020001E9
	.long	wctomb_bp - VLib_Origin

	.indirect_symbol	_write
	.long	0x020001C4
	.long	write_bp - VLib_Origin

	.indirect_symbol	_system
	.long	0x020000B8
	.long	system_bp - VLib_Origin

	.indirect_symbol	_ioctl
	.long	0x020000AD
	.long	ioctl_bp - VLib_Origin

	.indirect_symbol	_ftell
	.long	0x020000A8
	.long	ftell_bp - VLib_Origin

	.indirect_symbol	_cos
	.long	0x02000384
	.long	cos_bp - VLib_Origin

	.indirect_symbol	_rint
	.long	0x02000325
	.long	rint_bp - VLib_Origin

	.indirect_symbol	_vprintf
	.long	0x020002CD
	.long	vprintf_bp - VLib_Origin

	.indirect_symbol	_fprintf
	.long	0x0200022A
	.long	fprintf_bp - VLib_Origin

	.indirect_symbol	_bsearch
	.long	0x0200021E
	.long	bsearch_bp - VLib_Origin

	.indirect_symbol	_modf
	.long	0x02000335
	.long	modf_bp - VLib_Origin

	.indirect_symbol	_putc
	.long	0x02000239
	.long	putc_bp - VLib_Origin

	.indirect_symbol	_getchar
	.long	0x02000154
	.long	getchar_bp - VLib_Origin

	.indirect_symbol	_rename
	.long	0x020000CA
	.long	rename_bp - VLib_Origin

	.indirect_symbol	_qsort
	.long	0x020000C5
	.long	qsort_bp - VLib_Origin

	.indirect_symbol	_isxdigit
	.long	0x0200000B
	.long	isxdigit_bp - VLib_Origin

	.indirect_symbol	_fclose
	.long	0x0200027E
	.long	fclose_bp - VLib_Origin

	.indirect_symbol	_isgraph
	.long	0x02000254
	.long	isgraph_bp - VLib_Origin

	.indirect_symbol	_strcmp
	.long	0x02000075
	.long	strcmp_bp - VLib_Origin

	.indirect_symbol	_fputs
	.long	0x0200002A
	.long	fputs_bp - VLib_Origin

	.indirect_symbol	_asin
	.long	0x020003A6
	.long	asin_bp - VLib_Origin

	.indirect_symbol	_fseek
	.long	0x020001B9
	.long	fseek_bp - VLib_Origin

	.indirect_symbol	_strftime
	.long	0x02000191
	.long	strftime_bp - VLib_Origin

	.indirect_symbol	_strcoll
	.long	0x02000113
	.long	strcoll_bp - VLib_Origin

	.indirect_symbol	_longjmp
	.long	0x020000BE
	.long	longjmp_bp - VLib_Origin

	.indirect_symbol	_strtok
	.long	0x0200005A
	.long	strtok_bp - VLib_Origin

	.indirect_symbol	_open
	.long	0x02000235
	.long	open_bp - VLib_Origin

	.indirect_symbol	_feof
	.long	0x02000231
	.long	feof_bp - VLib_Origin

	.indirect_symbol	_isalnum
	.long	0x020000FC
	.long	isalnum_bp - VLib_Origin

	.indirect_symbol	_strxfrm
	.long	0x020000F5
	.long	strxfrm_bp - VLib_Origin

	.indirect_symbol	_strtol
	.long	0x0200002F
	.long	strtol_bp - VLib_Origin

	.indirect_symbol	_close
	.long	0x02000013
	.long	close_bp - VLib_Origin

	.indirect_symbol	_sscanf
	.long	0x020001EF
	.long	sscanf_bp - VLib_Origin

	.indirect_symbol	_strspn
	.long	0x020001DC
	.long	strspn_bp - VLib_Origin

	.indirect_symbol	_abort
	.long	0x02000035
	.long	abort_bp - VLib_Origin

	.indirect_symbol	_ispunct
	.long	0x020002D4
	.long	ispunct_bp - VLib_Origin

	.indirect_symbol	_time
	.long	0x020000DD
	.long	time_bp - VLib_Origin

	.indirect_symbol	_fmod
	.long	0x02000364
	.long	fmod_bp - VLib_Origin

	.indirect_symbol	_getenv
	.long	0x0200025E
	.long	getenv_bp - VLib_Origin

	.indirect_symbol	_ctime
	.long	0x02000207
	.long	ctime_bp - VLib_Origin

	.indirect_symbol	_gmtime
	.long	0x02000146
	.long	gmtime_bp - VLib_Origin

	.indirect_symbol	_strncmp
	.long	0x0200013B
	.long	strncmp_bp - VLib_Origin

	.indirect_symbol	_floor
	.long	0x02000368
	.long	floor_bp - VLib_Origin

	.indirect_symbol	_freopen
	.long	0x020002DF
	.long	freopen_bp - VLib_Origin

	.indirect_symbol	_srand
	.long	0x0200029D
	.long	srand_bp - VLib_Origin

	.indirect_symbol	_vfprintf
	.long	0x02000060
	.long	vfprintf_bp - VLib_Origin

	.indirect_symbol	_remainder
	.long	0x02000329
	.long	remainder_bp - VLib_Origin

	.indirect_symbol	_perror
	.long	0x0200026B
	.long	perror_bp - VLib_Origin

	.indirect_symbol	_tmpnam
	.long	0x0200011A
	.long	tmpnam_bp - VLib_Origin

	.indirect_symbol	_isprint
	.long	0x0200004D
	.long	isprint_bp - VLib_Origin

	.indirect_symbol	_mblen
	.long	0x020002B4
	.long	mblen_bp - VLib_Origin

	.indirect_symbol	_isdigit
	.long	0x0200023D
	.long	isdigit_bp - VLib_Origin

	.indirect_symbol	_fgetpos
	.long	0x02000046
	.long	fgetpos_bp - VLib_Origin

	.indirect_symbol	_fabs
	.long	0x0200036D
	.long	fabs_bp - VLib_Origin

	.indirect_symbol	_ldexp
	.long	0x02000350
	.long	ldexp_bp - VLib_Origin

	.indirect_symbol	_memcmp
	.long	0x02000213
	.long	memcmp_bp - VLib_Origin

	.indirect_symbol	_atof
	.long	0x02000142
	.long	atof_bp - VLib_Origin

	.indirect_symbol	_strncat
	.long	0x02000095
	.long	strncat_bp - VLib_Origin

	.indirect_symbol	_malloc
	.long	0x02000054
	.long	malloc_bp - VLib_Origin

	.indirect_symbol	_gamma
	.long	0x0200035A
	.long	gamma_bp - VLib_Origin

	.indirect_symbol	_ungetc
	.long	0x02000278
	.long	ungetc_bp - VLib_Origin

	.indirect_symbol	_log10
	.long	0x02000342
	.long	log10_bp - VLib_Origin

	.indirect_symbol	_fgets
	.long	0x020002B9
	.long	fgets_bp - VLib_Origin

	.indirect_symbol	_memmove
	.long	0x020001A7
	.long	memmove_bp - VLib_Origin

	.indirect_symbol	_strcat
	.long	0x02000187
	.long	strcat_bp - VLib_Origin

	.indirect_symbol	_getc
	.long	0x020000F1
	.long	getc_bp - VLib_Origin

	.indirect_symbol	_log1p
	.long	0x0200033D
	.long	log1p_bp - VLib_Origin

	.indirect_symbol	_tan
	.long	0x02000312
	.long	tan_bp - VLib_Origin

	.indirect_symbol	_strerror
	.long	0x020001D4
	.long	strerror_bp - VLib_Origin

	.indirect_symbol	_read
	.long	0x0200015B
	.long	read_bp - VLib_Origin

	.indirect_symbol	_clearerr
	.long	0x0200014C
	.long	clearerr_bp - VLib_Origin

	.indirect_symbol	_putchar
	.long	0x0200008E
	.long	putchar_bp - VLib_Origin

	.indirect_symbol	_acos
	.long	0x020003AF
	.long	acos_bp - VLib_Origin

	.indirect_symbol	_lgamma
	.long	0x0200034A
	.long	lgamma_bp - VLib_Origin

	.indirect_symbol	_setvbuf
	.long	0x020002F5
	.long	setvbuf_bp - VLib_Origin

	.indirect_symbol	_strcpy
	.long	0x02000297
	.long	strcpy_bp - VLib_Origin

	.indirect_symbol	_mbstowcs
	.long	0x02000172
	.long	mbstowcs_bp - VLib_Origin

	.indirect_symbol	_fscanf
	.long	0x020000D0
	.long	fscanf_bp - VLib_Origin

	.indirect_symbol	_strpbrk
	.long	0x0200007B
	.long	strpbrk_bp - VLib_Origin

	.indirect_symbol	_fdopen
	.long	0x02000018
	.long	fdopen_bp - VLib_Origin

	.indirect_symbol	_puts
	.long	0x020002DB
	.long	puts_bp - VLib_Origin

	.indirect_symbol	_strstr
	.long	0x020002A8
	.long	strstr_bp - VLib_Origin

	.indirect_symbol	_strlen
	.long	0x020002A2
	.long	strlen_bp - VLib_Origin

	.indirect_symbol	_difftime
	.long	0x020001FB
	.long	difftime_bp - VLib_Origin

	.indirect_symbol	_atol
	.long	0x0200018D
	.long	atol_bp - VLib_Origin

	.indirect_symbol	_rand
	.long	0x02000167
	.long	rand_bp - VLib_Origin

	.indirect_symbol	_mbtowc
	.long	0x0200009C
	.long	mbtowc_bp - VLib_Origin

	.indirect_symbol	_iscntrl
	.long	0x0200006E
	.long	iscntrl_bp - VLib_Origin

	.indirect_symbol	_sqrt
	.long	0x02000315
	.long	sqrt_bp - VLib_Origin

	.indirect_symbol	_vsprintf
	.long	0x02000127
	.long	vsprintf_bp - VLib_Origin

	.indirect_symbol	_fputc
	.long	0x02000108
	.long	fputc_bp - VLib_Origin

	.indirect_symbol	_isspace
	.long	0x020000D6
	.long	isspace_bp - VLib_Origin

	.indirect_symbol	_abs
	.long	0x02000082
	.long	abs_bp - VLib_Origin

	.indirect_symbol	_acosh
	.long	0x020003AA
	.long	acosh_bp - VLib_Origin

	.indirect_symbol	_cosh
	.long	0x02000380
	.long	cosh_bp - VLib_Origin

	.indirect_symbol	_sin
	.long	0x0200031D
	.long	sin_bp - VLib_Origin

	.indirect_symbol	_memchr
	.long	0x0200024E
	.long	memchr_bp - VLib_Origin

	.indirect_symbol	_isascii
	.long	0x020001C9
	.long	isascii_bp - VLib_Origin

	.indirect_symbol	_wcstombs
	.long	0x0200003E
	.long	wcstombs_bp - VLib_Origin

	.indirect_symbol	_islower
	.long	0x02000004
	.long	islower_bp - VLib_Origin

	.indirect_symbol	_asinh
	.long	0x020003A1
	.long	asinh_bp - VLib_Origin

	.indirect_symbol	_atan2
	.long	0x02000398
	.long	atan2_bp - VLib_Origin

	.indirect_symbol	_exp
	.long	0x02000376
	.long	exp_bp - VLib_Origin

	.indirect_symbol	_sinh
	.long	0x02000319
	.long	sinh_bp - VLib_Origin

	.indirect_symbol	_atoi
	.long	0x02000163
	.long	atoi_bp - VLib_Origin

	.indirect_symbol	_erfc
	.long	0x02000379
	.long	erfc_bp - VLib_Origin

	.indirect_symbol	_putw
	.long	0x02000303
	.long	putw_bp - VLib_Origin

	.indirect_symbol	_toascii
	.long	0x02000271
	.long	toascii_bp - VLib_Origin

	.indirect_symbol	_strncpy
	.long	0x0200020C
	.long	strncpy_bp - VLib_Origin

	.indirect_symbol	_exit
	.long	0x0200015F
	.long	exit_bp - VLib_Origin

	.indirect_symbol	_tolower
	.long	0x02000120
	.long	tolower_bp - VLib_Origin

	.indirect_symbol	_div
	.long	0x020000E1
	.long	div_bp - VLib_Origin

	.indirect_symbol	_calloc
	.long	0x02000024
	.long	calloc_bp - VLib_Origin

	.indirect_symbol	_erf
	.long	0x0200037D
	.long	erf_bp - VLib_Origin

	.indirect_symbol	_expm1
	.long	0x02000371
	.long	expm1_bp - VLib_Origin

	.indirect_symbol	_scalb
	.long	0x02000320
	.long	scalb_bp - VLib_Origin

	.indirect_symbol	_scanf
	.long	0x020002F0
	.long	scanf_bp - VLib_Origin

	.indirect_symbol	_asctime
	.long	0x02000199
	.long	asctime_bp - VLib_Origin

	.indirect_symbol	_fflush
	.long	0x02000068
	.long	fflush_bp - VLib_Origin

	.indirect_symbol	_setlocale
	.long	0x020002BE
	.long	setlocale_bp - VLib_Origin

	.indirect_symbol	_realloc
	.long	0x02000290
	.long	realloc_bp - VLib_Origin

	.indirect_symbol	_remove
	.long	0x02000181
	.long	remove_bp - VLib_Origin

	.indirect_symbol	_fopen
	.long	0x02000103
	.long	fopen_bp - VLib_Origin


	.globl	cfm_stub_binding_helper

	.section	__DATA, __VLib_Func_BPs, lazy_symbol_pointers

	.align	2

gets_bp:
	.indirect_symbol	_gets
	.long	cfm_stub_binding_helper

islower_bp:
	.indirect_symbol	_islower
	.long	cfm_stub_binding_helper

isxdigit_bp:
	.indirect_symbol	_isxdigit
	.long	cfm_stub_binding_helper

close_bp:
	.indirect_symbol	_close
	.long	cfm_stub_binding_helper

fdopen_bp:
	.indirect_symbol	_fdopen
	.long	cfm_stub_binding_helper

setbuf_bp:
	.indirect_symbol	_setbuf
	.long	cfm_stub_binding_helper

calloc_bp:
	.indirect_symbol	_calloc
	.long	cfm_stub_binding_helper

fputs_bp:
	.indirect_symbol	_fputs
	.long	cfm_stub_binding_helper

strtol_bp:
	.indirect_symbol	_strtol
	.long	cfm_stub_binding_helper

abort_bp:
	.indirect_symbol	_abort
	.long	cfm_stub_binding_helper

getw_bp:
	.indirect_symbol	_getw
	.long	cfm_stub_binding_helper

wcstombs_bp:
	.indirect_symbol	_wcstombs
	.long	cfm_stub_binding_helper

fgetpos_bp:
	.indirect_symbol	_fgetpos
	.long	cfm_stub_binding_helper

isprint_bp:
	.indirect_symbol	_isprint
	.long	cfm_stub_binding_helper

malloc_bp:
	.indirect_symbol	_malloc
	.long	cfm_stub_binding_helper

strtok_bp:
	.indirect_symbol	_strtok
	.long	cfm_stub_binding_helper

vfprintf_bp:
	.indirect_symbol	_vfprintf
	.long	cfm_stub_binding_helper

fflush_bp:
	.indirect_symbol	_fflush
	.long	cfm_stub_binding_helper

iscntrl_bp:
	.indirect_symbol	_iscntrl
	.long	cfm_stub_binding_helper

strcmp_bp:
	.indirect_symbol	_strcmp
	.long	cfm_stub_binding_helper

strpbrk_bp:
	.indirect_symbol	_strpbrk
	.long	cfm_stub_binding_helper

abs_bp:
	.indirect_symbol	_abs
	.long	cfm_stub_binding_helper

localtime_bp:
	.indirect_symbol	_localtime
	.long	cfm_stub_binding_helper

putchar_bp:
	.indirect_symbol	_putchar
	.long	cfm_stub_binding_helper

strncat_bp:
	.indirect_symbol	_strncat
	.long	cfm_stub_binding_helper

mbtowc_bp:
	.indirect_symbol	_mbtowc
	.long	cfm_stub_binding_helper

strtod_bp:
	.indirect_symbol	_strtod
	.long	cfm_stub_binding_helper

ftell_bp:
	.indirect_symbol	_ftell
	.long	cfm_stub_binding_helper

ioctl_bp:
	.indirect_symbol	_ioctl
	.long	cfm_stub_binding_helper

printf_bp:
	.indirect_symbol	_printf
	.long	cfm_stub_binding_helper

system_bp:
	.indirect_symbol	_system
	.long	cfm_stub_binding_helper

longjmp_bp:
	.indirect_symbol	_longjmp
	.long	cfm_stub_binding_helper

qsort_bp:
	.indirect_symbol	_qsort
	.long	cfm_stub_binding_helper

rename_bp:
	.indirect_symbol	_rename
	.long	cfm_stub_binding_helper

fscanf_bp:
	.indirect_symbol	_fscanf
	.long	cfm_stub_binding_helper

isspace_bp:
	.indirect_symbol	_isspace
	.long	cfm_stub_binding_helper

time_bp:
	.indirect_symbol	_time
	.long	cfm_stub_binding_helper

div_bp:
	.indirect_symbol	_div
	.long	cfm_stub_binding_helper

fsetpos_bp:
	.indirect_symbol	_fsetpos
	.long	cfm_stub_binding_helper

strchr_bp:
	.indirect_symbol	_strchr
	.long	cfm_stub_binding_helper

getc_bp:
	.indirect_symbol	_getc
	.long	cfm_stub_binding_helper

strxfrm_bp:
	.indirect_symbol	_strxfrm
	.long	cfm_stub_binding_helper

isalnum_bp:
	.indirect_symbol	_isalnum
	.long	cfm_stub_binding_helper

fopen_bp:
	.indirect_symbol	_fopen
	.long	cfm_stub_binding_helper

fputc_bp:
	.indirect_symbol	_fputc
	.long	cfm_stub_binding_helper

fwrite_bp:
	.indirect_symbol	_fwrite
	.long	cfm_stub_binding_helper

strcoll_bp:
	.indirect_symbol	_strcoll
	.long	cfm_stub_binding_helper

tmpnam_bp:
	.indirect_symbol	_tmpnam
	.long	cfm_stub_binding_helper

tolower_bp:
	.indirect_symbol	_tolower
	.long	cfm_stub_binding_helper

vsprintf_bp:
	.indirect_symbol	_vsprintf
	.long	cfm_stub_binding_helper

memcpy_bp:
	.indirect_symbol	_memcpy
	.long	cfm_stub_binding_helper

atexit_bp:
	.indirect_symbol	_atexit
	.long	cfm_stub_binding_helper

strncmp_bp:
	.indirect_symbol	_strncmp
	.long	cfm_stub_binding_helper

atof_bp:
	.indirect_symbol	_atof
	.long	cfm_stub_binding_helper

gmtime_bp:
	.indirect_symbol	_gmtime
	.long	cfm_stub_binding_helper

clearerr_bp:
	.indirect_symbol	_clearerr
	.long	cfm_stub_binding_helper

getchar_bp:
	.indirect_symbol	_getchar
	.long	cfm_stub_binding_helper

read_bp:
	.indirect_symbol	_read
	.long	cfm_stub_binding_helper

exit_bp:
	.indirect_symbol	_exit
	.long	cfm_stub_binding_helper

atoi_bp:
	.indirect_symbol	_atoi
	.long	cfm_stub_binding_helper

rand_bp:
	.indirect_symbol	_rand
	.long	cfm_stub_binding_helper

strcspn_bp:
	.indirect_symbol	_strcspn
	.long	cfm_stub_binding_helper

mbstowcs_bp:
	.indirect_symbol	_mbstowcs
	.long	cfm_stub_binding_helper

strtoul_bp:
	.indirect_symbol	_strtoul
	.long	cfm_stub_binding_helper

remove_bp:
	.indirect_symbol	_remove
	.long	cfm_stub_binding_helper

strcat_bp:
	.indirect_symbol	_strcat
	.long	cfm_stub_binding_helper

atol_bp:
	.indirect_symbol	_atol
	.long	cfm_stub_binding_helper

strftime_bp:
	.indirect_symbol	_strftime
	.long	cfm_stub_binding_helper

asctime_bp:
	.indirect_symbol	_asctime
	.long	cfm_stub_binding_helper

isalpha_bp:
	.indirect_symbol	_isalpha
	.long	cfm_stub_binding_helper

memmove_bp:
	.indirect_symbol	_memmove
	.long	cfm_stub_binding_helper

tmpfile_bp:
	.indirect_symbol	_tmpfile
	.long	cfm_stub_binding_helper

free_bp:
	.indirect_symbol	_free
	.long	cfm_stub_binding_helper

fseek_bp:
	.indirect_symbol	_fseek
	.long	cfm_stub_binding_helper

rewind_bp:
	.indirect_symbol	_rewind
	.long	cfm_stub_binding_helper

write_bp:
	.indirect_symbol	_write
	.long	cfm_stub_binding_helper

isascii_bp:
	.indirect_symbol	_isascii
	.long	cfm_stub_binding_helper

ldiv_bp:
	.indirect_symbol	_ldiv
	.long	cfm_stub_binding_helper

strerror_bp:
	.indirect_symbol	_strerror
	.long	cfm_stub_binding_helper

strspn_bp:
	.indirect_symbol	_strspn
	.long	cfm_stub_binding_helper

toupper_bp:
	.indirect_symbol	_toupper
	.long	cfm_stub_binding_helper

wctomb_bp:
	.indirect_symbol	_wctomb
	.long	cfm_stub_binding_helper

sscanf_bp:
	.indirect_symbol	_sscanf
	.long	cfm_stub_binding_helper

unlink_bp:
	.indirect_symbol	_unlink
	.long	cfm_stub_binding_helper

difftime_bp:
	.indirect_symbol	_difftime
	.long	cfm_stub_binding_helper

labs_bp:
	.indirect_symbol	_labs
	.long	cfm_stub_binding_helper

ctime_bp:
	.indirect_symbol	_ctime
	.long	cfm_stub_binding_helper

strncpy_bp:
	.indirect_symbol	_strncpy
	.long	cfm_stub_binding_helper

memcmp_bp:
	.indirect_symbol	_memcmp
	.long	cfm_stub_binding_helper

fgetc_bp:
	.indirect_symbol	_fgetc
	.long	cfm_stub_binding_helper

bsearch_bp:
	.indirect_symbol	_bsearch
	.long	cfm_stub_binding_helper

fcntl_bp:
	.indirect_symbol	_fcntl
	.long	cfm_stub_binding_helper

fprintf_bp:
	.indirect_symbol	_fprintf
	.long	cfm_stub_binding_helper

feof_bp:
	.indirect_symbol	_feof
	.long	cfm_stub_binding_helper

open_bp:
	.indirect_symbol	_open
	.long	cfm_stub_binding_helper

putc_bp:
	.indirect_symbol	_putc
	.long	cfm_stub_binding_helper

isdigit_bp:
	.indirect_symbol	_isdigit
	.long	cfm_stub_binding_helper

fread_bp:
	.indirect_symbol	_fread
	.long	cfm_stub_binding_helper

clock_bp:
	.indirect_symbol	_clock
	.long	cfm_stub_binding_helper

memchr_bp:
	.indirect_symbol	_memchr
	.long	cfm_stub_binding_helper

isgraph_bp:
	.indirect_symbol	_isgraph
	.long	cfm_stub_binding_helper

dup_bp:
	.indirect_symbol	_dup
	.long	cfm_stub_binding_helper

getenv_bp:
	.indirect_symbol	_getenv
	.long	cfm_stub_binding_helper

isupper_bp:
	.indirect_symbol	_isupper
	.long	cfm_stub_binding_helper

perror_bp:
	.indirect_symbol	_perror
	.long	cfm_stub_binding_helper

toascii_bp:
	.indirect_symbol	_toascii
	.long	cfm_stub_binding_helper

ungetc_bp:
	.indirect_symbol	_ungetc
	.long	cfm_stub_binding_helper

fclose_bp:
	.indirect_symbol	_fclose
	.long	cfm_stub_binding_helper

memset_bp:
	.indirect_symbol	_memset
	.long	cfm_stub_binding_helper

mktime_bp:
	.indirect_symbol	_mktime
	.long	cfm_stub_binding_helper

realloc_bp:
	.indirect_symbol	_realloc
	.long	cfm_stub_binding_helper

strcpy_bp:
	.indirect_symbol	_strcpy
	.long	cfm_stub_binding_helper

srand_bp:
	.indirect_symbol	_srand
	.long	cfm_stub_binding_helper

strlen_bp:
	.indirect_symbol	_strlen
	.long	cfm_stub_binding_helper

strstr_bp:
	.indirect_symbol	_strstr
	.long	cfm_stub_binding_helper

getpid_bp:
	.indirect_symbol	_getpid
	.long	cfm_stub_binding_helper

mblen_bp:
	.indirect_symbol	_mblen
	.long	cfm_stub_binding_helper

fgets_bp:
	.indirect_symbol	_fgets
	.long	cfm_stub_binding_helper

setlocale_bp:
	.indirect_symbol	_setlocale
	.long	cfm_stub_binding_helper

ferror_bp:
	.indirect_symbol	_ferror
	.long	cfm_stub_binding_helper

vprintf_bp:
	.indirect_symbol	_vprintf
	.long	cfm_stub_binding_helper

ispunct_bp:
	.indirect_symbol	_ispunct
	.long	cfm_stub_binding_helper

puts_bp:
	.indirect_symbol	_puts
	.long	cfm_stub_binding_helper

freopen_bp:
	.indirect_symbol	_freopen
	.long	cfm_stub_binding_helper

localeconv_bp:
	.indirect_symbol	_localeconv
	.long	cfm_stub_binding_helper

scanf_bp:
	.indirect_symbol	_scanf
	.long	cfm_stub_binding_helper

setvbuf_bp:
	.indirect_symbol	_setvbuf
	.long	cfm_stub_binding_helper

strrchr_bp:
	.indirect_symbol	_strrchr
	.long	cfm_stub_binding_helper

putw_bp:
	.indirect_symbol	_putw
	.long	cfm_stub_binding_helper

sprintf_bp:
	.indirect_symbol	_sprintf
	.long	cfm_stub_binding_helper

tanh_bp:
	.indirect_symbol	_tanh
	.long	cfm_stub_binding_helper

tan_bp:
	.indirect_symbol	_tan
	.long	cfm_stub_binding_helper

sqrt_bp:
	.indirect_symbol	_sqrt
	.long	cfm_stub_binding_helper

sinh_bp:
	.indirect_symbol	_sinh
	.long	cfm_stub_binding_helper

sin_bp:
	.indirect_symbol	_sin
	.long	cfm_stub_binding_helper

scalb_bp:
	.indirect_symbol	_scalb
	.long	cfm_stub_binding_helper

rint_bp:
	.indirect_symbol	_rint
	.long	cfm_stub_binding_helper

remainder_bp:
	.indirect_symbol	_remainder
	.long	cfm_stub_binding_helper

pow_bp:
	.indirect_symbol	_pow
	.long	cfm_stub_binding_helper

modf_bp:
	.indirect_symbol	_modf
	.long	cfm_stub_binding_helper

logb_bp:
	.indirect_symbol	_logb
	.long	cfm_stub_binding_helper

log1p_bp:
	.indirect_symbol	_log1p
	.long	cfm_stub_binding_helper

log10_bp:
	.indirect_symbol	_log10
	.long	cfm_stub_binding_helper

log_bp:
	.indirect_symbol	_log
	.long	cfm_stub_binding_helper

lgamma_bp:
	.indirect_symbol	_lgamma
	.long	cfm_stub_binding_helper

ldexp_bp:
	.indirect_symbol	_ldexp
	.long	cfm_stub_binding_helper

hypot_bp:
	.indirect_symbol	_hypot
	.long	cfm_stub_binding_helper

gamma_bp:
	.indirect_symbol	_gamma
	.long	cfm_stub_binding_helper

frexp_bp:
	.indirect_symbol	_frexp
	.long	cfm_stub_binding_helper

fmod_bp:
	.indirect_symbol	_fmod
	.long	cfm_stub_binding_helper

floor_bp:
	.indirect_symbol	_floor
	.long	cfm_stub_binding_helper

fabs_bp:
	.indirect_symbol	_fabs
	.long	cfm_stub_binding_helper

expm1_bp:
	.indirect_symbol	_expm1
	.long	cfm_stub_binding_helper

exp_bp:
	.indirect_symbol	_exp
	.long	cfm_stub_binding_helper

erfc_bp:
	.indirect_symbol	_erfc
	.long	cfm_stub_binding_helper

erf_bp:
	.indirect_symbol	_erf
	.long	cfm_stub_binding_helper

cosh_bp:
	.indirect_symbol	_cosh
	.long	cfm_stub_binding_helper

cos_bp:
	.indirect_symbol	_cos
	.long	cfm_stub_binding_helper

copysign_bp:
	.indirect_symbol	_copysign
	.long	cfm_stub_binding_helper

ceil_bp:
	.indirect_symbol	_ceil
	.long	cfm_stub_binding_helper

atanh_bp:
	.indirect_symbol	_atanh
	.long	cfm_stub_binding_helper

atan2_bp:
	.indirect_symbol	_atan2
	.long	cfm_stub_binding_helper

atan_bp:
	.indirect_symbol	_atan
	.long	cfm_stub_binding_helper

asinh_bp:
	.indirect_symbol	_asinh
	.long	cfm_stub_binding_helper

asin_bp:
	.indirect_symbol	_asin
	.long	cfm_stub_binding_helper

acosh_bp:
	.indirect_symbol	_acosh
	.long	cfm_stub_binding_helper

acos_bp:
	.indirect_symbol	_acos
	.long	cfm_stub_binding_helper

	.section	__DATA, __VLib_Data_BPs, non_lazy_symbol_pointers

	.align	2

#else
#endif

