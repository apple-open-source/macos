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
	.long	182
	.long	0
	.long	14
	.long	15
	.long	50
	.long	0x70777063
	.long	0x00000000
	.long	0x00000001
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
	.long	0x001C0018
	.long	0x0018001F
	.long	0x00200025
	.long	0x001C002D
	.long	0x00140034
	.long	0x00180039
	.long	0x0010003F
	.long	0x00180043
	.long	0x00180049
	.long	0x0010004F
	.long	0x000C0053
	.long	0x00180056
	.long	0x0010005C
	.long	0x00100060
	.long	0x000C0064
	.long	0x00180067
	.long	0x0008006D
	.long	0x0008006F
	.long	0x00100071
	.long	0x001C0075
	.long	0x0024007C
	.long	0x00200085
	.long	0x0018008D
	.long	0x001C0093
	.long	0x0018009A
	.long	0x002800A0
	.long	0x001C00AA
	.long	0x001400B1

VLib_HashKeys:

	.long	0x000717DE
	.long	0x00071021
	.long	0x00060A94
	.long	0x000B82F7
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
	.long	0x000B82F5
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
	.long	0x000828C2
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
	.long	0x000ACAFA
	.long	0x00071182
	.long	0x000402BB
	.long	0x00071582
	.long	0x0004023E
	.long	0x0006082E
	.long	0x0005048B
	.long	0x0006088B
	.long	0x00071736
	.long	0x000ACAF8
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
	.long	0x000505BB
	.long	0x00050436
	.long	0x0003017C
	.long	0x00082E64
	.long	0x000402A2
	.long	0x000822A2
	.long	0x000717E8
	.long	0x00094E64
	.long	0x00040229
	.long	0x000609F7
	.long	0x00071594
	.long	0x00060BA5
	.long	0x00082531
	.long	0x000609D6
	.long	0x000717E7
	.long	0x0006089C
	.long	0x000D3629
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
	.long	0x000D362B
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
	.long	0x0008301C
	.long	0x0004024F
	.long	0x000402CB
	.long	0x0007145F
	.long	0x00071705
	.long	0x0004026E
	.long	0x0007143C
	.long	0x00030134
	.long	0x0006081D
	.long	0x00050534
	.long	0x000B8191
	.long	0x00030116
	.long	0x000504BB
	.long	0x00050516
	.long	0x00050516
	.long	0x000712AB
	.long	0x0006089A
	.long	0x0008301E
	.long	0x000955D1
	.long	0x0007145D
	.long	0x00060A4D
	.long	0x0005047C
	.long	0x000B8193

VLib_ExportNames:

	.long	0x5F5F7369, 0x676E6269, 0x74665F5F, 0x69736E61
	.long	0x6E665F5F, 0x69736669, 0x6E697465, 0x665F5F69
	.long	0x736E6F72, 0x6D616C66, 0x5F5F6973, 0x6E616E64
	.long	0x5F5F6973, 0x66696E69, 0x7465645F, 0x5F69736E
	.long	0x6F726D61, 0x6C645F5F, 0x6670636C, 0x61737369
	.long	0x66796674, 0x72756E63, 0x726F756E, 0x64746F6C
	.long	0x726F756E, 0x6472696E, 0x74746F6C, 0x6E656172
	.long	0x6279696E, 0x745F5F73, 0x69676E62, 0x6974645F
	.long	0x5F667063, 0x6C617373, 0x69667964, 0x666F7065
	.long	0x6E72656D, 0x6F766572, 0x65616C6C, 0x6F637365
	.long	0x746C6F63, 0x616C6566, 0x666C7573, 0x68617363
	.long	0x74696D65, 0x7363616E, 0x66736361, 0x6C626578
	.long	0x706D3165, 0x72666361, 0x6C6C6F63, 0x64697674
	.long	0x6F6C6F77, 0x65726578, 0x69747374, 0x726E6370
	.long	0x79746F61, 0x73636969, 0x70757477, 0x65726663
	.long	0x61746F69, 0x73696E68, 0x65787061, 0x74616E32
	.long	0x6173696E, 0x6869736C, 0x6F776572, 0x77637374
	.long	0x6F6D6273, 0x69736173, 0x6369696D, 0x656D6368
	.long	0x7273696E, 0x636F7368, 0x61636F73, 0x68616273
	.long	0x69737370, 0x61636566, 0x70757463, 0x76737072
	.long	0x696E7466, 0x73717274, 0x6973636E, 0x74726C6D
	.long	0x62746F77, 0x6372616E, 0x6461746F, 0x6C646966
	.long	0x6674696D, 0x65737472, 0x6C656E73, 0x74727374
	.long	0x72707574, 0x7366646F, 0x70656E73, 0x74727062
	.long	0x726B6673, 0x63616E66, 0x6D627374, 0x6F776373
	.long	0x73747263, 0x70797365, 0x74766275, 0x666C6761
	.long	0x6D6D6161, 0x636F7370, 0x75746368, 0x6172636C
	.long	0x65617265, 0x72727265, 0x61647374, 0x72657272
	.long	0x6F727461, 0x6E6C6F67, 0x31706765, 0x74637374
	.long	0x72636174, 0x6D656D6D, 0x6F766566, 0x67657473
	.long	0x6C6F6731, 0x30756E67, 0x65746367, 0x616D6D61
	.long	0x6D616C6C, 0x6F637374, 0x726E6361, 0x7461746F
	.long	0x666D656D, 0x636D706C, 0x64657870, 0x66616273
	.long	0x66676574, 0x706F7369, 0x73646967, 0x69746D62
	.long	0x6C656E69, 0x73707269, 0x6E74746D, 0x706E616D
	.long	0x70657272, 0x6F727265, 0x6D61696E, 0x64657276
	.long	0x66707269, 0x6E746673, 0x72616E64, 0x6672656F
	.long	0x70656E66, 0x6C6F6F72, 0x7374726E, 0x636D7067
	.long	0x6D74696D, 0x65637469, 0x6D656765, 0x74656E76
	.long	0x666D6F64, 0x74696D65, 0x69737075, 0x6E637461
	.long	0x626F7274, 0x73747273, 0x706E7373, 0x63616E66
	.long	0x636C6F73, 0x65737472, 0x746F6C73, 0x74727866
	.long	0x726D6973, 0x616C6E75, 0x6D66656F, 0x666F7065
	.long	0x6E737472, 0x746F6B6C, 0x6F6E676A, 0x6D707374
	.long	0x72636F6C, 0x6C737472, 0x6674696D, 0x65667365
	.long	0x656B6173, 0x696E6670, 0x75747373, 0x7472636D
	.long	0x70697367, 0x72617068, 0x66636C6F, 0x73656973
	.long	0x78646967, 0x69747173, 0x6F727472, 0x656E616D
	.long	0x65676574, 0x63686172, 0x70757463, 0x6D6F6466
	.long	0x62736561, 0x72636866, 0x7072696E, 0x74667670
	.long	0x72696E74, 0x6672696E, 0x74636F73, 0x6674656C
	.long	0x6C696F63, 0x746C7379, 0x7374656D, 0x77726974
	.long	0x65776374, 0x6F6D6266, 0x6572726F, 0x72617461
	.long	0x6E676574, 0x73667365, 0x74706F73, 0x61746578
	.long	0x69746C64, 0x69766663, 0x6E746C6D, 0x6B74696D
	.long	0x656C6F63, 0x616C6563, 0x6F6E7663, 0x6F707973
	.long	0x69676E73, 0x7472746F, 0x756C746D, 0x7066696C
	.long	0x65667265, 0x65666765, 0x74636D65, 0x6D736574
	.long	0x67657470, 0x69647374, 0x72746F64, 0x7072696E
	.long	0x74666672, 0x65616469, 0x73757070, 0x65727461
	.long	0x6E686174, 0x616E686C, 0x6F63616C, 0x74696D65
	.long	0x73747263, 0x73706E63, 0x6C6F636B, 0x6C6F6762
	.long	0x6879706F, 0x74667265, 0x78706365, 0x696C6765
	.long	0x74777374, 0x72636872, 0x746F7570, 0x70657275
	.long	0x6E6C696E, 0x6B647570, 0x73707269, 0x6E746670
	.long	0x6F776677, 0x72697465, 0x6D656D63, 0x70797265
	.long	0x77696E64, 0x6C616273, 0x6C6F6773, 0x65746275
	.long	0x66697361, 0x6C706861, 0x73747272, 0x63687200

	.section	__TEXT, __VLib_Exports, symbol_stubs, none, 8

	.align	2

VLib_ExportSymbols:

	.indirect_symbol	_strrchr
	.long	0x02000438
	.long	strrchr_bp - VLib_Origin

	.indirect_symbol	_isalpha
	.long	0x02000431
	.long	isalpha_bp - VLib_Origin

	.indirect_symbol	_setbuf
	.long	0x0200042B
	.long	setbuf_bp - VLib_Origin

	.indirect_symbol	___isfinited
	.long	0x02000030
	.long	__isfinited_bp - VLib_Origin

	.indirect_symbol	_log
	.long	0x02000428
	.long	log_bp - VLib_Origin

	.indirect_symbol	_labs
	.long	0x02000424
	.long	labs_bp - VLib_Origin

	.indirect_symbol	_rewind
	.long	0x0200041E
	.long	rewind_bp - VLib_Origin

	.indirect_symbol	_memcpy
	.long	0x02000418
	.long	memcpy_bp - VLib_Origin

	.indirect_symbol	_fwrite
	.long	0x02000412
	.long	fwrite_bp - VLib_Origin

	.indirect_symbol	_pow
	.long	0x0200040F
	.long	pow_bp - VLib_Origin

	.indirect_symbol	_sprintf
	.long	0x02000408
	.long	sprintf_bp - VLib_Origin

	.indirect_symbol	_dup
	.long	0x02000405
	.long	dup_bp - VLib_Origin

	.indirect_symbol	_unlink
	.long	0x020003FF
	.long	unlink_bp - VLib_Origin

	.indirect_symbol	_toupper
	.long	0x020003F8
	.long	toupper_bp - VLib_Origin

	.indirect_symbol	_strchr
	.long	0x020003F2
	.long	strchr_bp - VLib_Origin

	.indirect_symbol	_getw
	.long	0x020003EE
	.long	getw_bp - VLib_Origin

	.indirect_symbol	___isfinitef
	.long	0x02000012
	.long	__isfinitef_bp - VLib_Origin

	.indirect_symbol	_ceil
	.long	0x020003EA
	.long	ceil_bp - VLib_Origin

	.indirect_symbol	_frexp
	.long	0x020003E5
	.long	frexp_bp - VLib_Origin

	.indirect_symbol	_hypot
	.long	0x020003E0
	.long	hypot_bp - VLib_Origin

	.indirect_symbol	_logb
	.long	0x020003DC
	.long	logb_bp - VLib_Origin

	.indirect_symbol	_clock
	.long	0x020003D7
	.long	clock_bp - VLib_Origin

	.indirect_symbol	_strcspn
	.long	0x020003D0
	.long	strcspn_bp - VLib_Origin

	.indirect_symbol	_localtime
	.long	0x020003C7
	.long	localtime_bp - VLib_Origin

	.indirect_symbol	_atanh
	.long	0x020003C2
	.long	atanh_bp - VLib_Origin

	.indirect_symbol	_tanh
	.long	0x020003BE
	.long	tanh_bp - VLib_Origin

	.indirect_symbol	_isupper
	.long	0x020003B7
	.long	isupper_bp - VLib_Origin

	.indirect_symbol	_fread
	.long	0x020003B2
	.long	fread_bp - VLib_Origin

	.indirect_symbol	_printf
	.long	0x020003AC
	.long	printf_bp - VLib_Origin

	.indirect_symbol	_strtod
	.long	0x020003A6
	.long	strtod_bp - VLib_Origin

	.indirect_symbol	_roundtol
	.long	0x02000058
	.long	roundtol_bp - VLib_Origin

	.indirect_symbol	_getpid
	.long	0x020003A0
	.long	getpid_bp - VLib_Origin

	.indirect_symbol	_memset
	.long	0x0200039A
	.long	memset_bp - VLib_Origin

	.indirect_symbol	_fgetc
	.long	0x02000395
	.long	fgetc_bp - VLib_Origin

	.indirect_symbol	_free
	.long	0x02000391
	.long	free_bp - VLib_Origin

	.indirect_symbol	_tmpfile
	.long	0x0200038A
	.long	tmpfile_bp - VLib_Origin

	.indirect_symbol	_strtoul
	.long	0x02000383
	.long	strtoul_bp - VLib_Origin

	.indirect_symbol	_copysign
	.long	0x0200037B
	.long	copysign_bp - VLib_Origin

	.indirect_symbol	_localeconv
	.long	0x02000371
	.long	localeconv_bp - VLib_Origin

	.indirect_symbol	_mktime
	.long	0x0200036B
	.long	mktime_bp - VLib_Origin

	.indirect_symbol	_fcntl
	.long	0x02000366
	.long	fcntl_bp - VLib_Origin

	.indirect_symbol	_ldiv
	.long	0x02000362
	.long	ldiv_bp - VLib_Origin

	.indirect_symbol	_atexit
	.long	0x0200035C
	.long	atexit_bp - VLib_Origin

	.indirect_symbol	_fsetpos
	.long	0x02000355
	.long	fsetpos_bp - VLib_Origin

	.indirect_symbol	_gets
	.long	0x02000351
	.long	gets_bp - VLib_Origin

	.indirect_symbol	_atan
	.long	0x0200034D
	.long	atan_bp - VLib_Origin

	.indirect_symbol	_ferror
	.long	0x02000347
	.long	ferror_bp - VLib_Origin

	.indirect_symbol	_wctomb
	.long	0x02000341
	.long	wctomb_bp - VLib_Origin

	.indirect_symbol	_write
	.long	0x0200033C
	.long	write_bp - VLib_Origin

	.indirect_symbol	_system
	.long	0x02000336
	.long	system_bp - VLib_Origin

	.indirect_symbol	_ioctl
	.long	0x02000331
	.long	ioctl_bp - VLib_Origin

	.indirect_symbol	_ftell
	.long	0x0200032C
	.long	ftell_bp - VLib_Origin

	.indirect_symbol	_cos
	.long	0x02000329
	.long	cos_bp - VLib_Origin

	.indirect_symbol	_rint
	.long	0x02000325
	.long	rint_bp - VLib_Origin

	.indirect_symbol	_vprintf
	.long	0x0200031E
	.long	vprintf_bp - VLib_Origin

	.indirect_symbol	_fprintf
	.long	0x02000317
	.long	fprintf_bp - VLib_Origin

	.indirect_symbol	_bsearch
	.long	0x02000310
	.long	bsearch_bp - VLib_Origin

	.indirect_symbol	_modf
	.long	0x0200030C
	.long	modf_bp - VLib_Origin

	.indirect_symbol	_putc
	.long	0x02000308
	.long	putc_bp - VLib_Origin

	.indirect_symbol	_getchar
	.long	0x02000301
	.long	getchar_bp - VLib_Origin

	.indirect_symbol	_rename
	.long	0x020002FB
	.long	rename_bp - VLib_Origin

	.indirect_symbol	_qsort
	.long	0x020002F6
	.long	qsort_bp - VLib_Origin

	.indirect_symbol	_isxdigit
	.long	0x020002EE
	.long	isxdigit_bp - VLib_Origin

	.indirect_symbol	_fclose
	.long	0x020002E8
	.long	fclose_bp - VLib_Origin

	.indirect_symbol	_isgraph
	.long	0x020002E1
	.long	isgraph_bp - VLib_Origin

	.indirect_symbol	_strcmp
	.long	0x020002DB
	.long	strcmp_bp - VLib_Origin

	.indirect_symbol	_fputs
	.long	0x020002D6
	.long	fputs_bp - VLib_Origin

	.indirect_symbol	_asin
	.long	0x020002D2
	.long	asin_bp - VLib_Origin

	.indirect_symbol	_fseek
	.long	0x020002CD
	.long	fseek_bp - VLib_Origin

	.indirect_symbol	_strftime
	.long	0x020002C5
	.long	strftime_bp - VLib_Origin

	.indirect_symbol	_strcoll
	.long	0x020002BE
	.long	strcoll_bp - VLib_Origin

	.indirect_symbol	_longjmp
	.long	0x020002B7
	.long	longjmp_bp - VLib_Origin

	.indirect_symbol	_strtok
	.long	0x020002B1
	.long	strtok_bp - VLib_Origin

	.indirect_symbol	_open
	.long	0x020002AD
	.long	open_bp - VLib_Origin

	.indirect_symbol	_feof
	.long	0x020002A9
	.long	feof_bp - VLib_Origin

	.indirect_symbol	_isalnum
	.long	0x020002A2
	.long	isalnum_bp - VLib_Origin

	.indirect_symbol	_strxfrm
	.long	0x0200029B
	.long	strxfrm_bp - VLib_Origin

	.indirect_symbol	_strtol
	.long	0x02000295
	.long	strtol_bp - VLib_Origin

	.indirect_symbol	_close
	.long	0x02000290
	.long	close_bp - VLib_Origin

	.indirect_symbol	_sscanf
	.long	0x0200028A
	.long	sscanf_bp - VLib_Origin

	.indirect_symbol	_strspn
	.long	0x02000284
	.long	strspn_bp - VLib_Origin

	.indirect_symbol	_abort
	.long	0x0200027F
	.long	abort_bp - VLib_Origin

	.indirect_symbol	___signbitf
	.long	0x02000000
	.long	__signbitf_bp - VLib_Origin

	.indirect_symbol	_ispunct
	.long	0x02000278
	.long	ispunct_bp - VLib_Origin

	.indirect_symbol	_time
	.long	0x02000274
	.long	time_bp - VLib_Origin

	.indirect_symbol	_rinttol
	.long	0x02000065
	.long	rinttol_bp - VLib_Origin

	.indirect_symbol	_fmod
	.long	0x02000270
	.long	fmod_bp - VLib_Origin

	.indirect_symbol	_getenv
	.long	0x0200026A
	.long	getenv_bp - VLib_Origin

	.indirect_symbol	_ctime
	.long	0x02000265
	.long	ctime_bp - VLib_Origin

	.indirect_symbol	_gmtime
	.long	0x0200025F
	.long	gmtime_bp - VLib_Origin

	.indirect_symbol	_strncmp
	.long	0x02000258
	.long	strncmp_bp - VLib_Origin

	.indirect_symbol	___signbitd
	.long	0x02000075
	.long	__signbitd_bp - VLib_Origin

	.indirect_symbol	_floor
	.long	0x02000253
	.long	floor_bp - VLib_Origin

	.indirect_symbol	_freopen
	.long	0x0200024C
	.long	freopen_bp - VLib_Origin

	.indirect_symbol	_srand
	.long	0x02000247
	.long	srand_bp - VLib_Origin

	.indirect_symbol	_vfprintf
	.long	0x0200023F
	.long	vfprintf_bp - VLib_Origin

	.indirect_symbol	_remainder
	.long	0x02000236
	.long	remainder_bp - VLib_Origin

	.indirect_symbol	_perror
	.long	0x02000230
	.long	perror_bp - VLib_Origin

	.indirect_symbol	_tmpnam
	.long	0x0200022A
	.long	tmpnam_bp - VLib_Origin

	.indirect_symbol	_isprint
	.long	0x02000223
	.long	isprint_bp - VLib_Origin

	.indirect_symbol	_mblen
	.long	0x0200021E
	.long	mblen_bp - VLib_Origin

	.indirect_symbol	_isdigit
	.long	0x02000217
	.long	isdigit_bp - VLib_Origin

	.indirect_symbol	_fgetpos
	.long	0x02000210
	.long	fgetpos_bp - VLib_Origin

	.indirect_symbol	_fabs
	.long	0x0200020C
	.long	fabs_bp - VLib_Origin

	.indirect_symbol	_ldexp
	.long	0x02000207
	.long	ldexp_bp - VLib_Origin

	.indirect_symbol	_memcmp
	.long	0x02000201
	.long	memcmp_bp - VLib_Origin

	.indirect_symbol	_atof
	.long	0x020001FD
	.long	atof_bp - VLib_Origin

	.indirect_symbol	_strncat
	.long	0x020001F6
	.long	strncat_bp - VLib_Origin

	.indirect_symbol	_malloc
	.long	0x020001F0
	.long	malloc_bp - VLib_Origin

	.indirect_symbol	_gamma
	.long	0x020001EB
	.long	gamma_bp - VLib_Origin

	.indirect_symbol	_ungetc
	.long	0x020001E5
	.long	ungetc_bp - VLib_Origin

	.indirect_symbol	_log10
	.long	0x020001E0
	.long	log10_bp - VLib_Origin

	.indirect_symbol	_fgets
	.long	0x020001DB
	.long	fgets_bp - VLib_Origin

	.indirect_symbol	_memmove
	.long	0x020001D4
	.long	memmove_bp - VLib_Origin

	.indirect_symbol	_strcat
	.long	0x020001CE
	.long	strcat_bp - VLib_Origin

	.indirect_symbol	_getc
	.long	0x020001CA
	.long	getc_bp - VLib_Origin

	.indirect_symbol	_trunc
	.long	0x02000053
	.long	trunc_bp - VLib_Origin

	.indirect_symbol	_log1p
	.long	0x020001C5
	.long	log1p_bp - VLib_Origin

	.indirect_symbol	_tan
	.long	0x020001C2
	.long	tan_bp - VLib_Origin

	.indirect_symbol	_strerror
	.long	0x020001BA
	.long	strerror_bp - VLib_Origin

	.indirect_symbol	_read
	.long	0x020001B6
	.long	read_bp - VLib_Origin

	.indirect_symbol	_clearerr
	.long	0x020001AE
	.long	clearerr_bp - VLib_Origin

	.indirect_symbol	_putchar
	.long	0x020001A7
	.long	putchar_bp - VLib_Origin

	.indirect_symbol	_nearbyint
	.long	0x0200006C
	.long	nearbyint_bp - VLib_Origin

	.indirect_symbol	_acos
	.long	0x020001A3
	.long	acos_bp - VLib_Origin

	.indirect_symbol	_lgamma
	.long	0x0200019D
	.long	lgamma_bp - VLib_Origin

	.indirect_symbol	_setvbuf
	.long	0x02000196
	.long	setvbuf_bp - VLib_Origin

	.indirect_symbol	_strcpy
	.long	0x02000190
	.long	strcpy_bp - VLib_Origin

	.indirect_symbol	_mbstowcs
	.long	0x02000188
	.long	mbstowcs_bp - VLib_Origin

	.indirect_symbol	_fscanf
	.long	0x02000182
	.long	fscanf_bp - VLib_Origin

	.indirect_symbol	_strpbrk
	.long	0x0200017B
	.long	strpbrk_bp - VLib_Origin

	.indirect_symbol	_fdopen
	.long	0x02000175
	.long	fdopen_bp - VLib_Origin

	.indirect_symbol	___fpclassifyf
	.long	0x02000046
	.long	__fpclassifyf_bp - VLib_Origin

	.indirect_symbol	_puts
	.long	0x02000171
	.long	puts_bp - VLib_Origin

	.indirect_symbol	_strstr
	.long	0x0200016B
	.long	strstr_bp - VLib_Origin

	.indirect_symbol	_strlen
	.long	0x02000165
	.long	strlen_bp - VLib_Origin

	.indirect_symbol	_difftime
	.long	0x0200015D
	.long	difftime_bp - VLib_Origin

	.indirect_symbol	_atol
	.long	0x02000159
	.long	atol_bp - VLib_Origin

	.indirect_symbol	_rand
	.long	0x02000155
	.long	rand_bp - VLib_Origin

	.indirect_symbol	_mbtowc
	.long	0x0200014F
	.long	mbtowc_bp - VLib_Origin

	.indirect_symbol	_iscntrl
	.long	0x02000148
	.long	iscntrl_bp - VLib_Origin

	.indirect_symbol	_sqrt
	.long	0x02000144
	.long	sqrt_bp - VLib_Origin

	.indirect_symbol	_vsprintf
	.long	0x0200013C
	.long	vsprintf_bp - VLib_Origin

	.indirect_symbol	_fputc
	.long	0x02000137
	.long	fputc_bp - VLib_Origin

	.indirect_symbol	_isspace
	.long	0x02000130
	.long	isspace_bp - VLib_Origin

	.indirect_symbol	_abs
	.long	0x0200012D
	.long	abs_bp - VLib_Origin

	.indirect_symbol	___fpclassifyd
	.long	0x0200007F
	.long	__fpclassifyd_bp - VLib_Origin

	.indirect_symbol	_acosh
	.long	0x02000128
	.long	acosh_bp - VLib_Origin

	.indirect_symbol	_cosh
	.long	0x02000124
	.long	cosh_bp - VLib_Origin

	.indirect_symbol	_sin
	.long	0x02000121
	.long	sin_bp - VLib_Origin

	.indirect_symbol	_memchr
	.long	0x0200011B
	.long	memchr_bp - VLib_Origin

	.indirect_symbol	_isascii
	.long	0x02000114
	.long	isascii_bp - VLib_Origin

	.indirect_symbol	_wcstombs
	.long	0x0200010C
	.long	wcstombs_bp - VLib_Origin

	.indirect_symbol	_islower
	.long	0x02000105
	.long	islower_bp - VLib_Origin

	.indirect_symbol	_asinh
	.long	0x02000100
	.long	asinh_bp - VLib_Origin

	.indirect_symbol	_atan2
	.long	0x020000FB
	.long	atan2_bp - VLib_Origin

	.indirect_symbol	_exp
	.long	0x020000F8
	.long	exp_bp - VLib_Origin

	.indirect_symbol	_sinh
	.long	0x020000F4
	.long	sinh_bp - VLib_Origin

	.indirect_symbol	_atoi
	.long	0x020000F0
	.long	atoi_bp - VLib_Origin

	.indirect_symbol	___isnand
	.long	0x02000028
	.long	__isnand_bp - VLib_Origin

	.indirect_symbol	_erfc
	.long	0x020000EC
	.long	erfc_bp - VLib_Origin

	.indirect_symbol	_putw
	.long	0x020000E8
	.long	putw_bp - VLib_Origin

	.indirect_symbol	_toascii
	.long	0x020000E1
	.long	toascii_bp - VLib_Origin

	.indirect_symbol	_strncpy
	.long	0x020000DA
	.long	strncpy_bp - VLib_Origin

	.indirect_symbol	_exit
	.long	0x020000D6
	.long	exit_bp - VLib_Origin

	.indirect_symbol	_tolower
	.long	0x020000CF
	.long	tolower_bp - VLib_Origin

	.indirect_symbol	_div
	.long	0x020000CC
	.long	div_bp - VLib_Origin

	.indirect_symbol	_calloc
	.long	0x020000C6
	.long	calloc_bp - VLib_Origin

	.indirect_symbol	_round
	.long	0x02000060
	.long	round_bp - VLib_Origin

	.indirect_symbol	___isnormald
	.long	0x0200003B
	.long	__isnormald_bp - VLib_Origin

	.indirect_symbol	_erf
	.long	0x020000C3
	.long	erf_bp - VLib_Origin

	.indirect_symbol	_expm1
	.long	0x020000BE
	.long	expm1_bp - VLib_Origin

	.indirect_symbol	_scalb
	.long	0x020000B9
	.long	scalb_bp - VLib_Origin

	.indirect_symbol	_scanf
	.long	0x020000B4
	.long	scanf_bp - VLib_Origin

	.indirect_symbol	_asctime
	.long	0x020000AD
	.long	asctime_bp - VLib_Origin

	.indirect_symbol	_fflush
	.long	0x020000A7
	.long	fflush_bp - VLib_Origin

	.indirect_symbol	___isnanf
	.long	0x0200000A
	.long	__isnanf_bp - VLib_Origin

	.indirect_symbol	_setlocale
	.long	0x0200009E
	.long	setlocale_bp - VLib_Origin

	.indirect_symbol	_realloc
	.long	0x02000097
	.long	realloc_bp - VLib_Origin

	.indirect_symbol	_remove
	.long	0x02000091
	.long	remove_bp - VLib_Origin

	.indirect_symbol	_fopen
	.long	0x0200008C
	.long	fopen_bp - VLib_Origin

	.indirect_symbol	___isnormalf
	.long	0x0200001D
	.long	__isnormalf_bp - VLib_Origin


	.globl	cfm_stub_binding_helper

	.section	__DATA, __VLib_Func_BPs, lazy_symbol_pointers

	.align	2

__signbitf_bp:
	.indirect_symbol	___signbitf
	.long	cfm_stub_binding_helper

__isnanf_bp:
	.indirect_symbol	___isnanf
	.long	cfm_stub_binding_helper

__isfinitef_bp:
	.indirect_symbol	___isfinitef
	.long	cfm_stub_binding_helper

__isnormalf_bp:
	.indirect_symbol	___isnormalf
	.long	cfm_stub_binding_helper

__isnand_bp:
	.indirect_symbol	___isnand
	.long	cfm_stub_binding_helper

__isfinited_bp:
	.indirect_symbol	___isfinited
	.long	cfm_stub_binding_helper

__isnormald_bp:
	.indirect_symbol	___isnormald
	.long	cfm_stub_binding_helper

__fpclassifyf_bp:
	.indirect_symbol	___fpclassifyf
	.long	cfm_stub_binding_helper

trunc_bp:
	.indirect_symbol	_trunc
	.long	cfm_stub_binding_helper

roundtol_bp:
	.indirect_symbol	_roundtol
	.long	cfm_stub_binding_helper

round_bp:
	.indirect_symbol	_round
	.long	cfm_stub_binding_helper

rinttol_bp:
	.indirect_symbol	_rinttol
	.long	cfm_stub_binding_helper

nearbyint_bp:
	.indirect_symbol	_nearbyint
	.long	cfm_stub_binding_helper

__signbitd_bp:
	.indirect_symbol	___signbitd
	.long	cfm_stub_binding_helper

__fpclassifyd_bp:
	.indirect_symbol	___fpclassifyd
	.long	cfm_stub_binding_helper

fopen_bp:
	.indirect_symbol	_fopen
	.long	cfm_stub_binding_helper

remove_bp:
	.indirect_symbol	_remove
	.long	cfm_stub_binding_helper

realloc_bp:
	.indirect_symbol	_realloc
	.long	cfm_stub_binding_helper

setlocale_bp:
	.indirect_symbol	_setlocale
	.long	cfm_stub_binding_helper

fflush_bp:
	.indirect_symbol	_fflush
	.long	cfm_stub_binding_helper

asctime_bp:
	.indirect_symbol	_asctime
	.long	cfm_stub_binding_helper

scanf_bp:
	.indirect_symbol	_scanf
	.long	cfm_stub_binding_helper

scalb_bp:
	.indirect_symbol	_scalb
	.long	cfm_stub_binding_helper

expm1_bp:
	.indirect_symbol	_expm1
	.long	cfm_stub_binding_helper

erf_bp:
	.indirect_symbol	_erf
	.long	cfm_stub_binding_helper

calloc_bp:
	.indirect_symbol	_calloc
	.long	cfm_stub_binding_helper

div_bp:
	.indirect_symbol	_div
	.long	cfm_stub_binding_helper

tolower_bp:
	.indirect_symbol	_tolower
	.long	cfm_stub_binding_helper

exit_bp:
	.indirect_symbol	_exit
	.long	cfm_stub_binding_helper

strncpy_bp:
	.indirect_symbol	_strncpy
	.long	cfm_stub_binding_helper

toascii_bp:
	.indirect_symbol	_toascii
	.long	cfm_stub_binding_helper

putw_bp:
	.indirect_symbol	_putw
	.long	cfm_stub_binding_helper

erfc_bp:
	.indirect_symbol	_erfc
	.long	cfm_stub_binding_helper

atoi_bp:
	.indirect_symbol	_atoi
	.long	cfm_stub_binding_helper

sinh_bp:
	.indirect_symbol	_sinh
	.long	cfm_stub_binding_helper

exp_bp:
	.indirect_symbol	_exp
	.long	cfm_stub_binding_helper

atan2_bp:
	.indirect_symbol	_atan2
	.long	cfm_stub_binding_helper

asinh_bp:
	.indirect_symbol	_asinh
	.long	cfm_stub_binding_helper

islower_bp:
	.indirect_symbol	_islower
	.long	cfm_stub_binding_helper

wcstombs_bp:
	.indirect_symbol	_wcstombs
	.long	cfm_stub_binding_helper

isascii_bp:
	.indirect_symbol	_isascii
	.long	cfm_stub_binding_helper

memchr_bp:
	.indirect_symbol	_memchr
	.long	cfm_stub_binding_helper

sin_bp:
	.indirect_symbol	_sin
	.long	cfm_stub_binding_helper

cosh_bp:
	.indirect_symbol	_cosh
	.long	cfm_stub_binding_helper

acosh_bp:
	.indirect_symbol	_acosh
	.long	cfm_stub_binding_helper

abs_bp:
	.indirect_symbol	_abs
	.long	cfm_stub_binding_helper

isspace_bp:
	.indirect_symbol	_isspace
	.long	cfm_stub_binding_helper

fputc_bp:
	.indirect_symbol	_fputc
	.long	cfm_stub_binding_helper

vsprintf_bp:
	.indirect_symbol	_vsprintf
	.long	cfm_stub_binding_helper

sqrt_bp:
	.indirect_symbol	_sqrt
	.long	cfm_stub_binding_helper

iscntrl_bp:
	.indirect_symbol	_iscntrl
	.long	cfm_stub_binding_helper

mbtowc_bp:
	.indirect_symbol	_mbtowc
	.long	cfm_stub_binding_helper

rand_bp:
	.indirect_symbol	_rand
	.long	cfm_stub_binding_helper

atol_bp:
	.indirect_symbol	_atol
	.long	cfm_stub_binding_helper

difftime_bp:
	.indirect_symbol	_difftime
	.long	cfm_stub_binding_helper

strlen_bp:
	.indirect_symbol	_strlen
	.long	cfm_stub_binding_helper

strstr_bp:
	.indirect_symbol	_strstr
	.long	cfm_stub_binding_helper

puts_bp:
	.indirect_symbol	_puts
	.long	cfm_stub_binding_helper

fdopen_bp:
	.indirect_symbol	_fdopen
	.long	cfm_stub_binding_helper

strpbrk_bp:
	.indirect_symbol	_strpbrk
	.long	cfm_stub_binding_helper

fscanf_bp:
	.indirect_symbol	_fscanf
	.long	cfm_stub_binding_helper

mbstowcs_bp:
	.indirect_symbol	_mbstowcs
	.long	cfm_stub_binding_helper

strcpy_bp:
	.indirect_symbol	_strcpy
	.long	cfm_stub_binding_helper

setvbuf_bp:
	.indirect_symbol	_setvbuf
	.long	cfm_stub_binding_helper

lgamma_bp:
	.indirect_symbol	_lgamma
	.long	cfm_stub_binding_helper

acos_bp:
	.indirect_symbol	_acos
	.long	cfm_stub_binding_helper

putchar_bp:
	.indirect_symbol	_putchar
	.long	cfm_stub_binding_helper

clearerr_bp:
	.indirect_symbol	_clearerr
	.long	cfm_stub_binding_helper

read_bp:
	.indirect_symbol	_read
	.long	cfm_stub_binding_helper

strerror_bp:
	.indirect_symbol	_strerror
	.long	cfm_stub_binding_helper

tan_bp:
	.indirect_symbol	_tan
	.long	cfm_stub_binding_helper

log1p_bp:
	.indirect_symbol	_log1p
	.long	cfm_stub_binding_helper

getc_bp:
	.indirect_symbol	_getc
	.long	cfm_stub_binding_helper

strcat_bp:
	.indirect_symbol	_strcat
	.long	cfm_stub_binding_helper

memmove_bp:
	.indirect_symbol	_memmove
	.long	cfm_stub_binding_helper

fgets_bp:
	.indirect_symbol	_fgets
	.long	cfm_stub_binding_helper

log10_bp:
	.indirect_symbol	_log10
	.long	cfm_stub_binding_helper

ungetc_bp:
	.indirect_symbol	_ungetc
	.long	cfm_stub_binding_helper

gamma_bp:
	.indirect_symbol	_gamma
	.long	cfm_stub_binding_helper

malloc_bp:
	.indirect_symbol	_malloc
	.long	cfm_stub_binding_helper

strncat_bp:
	.indirect_symbol	_strncat
	.long	cfm_stub_binding_helper

atof_bp:
	.indirect_symbol	_atof
	.long	cfm_stub_binding_helper

memcmp_bp:
	.indirect_symbol	_memcmp
	.long	cfm_stub_binding_helper

ldexp_bp:
	.indirect_symbol	_ldexp
	.long	cfm_stub_binding_helper

fabs_bp:
	.indirect_symbol	_fabs
	.long	cfm_stub_binding_helper

fgetpos_bp:
	.indirect_symbol	_fgetpos
	.long	cfm_stub_binding_helper

isdigit_bp:
	.indirect_symbol	_isdigit
	.long	cfm_stub_binding_helper

mblen_bp:
	.indirect_symbol	_mblen
	.long	cfm_stub_binding_helper

isprint_bp:
	.indirect_symbol	_isprint
	.long	cfm_stub_binding_helper

tmpnam_bp:
	.indirect_symbol	_tmpnam
	.long	cfm_stub_binding_helper

perror_bp:
	.indirect_symbol	_perror
	.long	cfm_stub_binding_helper

remainder_bp:
	.indirect_symbol	_remainder
	.long	cfm_stub_binding_helper

vfprintf_bp:
	.indirect_symbol	_vfprintf
	.long	cfm_stub_binding_helper

srand_bp:
	.indirect_symbol	_srand
	.long	cfm_stub_binding_helper

freopen_bp:
	.indirect_symbol	_freopen
	.long	cfm_stub_binding_helper

floor_bp:
	.indirect_symbol	_floor
	.long	cfm_stub_binding_helper

strncmp_bp:
	.indirect_symbol	_strncmp
	.long	cfm_stub_binding_helper

gmtime_bp:
	.indirect_symbol	_gmtime
	.long	cfm_stub_binding_helper

ctime_bp:
	.indirect_symbol	_ctime
	.long	cfm_stub_binding_helper

getenv_bp:
	.indirect_symbol	_getenv
	.long	cfm_stub_binding_helper

fmod_bp:
	.indirect_symbol	_fmod
	.long	cfm_stub_binding_helper

time_bp:
	.indirect_symbol	_time
	.long	cfm_stub_binding_helper

ispunct_bp:
	.indirect_symbol	_ispunct
	.long	cfm_stub_binding_helper

abort_bp:
	.indirect_symbol	_abort
	.long	cfm_stub_binding_helper

strspn_bp:
	.indirect_symbol	_strspn
	.long	cfm_stub_binding_helper

sscanf_bp:
	.indirect_symbol	_sscanf
	.long	cfm_stub_binding_helper

close_bp:
	.indirect_symbol	_close
	.long	cfm_stub_binding_helper

strtol_bp:
	.indirect_symbol	_strtol
	.long	cfm_stub_binding_helper

strxfrm_bp:
	.indirect_symbol	_strxfrm
	.long	cfm_stub_binding_helper

isalnum_bp:
	.indirect_symbol	_isalnum
	.long	cfm_stub_binding_helper

feof_bp:
	.indirect_symbol	_feof
	.long	cfm_stub_binding_helper

open_bp:
	.indirect_symbol	_open
	.long	cfm_stub_binding_helper

strtok_bp:
	.indirect_symbol	_strtok
	.long	cfm_stub_binding_helper

longjmp_bp:
	.indirect_symbol	_longjmp
	.long	cfm_stub_binding_helper

strcoll_bp:
	.indirect_symbol	_strcoll
	.long	cfm_stub_binding_helper

strftime_bp:
	.indirect_symbol	_strftime
	.long	cfm_stub_binding_helper

fseek_bp:
	.indirect_symbol	_fseek
	.long	cfm_stub_binding_helper

asin_bp:
	.indirect_symbol	_asin
	.long	cfm_stub_binding_helper

fputs_bp:
	.indirect_symbol	_fputs
	.long	cfm_stub_binding_helper

strcmp_bp:
	.indirect_symbol	_strcmp
	.long	cfm_stub_binding_helper

isgraph_bp:
	.indirect_symbol	_isgraph
	.long	cfm_stub_binding_helper

fclose_bp:
	.indirect_symbol	_fclose
	.long	cfm_stub_binding_helper

isxdigit_bp:
	.indirect_symbol	_isxdigit
	.long	cfm_stub_binding_helper

qsort_bp:
	.indirect_symbol	_qsort
	.long	cfm_stub_binding_helper

rename_bp:
	.indirect_symbol	_rename
	.long	cfm_stub_binding_helper

getchar_bp:
	.indirect_symbol	_getchar
	.long	cfm_stub_binding_helper

putc_bp:
	.indirect_symbol	_putc
	.long	cfm_stub_binding_helper

modf_bp:
	.indirect_symbol	_modf
	.long	cfm_stub_binding_helper

bsearch_bp:
	.indirect_symbol	_bsearch
	.long	cfm_stub_binding_helper

fprintf_bp:
	.indirect_symbol	_fprintf
	.long	cfm_stub_binding_helper

vprintf_bp:
	.indirect_symbol	_vprintf
	.long	cfm_stub_binding_helper

rint_bp:
	.indirect_symbol	_rint
	.long	cfm_stub_binding_helper

cos_bp:
	.indirect_symbol	_cos
	.long	cfm_stub_binding_helper

ftell_bp:
	.indirect_symbol	_ftell
	.long	cfm_stub_binding_helper

ioctl_bp:
	.indirect_symbol	_ioctl
	.long	cfm_stub_binding_helper

system_bp:
	.indirect_symbol	_system
	.long	cfm_stub_binding_helper

write_bp:
	.indirect_symbol	_write
	.long	cfm_stub_binding_helper

wctomb_bp:
	.indirect_symbol	_wctomb
	.long	cfm_stub_binding_helper

ferror_bp:
	.indirect_symbol	_ferror
	.long	cfm_stub_binding_helper

atan_bp:
	.indirect_symbol	_atan
	.long	cfm_stub_binding_helper

gets_bp:
	.indirect_symbol	_gets
	.long	cfm_stub_binding_helper

fsetpos_bp:
	.indirect_symbol	_fsetpos
	.long	cfm_stub_binding_helper

atexit_bp:
	.indirect_symbol	_atexit
	.long	cfm_stub_binding_helper

ldiv_bp:
	.indirect_symbol	_ldiv
	.long	cfm_stub_binding_helper

fcntl_bp:
	.indirect_symbol	_fcntl
	.long	cfm_stub_binding_helper

mktime_bp:
	.indirect_symbol	_mktime
	.long	cfm_stub_binding_helper

localeconv_bp:
	.indirect_symbol	_localeconv
	.long	cfm_stub_binding_helper

copysign_bp:
	.indirect_symbol	_copysign
	.long	cfm_stub_binding_helper

strtoul_bp:
	.indirect_symbol	_strtoul
	.long	cfm_stub_binding_helper

tmpfile_bp:
	.indirect_symbol	_tmpfile
	.long	cfm_stub_binding_helper

free_bp:
	.indirect_symbol	_free
	.long	cfm_stub_binding_helper

fgetc_bp:
	.indirect_symbol	_fgetc
	.long	cfm_stub_binding_helper

memset_bp:
	.indirect_symbol	_memset
	.long	cfm_stub_binding_helper

getpid_bp:
	.indirect_symbol	_getpid
	.long	cfm_stub_binding_helper

strtod_bp:
	.indirect_symbol	_strtod
	.long	cfm_stub_binding_helper

printf_bp:
	.indirect_symbol	_printf
	.long	cfm_stub_binding_helper

fread_bp:
	.indirect_symbol	_fread
	.long	cfm_stub_binding_helper

isupper_bp:
	.indirect_symbol	_isupper
	.long	cfm_stub_binding_helper

tanh_bp:
	.indirect_symbol	_tanh
	.long	cfm_stub_binding_helper

atanh_bp:
	.indirect_symbol	_atanh
	.long	cfm_stub_binding_helper

localtime_bp:
	.indirect_symbol	_localtime
	.long	cfm_stub_binding_helper

strcspn_bp:
	.indirect_symbol	_strcspn
	.long	cfm_stub_binding_helper

clock_bp:
	.indirect_symbol	_clock
	.long	cfm_stub_binding_helper

logb_bp:
	.indirect_symbol	_logb
	.long	cfm_stub_binding_helper

hypot_bp:
	.indirect_symbol	_hypot
	.long	cfm_stub_binding_helper

frexp_bp:
	.indirect_symbol	_frexp
	.long	cfm_stub_binding_helper

ceil_bp:
	.indirect_symbol	_ceil
	.long	cfm_stub_binding_helper

getw_bp:
	.indirect_symbol	_getw
	.long	cfm_stub_binding_helper

strchr_bp:
	.indirect_symbol	_strchr
	.long	cfm_stub_binding_helper

toupper_bp:
	.indirect_symbol	_toupper
	.long	cfm_stub_binding_helper

unlink_bp:
	.indirect_symbol	_unlink
	.long	cfm_stub_binding_helper

dup_bp:
	.indirect_symbol	_dup
	.long	cfm_stub_binding_helper

sprintf_bp:
	.indirect_symbol	_sprintf
	.long	cfm_stub_binding_helper

pow_bp:
	.indirect_symbol	_pow
	.long	cfm_stub_binding_helper

fwrite_bp:
	.indirect_symbol	_fwrite
	.long	cfm_stub_binding_helper

memcpy_bp:
	.indirect_symbol	_memcpy
	.long	cfm_stub_binding_helper

rewind_bp:
	.indirect_symbol	_rewind
	.long	cfm_stub_binding_helper

labs_bp:
	.indirect_symbol	_labs
	.long	cfm_stub_binding_helper

log_bp:
	.indirect_symbol	_log
	.long	cfm_stub_binding_helper

setbuf_bp:
	.indirect_symbol	_setbuf
	.long	cfm_stub_binding_helper

isalpha_bp:
	.indirect_symbol	_isalpha
	.long	cfm_stub_binding_helper

strrchr_bp:
	.indirect_symbol	_strrchr
	.long	cfm_stub_binding_helper

	.section	__DATA, __VLib_Data_BPs, non_lazy_symbol_pointers

	.align	2

#else
#endif

