/*
 * $RCSId: xc/lib/fontconfig/fc-lang/fclang.tmpl.h,v 1.1 2002/07/06 23:21:36 keithp Exp $
 *
 * Copyright © 2002 Keith Packard
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

/* total size: 819 unique leaves: 568 */

#define LEAF0       (185 * sizeof (FcLangCharSet))
#define OFF0        (LEAF0 + 568 * sizeof (FcCharLeaf))
#define NUM0        (OFF0 + 610 * sizeof (intptr_t))
#define SET(n)      (n * sizeof (FcLangCharSet) + offsetof (FcLangCharSet, charset))
#define OFF(s,o)    (OFF0 + o * sizeof (intptr_t) - SET(s))
#define NUM(s,n)    (NUM0 + n * sizeof (FcChar16) - SET(s))
#define LEAF(o,l)   (LEAF0 + l * sizeof (FcCharLeaf) - (OFF0 + o * sizeof (intptr_t)))
#define fcLangCharSets (fcLangData.langCharSets)

static const struct {
    FcLangCharSet  langCharSets[185];
    FcCharLeaf     leaves[568];
    intptr_t       leaf_offsets[610];
    FcChar16       numbers[610];
} fcLangData = {
{
    { (FcChar8 *) "aa",  { FC_REF_CONSTANT, 1, OFF(0,0), NUM(0,0) } }, /* 0 */
    { (FcChar8 *) "ab",  { FC_REF_CONSTANT, 1, OFF(1,1), NUM(1,1) } }, /* 1 */
    { (FcChar8 *) "af",  { FC_REF_CONSTANT, 2, OFF(2,2), NUM(2,2) } }, /* 2 */
    { (FcChar8 *) "am",  { FC_REF_CONSTANT, 2, OFF(3,4), NUM(3,4) } }, /* 3 */
    { (FcChar8 *) "ar",  { FC_REF_CONSTANT, 1, OFF(4,6), NUM(4,6) } }, /* 4 */
    { (FcChar8 *) "as",  { FC_REF_CONSTANT, 1, OFF(5,7), NUM(5,7) } }, /* 5 */
    { (FcChar8 *) "ast",  { FC_REF_CONSTANT, 1, OFF(6,8), NUM(6,8) } }, /* 6 */
    { (FcChar8 *) "ava",  { FC_REF_CONSTANT, 1, OFF(7,9), NUM(7,9) } }, /* 7 */
    { (FcChar8 *) "ay",  { FC_REF_CONSTANT, 1, OFF(8,10), NUM(8,10) } }, /* 8 */
    { (FcChar8 *) "az",  { FC_REF_CONSTANT, 4, OFF(9,11), NUM(9,11) } }, /* 9 */
    { (FcChar8 *) "az-ir",  { FC_REF_CONSTANT, 2, OFF(10,15), NUM(10,15) } }, /* 10 */
    { (FcChar8 *) "ba",  { FC_REF_CONSTANT, 1, OFF(11,17), NUM(11,17) } }, /* 11 */
    { (FcChar8 *) "bam",  { FC_REF_CONSTANT, 3, OFF(12,18), NUM(12,18) } }, /* 12 */
    { (FcChar8 *) "be",  { FC_REF_CONSTANT, 1, OFF(13,21), NUM(13,21) } }, /* 13 */
    { (FcChar8 *) "bg",  { FC_REF_CONSTANT, 1, OFF(14,22), NUM(14,22) } }, /* 14 */
    { (FcChar8 *) "bh",  { FC_REF_CONSTANT, 1, OFF(15,23), NUM(15,23) } }, /* 15 */
    { (FcChar8 *) "bho",  { FC_REF_CONSTANT, 1, OFF(16,23), NUM(16,23) } }, /* 16 */
    { (FcChar8 *) "bi",  { FC_REF_CONSTANT, 1, OFF(17,24), NUM(17,24) } }, /* 17 */
    { (FcChar8 *) "bin",  { FC_REF_CONSTANT, 3, OFF(18,25), NUM(18,25) } }, /* 18 */
    { (FcChar8 *) "bn",  { FC_REF_CONSTANT, 1, OFF(19,7), NUM(19,7) } }, /* 19 */
    { (FcChar8 *) "bo",  { FC_REF_CONSTANT, 1, OFF(20,28), NUM(20,28) } }, /* 20 */
    { (FcChar8 *) "br",  { FC_REF_CONSTANT, 1, OFF(21,29), NUM(21,29) } }, /* 21 */
    { (FcChar8 *) "bs",  { FC_REF_CONSTANT, 2, OFF(22,30), NUM(22,30) } }, /* 22 */
    { (FcChar8 *) "bua",  { FC_REF_CONSTANT, 1, OFF(23,32), NUM(23,32) } }, /* 23 */
    { (FcChar8 *) "ca",  { FC_REF_CONSTANT, 2, OFF(24,33), NUM(24,33) } }, /* 24 */
    { (FcChar8 *) "ce",  { FC_REF_CONSTANT, 1, OFF(25,9), NUM(25,9) } }, /* 25 */
    { (FcChar8 *) "ch",  { FC_REF_CONSTANT, 1, OFF(26,35), NUM(26,35) } }, /* 26 */
    { (FcChar8 *) "chm",  { FC_REF_CONSTANT, 1, OFF(27,36), NUM(27,36) } }, /* 27 */
    { (FcChar8 *) "chr",  { FC_REF_CONSTANT, 1, OFF(28,37), NUM(28,37) } }, /* 28 */
    { (FcChar8 *) "co",  { FC_REF_CONSTANT, 3, OFF(29,38), NUM(29,38) } }, /* 29 */
    { (FcChar8 *) "cs",  { FC_REF_CONSTANT, 2, OFF(30,41), NUM(30,41) } }, /* 30 */
    { (FcChar8 *) "cu",  { FC_REF_CONSTANT, 1, OFF(31,43), NUM(31,43) } }, /* 31 */
    { (FcChar8 *) "cv",  { FC_REF_CONSTANT, 2, OFF(32,44), NUM(32,44) } }, /* 32 */
    { (FcChar8 *) "cy",  { FC_REF_CONSTANT, 3, OFF(33,46), NUM(33,46) } }, /* 33 */
    { (FcChar8 *) "da",  { FC_REF_CONSTANT, 1, OFF(34,49), NUM(34,49) } }, /* 34 */
    { (FcChar8 *) "de",  { FC_REF_CONSTANT, 2, OFF(35,50), NUM(35,50) } }, /* 35 */
    { (FcChar8 *) "dz",  { FC_REF_CONSTANT, 1, OFF(36,28), NUM(36,28) } }, /* 36 */
    { (FcChar8 *) "el",  { FC_REF_CONSTANT, 2, OFF(37,52), NUM(37,52) } }, /* 37 */
    { (FcChar8 *) "en",  { FC_REF_CONSTANT, 2, OFF(38,54), NUM(38,54) } }, /* 38 */
    { (FcChar8 *) "eo",  { FC_REF_CONSTANT, 2, OFF(39,56), NUM(39,56) } }, /* 39 */
    { (FcChar8 *) "es",  { FC_REF_CONSTANT, 2, OFF(40,58), NUM(40,58) } }, /* 40 */
    { (FcChar8 *) "et",  { FC_REF_CONSTANT, 2, OFF(41,60), NUM(41,60) } }, /* 41 */
    { (FcChar8 *) "eu",  { FC_REF_CONSTANT, 1, OFF(42,62), NUM(42,62) } }, /* 42 */
    { (FcChar8 *) "fa",  { FC_REF_CONSTANT, 2, OFF(43,15), NUM(43,15) } }, /* 43 */
    { (FcChar8 *) "fi",  { FC_REF_CONSTANT, 3, OFF(44,63), NUM(44,63) } }, /* 44 */
    { (FcChar8 *) "fj",  { FC_REF_CONSTANT, 1, OFF(45,66), NUM(45,66) } }, /* 45 */
    { (FcChar8 *) "fo",  { FC_REF_CONSTANT, 1, OFF(46,67), NUM(46,67) } }, /* 46 */
    { (FcChar8 *) "fr",  { FC_REF_CONSTANT, 3, OFF(47,38), NUM(47,38) } }, /* 47 */
    { (FcChar8 *) "ful",  { FC_REF_CONSTANT, 3, OFF(48,68), NUM(48,68) } }, /* 48 */
    { (FcChar8 *) "fur",  { FC_REF_CONSTANT, 1, OFF(49,71), NUM(49,71) } }, /* 49 */
    { (FcChar8 *) "fy",  { FC_REF_CONSTANT, 1, OFF(50,72), NUM(50,72) } }, /* 50 */
    { (FcChar8 *) "ga",  { FC_REF_CONSTANT, 3, OFF(51,73), NUM(51,73) } }, /* 51 */
    { (FcChar8 *) "gd",  { FC_REF_CONSTANT, 1, OFF(52,76), NUM(52,76) } }, /* 52 */
    { (FcChar8 *) "gez",  { FC_REF_CONSTANT, 2, OFF(53,4), NUM(53,4) } }, /* 53 */
    { (FcChar8 *) "gl",  { FC_REF_CONSTANT, 1, OFF(54,77), NUM(54,77) } }, /* 54 */
    { (FcChar8 *) "gn",  { FC_REF_CONSTANT, 3, OFF(55,78), NUM(55,78) } }, /* 55 */
    { (FcChar8 *) "gu",  { FC_REF_CONSTANT, 1, OFF(56,81), NUM(56,81) } }, /* 56 */
    { (FcChar8 *) "gv",  { FC_REF_CONSTANT, 1, OFF(57,82), NUM(57,82) } }, /* 57 */
    { (FcChar8 *) "ha",  { FC_REF_CONSTANT, 3, OFF(58,83), NUM(58,83) } }, /* 58 */
    { (FcChar8 *) "haw",  { FC_REF_CONSTANT, 3, OFF(59,86), NUM(59,86) } }, /* 59 */
    { (FcChar8 *) "he",  { FC_REF_CONSTANT, 1, OFF(60,89), NUM(60,89) } }, /* 60 */
    { (FcChar8 *) "hi",  { FC_REF_CONSTANT, 1, OFF(61,23), NUM(61,23) } }, /* 61 */
    { (FcChar8 *) "ho",  { FC_REF_CONSTANT, 1, OFF(62,66), NUM(62,66) } }, /* 62 */
    { (FcChar8 *) "hr",  { FC_REF_CONSTANT, 2, OFF(63,30), NUM(63,30) } }, /* 63 */
    { (FcChar8 *) "hu",  { FC_REF_CONSTANT, 2, OFF(64,90), NUM(64,90) } }, /* 64 */
    { (FcChar8 *) "hy",  { FC_REF_CONSTANT, 1, OFF(65,92), NUM(65,92) } }, /* 65 */
    { (FcChar8 *) "ia",  { FC_REF_CONSTANT, 1, OFF(66,66), NUM(66,66) } }, /* 66 */
    { (FcChar8 *) "ibo",  { FC_REF_CONSTANT, 2, OFF(67,93), NUM(67,93) } }, /* 67 */
    { (FcChar8 *) "id",  { FC_REF_CONSTANT, 1, OFF(68,95), NUM(68,95) } }, /* 68 */
    { (FcChar8 *) "ie",  { FC_REF_CONSTANT, 1, OFF(69,66), NUM(69,66) } }, /* 69 */
    { (FcChar8 *) "ik",  { FC_REF_CONSTANT, 1, OFF(70,96), NUM(70,96) } }, /* 70 */
    { (FcChar8 *) "io",  { FC_REF_CONSTANT, 1, OFF(71,66), NUM(71,66) } }, /* 71 */
    { (FcChar8 *) "is",  { FC_REF_CONSTANT, 1, OFF(72,97), NUM(72,97) } }, /* 72 */
    { (FcChar8 *) "it",  { FC_REF_CONSTANT, 2, OFF(73,98), NUM(73,98) } }, /* 73 */
    { (FcChar8 *) "iu",  { FC_REF_CONSTANT, 3, OFF(74,100), NUM(74,100) } }, /* 74 */
    { (FcChar8 *) "ja",  { FC_REF_CONSTANT, 84, OFF(75,103), NUM(75,103) } }, /* 75 */
    { (FcChar8 *) "ka",  { FC_REF_CONSTANT, 1, OFF(76,187), NUM(76,187) } }, /* 76 */
    { (FcChar8 *) "kaa",  { FC_REF_CONSTANT, 1, OFF(77,188), NUM(77,188) } }, /* 77 */
    { (FcChar8 *) "ki",  { FC_REF_CONSTANT, 2, OFF(78,189), NUM(78,189) } }, /* 78 */
    { (FcChar8 *) "kk",  { FC_REF_CONSTANT, 1, OFF(79,191), NUM(79,191) } }, /* 79 */
    { (FcChar8 *) "kl",  { FC_REF_CONSTANT, 2, OFF(80,192), NUM(80,192) } }, /* 80 */
    { (FcChar8 *) "km",  { FC_REF_CONSTANT, 1, OFF(81,194), NUM(81,194) } }, /* 81 */
    { (FcChar8 *) "kn",  { FC_REF_CONSTANT, 1, OFF(82,195), NUM(82,195) } }, /* 82 */
    { (FcChar8 *) "ko",  { FC_REF_CONSTANT, 45, OFF(83,196), NUM(83,196) } }, /* 83 */
    { (FcChar8 *) "kok",  { FC_REF_CONSTANT, 1, OFF(84,23), NUM(84,23) } }, /* 84 */
    { (FcChar8 *) "ks",  { FC_REF_CONSTANT, 1, OFF(85,23), NUM(85,23) } }, /* 85 */
    { (FcChar8 *) "ku",  { FC_REF_CONSTANT, 2, OFF(86,241), NUM(86,241) } }, /* 86 */
    { (FcChar8 *) "ku-ir",  { FC_REF_CONSTANT, 1, OFF(87,243), NUM(87,243) } }, /* 87 */
    { (FcChar8 *) "kum",  { FC_REF_CONSTANT, 1, OFF(88,244), NUM(88,244) } }, /* 88 */
    { (FcChar8 *) "kv",  { FC_REF_CONSTANT, 1, OFF(89,245), NUM(89,245) } }, /* 89 */
    { (FcChar8 *) "kw",  { FC_REF_CONSTANT, 3, OFF(90,246), NUM(90,246) } }, /* 90 */
    { (FcChar8 *) "ky",  { FC_REF_CONSTANT, 1, OFF(91,249), NUM(91,249) } }, /* 91 */
    { (FcChar8 *) "la",  { FC_REF_CONSTANT, 2, OFF(92,250), NUM(92,250) } }, /* 92 */
    { (FcChar8 *) "lb",  { FC_REF_CONSTANT, 1, OFF(93,252), NUM(93,252) } }, /* 93 */
    { (FcChar8 *) "lez",  { FC_REF_CONSTANT, 1, OFF(94,9), NUM(94,9) } }, /* 94 */
    { (FcChar8 *) "ln",  { FC_REF_CONSTANT, 4, OFF(95,253), NUM(95,253) } }, /* 95 */
    { (FcChar8 *) "lo",  { FC_REF_CONSTANT, 1, OFF(96,257), NUM(96,257) } }, /* 96 */
    { (FcChar8 *) "lt",  { FC_REF_CONSTANT, 2, OFF(97,258), NUM(97,258) } }, /* 97 */
    { (FcChar8 *) "lv",  { FC_REF_CONSTANT, 2, OFF(98,260), NUM(98,260) } }, /* 98 */
    { (FcChar8 *) "mg",  { FC_REF_CONSTANT, 1, OFF(99,262), NUM(99,262) } }, /* 99 */
    { (FcChar8 *) "mh",  { FC_REF_CONSTANT, 2, OFF(100,263), NUM(100,263) } }, /* 100 */
    { (FcChar8 *) "mi",  { FC_REF_CONSTANT, 3, OFF(101,265), NUM(101,265) } }, /* 101 */
    { (FcChar8 *) "mk",  { FC_REF_CONSTANT, 1, OFF(102,268), NUM(102,268) } }, /* 102 */
    { (FcChar8 *) "ml",  { FC_REF_CONSTANT, 1, OFF(103,269), NUM(103,269) } }, /* 103 */
    { (FcChar8 *) "mn",  { FC_REF_CONSTANT, 1, OFF(104,270), NUM(104,270) } }, /* 104 */
    { (FcChar8 *) "mo",  { FC_REF_CONSTANT, 4, OFF(105,271), NUM(105,271) } }, /* 105 */
    { (FcChar8 *) "mr",  { FC_REF_CONSTANT, 1, OFF(106,23), NUM(106,23) } }, /* 106 */
    { (FcChar8 *) "mt",  { FC_REF_CONSTANT, 2, OFF(107,275), NUM(107,275) } }, /* 107 */
    { (FcChar8 *) "my",  { FC_REF_CONSTANT, 1, OFF(108,277), NUM(108,277) } }, /* 108 */
    { (FcChar8 *) "nb",  { FC_REF_CONSTANT, 1, OFF(109,278), NUM(109,278) } }, /* 109 */
    { (FcChar8 *) "nds",  { FC_REF_CONSTANT, 1, OFF(110,279), NUM(110,279) } }, /* 110 */
    { (FcChar8 *) "ne",  { FC_REF_CONSTANT, 1, OFF(111,23), NUM(111,23) } }, /* 111 */
    { (FcChar8 *) "nl",  { FC_REF_CONSTANT, 2, OFF(112,280), NUM(112,280) } }, /* 112 */
    { (FcChar8 *) "nn",  { FC_REF_CONSTANT, 1, OFF(113,282), NUM(113,282) } }, /* 113 */
    { (FcChar8 *) "no",  { FC_REF_CONSTANT, 1, OFF(114,278), NUM(114,278) } }, /* 114 */
    { (FcChar8 *) "ny",  { FC_REF_CONSTANT, 2, OFF(115,283), NUM(115,283) } }, /* 115 */
    { (FcChar8 *) "oc",  { FC_REF_CONSTANT, 1, OFF(116,285), NUM(116,285) } }, /* 116 */
    { (FcChar8 *) "om",  { FC_REF_CONSTANT, 1, OFF(117,66), NUM(117,66) } }, /* 117 */
    { (FcChar8 *) "or",  { FC_REF_CONSTANT, 1, OFF(118,286), NUM(118,286) } }, /* 118 */
    { (FcChar8 *) "os",  { FC_REF_CONSTANT, 1, OFF(119,244), NUM(119,244) } }, /* 119 */
    { (FcChar8 *) "pa",  { FC_REF_CONSTANT, 1, OFF(120,287), NUM(120,287) } }, /* 120 */
    { (FcChar8 *) "pl",  { FC_REF_CONSTANT, 2, OFF(121,288), NUM(121,288) } }, /* 121 */
    { (FcChar8 *) "ps-af",  { FC_REF_CONSTANT, 1, OFF(122,290), NUM(122,290) } }, /* 122 */
    { (FcChar8 *) "ps-pk",  { FC_REF_CONSTANT, 1, OFF(123,291), NUM(123,291) } }, /* 123 */
    { (FcChar8 *) "pt",  { FC_REF_CONSTANT, 2, OFF(124,292), NUM(124,292) } }, /* 124 */
    { (FcChar8 *) "rm",  { FC_REF_CONSTANT, 1, OFF(125,294), NUM(125,294) } }, /* 125 */
    { (FcChar8 *) "ro",  { FC_REF_CONSTANT, 3, OFF(126,295), NUM(126,295) } }, /* 126 */
    { (FcChar8 *) "ru",  { FC_REF_CONSTANT, 1, OFF(127,244), NUM(127,244) } }, /* 127 */
    { (FcChar8 *) "sa",  { FC_REF_CONSTANT, 1, OFF(128,23), NUM(128,23) } }, /* 128 */
    { (FcChar8 *) "sah",  { FC_REF_CONSTANT, 1, OFF(129,298), NUM(129,298) } }, /* 129 */
    { (FcChar8 *) "sco",  { FC_REF_CONSTANT, 3, OFF(130,299), NUM(130,299) } }, /* 130 */
    { (FcChar8 *) "se",  { FC_REF_CONSTANT, 2, OFF(131,302), NUM(131,302) } }, /* 131 */
    { (FcChar8 *) "sel",  { FC_REF_CONSTANT, 1, OFF(132,244), NUM(132,244) } }, /* 132 */
    { (FcChar8 *) "sh",  { FC_REF_CONSTANT, 1, OFF(133,304), NUM(133,304) } }, /* 133 */
    { (FcChar8 *) "si",  { FC_REF_CONSTANT, 1, OFF(134,305), NUM(134,305) } }, /* 134 */
    { (FcChar8 *) "sk",  { FC_REF_CONSTANT, 2, OFF(135,306), NUM(135,306) } }, /* 135 */
    { (FcChar8 *) "sl",  { FC_REF_CONSTANT, 2, OFF(136,30), NUM(136,30) } }, /* 136 */
    { (FcChar8 *) "sm",  { FC_REF_CONSTANT, 2, OFF(137,308), NUM(137,308) } }, /* 137 */
    { (FcChar8 *) "sma",  { FC_REF_CONSTANT, 1, OFF(138,310), NUM(138,310) } }, /* 138 */
    { (FcChar8 *) "smj",  { FC_REF_CONSTANT, 1, OFF(139,311), NUM(139,311) } }, /* 139 */
    { (FcChar8 *) "smn",  { FC_REF_CONSTANT, 2, OFF(140,312), NUM(140,312) } }, /* 140 */
    { (FcChar8 *) "sms",  { FC_REF_CONSTANT, 3, OFF(141,314), NUM(141,314) } }, /* 141 */
    { (FcChar8 *) "so",  { FC_REF_CONSTANT, 1, OFF(142,66), NUM(142,66) } }, /* 142 */
    { (FcChar8 *) "sq",  { FC_REF_CONSTANT, 1, OFF(143,317), NUM(143,317) } }, /* 143 */
    { (FcChar8 *) "sr",  { FC_REF_CONSTANT, 1, OFF(144,304), NUM(144,304) } }, /* 144 */
    { (FcChar8 *) "sv",  { FC_REF_CONSTANT, 1, OFF(145,318), NUM(145,318) } }, /* 145 */
    { (FcChar8 *) "sw",  { FC_REF_CONSTANT, 1, OFF(146,66), NUM(146,66) } }, /* 146 */
    { (FcChar8 *) "syr",  { FC_REF_CONSTANT, 1, OFF(147,319), NUM(147,319) } }, /* 147 */
    { (FcChar8 *) "ta",  { FC_REF_CONSTANT, 1, OFF(148,320), NUM(148,320) } }, /* 148 */
    { (FcChar8 *) "te",  { FC_REF_CONSTANT, 1, OFF(149,321), NUM(149,321) } }, /* 149 */
    { (FcChar8 *) "tg",  { FC_REF_CONSTANT, 1, OFF(150,322), NUM(150,322) } }, /* 150 */
    { (FcChar8 *) "th",  { FC_REF_CONSTANT, 1, OFF(151,323), NUM(151,323) } }, /* 151 */
    { (FcChar8 *) "ti-er",  { FC_REF_CONSTANT, 2, OFF(152,324), NUM(152,324) } }, /* 152 */
    { (FcChar8 *) "ti-et",  { FC_REF_CONSTANT, 2, OFF(153,324), NUM(153,324) } }, /* 153 */
    { (FcChar8 *) "tig",  { FC_REF_CONSTANT, 2, OFF(154,326), NUM(154,326) } }, /* 154 */
    { (FcChar8 *) "tk",  { FC_REF_CONSTANT, 1, OFF(155,328), NUM(155,328) } }, /* 155 */
    { (FcChar8 *) "tl",  { FC_REF_CONSTANT, 1, OFF(156,329), NUM(156,329) } }, /* 156 */
    { (FcChar8 *) "tn",  { FC_REF_CONSTANT, 1, OFF(157,330), NUM(157,330) } }, /* 157 */
    { (FcChar8 *) "to",  { FC_REF_CONSTANT, 2, OFF(158,308), NUM(158,308) } }, /* 158 */
    { (FcChar8 *) "tr",  { FC_REF_CONSTANT, 2, OFF(159,331), NUM(159,331) } }, /* 159 */
    { (FcChar8 *) "ts",  { FC_REF_CONSTANT, 1, OFF(160,66), NUM(160,66) } }, /* 160 */
    { (FcChar8 *) "tt",  { FC_REF_CONSTANT, 1, OFF(161,333), NUM(161,333) } }, /* 161 */
    { (FcChar8 *) "tw",  { FC_REF_CONSTANT, 5, OFF(162,334), NUM(162,334) } }, /* 162 */
    { (FcChar8 *) "tyv",  { FC_REF_CONSTANT, 1, OFF(163,249), NUM(163,249) } }, /* 163 */
    { (FcChar8 *) "ug",  { FC_REF_CONSTANT, 1, OFF(164,6), NUM(164,6) } }, /* 164 */
    { (FcChar8 *) "uk",  { FC_REF_CONSTANT, 1, OFF(165,339), NUM(165,339) } }, /* 165 */
    { (FcChar8 *) "ur",  { FC_REF_CONSTANT, 2, OFF(166,340), NUM(166,340) } }, /* 166 */
    { (FcChar8 *) "uz",  { FC_REF_CONSTANT, 1, OFF(167,342), NUM(167,342) } }, /* 167 */
    { (FcChar8 *) "ven",  { FC_REF_CONSTANT, 2, OFF(168,343), NUM(168,343) } }, /* 168 */
    { (FcChar8 *) "vi",  { FC_REF_CONSTANT, 4, OFF(169,345), NUM(169,345) } }, /* 169 */
    { (FcChar8 *) "vo",  { FC_REF_CONSTANT, 1, OFF(170,349), NUM(170,349) } }, /* 170 */
    { (FcChar8 *) "vot",  { FC_REF_CONSTANT, 2, OFF(171,350), NUM(171,350) } }, /* 171 */
    { (FcChar8 *) "wa",  { FC_REF_CONSTANT, 1, OFF(172,352), NUM(172,352) } }, /* 172 */
    { (FcChar8 *) "wen",  { FC_REF_CONSTANT, 2, OFF(173,353), NUM(173,353) } }, /* 173 */
    { (FcChar8 *) "wo",  { FC_REF_CONSTANT, 2, OFF(174,355), NUM(174,355) } }, /* 174 */
    { (FcChar8 *) "xh",  { FC_REF_CONSTANT, 1, OFF(175,66), NUM(175,66) } }, /* 175 */
    { (FcChar8 *) "yap",  { FC_REF_CONSTANT, 1, OFF(176,357), NUM(176,357) } }, /* 176 */
    { (FcChar8 *) "yi",  { FC_REF_CONSTANT, 1, OFF(177,89), NUM(177,89) } }, /* 177 */
    { (FcChar8 *) "yo",  { FC_REF_CONSTANT, 4, OFF(178,358), NUM(178,358) } }, /* 178 */
    { (FcChar8 *) "zh-cn",  { FC_REF_CONSTANT, 82, OFF(179,362), NUM(179,362) } }, /* 179 */
    { (FcChar8 *) "zh-hk",  { FC_REF_CONSTANT, 83, OFF(180,444), NUM(180,444) } }, /* 180 */
    { (FcChar8 *) "zh-mo",  { FC_REF_CONSTANT, 83, OFF(181,444), NUM(181,444) } }, /* 181 */
    { (FcChar8 *) "zh-sg",  { FC_REF_CONSTANT, 82, OFF(182,362), NUM(182,362) } }, /* 182 */
    { (FcChar8 *) "zh-tw",  { FC_REF_CONSTANT, 83, OFF(183,527), NUM(183,527) } }, /* 183 */
    { (FcChar8 *) "zu",  { FC_REF_CONSTANT, 1, OFF(184,66), NUM(184,66) } }, /* 184 */
},
{
    { { /* 0 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x08104404, 0x08104404,
    } },
    { { /* 1 */
    0xffff8002, 0xffffffff, 0x8002ffff, 0x00000000,
    0xc0000000, 0xf0fc33c0, 0x03000000, 0x00000003,
    } },
    { { /* 2 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x0810cf00, 0x0810cf00,
    } },
    { { /* 3 */
    0x00000000, 0x00000000, 0x00000200, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 4 */
    0xff7fff7f, 0xff01ff7f, 0x00003d7f, 0x00ff00ff,
    0x00ff3d7f, 0x003d7fff, 0xff7f7f00, 0x00ff7f00,
    } },
    { { /* 5 */
    0x003d7f00, 0xffff01ff, 0x007fff7f, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 6 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xffffffff, 0xffffffff, 0xffffffff, 0x1fffffff,
    } },
    { { /* 7 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xfff99fee, 0xd3c5fdff, 0xb080399f, 0x07ffffcf,
    } },
    { { /* 8 */
    0x00000000, 0x00000000, 0x07ffffff, 0x07ffffff,
    0x00000000, 0x88000802, 0x140a2202, 0x140a2202,
    } },
    { { /* 9 */
    0xffff0042, 0xffffffff, 0x0002ffff, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 10 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x10028010, 0x10028010,
    } },
    { { /* 11 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x10400090, 0x10400090,
    } },
    { { /* 12 */
    0xc0000000, 0x00030000, 0xc0000000, 0x00000000,
    0x00008000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 13 */
    0x00000000, 0x00000000, 0x02000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 14 */
    0xffff0100, 0xffffffff, 0x0100ffff, 0x00000000,
    0x300c0000, 0x0f00c000, 0x03000000, 0x00000300,
    } },
    { { /* 15 */
    0x00000000, 0x00000000, 0x03c00000, 0x3c000000,
    0x003fcc00, 0x00000000, 0x00000000, 0xf0000000,
    } },
    { { /* 16 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xfffffe7f, 0xffffffff, 0xe1ffffff, 0x18007fff,
    } },
    { { /* 17 */
    0xffff0000, 0xffffffff, 0x0000ffff, 0x00000000,
    0x030c0000, 0x0c00cc0f, 0x03000000, 0x00000300,
    } },
    { { /* 18 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 19 */
    0x00000000, 0x00000000, 0x00000c00, 0x00000000,
    0x20010040, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 20 */
    0x00000000, 0x00000000, 0x08100000, 0x00040000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 21 */
    0xffff4040, 0xffffffff, 0x4040ffff, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 22 */
    0xffff0000, 0xffffd7ff, 0x0000d7ff, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 23 */
    0xffffffe0, 0x83ffffff, 0x00003fff, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 24 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x10008200, 0x10008200,
    } },
    { { /* 25 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x060c3303, 0x060c3303,
    } },
    { { /* 26 */
    0x00000003, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 27 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x03000000, 0x00003000, 0x00000000,
    } },
    { { /* 28 */
    0x00000000, 0x00000000, 0xfffffeff, 0x3d7e03ff,
    0xfeff0003, 0x03ffffff, 0x00000000, 0x00000000,
    } },
    { { /* 29 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x12120404, 0x12120404,
    } },
    { { /* 30 */
    0x000330c0, 0x00000000, 0x00000000, 0x60000003,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 31 */
    0xffff0002, 0xffffffff, 0x0002ffff, 0x00000000,
    0x00000000, 0x0c00c000, 0x00000000, 0x00000000,
    } },
    { { /* 32 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x140ca381, 0x140ca381,
    } },
    { { /* 33 */
    0x00000000, 0x80000000, 0x00000001, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 34 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x10020004, 0x10020004,
    } },
    { { /* 35 */
    0xffff0002, 0xffffffff, 0x0002ffff, 0x00000000,
    0x00000000, 0x00000030, 0x000c0000, 0x030300c0,
    } },
    { { /* 36 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0xffffffff, 0xffffffff, 0x001fffff,
    } },
    { { /* 37 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x1a10cfc5, 0x9a10cfc5,
    } },
    { { /* 38 */
    0x00000000, 0x00000000, 0x000c0000, 0x01000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 39 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00001000, 0x00000000, 0x00000000,
    } },
    { { /* 40 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x24082202, 0x24082202,
    } },
    { { /* 41 */
    0x0c00f000, 0x00000000, 0x03000180, 0x6000c033,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 42 */
    0xfdffa966, 0xffffdfff, 0xa965dfff, 0x03ffffff,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 43 */
    0x0000000c, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 44 */
    0xffff0002, 0xffffffff, 0x0002ffff, 0x00000000,
    0x00000000, 0x00000c00, 0x00c00000, 0x000c0000,
    } },
    { { /* 45 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x0010c604, 0x8010c604,
    } },
    { { /* 46 */
    0x00000000, 0x00000000, 0x00000000, 0x01f00000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 47 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x0000003f, 0x00000000, 0x00000000, 0x000c0000,
    } },
    { { /* 48 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x25082262, 0x25082262,
    } },
    { { /* 49 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x90400010, 0x10400010,
    } },
    { { /* 50 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xffffd740, 0xfffffffb, 0x00007fff, 0x00000000,
    } },
    { { /* 51 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00528f81, 0x00528f81,
    } },
    { { /* 52 */
    0x30000300, 0x00300030, 0x30000000, 0x00003000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 53 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x140a2202, 0x140a2202,
    } },
    { { /* 54 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x10600010, 0x10600010,
    } },
    { { /* 55 */
    0x00000000, 0x00000000, 0x00000000, 0x60000003,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 56 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x10020000, 0x10020000,
    } },
    { { /* 57 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00400030, 0x00400030,
    } },
    { { /* 58 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x25092042, 0x25092042,
    } },
    { { /* 59 */
    0x00000000, 0x00000000, 0x00000c00, 0x00000000,
    0x20000402, 0x00180000, 0x00000000, 0x00000000,
    } },
    { { /* 60 */
    0x00000000, 0x00000000, 0x00880000, 0x00040000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 61 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x02041107, 0x02041107,
    } },
    { { /* 62 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x9c508e14, 0x1c508e14,
    } },
    { { /* 63 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x04082202, 0x04082202,
    } },
    { { /* 64 */
    0x00000c00, 0x00000003, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 65 */
    0xc0000c0c, 0x00000000, 0x00c00003, 0x00000c03,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 66 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x020c1383, 0x020c1383,
    } },
    { { /* 67 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x040a2202, 0x042a220a,
    } },
    { { /* 68 */
    0x00000000, 0x00000200, 0x00000000, 0x00000200,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 69 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x20000000, 0x00000000, 0x02000000,
    } },
    { { /* 70 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xfffbafee, 0xf3edfdff, 0x00013bbf, 0x0000ffc1,
    } },
    { { /* 71 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00000080, 0x00000080,
    } },
    { { /* 72 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x03000402, 0x00180000, 0x00000000, 0x00000000,
    } },
    { { /* 73 */
    0x00000000, 0x00000000, 0x00880000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 74 */
    0x000c0003, 0x00000c00, 0x00003000, 0x00000c00,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 75 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x08000000, 0x00000000, 0x00000000,
    } },
    { { /* 76 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0xffff0000, 0x000007ff,
    } },
    { { /* 77 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x14482202, 0x14482202,
    } },
    { { /* 78 */
    0x00000000, 0x00000000, 0x00030000, 0x00030000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 79 */
    0x00000000, 0xfffe0000, 0x007fffff, 0xfffffffe,
    0x000000ff, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 80 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00003c00, 0x00000030,
    } },
    { { /* 81 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00000200, 0x00000200,
    } },
    { { /* 82 */
    0xffff4002, 0xffffffff, 0x4002ffff, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 83 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x64092242, 0x64092242,
    } },
    { { /* 84 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x060cb301, 0x060cb301,
    } },
    { { /* 85 */
    0x00000c7e, 0x031f8000, 0x0063f200, 0x000df840,
    0x00037e08, 0x08000dfa, 0x0df901bf, 0x5437e400,
    } },
    { { /* 86 */
    0x00000025, 0x40006fc0, 0x27f91be4, 0xdee00000,
    0x007ff83f, 0x00007f7f, 0x00000000, 0x00000000,
    } },
    { { /* 87 */
    0x00000000, 0x00000000, 0x00000000, 0x007f8000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 88 */
    0x000000e7, 0x00000000, 0xfffffffe, 0xffffffff,
    0x780fffff, 0xfffffffe, 0xffffffff, 0x787fffff,
    } },
    { { /* 89 */
    0x43f36f8b, 0x9b462442, 0xe3e0e82c, 0x400a0004,
    0xdb365f65, 0x04497977, 0xe3f0ecd7, 0x08c56038,
    } },
    { { /* 90 */
    0x3403e602, 0x35518000, 0x7eabe0c8, 0x98698200,
    0x2942a948, 0x8060e803, 0xad93441c, 0x4568c03a,
    } },
    { { /* 91 */
    0x8656aa60, 0x02403f7a, 0x14618388, 0x21741020,
    0x07022021, 0x40bc3000, 0x4462a624, 0x0a2060a8,
    } },
    { { /* 92 */
    0x85740217, 0x9c840402, 0x14157bfb, 0x11e27f24,
    0x02efb665, 0x20ff1f75, 0x28403a70, 0x676326c3,
    } },
    { { /* 93 */
    0x20924dd9, 0x0fc946b0, 0x4850bc98, 0xa03f8638,
    0x88162388, 0x52323e09, 0xe3a422aa, 0xc72c00dd,
    } },
    { { /* 94 */
    0x26e1a166, 0x8f0a840b, 0x559e27eb, 0x89bbc241,
    0x85400014, 0x08496361, 0x8ad07f0c, 0x05cfff3e,
    } },
    { { /* 95 */
    0xa803ff1a, 0x7b407a41, 0x80024745, 0x38eb0500,
    0x0005d851, 0x710c9934, 0x01000397, 0x24046366,
    } },
    { { /* 96 */
    0x005180d0, 0x430ac000, 0x30c89071, 0x58000008,
    0xf7000e99, 0x00415f80, 0x941000b0, 0x62800018,
    } },
    { { /* 97 */
    0x09d00240, 0x01568200, 0x08015004, 0x05101d10,
    0x001084c1, 0x10504025, 0x4d8a410f, 0xa60d4009,
    } },
    { { /* 98 */
    0x914cab19, 0x098121c0, 0x0003c485, 0x80000652,
    0x00080b04, 0x0009041d, 0x905c4849, 0x16900009,
    } },
    { { /* 99 */
    0x22200c65, 0x24338412, 0x47960c03, 0x42250a04,
    0x90880028, 0x4f084900, 0xd3aa14a2, 0x3e87d830,
    } },
    { { /* 100 */
    0x1f618604, 0x41867ea4, 0x05b3c390, 0x211857a5,
    0x2a48241e, 0x4a041128, 0x161b0a40, 0x88400d60,
    } },
    { { /* 101 */
    0x9502020a, 0x10608221, 0x04000243, 0x80001444,
    0x0c040000, 0x70000000, 0x00c11a06, 0x0c00024a,
    } },
    { { /* 102 */
    0x00401a00, 0x40451404, 0xbdb30029, 0x052b0a78,
    0xbfa0bba9, 0x8379407c, 0xe81d12fc, 0xc5694bf6,
    } },
    { { /* 103 */
    0x044aeff6, 0xff022115, 0x402bed63, 0x0242d033,
    0x00131000, 0x59ca1b02, 0x020000a0, 0x2c41a703,
    } },
    { { /* 104 */
    0x8ff24880, 0x00000204, 0x10055800, 0x00489200,
    0x20011894, 0x34805004, 0x684c3200, 0x68be49ea,
    } },
    { { /* 105 */
    0x2e42184c, 0x21c9a820, 0x80b050b9, 0xff7c001e,
    0x14e0849a, 0x01e028c1, 0xac49870e, 0xdddb130f,
    } },
    { { /* 106 */
    0x89fbbe1a, 0x51a2a2e0, 0x32ca5502, 0x928b3e46,
    0x438f1dbf, 0x32186703, 0x33c03028, 0xa9230811,
    } },
    { { /* 107 */
    0x3a65c000, 0x04028fe3, 0x86252c4e, 0x00a1bf3d,
    0x8cd43a1a, 0x317c06c9, 0x950a00e0, 0x0edb018b,
    } },
    { { /* 108 */
    0x8c20e34b, 0xf0101182, 0xa7287d94, 0x40fbc9ac,
    0x06534484, 0x44445a90, 0x00013fc8, 0xf5d40048,
    } },
    { { /* 109 */
    0xec577701, 0x891dc442, 0x49286b83, 0xd2424109,
    0x59fe061d, 0x3a221800, 0x3b9fb7e4, 0xc0eaf003,
    } },
    { { /* 110 */
    0x82021386, 0xe4008980, 0x10a1b200, 0x0cc44b80,
    0x8944d309, 0x48341faf, 0x0c458259, 0x0450420a,
    } },
    { { /* 111 */
    0x10c8a040, 0x44503140, 0x01004004, 0x05408280,
    0x442c0108, 0x1a056a30, 0x051420a6, 0x645690cf,
    } },
    { { /* 112 */
    0x31000021, 0xcbf09c18, 0x63e2a120, 0x01b5104c,
    0x9a83538c, 0x3281b8b2, 0x0a84987a, 0x0c0233e7,
    } },
    { { /* 113 */
    0x9018d4cc, 0x9070a1a1, 0xe0048a1e, 0x0451c3d4,
    0x21c2439a, 0x53104844, 0x36400292, 0xf3bd0241,
    } },
    { { /* 114 */
    0xe8f0ab09, 0xa5d27dc0, 0xd24bc242, 0xd0afa43f,
    0x34a11aa0, 0x03d88247, 0x651bc452, 0xc83ad294,
    } },
    { { /* 115 */
    0x40c8001c, 0x33140e06, 0xb21b614f, 0xc0d00088,
    0xa898a02a, 0x166ba1c5, 0x85b42e50, 0x0604c08b,
    } },
    { { /* 116 */
    0x1e04f933, 0xa251056e, 0x76380400, 0x73b8ec07,
    0x18324406, 0xc8164081, 0x63097c8a, 0xaa042980,
    } },
    { { /* 117 */
    0xca9c1c24, 0x27604e0e, 0x83000990, 0x81040046,
    0x10816011, 0x0908540d, 0xcc0a000e, 0x0c000500,
    } },
    { { /* 118 */
    0xa0440430, 0x6784008b, 0x8a195288, 0x8b18865e,
    0x41602e59, 0x9cbe8c10, 0x891c6861, 0x00089800,
    } },
    { { /* 119 */
    0x089a8100, 0x41900018, 0xe4a14007, 0x640d0505,
    0x0e4d310e, 0xff0a4806, 0x2aa81632, 0x000b852e,
    } },
    { { /* 120 */
    0xca841800, 0x696c0e20, 0x16000032, 0x03905658,
    0x1a285120, 0x11248000, 0x432618e1, 0x0eaa5d52,
    } },
    { { /* 121 */
    0xae280fa0, 0x4500fa7b, 0x89406408, 0xc044c880,
    0xb1419005, 0x24c48424, 0x603a1a34, 0xc1949000,
    } },
    { { /* 122 */
    0x003a8246, 0xc106180d, 0x99100022, 0x1511e050,
    0x00824057, 0x020a041a, 0x8930004f, 0x444ad813,
    } },
    { { /* 123 */
    0xed228a02, 0x400510c0, 0x01021000, 0x31018808,
    0x02044600, 0x0708f000, 0xa2008900, 0x22020000,
    } },
    { { /* 124 */
    0x16100200, 0x10400042, 0x02605200, 0x200052f4,
    0x80308510, 0x42021100, 0x80b54308, 0x9a2070e1,
    } },
    { { /* 125 */
    0x08012040, 0xfc653500, 0xab0419c1, 0x62140286,
    0x00440087, 0x02449085, 0x0a85405c, 0x33803207,
    } },
    { { /* 126 */
    0xb8c00400, 0xc0d0ce20, 0x0080c030, 0x0d250508,
    0x00400a90, 0x080c0200, 0x40006505, 0x41026421,
    } },
    { { /* 127 */
    0x00000268, 0x847c0024, 0xde200002, 0x40498619,
    0x40000808, 0x20010084, 0x10108400, 0x01c742cd,
    } },
    { { /* 128 */
    0xd52a7038, 0x1d8f1968, 0x3e12be50, 0x81d92ef5,
    0x2412cec4, 0x732e0828, 0x4b3424ac, 0xd41d020c,
    } },
    { { /* 129 */
    0x80002a02, 0x08110097, 0x114411c4, 0x7d451786,
    0x064949d9, 0x87914000, 0xd8c4254c, 0x491444ba,
    } },
    { { /* 130 */
    0xc8001b92, 0x15800271, 0x0c000081, 0xc200096a,
    0x40024800, 0xba493021, 0x1c802080, 0x1008e2ac,
    } },
    { { /* 131 */
    0x00341004, 0x841400e1, 0x20000020, 0x10149800,
    0x04aa70c2, 0x54208688, 0x04130c62, 0x20109180,
    } },
    { { /* 132 */
    0x02064082, 0x54001c40, 0xe4e90383, 0x84802125,
    0x2000e433, 0xe60944c0, 0x81260a03, 0x080112da,
    } },
    { { /* 133 */
    0x97906901, 0xf8864001, 0x0081e24d, 0xa6510a0e,
    0x81ec011a, 0x8441c600, 0xb62cadb8, 0x8741a46f,
    } },
    { { /* 134 */
    0x4b028d54, 0x02681161, 0x2057bb60, 0x043350a0,
    0xb7b4a8c0, 0x01122402, 0x20009ad3, 0x00c82271,
    } },
    { { /* 135 */
    0x809e2081, 0xe1800c8a, 0x8151b009, 0x40281031,
    0x89a52a0e, 0x620e69b6, 0xd1444425, 0x4d548085,
    } },
    { { /* 136 */
    0x1fb12c75, 0x862dd807, 0x4841d87c, 0x226e414e,
    0x9e088200, 0xed37f80c, 0x75268c80, 0x08149313,
    } },
    { { /* 137 */
    0xc8040e32, 0x6ea6484e, 0x66702c4a, 0xba0126c0,
    0x185dd30c, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 138 */
    0x00000000, 0x05400000, 0x81337020, 0x03a54f81,
    0x641055ec, 0x2344c318, 0x00341462, 0x1a090a43,
    } },
    { { /* 139 */
    0x13a5187b, 0xa8480102, 0xc5440440, 0xe2dd8106,
    0x2d481af0, 0x0416b626, 0x6e405058, 0x31128032,
    } },
    { { /* 140 */
    0x0c0007e4, 0x420a8208, 0x803b4840, 0x87134860,
    0x3428850d, 0xe5290319, 0x870a2345, 0x5c1825a9,
    } },
    { { /* 141 */
    0xd9c577a6, 0x03e85e00, 0xa7000081, 0x41c6cd54,
    0xa2042800, 0x2b0ab860, 0xda9e0020, 0x0e1a08ea,
    } },
    { { /* 142 */
    0x11c0427c, 0x03768908, 0x01058621, 0x18a80000,
    0xc44846a0, 0x20220d05, 0x91485422, 0x28978a01,
    } },
    { { /* 143 */
    0x00087898, 0x31221605, 0x08804240, 0x06a2fa4e,
    0x92110814, 0x9b042002, 0x06432e52, 0x90105000,
    } },
    { { /* 144 */
    0x85ba0041, 0x20203042, 0x05a04f0b, 0x40802708,
    0x1a930591, 0x0600df50, 0x3021a202, 0x4e800630,
    } },
    { { /* 145 */
    0x04c80cc4, 0x8001a004, 0xd4316000, 0x0a020880,
    0x00281c00, 0x00418e18, 0xca106ad0, 0x4b00f210,
    } },
    { { /* 146 */
    0x1506274d, 0x88900220, 0x82a85a00, 0x81504549,
    0x80002004, 0x2c088804, 0x000508d1, 0x4ac48001,
    } },
    { { /* 147 */
    0x0062e020, 0x0a42008e, 0x6a8c3055, 0xe0a5090e,
    0x42c42906, 0x80b34814, 0xb330803e, 0x731c0102,
    } },
    { { /* 148 */
    0x600d1494, 0x09400c20, 0xc040301a, 0xc094a451,
    0x05c88dca, 0xa40c96c2, 0x34040001, 0x011000c8,
    } },
    { { /* 149 */
    0xa9c9550d, 0x1c5a2428, 0x48370142, 0x100f7a4d,
    0x452a32b4, 0x9205317b, 0x5c44b894, 0x458a68d7,
    } },
    { { /* 150 */
    0x2ed15097, 0x42081943, 0x9d40d202, 0x20979840,
    0x064d5409, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 151 */
    0x00000000, 0x84800000, 0x04215542, 0x17001c06,
    0x61107624, 0xb9ddff87, 0x5c0a659f, 0x3c00245d,
    } },
    { { /* 152 */
    0x0059adb0, 0x00000000, 0x00000000, 0x009b28d0,
    0x02000422, 0x44080108, 0xac409804, 0x90288d0a,
    } },
    { { /* 153 */
    0xe0018700, 0x00310400, 0x82211794, 0x10540019,
    0x021a2cb2, 0x40039c02, 0x88043d60, 0x7900080c,
    } },
    { { /* 154 */
    0xba3c1628, 0xcb088640, 0x90807274, 0x0000001e,
    0xd8000000, 0x9c87e188, 0x04124034, 0x2791ae64,
    } },
    { { /* 155 */
    0xe6fbe86b, 0x5366408f, 0x537feea6, 0xb5e4e32b,
    0x0002869f, 0x01228548, 0x08004402, 0x20a02116,
    } },
    { { /* 156 */
    0x02040004, 0x00052000, 0x01547e00, 0x01ac162c,
    0x10852a84, 0x05308c14, 0xb943fbc3, 0x906000ca,
    } },
    { { /* 157 */
    0x40326000, 0x80901200, 0x4c810b30, 0x40020054,
    0x1d6a0029, 0x02802000, 0x00048000, 0x150c2610,
    } },
    { { /* 158 */
    0x07018040, 0x0c24d94d, 0x18502810, 0x50205001,
    0x04d01000, 0x02017080, 0x21c30108, 0x00000132,
    } },
    { { /* 159 */
    0x07190088, 0x05600802, 0x4c0e0012, 0xf0a10405,
    0x00000002, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 160 */
    0x00000000, 0x00000000, 0x00000000, 0x00800000,
    0x035a8e8d, 0x5a0421bd, 0x11703488, 0x00000026,
    } },
    { { /* 161 */
    0x10000000, 0x8804c502, 0xf801b815, 0x25ed147c,
    0x1bb0ed60, 0x1bd70589, 0x1a627af3, 0x0ac50d0c,
    } },
    { { /* 162 */
    0x524ae5d1, 0x63050490, 0x52440354, 0x16122b57,
    0x1101a872, 0x00182949, 0x10080948, 0x886c6000,
    } },
    { { /* 163 */
    0x058f916e, 0x39903012, 0x4930f840, 0x001b8880,
    0x00000000, 0x00428500, 0x98000058, 0x7014ea04,
    } },
    { { /* 164 */
    0x611d1628, 0x60005113, 0x00a71a24, 0x00000000,
    0x03c00000, 0x10187120, 0xa9270172, 0x89066004,
    } },
    { { /* 165 */
    0x020cc022, 0x40810900, 0x8ca0202d, 0x00000e34,
    0x00000000, 0x11012100, 0xc11a8011, 0x0892ec4c,
    } },
    { { /* 166 */
    0x85000040, 0x1806c7ac, 0x0512e03e, 0x00108000,
    0x80ce4008, 0x02106d01, 0x08568641, 0x0027011e,
    } },
    { { /* 167 */
    0x083d3750, 0x4e05e032, 0x048401c0, 0x01400081,
    0x00000000, 0x00000000, 0x00000000, 0x00591aa0,
    } },
    { { /* 168 */
    0x882443c8, 0xc8001d48, 0x72030152, 0x04049013,
    0x04008280, 0x0d148a10, 0x02088056, 0x2704a040,
    } },
    { { /* 169 */
    0x4c000000, 0x00000000, 0x00000000, 0xa3200000,
    0xa0ae1902, 0xdf002660, 0x7b15f010, 0x3ad08121,
    } },
    { { /* 170 */
    0x00284180, 0x48001003, 0x8014cc00, 0x00c414cf,
    0x30202000, 0x00000001, 0x00000000, 0x00000000,
    } },
    { { /* 171 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000020,
    } },
    { { /* 172 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0xffff0000, 0x0001ffff,
    } },
    { { /* 173 */
    0xffff0002, 0xffffffff, 0x0002ffff, 0x00000000,
    0x0c0c0000, 0x000cc00c, 0x03000000, 0x00000000,
    } },
    { { /* 174 */
    0x00000000, 0x00000300, 0x00000000, 0x00000300,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 175 */
    0xffff0000, 0xffffffff, 0x0040ffff, 0x00000000,
    0x0c0c0000, 0x0c00000c, 0x03000000, 0x00000300,
    } },
    { { /* 176 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x0d10646e, 0x0d10646e,
    } },
    { { /* 177 */
    0x00000000, 0x01000300, 0x00000000, 0x00000300,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 178 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xffffffff, 0xffffffff, 0x0000003f, 0x00000000,
    } },
    { { /* 179 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xfffddfec, 0xc3effdff, 0x40603ddf, 0x0000ffc3,
    } },
    { { /* 180 */
    0x00000000, 0xfffe0000, 0xffffffff, 0xffffffff,
    0x00007fff, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 181 */
    0x3eff0793, 0x1303b011, 0x11102801, 0x05930000,
    0xb0111e7b, 0x3b019703, 0x00a01112, 0x306b9593,
    } },
    { { /* 182 */
    0x1102b051, 0x11303201, 0x011102b0, 0xb879300a,
    0x30011306, 0x00800010, 0x100b0113, 0x93000011,
    } },
    { { /* 183 */
    0x00102b03, 0x05930000, 0xb051746b, 0x3b011323,
    0x00001030, 0x70000000, 0x1303b011, 0x11102900,
    } },
    { { /* 184 */
    0x00012180, 0xb0153000, 0x3001030e, 0x02000030,
    0x10230111, 0x13000000, 0x10106b81, 0x01130300,
    } },
    { { /* 185 */
    0x30111013, 0x00000100, 0x22b85530, 0x30000000,
    0x9702b011, 0x113afb07, 0x011303b0, 0x00000021,
    } },
    { { /* 186 */
    0x3b0d1b00, 0x03b01138, 0x11330113, 0x13000001,
    0x111c2b05, 0x00000100, 0xb0111000, 0x2a011300,
    } },
    { { /* 187 */
    0x02b01930, 0x10100001, 0x11000000, 0x10300301,
    0x07130230, 0x0011146b, 0x2b051300, 0x8fb8f974,
    } },
    { { /* 188 */
    0x103b0113, 0x00000000, 0xd9700000, 0x01134ab0,
    0x0011103b, 0x00001103, 0x2ab15930, 0x10000111,
    } },
    { { /* 189 */
    0x11010000, 0x00100b01, 0x01130000, 0x0000102b,
    0x20000101, 0x02a01110, 0x30210111, 0x0102b059,
    } },
    { { /* 190 */
    0x19300000, 0x011307b0, 0xb011383b, 0x00000003,
    0x00000000, 0x383b0d13, 0x0103b011, 0x00001000,
    } },
    { { /* 191 */
    0x01130000, 0x00101020, 0x00000100, 0x00000110,
    0x30000000, 0x00021811, 0x00100000, 0x01110000,
    } },
    { { /* 192 */
    0x00000023, 0x0b019300, 0x00301110, 0x302b0111,
    0x13c7b011, 0x01303b01, 0x00000280, 0xb0113000,
    } },
    { { /* 193 */
    0x2b011383, 0x03b01130, 0x300a0011, 0x1102b011,
    0x00002000, 0x01110100, 0xa011102b, 0x2b011302,
    } },
    { { /* 194 */
    0x01000010, 0x30000001, 0x13029011, 0x11302b01,
    0x000066b0, 0xb0113000, 0x6b07d302, 0x07b0113a,
    } },
    { { /* 195 */
    0x00200103, 0x13000000, 0x11386b05, 0x011303b0,
    0x000010b8, 0x2b051b00, 0x03000110, 0x10000000,
    } },
    { { /* 196 */
    0x1102a011, 0x79700a01, 0x0111a2b0, 0x0000100a,
    0x00011100, 0x00901110, 0x00090111, 0x93000000,
    } },
    { { /* 197 */
    0xf9f2bb05, 0x011322b0, 0x2001323b, 0x00000000,
    0x06b05930, 0x303b0193, 0x1123a011, 0x11700000,
    } },
    { { /* 198 */
    0x001102b0, 0x00001010, 0x03011301, 0x00000110,
    0x162b0793, 0x01010010, 0x11300000, 0x01110200,
    } },
    { { /* 199 */
    0xb0113029, 0x00000000, 0x0eb05130, 0x383b0513,
    0x0303b011, 0x00000100, 0x01930000, 0x00001039,
    } },
    { { /* 200 */
    0x3b000302, 0x00000000, 0x00230113, 0x00000000,
    0x00100000, 0x00010000, 0x90113020, 0x00000002,
    } },
    { { /* 201 */
    0x00000000, 0x10000000, 0x11020000, 0x00000301,
    0x01130000, 0xb079b02b, 0x3b011323, 0x02b01130,
    } },
    { { /* 202 */
    0xf0210111, 0x1343b0d9, 0x11303b01, 0x011103b0,
    0xb0517020, 0x20011322, 0x01901110, 0x300b0111,
    } },
    { { /* 203 */
    0x9302b011, 0x0016ab01, 0x01130100, 0xb0113021,
    0x29010302, 0x02b03130, 0x30000000, 0x1b42b819,
    } },
    { { /* 204 */
    0x11383301, 0x00000330, 0x00000020, 0x33051300,
    0x00001110, 0x00000000, 0x93000000, 0x01302305,
    } },
    { { /* 205 */
    0x00010100, 0x30111010, 0x00000100, 0x02301130,
    0x10100001, 0x11000000, 0x00000000, 0x85130200,
    } },
    { { /* 206 */
    0x10111003, 0x2b011300, 0x63b87730, 0x303b0113,
    0x11a2b091, 0x7b300201, 0x011357f0, 0xf0d1702b,
    } },
    { { /* 207 */
    0x1b0111e3, 0x0ab97130, 0x303b0113, 0x13029001,
    0x11302b01, 0x071302b0, 0x3011302b, 0x23011303,
    } },
    { { /* 208 */
    0x02b01130, 0x30ab0113, 0x11feb411, 0x71300901,
    0x05d347b8, 0xb011307b, 0x21015303, 0x00001110,
    } },
    { { /* 209 */
    0x306b0513, 0x1102b011, 0x00103301, 0x05130000,
    0xa01038eb, 0x30000102, 0x02b01110, 0x30200013,
    } },
    { { /* 210 */
    0x0102b071, 0x00101000, 0x01130000, 0x1011100b,
    0x2b011300, 0x00000000, 0x366b0593, 0x1303b095,
    } },
    { { /* 211 */
    0x01103b01, 0x00000200, 0xb0113000, 0x20000103,
    0x01000010, 0x30000000, 0x030ab011, 0x00101001,
    } },
    { { /* 212 */
    0x01110100, 0x00000003, 0x23011302, 0x03000010,
    0x10000000, 0x01000000, 0x00100000, 0x00000290,
    } },
    { { /* 213 */
    0x30113000, 0x7b015386, 0x03b01130, 0x00210151,
    0x13000000, 0x11303b01, 0x001102b0, 0x00011010,
    } },
    { { /* 214 */
    0x2b011302, 0x02001110, 0x10000000, 0x0102b011,
    0x11300100, 0x000102b0, 0x00011010, 0x2b011100,
    } },
    { { /* 215 */
    0x02101110, 0x002b0113, 0x93000000, 0x11302b03,
    0x011302b0, 0x0000303b, 0x00000002, 0x03b01930,
    } },
    { { /* 216 */
    0x102b0113, 0x0103b011, 0x11300000, 0x011302b0,
    0x00001021, 0x00010102, 0x00000010, 0x102b0113,
    } },
    { { /* 217 */
    0x01020011, 0x11302000, 0x011102b0, 0x30113001,
    0x00000002, 0x02b01130, 0x303b0313, 0x0103b011,
    } },
    { { /* 218 */
    0x00002000, 0x05130000, 0xb011303b, 0x10001102,
    0x00000110, 0x142b0113, 0x01000001, 0x01100000,
    } },
    { { /* 219 */
    0x00010280, 0xb0113000, 0x10000102, 0x00000010,
    0x10230113, 0x93021011, 0x11100b05, 0x01130030,
    } },
    { { /* 220 */
    0xb051702b, 0x3b011323, 0x00000030, 0x30000000,
    0x1303b011, 0x11102b01, 0x01010330, 0xb011300a,
    } },
    { { /* 221 */
    0x20000102, 0x00000000, 0x10000011, 0x9300a011,
    0x00102b05, 0x00000200, 0x90111000, 0x29011100,
    } },
    { { /* 222 */
    0x00b01110, 0x30000000, 0x1302b011, 0x11302b21,
    0x000103b0, 0x00000020, 0x2b051300, 0x02b01130,
    } },
    { { /* 223 */
    0x103b0113, 0x13002011, 0x11322b21, 0x00130280,
    0xa0113028, 0x0a011102, 0x02921130, 0x30210111,
    } },
    { { /* 224 */
    0x13020011, 0x11302b01, 0x03d30290, 0x3011122b,
    0x2b011302, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 225 */
    0x00000000, 0x00000000, 0x00820000, 0x00820000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 226 */
    0xffff0000, 0xffff27bf, 0x000027bf, 0x00000000,
    0x00000000, 0x0c000000, 0x03000000, 0x000000c0,
    } },
    { { /* 227 */
    0x00000000, 0x061ef5c0, 0x000001f6, 0x40000000,
    0x01040040, 0x00208210, 0x00005040, 0x00000000,
    } },
    { { /* 228 */
    0xffff0002, 0xffffffff, 0x0002ffff, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 229 */
    0xffff0042, 0xffffffff, 0x0042ffff, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x000000c0,
    } },
    { { /* 230 */
    0x00000000, 0x000c0000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 231 */
    0xffff0002, 0xffffffff, 0x0002ffff, 0x00000000,
    0x00000000, 0x0000c00c, 0x00000000, 0x00000000,
    } },
    { { /* 232 */
    0x000c0003, 0x00003c00, 0x0000f000, 0x00003c00,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 233 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x98504f14, 0x18504f14,
    } },
    { { /* 234 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x0c186606, 0x0c186606,
    } },
    { { /* 235 */
    0x0c000000, 0x00000000, 0x00000000, 0x00000000,
    0x00010040, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 236 */
    0x00000000, 0x00000000, 0x08100000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 237 */
    0x00001006, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 238 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xfef02596, 0x3bffecae, 0x33ff3f5f, 0x00000000,
    } },
    { { /* 239 */
    0x03c03030, 0x0000c000, 0x00000000, 0x600c0c03,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 240 */
    0x000c3003, 0x18c00c0c, 0x00c03060, 0x60000c03,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 241 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00100002, 0x00100002,
    } },
    { { /* 242 */
    0x00000003, 0x18000000, 0x00003060, 0x00000c00,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 243 */
    0x00000000, 0x00300000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 244 */
    0xfdffb729, 0x000001ff, 0xb7290000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 245 */
    0xfffddfec, 0xc3fffdff, 0x00803dcf, 0x0000ffc3,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 246 */
    0x00000000, 0xffffffff, 0xffffffff, 0x00ffffff,
    0xffffffff, 0x000003ff, 0x00000000, 0x00000000,
    } },
    { { /* 247 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00004004, 0x00004004,
    } },
    { { /* 248 */
    0x0f000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 249 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x02045101, 0x02045101,
    } },
    { { /* 250 */
    0x00000c00, 0x000000c3, 0x00000000, 0x18000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 251 */
    0xffffffff, 0x0007f6fb, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 252 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x011c0661, 0x011c0661,
    } },
    { { /* 253 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x1c58af16, 0x1c58af16,
    } },
    { { /* 254 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x115c0671, 0x115c0671,
    } },
    { { /* 255 */
    0x00000000, 0x00000000, 0x00000000, 0x00300000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 256 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x040c2383, 0x040c2383,
    } },
    { { /* 257 */
    0xfff99fee, 0xf3cdfdff, 0xb0c0398f, 0x0001ffc3,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 258 */
    0xfff987e0, 0xd36dfdff, 0x1e003987, 0x001f0000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 259 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00080000, 0x00080000,
    } },
    { { /* 260 */
    0x030000f0, 0x00000000, 0x0c00001e, 0x1e000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 261 */
    0x00000000, 0x07ffffde, 0x000005f6, 0x50000000,
    0x05480262, 0x10000a00, 0x00013000, 0x00000000,
    } },
    { { /* 262 */
    0x00000000, 0x07ffffde, 0x000005f6, 0x50000000,
    0x05480262, 0x10000a00, 0x00052000, 0x00000000,
    } },
    { { /* 263 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x143c278f, 0x143c278f,
    } },
    { { /* 264 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x02045301, 0x02045301,
    } },
    { { /* 265 */
    0xffff0002, 0xffffffff, 0x0002ffff, 0x00000000,
    0x00300000, 0x0c00c030, 0x03000000, 0x00000000,
    } },
    { { /* 266 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00800000, 0x00000000, 0x00000000,
    } },
    { { /* 267 */
    0x30000000, 0x00000000, 0x00000000, 0x00000000,
    0x00040000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 268 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00000002, 0x00000002,
    } },
    { { /* 269 */
    0x00033000, 0x00000000, 0x00000c00, 0x600000c3,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 270 */
    0xffff8f04, 0xffffffff, 0x8f04ffff, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 271 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xfc7fffe0, 0x2ffbffff, 0xff5f807f, 0x001c0000,
    } },
    { { /* 272 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x24182212, 0x24182212,
    } },
    { { /* 273 */
    0x0000f000, 0x66000000, 0x00300180, 0x60000033,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 274 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00408030, 0x00408030,
    } },
    { { /* 275 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00020032, 0x00020032,
    } },
    { { /* 276 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00000016, 0x00000016,
    } },
    { { /* 277 */
    0x00033000, 0x00000000, 0x00000c00, 0x60000003,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 278 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00200034, 0x00200034,
    } },
    { { /* 279 */
    0x00033000, 0x00000000, 0x00000c00, 0x60000003,
    0x00000000, 0x00800000, 0x00000000, 0x0000c3f0,
    } },
    { { /* 280 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00040000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 281 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00000880, 0x00000880,
    } },
    { { /* 282 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x10400a33, 0x10400a33,
    } },
    { { /* 283 */
    0xffff0000, 0xffff1fff, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 284 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xd63dc7e8, 0xc3bfc718, 0x00803dc7, 0x00000000,
    } },
    { { /* 285 */
    0xfffddfee, 0xc3effdff, 0x00603ddf, 0x0000ffc3,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 286 */
    0xffff0002, 0xffffffff, 0x0002ffff, 0x00000000,
    0x0c0c0000, 0x00cc0000, 0x00000000, 0x0000c00c,
    } },
    { { /* 287 */
    0xfffffffe, 0x87ffffff, 0x0fffffff, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 288 */
    0xff7fff7f, 0xff01ff00, 0x3dff3d7f, 0xffff7fff,
    0xffff0000, 0x003d7fff, 0xff7f7f3d, 0x00ff7fff,
    } },
    { { /* 289 */
    0x003d7fff, 0xffffffff, 0x007fff00, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 290 */
    0xff7fff7f, 0xff01ff00, 0x00003d7f, 0xffff7fff,
    0x00ff0000, 0x003d7f7f, 0xff7f7f00, 0x00ff7f00,
    } },
    { { /* 291 */
    0xffff0002, 0xffffffff, 0x0002ffff, 0x00000000,
    0x00c00000, 0x0000c00c, 0x03000000, 0x00000000,
    } },
    { { /* 292 */
    0x000fdfff, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 293 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00100400, 0x00100400,
    } },
    { { /* 294 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x18404084, 0x18404084,
    } },
    { { /* 295 */
    0xc0000000, 0x00030000, 0xc0000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 296 */
    0xffff0002, 0xffffffff, 0x0002ffff, 0x00000000,
    0x00c00000, 0x0c00c00c, 0x03000000, 0x00000000,
    } },
    { { /* 297 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00220008, 0x00220008,
    } },
    { { /* 298 */
    0x00000000, 0x00000300, 0x00000000, 0x00000300,
    0x00010040, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 299 */
    0x00000048, 0x00000200, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 300 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x30000000, 0x00000000, 0x03000000,
    } },
    { { /* 301 */
    0xffff00d0, 0xffffffff, 0x00d0ffff, 0x00000000,
    0x00030000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 302 */
    0x00000000, 0x00000000, 0x03c00000, 0x3c0003c0,
    0xc03fff00, 0x0000fc00, 0x00000000, 0xf0000000,
    } },
    { { /* 303 */
    0xffff0002, 0xffffe5ff, 0x0002e5ff, 0x00000000,
    0x0c0c0000, 0x000c0000, 0x00000000, 0x00030000,
    } },
    { { /* 304 */
    0x000c0000, 0x30000000, 0x00000c30, 0x00030000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 305 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x263c370f, 0x263c370f,
    } },
    { { /* 306 */
    0x0003000c, 0x00000300, 0x00000000, 0x00000300,
    0x00000000, 0x00018003, 0x00000000, 0x00000000,
    } },
    { { /* 307 */
    0x0800024f, 0x00000008, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 308 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0xffffffff, 0xffffffff, 0x03ffffff,
    } },
    { { /* 309 */
    0x00000000, 0x00000000, 0x077dfffe, 0x077dfffe,
    0x00000000, 0x00000000, 0x10400010, 0x10400010,
    } },
    { { /* 310 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x10400010, 0x10400010,
    } },
    { { /* 311 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x081047a4, 0x081047a4,
    } },
    { { /* 312 */
    0x0c0030c0, 0x00000000, 0x0f30001e, 0x66000003,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 313 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x000a0a09, 0x000a0a09,
    } },
    { { /* 314 */
    0x00000000, 0x00000000, 0x00000c00, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 315 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x00400810, 0x00400810,
    } },
    { { /* 316 */
    0x00000000, 0x00000000, 0x07fffffe, 0x07fffffe,
    0x00000000, 0x00000000, 0x0e3c770f, 0x0e3c770f,
    } },
    { { /* 317 */
    0x0c000000, 0x00000300, 0x00000018, 0x00000300,
    0x00000000, 0x00000000, 0x001fe000, 0x03000000,
    } },
    { { /* 318 */
    0x0000100f, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 319 */
    0x00000000, 0xc0000000, 0x00000000, 0x0000000c,
    0x00000000, 0x33000000, 0x00003000, 0x00000000,
    } },
    { { /* 320 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000280, 0x00000000,
    } },
    { { /* 321 */
    0x7f7b7f8b, 0xef553db4, 0xf35dfba8, 0x400b0243,
    0x8d3efb40, 0x8c2c7bf7, 0xe3fa6eff, 0xa8ed1d3a,
    } },
    { { /* 322 */
    0xcf83e602, 0x35558cf5, 0xffabe048, 0xd85992b9,
    0x2892ab18, 0x8020d7e9, 0xf583c438, 0x450ae74a,
    } },
    { { /* 323 */
    0x9714b000, 0x54007762, 0x1420d188, 0xc8c01020,
    0x00002121, 0x0c0413a8, 0x04408000, 0x082870c0,
    } },
    { { /* 324 */
    0x000408c0, 0x80000002, 0x14722b7b, 0x3bfb7924,
    0x1ae43327, 0x38ef9835, 0x28029ad1, 0xbf69a813,
    } },
    { { /* 325 */
    0x2fc665cf, 0xafc96b11, 0x5053340f, 0xa00486a2,
    0xe8090106, 0xc00e3f0f, 0x81450a88, 0xc6010010,
    } },
    { { /* 326 */
    0x26e1a161, 0xce00444b, 0xd4eec7aa, 0x85bbcadf,
    0xa5203a74, 0x8840436c, 0x8bd23f06, 0x3befff79,
    } },
    { { /* 327 */
    0xe8eff75a, 0x5b36fbcb, 0x1bfd0d49, 0x39ee0154,
    0x2e75d855, 0xa91abfd8, 0xf6bff3d7, 0xb40c67e0,
    } },
    { { /* 328 */
    0x081382c2, 0xd08bd49d, 0x1061065a, 0x59e074f2,
    0xb3128f9f, 0x6aaa0080, 0xb05e3230, 0x60ac9d7a,
    } },
    { { /* 329 */
    0xc900d303, 0x8a563098, 0x13907000, 0x18421f14,
    0x0008c060, 0x10808008, 0xec900400, 0xe6332817,
    } },
    { { /* 330 */
    0x90000758, 0x4e09f708, 0xfc83f485, 0x18c8af53,
    0x080c187c, 0x01146adf, 0xa734c80c, 0x2710a011,
    } },
    { { /* 331 */
    0x422228c5, 0x00210413, 0x41123010, 0x40001820,
    0xc60c022b, 0x10000300, 0x00220022, 0x02495810,
    } },
    { { /* 332 */
    0x9670a094, 0x1792eeb0, 0x05f2cb96, 0x23580025,
    0x42cc25de, 0x4a04cf38, 0x359f0c40, 0x8a001128,
    } },
    { { /* 333 */
    0x910a13fa, 0x10560229, 0x04200641, 0x84f00484,
    0x0c040000, 0x412c0400, 0x11541206, 0x00020a4b,
    } },
    { { /* 334 */
    0x00c00200, 0x00940000, 0xbfbb0001, 0x242b167c,
    0x7fa89bbb, 0xe3790c7f, 0xe00d10f4, 0x9f014132,
    } },
    { { /* 335 */
    0x35728652, 0xff1210b4, 0x4223cf27, 0x8602c06b,
    0x1fd33106, 0xa1aa3a0c, 0x02040812, 0x08012572,
    } },
    { { /* 336 */
    0x485040cc, 0x601062d0, 0x29001c80, 0x00109a00,
    0x22000004, 0x00800000, 0x68002020, 0x609ecbe6,
    } },
    { { /* 337 */
    0x3f73916e, 0x398260c0, 0x48301034, 0xbd5c0006,
    0xd6fb8cd1, 0x43e820e1, 0x084e0600, 0xc4d00500,
    } },
    { { /* 338 */
    0x89aa8d1f, 0x1602a6e1, 0x21ed0001, 0x1a8b3656,
    0x13a51fb7, 0x30a06502, 0x23c7b278, 0xe9226c93,
    } },
    { { /* 339 */
    0x3a74e47f, 0x98208fe3, 0x2625280e, 0xbf49bf9c,
    0xac543218, 0x1916b949, 0xb5220c60, 0x0659fbc1,
    } },
    { { /* 340 */
    0x8420e343, 0x800008d9, 0x20225500, 0x00a10184,
    0x20104800, 0x40801380, 0x00160d04, 0x80200040,
    } },
    { { /* 341 */
    0x8de7fd40, 0xe0985436, 0x091e7b8b, 0xd249fec8,
    0x8dee0611, 0xba221937, 0x9fdd77f4, 0xf0daf3ec,
    } },
    { { /* 342 */
    0xec424386, 0x26048d3f, 0xc021fa6c, 0x0cc2628e,
    0x0145d785, 0x559977ad, 0x4045e250, 0xa154260b,
    } },
    { { /* 343 */
    0x58199827, 0xa4103443, 0x411405f2, 0x07002280,
    0x426600b4, 0x15a17210, 0x41856025, 0x00000054,
    } },
    { { /* 344 */
    0x01040201, 0xcb70c820, 0x6a629320, 0x0095184c,
    0x9a8b1880, 0x3201aab2, 0x00c4d87a, 0x04c3f3e5,
    } },
    { { /* 345 */
    0xa238d44d, 0x5072a1a1, 0x84fc980a, 0x44d1c152,
    0x20c21094, 0x42104180, 0x3a000000, 0xd29d0240,
    } },
    { { /* 346 */
    0xa8b12f01, 0x2432bd40, 0xd04bd34d, 0xd0ada723,
    0x75a10a92, 0x01e9adac, 0x771f801a, 0xa01b9225,
    } },
    { { /* 347 */
    0x20cadfa1, 0x738c0602, 0x003b577f, 0x00d00bff,
    0x0088806a, 0x0029a1c4, 0x05242a05, 0x16234009,
    } },
    { { /* 348 */
    0x80056822, 0xa2112011, 0x64900004, 0x13824849,
    0x193023d5, 0x08922980, 0x88115402, 0xa0042001,
    } },
    { { /* 349 */
    0x81800400, 0x60228502, 0x0b010090, 0x12020022,
    0x00834011, 0x00001a01, 0x00000000, 0x00000000,
    } },
    { { /* 350 */
    0x00000000, 0x4684009f, 0x020012c8, 0x1a0004fc,
    0x0c4c2ede, 0x80b80402, 0x0afca826, 0x22288c02,
    } },
    { { /* 351 */
    0x8f7ba0e0, 0x2135c7d6, 0xf8b106c7, 0x62550713,
    0x8a19936e, 0xfb0e6efa, 0x48f91630, 0x7debcd2f,
    } },
    { { /* 352 */
    0x4e845892, 0x7a2e4ca0, 0x561eedea, 0x1190c649,
    0xe83a5324, 0x8124cfdb, 0x634218f1, 0x1a8a5853,
    } },
    { { /* 353 */
    0x24d37420, 0x0514aa3b, 0x89586018, 0xc0004800,
    0x91018268, 0x2cd684a4, 0xc4ba8886, 0x02100377,
    } },
    { { /* 354 */
    0x00388244, 0x404aae11, 0x510028c0, 0x15146044,
    0x10007310, 0x02480082, 0x40060205, 0x0000c003,
    } },
    { { /* 355 */
    0x0c020000, 0x02200008, 0x40009000, 0xd161b800,
    0x32744621, 0x3b8af800, 0x8b00050f, 0x2280bbd0,
    } },
    { { /* 356 */
    0x07690600, 0x00438040, 0x50005420, 0x250c41d0,
    0x83108410, 0x02281101, 0x00304008, 0x020040a1,
    } },
    { { /* 357 */
    0x20000040, 0xabe31500, 0xaa443180, 0xc624c2c6,
    0x8004ac13, 0x03d1b000, 0x4285611e, 0x1d9ff303,
    } },
    { { /* 358 */
    0x78e8440a, 0xc3925e26, 0x00852000, 0x4000b001,
    0x88424a90, 0x0c8dca04, 0x4203a705, 0x000422a1,
    } },
    { { /* 359 */
    0x0c018668, 0x10795564, 0xdea00002, 0x40c12000,
    0x5001488b, 0x04000380, 0x50040000, 0x80d0c05d,
    } },
    { { /* 360 */
    0x970aa010, 0x4dafbb20, 0x1e10d921, 0x83140460,
    0xa6d68848, 0x733fd83b, 0x497427bc, 0x92130ddc,
    } },
    { { /* 361 */
    0x8ba1142b, 0xd1392e75, 0x50503009, 0x69008808,
    0x024a49d4, 0x80164010, 0x89d7e564, 0x5316c020,
    } },
    { { /* 362 */
    0x86002b92, 0x15e0a345, 0x0c03008b, 0xe200196e,
    0x80067031, 0xa82916a5, 0x18802000, 0xe1487aac,
    } },
    { { /* 363 */
    0xb5d63207, 0x5f9132e8, 0x20e550a1, 0x10807c00,
    0x9d8a7280, 0x421f00aa, 0x02310e22, 0x04941100,
    } },
    { { /* 364 */
    0x40080022, 0x5c100010, 0xfcc80343, 0x0580a1a5,
    0x04008433, 0x6e080080, 0x81262a4b, 0x2901aad8,
    } },
    { { /* 365 */
    0x4490684d, 0xba880009, 0x00820040, 0x87d10000,
    0xb1e6215b, 0x80083161, 0xc2400800, 0xa600a069,
    } },
    { { /* 366 */
    0x4a328d58, 0x550a5d71, 0x2d579aa0, 0x4aa64005,
    0x30b12021, 0x01123fc6, 0x260a10c2, 0x50824462,
    } },
    { { /* 367 */
    0x80409880, 0x810004c0, 0x00002003, 0x38180000,
    0xf1a60200, 0x720e4434, 0x92e035a2, 0x09008101,
    } },
    { { /* 368 */
    0x00000400, 0x00008885, 0x00000000, 0x00804000,
    0x00000000, 0x00004040, 0x00000000, 0x00000000,
    } },
    { { /* 369 */
    0x00000000, 0x08000000, 0x00000082, 0x00000000,
    0x88000004, 0xe7efbfff, 0xffbfffff, 0xfdffefef,
    } },
    { { /* 370 */
    0xbffefbff, 0x057fffff, 0x85b30034, 0x42164706,
    0xe4105402, 0xb3058092, 0x81305422, 0x180b4263,
    } },
    { { /* 371 */
    0x13f5387b, 0xa9ea07e5, 0x05143c4c, 0x80020600,
    0xbd481ad9, 0xf496ee37, 0x7ec0705f, 0x355fbfb2,
    } },
    { { /* 372 */
    0x455fe644, 0x41469000, 0x063b1d40, 0xfe1362a1,
    0x39028505, 0x0c080548, 0x0000144f, 0x58183488,
    } },
    { { /* 373 */
    0xd8153077, 0x4bfbbd0e, 0x85008a90, 0xe61dc100,
    0xb386ed14, 0x639bff72, 0xd9befd92, 0x0a92887b,
    } },
    { { /* 374 */
    0x1cb2d3fe, 0x177ab980, 0xdc1782c9, 0x3980fffb,
    0x590c4260, 0x37df0f01, 0xb15094a3, 0x23070623,
    } },
    { { /* 375 */
    0x3102f85a, 0x310201f0, 0x1e820040, 0x056a3a0a,
    0x12805b84, 0xa7148002, 0xa04b2612, 0x90011069,
    } },
    { { /* 376 */
    0x848a1000, 0x3f801802, 0x42400708, 0x4e140110,
    0x180080b0, 0x0281c510, 0x10298202, 0x88000210,
    } },
    { { /* 377 */
    0x00420020, 0x11000280, 0x4413e000, 0xfe025804,
    0x30283c07, 0x04739798, 0xcb13ced1, 0x431f6210,
    } },
    { { /* 378 */
    0x55ac278d, 0xc892422e, 0x02885380, 0x78514039,
    0x8088292c, 0x2428b900, 0x080e0c41, 0x42004421,
    } },
    { { /* 379 */
    0x08680408, 0x12040006, 0x02903031, 0xe0855b3e,
    0x10442936, 0x10822814, 0x83344266, 0x531b013c,
    } },
    { { /* 380 */
    0x0e0d0404, 0x00510c22, 0xc0000012, 0x88000040,
    0x0000004a, 0x00000000, 0x5447dff6, 0x00088868,
    } },
    { { /* 381 */
    0x00000081, 0x40000000, 0x00000100, 0x02000000,
    0x00080600, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 382 */
    0x00000080, 0x00000040, 0x00000000, 0x00001040,
    0x00000000, 0xf7fdefff, 0xfffeff7f, 0xfffffbff,
    } },
    { { /* 383 */
    0xbffffdff, 0x00ffffff, 0x042012c2, 0x07080c06,
    0x01101624, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 384 */
    0xe0000000, 0xfffffffe, 0x7f79ffff, 0x00f928df,
    0x80120c32, 0xd53a0008, 0xecc2d858, 0x2fa89d18,
    } },
    { { /* 385 */
    0xe0109620, 0x2622d60c, 0x02060f97, 0x9055b240,
    0x501180a2, 0x04049800, 0x00004000, 0x00000000,
    } },
    { { /* 386 */
    0x00000000, 0x00000000, 0x00000000, 0xfffffbc0,
    0xdffbeffe, 0x62430b08, 0xfb3b41b6, 0x23896f74,
    } },
    { { /* 387 */
    0xecd7ae7f, 0x5960e047, 0x098fa096, 0xa030612c,
    0x2aaa090d, 0x4f7bd44e, 0x388bc4b2, 0x6110a9c6,
    } },
    { { /* 388 */
    0x42000014, 0x0202800c, 0x6485fe48, 0xe3f7d63e,
    0x0c073aa0, 0x0430e40c, 0x1002f680, 0x00000000,
    } },
    { { /* 389 */
    0x00000000, 0x00000000, 0x00000000, 0x00100000,
    0x00004000, 0x00004000, 0x00000100, 0x00000000,
    } },
    { { /* 390 */
    0x00000000, 0x40000000, 0x00000000, 0x00000400,
    0x00008000, 0x00000000, 0x00400400, 0x00000000,
    } },
    { { /* 391 */
    0x00000000, 0x40000000, 0x00000000, 0x00000800,
    0xfebdffe0, 0xffffffff, 0xfbe77f7f, 0xf7ffffbf,
    } },
    { { /* 392 */
    0xefffffff, 0xdff7ff7e, 0xfbdff6f7, 0x804fbffe,
    0x00000000, 0x00000000, 0x00000000, 0x7fffef00,
    } },
    { { /* 393 */
    0xb6f7ff7f, 0xb87e4406, 0x88313bf5, 0x00f41796,
    0x1391a960, 0x72490080, 0x0024f2f3, 0x42c88701,
    } },
    { { /* 394 */
    0x5048e3d3, 0x43052400, 0x4a4c0000, 0x10580227,
    0x01162820, 0x0014a809, 0x00000000, 0x00683ec0,
    } },
    { { /* 395 */
    0x00000000, 0x00000000, 0x00000000, 0xffe00000,
    0xfddbb7ff, 0x000000f7, 0xc72e4000, 0x00000180,
    } },
    { { /* 396 */
    0x00012000, 0x00004000, 0x00300000, 0xb4f7ffa8,
    0x03ffadf3, 0x00000120, 0x00000000, 0x00000000,
    } },
    { { /* 397 */
    0x00000000, 0x00000000, 0x00000000, 0xfffbf000,
    0xfdcf9df7, 0x15c301bf, 0x810a1827, 0x0a00a842,
    } },
    { { /* 398 */
    0x80088108, 0x18048008, 0x0012a3be, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 399 */
    0x00000000, 0x00000000, 0x00000000, 0x90000000,
    0xdc3769e6, 0x3dff6bff, 0xf3f9fcf8, 0x00000004,
    } },
    { { /* 400 */
    0x80000000, 0xe7eebf6f, 0x5da2dffe, 0xc00b3fd8,
    0xa00c0984, 0x69100040, 0xb912e210, 0x5a0086a5,
    } },
    { { /* 401 */
    0x02896800, 0x6a809005, 0x00030010, 0x80000000,
    0x8e001ff9, 0x00000001, 0x00000000, 0x00000000,
    } },
    { { /* 402 */
    0x14000010, 0xa0c09512, 0x0c000200, 0x01000400,
    0x050002a3, 0x98800009, 0x00004000, 0x01004c00,
    } },
    { { /* 403 */
    0x04800008, 0x02840300, 0x00000824, 0x00040000,
    0x00000400, 0x20010904, 0x00001100, 0x22050030,
    } },
    { { /* 404 */
    0x00000108, 0x08104000, 0x01400000, 0x00001040,
    0x00010102, 0x10000040, 0x82012000, 0x10100002,
    } },
    { { /* 405 */
    0x00006002, 0x00000800, 0x00400000, 0x02100401,
    0x14400144, 0x013c4980, 0x880e8288, 0x50102014,
    } },
    { { /* 406 */
    0x80000824, 0x101000c1, 0x02800000, 0x02080101,
    0x04118000, 0x02200112, 0x00031020, 0x02000003,
    } },
    { { /* 407 */
    0x00000002, 0x090c1090, 0xa0004004, 0x60102290,
    0x00080000, 0x00414f45, 0x07071026, 0x40c00001,
    } },
    { { /* 408 */
    0x04580000, 0x0014800a, 0x00002800, 0x00002600,
    0x50988020, 0x02140018, 0x04013800, 0x00008008,
    } },
    { { /* 409 */
    0x41082004, 0x80000928, 0x20080280, 0x020e0a00,
    0x00010040, 0x16110200, 0x41800002, 0x08231400,
    } },
    { { /* 410 */
    0x40020020, 0x0080202f, 0x2015a008, 0x1c000002,
    0xc0040e00, 0x82028012, 0x00400000, 0x2002a004,
    } },
    { { /* 411 */
    0x20200001, 0xa0040000, 0x8890004c, 0xc4000080,
    0x10012500, 0x48100482, 0x60800110, 0x40008040,
    } },
    { { /* 412 */
    0x00040008, 0x04000044, 0x90000091, 0x000c1200,
    0x06040000, 0x08610480, 0x10010800, 0x080d0001,
    } },
    { { /* 413 */
    0x800204b4, 0x00140000, 0x00000000, 0x00200020,
    0x84100200, 0x01811000, 0x02000210, 0x03018800,
    } },
    { { /* 414 */
    0x04042804, 0x20001c92, 0x02100020, 0x4202490a,
    0x02420146, 0x00000803, 0x0008c008, 0x44050010,
    } },
    { { /* 415 */
    0x80222000, 0x00000800, 0x00008452, 0x10502140,
    0xe0410005, 0x00000400, 0x00a00008, 0x80080000,
    } },
    { { /* 416 */
    0x50180020, 0x00000009, 0x40080600, 0x00000000,
    0x56000020, 0x04000000, 0x00020006, 0x00208220,
    } },
    { { /* 417 */
    0x01210000, 0x40009000, 0x08c00140, 0x08110000,
    0x00004820, 0x02400810, 0x08800002, 0x00200000,
    } },
    { { /* 418 */
    0x00040a00, 0x00004000, 0x40000104, 0x84000000,
    0x02040048, 0x20000000, 0x00012000, 0x1b100000,
    } },
    { { /* 419 */
    0x00007000, 0x04000020, 0x10032000, 0x0804000a,
    0x00000008, 0x04020090, 0x88000014, 0x00000000,
    } },
    { { /* 420 */
    0x00000000, 0x08020008, 0x00040400, 0x40a00000,
    0x40000000, 0x00080090, 0x40800000, 0x20000388,
    } },
    { { /* 421 */
    0x02001080, 0x20010004, 0x12010004, 0x20008011,
    0x13200082, 0x02800000, 0x04098001, 0x00000004,
    } },
    { { /* 422 */
    0x00000000, 0x02801000, 0x00001000, 0x00000100,
    0x20010024, 0x00000050, 0x80200028, 0x00000020,
    } },
    { { /* 423 */
    0x01000000, 0x00a24000, 0x00000000, 0x82001010,
    0x00000800, 0x02000000, 0x40020002, 0x59000044,
    } },
    { { /* 424 */
    0x00000080, 0x0d040000, 0x04000000, 0x10020000,
    0x00022000, 0x00508000, 0x20080001, 0x000004a2,
    } },
    { { /* 425 */
    0xc0020400, 0x00310000, 0x80002000, 0x00002800,
    0x00000b60, 0x40200000, 0x00120000, 0x80000009,
    } },
    { { /* 426 */
    0x41000000, 0x00010008, 0x00880910, 0x20080888,
    0x04044020, 0x80482010, 0x00006000, 0x00020000,
    } },
    { { /* 427 */
    0x42405004, 0x00400020, 0x00000010, 0x00000886,
    0x00008000, 0x80021011, 0x00c00000, 0x42000000,
    } },
    { { /* 428 */
    0x4801201f, 0x40c00004, 0x20600480, 0x00000020,
    0x01000110, 0x22400040, 0x00000428, 0x00000000,
    } },
    { { /* 429 */
    0x0f00020f, 0x40401000, 0x00200048, 0x000c0092,
    0x81000421, 0x00040004, 0x00620001, 0x06000202,
    } },
    { { /* 430 */
    0x14001808, 0x00083800, 0x008c1028, 0x04120028,
    0x22008404, 0x40260880, 0x01100700, 0x00400000,
    } },
    { { /* 431 */
    0x20000020, 0x00200000, 0x00840000, 0x04108000,
    0x00000002, 0x00000000, 0x10000002, 0x04000402,
    } },
    { { /* 432 */
    0x10000000, 0x26a20000, 0x05000200, 0x82204000,
    0x80000000, 0x00048404, 0x80004800, 0x80000400,
    } },
    { { /* 433 */
    0x00000064, 0x00000050, 0x18804000, 0x00060000,
    0x00408002, 0x02020030, 0x00000000, 0x40000000,
    } },
    { { /* 434 */
    0x01208414, 0x00000600, 0x02018000, 0x10400000,
    0x04000840, 0x09200000, 0x2e000000, 0x04000304,
    } },
    { { /* 435 */
    0x00c01810, 0x20100010, 0x10400010, 0x02100000,
    0xa0000402, 0x48200000, 0x06080000, 0x01400000,
    } },
    { { /* 436 */
    0x40000008, 0x00001000, 0x10112800, 0xc2a09080,
    0x00008a02, 0x3a0000e9, 0x80611011, 0x40220000,
    } },
    { { /* 437 */
    0x20000020, 0x48381a00, 0x00028421, 0x54ea0800,
    0x01425100, 0x0490200c, 0x20020000, 0x00600800,
    } },
    { { /* 438 */
    0x00e0c201, 0x00004810, 0x10a10001, 0x00000040,
    0x80108084, 0x00042000, 0x00002000, 0x00000004,
    } },
    { { /* 439 */
    0x00010014, 0x03005d00, 0x00008102, 0x00120000,
    0x51009000, 0x04000480, 0x0021c200, 0x0a888056,
    } },
    { { /* 440 */
    0xd2b60004, 0x13800000, 0x204803a8, 0x04501921,
    0x0a003004, 0x02100010, 0x00091100, 0x01070080,
    } },
    { { /* 441 */
    0x42004020, 0x08300000, 0x002a2444, 0x04046081,
    0x40046008, 0x00120000, 0x10000108, 0x00000000,
    } },
    { { /* 442 */
    0x00000084, 0x08001000, 0x0012e001, 0x045880c0,
    0x00010000, 0x00800022, 0x02401000, 0x00000000,
    } },
    { { /* 443 */
    0x4000d000, 0x00000850, 0x01000009, 0x0d840000,
    0x01080000, 0x42008000, 0x20000828, 0x40100040,
    } },
    { { /* 444 */
    0x51000100, 0x32000000, 0x001a0894, 0x04000040,
    0x00002102, 0x03428000, 0x018c0080, 0x00234010,
    } },
    { { /* 445 */
    0x00000040, 0x185c4000, 0x03000000, 0x40020004,
    0xa20200c9, 0x00000220, 0x00101050, 0x00120004,
    } },
    { { /* 446 */
    0x00000040, 0x44002400, 0x00000228, 0x20000020,
    0x000a0008, 0x18010000, 0x3c08830c, 0x40000684,
    } },
    { { /* 447 */
    0x80101800, 0x02000280, 0x0020000c, 0x08009004,
    0x00040000, 0x0004000c, 0x00018000, 0x14001000,
    } },
    { { /* 448 */
    0x08240000, 0x00200000, 0x20420014, 0x58112000,
    0x10004048, 0x010050c0, 0x0408228c, 0x12282040,
    } },
    { { /* 449 */
    0x00000000, 0x00000020, 0x24002000, 0x00000000,
    0x00800a00, 0x00080910, 0x1019a000, 0x60200030,
    } },
    { { /* 450 */
    0x00000080, 0x00000080, 0x08000000, 0x800050a0,
    0x80044000, 0x04001010, 0x80008080, 0x00000000,
    } },
    { { /* 451 */
    0x00000040, 0x00800000, 0x000c4283, 0x01020000,
    0x00888000, 0x00104008, 0x20000000, 0x04000080,
    } },
    { { /* 452 */
    0x20000104, 0x1802c021, 0x08100000, 0x0000004e,
    0x80000001, 0x30c00080, 0x00000040, 0x00401200,
    } },
    { { /* 453 */
    0x04945288, 0x00940400, 0x06400104, 0x10002000,
    0x00080010, 0x00400420, 0x00000102, 0x00408010,
    } },
    { { /* 454 */
    0x05000000, 0x40002240, 0x00100000, 0x0e400024,
    0x00000080, 0x80000440, 0x01018410, 0xb1804004,
    } },
    { { /* 455 */
    0x25000800, 0x20000000, 0x00800000, 0x0000804c,
    0x10020020, 0x42001000, 0x00082000, 0x00002000,
    } },
    { { /* 456 */
    0x11500020, 0x40004053, 0x11280500, 0x80060014,
    0x004c0101, 0x60002008, 0x44000000, 0x01000036,
    } },
    { { /* 457 */
    0x00010028, 0x01180000, 0x84041804, 0x00098000,
    0x00800000, 0x00000000, 0x00400002, 0x10004001,
    } },
    { { /* 458 */
    0x0051a004, 0x00008100, 0x00000024, 0x40041000,
    0x00040000, 0x00042001, 0x00000000, 0x00008000,
    } },
    { { /* 459 */
    0x00000000, 0x00000000, 0x00000000, 0x20030000,
    0x00001840, 0x00020220, 0x04404002, 0x00204000,
    } },
    { { /* 460 */
    0x01008010, 0x00002080, 0x40008064, 0x00004031,
    0x10018090, 0x80304001, 0x000080a0, 0x80200040,
    } },
    { { /* 461 */
    0x00000001, 0x00000010, 0x00102088, 0x00800020,
    0x00120681, 0x100002a0, 0x00000042, 0x00000080,
    } },
    { { /* 462 */
    0x10000000, 0x21000a00, 0x00000200, 0x40000080,
    0x10110000, 0x00108200, 0x04000000, 0x00000400,
    } },
    { { /* 463 */
    0x80001000, 0x80002000, 0x40003008, 0x00000204,
    0x0801000a, 0x40000001, 0x00000000, 0x00000004,
    } },
    { { /* 464 */
    0x00000000, 0x00000000, 0x00020000, 0x00000000,
    0x88000000, 0x00002000, 0x08502000, 0x00840a00,
    } },
    { { /* 465 */
    0x31061808, 0x00000000, 0x00000000, 0x04000000,
    0x00000004, 0x00000240, 0x00100009, 0x00000000,
    } },
    { { /* 466 */
    0x00004002, 0x04002500, 0x00008040, 0x40a20100,
    0x00000001, 0x12412080, 0x04004008, 0x00042014,
    } },
    { { /* 467 */
    0x02000000, 0x00012000, 0x10000402, 0x000040c0,
    0x00080000, 0x5fe800a1, 0x04019402, 0x02000000,
    } },
    { { /* 468 */
    0x00040100, 0x00880000, 0x00401000, 0x00001012,
    0x00000000, 0x08004100, 0x00000010, 0x00000000,
    } },
    { { /* 469 */
    0x00000000, 0x00000000, 0x52020000, 0x10410080,
    0x00005000, 0x08400200, 0x80400010, 0x44400020,
    } },
    { { /* 470 */
    0x00084100, 0x10200d02, 0xa1200012, 0x00804804,
    0x00008212, 0xc6024000, 0x08100000, 0x205c1828,
    } },
    { { /* 471 */
    0x00000088, 0x00031000, 0x8000013f, 0x21184b44,
    0x100100f2, 0xa9002001, 0x08080840, 0x001b0001,
    } },
    { { /* 472 */
    0x28800112, 0x400020f0, 0x0910200c, 0x0a0010a0,
    0x80000020, 0x00000004, 0x1000000a, 0x00400000,
    } },
    { { /* 473 */
    0x00000000, 0x00002000, 0x00000080, 0x81000000,
    0x02c00020, 0x000004c5, 0x00000000, 0x00100100,
    } },
    { { /* 474 */
    0x20000000, 0x01080000, 0x00400022, 0x08000200,
    0x00408002, 0x20400028, 0x00000000, 0x00100000,
    } },
    { { /* 475 */
    0x08000008, 0x00420002, 0xa0a20003, 0x00022000,
    0x88000280, 0x65160000, 0x00040105, 0x00244041,
    } },
    { { /* 476 */
    0x80300000, 0x00184008, 0x00000880, 0x00201140,
    0x00000000, 0x02900000, 0x50004588, 0x00221043,
    } },
    { { /* 477 */
    0x12004000, 0x0b800000, 0x20002405, 0x0000000c,
    0x08000000, 0x11000410, 0x04000030, 0x00200043,
    } },
    { { /* 478 */
    0x80011000, 0x18008042, 0x11000000, 0x00001008,
    0x00008000, 0x24440000, 0x00800000, 0x80100005,
    } },
    { { /* 479 */
    0x00108204, 0x02102400, 0x00010001, 0x80000200,
    0xa080e80a, 0x00010000, 0x20008000, 0x80122200,
    } },
    { { /* 480 */
    0x88211404, 0x04208041, 0x20088020, 0x18040000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 481 */
    0x00401004, 0x02100002, 0x40014210, 0x50006000,
    0x00080008, 0x20000820, 0x00100211, 0x10000000,
    } },
    { { /* 482 */
    0x91005400, 0x00000000, 0x00000000, 0x08000000,
    0x41610032, 0xa0029d44, 0x000000d2, 0x41020004,
    } },
    { { /* 483 */
    0x00800104, 0x020000c0, 0x04090030, 0x80000204,
    0x82004000, 0x00000020, 0x00000000, 0x00000000,
    } },
    { { /* 484 */
    0x00000000, 0x00000000, 0x00000080, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 485 */
    0xc373ff8b, 0x1b0f6840, 0xf34ce9ac, 0xc0080200,
    0xca3e795c, 0x06487976, 0xf7f02fdf, 0xa8ff033a,
    } },
    { { /* 486 */
    0x233fef37, 0xfd59b004, 0xfffff3ca, 0xfff9de9f,
    0x7df7abff, 0x8eecc000, 0xffdbeebf, 0x45fad003,
    } },
    { { /* 487 */
    0xdffefae1, 0x10abbfef, 0xfcaaffeb, 0x24fdef3f,
    0x7f7678ad, 0xedfff00c, 0x2cfacff6, 0xeb6bf7f9,
    } },
    { { /* 488 */
    0x95bf1ffd, 0xbfbf6677, 0xfeb43bfb, 0x11e27bae,
    0x41bea681, 0x72c31435, 0x71917d70, 0x276b0003,
    } },
    { { /* 489 */
    0x70cf57cb, 0x0def4732, 0xfc747eda, 0xbdb4fe06,
    0x8bca3f9f, 0x58007e49, 0xebec228f, 0xddbb8a5c,
    } },
    { { /* 490 */
    0xb6e7ef60, 0xf293a40f, 0x549e37bb, 0x9bafd04b,
    0xf7d4c414, 0x0a1430b0, 0x88d02f08, 0x192fff7e,
    } },
    { { /* 491 */
    0xfb07ffda, 0x7beb7ff1, 0x0010c5ef, 0xfdff99ff,
    0x056779d7, 0xfdcbffe7, 0x4040c3ff, 0xbd8e6ff7,
    } },
    { { /* 492 */
    0x0497dffa, 0x5bfff4c0, 0xd0e7ed7b, 0xf8e0047e,
    0xb73eff9f, 0x882e7dfe, 0xbe7ffffd, 0xf6c483fe,
    } },
    { { /* 493 */
    0xb8fdf357, 0xef7dd680, 0x47885767, 0xc3dfff7d,
    0x37a9f0ff, 0x70fc7de0, 0xec9a3f6f, 0x86814cb3,
    } },
    { { /* 494 */
    0xdd5c3f9e, 0x4819f70d, 0x0007fea3, 0x38ffaf56,
    0xefb8980d, 0xb760403d, 0x9035d8ce, 0x3fff72bf,
    } },
    { { /* 495 */
    0x7a117ff7, 0xabfff7bb, 0x6fbeff00, 0xfe72a93c,
    0xf11bcfef, 0xf40adb6b, 0xef7ec3e6, 0xf6109b9c,
    } },
    { { /* 496 */
    0x16f4f048, 0x5182feb5, 0x15bbc7b1, 0xfbdf6e87,
    0x63cde43f, 0x7e7ec1ff, 0x7d5ffdeb, 0xfcfe777b,
    } },
    { { /* 497 */
    0xdbea960b, 0x53e86229, 0xfdef37df, 0xbd8136f5,
    0xfcbddc18, 0xffffd2e4, 0xffe03fd7, 0xabf87f6f,
    } },
    { { /* 498 */
    0x6ed99bae, 0xf115f5fb, 0xbdfb79a9, 0xadaf5a3c,
    0x1facdbba, 0x837971fc, 0xc35f7cf7, 0x0567dfff,
    } },
    { { /* 499 */
    0x8467ff9a, 0xdf8b1534, 0x3373f9f3, 0x5e1af7bd,
    0xa03fbf40, 0x01ebffff, 0xcfdddfc0, 0xabd37500,
    } },
    { { /* 500 */
    0xeed6f8c3, 0xb7ff43fd, 0x42275eaf, 0xf6869bac,
    0xf6bc27d7, 0x35b7f787, 0xe176aacd, 0xe29f49e7,
    } },
    { { /* 501 */
    0xaff2545c, 0x61d82b3f, 0xbbb8fc3b, 0x7b7dffcf,
    0x1ce0bf95, 0x43ff7dfd, 0xfffe5ff6, 0xc4ced3ef,
    } },
    { { /* 502 */
    0xadbc8db6, 0x11eb63dc, 0x23d0df59, 0xf3dbbeb4,
    0xdbc71fe7, 0xfae4ff63, 0x63f7b22b, 0xadbaed3b,
    } },
    { { /* 503 */
    0x7efffe01, 0x02bcfff7, 0xef3932ff, 0x8005fffc,
    0xbcf577fb, 0xfff7010d, 0xbf3afffb, 0xdfff0057,
    } },
    { { /* 504 */
    0xbd7def7b, 0xc8d4db88, 0xed7cfff3, 0x56ff5dee,
    0xac5f7e0d, 0xd57fff96, 0xc1403fee, 0xffe76ff9,
    } },
    { { /* 505 */
    0x8e77779b, 0xe45d6ebf, 0x5f1f6fcf, 0xfedfe07f,
    0x01fed7db, 0xfb7bff00, 0x1fdfffd4, 0xfffff800,
    } },
    { { /* 506 */
    0x007bfb8f, 0x7f5cbf00, 0x07f3ffff, 0x3de7eba0,
    0xfbd7f7bf, 0x6003ffbf, 0xbfedfffd, 0x027fefbb,
    } },
    { { /* 507 */
    0xddfdfe40, 0xe2f9fdff, 0xfb1f680b, 0xaffdfbe3,
    0xf7ed9fa4, 0xf80f7a7d, 0x0fd5eebe, 0xfd9fbb5d,
    } },
    { { /* 508 */
    0x3bf9f2db, 0xebccfe7f, 0x73fa876a, 0x9ffc95fc,
    0xfaf7109f, 0xbbcdddb7, 0xeccdf87e, 0x3c3ff366,
    } },
    { { /* 509 */
    0xb03ffffd, 0x067ee9f7, 0xfe0696ae, 0x5fd7d576,
    0xa3f33fd1, 0x6fb7cf07, 0x7f449fd1, 0xd3dd7b59,
    } },
    { { /* 510 */
    0xa9bdaf3b, 0xff3a7dcf, 0xf6ebfbe0, 0xffffb401,
    0xb7bf7afa, 0x0ffdc000, 0xff1fff7f, 0x95fffefc,
    } },
    { { /* 511 */
    0xb5dc0000, 0x3f3eef63, 0x001bfb7f, 0xfbf6e800,
    0xb8df9eef, 0x003fff9f, 0xf5ff7bd0, 0x3fffdfdb,
    } },
    { { /* 512 */
    0x00bffdf0, 0xbbbd8420, 0xffdedf37, 0x0ff3ff6d,
    0x5efb604c, 0xfafbfffb, 0x0219fe5e, 0xf9de79f4,
    } },
    { { /* 513 */
    0xebfaa7f7, 0xff3401eb, 0xef73ebd3, 0xc040afd7,
    0xdcff72bb, 0x2fd8f17f, 0xfe0bb8ec, 0x1f0bdda3,
    } },
    { { /* 514 */
    0x47cf8f1d, 0xffdeb12b, 0xda737fee, 0xcbc424ff,
    0xcbf2f75d, 0xb4edecfd, 0x4dddbff9, 0xfb8d99dd,
    } },
    { { /* 515 */
    0xaf7bbb7f, 0xc959ddfb, 0xfab5fc4f, 0x6d5fafe3,
    0x3f7dffff, 0xffdb7800, 0x7effb6ff, 0x022ffbaf,
    } },
    { { /* 516 */
    0xefc7ff9b, 0xffffffa5, 0xc7000007, 0xfff1f7ff,
    0x01bf7ffd, 0xfdbcdc00, 0xffffbff5, 0x3effff7f,
    } },
    { { /* 517 */
    0xbe000029, 0xff7ff9ff, 0xfd7e6efb, 0x039ecbff,
    0xfbdde300, 0xf6dfccff, 0x117fffff, 0xfbf6f800,
    } },
    { { /* 518 */
    0xd73ce7ef, 0xdfeffeef, 0xedbfc00b, 0xfdcdfedf,
    0x40fd7bf5, 0xb75fffff, 0xf930ffdf, 0xdc97fbdf,
    } },
    { { /* 519 */
    0xbff2fef3, 0xdfbf8fdf, 0xede6177f, 0x35530f7f,
    0x877e447c, 0x45bbfa12, 0x779eede0, 0xbfd98017,
    } },
    { { /* 520 */
    0xde897e55, 0x0447c16f, 0xf75d7ade, 0x290557ff,
    0xfe9586f7, 0xf32f97b3, 0x9f75cfff, 0xfb1771f7,
    } },
    { { /* 521 */
    0xee1934ee, 0xef6137cc, 0xef4c9fd6, 0xfbddd68f,
    0x6def7b73, 0xa431d7fe, 0x97d75e7f, 0xffd80f5b,
    } },
    { { /* 522 */
    0x7bce9d83, 0xdcff22ec, 0xef87763d, 0xfdeddfe7,
    0xa0fc4fff, 0xdbfc3b77, 0x7fdc3ded, 0xf5706fa9,
    } },
    { { /* 523 */
    0x2c403ffb, 0x847fff7f, 0xdeb7ec57, 0xf22fe69c,
    0xd5b50feb, 0xede7afeb, 0xfff08c2f, 0xe8f0537f,
    } },
    { { /* 524 */
    0xb5ffb99d, 0xe78fff66, 0xbe10d981, 0xe3c19c7c,
    0x27339cd1, 0xff6d0cbc, 0xefb7fcb7, 0xffffa0df,
    } },
    { { /* 525 */
    0xfe7bbf0b, 0x353fa3ff, 0x97cd13cc, 0xfb277637,
    0x7e6ccfd6, 0xed31ec50, 0xfc1c677c, 0x5fbff6fa,
    } },
    { { /* 526 */
    0xae2f0fba, 0x7ffea3ad, 0xde74fcf0, 0xf200ffef,
    0xfea2fbbf, 0xbcff3daf, 0x5fb9f694, 0x3f8ff3ad,
    } },
    { { /* 527 */
    0xa01ff26c, 0x01bfffef, 0x70057728, 0xda03ff35,
    0xc7fad2f9, 0x5c1d3fbf, 0xec33ff3a, 0xfe9cb7af,
    } },
    { { /* 528 */
    0x7a9f5236, 0xe722bffa, 0xfcff9ff7, 0xb61d2fbb,
    0x1dfded06, 0xefdf7dd7, 0xf166eb23, 0x0dc07ed9,
    } },
    { { /* 529 */
    0xdfbf3d3d, 0xba83c945, 0x9dd07dd1, 0xcf737b87,
    0xc3f59ff3, 0xc5fedf0d, 0x83020cb3, 0xaec0e879,
    } },
    { { /* 530 */
    0x6f0fc773, 0x093ffd7d, 0x0157fff1, 0x01ff62fb,
    0x3bf3fdb4, 0x43b2b013, 0xff305ed3, 0xeb9f0fff,
    } },
    { { /* 531 */
    0xf203feef, 0xfb893fef, 0x9e9937a9, 0xa72cdef9,
    0xc1f63733, 0xfe3e812e, 0xf2f75d20, 0x69d7d585,
    } },
    { { /* 532 */
    0xffffffff, 0xff6fdb07, 0xd97fc4ff, 0xbe0fefce,
    0xf05ef17b, 0xffb7f6cf, 0xef845ef7, 0x0edfd7cb,
    } },
    { { /* 533 */
    0xfcffff08, 0xffffee3f, 0xd7ff13ff, 0x7ffdaf0f,
    0x1ffabdc7, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 534 */
    0x00000000, 0xe7400000, 0xf933bd38, 0xfeed7feb,
    0x7c767fe8, 0xffefb3f7, 0xd8b7feaf, 0xfbbfff6f,
    } },
    { { /* 535 */
    0xdbf7f8fb, 0xe2f91752, 0x754785c8, 0xe3ef9090,
    0x3f6d9ef4, 0x0536ee2e, 0x7ff3f7bc, 0x7f3fa07b,
    } },
    { { /* 536 */
    0xeb600567, 0x6601babe, 0x583ffcd8, 0x87dfcaf7,
    0xffa0bfcd, 0xfebf5bcd, 0xefa7b6fd, 0xdf9c77ef,
    } },
    { { /* 537 */
    0xf8773fb7, 0xb7fc9d27, 0xdfefcab5, 0xf1b6fb5a,
    0xef1fec39, 0x7ffbfbbf, 0xdafe000d, 0x4e7fbdfb,
    } },
    { { /* 538 */
    0x5ac033ff, 0x9ffebff5, 0x005fffbf, 0xfdf80000,
    0x6ffdffca, 0xa001cffd, 0xfbf2dfff, 0xff7fdfbf,
    } },
    { { /* 539 */
    0x080ffeda, 0xbfffba08, 0xeed77afd, 0x67f9fbeb,
    0xff93e044, 0x9f57df97, 0x08dffef7, 0xfedfdf80,
    } },
    { { /* 540 */
    0xf7feffc5, 0x6803fffb, 0x6bfa67fb, 0x5fe27fff,
    0xff73ffff, 0xe7fb87df, 0xf7a7ebfd, 0xefc7bf7e,
    } },
    { { /* 541 */
    0xdf821ef3, 0xdf7e76ff, 0xda7d79c9, 0x1e9befbe,
    0x77fb7ce0, 0xfffb87be, 0xffdb1bff, 0x4fe03f5c,
    } },
    { { /* 542 */
    0x5f0e7fff, 0xddbf77ff, 0xfffff04f, 0x0ff8ffff,
    0xfddfa3be, 0xfffdfc1c, 0xfb9e1f7d, 0xdedcbdff,
    } },
    { { /* 543 */
    0xbafb3f6f, 0xfbefdf7f, 0x2eec7d1b, 0xf2f7af8e,
    0xcfee7b0f, 0x77c61d96, 0xfff57e07, 0x7fdfd982,
    } },
    { { /* 544 */
    0xc7ff5ee6, 0x79effeee, 0xffcf9a56, 0xde5efe5f,
    0xf9e8896e, 0xe6c4f45e, 0xbe7c0001, 0xdddf3b7f,
    } },
    { { /* 545 */
    0xe9efd59d, 0xde5334ac, 0x4bf7f573, 0x9eff7b4f,
    0x476eb8fe, 0xff450dfb, 0xfbfeabfd, 0xddffe9d7,
    } },
    { { /* 546 */
    0x7fffedf7, 0x7eebddfd, 0xb7ffcfe7, 0xef91bde9,
    0xd77c5d75, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 547 */
    0x00000000, 0xfa800000, 0xb4f1ffee, 0x2fefbf76,
    0x77bfb677, 0xfffd9fbf, 0xf6ae95bf, 0x7f3b75ff,
    } },
    { { /* 548 */
    0x0af9a7f5, 0x00000000, 0x00000000, 0x2bddfbd0,
    0x9a7ff633, 0xd6fcfdab, 0xbfebf9e6, 0xf41fdfdf,
    } },
    { { /* 549 */
    0xffffa6fd, 0xf37b4aff, 0xfef97fb7, 0x1d5cb6ff,
    0xe5ff7ff6, 0x24041f7b, 0xf99ebe05, 0xdff2dbe3,
    } },
    { { /* 550 */
    0xfdff6fef, 0xcbfcd679, 0xefffebfd, 0x0000001f,
    0x98000000, 0x8017e148, 0x00fe6a74, 0xfdf16d7f,
    } },
    { { /* 551 */
    0xfef3b87f, 0xf176e01f, 0x7b3fee96, 0xfffdeb8d,
    0xcbb3adff, 0xe17f84ef, 0xbff04daa, 0xfe3fbf3f,
    } },
    { { /* 552 */
    0xffd7ebff, 0xcf7fffdf, 0x85edfffb, 0x07bcd73f,
    0xfe0faeff, 0x76bffdaf, 0x37bbfaef, 0xa3ba7fdc,
    } },
    { { /* 553 */
    0x56f7b6ff, 0xe7df60f8, 0x4cdfff61, 0xff45b0fb,
    0x3ffa7ded, 0x18fc1fff, 0xe3afffff, 0xdf83c7d3,
    } },
    { { /* 554 */
    0xef7dfb57, 0x1378efff, 0x5ff7fec0, 0x5ee334bb,
    0xeff6f70d, 0x00bfd7fe, 0xf7f7f59d, 0xffe051de,
    } },
    { { /* 555 */
    0x037ffec9, 0xbfef5f01, 0x60a79ff1, 0xf1ffef1d,
    0x0000000f, 0x00000000, 0x00000000, 0x00000000,
    } },
    { { /* 556 */
    0x00000000, 0x00000000, 0x00000000, 0x3c800000,
    0xd91ffb4d, 0xfee37b3a, 0xdc7f3fe9, 0x0000003f,
    } },
    { { /* 557 */
    0x50000000, 0xbe07f51f, 0xf91bfc1d, 0x71ffbc1e,
    0x5bbe6ff9, 0x9b1b5796, 0xfffc7fff, 0xafe7872e,
    } },
    { { /* 558 */
    0xf34febf5, 0xe725dffd, 0x5d440bdc, 0xfddd5747,
    0x7790ed3f, 0x8ac87d7f, 0xf3f9fafa, 0xef4b202a,
    } },
    { { /* 559 */
    0x79cff5ff, 0x0ba5abd3, 0xfb8ff77a, 0x001f8ebd,
    0x00000000, 0xfd4ef300, 0x88001a57, 0x7654aeac,
    } },
    { { /* 560 */
    0xcdff17ad, 0xf42fffb2, 0xdbff5baa, 0x00000002,
    0x73c00000, 0x2e3ff9ea, 0xbbfffa8e, 0xffd376bc,
    } },
    { { /* 561 */
    0x7e72eefe, 0xe7f77ebd, 0xcefdf77f, 0x00000ff5,
    0x00000000, 0xdb9ba900, 0x917fa4c7, 0x7ecef8ca,
    } },
    { { /* 562 */
    0xc7e77d7a, 0xdcaecbbd, 0x8f76fd7e, 0x7cf391d3,
    0x4c2f01e5, 0xa360ed77, 0x5ef807db, 0x21811df7,
    } },
    { { /* 563 */
    0x309c6be0, 0xfade3b3a, 0xc3f57f53, 0x07ba61cd,
    0x00000000, 0x00000000, 0x00000000, 0xbefe26e0,
    } },
    { { /* 564 */
    0xebb503f9, 0xe9cbe36d, 0xbfde9c2f, 0xabbf9f83,
    0xffd51ff7, 0xdffeb7df, 0xffeffdae, 0xeffdfb7e,
    } },
    { { /* 565 */
    0x6ebfaaff, 0x00000000, 0x00000000, 0xb6200000,
    0xbe9e7fcd, 0x58f162b3, 0xfd7bf10d, 0xbefde9f1,
    } },
    { { /* 566 */
    0x5f6dc6c3, 0x69ffff3d, 0xfbf4ffcf, 0x4ff7dcfb,
    0x11372000, 0x00000015, 0x00000000, 0x00000000,
    } },
    { { /* 567 */
    0x00003000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    } },
},
{
    /* aa */
    LEAF(  0,  0),
    /* ab */
    LEAF(  1,  1),
    /* af */
    LEAF(  2,  2), LEAF(  2,  3),
    /* am */
    LEAF(  4,  4), LEAF(  4,  5),
    /* ar */
    LEAF(  6,  6),
    /* as */
    LEAF(  7,  7),
    /* ast */
    LEAF(  8,  8),
    /* ava */
    LEAF(  9,  9),
    /* ay */
    LEAF( 10, 10),
    /* az */
    LEAF( 11, 11), LEAF( 11, 12), LEAF( 11, 13), LEAF( 11, 14),
    /* az_ir */
    LEAF( 15, 15), LEAF( 15, 16),
    /* ba */
    LEAF( 17, 17),
    /* bam */
    LEAF( 18, 18), LEAF( 18, 19), LEAF( 18, 20),
    /* be */
    LEAF( 21, 21),
    /* bg */
    LEAF( 22, 22),
    /* bh */
    LEAF( 23, 23),
    /* bi */
    LEAF( 24, 24),
    /* bin */
    LEAF( 25, 25), LEAF( 25, 26), LEAF( 25, 27),
    /* bo */
    LEAF( 28, 28),
    /* br */
    LEAF( 29, 29),
    /* bs */
    LEAF( 30, 18), LEAF( 30, 30),
    /* bua */
    LEAF( 32, 31),
    /* ca */
    LEAF( 33, 32), LEAF( 33, 33),
    /* ch */
    LEAF( 35, 34),
    /* chm */
    LEAF( 36, 35),
    /* chr */
    LEAF( 37, 36),
    /* co */
    LEAF( 38, 37), LEAF( 38, 38), LEAF( 38, 39),
    /* cs */
    LEAF( 41, 40), LEAF( 41, 41),
    /* cu */
    LEAF( 43, 42),
    /* cv */
    LEAF( 44, 43), LEAF( 44, 44),
    /* cy */
    LEAF( 46, 45), LEAF( 46, 46), LEAF( 46, 47),
    /* da */
    LEAF( 49, 48),
    /* de */
    LEAF( 50, 49), LEAF( 50, 39),
    /* el */
    LEAF( 52, 50), LEAF( 52, 39),
    /* en */
    LEAF( 54, 51), LEAF( 54, 39),
    /* eo */
    LEAF( 56, 18), LEAF( 56, 52),
    /* es */
    LEAF( 58, 53), LEAF( 58, 39),
    /* et */
    LEAF( 60, 54), LEAF( 60, 55),
    /* eu */
    LEAF( 62, 56),
    /* fi */
    LEAF( 63, 57), LEAF( 63, 55), LEAF( 63, 39),
    /* fj */
    LEAF( 66, 18),
    /* fo */
    LEAF( 67, 58),
    /* ful */
    LEAF( 68, 18), LEAF( 68, 59), LEAF( 68, 60),
    /* fur */
    LEAF( 71, 61),
    /* fy */
    LEAF( 72, 62),
    /* ga */
    LEAF( 73, 63), LEAF( 73, 64), LEAF( 73, 65),
    /* gd */
    LEAF( 76, 66),
    /* gl */
    LEAF( 77, 53),
    /* gn */
    LEAF( 78, 67), LEAF( 78, 68), LEAF( 78, 69),
    /* gu */
    LEAF( 81, 70),
    /* gv */
    LEAF( 82, 71),
    /* ha */
    LEAF( 83, 18), LEAF( 83, 72), LEAF( 83, 73),
    /* haw */
    LEAF( 86, 18), LEAF( 86, 74), LEAF( 86, 75),
    /* he */
    LEAF( 89, 76),
    /* hu */
    LEAF( 90, 77), LEAF( 90, 78),
    /* hy */
    LEAF( 92, 79),
    /* ibo */
    LEAF( 93, 18), LEAF( 93, 80),
    /* id */
    LEAF( 95, 81),
    /* ik */
    LEAF( 96, 82),
    /* is */
    LEAF( 97, 83),
    /* it */
    LEAF( 98, 84), LEAF( 98, 39),
    /* iu */
    LEAF(100, 85), LEAF(100, 86), LEAF(100, 87),
    /* ja */
    LEAF(103, 88), LEAF(103, 89), LEAF(103, 90), LEAF(103, 91),
    LEAF(103, 92), LEAF(103, 93), LEAF(103, 94), LEAF(103, 95),
    LEAF(103, 96), LEAF(103, 97), LEAF(103, 98), LEAF(103, 99),
    LEAF(103,100), LEAF(103,101), LEAF(103,102), LEAF(103,103),
    LEAF(103,104), LEAF(103,105), LEAF(103,106), LEAF(103,107),
    LEAF(103,108), LEAF(103,109), LEAF(103,110), LEAF(103,111),
    LEAF(103,112), LEAF(103,113), LEAF(103,114), LEAF(103,115),
    LEAF(103,116), LEAF(103,117), LEAF(103,118), LEAF(103,119),
    LEAF(103,120), LEAF(103,121), LEAF(103,122), LEAF(103,123),
    LEAF(103,124), LEAF(103,125), LEAF(103,126), LEAF(103,127),
    LEAF(103,128), LEAF(103,129), LEAF(103,130), LEAF(103,131),
    LEAF(103,132), LEAF(103,133), LEAF(103,134), LEAF(103,135),
    LEAF(103,136), LEAF(103,137), LEAF(103,138), LEAF(103,139),
    LEAF(103,140), LEAF(103,141), LEAF(103,142), LEAF(103,143),
    LEAF(103,144), LEAF(103,145), LEAF(103,146), LEAF(103,147),
    LEAF(103,148), LEAF(103,149), LEAF(103,150), LEAF(103,151),
    LEAF(103,152), LEAF(103,153), LEAF(103,154), LEAF(103,155),
    LEAF(103,156), LEAF(103,157), LEAF(103,158), LEAF(103,159),
    LEAF(103,160), LEAF(103,161), LEAF(103,162), LEAF(103,163),
    LEAF(103,164), LEAF(103,165), LEAF(103,166), LEAF(103,167),
    LEAF(103,168), LEAF(103,169), LEAF(103,170), LEAF(103,171),
    /* ka */
    LEAF(187,172),
    /* kaa */
    LEAF(188,173),
    /* ki */
    LEAF(189, 18), LEAF(189,174),
    /* kk */
    LEAF(191,175),
    /* kl */
    LEAF(192,176), LEAF(192,177),
    /* km */
    LEAF(194,178),
    /* kn */
    LEAF(195,179),
    /* ko */
    LEAF(196,180), LEAF(196,181), LEAF(196,182), LEAF(196,183),
    LEAF(196,184), LEAF(196,185), LEAF(196,186), LEAF(196,187),
    LEAF(196,188), LEAF(196,189), LEAF(196,190), LEAF(196,191),
    LEAF(196,192), LEAF(196,193), LEAF(196,194), LEAF(196,195),
    LEAF(196,196), LEAF(196,197), LEAF(196,198), LEAF(196,199),
    LEAF(196,200), LEAF(196,201), LEAF(196,202), LEAF(196,203),
    LEAF(196,204), LEAF(196,205), LEAF(196,206), LEAF(196,207),
    LEAF(196,208), LEAF(196,209), LEAF(196,210), LEAF(196,211),
    LEAF(196,212), LEAF(196,213), LEAF(196,214), LEAF(196,215),
    LEAF(196,216), LEAF(196,217), LEAF(196,218), LEAF(196,219),
    LEAF(196,220), LEAF(196,221), LEAF(196,222), LEAF(196,223),
    LEAF(196,224),
    /* ku */
    LEAF(241,225), LEAF(241,226),
    /* ku_ir */
    LEAF(243,227),
    /* kum */
    LEAF(244,228),
    /* kv */
    LEAF(245,229),
    /* kw */
    LEAF(246, 18), LEAF(246, 74), LEAF(246,230),
    /* ky */
    LEAF(249,231),
    /* la */
    LEAF(250, 18), LEAF(250,232),
    /* lb */
    LEAF(252,233),
    /* ln */
    LEAF(253,234), LEAF(253,235), LEAF(253,236), LEAF(253,237),
    /* lo */
    LEAF(257,238),
    /* lt */
    LEAF(258, 18), LEAF(258,239),
    /* lv */
    LEAF(260, 18), LEAF(260,240),
    /* mg */
    LEAF(262,241),
    /* mh */
    LEAF(263, 18), LEAF(263,242),
    /* mi */
    LEAF(265, 18), LEAF(265, 74), LEAF(265,243),
    /* mk */
    LEAF(268,244),
    /* ml */
    LEAF(269,245),
    /* mn */
    LEAF(270,246),
    /* mo */
    LEAF(271,247), LEAF(271, 43), LEAF(271,248), LEAF(271,228),
    /* mt */
    LEAF(275,249), LEAF(275,250),
    /* my */
    LEAF(277,251),
    /* nb */
    LEAF(278,252),
    /* nds */
    LEAF(279, 49),
    /* nl */
    LEAF(280,253), LEAF(280, 39),
    /* nn */
    LEAF(282,254),
    /* ny */
    LEAF(283, 18), LEAF(283,255),
    /* oc */
    LEAF(285,256),
    /* or */
    LEAF(286,257),
    /* pa */
    LEAF(287,258),
    /* pl */
    LEAF(288,259), LEAF(288,260),
    /* ps_af */
    LEAF(290,261),
    /* ps_pk */
    LEAF(291,262),
    /* pt */
    LEAF(292,263), LEAF(292, 39),
    /* rm */
    LEAF(294,264),
    /* ro */
    LEAF(295,247), LEAF(295, 43), LEAF(295,248),
    /* sah */
    LEAF(298,265),
    /* sco */
    LEAF(299, 18), LEAF(299,266), LEAF(299,267),
    /* se */
    LEAF(302,268), LEAF(302,269),
    /* sh */
    LEAF(304,270),
    /* si */
    LEAF(305,271),
    /* sk */
    LEAF(306,272), LEAF(306,273),
    /* sm */
    LEAF(308, 18), LEAF(308, 75),
    /* sma */
    LEAF(310,274),
    /* smj */
    LEAF(311,275),
    /* smn */
    LEAF(312,276), LEAF(312,277),
    /* sms */
    LEAF(314,278), LEAF(314,279), LEAF(314,280),
    /* sq */
    LEAF(317,281),
    /* sv */
    LEAF(318,282),
    /* syr */
    LEAF(319,283),
    /* ta */
    LEAF(320,284),
    /* te */
    LEAF(321,285),
    /* tg */
    LEAF(322,286),
    /* th */
    LEAF(323,287),
    /* ti_er */
    LEAF(324,288), LEAF(324,289),
    /* tig */
    LEAF(326,290), LEAF(326,289),
    /* tk */
    LEAF(328,291),
    /* tl */
    LEAF(329,292),
    /* tn */
    LEAF(330,293),
    /* tr */
    LEAF(331,294), LEAF(331,295),
    /* tt */
    LEAF(333,296),
    /* tw */
    LEAF(334,297), LEAF(334,298), LEAF(334,236), LEAF(334,299),
    LEAF(334,300),
    /* uk */
    LEAF(339,301),
    /* ur */
    LEAF(340,302), LEAF(340, 16),
    /* uz */
    LEAF(342,303),
    /* ven */
    LEAF(343, 18), LEAF(343,304),
    /* vi */
    LEAF(345,305), LEAF(345,306), LEAF(345,307), LEAF(345,308),
    /* vo */
    LEAF(349,309),
    /* vot */
    LEAF(350,310), LEAF(350, 55),
    /* wa */
    LEAF(352,311),
    /* wen */
    LEAF(353,259), LEAF(353,312),
    /* wo */
    LEAF(355,313), LEAF(355,314),
    /* yap */
    LEAF(357,315),
    /* yo */
    LEAF(358,316), LEAF(358,317), LEAF(358,318), LEAF(358,319),
    /* zh_cn */
    LEAF(362,320), LEAF(362,321), LEAF(362,322), LEAF(362,323),
    LEAF(362,324), LEAF(362,325), LEAF(362,326), LEAF(362,327),
    LEAF(362,328), LEAF(362,329), LEAF(362,330), LEAF(362,331),
    LEAF(362,332), LEAF(362,333), LEAF(362,334), LEAF(362,335),
    LEAF(362,336), LEAF(362,337), LEAF(362,338), LEAF(362,339),
    LEAF(362,340), LEAF(362,341), LEAF(362,342), LEAF(362,343),
    LEAF(362,344), LEAF(362,345), LEAF(362,346), LEAF(362,347),
    LEAF(362,348), LEAF(362,349), LEAF(362,350), LEAF(362,351),
    LEAF(362,352), LEAF(362,353), LEAF(362,354), LEAF(362,355),
    LEAF(362,356), LEAF(362,357), LEAF(362,358), LEAF(362,359),
    LEAF(362,360), LEAF(362,361), LEAF(362,362), LEAF(362,363),
    LEAF(362,364), LEAF(362,365), LEAF(362,366), LEAF(362,367),
    LEAF(362,368), LEAF(362,369), LEAF(362,370), LEAF(362,371),
    LEAF(362,372), LEAF(362,373), LEAF(362,374), LEAF(362,375),
    LEAF(362,376), LEAF(362,377), LEAF(362,378), LEAF(362,379),
    LEAF(362,380), LEAF(362,381), LEAF(362,382), LEAF(362,383),
    LEAF(362,384), LEAF(362,385), LEAF(362,386), LEAF(362,387),
    LEAF(362,388), LEAF(362,389), LEAF(362,390), LEAF(362,391),
    LEAF(362,392), LEAF(362,393), LEAF(362,394), LEAF(362,395),
    LEAF(362,396), LEAF(362,397), LEAF(362,398), LEAF(362,399),
    LEAF(362,400), LEAF(362,401),
    /* zh_hk */
    LEAF(444,402), LEAF(444,403), LEAF(444,404), LEAF(444,405),
    LEAF(444,406), LEAF(444,407), LEAF(444,408), LEAF(444,409),
    LEAF(444,410), LEAF(444,411), LEAF(444,412), LEAF(444,413),
    LEAF(444,414), LEAF(444,415), LEAF(444,416), LEAF(444,417),
    LEAF(444,418), LEAF(444,419), LEAF(444,420), LEAF(444,421),
    LEAF(444,422), LEAF(444,423), LEAF(444,424), LEAF(444,425),
    LEAF(444,426), LEAF(444,427), LEAF(444,428), LEAF(444,429),
    LEAF(444,430), LEAF(444,431), LEAF(444,432), LEAF(444,433),
    LEAF(444,434), LEAF(444,435), LEAF(444,436), LEAF(444,437),
    LEAF(444,438), LEAF(444,439), LEAF(444,440), LEAF(444,441),
    LEAF(444,442), LEAF(444,443), LEAF(444,444), LEAF(444,445),
    LEAF(444,446), LEAF(444,447), LEAF(444,448), LEAF(444,449),
    LEAF(444,450), LEAF(444,451), LEAF(444,452), LEAF(444,453),
    LEAF(444,454), LEAF(444,455), LEAF(444,456), LEAF(444,457),
    LEAF(444,458), LEAF(444,459), LEAF(444,460), LEAF(444,461),
    LEAF(444,462), LEAF(444,463), LEAF(444,464), LEAF(444,465),
    LEAF(444,466), LEAF(444,467), LEAF(444,468), LEAF(444,469),
    LEAF(444,470), LEAF(444,471), LEAF(444,472), LEAF(444,473),
    LEAF(444,474), LEAF(444,475), LEAF(444,476), LEAF(444,477),
    LEAF(444,478), LEAF(444,479), LEAF(444,480), LEAF(444,481),
    LEAF(444,482), LEAF(444,483), LEAF(444,484),
    /* zh_tw */
    LEAF(527,485), LEAF(527,486), LEAF(527,487), LEAF(527,488),
    LEAF(527,489), LEAF(527,490), LEAF(527,491), LEAF(527,492),
    LEAF(527,493), LEAF(527,494), LEAF(527,495), LEAF(527,496),
    LEAF(527,497), LEAF(527,498), LEAF(527,499), LEAF(527,500),
    LEAF(527,501), LEAF(527,502), LEAF(527,503), LEAF(527,504),
    LEAF(527,505), LEAF(527,506), LEAF(527,507), LEAF(527,508),
    LEAF(527,509), LEAF(527,510), LEAF(527,511), LEAF(527,512),
    LEAF(527,513), LEAF(527,514), LEAF(527,515), LEAF(527,516),
    LEAF(527,517), LEAF(527,518), LEAF(527,519), LEAF(527,520),
    LEAF(527,521), LEAF(527,522), LEAF(527,523), LEAF(527,524),
    LEAF(527,525), LEAF(527,526), LEAF(527,527), LEAF(527,528),
    LEAF(527,529), LEAF(527,530), LEAF(527,531), LEAF(527,532),
    LEAF(527,533), LEAF(527,534), LEAF(527,535), LEAF(527,536),
    LEAF(527,537), LEAF(527,538), LEAF(527,539), LEAF(527,540),
    LEAF(527,541), LEAF(527,542), LEAF(527,543), LEAF(527,544),
    LEAF(527,545), LEAF(527,546), LEAF(527,547), LEAF(527,548),
    LEAF(527,549), LEAF(527,550), LEAF(527,551), LEAF(527,552),
    LEAF(527,553), LEAF(527,554), LEAF(527,555), LEAF(527,556),
    LEAF(527,557), LEAF(527,558), LEAF(527,559), LEAF(527,560),
    LEAF(527,561), LEAF(527,562), LEAF(527,563), LEAF(527,564),
    LEAF(527,565), LEAF(527,566), LEAF(527,567),
},
{
    /* aa */
    0x0000,
    /* ab */
    0x0004,
    /* af */
    0x0000, 0x0001,
    /* am */
    0x0012, 0x0013,
    /* ar */
    0x00fe,
    /* as */
    0x0009,
    /* ast */
    0x0000,
    /* ava */
    0x0004,
    /* ay */
    0x0000,
    /* az */
    0x0000, 0x0001, 0x0002, 0x0004,
    /* az_ir */
    0x00fb, 0x00fe,
    /* ba */
    0x0004,
    /* bam */
    0x0000, 0x0001, 0x0002,
    /* be */
    0x0004,
    /* bg */
    0x0004,
    /* bh */
    0x0009,
    /* bi */
    0x0000,
    /* bin */
    0x0000, 0x0003, 0x001e,
    /* bo */
    0x000f,
    /* br */
    0x0000,
    /* bs */
    0x0000, 0x0001,
    /* bua */
    0x0004,
    /* ca */
    0x0000, 0x0001,
    /* ch */
    0x0000,
    /* chm */
    0x0004,
    /* chr */
    0x0013,
    /* co */
    0x0000, 0x0001, 0x0020,
    /* cs */
    0x0000, 0x0001,
    /* cu */
    0x0004,
    /* cv */
    0x0001, 0x0004,
    /* cy */
    0x0000, 0x0001, 0x001e,
    /* da */
    0x0000,
    /* de */
    0x0000, 0x0020,
    /* el */
    0x0003, 0x0020,
    /* en */
    0x0000, 0x0020,
    /* eo */
    0x0000, 0x0001,
    /* es */
    0x0000, 0x0020,
    /* et */
    0x0000, 0x0001,
    /* eu */
    0x0000,
    /* fi */
    0x0000, 0x0001, 0x0020,
    /* fj */
    0x0000,
    /* fo */
    0x0000,
    /* ful */
    0x0000, 0x0001, 0x0002,
    /* fur */
    0x0000,
    /* fy */
    0x0000,
    /* ga */
    0x0000, 0x0001, 0x001e,
    /* gd */
    0x0000,
    /* gl */
    0x0000,
    /* gn */
    0x0000, 0x0001, 0x001e,
    /* gu */
    0x000a,
    /* gv */
    0x0000,
    /* ha */
    0x0000, 0x0001, 0x0002,
    /* haw */
    0x0000, 0x0001, 0x0002,
    /* he */
    0x0005,
    /* hu */
    0x0000, 0x0001,
    /* hy */
    0x0005,
    /* ibo */
    0x0000, 0x001e,
    /* id */
    0x0000,
    /* ik */
    0x0004,
    /* is */
    0x0000,
    /* it */
    0x0000, 0x0020,
    /* iu */
    0x0014, 0x0015, 0x0016,
    /* ja */
    0x0030, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054,
    0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x005b, 0x005c,
    0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064,
    0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c,
    0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074,
    0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c,
    0x007d, 0x007e, 0x007f, 0x0080, 0x0081, 0x0082, 0x0083, 0x0084,
    0x0085, 0x0086, 0x0087, 0x0088, 0x0089, 0x008a, 0x008b, 0x008c,
    0x008d, 0x008e, 0x008f, 0x0090, 0x0091, 0x0092, 0x0093, 0x0094,
    0x0095, 0x0096, 0x0097, 0x0098, 0x0099, 0x009a, 0x009b, 0x009c,
    0x009d, 0x009e, 0x009f, 0x00ff,
    /* ka */
    0x0010,
    /* kaa */
    0x0004,
    /* ki */
    0x0000, 0x0001,
    /* kk */
    0x0004,
    /* kl */
    0x0000, 0x0001,
    /* km */
    0x0017,
    /* kn */
    0x000c,
    /* ko */
    0x0031, 0x00ac, 0x00ad, 0x00ae, 0x00af, 0x00b0, 0x00b1, 0x00b2,
    0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7, 0x00b8, 0x00b9, 0x00ba,
    0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf, 0x00c0, 0x00c1, 0x00c2,
    0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7, 0x00c8, 0x00c9, 0x00ca,
    0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf, 0x00d0, 0x00d1, 0x00d2,
    0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
    /* ku */
    0x0000, 0x0004,
    /* ku_ir */
    0x0006,
    /* kum */
    0x0004,
    /* kv */
    0x0004,
    /* kw */
    0x0000, 0x0001, 0x0002,
    /* ky */
    0x0004,
    /* la */
    0x0000, 0x0001,
    /* lb */
    0x0000,
    /* ln */
    0x0000, 0x0001, 0x0002, 0x0003,
    /* lo */
    0x000e,
    /* lt */
    0x0000, 0x0001,
    /* lv */
    0x0000, 0x0001,
    /* mg */
    0x0000,
    /* mh */
    0x0000, 0x0001,
    /* mi */
    0x0000, 0x0001, 0x001e,
    /* mk */
    0x0004,
    /* ml */
    0x000d,
    /* mn */
    0x0018,
    /* mo */
    0x0000, 0x0001, 0x0002, 0x0004,
    /* mt */
    0x0000, 0x0001,
    /* my */
    0x0010,
    /* nb */
    0x0000,
    /* nds */
    0x0000,
    /* nl */
    0x0000, 0x0020,
    /* nn */
    0x0000,
    /* ny */
    0x0000, 0x0001,
    /* oc */
    0x0000,
    /* or */
    0x000b,
    /* pa */
    0x000a,
    /* pl */
    0x0000, 0x0001,
    /* ps_af */
    0x0006,
    /* ps_pk */
    0x0006,
    /* pt */
    0x0000, 0x0020,
    /* rm */
    0x0000,
    /* ro */
    0x0000, 0x0001, 0x0002,
    /* sah */
    0x0004,
    /* sco */
    0x0000, 0x0001, 0x0002,
    /* se */
    0x0000, 0x0001,
    /* sh */
    0x0004,
    /* si */
    0x000d,
    /* sk */
    0x0000, 0x0001,
    /* sm */
    0x0000, 0x0002,
    /* sma */
    0x0000,
    /* smj */
    0x0000,
    /* smn */
    0x0000, 0x0001,
    /* sms */
    0x0000, 0x0001, 0x0002,
    /* sq */
    0x0000,
    /* sv */
    0x0000,
    /* syr */
    0x0007,
    /* ta */
    0x000b,
    /* te */
    0x000c,
    /* tg */
    0x0004,
    /* th */
    0x000e,
    /* ti_er */
    0x0012, 0x0013,
    /* tig */
    0x0012, 0x0013,
    /* tk */
    0x0004,
    /* tl */
    0x0017,
    /* tn */
    0x0000,
    /* tr */
    0x0000, 0x0001,
    /* tt */
    0x0004,
    /* tw */
    0x0000, 0x0001, 0x0002, 0x0003, 0x001e,
    /* uk */
    0x0004,
    /* ur */
    0x00fb, 0x00fe,
    /* uz */
    0x0004,
    /* ven */
    0x0000, 0x001e,
    /* vi */
    0x0000, 0x0001, 0x0003, 0x001e,
    /* vo */
    0x0000,
    /* vot */
    0x0000, 0x0001,
    /* wa */
    0x0000,
    /* wen */
    0x0000, 0x0001,
    /* wo */
    0x0000, 0x0001,
    /* yap */
    0x0000,
    /* yo */
    0x0000, 0x0001, 0x0003, 0x001e,
    /* zh_cn */
    0x0002, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054,
    0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x005b, 0x005c,
    0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064,
    0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c,
    0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074,
    0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c,
    0x007d, 0x007e, 0x007f, 0x0080, 0x0081, 0x0082, 0x0083, 0x0084,
    0x0085, 0x0086, 0x0087, 0x0088, 0x0089, 0x008a, 0x008b, 0x008c,
    0x008d, 0x008e, 0x008f, 0x0090, 0x0091, 0x0092, 0x0093, 0x0094,
    0x0095, 0x0096, 0x0097, 0x0098, 0x0099, 0x009a, 0x009b, 0x009c,
    0x009e, 0x009f,
    /* zh_hk */
    0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055,
    0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d,
    0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065,
    0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d,
    0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075,
    0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d,
    0x007e, 0x007f, 0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085,
    0x0086, 0x0087, 0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d,
    0x008e, 0x008f, 0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095,
    0x0096, 0x0097, 0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d,
    0x009e, 0x009f, 0x0205,
    /* zh_tw */
    0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055,
    0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d,
    0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065,
    0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d,
    0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075,
    0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d,
    0x007e, 0x007f, 0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085,
    0x0086, 0x0087, 0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d,
    0x008e, 0x008f, 0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095,
    0x0096, 0x0097, 0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d,
    0x009e, 0x009f, 0x00fa,
}
};

#define NUM_LANG_CHAR_SET	185
#define NUM_LANG_SET_MAP	6

static const FcChar32 fcLangCountrySets[][NUM_LANG_SET_MAP] = {
    { 0x00000400, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, }, /* az */
    { 0x00000000, 0x00000000, 0x00800000, 0x00000000, 0x00000000, 0x00000000, }, /* ku */
    { 0x00000000, 0x00000000, 0x00000000, 0x04000000, 0x00000000, 0x00000000, }, /* ps */
    { 0x00000000, 0x00000000, 0x00000000, 0x08000000, 0x00000000, 0x00000000, }, /* ps */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x01000000, 0x00000000, }, /* ti */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x02000000, 0x00000000, }, /* ti */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00080000, }, /* zh */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00100000, }, /* zh */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00200000, }, /* zh */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00400000, }, /* zh */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00800000, }, /* zh */
};

#define NUM_COUNTRY_SET 11
static const FcLangCharSetRange  fcLangCharSetRanges[] = {
    { 0, 10 }, /* a */
    { 11, 23 }, /* b */
    { 24, 33 }, /* c */
    { 34, 36 }, /* d */
    { 37, 42 }, /* e */
    { 43, 50 }, /* f */
    { 51, 57 }, /* g */
    { 58, 65 }, /* h */
    { 66, 74 }, /* i */
    { 75, 75 }, /* j */
    { 76, 91 }, /* k */
    { 92, 98 }, /* l */
    { 99, 108 }, /* m */
    { 109, 115 }, /* n */
    { 116, 119 }, /* o */
    { 120, 124 }, /* p */
    { 125, 124 }, /* q */
    { 125, 127 }, /* r */
    { 128, 147 }, /* s */
    { 148, 163 }, /* t */
    { 164, 167 }, /* u */
    { 168, 171 }, /* v */
    { 172, 174 }, /* w */
    { 175, 175 }, /* x */
    { 176, 178 }, /* y */
    { 179, 184 }, /* z */
};

