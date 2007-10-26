/*
 * $Id$
 *
 * Copyright © 2003 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* 201 glyphnames in 271 entries, 74% occupancy */

#define FC_GLYPHNAME_HASH 271
#define FC_GLYPHNAME_REHASH 269
#define FC_GLYPHNAME_MAXLEN 4

static const struct { const FcChar32 ucs; const FcChar8 name[3]; } glyph0 = { 0x2701, "a1" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph1 = { 0x2721, "a10" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph2 = { 0x275e, "a100" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph3 = { 0x2761, "a101" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph4 = { 0x2762, "a102" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph5 = { 0x2763, "a103" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph6 = { 0x2764, "a104" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph7 = { 0x2710, "a105" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph8 = { 0x2765, "a106" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph9 = { 0x2766, "a107" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph10 = { 0x2767, "a108" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph11 = { 0x2660, "a109" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph12 = { 0x261b, "a11" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph13 = { 0x2665, "a110" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph14 = { 0x2666, "a111" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph15 = { 0x2663, "a112" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph16 = { 0x2709, "a117" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph17 = { 0x2708, "a118" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph18 = { 0x2707, "a119" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph19 = { 0x261e, "a12" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph20 = { 0x2460, "a120" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph21 = { 0x2461, "a121" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph22 = { 0x2462, "a122" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph23 = { 0x2463, "a123" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph24 = { 0x2464, "a124" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph25 = { 0x2465, "a125" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph26 = { 0x2466, "a126" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph27 = { 0x2467, "a127" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph28 = { 0x2468, "a128" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph29 = { 0x2469, "a129" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph30 = { 0x270c, "a13" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph31 = { 0x2776, "a130" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph32 = { 0x2777, "a131" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph33 = { 0x2778, "a132" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph34 = { 0x2779, "a133" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph35 = { 0x277a, "a134" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph36 = { 0x277b, "a135" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph37 = { 0x277c, "a136" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph38 = { 0x277d, "a137" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph39 = { 0x277e, "a138" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph40 = { 0x277f, "a139" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph41 = { 0x270d, "a14" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph42 = { 0x2780, "a140" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph43 = { 0x2781, "a141" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph44 = { 0x2782, "a142" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph45 = { 0x2783, "a143" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph46 = { 0x2784, "a144" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph47 = { 0x2785, "a145" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph48 = { 0x2786, "a146" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph49 = { 0x2787, "a147" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph50 = { 0x2788, "a148" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph51 = { 0x2789, "a149" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph52 = { 0x270e, "a15" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph53 = { 0x278a, "a150" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph54 = { 0x278b, "a151" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph55 = { 0x278c, "a152" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph56 = { 0x278d, "a153" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph57 = { 0x278e, "a154" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph58 = { 0x278f, "a155" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph59 = { 0x2790, "a156" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph60 = { 0x2791, "a157" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph61 = { 0x2792, "a158" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph62 = { 0x2793, "a159" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph63 = { 0x270f, "a16" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph64 = { 0x2794, "a160" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph65 = { 0x2192, "a161" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph66 = { 0x27a3, "a162" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph67 = { 0x2194, "a163" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph68 = { 0x2195, "a164" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph69 = { 0x2799, "a165" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph70 = { 0x279b, "a166" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph71 = { 0x279c, "a167" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph72 = { 0x279d, "a168" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph73 = { 0x279e, "a169" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph74 = { 0x2711, "a17" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph75 = { 0x279f, "a170" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph76 = { 0x27a0, "a171" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph77 = { 0x27a1, "a172" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph78 = { 0x27a2, "a173" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph79 = { 0x27a4, "a174" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph80 = { 0x27a5, "a175" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph81 = { 0x27a6, "a176" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph82 = { 0x27a7, "a177" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph83 = { 0x27a8, "a178" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph84 = { 0x27a9, "a179" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph85 = { 0x2712, "a18" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph86 = { 0x27ab, "a180" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph87 = { 0x27ad, "a181" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph88 = { 0x27af, "a182" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph89 = { 0x27b2, "a183" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph90 = { 0x27b3, "a184" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph91 = { 0x27b5, "a185" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph92 = { 0x27b8, "a186" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph93 = { 0x27ba, "a187" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph94 = { 0x27bb, "a188" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph95 = { 0x27bc, "a189" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph96 = { 0x2713, "a19" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph97 = { 0x27bd, "a190" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph98 = { 0x27be, "a191" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph99 = { 0x279a, "a192" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph100 = { 0x27aa, "a193" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph101 = { 0x27b6, "a194" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph102 = { 0x27b9, "a195" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph103 = { 0x2798, "a196" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph104 = { 0x27b4, "a197" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph105 = { 0x27b7, "a198" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph106 = { 0x27ac, "a199" };
static const struct { const FcChar32 ucs; const FcChar8 name[3]; } glyph107 = { 0x2702, "a2" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph108 = { 0x2714, "a20" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph109 = { 0x27ae, "a200" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph110 = { 0x27b1, "a201" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph111 = { 0x2703, "a202" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph112 = { 0x2750, "a203" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph113 = { 0x2752, "a204" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph114 = { 0x276e, "a205" };
static const struct { const FcChar32 ucs; const FcChar8 name[5]; } glyph115 = { 0x2770, "a206" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph116 = { 0x2715, "a21" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph117 = { 0x2716, "a22" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph118 = { 0x2717, "a23" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph119 = { 0x2718, "a24" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph120 = { 0x2719, "a25" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph121 = { 0x271a, "a26" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph122 = { 0x271b, "a27" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph123 = { 0x271c, "a28" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph124 = { 0x2722, "a29" };
static const struct { const FcChar32 ucs; const FcChar8 name[3]; } glyph125 = { 0x2704, "a3" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph126 = { 0x2723, "a30" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph127 = { 0x2724, "a31" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph128 = { 0x2725, "a32" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph129 = { 0x2726, "a33" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph130 = { 0x2727, "a34" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph131 = { 0x2605, "a35" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph132 = { 0x2729, "a36" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph133 = { 0x272a, "a37" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph134 = { 0x272b, "a38" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph135 = { 0x272c, "a39" };
static const struct { const FcChar32 ucs; const FcChar8 name[3]; } glyph136 = { 0x260e, "a4" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph137 = { 0x272d, "a40" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph138 = { 0x272e, "a41" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph139 = { 0x272f, "a42" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph140 = { 0x2730, "a43" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph141 = { 0x2731, "a44" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph142 = { 0x2732, "a45" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph143 = { 0x2733, "a46" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph144 = { 0x2734, "a47" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph145 = { 0x2735, "a48" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph146 = { 0x2736, "a49" };
static const struct { const FcChar32 ucs; const FcChar8 name[3]; } glyph147 = { 0x2706, "a5" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph148 = { 0x2737, "a50" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph149 = { 0x2738, "a51" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph150 = { 0x2739, "a52" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph151 = { 0x273a, "a53" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph152 = { 0x273b, "a54" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph153 = { 0x273c, "a55" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph154 = { 0x273d, "a56" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph155 = { 0x273e, "a57" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph156 = { 0x273f, "a58" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph157 = { 0x2740, "a59" };
static const struct { const FcChar32 ucs; const FcChar8 name[3]; } glyph158 = { 0x271d, "a6" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph159 = { 0x2741, "a60" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph160 = { 0x2742, "a61" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph161 = { 0x2743, "a62" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph162 = { 0x2744, "a63" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph163 = { 0x2745, "a64" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph164 = { 0x2746, "a65" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph165 = { 0x2747, "a66" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph166 = { 0x2748, "a67" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph167 = { 0x2749, "a68" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph168 = { 0x274a, "a69" };
static const struct { const FcChar32 ucs; const FcChar8 name[3]; } glyph169 = { 0x271e, "a7" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph170 = { 0x274b, "a70" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph171 = { 0x25cf, "a71" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph172 = { 0x274d, "a72" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph173 = { 0x25a0, "a73" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph174 = { 0x274f, "a74" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph175 = { 0x2751, "a75" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph176 = { 0x25b2, "a76" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph177 = { 0x25bc, "a77" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph178 = { 0x25c6, "a78" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph179 = { 0x2756, "a79" };
static const struct { const FcChar32 ucs; const FcChar8 name[3]; } glyph180 = { 0x271f, "a8" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph181 = { 0x25d7, "a81" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph182 = { 0x2758, "a82" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph183 = { 0x2759, "a83" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph184 = { 0x275a, "a84" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph185 = { 0x276f, "a85" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph186 = { 0x2771, "a86" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph187 = { 0x2772, "a87" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph188 = { 0x2773, "a88" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph189 = { 0x2768, "a89" };
static const struct { const FcChar32 ucs; const FcChar8 name[3]; } glyph190 = { 0x2720, "a9" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph191 = { 0x2769, "a90" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph192 = { 0x276c, "a91" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph193 = { 0x276d, "a92" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph194 = { 0x276a, "a93" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph195 = { 0x276b, "a94" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph196 = { 0x2774, "a95" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph197 = { 0x2775, "a96" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph198 = { 0x275b, "a97" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph199 = { 0x275c, "a98" };
static const struct { const FcChar32 ucs; const FcChar8 name[4]; } glyph200 = { 0x275d, "a99" };
static const FcGlyphName	*name_to_ucs[271] = {
(FcGlyphName *) &glyph195,
(FcGlyphName *) &glyph124,
(FcGlyphName *) &glyph82,
0,
(FcGlyphName *) &glyph155,
(FcGlyphName *) &glyph143,
(FcGlyphName *) &glyph187,
(FcGlyphName *) &glyph79,
(FcGlyphName *) &glyph194,
(FcGlyphName *) &glyph144,
(FcGlyphName *) &glyph173,
(FcGlyphName *) &glyph182,
(FcGlyphName *) &glyph80,
(FcGlyphName *) &glyph163,
(FcGlyphName *) &glyph192,
(FcGlyphName *) &glyph168,
0,
(FcGlyphName *) &glyph138,
(FcGlyphName *) &glyph196,
(FcGlyphName *) &glyph170,
(FcGlyphName *) &glyph171,
(FcGlyphName *) &glyph148,
(FcGlyphName *) &glyph73,
(FcGlyphName *) &glyph102,
(FcGlyphName *) &glyph191,
(FcGlyphName *) &glyph140,
(FcGlyphName *) &glyph99,
0,
0,
(FcGlyphName *) &glyph100,
0,
0,
(FcGlyphName *) &glyph97,
0,
0,
(FcGlyphName *) &glyph199,
(FcGlyphName *) &glyph177,
(FcGlyphName *) &glyph50,
(FcGlyphName *) &glyph59,
0,
0,
(FcGlyphName *) &glyph60,
(FcGlyphName *) &glyph51,
0,
(FcGlyphName *) &glyph57,
(FcGlyphName *) &glyph76,
0,
(FcGlyphName *) &glyph35,
0,
0,
(FcGlyphName *) &glyph55,
0,
(FcGlyphName *) &glyph62,
(FcGlyphName *) &glyph56,
0,
0,
(FcGlyphName *) &glyph53,
0,
0,
(FcGlyphName *) &glyph54,
0,
(FcGlyphName *) &glyph114,
(FcGlyphName *) &glyph33,
(FcGlyphName *) &glyph172,
(FcGlyphName *) &glyph154,
(FcGlyphName *) &glyph21,
(FcGlyphName *) &glyph189,
(FcGlyphName *) &glyph174,
(FcGlyphName *) &glyph31,
0,
0,
(FcGlyphName *) &glyph32,
0,
0,
(FcGlyphName *) &glyph24,
0,
0,
(FcGlyphName *) &glyph38,
(FcGlyphName *) &glyph161,
0,
(FcGlyphName *) &glyph26,
0,
(FcGlyphName *) &glyph109,
(FcGlyphName *) &glyph27,
0,
(FcGlyphName *) &glyph162,
(FcGlyphName *) &glyph96,
(FcGlyphName *) &glyph94,
(FcGlyphName *) &glyph95,
(FcGlyphName *) &glyph105,
(FcGlyphName *) &glyph106,
(FcGlyphName *) &glyph90,
(FcGlyphName *) &glyph91,
(FcGlyphName *) &glyph92,
(FcGlyphName *) &glyph93,
(FcGlyphName *) &glyph86,
(FcGlyphName *) &glyph87,
(FcGlyphName *) &glyph88,
(FcGlyphName *) &glyph89,
(FcGlyphName *) &glyph28,
(FcGlyphName *) &glyph29,
(FcGlyphName *) &glyph39,
(FcGlyphName *) &glyph40,
(FcGlyphName *) &glyph10,
(FcGlyphName *) &glyph11,
(FcGlyphName *) &glyph17,
(FcGlyphName *) &glyph18,
(FcGlyphName *) &glyph6,
(FcGlyphName *) &glyph7,
(FcGlyphName *) &glyph8,
(FcGlyphName *) &glyph9,
(FcGlyphName *) &glyph2,
(FcGlyphName *) &glyph3,
(FcGlyphName *) &glyph4,
(FcGlyphName *) &glyph5,
(FcGlyphName *) &glyph200,
0,
(FcGlyphName *) &glyph108,
(FcGlyphName *) &glyph115,
(FcGlyphName *) &glyph116,
(FcGlyphName *) &glyph156,
(FcGlyphName *) &glyph117,
0,
(FcGlyphName *) &glyph118,
0,
(FcGlyphName *) &glyph119,
(FcGlyphName *) &glyph157,
(FcGlyphName *) &glyph120,
0,
(FcGlyphName *) &glyph121,
0,
(FcGlyphName *) &glyph122,
(FcGlyphName *) &glyph141,
(FcGlyphName *) &glyph123,
0,
(FcGlyphName *) &glyph110,
0,
(FcGlyphName *) &glyph134,
(FcGlyphName *) &glyph142,
(FcGlyphName *) &glyph135,
0,
(FcGlyphName *) &glyph150,
0,
(FcGlyphName *) &glyph64,
(FcGlyphName *) &glyph152,
(FcGlyphName *) &glyph139,
0,
(FcGlyphName *) &glyph65,
0,
0,
(FcGlyphName *) &glyph153,
(FcGlyphName *) &glyph66,
0,
(FcGlyphName *) &glyph112,
(FcGlyphName *) &glyph179,
(FcGlyphName *) &glyph58,
(FcGlyphName *) &glyph176,
0,
0,
(FcGlyphName *) &glyph68,
(FcGlyphName *) &glyph167,
0,
(FcGlyphName *) &glyph164,
(FcGlyphName *) &glyph69,
0,
(FcGlyphName *) &glyph129,
(FcGlyphName *) &glyph61,
(FcGlyphName *) &glyph70,
(FcGlyphName *) &glyph165,
0,
(FcGlyphName *) &glyph178,
(FcGlyphName *) &glyph71,
(FcGlyphName *) &glyph197,
0,
(FcGlyphName *) &glyph166,
(FcGlyphName *) &glyph72,
(FcGlyphName *) &glyph198,
(FcGlyphName *) &glyph184,
(FcGlyphName *) &glyph185,
(FcGlyphName *) &glyph34,
(FcGlyphName *) &glyph145,
(FcGlyphName *) &glyph193,
(FcGlyphName *) &glyph181,
(FcGlyphName *) &glyph83,
(FcGlyphName *) &glyph183,
(FcGlyphName *) &glyph146,
0,
(FcGlyphName *) &glyph84,
(FcGlyphName *) &glyph103,
(FcGlyphName *) &glyph188,
(FcGlyphName *) &glyph104,
(FcGlyphName *) &glyph37,
(FcGlyphName *) &glyph101,
(FcGlyphName *) &glyph63,
(FcGlyphName *) &glyph74,
(FcGlyphName *) &glyph41,
(FcGlyphName *) &glyph52,
(FcGlyphName *) &glyph19,
(FcGlyphName *) &glyph30,
(FcGlyphName *) &glyph1,
(FcGlyphName *) &glyph12,
(FcGlyphName *) &glyph111,
(FcGlyphName *) &glyph98,
(FcGlyphName *) &glyph36,
(FcGlyphName *) &glyph46,
(FcGlyphName *) &glyph137,
(FcGlyphName *) &glyph47,
(FcGlyphName *) &glyph85,
(FcGlyphName *) &glyph48,
0,
(FcGlyphName *) &glyph49,
(FcGlyphName *) &glyph160,
(FcGlyphName *) &glyph42,
0,
(FcGlyphName *) &glyph43,
(FcGlyphName *) &glyph113,
(FcGlyphName *) &glyph44,
(FcGlyphName *) &glyph158,
(FcGlyphName *) &glyph45,
0,
(FcGlyphName *) &glyph20,
0,
(FcGlyphName *) &glyph16,
0,
(FcGlyphName *) &glyph22,
(FcGlyphName *) &glyph151,
(FcGlyphName *) &glyph23,
(FcGlyphName *) &glyph127,
(FcGlyphName *) &glyph15,
0,
(FcGlyphName *) &glyph25,
0,
(FcGlyphName *) &glyph13,
0,
(FcGlyphName *) &glyph14,
(FcGlyphName *) &glyph149,
(FcGlyphName *) &glyph128,
0,
0,
0,
(FcGlyphName *) &glyph107,
(FcGlyphName *) &glyph125,
0,
(FcGlyphName *) &glyph0,
(FcGlyphName *) &glyph126,
(FcGlyphName *) &glyph169,
(FcGlyphName *) &glyph136,
(FcGlyphName *) &glyph147,
(FcGlyphName *) &glyph77,
0,
(FcGlyphName *) &glyph180,
(FcGlyphName *) &glyph190,
(FcGlyphName *) &glyph132,
(FcGlyphName *) &glyph78,
0,
0,
(FcGlyphName *) &glyph133,
0,
(FcGlyphName *) &glyph75,
0,
(FcGlyphName *) &glyph130,
0,
(FcGlyphName *) &glyph175,
(FcGlyphName *) &glyph67,
(FcGlyphName *) &glyph131,
0,
0,
(FcGlyphName *) &glyph159,
(FcGlyphName *) &glyph81,
0,
(FcGlyphName *) &glyph186,
};
static const FcGlyphName	*ucs_to_name[271] = {
(FcGlyphName *) &glyph134,
(FcGlyphName *) &glyph57,
(FcGlyphName *) &glyph19,
(FcGlyphName *) &glyph58,
(FcGlyphName *) &glyph139,
(FcGlyphName *) &glyph59,
(FcGlyphName *) &glyph141,
(FcGlyphName *) &glyph60,
(FcGlyphName *) &glyph143,
(FcGlyphName *) &glyph61,
(FcGlyphName *) &glyph145,
(FcGlyphName *) &glyph62,
(FcGlyphName *) &glyph148,
(FcGlyphName *) &glyph64,
(FcGlyphName *) &glyph150,
(FcGlyphName *) &glyph151,
(FcGlyphName *) &glyph152,
(FcGlyphName *) &glyph153,
(FcGlyphName *) &glyph154,
(FcGlyphName *) &glyph155,
(FcGlyphName *) &glyph156,
(FcGlyphName *) &glyph157,
(FcGlyphName *) &glyph159,
(FcGlyphName *) &glyph160,
(FcGlyphName *) &glyph161,
(FcGlyphName *) &glyph162,
(FcGlyphName *) &glyph131,
(FcGlyphName *) &glyph164,
(FcGlyphName *) &glyph165,
(FcGlyphName *) &glyph166,
(FcGlyphName *) &glyph167,
(FcGlyphName *) &glyph168,
(FcGlyphName *) &glyph170,
0,
(FcGlyphName *) &glyph172,
0,
(FcGlyphName *) &glyph174,
(FcGlyphName *) &glyph112,
(FcGlyphName *) &glyph175,
(FcGlyphName *) &glyph113,
0,
0,
0,
(FcGlyphName *) &glyph179,
0,
(FcGlyphName *) &glyph182,
(FcGlyphName *) &glyph158,
(FcGlyphName *) &glyph184,
(FcGlyphName *) &glyph198,
(FcGlyphName *) &glyph199,
(FcGlyphName *) &glyph200,
(FcGlyphName *) &glyph2,
0,
0,
(FcGlyphName *) &glyph3,
(FcGlyphName *) &glyph4,
(FcGlyphName *) &glyph5,
(FcGlyphName *) &glyph6,
(FcGlyphName *) &glyph8,
(FcGlyphName *) &glyph9,
(FcGlyphName *) &glyph10,
(FcGlyphName *) &glyph189,
(FcGlyphName *) &glyph191,
(FcGlyphName *) &glyph194,
(FcGlyphName *) &glyph195,
(FcGlyphName *) &glyph192,
(FcGlyphName *) &glyph193,
(FcGlyphName *) &glyph114,
(FcGlyphName *) &glyph11,
(FcGlyphName *) &glyph115,
(FcGlyphName *) &glyph186,
(FcGlyphName *) &glyph15,
(FcGlyphName *) &glyph133,
(FcGlyphName *) &glyph13,
(FcGlyphName *) &glyph14,
(FcGlyphName *) &glyph31,
(FcGlyphName *) &glyph32,
(FcGlyphName *) &glyph33,
(FcGlyphName *) &glyph34,
(FcGlyphName *) &glyph35,
(FcGlyphName *) &glyph36,
(FcGlyphName *) &glyph37,
(FcGlyphName *) &glyph38,
(FcGlyphName *) &glyph39,
(FcGlyphName *) &glyph40,
(FcGlyphName *) &glyph42,
(FcGlyphName *) &glyph43,
(FcGlyphName *) &glyph44,
(FcGlyphName *) &glyph45,
(FcGlyphName *) &glyph46,
(FcGlyphName *) &glyph47,
(FcGlyphName *) &glyph48,
(FcGlyphName *) &glyph49,
(FcGlyphName *) &glyph50,
(FcGlyphName *) &glyph51,
(FcGlyphName *) &glyph53,
(FcGlyphName *) &glyph54,
(FcGlyphName *) &glyph55,
(FcGlyphName *) &glyph20,
(FcGlyphName *) &glyph21,
(FcGlyphName *) &glyph22,
(FcGlyphName *) &glyph23,
(FcGlyphName *) &glyph24,
(FcGlyphName *) &glyph25,
(FcGlyphName *) &glyph26,
(FcGlyphName *) &glyph27,
(FcGlyphName *) &glyph28,
(FcGlyphName *) &glyph29,
0,
(FcGlyphName *) &glyph103,
(FcGlyphName *) &glyph69,
(FcGlyphName *) &glyph99,
(FcGlyphName *) &glyph70,
(FcGlyphName *) &glyph71,
(FcGlyphName *) &glyph72,
(FcGlyphName *) &glyph73,
(FcGlyphName *) &glyph75,
(FcGlyphName *) &glyph76,
(FcGlyphName *) &glyph77,
(FcGlyphName *) &glyph78,
(FcGlyphName *) &glyph66,
(FcGlyphName *) &glyph79,
(FcGlyphName *) &glyph80,
(FcGlyphName *) &glyph81,
(FcGlyphName *) &glyph82,
(FcGlyphName *) &glyph83,
(FcGlyphName *) &glyph84,
(FcGlyphName *) &glyph100,
(FcGlyphName *) &glyph86,
(FcGlyphName *) &glyph106,
(FcGlyphName *) &glyph87,
(FcGlyphName *) &glyph109,
(FcGlyphName *) &glyph88,
0,
(FcGlyphName *) &glyph110,
(FcGlyphName *) &glyph89,
(FcGlyphName *) &glyph90,
(FcGlyphName *) &glyph104,
(FcGlyphName *) &glyph91,
(FcGlyphName *) &glyph101,
(FcGlyphName *) &glyph105,
(FcGlyphName *) &glyph92,
(FcGlyphName *) &glyph102,
(FcGlyphName *) &glyph93,
(FcGlyphName *) &glyph94,
(FcGlyphName *) &glyph95,
(FcGlyphName *) &glyph97,
(FcGlyphName *) &glyph98,
0,
(FcGlyphName *) &glyph177,
0,
(FcGlyphName *) &glyph135,
0,
0,
(FcGlyphName *) &glyph137,
0,
0,
(FcGlyphName *) &glyph138,
0,
0,
0,
0,
0,
(FcGlyphName *) &glyph140,
0,
(FcGlyphName *) &glyph176,
(FcGlyphName *) &glyph183,
0,
0,
(FcGlyphName *) &glyph142,
0,
(FcGlyphName *) &glyph56,
0,
0,
0,
(FcGlyphName *) &glyph144,
0,
0,
0,
0,
0,
(FcGlyphName *) &glyph146,
0,
0,
0,
(FcGlyphName *) &glyph178,
0,
(FcGlyphName *) &glyph149,
0,
0,
0,
0,
0,
(FcGlyphName *) &glyph65,
(FcGlyphName *) &glyph171,
(FcGlyphName *) &glyph67,
(FcGlyphName *) &glyph68,
0,
0,
0,
0,
0,
(FcGlyphName *) &glyph173,
0,
0,
(FcGlyphName *) &glyph181,
0,
0,
0,
0,
(FcGlyphName *) &glyph185,
0,
0,
0,
0,
0,
(FcGlyphName *) &glyph187,
0,
(FcGlyphName *) &glyph188,
0,
(FcGlyphName *) &glyph196,
0,
(FcGlyphName *) &glyph197,
0,
0,
0,
(FcGlyphName *) &glyph163,
0,
0,
(FcGlyphName *) &glyph0,
(FcGlyphName *) &glyph107,
(FcGlyphName *) &glyph111,
(FcGlyphName *) &glyph125,
0,
(FcGlyphName *) &glyph147,
(FcGlyphName *) &glyph18,
(FcGlyphName *) &glyph17,
(FcGlyphName *) &glyph16,
0,
0,
(FcGlyphName *) &glyph30,
(FcGlyphName *) &glyph41,
(FcGlyphName *) &glyph52,
(FcGlyphName *) &glyph63,
(FcGlyphName *) &glyph7,
(FcGlyphName *) &glyph74,
(FcGlyphName *) &glyph85,
(FcGlyphName *) &glyph96,
(FcGlyphName *) &glyph108,
(FcGlyphName *) &glyph116,
(FcGlyphName *) &glyph117,
(FcGlyphName *) &glyph118,
(FcGlyphName *) &glyph119,
(FcGlyphName *) &glyph120,
(FcGlyphName *) &glyph121,
(FcGlyphName *) &glyph122,
(FcGlyphName *) &glyph123,
(FcGlyphName *) &glyph136,
(FcGlyphName *) &glyph169,
(FcGlyphName *) &glyph180,
(FcGlyphName *) &glyph190,
(FcGlyphName *) &glyph1,
(FcGlyphName *) &glyph124,
(FcGlyphName *) &glyph126,
(FcGlyphName *) &glyph127,
(FcGlyphName *) &glyph128,
(FcGlyphName *) &glyph129,
(FcGlyphName *) &glyph130,
0,
(FcGlyphName *) &glyph132,
(FcGlyphName *) &glyph12,
};
