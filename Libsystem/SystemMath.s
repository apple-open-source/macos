/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
	.long	205
	.long	0
	.long	14
	.long	15
	.long	50
	.long	0x70777063
	.long	0x00000000
	.long	0xB8B293D6
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

	.long	0x000717DE
	.long	0x00071021
	.long	0x00060A94
	.long	0x000B82F7
	.long	0x00040253
	.long	0x00060AB4
	.long	0x0006098D
	.long	0x00060909
	.long	0x00030109
	.long	0x0007179E
	.long	0x0003010A
	.long	0x00060A33
	.long	0x00071548
	.long	0x00060B9E
	.long	0x00040233
	.long	0x00030169
	.long	0x000B82F5
	.long	0x00050441
	.long	0x0007173A
	.long	0x00094B1B
	.long	0x000504E4
	.long	0x00040270
	.long	0x00050422
	.long	0x00040232
	.long	0x00071188
	.long	0x000504C2
	.long	0x00060B5E
	.long	0x00060BDA
	.long	0x00050480
	.long	0x00040290
	.long	0x000828C2
	.long	0x000B13B9
	.long	0x000B07B9
	.long	0x00060866
	.long	0x000609EA
	.long	0x00050447
	.long	0x00040257
	.long	0x00071589
	.long	0x000717FA
	.long	0x000A971E
	.long	0x000609AB
	.long	0x00050444
	.long	0x00040254
	.long	0x0006090E
	.long	0x0007137D
	.long	0x00040237
	.long	0x00050616
	.long	0x0008214C
	.long	0x00060864
	.long	0x00060A74
	.long	0x000505C9
	.long	0x00060B1F
	.long	0x000504E0
	.long	0x000504E0
	.long	0x00040274
	.long	0x000A8274
	.long	0x000716DE
	.long	0x000712DE
	.long	0x000712DE
	.long	0x00030121
	.long	0x0004029C
	.long	0x00060A39
	.long	0x000402DF
	.long	0x00071028
	.long	0x00060A5B
	.long	0x000505A4
	.long	0x00082372
	.long	0x0004027A
	.long	0x00040238
	.long	0x000820EE
	.long	0x000608AF
	.long	0x000710CC
	.long	0x00060B96
	.long	0x000504AF
	.long	0x0004021A
	.long	0x000504CD
	.long	0x00082E1B
	.long	0x00071770
	.long	0x0007123A
	.long	0x00060BD5
	.long	0x00040278
	.long	0x0004021C
	.long	0x0004021C
	.long	0x0007106F
	.long	0x000717B1
	.long	0x00060BD2
	.long	0x0005046F
	.long	0x000B0F77
	.long	0x00060B76
	.long	0x00060BF2
	.long	0x0005042C
	.long	0x000ACAFA
	.long	0x00071182
	.long	0x000402BB
	.long	0x00071582
	.long	0x0006082E
	.long	0x0005048B
	.long	0x0006088B
	.long	0x00071736
	.long	0x000ACAF8
	.long	0x0004023E
	.long	0x0004023E
	.long	0x0004021F
	.long	0x000D02F8
	.long	0x0007138C
	.long	0x0005059C
	.long	0x000829DE
	.long	0x00050410
	.long	0x00060AA4
	.long	0x00060AC7
	.long	0x000711BC
	.long	0x00040220
	.long	0x000A90F6
	.long	0x00095328
	.long	0x000504D4
	.long	0x00071012
	.long	0x000711FD
	.long	0x000609BE
	.long	0x00040260
	.long	0x0007172A
	.long	0x000609DD
	.long	0x00040203
	.long	0x000504F4
	.long	0x000A90F4
	.long	0x00060A67
	.long	0x00050477
	.long	0x000C3D3D
	.long	0x00050457
	.long	0x00050476
	.long	0x0007136D
	.long	0x00060B8A
	.long	0x00040227
	.long	0x000505BB
	.long	0x00050492
	.long	0x00082E64
	.long	0x000402A2
	.long	0x000822A2
	.long	0x000717E8
	.long	0x00050436
	.long	0x0003017C
	.long	0x00094E64
	.long	0x000A8874
	.long	0x00071594
	.long	0x00060BA5
	.long	0x00082531
	.long	0x000609D6
	.long	0x000717E7
	.long	0x0006089C
	.long	0x00040229
	.long	0x000609F7
	.long	0x000D3629
	.long	0x000C04BD
	.long	0x000402CF
	.long	0x00060BE6
	.long	0x00060BA4
	.long	0x0008205B
	.long	0x0004026A
	.long	0x000402AC
	.long	0x00060911
	.long	0x00071038
	.long	0x000D5C38
	.long	0x00082C9E
	.long	0x000504BF
	.long	0x000711B7
	.long	0x00030133
	.long	0x000D362B
	.long	0x000402CC
	.long	0x000609B6
	.long	0x0007109F
	.long	0x0008281B
	.long	0x000710FC
	.long	0x00030170
	.long	0x0005043A
	.long	0x0004022A
	.long	0x0004026F
	.long	0x000504DA
	.long	0x00030114
	.long	0x00030114
	.long	0x0008301C
	.long	0x00050498
	.long	0x00040288
	.long	0x0004024E
	.long	0x000402CB
	.long	0x0007145F
	.long	0x00071705
	.long	0x0004026E
	.long	0x0007143C
	.long	0x00030134
	.long	0x0006081D
	.long	0x0004024F
	.long	0x00050534
	.long	0x000B8191
	.long	0x0008226E
	.long	0x00050516
	.long	0x000712AB
	.long	0x0006089A
	.long	0x000504BB
	.long	0x0008301E
	.long	0x00030116
	.long	0x00050516
	.long	0x000955D1
	.long	0x0007145D
	.long	0x00060A4D
	.long	0x0005047C
	.long	0x000B8193

VLib_ExportNames:

	.long	0x66657570, 0x64617465, 0x656E7666, 0x65736574
	.long	0x656E7666, 0x65676574, 0x726F756E, 0x64666573
	.long	0x65746578, 0x63657074, 0x66657465, 0x73746578
	.long	0x63657074, 0x66656765, 0x74656E76, 0x6665636C
	.long	0x65617265, 0x78636570, 0x74666568, 0x6F6C6465
	.long	0x78636570, 0x74666573, 0x6574726F, 0x756E6466
	.long	0x65676574, 0x65786365, 0x70746665, 0x72616973
	.long	0x65657863, 0x65707466, 0x64696D5F, 0x5F697366
	.long	0x696E6974, 0x65665F5F, 0x69736E6F, 0x726D616C
	.long	0x646E6578, 0x74616674, 0x6572646E, 0x616E6663
	.long	0x6F707973, 0x69676E72, 0x656D7175, 0x6F72656D
	.long	0x61696E64, 0x6572726F, 0x756E6466, 0x6C6F6F72
	.long	0x6365696C, 0x67616D6D, 0x616C6F67, 0x31306578
	.long	0x70326C6F, 0x6772696E, 0x74746F6C, 0x6E657874
	.long	0x61667465, 0x72666C64, 0x65787070, 0x6F776879
	.long	0x706F7473, 0x63616C62, 0x666D696E, 0x726F756E
	.long	0x64746F6C, 0x6E656172, 0x6279696E, 0x74666162
	.long	0x73737172, 0x74636F73, 0x686D6F64, 0x66657266
	.long	0x73696E68, 0x61636F73, 0x6874616E, 0x6174616E
	.long	0x6173696E, 0x685F5F69, 0x736E616E, 0x665F5F69
	.long	0x736E616E, 0x646C6F67, 0x626C6F67, 0x31705F5F
	.long	0x696E666D, 0x6F646666, 0x5F5F6670, 0x636C6173
	.long	0x73696679, 0x665F5F66, 0x70636C61, 0x73736966
	.long	0x79646C67, 0x616D6D61, 0x74616E68, 0x6E616E72
	.long	0x696E7461, 0x74616E68, 0x6C6F6732, 0x666D6178
	.long	0x666D6F64, 0x6578706D, 0x31657870, 0x65726663
	.long	0x5F5F7369, 0x676E6269, 0x74665F5F, 0x7369676E
	.long	0x62697464, 0x66726578, 0x705F5F69, 0x736E6F72
	.long	0x6D616C66, 0x636F7361, 0x636F735F, 0x5F697366
	.long	0x696E6974, 0x65647472, 0x756E6361, 0x74616E32
	.long	0x73696E61, 0x73696E67, 0x65747369, 0x736C6F77
	.long	0x65726973, 0x78646967, 0x6974636C, 0x6F736566
	.long	0x646F7065, 0x6E736574, 0x62756663, 0x616C6C6F
	.long	0x63667075, 0x74737374, 0x72746F6C, 0x61626F72
	.long	0x74676574, 0x77776373, 0x746F6D62, 0x73666765
	.long	0x74706F73, 0x69737072, 0x696E746D, 0x616C6C6F
	.long	0x63737472, 0x746F6B76, 0x66707269, 0x6E746666
	.long	0x666C7573, 0x68697363, 0x6E74726C, 0x73747263
	.long	0x6D707374, 0x72706272, 0x6B616273, 0x6C6F6361
	.long	0x6C74696D, 0x65707574, 0x63686172, 0x7374726E
	.long	0x6361746D, 0x62746F77, 0x63737472, 0x746F6466
	.long	0x74656C6C, 0x696F6374, 0x6C707269, 0x6E746673
	.long	0x79737465, 0x6D6C6F6E, 0x676A6D70, 0x71736F72
	.long	0x7472656E, 0x616D6566, 0x7363616E, 0x66697373
	.long	0x70616365, 0x74696D65, 0x64697666, 0x73657470
	.long	0x6F737374, 0x72636872, 0x67657463, 0x73747278
	.long	0x66726D69, 0x73616C6E, 0x756D666F, 0x70656E66
	.long	0x70757463, 0x66777269, 0x74657374, 0x72636F6C
	.long	0x6C746D70, 0x6E616D74, 0x6F6C6F77, 0x65727673
	.long	0x7072696E, 0x74666D65, 0x6D637079, 0x61746578
	.long	0x69747374, 0x726E636D, 0x7061746F, 0x66676D74
	.long	0x696D6563, 0x6C656172, 0x65727267, 0x65746368
	.long	0x61727265, 0x61646578, 0x69746174, 0x6F697261
	.long	0x6E647374, 0x72637370, 0x6E6D6273, 0x746F7763
	.long	0x73737472, 0x746F756C, 0x72656D6F, 0x76657374
	.long	0x72636174, 0x61746F6C, 0x73747266, 0x74696D65
	.long	0x61736374, 0x696D6569, 0x73616C70, 0x68616D65
	.long	0x6D6D6F76, 0x65746D70, 0x66696C65, 0x66726565
	.long	0x66736565, 0x6B726577, 0x696E6477, 0x72697465
	.long	0x69736173, 0x6369696C, 0x64697673, 0x74726572
	.long	0x726F7273, 0x74727370, 0x6E746F75, 0x70706572
	.long	0x7763746F, 0x6D627373, 0x63616E66, 0x756E6C69
	.long	0x6E6B6469, 0x66667469, 0x6D656C61, 0x62736374
	.long	0x696D6573, 0x74726E63, 0x70796D65, 0x6D636D70
	.long	0x66676574, 0x63627365, 0x61726368, 0x66636E74
	.long	0x6C667072, 0x696E7466, 0x66656F66, 0x6F70656E
	.long	0x70757463, 0x69736469, 0x67697466, 0x72656164
	.long	0x636C6F63, 0x6B6D656D, 0x63687269, 0x73677261
	.long	0x70686475, 0x70676574, 0x656E7669, 0x73757070
	.long	0x65727065, 0x72726F72, 0x746F6173, 0x63696975
	.long	0x6E676574, 0x6366636C, 0x6F73656D, 0x656D7365
	.long	0x746D6B74, 0x696D6572, 0x65616C6C, 0x6F637374
	.long	0x72637079, 0x7372616E, 0x64737472, 0x6C656E73
	.long	0x74727374, 0x72676574, 0x7069646D, 0x626C656E
	.long	0x66676574, 0x73736574, 0x6C6F6361, 0x6C656665
	.long	0x72726F72, 0x76707269, 0x6E746669, 0x7370756E
	.long	0x63747075, 0x74736672, 0x656F7065, 0x6E6C6F63
	.long	0x616C6563, 0x6F6E7673, 0x63616E66, 0x73657476
	.long	0x62756673, 0x74727263, 0x68727075, 0x74777370
	.long	0x72696E74
	.long	0x66000000

	.section	__TEXT, __VLib_Exports, symbol_stubs, none, 8

	.align	2

VLib_ExportSymbols:

	.indirect_symbol	_strrchr
	.long	0x020004E3
	.long	strrchr_bp - VLib_Origin

	.indirect_symbol	_isalpha
	.long	0x02000387
	.long	isalpha_bp - VLib_Origin

	.indirect_symbol	_setbuf
	.long	0x02000205
	.long	setbuf_bp - VLib_Origin

	.indirect_symbol	___isfinited
	.long	0x020001CB
	.long	__isfinited_bp - VLib_Origin

	.indirect_symbol	_labs
	.long	0x020003EA
	.long	labs_bp - VLib_Origin

	.indirect_symbol	_rewind
	.long	0x020003A5
	.long	rewind_bp - VLib_Origin

	.indirect_symbol	_memcpy
	.long	0x02000316
	.long	memcpy_bp - VLib_Origin

	.indirect_symbol	_fwrite
	.long	0x020002F4
	.long	fwrite_bp - VLib_Origin

	.indirect_symbol	_log
	.long	0x020000D2
	.long	log_bp - VLib_Origin

	.indirect_symbol	_sprintf
	.long	0x020004EE
	.long	sprintf_bp - VLib_Origin

	.indirect_symbol	_dup
	.long	0x02000442
	.long	dup_bp - VLib_Origin

	.indirect_symbol	_unlink
	.long	0x020003DC
	.long	unlink_bp - VLib_Origin

	.indirect_symbol	_toupper
	.long	0x020003C9
	.long	toupper_bp - VLib_Origin

	.indirect_symbol	_strchr
	.long	0x020002D2
	.long	strchr_bp - VLib_Origin

	.indirect_symbol	_getw
	.long	0x02000221
	.long	getw_bp - VLib_Origin

	.indirect_symbol	_pow
	.long	0x020000EB
	.long	pow_bp - VLib_Origin

	.indirect_symbol	___isfinitef
	.long	0x0200007B
	.long	__isfinitef_bp - VLib_Origin

	.indirect_symbol	_clock
	.long	0x02000430
	.long	clock_bp - VLib_Origin

	.indirect_symbol	_strcspn
	.long	0x02000352
	.long	strcspn_bp - VLib_Origin

	.indirect_symbol	_localtime
	.long	0x0200026C
	.long	localtime_bp - VLib_Origin

	.indirect_symbol	_frexp
	.long	0x020001B4
	.long	frexp_bp - VLib_Origin

	.indirect_symbol	_logb
	.long	0x02000145
	.long	logb_bp - VLib_Origin

	.indirect_symbol	_hypot
	.long	0x020000EE
	.long	hypot_bp - VLib_Origin

	.indirect_symbol	_ceil
	.long	0x020000C0
	.long	ceil_bp - VLib_Origin

	.indirect_symbol	_isupper
	.long	0x0200044B
	.long	isupper_bp - VLib_Origin

	.indirect_symbol	_fread
	.long	0x0200042B
	.long	fread_bp - VLib_Origin

	.indirect_symbol	_printf
	.long	0x02000299
	.long	printf_bp - VLib_Origin

	.indirect_symbol	_strtod
	.long	0x02000289
	.long	strtod_bp - VLib_Origin

	.indirect_symbol	_atanh
	.long	0x02000183
	.long	atanh_bp - VLib_Origin

	.indirect_symbol	_tanh
	.long	0x02000178
	.long	tanh_bp - VLib_Origin

	.indirect_symbol	_roundtol
	.long	0x020000FC
	.long	roundtol_bp - VLib_Origin

	.indirect_symbol	_fegetexcept
	.long	0x0200005F
	.long	fegetexcept_bp - VLib_Origin

	.indirect_symbol	_fesetexcept
	.long	0x0200001D
	.long	fesetexcept_bp - VLib_Origin

	.indirect_symbol	_getpid
	.long	0x02000495
	.long	getpid_bp - VLib_Origin

	.indirect_symbol	_memset
	.long	0x0200046B
	.long	memset_bp - VLib_Origin

	.indirect_symbol	_fgetc
	.long	0x02000400
	.long	fgetc_bp - VLib_Origin

	.indirect_symbol	_free
	.long	0x0200039C
	.long	free_bp - VLib_Origin

	.indirect_symbol	_tmpfile
	.long	0x02000395
	.long	tmpfile_bp - VLib_Origin

	.indirect_symbol	_strtoul
	.long	0x02000361
	.long	strtoul_bp - VLib_Origin

	.indirect_symbol	_localeconv
	.long	0x020004CD
	.long	localeconv_bp - VLib_Origin

	.indirect_symbol	_mktime
	.long	0x02000471
	.long	mktime_bp - VLib_Origin

	.indirect_symbol	_fcntl
	.long	0x0200040C
	.long	fcntl_bp - VLib_Origin

	.indirect_symbol	_ldiv
	.long	0x020003B7
	.long	ldiv_bp - VLib_Origin

	.indirect_symbol	_atexit
	.long	0x0200031C
	.long	atexit_bp - VLib_Origin

	.indirect_symbol	_fsetpos
	.long	0x020002CB
	.long	fsetpos_bp - VLib_Origin

	.indirect_symbol	_gets
	.long	0x020001E7
	.long	gets_bp - VLib_Origin

	.indirect_symbol	___inf
	.long	0x0200014E
	.long	__inf_bp - VLib_Origin

	.indirect_symbol	_copysign
	.long	0x0200009F
	.long	copysign_bp - VLib_Origin

	.indirect_symbol	_ferror
	.long	0x020004AE
	.long	ferror_bp - VLib_Origin

	.indirect_symbol	_wctomb
	.long	0x020003D0
	.long	wctomb_bp - VLib_Origin

	.indirect_symbol	_write
	.long	0x020003AB
	.long	write_bp - VLib_Origin

	.indirect_symbol	_system
	.long	0x0200029F
	.long	system_bp - VLib_Origin

	.indirect_symbol	_ioctl
	.long	0x02000294
	.long	ioctl_bp - VLib_Origin

	.indirect_symbol	_ftell
	.long	0x0200028F
	.long	ftell_bp - VLib_Origin

	.indirect_symbol	_atan
	.long	0x0200012C
	.long	atan_bp - VLib_Origin

	.indirect_symbol	_fesetround
	.long	0x02000055
	.long	fesetround_bp - VLib_Origin

	.indirect_symbol	_vprintf
	.long	0x020004B4
	.long	vprintf_bp - VLib_Origin

	.indirect_symbol	_fprintf
	.long	0x02000411
	.long	fprintf_bp - VLib_Origin

	.indirect_symbol	_bsearch
	.long	0x02000405
	.long	bsearch_bp - VLib_Origin

	.indirect_symbol	_cos
	.long	0x020001C4
	.long	cos_bp - VLib_Origin

	.indirect_symbol	_rint
	.long	0x0200017F
	.long	rint_bp - VLib_Origin

	.indirect_symbol	_remquo
	.long	0x020000A7
	.long	remquo_bp - VLib_Origin

	.indirect_symbol	_putc
	.long	0x02000420
	.long	putc_bp - VLib_Origin

	.indirect_symbol	_getchar
	.long	0x0200033B
	.long	getchar_bp - VLib_Origin

	.indirect_symbol	_rename
	.long	0x020002B1
	.long	rename_bp - VLib_Origin

	.indirect_symbol	_qsort
	.long	0x020002AC
	.long	qsort_bp - VLib_Origin

	.indirect_symbol	_isxdigit
	.long	0x020001F2
	.long	isxdigit_bp - VLib_Origin

	.indirect_symbol	_modf
	.long	0x02000119
	.long	modf_bp - VLib_Origin

	.indirect_symbol	_fmin
	.long	0x020000F8
	.long	fmin_bp - VLib_Origin

	.indirect_symbol	_fesetenv
	.long	0x0200000B
	.long	fesetenv_bp - VLib_Origin

	.indirect_symbol	_fclose
	.long	0x02000465
	.long	fclose_bp - VLib_Origin

	.indirect_symbol	_isgraph
	.long	0x0200043B
	.long	isgraph_bp - VLib_Origin

	.indirect_symbol	_strcmp
	.long	0x0200025C
	.long	strcmp_bp - VLib_Origin

	.indirect_symbol	_fputs
	.long	0x02000211
	.long	fputs_bp - VLib_Origin

	.indirect_symbol	_exp2
	.long	0x020000CE
	.long	exp2_bp - VLib_Origin

	.indirect_symbol	_fseek
	.long	0x020003A0
	.long	fseek_bp - VLib_Origin

	.indirect_symbol	_strftime
	.long	0x02000378
	.long	strftime_bp - VLib_Origin

	.indirect_symbol	_strcoll
	.long	0x020002FA
	.long	strcoll_bp - VLib_Origin

	.indirect_symbol	_longjmp
	.long	0x020002A5
	.long	longjmp_bp - VLib_Origin

	.indirect_symbol	_strtok
	.long	0x02000241
	.long	strtok_bp - VLib_Origin

	.indirect_symbol	_asin
	.long	0x020001E3
	.long	asin_bp - VLib_Origin

	.indirect_symbol	_open
	.long	0x0200041C
	.long	open_bp - VLib_Origin

	.indirect_symbol	_feof
	.long	0x02000418
	.long	feof_bp - VLib_Origin

	.indirect_symbol	_isalnum
	.long	0x020002E3
	.long	isalnum_bp - VLib_Origin

	.indirect_symbol	_strxfrm
	.long	0x020002DC
	.long	strxfrm_bp - VLib_Origin

	.indirect_symbol	_strtol
	.long	0x02000216
	.long	strtol_bp - VLib_Origin

	.indirect_symbol	_close
	.long	0x020001FA
	.long	close_bp - VLib_Origin

	.indirect_symbol	_feupdateenv
	.long	0x02000000
	.long	feupdateenv_bp - VLib_Origin

	.indirect_symbol	_sscanf
	.long	0x020003D6
	.long	sscanf_bp - VLib_Origin

	.indirect_symbol	_strspn
	.long	0x020003C3
	.long	strspn_bp - VLib_Origin

	.indirect_symbol	_abort
	.long	0x0200021C
	.long	abort_bp - VLib_Origin

	.indirect_symbol	___signbitf
	.long	0x020001A0
	.long	__signbitf_bp - VLib_Origin

	.indirect_symbol	_ispunct
	.long	0x020004BB
	.long	ispunct_bp - VLib_Origin

	.indirect_symbol	_time
	.long	0x020002C4
	.long	time_bp - VLib_Origin

	.indirect_symbol	_rinttol
	.long	0x020000D5
	.long	rinttol_bp - VLib_Origin

	.indirect_symbol	_getenv
	.long	0x02000445
	.long	getenv_bp - VLib_Origin

	.indirect_symbol	_ctime
	.long	0x020003EE
	.long	ctime_bp - VLib_Origin

	.indirect_symbol	_gmtime
	.long	0x0200032D
	.long	gmtime_bp - VLib_Origin

	.indirect_symbol	_strncmp
	.long	0x02000322
	.long	strncmp_bp - VLib_Origin

	.indirect_symbol	___signbitd
	.long	0x020001AA
	.long	__signbitd_bp - VLib_Origin

	.indirect_symbol	_fmod
	.long	0x02000190
	.long	fmod_bp - VLib_Origin

	.indirect_symbol	_fmax
	.long	0x0200018C
	.long	fmax_bp - VLib_Origin

	.indirect_symbol	_fdim
	.long	0x02000077
	.long	fdim_bp - VLib_Origin

	.indirect_symbol	_feraiseexcept
	.long	0x0200006A
	.long	feraiseexcept_bp - VLib_Origin

	.indirect_symbol	_freopen
	.long	0x020004C6
	.long	freopen_bp - VLib_Origin

	.indirect_symbol	_srand
	.long	0x02000484
	.long	srand_bp - VLib_Origin

	.indirect_symbol	_vfprintf
	.long	0x02000247
	.long	vfprintf_bp - VLib_Origin

	.indirect_symbol	_floor
	.long	0x020000BB
	.long	floor_bp - VLib_Origin

	.indirect_symbol	_perror
	.long	0x02000452
	.long	perror_bp - VLib_Origin

	.indirect_symbol	_tmpnam
	.long	0x02000301
	.long	tmpnam_bp - VLib_Origin

	.indirect_symbol	_isprint
	.long	0x02000234
	.long	isprint_bp - VLib_Origin

	.indirect_symbol	_log2
	.long	0x02000188
	.long	log2_bp - VLib_Origin

	.indirect_symbol	_nextafterf
	.long	0x020000DC
	.long	nextafterf_bp - VLib_Origin

	.indirect_symbol	_remainder
	.long	0x020000AD
	.long	remainder_bp - VLib_Origin

	.indirect_symbol	_mblen
	.long	0x0200049B
	.long	mblen_bp - VLib_Origin

	.indirect_symbol	_isdigit
	.long	0x02000424
	.long	isdigit_bp - VLib_Origin

	.indirect_symbol	_fgetpos
	.long	0x0200022D
	.long	fgetpos_bp - VLib_Origin

	.indirect_symbol	_memcmp
	.long	0x020003FA
	.long	memcmp_bp - VLib_Origin

	.indirect_symbol	_atof
	.long	0x02000329
	.long	atof_bp - VLib_Origin

	.indirect_symbol	_strncat
	.long	0x0200027C
	.long	strncat_bp - VLib_Origin

	.indirect_symbol	_malloc
	.long	0x0200023B
	.long	malloc_bp - VLib_Origin

	.indirect_symbol	_fabs
	.long	0x0200010D
	.long	fabs_bp - VLib_Origin

	.indirect_symbol	_ldexp
	.long	0x020000E6
	.long	ldexp_bp - VLib_Origin

	.indirect_symbol	_nextafterd
	.long	0x02000091
	.long	nextafterd_bp - VLib_Origin

	.indirect_symbol	_ungetc
	.long	0x0200045F
	.long	ungetc_bp - VLib_Origin

	.indirect_symbol	_gamma
	.long	0x020000C4
	.long	gamma_bp - VLib_Origin

	.indirect_symbol	_feholdexcept
	.long	0x02000049
	.long	feholdexcept_bp - VLib_Origin

	.indirect_symbol	_fgets
	.long	0x020004A0
	.long	fgets_bp - VLib_Origin

	.indirect_symbol	_log10
	.long	0x020000C9
	.long	log10_bp - VLib_Origin

	.indirect_symbol	_memmove
	.long	0x0200038E
	.long	memmove_bp - VLib_Origin

	.indirect_symbol	_strcat
	.long	0x0200036E
	.long	strcat_bp - VLib_Origin

	.indirect_symbol	_getc
	.long	0x020002D8
	.long	getc_bp - VLib_Origin

	.indirect_symbol	_trunc
	.long	0x020001D6
	.long	trunc_bp - VLib_Origin

	.indirect_symbol	_modff
	.long	0x02000153
	.long	modff_bp - VLib_Origin

	.indirect_symbol	_strerror
	.long	0x020003BB
	.long	strerror_bp - VLib_Origin

	.indirect_symbol	_read
	.long	0x02000342
	.long	read_bp - VLib_Origin

	.indirect_symbol	_clearerr
	.long	0x02000333
	.long	clearerr_bp - VLib_Origin

	.indirect_symbol	_putchar
	.long	0x02000275
	.long	putchar_bp - VLib_Origin

	.indirect_symbol	_log1p
	.long	0x02000149
	.long	log1p_bp - VLib_Origin

	.indirect_symbol	_tan
	.long	0x02000129
	.long	tan_bp - VLib_Origin

	.indirect_symbol	_nearbyint
	.long	0x02000104
	.long	nearbyint_bp - VLib_Origin

	.indirect_symbol	_fegetround
	.long	0x02000013
	.long	fegetround_bp - VLib_Origin

	.indirect_symbol	_setvbuf
	.long	0x020004DC
	.long	setvbuf_bp - VLib_Origin

	.indirect_symbol	_strcpy
	.long	0x0200047E
	.long	strcpy_bp - VLib_Origin

	.indirect_symbol	_mbstowcs
	.long	0x02000359
	.long	mbstowcs_bp - VLib_Origin

	.indirect_symbol	_fscanf
	.long	0x020002B7
	.long	fscanf_bp - VLib_Origin

	.indirect_symbol	_strpbrk
	.long	0x02000262
	.long	strpbrk_bp - VLib_Origin

	.indirect_symbol	_fdopen
	.long	0x020001FF
	.long	fdopen_bp - VLib_Origin

	.indirect_symbol	_acos
	.long	0x020001C7
	.long	acos_bp - VLib_Origin

	.indirect_symbol	_lgamma
	.long	0x02000172
	.long	lgamma_bp - VLib_Origin

	.indirect_symbol	___fpclassifyf
	.long	0x02000158
	.long	__fpclassifyf_bp - VLib_Origin

	.indirect_symbol	_fetestexcept
	.long	0x02000028
	.long	fetestexcept_bp - VLib_Origin

	.indirect_symbol	_puts
	.long	0x020004C2
	.long	puts_bp - VLib_Origin

	.indirect_symbol	_strstr
	.long	0x0200048F
	.long	strstr_bp - VLib_Origin

	.indirect_symbol	_strlen
	.long	0x02000489
	.long	strlen_bp - VLib_Origin

	.indirect_symbol	_difftime
	.long	0x020003E2
	.long	difftime_bp - VLib_Origin

	.indirect_symbol	_atol
	.long	0x02000374
	.long	atol_bp - VLib_Origin

	.indirect_symbol	_rand
	.long	0x0200034E
	.long	rand_bp - VLib_Origin

	.indirect_symbol	_mbtowc
	.long	0x02000283
	.long	mbtowc_bp - VLib_Origin

	.indirect_symbol	_iscntrl
	.long	0x02000255
	.long	iscntrl_bp - VLib_Origin

	.indirect_symbol	_feclearexcept
	.long	0x0200003C
	.long	feclearexcept_bp - VLib_Origin

	.indirect_symbol	_vsprintf
	.long	0x0200030E
	.long	vsprintf_bp - VLib_Origin

	.indirect_symbol	_fputc
	.long	0x020002EF
	.long	fputc_bp - VLib_Origin

	.indirect_symbol	_isspace
	.long	0x020002BD
	.long	isspace_bp - VLib_Origin

	.indirect_symbol	_abs
	.long	0x02000269
	.long	abs_bp - VLib_Origin

	.indirect_symbol	___fpclassifyd
	.long	0x02000165
	.long	__fpclassifyd_bp - VLib_Origin

	.indirect_symbol	_sqrt
	.long	0x02000111
	.long	sqrt_bp - VLib_Origin

	.indirect_symbol	_memchr
	.long	0x02000435
	.long	memchr_bp - VLib_Origin

	.indirect_symbol	_isascii
	.long	0x020003B0
	.long	isascii_bp - VLib_Origin

	.indirect_symbol	_wcstombs
	.long	0x02000225
	.long	wcstombs_bp - VLib_Origin

	.indirect_symbol	_islower
	.long	0x020001EB
	.long	islower_bp - VLib_Origin

	.indirect_symbol	_sin
	.long	0x020001E0
	.long	sin_bp - VLib_Origin

	.indirect_symbol	_acosh
	.long	0x02000124
	.long	acosh_bp - VLib_Origin

	.indirect_symbol	_cosh
	.long	0x02000115
	.long	cosh_bp - VLib_Origin

	.indirect_symbol	_atoi
	.long	0x0200034A
	.long	atoi_bp - VLib_Origin

	.indirect_symbol	_atan2
	.long	0x020001DB
	.long	atan2_bp - VLib_Origin

	.indirect_symbol	_exp
	.long	0x02000199
	.long	exp_bp - VLib_Origin

	.indirect_symbol	_nan
	.long	0x0200017C
	.long	nan_bp - VLib_Origin

	.indirect_symbol	___isnand
	.long	0x0200013D
	.long	__isnand_bp - VLib_Origin

	.indirect_symbol	_asinh
	.long	0x02000130
	.long	asinh_bp - VLib_Origin

	.indirect_symbol	_sinh
	.long	0x02000120
	.long	sinh_bp - VLib_Origin

	.indirect_symbol	_nanf
	.long	0x0200009B
	.long	nanf_bp - VLib_Origin

	.indirect_symbol	_putw
	.long	0x020004EA
	.long	putw_bp - VLib_Origin

	.indirect_symbol	_toascii
	.long	0x02000458
	.long	toascii_bp - VLib_Origin

	.indirect_symbol	_strncpy
	.long	0x020003F3
	.long	strncpy_bp - VLib_Origin

	.indirect_symbol	_exit
	.long	0x02000346
	.long	exit_bp - VLib_Origin

	.indirect_symbol	_tolower
	.long	0x02000307
	.long	tolower_bp - VLib_Origin

	.indirect_symbol	_div
	.long	0x020002C8
	.long	div_bp - VLib_Origin

	.indirect_symbol	_calloc
	.long	0x0200020B
	.long	calloc_bp - VLib_Origin

	.indirect_symbol	_erfc
	.long	0x0200019C
	.long	erfc_bp - VLib_Origin

	.indirect_symbol	_round
	.long	0x020000B6
	.long	round_bp - VLib_Origin

	.indirect_symbol	___isnormald
	.long	0x02000086
	.long	__isnormald_bp - VLib_Origin

	.indirect_symbol	_fegetenv
	.long	0x02000034
	.long	fegetenv_bp - VLib_Origin

	.indirect_symbol	_scanf
	.long	0x020004D7
	.long	scanf_bp - VLib_Origin

	.indirect_symbol	_asctime
	.long	0x02000380
	.long	asctime_bp - VLib_Origin

	.indirect_symbol	_fflush
	.long	0x0200024F
	.long	fflush_bp - VLib_Origin

	.indirect_symbol	_expm1
	.long	0x02000194
	.long	expm1_bp - VLib_Origin

	.indirect_symbol	___isnanf
	.long	0x02000135
	.long	__isnanf_bp - VLib_Origin

	.indirect_symbol	_erf
	.long	0x0200011D
	.long	erf_bp - VLib_Origin

	.indirect_symbol	_scalb
	.long	0x020000F3
	.long	scalb_bp - VLib_Origin

	.indirect_symbol	_setlocale
	.long	0x020004A5
	.long	setlocale_bp - VLib_Origin

	.indirect_symbol	_realloc
	.long	0x02000477
	.long	realloc_bp - VLib_Origin

	.indirect_symbol	_remove
	.long	0x02000368
	.long	remove_bp - VLib_Origin

	.indirect_symbol	_fopen
	.long	0x020002EA
	.long	fopen_bp - VLib_Origin

	.indirect_symbol	___isnormalf
	.long	0x020001B9
	.long	__isnormalf_bp - VLib_Origin


	.globl	cfm_stub_binding_helper

	.section	__DATA, __VLib_Func_BPs, lazy_symbol_pointers

	.align	2

feupdateenv_bp:
	.indirect_symbol	_feupdateenv
	.long	cfm_stub_binding_helper

fesetenv_bp:
	.indirect_symbol	_fesetenv
	.long	cfm_stub_binding_helper

fegetround_bp:
	.indirect_symbol	_fegetround
	.long	cfm_stub_binding_helper

fesetexcept_bp:
	.indirect_symbol	_fesetexcept
	.long	cfm_stub_binding_helper

fetestexcept_bp:
	.indirect_symbol	_fetestexcept
	.long	cfm_stub_binding_helper

fegetenv_bp:
	.indirect_symbol	_fegetenv
	.long	cfm_stub_binding_helper

feclearexcept_bp:
	.indirect_symbol	_feclearexcept
	.long	cfm_stub_binding_helper

feholdexcept_bp:
	.indirect_symbol	_feholdexcept
	.long	cfm_stub_binding_helper

fesetround_bp:
	.indirect_symbol	_fesetround
	.long	cfm_stub_binding_helper

fegetexcept_bp:
	.indirect_symbol	_fegetexcept
	.long	cfm_stub_binding_helper

feraiseexcept_bp:
	.indirect_symbol	_feraiseexcept
	.long	cfm_stub_binding_helper

fdim_bp:
	.indirect_symbol	_fdim
	.long	cfm_stub_binding_helper

__isfinitef_bp:
	.indirect_symbol	___isfinitef
	.long	cfm_stub_binding_helper

__isnormald_bp:
	.indirect_symbol	___isnormald
	.long	cfm_stub_binding_helper

nextafterd_bp:
	.indirect_symbol	_nextafterd
	.long	cfm_stub_binding_helper

nanf_bp:
	.indirect_symbol	_nanf
	.long	cfm_stub_binding_helper

copysign_bp:
	.indirect_symbol	_copysign
	.long	cfm_stub_binding_helper

remquo_bp:
	.indirect_symbol	_remquo
	.long	cfm_stub_binding_helper

remainder_bp:
	.indirect_symbol	_remainder
	.long	cfm_stub_binding_helper

round_bp:
	.indirect_symbol	_round
	.long	cfm_stub_binding_helper

floor_bp:
	.indirect_symbol	_floor
	.long	cfm_stub_binding_helper

ceil_bp:
	.indirect_symbol	_ceil
	.long	cfm_stub_binding_helper

gamma_bp:
	.indirect_symbol	_gamma
	.long	cfm_stub_binding_helper

log10_bp:
	.indirect_symbol	_log10
	.long	cfm_stub_binding_helper

exp2_bp:
	.indirect_symbol	_exp2
	.long	cfm_stub_binding_helper

log_bp:
	.indirect_symbol	_log
	.long	cfm_stub_binding_helper

rinttol_bp:
	.indirect_symbol	_rinttol
	.long	cfm_stub_binding_helper

nextafterf_bp:
	.indirect_symbol	_nextafterf
	.long	cfm_stub_binding_helper

ldexp_bp:
	.indirect_symbol	_ldexp
	.long	cfm_stub_binding_helper

pow_bp:
	.indirect_symbol	_pow
	.long	cfm_stub_binding_helper

hypot_bp:
	.indirect_symbol	_hypot
	.long	cfm_stub_binding_helper

scalb_bp:
	.indirect_symbol	_scalb
	.long	cfm_stub_binding_helper

fmin_bp:
	.indirect_symbol	_fmin
	.long	cfm_stub_binding_helper

roundtol_bp:
	.indirect_symbol	_roundtol
	.long	cfm_stub_binding_helper

nearbyint_bp:
	.indirect_symbol	_nearbyint
	.long	cfm_stub_binding_helper

fabs_bp:
	.indirect_symbol	_fabs
	.long	cfm_stub_binding_helper

sqrt_bp:
	.indirect_symbol	_sqrt
	.long	cfm_stub_binding_helper

cosh_bp:
	.indirect_symbol	_cosh
	.long	cfm_stub_binding_helper

modf_bp:
	.indirect_symbol	_modf
	.long	cfm_stub_binding_helper

erf_bp:
	.indirect_symbol	_erf
	.long	cfm_stub_binding_helper

sinh_bp:
	.indirect_symbol	_sinh
	.long	cfm_stub_binding_helper

acosh_bp:
	.indirect_symbol	_acosh
	.long	cfm_stub_binding_helper

tan_bp:
	.indirect_symbol	_tan
	.long	cfm_stub_binding_helper

atan_bp:
	.indirect_symbol	_atan
	.long	cfm_stub_binding_helper

asinh_bp:
	.indirect_symbol	_asinh
	.long	cfm_stub_binding_helper

__isnanf_bp:
	.indirect_symbol	___isnanf
	.long	cfm_stub_binding_helper

__isnand_bp:
	.indirect_symbol	___isnand
	.long	cfm_stub_binding_helper

logb_bp:
	.indirect_symbol	_logb
	.long	cfm_stub_binding_helper

log1p_bp:
	.indirect_symbol	_log1p
	.long	cfm_stub_binding_helper

__inf_bp:
	.indirect_symbol	___inf
	.long	cfm_stub_binding_helper

modff_bp:
	.indirect_symbol	_modff
	.long	cfm_stub_binding_helper

__fpclassifyf_bp:
	.indirect_symbol	___fpclassifyf
	.long	cfm_stub_binding_helper

__fpclassifyd_bp:
	.indirect_symbol	___fpclassifyd
	.long	cfm_stub_binding_helper

lgamma_bp:
	.indirect_symbol	_lgamma
	.long	cfm_stub_binding_helper

tanh_bp:
	.indirect_symbol	_tanh
	.long	cfm_stub_binding_helper

nan_bp:
	.indirect_symbol	_nan
	.long	cfm_stub_binding_helper

rint_bp:
	.indirect_symbol	_rint
	.long	cfm_stub_binding_helper

atanh_bp:
	.indirect_symbol	_atanh
	.long	cfm_stub_binding_helper

log2_bp:
	.indirect_symbol	_log2
	.long	cfm_stub_binding_helper

fmax_bp:
	.indirect_symbol	_fmax
	.long	cfm_stub_binding_helper

fmod_bp:
	.indirect_symbol	_fmod
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

__signbitf_bp:
	.indirect_symbol	___signbitf
	.long	cfm_stub_binding_helper

__signbitd_bp:
	.indirect_symbol	___signbitd
	.long	cfm_stub_binding_helper

frexp_bp:
	.indirect_symbol	_frexp
	.long	cfm_stub_binding_helper

__isnormalf_bp:
	.indirect_symbol	___isnormalf
	.long	cfm_stub_binding_helper

cos_bp:
	.indirect_symbol	_cos
	.long	cfm_stub_binding_helper

acos_bp:
	.indirect_symbol	_acos
	.long	cfm_stub_binding_helper

__isfinited_bp:
	.indirect_symbol	___isfinited
	.long	cfm_stub_binding_helper

trunc_bp:
	.indirect_symbol	_trunc
	.long	cfm_stub_binding_helper

atan2_bp:
	.indirect_symbol	_atan2
	.long	cfm_stub_binding_helper

sin_bp:
	.indirect_symbol	_sin
	.long	cfm_stub_binding_helper

asin_bp:
	.indirect_symbol	_asin
	.long	cfm_stub_binding_helper

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

	.section	__DATA, __VLib_Data_BPs, non_lazy_symbol_pointers

	.align	2

#else
#endif

