# $XConsortium: mondata.tcl /main/4 1996/10/28 05:42:19 kaleb $
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/mondata.tcl,v 3.12 1997/12/14 10:03:55 hohndel Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

#
# Data used by the monitor configuration routines
#

array set MonitorVsyncRanges {
	0	60
	1	55-60
	2	60,70,87
	3	55-90
	4	55-90
	5	55-90
	6	50-90
	7	50-90
	8	50-100
	9	40-100
}

array set MonitorHsyncRanges {
	0	31.5
	1	31.5-35.1
	2	31.5,35.5
	3	31.5,35.15,35.5
	4	31.5-37.9
	5	31.5-48.5
	6	31.5-57.0
	7	31.5-64.3
	8	31.5-79.0
	9	31.5-82.0
}

set MonitorDescriptions [list \
	"Standard VGA, 640x480 @ 60 Hz" \
	"Super VGA, 800x600 @ 56 Hz" \
	"8514 Compatible, 1024x768@87 Hz interlaced (no 800x600)" \
	"Super VGA, 1024x768 @ 87 Hz interlaced, 800x600 @ 56 Hz" \
	"Extended Super VGA, 800x600 @ 60 Hz, 640x480 @ 72 Hz" \
	"Non-Interlaced SVGA, 1024x768 @ 60 Hz, 800x600 @ 72 Hz" \
	"High Frequency SVGA, 1024x768 @ 70 Hz" \
	"Multi-frequency that can do 1280x1024 @ 60 Hz" \
	"Multi-frequency that can do 1280x1024 @ 74 Hz" \
	"Multi-frequency that can do 1280x1024 @ 76 Hz" \
]

set haveSelectedModes 0

set DefaultColorDepth 8

array set SelectedMonitorModes { }

array set MonitorStdModes {
	" 640x400 @ pc98 EGC mode : 60 Hz, 24 kHz hsync"
	    "16.442 640 649 656 663 400 407 415 440"
	" 640x400 @ 70 Hz, 31.5 kHz hsync"
	    "25.175 640  664  760  800   400  409  411  450"
	" 640x480 @ 60 Hz, 31.5 kHz hsync"
	    "25.175 640  664  760  800   480  491  493  525"
	" 800x600 @ 56 Hz, 35.15 kHz hsync"
	    "36     800  824  896 1024   600  601  603  625"
	"1024x768 @ 87 Hz interlaced, 35.5 kHz hsync"
	    "44.9  1024 1048 1208 1264   768  776  784  817 Interlace"
	" 640x480 @ 67 Hz, 35.0 kHz hsync"
	    "28     640  664  760  800   480  491  493  525"
	" 640x400 @ 85 Hz, 37.86 kHz hsync"
	   "31.5    640  672 736   832   400  401  404  445 -HSync +VSync"
	" 640x480 @ 70 Hz, 36.5 kHz hsync"
	    "31.5   640  680  720  864   480  488  491  521"
	" 640x480 @ 75 Hz, 37.50 kHz hsync"
	    "31.5   640  656  720  840   480  481  484  500 -HSync -VSync"
	" 800x600 @ 60 Hz, 37.8 kHz hsync"
	    "40     800  840  968 1056   600  601  605  628 +hsync +vsync"
	" 640x480 @ 85 Hz, 43.27 kHz hsync"
	    "36     640  696  752  832   480  481  484  509 -HSync -VSync"
	"1152x864 @ 89 Hz interlaced, 44 kHz hsync"
	    "65    1152 1168 1384 1480   864  865  875  985 Interlace"
	" 800x600 @ 72 Hz, 48.0 kHz hsync"
	    "50     800  856  976 1040   600  637  643  666 +hsync +vsync"
	"1024x768 @ 60 Hz, 48.4 kHz hsync"
	    "65    1024 1032 1176 1344   768  771  777  806 -hsync -vsync"
	" 640x480 @ 100 Hz, 53.01 kHz hsync"
	    "45.8   640  672  768  864   480  488  494  530 -HSync -VSync"
	"1152x864 @ 60 Hz, 53.5 kHz hsync"
	    "89.9  1152 1216 1472 1680   864  868  876  892 -HSync -VSync"
	" 800x600 @ 85 Hz, 55.84 kHz hsync"
	   "60.75   800  864  928 1088   600  616  621  657 -HSync -VSync"
	"1024x768 @ 70 Hz, 56.5 kHz hsync"
	    "75    1024 1048 1184 1328   768  771  777  806 -hsync -vsync"
	"1280x1024 @ 87 Hz interlaced, 51 kHz hsync"
	    "80    1280 1296 1512 1568  1024 1025 1037 1165 Interlace"
	"1024x768 @ 76 Hz, 62.5 kHz hsync"
	    "85    1024 1032 1152 1360   768  784  787  823"
	"1152x864 @ 70 Hz, 62.4 kHz hsync"
	    "92    1152 1208 1368 1474   864  865  875  895"
	" 800x600 @ 100 Hz, 64.02 kHz hsync"
	    "69.650 800  864  928 1088   600  604  610  640 -HSync -VSync"
	"1024x768 @ 85 Hz, 70.24 kHz hsync"
	    "98.9  1024 1056 1216 1408   768 782 788 822 -HSync -VSync"
	"1152x864 @ 78 Hz, 70.8 kHz hsync"
	    "110    1152 1240 1324 1552   864  864  876  908"
	"1152x864 @ 84 Hz, 76.0 kHz hsync"
	    "135    1152 1464 1592 1776   864  864  876  908"
	"1280x1024 @ 61 Hz, 64.2 kHz hsync"
	    "110    1280 1328 1512 1712  1024 1025 1028 1054"
	"1280x1024 @ 70 Hz, 74.59 kHz hsync"
	    "126.5  1280 1312 1472 1696  1024 1032 1040 1068 -HSync -VSync"
	"1600x1200 @ 60Hz, 75.00 kHz hsync"
	    "162    1600 1664 1856 2160  1200 1201 1204 1250 +HSync +VSync"
	"1280x1024 @ 74 Hz, 78.85 kHz hsync"
	    "135    1280 1312 1456 1712  1024 1027 1030 1064"
	"1024x768 @ 100Hz, 80.21 kHz hsync"
	    "115.5  1024 1056 1248 1440  768  771  781  802 -HSync -VSync"
	"1280x1024 @ 76 Hz, 81.13 kHz hsync"
	    "135    1280 1312 1416 1664  1024 1027 1030 1064"
	"1600x1200 @ 70 Hz, 87.50 kHz hsync"
	    "189    1600 1664 1856 2160  1200 1201 1204 1250 -HSync -VSync"
	"1152x864 @ 100 Hz, 89.62 kHz hsync"
	    "137.65 1152 1184 1312 1536   864  866  885  902 -HSync -VSync"
	"1280x1024 @ 85 Hz, 91.15 kHz hsync"
	    "157.5  1280 1344 1504 1728  1024 1025 1028 1072 +HSync +VSync"
	"1600x1200 @ 75 Hz, 93.75 kHz hsync"
	    "202.5  1600 1664 1856 2160  1200 1201 1204 1250 +HSync +VSync"
	"1600x1200 @ 85 Hz, 105.77 kHz hsync"
	    "220    1600 1616 1808 2080  1200 1204 1207 1244 +HSync +VSync"
	"1280x1024 @ 100 Hz, 107.16 kHz hsync"
	    "181.75 1280 1312 1440 1696  1024 1031 1046 1072 -HSync -VSync"
	"1800x1440 @ 64Hz, 96.15 kHz hsync"
	    "230    1800 1896 2088 2392 1440 1441 1444 1490 +HSync +VSync"
	"1800x1440 @ 70Hz, 104.52 kHz hsync"
	    "250    1800 1896 2088 2392 1440 1441 1444 1490 +HSync +VSync"
	" 320x200 @ 70 Hz, 31.5 kHz hsync, 8:5 aspect ratio"
	    "12.588 320  336  384  400   200  204  205  225 Doublescan"
	" 320x240 @ 60 Hz, 31.5 kHz hsync, 4:3 aspect ratio"
	    "12.588 320  336  384  400   240  245  246  262 Doublescan"
	" 320x240 @ 72 Hz, 36.5 kHz hsync"
	    "15.750 320  336  384  400   240  244  246  262 Doublescan"
	" 400x300 @ 56 Hz, 35.2 kHz hsync, 4:3 aspect ratio"
	    "18     400  416  448  512   300  301  302  312 Doublescan"
	" 400x300 @ 60 Hz, 37.8 kHz hsync"
	    "20     400  416  480  528   300  301  303  314 Doublescan"
	" 400x300 @ 72 Hz, 48.0 kHz hsync"
	    "25     400  424  488  520   300  319  322  333 Doublescan"
	" 480x300 @ 56 Hz, 35.2 kHz hsync, 8:5 aspect ratio"
	    "21.656 480  496  536  616   300  301  302  312 Doublescan"
	" 480x300 @ 60 Hz, 37.8 kHz hsync"
	    "23.890 480  496  576  632   300  301  303  314 Doublescan"
	" 480x300 @ 63 Hz, 39.6 kHz hsync"
	    "25     480  496  576  632   300  301  303  314 Doublescan"
	" 480x300 @ 72 Hz, 48.0 kHz hsync"
	    "29.952 480  504  584  624   300  319  322  333 Doublescan"
	" 512x384 @ 78 Hz, 31.50 kHz hsync"
	    "20.160 512  528  592  640   384  385  388  404 -HSync -VSync"
	" 512x384 @ 85 Hz, 34.38 kHz hsync"
	    "22     512  528  592  640   384  385  388  404 -HSync -VSync"
}

