# $XConsortium: carddata.tcl /main/8 1996/10/28 05:42:15 kaleb $
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/carddata.tcl,v 3.20 1998/03/20 21:05:21 hohndel Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

#
#  Data used by the card configuration routines
#


if !$pc98 {
    set ServerList		[list Mono VGA16 SVGA 8514 AGX I128 \
			          Mach8 Mach32 Mach64 P9000 S3 S3V TGA ]
    set AccelServerList	[list 8514 AGX I128 Mach8 Mach32 Mach64 P9000 \
			          S3 S3V TGA ]
} else {
    set ServerList		[list EGC PEGC GANBWAP NKVNEC TGUI MGA \
			          WABS WABEP WSNA NECS3 PWSKB PWLB GA968 ]
    set AccelServerList [list EGC PEGC GANBWAP NKVNEC TGUI MGA \
			          WABS WABEP WSNA NECS3 PWSKB PWLB GA968 ]
}

###

# For each server, what chipsets can be chosen (for the Mono, VGA16,
# and SVGA servers, the list is broken out by driver)?
set CardChipSets(SVGA-al2101)	al2101
set CardChipSets(SVGA-ali)	{ ali2228 ali2301 ali2302 ali2308 ali2401 }
set CardChipSets(SVGA-apm)	ap6422
set CardChipSets(SVGA-ark)	{ ark1000vl ark1000pv ark2000pv }
set CardChipSets(SVGA-ati)	ati
set CardChipSets(SVGA-cl64xx)	{ cl6410 cl6412 cl6420 cl6440 }
set CardChipSets(SVGA-cirrus)	{ clgd5420 clgd5422 clgd5424 clgd5426 \
				  clgd5428 clgd5429 clgd5430 clgd5434 \
				  clgd5436 clgd5446 clgd6215 clgd6225 \
				  clgd6235 clgd7541 clgd7542 clgd7543 }
#set CardChipSets(SVGA-compaq)	cpq_avga
set CardChipSets(SVGA-chips)	{ ct65520 ct65530 ct65540 ct65545 \
				  ct65546 ct65548 ct65550 ct65554 \
				  ct65555 ct68554 ct69000 \
				  ct64200 ct64300 \
				  ct451 ct452 ct453 ct455 ct456 ct457 }
set CardChipSets(SVGA-et3000)	et3000
set CardChipSets(SVGA-et4000)	{ et4000 et4000w32 et4000w32i et4000w32i_rev_b \
				  et4000w32i_rev_c et4000w32p et4000w32p_rev_a \
				  et4000w32p_rev_b et4000w32p_rev_c \
				  et4000w32p_rev_d, et6000 }
set CardChipSets(SVGA-gvga)	gvga
set CardChipSets(SVGA-mga)	mga2064w
set CardChipSets(SVGA-mx)	mx
set CardChipSets(SVGA-ncr77c22)	{ ncr77c22 ncr77c22e }
set CardChipSets(SVGA-nv)	{ nv1 stg2000 riva128 }
set CardChipSets(SVGA-oak)	{ oti067 oti077 oti087 oti037c }
set CardChipSets(SVGA-pvga1)	{ pvga1 \
				  wd90c00 wd90c10 wd90c30 wd90c24 \
				  wd90c31 wd90c31 wd90c33 wd90c20 }
set CardChipSets(SVGA-realtek)	realtek
#set CardChipSets(SVGA-s3_svga)	s3
set CardChipSets(SVGA-sis)	{ sis86c201 sis86c202 sis86c205 }
set CardChipSets(SVGA-tvga8900)	{ tvga8200lx tvga8800cs tvga8900b tvga8900c \
				  tvga8900cl tvga8900d tvga9000 tvga9000i \
				  tvga9100b tvga9200cxr \
				  tgui9320lcd tgui9400cxi tgui9420 \
				  tgui9420dgi tgui9430dgi tgui9440agi \
				  tgui9660xgi tgui9680 cyber938x }
set CardChipSets(SVGA-video7)	video7
set chiplist ""
foreach idx [array names CardChipSets SVGA-*] {
	eval lappend chiplist $CardChipSets($idx)
}
set CardChipSets(SVGA)	   [concat generic [lrmdups $chiplist]]

set CardChipSets(VGA16-ati)	 $CardChipSets(SVGA-ati)
set CardChipSets(VGA16-cl64xx)	 $CardChipSets(SVGA-cl64xx)
set CardChipSets(VGA16-et3000)	 $CardChipSets(SVGA-et3000)
set CardChipSets(VGA16-et4000)	 $CardChipSets(SVGA-et4000)
set CardChipSets(VGA16-ncr77c22) $CardChipSets(SVGA-ncr77c22)
set CardChipSets(VGA16-oak)	 $CardChipSets(SVGA-oak)
set CardChipSets(VGA16-sis)	 $CardChipSets(SVGA-sis)
set CardChipSets(VGA16-tvga8900) $CardChipSets(SVGA-tvga8900)
set chiplist ""
foreach idx [array names CardChipSets VGA16-*] {
	eval lappend chiplist $CardChipSets($idx)
}
set CardChipSets(VGA16)	   [concat generic [lrmdups $chiplist]]

set CardChipSets(Mono-ati)	$CardChipSets(SVGA-ati)
set CardChipSets(Mono-cl64xx)	$CardChipSets(SVGA-cl64xx)
set CardChipSets(Mono-cirrus)	$CardChipSets(SVGA-cirrus)
set CardChipSets(Mono-et3000)	$CardChipSets(SVGA-et3000)
set CardChipSets(Mono-et4000)	$CardChipSets(SVGA-et4000)
set CardChipSets(Mono-gvga)	$CardChipSets(SVGA-gvga)
set CardChipSets(Mono-ncr77c22)	$CardChipSets(SVGA-ncr77c22)
set CardChipSets(Mono-oak)	$CardChipSets(SVGA-oak)
set CardChipSets(Mono-pvga1)	$CardChipSets(SVGA-pvga1)
set CardChipSets(Mono-sis)	$CardChipSets(SVGA-sis)
set CardChipSets(Mono-tvga8900)	$CardChipSets(SVGA-tvga8900)
set chiplist ""
foreach idx [array names CardChipSets Mono-*] {
	eval lappend chiplist $CardChipSets($idx)
}
set CardChipSets(Mono)	   [concat generic [lrmdups $chiplist]]
unset chiplist idx

set CardChipSets(8514)	   { ibm8514 }
set CardChipSets(AGX)	   { agx-010 agx-014 agx-015 agx-016 xga-1 xga-2 }
set CardChipSets(I128)	   { i128 }
set CardChipSets(Mach8)	   { mach8 }
set CardChipSets(Mach32)   { mach32 }
set CardChipSets(Mach64)   { mach64 }
set CardChipSets(P9000)	   { orchid_p9000 viperpci vipervlb }
set CardChipSets(S3)	   { mmio_928 newmmio s3_generic }
set CardChipSets(S3V)	   { s3_virge }
set CardChipSets(TGA)	   { tga }
set CardChipSets(EGC)	   { vga }
set CardChipSets(PEGC)	   { pegc }
set CardChipSets(GANBWAP)  { clgd5426 clgd5428 clgd5429 clgd5430 \
			     clgd5434 clgd5440 clgd5446 clgd7543 \
			     clgd7548 clgd7555 }
set CardChipSets(NKVNEC)   { clgd5426 clgd5428 clgd5429 clgd5430 \
			     clgd5434 clgd5440 clgd5446 clgd7543 \
			     clgd7548 clgd7555 }
set CardChipSets(WABS)	   { clgd5426 clgd5428 clgd5429 clgd5430 \
			     clgd5434 clgd5440 clgd5446 clgd7543 \
			     clgd7548 clgd7555 }
set CardChipSets(WABEP)	   { clgd5426 clgd5428 clgd5429 clgd5430 \
			     clgd5434 clgd5440 clgd5446 clgd7543 \
			     clgd7548 clgd7555 }
set CardChipSets(WSNA)	   { clgd5426 clgd5428 clgd5429 clgd5430 \
			     clgd5434 clgd5440 clgd5446 clgd7543 \
			     clgd7548 clgd7555 }
set CardChipSets(TGUI)	   { tgui9660xgi tgui9680 cyber938x }
set CardChipSets(MGA)	   { }
set CardChipSets(NECS3)	   { s3_generic mmio_928 }
set CardChipSets(PWSKB)	   { s3_generic mmio_928 }
set CardChipSets(PWLB)	   { mmio_928 s3_generic }
set CardChipSets(GA968)	   { newmmio mmio_928 s3_generic }

###

# For each server, what ramdacs can be chosen?
set CardRamDacs(8514)	   {}
set CardRamDacs(AGX)	   { normal att20c490 bt481 bt482 \
			     herc_dual_dac herc_small_dac \
			     sc15025 xga }
set CardRamDacs(I128)	   { ibm526 ibm528 ti3025 }
set CardRamDacs(Mach8)	   {}
set CardRamDacs(Mach32)	   { ati68830 ati68860 ati68875 \
			     att20c490 att20c491 att21c498 \
			     bt476 bt478 bt481 bt482 bt885 \
			     ims_g173 ims_g174 \
			     inmos176 inmos178 \
			     mu9c1880 mu9c4870 mu9c4910 \
			     sc11483 sc11486 sc11488 \
			     	sc15021 sc15025 sc15026 \
			     stg1700 stg1702 \
			     tlc34075 }
set CardRamDacs(Mach64)	   { internal \
			     ati68860 ati68860b ati68860c ati68875 \
			     att20c408 att20c491 att20c498 att21c498 \
			     	att498 \
			     bt476 bt478 bt481 \
			     ch8398 \
			     ibm_rgb514 \
			     ims_g174 \
			     inmos176 inmos178 \
			     mu9c1880 \
			     sc15021 sc15026 \
			     stg1700 stg1702 stg1703 \
			     tlc34075 \
			     tvp3026 \
			   }
set CardRamDacs(P9000)	   {}
set CardRamDacs(S3)	   { normal \
			     att20c409 att20c490 att20c491 att20c498 \
				att20c505 att21c498 att22c498 \
			     bt485 bt9485 \
			     ch8391 \
			     ibm_rgb514 ibm_rgb524 ibm_rgb525 \
				ibm_rgb526 ibm_rgb528 \
			     ics5300 ics5342 \
			     s3gendac s3_sdac \
				s3_trio s3_trio32 s3_trio64 \
			     sc11482 sc11483 sc11484 sc11485 \
				sc11487 sc11489 sc15025 \
			     stg1700 stg1703 \
			     ti3020 ti3025 ti3026 ti3030 \
			   }
set CardRamDacs(S3V)	   {} ;# { normal s3_trio64 }
set CardRamDacs(TGA)	   { bt485 }

set CardRamDacs(SVGA-ark)	   { ark1491a att20c490 att20c498 \
					ics5342 stg1700 \
					w30c491 w30c498 w30c516 \
					zoomdac }
set CardRamDacs(SVGA-ati)	   [lrmdups [concat \
					$CardRamDacs(Mach8) \
					$CardRamDacs(Mach32) \
					$CardRamDacs(Mach64)] ]
set CardRamDacs(SVGA-et4000)	   { normal \
			     att20c47xa att20c490 att20c491 \
			     att20c492 att20c493 att20c497 \
			     ics5341 sc1502x stg1700 stg1702 \
			     stg1703 ch8398 ics5301 et6000 }
set CardRamDacs(SVGA-mga)	   ti3026
set daclist ""
foreach idx [array names CardRamDacs SVGA-*] {
	eval lappend daclist $CardRamDacs($idx)
}
set CardRamDacs(SVGA)		[lrmdups $daclist]

set CardRamDacs(VGA16-ati)	$CardRamDacs(SVGA-ati)
set CardRamDacs(VGA16-et4000)	$CardRamDacs(SVGA-et4000)
set CardRamDacs(VGA16)		[lrmdups [concat $CardRamDacs(SVGA-ati) \
				  $CardRamDacs(SVGA-et4000)] ]
set CardRamDacs(Mono-ati)	$CardRamDacs(SVGA-ati)
set CardRamDacs(Mono-et4000)	$CardRamDacs(SVGA-et4000)
set CardRamDacs(Mono)		$CardRamDacs(VGA16)

set CardRamDacs(EGC)		{}
set CardRamDacs(PEGC)		{}
set CardRamDacs(GANBWAP)	{}
set CardRamDacs(NKVNEC)		{}
set CardRamDacs(WABS)		{}
set CardRamDacs(WABEP)		{}
set CardRamDacs(WSNA)		{}
set CardRamDacs(TGUI)		{}
set CardRamDacs(MGA)		ti3026
set CardRamDacs(NECS3)		{ sc15025 s3_sdac }
set CardRamDacs(PWSKB)		{ sc15025 bt478 bt485 s3_gendac att20c498 }
set CardRamDacs(PWLB)		{ att20c505 sc15025 ti3025 }
set CardRamDacs(GA968)		ibm_rgb524
unset daclist idx

###

# For each server, what clockchips can be chosen?
set CardClockChips(8514)   {}
set CardClockChips(AGX)	   {}
set CardClockChips(I128)   { ibm_rgb526 ibm_rgb528 ibm_rbg52x ibm_rgb5xx \
			     ti3025 }
set CardClockChips(Mach8)  {}
set CardClockChips(Mach32) {}
set CardClockChips(Mach64) { ati18818 att20c408 ch8398 ibm_rgb514 \
			     ics2595 stg1703 }
set CardClockChips(P9000)  { icd2061a }
set CardClockChips(S3)	   { att20c409 att20c499 att20c408 \
			     ch8391 dcs2824 \
			     ibm_rgb514 ibm_rgb51x ibm_rgb524 ibm_rgb525 \
				ibm_rgb528 ibm_rgb52x ibm_rgb5xx \
			     icd2061a ics2595 ics5300 ics5342 ics9161a \
			     s3_aurora64 s3_sdac s3_trio s3_trio32 \
				s3_trio64 s3_trio64v2 s3gendac \
			     sc11412 stg1703 ti3025 ti3026 ti3030 \
			   }
set CardClockChips(S3V)	   {} ;# { s3_trio64 }
set CardClockChips(TGA)	   ics1562 

set CardClockChips(SVGA-ark)		ics5342
set CardClockChips(SVGA-cirrus)		cirrus
set CardClockChips(SVGA-et4000)		{ dcs2824 et6000 icd2061a ics5341 ics5301 stg1703 }
set CardClockChips(SVGA-mga)		ti3026
set CardClockChips(SVGA-pvga1)          icd2061A
set CardClockChips(SVGA-tvga8900)	tgui
set clklist ""
foreach idx [array names CardClockChips SVGA-*] {
	eval lappend clklist $CardClockChips($idx)
}
set CardClockChips(SVGA)   [lrmdups $clklist]

set CardClockChips(VGA16-et4000)	$CardClockChips(SVGA-et4000)
set CardClockChips(VGA16-tvga8900)	$CardClockChips(SVGA-tvga8900)
set CardClockChips(VGA16)  [lrmdups [concat $CardClockChips(SVGA-et4000) \
				$CardClockChips(SVGA-tvga8900)] ]

set CardClockChips(Mono-cirrus)		$CardClockChips(SVGA-cirrus)
set CardClockChips(Mono-et4000)		$CardClockChips(SVGA-et4000)
set CardClockChips(Mono-tvga8900)	$CardClockChips(SVGA-tvga8900)
set CardClockChips(Mono)  [lrmdups [concat $CardClockChips(Mono-cirrus) \
				$CardClockChips(Mono-et4000) \
				$CardClockChips(Mono-tvga8900)] ]

set CardClockChips(EGC)		{}
set CardClockChips(PEGC)	{}
set CardClockChips(GANBWAP)	cirrus
set CardClockChips(NKVNEC)	cirrus
set CardClockChips(WABS)	cirrus
set CardClockChips(WABEP)	cirrus
set CardClockChips(WSNA)	cirrus
set CardClockChips(TGUI)	tgui
set CardClockChips(MGA)		ti3026
set CardClockChips(NECS3)	s3_sdac
set CardClockChips(PWSKB)	{ icd2061a s3_gendac }
set CardClockChips(PWLB)	{ icd2061a ti3025 }
set CardClockChips(GA968)	{}
unset clklist idx

# For each server, what options can be chosen?
set CardOptions(Mono)	   { 16clocks 8clocks all_wait clgd6225_lcd \
			     clkdiv2 clock_50 clock_66 composite \
			     enable_bitblt epsonmemwin extern_disp \
			     fast_dram favour_bitblt favor_bitblt \
			     fb_debug fifo_aggressive fifo_conservative \
			     first_wwait ga98nb1 ga98nb2 ga98nb4 \
			     hibit_high hibit_low hw_clocks hw_cursor \
			     intern_disp lcd_center lcd_stretch \
			     legend linear med_dram mmio nec_cirrus \
			     noaccel nolinear no_2mb_banksel no_bitblt \
			     no_imageblt no_pci_probe no_pixmap_cache \
			     no_program_clocks \
			     no_wait one_wait pc98_tgui pci_burst_off \
			     pci_burst_on pci_retry power_saver probe_clocks \
			     read_wait secondary \
			     slow_dram swap_hibit sw_cursor tgui_mclk_66 \
			     tgui_pci_read_off tgui_pci_read_on \
			     tgui_pci_write_off tgui_pci_write_on \
			     w32_interleave_off w32_interleave_on \
			     wap write_wait xaa_no_col_exp\
			   }
set CardOptions(VGA16)	   { 16clocks all_wait clgd6225_lcd clkdiv2 \
			     clock_50 clock_66 composite enable_bitblt \
			     fast_dram fb_debug fifo_aggressive \
			     fifo_conservative first_wwait hibit_high \
			     hibit_low hw_clocks hw_cursor \
			     lcd_center lcd_stretch legend \
			     linear med_dram \
			     mmio noaccel nolinear no_pci_probe \
			     no_program_clocks no_wait one_wait \
			     pc98_tgui pci_burst_off pci_burst_on pci_retry \
			     power_saver probe_clocks read_wait \
			     secondary \
			     slow_dram tgui_mclk_66 \
			     tgui_pci_read_off tgui_pci_read_on \
			     tgui_pci_write_off tgui_pci_write_on \
			     w32_interleave_off w32_interleave_on \
			     write_wait xaa_no_col_exp\
			   }
set CardOptions(SVGA)	   { 16clocks 8clocks all_wait clgd6225_lcd \
			     clkdiv2 clock_50 clock_66 composite \
			     dac_6_bit dac_8_bit early_ras_precharge \
			     enable_bitblt \
			     epsonmemwin extern_disp \
			     ext_fram_buf fast_dram favour_bitblt \
			     favor_bitblt fb_debug fifo_aggressive \
			     fifo_conservative fifo_moderate \
			     first_wwait fix_panel_size \
			     fpm_vram ga98nb1 \
			     ga98nb2 ga98nb4 hibit_high hibit_low \
			     hw_clocks hw_cursor intern_disp \
			     late_ras_precharge lcd_center \
			     lcd_centre legend linear \
			     med_dram mmio nec_cirrus noaccel nolinear \
			     no_2mb_banksel no_bitblt no_imageblt \
			     no_pci_probe no_pixmap_cache \
			     no_program_clocks no_stretch no_wait \
			     one_wait pc98_tgui pci_burst_off\
			     pci_burst_on pci_retry power_saver probe_clocks \
			     read_wait slow_edoram slow_dram stn suspend_hack \
			     swap_hibit sw_cursor sync_on_green \
			     tgui_mclk_66 tgui_pci_read_off \
			     tgui_pci_read_on tgui_pci_write_off \
			     tgui_pci_write_on use_18bit_bus use_modeline \
			     use_vclk1 \
			     w32_interleave_off w32_interleave_on wap \
			     write_wait pci_retry no_pixmap_cache \
			     xaa_benchmark xaa_no_col_exp \
			   }
set CardOptions(8514)	   {}
set CardOptions(AGX)	   { 8_bit_bus bt482_curs bt485_curs clkdiv2 \
			     crtc_delay dac_6_bit dac_8_bit engine_delay \
			     fast_dram fast_vram s3_fast_vram \
			     fifo_aggressive fifo_conservative \
			     fifo_moderate med_dram noaccel nolinear \
			     no_wait_state refresh_20 refresh_25 \
			     slow_dram slow_vram s3_slow_vram \
			     sprite_refresh screen_refresh sw_cursor \
			     sync_on_green vlb_a vlb_b vram_128 \
			     vram_256 vram_delay_latch vram_delay_ras \
			     vram_extend_ras wait_state \
			   }
set CardOptions(I128)	   { dac_8_bit noaccel power_saver showcache \
	                     sync_on_green }
set CardOptions(Mach8)	   composite
set CardOptions(Mach32)	   { clkdiv2 composite dac_8_bit intel_gx \
			     nolinear sw_cursor }
set CardOptions(Mach64)	   { block_write clkdiv2 composite dac_6_bit \
			     dac_8_bit hw_cursor no_bios_clocks \
			     no_block_write no_font_cache no_pixmap_cache \
			     no_program_clocks override_bios power_saver \
			     sw_cursor \
			   }
set CardOptions(P9000)	   { noaccel sw_cursor sync_on_green vram_128 vram_256 }
set CardOptions(S3)	   { bt485_curs clkdiv2 dac_6_bit dac_8_bit \
			     diamond early_ras_precharge elsa_w1000pro \
			     elsa_w1000isa elsa_w2000pro elsa_w2000pro/x8 \
			     epsonmemwin fast_vram \
			     s3_fast_vram fb_debug genoa hercules \
			     ibmrgb_curs late_ras_precharge legend \
			     miro_80sv miro_magic_s4 \
			     necwab noinit nolinear no_font_cache \
			     nomemaccess no_pci_disconnect no_pixmap_cache \
			     no_ti3020_curs \
			     number_nine pchkb pci_hack pcskb pcskb4 \
			     power_saver pw805i pw968 pw_localbus \
			     pw_mux s3_964_bt485_vclk s3_968_dash_bug \
			     showcache slow_dram slow_dram_refresh \
			     s3_slow_dram_refresh slow_edodram slow_vram \
			     s3_slow_vram spea_mercury stb stb_pegasus \
			     sw_cursor sync_on_green ti3020_curs \
			     ti3026_curs trio32_fc_bug trio64v+_bug1 \
			     trio64v+_bug2 trio64v+_bug3 \
			   }
set CardOptions(S3V)	   { sw_cursor dac_6_bit dac_8_bit power_saver \
	                     slow_dram_refresh slow_edodram slow_vram }
set CardOptions(TGA)	   { bt485_cursor dac_6_bit dac_8_bit hw_cursor \
	                     power_saver sw_cursor \
			   }

set CardOptions(EGC)		{}
set CardOptions(PEGC)		{}
set CardOptions(GANBWAP)	{ ga98nb1 ga98nb2 ga98nb4 wap epsonmemwin \
				  sw_cursor }
set CardOptions(NKVNEC)		{ nec_cirrus }
set CardOptions(WABS)		{}
set CardOptions(WABEP)		{ med_dram }
set CardOptions(WSNA)		{ epsonmemwin sw_cursor med_dram }
set CardOptions(TGUI)		{ noaccel }
set CardOptions(MGA)		{ noaccel }
set CardOptions(NECS3)		{ necwab nomemaccess dac_8_bit bt485_curs }
set CardOptions(PWSKB)		{ pcskb pcskb4 pchkb pw805i pw_mux \
				  nomemaccess epsonmemwin dac_8_bit \
				  bt485_curs }
set CardOptions(PWLB)		{ pw_localbus dac_8_bit bt485_curs numbernine }
set CardOptions(GA968)		{}

# For each server, what readme files are applicable?
set CardReadmes(SVGA-ark)	README.ark
set CardReadmes(SVGA-ati)	README.ati
set CardReadmes(SVGA-cl64xx)	README.cirrus
set CardReadmes(SVGA-cirrus)	README.cirrus
set CardReadmes(SVGA-chips)	README.chips
set CardReadmes(SVGA-et3000)	README.tseng
set CardReadmes(SVGA-et4000)	README.tseng
set CardReadmes(SVGA-mga)	README.MGA
set CardReadmes(SVGA-nv)	README.NV1
set CardReadmes(SVGA-oak)	README.Oak
set CardReadmes(SVGA-pvga1)	README.WstDig
set CardReadmes(SVGA-s3v)	README.S3V
set CardReadmes(SVGA-sis)	README.SiS
set CardReadmes(SVGA-tvga8900)	README.trident
set CardReadmes(SVGA-video7)	README.Video7
set CardReadmes(SVGA-NONE)	README
set rdmelist ""
foreach idx [array names CardReadmes SVGA-*] {
	eval lappend rdmelist $CardReadmes($idx)
}
set CardReadmes(SVGA)	   [concat [lrmdups $rdmelist]]

set CardReadmes(VGA16-ati)	$CardReadmes(SVGA-ati)
set CardReadmes(VGA16-cl64xx)	$CardReadmes(SVGA-cl64xx)
set CardReadmes(VGA16-et3000)	$CardReadmes(SVGA-et3000)
set CardReadmes(VGA16-et4000)	$CardReadmes(SVGA-et4000)
set CardReadmes(VGA16-oak)	$CardReadmes(SVGA-oak)
set CardReadmes(VGA16-tvga8900)	$CardReadmes(SVGA-tvga8900)
set rdmelist ""
foreach idx [array names CardReadmes VGA16-*] {
	eval lappend rdmelist $CardReadmes($idx)
}
set CardReadmes(VGA16)	   [concat [lrmdups $rdmelist]]


set CardReadmes(Mono-ati)	$CardReadmes(SVGA-ati)
set CardReadmes(Mono-cl64xx)	$CardReadmes(SVGA-cl64xx)
set CardReadmes(Mono-cirrus)	$CardReadmes(SVGA-cirrus)
set CardReadmes(Mono-et3000)	$CardReadmes(SVGA-et3000)
set CardReadmes(Mono-et4000)	$CardReadmes(SVGA-et4000)
set CardReadmes(Mono-oak)	$CardReadmes(SVGA-oak)
set CardReadmes(Mono-pvga1)	$CardReadmes(SVGA-pvga1)
set CardReadmes(Mono-tvga8900)	$CardReadmes(SVGA-tvga8900)
set CardReadmes(Mono)		$CardReadmes(SVGA)
set rdmelist ""
foreach idx [array names CardReadmes Mono-*] {
	eval lappend rdmelist $CardReadmes($idx)
}
set CardReadmes(Mono)	   [concat [lrmdups $rdmelist]]


set CardReadmes(8514)	   {}
set CardReadmes(AGX)	   README.agx
set CardReadmes(I128)	   {}
set CardReadmes(Mach8)	   {}
set CardReadmes(Mach32)	   README.Mach32
set CardReadmes(Mach64)	   README.Mach64
set CardReadmes(P9000)	   README.P9000
set CardReadmes(S3)	   README.S3
set CardReadmes(S3V)	   README.S3V
set CardReadmes(TGA)	   README.DECtga
set CardReadmes(W32)	   README.W32

set CardReadmes(EGC)	   {}
set CardReadmes(PEGC)	   {}
set CardReadmes(GANBWAP)   README.cirrus
set CardReadmes(NKVNEC)    README.cirrus
set CardReadmes(WABS)	   README.cirrus
set CardReadmes(WABEP)	   README.cirrus
set CardReadmes(WSNA)	   README.cirrus
set CardReadmes(TGUI)	   README.trident
set CardReadmes(MGA)	   README.MGA
set CardReadmes(NECS3)	   README.S3
set CardReadmes(PWSKB)	   README.S3
set CardReadmes(PWLB)	   README.S3
set CardReadmes(GA968)	   README.S3
