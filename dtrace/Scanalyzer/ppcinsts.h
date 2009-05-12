
iType   itypeA      = {  1, 0x001F, dcdFormA,   0};
iType   itypeB      = {  0, 0x0000, dcdFormB,   0};
iType   itypeD      = {  0, 0x0000, dcdFormD,   0};
iType   itypeDS     = {  0, 0x0003, dcdFormDS,  0};
iType   itypeI      = {  0, 0x0000, dcdFormI,   0};
iType   itypeM      = {  0, 0x0000, dcdFormM,   0};
iType   itypeMD     = {  2, 0x0007, dcdFormMD,  0};
iType   itypeMDS    = {  1, 0x000F, dcdFormMDS, 0};
iType   itypeSC     = {  0, 0x0000, dcdFormSC,  0};
iType   itypeVA     = {  0, 0x003F, dcdFormVA,  0};
iType   itypeVX     = {  0, 0x07FF, dcdFormVX,  0};
iType   itypeVXR    = {  0, 0x03FF, dcdFormVXR, 0};
iType   itypeX      = {  1, 0x03FF, dcdFormX,   0};
iType   itypeXFL    = {  1, 0x03FF, dcdFormXFL, 0};
iType   itypeXFX    = {  1, 0x03FF, dcdFormXFX, 0};
iType   itypeXL     = {  1, 0x03FF, dcdFormXL,  0};
iType   itypeXO     = {  1, 0x01FF, dcdFormXO,  0};
iType   itypeXS     = {  2, 0x01FF, dcdFormXS,  0};


ppcinst ppcm00[] = {
    {   0,   256, &itypeX,   xattn,         "attn",     modVtab | modUnimpl,           isScalar, 4, 0},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};

ppcinst ppcm02[] = {
    {   2,     0, &itypeD,   xtdi,          "tdi",     modFDtoa | modUnimpl,           isTrap, 8, 0},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm03[] = {
    {   3,     0, &itypeD,   xtwi,          "twi",     modVta | modUnimpl,             isTrap, 4, 0},
     {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm04[] = {
    {   4,     0, &itypeVX,  0,      "vaddubm",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,     2, &itypeVX,  0,       "vmaxub",        modVtab  | mod3op | modUnimpl,  isVec, 4, rfVpr},
    {   4,     4, &itypeVX,  0,         "vrlb",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,     6, &itypeVXR, 0,    "vcmpequb",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,     8, &itypeVX,  0,      "vmuloub",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    10, &itypeVX,  0,       "vaddfp",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    12, &itypeVX,  0,       "vmrghb",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    14, &itypeVX,  0,      "vpkuhum",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    32, &itypeVA,  0,    "vmhaddshs",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    33, &itypeVA,  0,   "vmhraddshs",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    34, &itypeVA,  0,    "vmladduhm",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    36, &itypeVA,  0,     "vmsumubm",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    37, &itypeVA,  0,     "vmsummbm",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    38, &itypeVA,  0,     "vmsumuhm",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    39, &itypeVA,  0,     "vmsumuhs",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    40, &itypeVA,  0,     "vmsumshm",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    41, &itypeVA,  0,     "vmsumshs",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    42, &itypeVA,  0,         "vsel",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    43, &itypeVA,  0,        "vperm",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    44, &itypeVA,  0,       "vsldoi",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    46, &itypeVA,  0,      "vmaddfp",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    47, &itypeVA,  0,     "vnmsubfp",        modVtabc | mod4op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    64, &itypeVX,  0,      "vadduhm",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    66, &itypeVX,  0,       "vmaxuh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    68, &itypeVX,  0,         "vrlh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    70, &itypeVXR, 0,    "vcmpequh",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    72, &itypeVX,  0,      "vmulouh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    74, &itypeVX,  0,       "vsubfp",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    76, &itypeVX,  0,       "vmrghh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,    78, &itypeVX,  0,      "vpkuwum",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   128, &itypeVX,  0,      "vadduwm",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   130, &itypeVX,  0,       "vmaxuw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   132, &itypeVX,  0,         "vrlw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   134, &itypeVXR, 0,    "vcmpequw",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   140, &itypeVX,  0,       "vmrghw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   142, &itypeVX,  0,      "vpkuhus",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   198, &itypeVXR, 0,    "vcmpeqfp",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   206, &itypeVX,  0,      "vpkuwus",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   258, &itypeVX,  0,       "vmaxsb",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   260, &itypeVX,  0,         "vslb",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   264, &itypeVX,  0,      "vmulosb",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   266, &itypeVX,  0,        "vrefp",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   268, &itypeVX,  0,       "vmrglb",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   270, &itypeVX,  0,      "vpkshus",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   322, &itypeVX,  0,       "vmaxsh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   324, &itypeVX,  0,         "vslh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   328, &itypeVX,  0,      "vmulosh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   330, &itypeVX,  0,    "vrsqrtefp",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   332, &itypeVX,  0,       "vmrglh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   334, &itypeVX,  0,      "vpkswus",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   384, &itypeVX,  0,      "vaddcuw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   386, &itypeVX,  0,       "vmaxsw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   388, &itypeVX,  0,         "vslw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   394, &itypeVX,  0,     "vexptefp",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   396, &itypeVX,  0,       "vmrglw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   398, &itypeVX,  0,      "vpkshss",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   452, &itypeVX,  0,          "vsl",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   454, &itypeVXR, 0,    "vcmpgefp",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   458, &itypeVX,  0,      "vlogefp",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   462, &itypeVX,  0,      "vpkswss",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   512, &itypeVX,  0,      "vaddubs",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   514, &itypeVX,  0,       "vminub",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   516, &itypeVX,  0,         "vsrb",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   518, &itypeVXR, 0,    "vcmpgtub",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   520, &itypeVX,  0,      "vmuleub",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   522, &itypeVX,  0,        "vrfin",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   524, &itypeVX,  0,       "vspltb",        modVtb   | moduim | modUnimpl,  isVec, 16, rfVpr},
    {   4,   526, &itypeVX,  0,      "vupkhsb",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   576, &itypeVX,  0,      "vadduhs",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   578, &itypeVX,  0,       "vminuh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   580, &itypeVX,  0,         "vsrh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   582, &itypeVXR, 0,    "vcmpgtuh",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   584, &itypeVX,  0,      "vmuleuh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   586, &itypeVX,  0,        "vrfiz",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   588, &itypeVX,  0,       "vsplth",        modVtb   | moduim | modUnimpl,  isVec, 16, rfVpr},
    {   4,   590, &itypeVX,  0,      "vupkhsh",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   640, &itypeVX,  0,      "vadduws",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   642, &itypeVX,  0,       "vminuw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   644, &itypeVX,  0,         "vsrw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   646, &itypeVXR, 0,    "vcmpgtuw",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   650, &itypeVX,  0,        "vrfip",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   652, &itypeVX,  0,       "vspltw",        modVtb   | moduim | modUnimpl,  isVec, 16, rfVpr},
    {   4,   654, &itypeVX,  0,      "vupklsb",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   708, &itypeVX,  0,          "vsr",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   710, &itypeVXR, 0,    "vcmpgtfp",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   714, &itypeVX,  0,        "vrfim",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   718, &itypeVX,  0,      "vupklsh",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   768, &itypeVX,  0,      "vaddsbs",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   770, &itypeVX,  0,       "vminsb",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   772, &itypeVX,  0,        "vsrab",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   774, &itypeVXR, 0,    "vcmpgtsb",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   776, &itypeVX,  0,      "vmulesb",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   778, &itypeVX,  0,        "vcfux",        modVtb   | moduim | modUnimpl,  isVec, 16, rfVpr},
    {   4,   780, &itypeVX,  0,     "vspltisb",        modVt    | modsim | modUnimpl,  isVec, 16, rfVpr},
    {   4,   782, &itypeVX,  0,        "vpkpx",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   832, &itypeVX,  0,      "vaddshs",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   834, &itypeVX,  0,       "vminsh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   836, &itypeVX,  0,        "vsrah",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   838, &itypeVXR, 0,    "vcmpgtsh",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   840, &itypeVX,  0,      "vmulesh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   842, &itypeVX,  0,        "vcfsx",        modVtb   | moduim | modUnimpl,  isVec, 16, rfVpr},
    {   4,   844, &itypeVX,  0,     "vspltish",        modVt    | modsim | modUnimpl,  isVec, 16, rfVpr},
    {   4,   846, &itypeVX,  0,      "vupkhpx",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   896, &itypeVX,  0,      "vaddsws",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   898, &itypeVX,  0,       "vminsw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   900, &itypeVX,  0,        "vsraw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   902, &itypeVXR, 0,    "vcmpgtsw",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   906, &itypeVX,  0,       "vctuxs",        modVtb   | moduim | modUnimpl,  isVec, 16, rfVpr},
    {   4,   908, &itypeVX,  0,     "vspltisw",        modVt    | modsim | modUnimpl,  isVec, 16, rfVpr},
    {   4,   966, &itypeVXR, 0,     "vcmpbfp",         modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,   970, &itypeVX,  0,       "vctsxs",        modVtb   | moduim | modUnimpl,  isVec, 16, rfVpr},
    {   4,   974, &itypeVX,  0,      "vupklpx",        modVtb   | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1024, &itypeVX,  0,      "vsububm",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1026, &itypeVX,  0,       "vavgub",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1028, &itypeVX,  0,         "vand",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1034, &itypeVX,  0,       "vmaxfp",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1036, &itypeVX,  0,         "vslo",        modVtab  | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1088, &itypeVX,  0,      "vsubuhm",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1090, &itypeVX,  0,       "vavguh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1092, &itypeVX,  0,        "vandc",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1098, &itypeVX,  0,       "vminfp",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1100, &itypeVX,  0,         "vsro",        modVtab  | mod2op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1152, &itypeVX,  0,      "vsubuwm",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1154, &itypeVX,  0,       "vavguw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1156, &itypeVX,  0,          "vor",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1220, &itypeVX,  0,         "vxor",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1282, &itypeVX,  0,       "vavgsb",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1284, &itypeVX,  0,         "vnor",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1346, &itypeVX,  0,       "vavgsh",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1408, &itypeVX,  0,      "vsubcuw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1410, &itypeVX,  0,       "vavgsw",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1536, &itypeVX,  0,      "vsububs",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1540, &itypeVX,  0,       "mfvscr",        modVt    | mod1opt | modUnimpl, isVec, 16, rfVpr},
    {   4,  1544, &itypeVX,  0,     "vsum4ubs",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1600, &itypeVX,  0,      "vsubuhs",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1604, &itypeVX,  0,       "mtvscr",        modVb    | mod1opb | modUnimpl, isVec, 16, rfVpr},
    {   4,  1608, &itypeVX,  0,     "vsum4shs",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1664, &itypeVX,  0,      "vsubuws",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1672, &itypeVX,  0,     "vsum2sws",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1792, &itypeVX,  0,      "vsubsbs",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1800, &itypeVX,  0,     "vsum4sbs",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1856, &itypeVX,  0,      "vsubshs",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1920, &itypeVX,  0,      "vsubsws",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {   4,  1928, &itypeVX,  0,      "vsumsws",        modVtab  | mod3op | modUnimpl,  isVec, 16, rfVpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm07[] = {
    {   7,     0, &itypeD,        0,        "mulli",    modVta | modUnimpl,            isScalar, 4, rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm08[] = {
    {   8,     0, &itypeD,         0,       "subfic",  modVta | modUnimpl,             isScalar, 4, rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm10[] = {
    {  10,     0, &itypeD,   xcmpli,        "cmpli",   modVta | modUnimpl | modTnomod, isScalar, 4, dNoFmt | dImmUn},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm11[] = {
    {  11,     0, &itypeD,   xcmpi,         "cmpi",    modVta | modUnimpl | modTnomod, isScalar, 4, dNoFmt},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm12[] = {
    {  12,     0, &itypeD,   xaddic,        "addic",   modVta,                         isScalar, 4, rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm13[] = {
    {  13,     0, &itypeD,   xaddicdot,     "addic.",  modVta,                         isScalar, 4, rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm14[] = {
    {  14,     0, &itypeD,   xaddi,         "addi",    modVta,                         isScalar, 4, rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm15[] = {
    {  15,     0, &itypeD,   xaddis,        "addis",   modVta,                         isScalar, 4, rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm16[] = {
    {  16,     0, &itypeB,   xbc,           "bc",      modFBBOBI,                      isBrCond, 0, 0},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm17[] = {
    {  17,     0, &itypeSC,   xsc,           "sc",     modUnimpl,                      isSysCall, 0, 0},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm18[] = {
    {  18,     0, &itypeI,   xb,            "b",       0,                              isBranch, 4, 0},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm19[] = {
    {  19,     0, &itypeXL,  xmcrf,         "mcrf",    modVtab | modUnimpl,            isScalar, 4, dSkip | rfSpr | (sPunimp << 8)},
    {  19,    16, &itypeXL,  xbclr,         "bclr",    modVtab | modLR,                isBrCond, 4, dSkip},
    {  19,    18, &itypeXL,  xoponly,       "rfid",    modPrv,                         isRFI, 8, dSkip},
    {  19,    33, &itypeXL,  0,             "crnor",   modVtab | modUnimpl,            isScalar, 4, rfSpr | (sPunimp << 8)},
    {  19,    50, &itypeXL,  xoponly,       "rfi",     modPrv,                         isRFI, 4, dSkip},
    {  19,   129, &itypeXL,  0,             "crandc",  modVtab | modUnimpl,            isScalar, 4, rfSpr | (sPunimp << 8)},
    {  19,   150, &itypeXL,  xoponly,       "isync",   0,                              isScalar, 0, dSkip},
    {  19,   193, &itypeXL,  0,             "crxor",   modVtab | modUnimpl,            isScalar, 4, rfSpr | (sPunimp << 8)},
    {  19,   225, &itypeXL,  0,             "crnand",  modVtab | modUnimpl,            isScalar, 4, rfSpr | (sPunimp << 8)},
    {  19,   257, &itypeXL,  0,             "crand",   modVtab | modUnimpl,            isScalar, 4, rfSpr | (sPunimp << 8)},
    {  19,   274, &itypeXL,  xoponly,       "hrfid",   modPrv,                         isRFI, 8, dSkip},
    {  19,   289, &itypeXL,  0,             "creqv",   modVtab | modUnimpl,            isScalar, 4, rfSpr | (sPunimp << 8)},
    {  19,   417, &itypeXL,  0,             "crorc",   modVtab | modUnimpl,            isScalar, 4, rfSpr | (sPunimp << 8)},
    {  19,   449, &itypeXL,  0,             "cror",    modVtab | modUnimpl,            isScalar, 4, rfSpr | (sPunimp << 8)},
    {  19,   528, &itypeXL,  xbctr,         "bctr",    modFBBOBI | modCTR,             isBranch, 4, dSkip},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm20[] = {
    {  20,     0, &itypeM,   xrot,    "rlwimi",        modSHtabc | modROTL32 | modInsert | modDplus32, isScalar, 4, rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm21[] = {
    {  21,     0, &itypeM,   xrot,    "rlwinm",        modSHtabc | modROTL32 | modDplus32, isScalar, 4, rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm23[] = {
    {  23,     0, &itypeM,   xrot,     "rlwnm",        modSHtabc | modROTL32 | modUseRB | modDplus32, isScalar, 4, rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm24[] = {
    {  24,     0, &itypeD,   xlogi,         "ori",     modVta | modor,                 isScalar, 4, dSwapTA | dImmUn | rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm25[] = {
    {  25,     0, &itypeD,   xlogi,         "oris",    modVta | modor | modshft,       isScalar, 4, dSwapTA | dImmUn | rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
}; 


ppcinst ppcm26[] = {
    {  26,     0, &itypeD,   xlogi,         "xori",    modVta | modxor,                isScalar, 4, dSwapTA | dImmUn | rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm27[] = {
    {  27,     0, &itypeD,   xlogi,        "xoris",   modVta | modxor | modshft,       isScalar, 4, dSwapTA | dImmUn | rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm28[] = {
    {  28,     0, &itypeD,   xlogi,      "andi.",   modVta | modxor | modSetCRF,       isScalar, 4, dSwapTA | dImmUn | rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm29[] = {
    {  29,     0, &itypeD,   xlogi,     "andis.",  modVta | modxor | modshft | modSetCRF, isScalar, 4, dSwapTA | dImmUn | rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm30[] = {
    {  30,     0, &itypeMD,  xrot,    "rldicl",        modSHtabc | mod63d,             isScalar, 8, rfGpr},
    {  30,     1, &itypeMD,  xrot,    "rldicr",        modSHtabc | modZeroc,           isScalar, 8, rfGpr},
    {  30,     2, &itypeMD,  xrot,     "rldic",        modSHtabc | mod63minusd,        isScalar, 8, rfGpr},
    {  30,     3, &itypeMD,  xrot,    "rldimi",        modSHtabc | modInsert | mod63minusd, isScalar, 8, rfGpr},
    {  30,     8, &itypeMDS, xrot,     "rldcl",        modSHtabc | modUseRB,           isScalar, 8, rfGpr},
    {  30,     9, &itypeMDS, xrot,     "rldcr",        modSHtabc | modZeroc,           isScalar, 8, rfGpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm31[] = {
    {  31,     0, &itypeX,   xcmp,          "cmp",     modVtab | modUnimpl | modTnomod, isScalar, 4, dNoFmt},
    {  31,     4, &itypeX, xtrap,           "tw",      modVtab | modUnimpl,            isTrap, 4, dNoFmt},
    {  31,     6, &itypeX,       0,         "lvsl",    modVtab | modUnimpl | modTVec,  isVec, 16, rfVpr},
    {  31,     7, &itypeX,        0,        "lvebx",   modVtab | modUnimpl | modTVec,  isRead, 1, rfVpr},
    {  31,     8, &itypeXO,       0,       "subfc",    modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,     9, &itypeXO,        0,      "mulhdu",   modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,    10, &itypeXO,  xaddc,        "addc",     modVtab,                        isScalar, 4, rfGpr},
    {  31,    11, &itypeXO,        0,      "mulhwu",   modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,    19, &itypeXFX,   xmfcr,       "mfcr",    modVt   | modUnimpl,            isScalar, 4, rfGpr}, // also MFOCRF
    {  31,    20, &itypeX,        0,        "lwarx",   modVtab | modUnimpl | modRsvn,  isRead, 4, rfGpr},
    {  31,    21, &itypeX,      0,          "ldx",     modVtab,                        isRead, 8, rfGpr},
    {  31,    23, &itypeX,       0,         "lwzx",    modVtab ,                       isRead, 4, rfGpr},
    {  31,    24, &itypeX,   xrot,          "slw",     modVtab | modUseRB | modShift | modROTL32, isScalar, 4, rfGpr},
    {  31,    26, &itypeX,         0,       "cntlzw",  modVta  | modUnimpl,            isScalar, 4, rfGpr},
    {  31,    27, &itypeX,   xrot,          "sld",     modVtab | modUseRB | modShift,  isScalar, 4, rfGpr},
    {  31,    28, &itypeX,   xlogx,          "and",    modVtab | modand,               isScalar, 4, rfGpr | dSwapTA},
    {  31,    32, &itypeX,   xcmpl,         "cmpl",    modVtab | modUnimpl | modTnomod, isScalar, 4,  dNoFmt},
    {  31,    38, &itypeX,       0,         "lvsr",    modVtab | modUnimpl | modTVec,  isVec, 16, rfVpr},
    {  31,    39, &itypeX,        0,        "lvehx",   modVtab | modUnimpl | modTVec,  isRead, 2, rfVpr},
    {  31,    40, &itypeXO,      0,        "subf",     modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,    53, &itypeX,       0,         "ldux",    modVtab,                        isRead, 8, rfGpr},
    {  31,    54, &itypeX,    xrarb,        "dcbst",   modVab  | modUnimpl,            isRead, 128, dNoFmt},
    {  31,    55, &itypeX,        0,        "lwzux",   modVtab | modUpd,               isRead, 4, rfGpr},
    {  31,    58, &itypeX,         0,       "cntlzd",  modVta  | modUnimpl,            isScalar, 4, rfGpr},
    {  31,    60, &itypeX,   xlogx,         "andc",    modVtab | modandc,              isScalar, 4, rfGpr | dSwapTA},
    {  31,    68, &itypeX, xtrap,           "td",      modVtab | modUnimpl,            isTrap, 8, 0},
    {  31,    71, &itypeX,        0,        "lvewx",   modVtab | modUnimpl | modTVec,  isRead, 4, rfVpr},
    {  31,    73, &itypeXO,       0,       "mulhd",    modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,    75, &itypeXO,       0,       "mulhw",    modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,    83, &itypeX,      xrt,        "mfmsr",   modVtab | modUnimpl | modPrv,   isScalar, 4, rfGpr},
    {  31,    84, &itypeX,        0,        "ldarx",   modVtab | modUnimpl | modRsvn,  isRead, 8, rfGpr},
    {  31,    86, &itypeX,   xrarb,         "dcbf",    modVab  | modUnimpl,            isRead, 128, dNoFmt},
    {  31,    87, &itypeX,       0,         "lbzx",    modVtab,                        isRead, 1, rfGpr},
    {  31,   103, &itypeX,      0,          "lvx",     modVtab | modUnimpl | modTVec,  isRead, 32, rfGpr},
    {  31,   104, &itypeXO,     0,         "neg",      modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   119, &itypeX,        0,        "lbzux",   modVtab | modUpd,               isRead, 1, rfGpr | dSwapTA},
    {  31,   124, &itypeX,  xlogx,          "nor",     modVtab | modnor,               isScalar, 4, rfGpr | dSwapTA},
    {  31,   135, &itypeX,         0,       "stvebx",  modVtab | modUnimpl | modTVec,  isWrite, 1, rfVpr},
    {  31,   136, &itypeXO,       0,       "subfe",    modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   138, &itypeXO,  xadde,        "adde",     modVtab,                        isScalar, 4, rfGpr},
    {  31,   144, &itypeXFX, xmtcrf,      "mtcrf",     modVt   | modUnimpl,            isScalar, 4, rfSpr | (sPcr << 8)}, // also MTOCRF
    {  31,   146, &itypeX,   xmtmsrx,       "mtmsr",   modVtab | modUnimpl | modPrv,   isScalar, 4, rfSpr | (sPmsr << 8)},
    {  31,   149, &itypeX,       0,         "stdx",    modVtab,                        isWrite, 8, rfGpr},
    {  31,   150, &itypeX,           0,     "stwcx",   modVtab | modUnimpl | modRsvn,  isWrite, 4, rfGpr},
    {  31,   151, &itypeX,       0,         "stwx",    modVtab,                        isWrite, 4, rfGpr},
    {  31,   167, &itypeX,         0,       "stvehx",  modVtab | modUnimpl | modTVec,  isWrite, 2, rfVpr},
    {  31,   178, &itypeX,   xmtmsrx,       "mtmsrd",  modVtab | modUnimpl | modPrv,   isScalar, 8, rfSpr | (sPmsr << 8)},
    {  31,   181, &itypeX,        0,        "stdux",   modVtab | modUpd,               isWrite, 8, rfGpr},
    {  31,   183, &itypeX,        0,        "stwux",   modVtab | modUpd,               isWrite, 4, rfGpr},
    {  31,   199, &itypeX,         0,       "stvewx",  modVtab | modUnimpl | modTVec,  isWrite, 4, rfVpr},
    {  31,   200, &itypeXO,        0,      "subfze",   modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   202, &itypeXO,  xaddze,       "addze",    modVtab,                        isScalar, 4, rfGpr},
    {  31,   210, &itypeX,   xmtsr,         "mtsr",    modVtab | modUnimpl,            isScalar, 4, rfSpr | (sPunimp << 8)},
    {  31,   214, &itypeX,           0,      "stdcx",  modVtab | modUnimpl | modRsvn,	isWrite, 8, rfGpr},
    {  31,   215, &itypeX,       0,         "stbx",    modVtab,                        isWrite, 1, rfGpr},
    {  31,   231, &itypeX,       0,         "stvx",    modVtab | modUnimpl | modTVec,  isWrite, 32, rfVpr},
    {  31,   232, &itypeXO,        0,      "subfme",   modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   233, &itypeXO,       0,       "mulld",    modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   234, &itypeXO,  xaddme,       "addme",    modVtab,                        isScalar, 4, rfGpr},
    {  31,   235, &itypeXO,       0,       "mullw",    modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   242, &itypeX,     xrtrb,       "mtsrin",  modVtab | modUnimpl | modPrv,   isScalar, 4, rfSpr | (sPmsr << 8)},
    {  31,   246, &itypeX,   xrarb,         "dcbtst",  modVab  | modUnimpl,            isRead, 128, dNoFmt},
    {  31,   247, &itypeX,        0,        "stbux",   modVtab | modUpd,               isWrite, 1, rfGpr},
    {  31,   266, &itypeXO,  xadd,         "add",      modVtab,                        isScalar, 4, rfGpr},
    {  31,   274, &itypeX,   xtlbiex,      "tlbiel",   modVtab | modUnimpl | modPrv,   isScalar, 0, dNoFmt},
    {  31,   278, &itypeX,   xrarb,         "dcbt",    modVab  | modUnimpl,            isRead, 128, dNoFmt},
    {  31,   279, &itypeX,       0,         "lhzx",    modVtab,                        isRead, 2, rfGpr},
    {  31,   284, &itypeX,  xlogx,          "eqv",     modVtab | modeqv,               isScalar, 4, rfGpr | dSwapTA},
    {  31,   306, &itypeX,  xtlbiex,        "tlbie",   modVtab | modUnimpl | modPrv,   isScalar, 0, 0},
    {  31,   310, &itypeX,        0,        "eciwx",   modVtab | modUnimpl | modPrv,   isScalar, 4, rfGpr},
    {  31,   311, &itypeX,        0,        "lhzux",   modVtab | modUpd,               isRead, 2, rfGpr},
    {  31,   316, &itypeX,  xlogx,          "xor",     modVtab | modor,        	       isScalar, 4, rfGpr | dSwapTA},
    {  31,   339, &itypeXFX, xmfspr,      "mfspr",     modVt,                          isScalar, 4, rfGpr},
    {  31,   341, &itypeX,       0,         "lwax",    modVtab | modSxtnd,             isRead, 4, rfGpr},
    {  31,   342, &itypeX,   xdstx,         "dst",     modVtab | modUnimpl,            isScalar, 4, rfGpr | dNoFmt},
    {  31,   343, &itypeX,       0,         "lhax",    modVtab | modSxtnd,             isRead, 2, rfGpr},
    {  31,   359, &itypeX,       0,         "lvxl",    modVtab | modUnimpl | modTVec,  isRead, 32, rfVpr},
    {  31,   370, &itypeX,   xoponly,       "tlbia",   modVtab | modUnimpl | modPrv,   isScalar, 4, dSkip},
    {  31,   371, &itypeXFX, xmftb,       "mftb",      modVt   | modUnimpl,            isScalar, 4, rfSpr | (sPtb << 8)},
    {  31,   373, &itypeX,        0,        "lwaux",   modVtab | modUpd,               isRead, 4, rfGpr},
    {  31,   374, &itypeX,   xdstx,         "dstst",   modVtab | modUnimpl,            isScalar, 128, 0},
    {  31,   375, &itypeX,        0,        "lhaux",   modVtab | modSxtnd | modUpd,    isRead, 2, rfGpr},
    {  31,   402, &itypeX,     xrtrb,       "slbmte",  modVtab | modUnimpl | modPrv,   isScalar, 4, rfGpr},
    {  31,   407, &itypeX,       0,         "sthx",    modVtab,                        isWrite, 2, rfGpr},
    {  31,   412, &itypeX,  xlogx,          "orc",     modVtab | modorc,               isScalar, 4, rfGpr | dSwapTA},
    {  31,   413, &itypeXS,       0,       "sradi",    modVta  | modUnimpl,            isScalar, 8, dSwapTA | rfGpr},
    {  31,   434, &itypeX,      xrb,        "slbie",   modVtab | modUnimpl | modPrv,   isScalar, 0, dNoFmt},
    {  31,   438, &itypeX,        0,        "ecowx",   modVtab | modUnimpl | modPrv,   isScalar, 4, rfGpr},
    {  31,   439, &itypeX,        0,        "sthux",   modVtab | modUpd,               isScalar, 4, rfGpr},
    {  31,   444, &itypeX, xlogx,           "or",      modVtab | modor,                isScalar,  4, rfGpr | dSwapTA},
    {  31,   457, &itypeXO,       0,       "divdu",    modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   459, &itypeXO,       0,       "divwu",    modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   467, &itypeXFX, xmtspr,      "mtspr",     modVt,                          isScalar, 8, rfSpr},
    {  31,   476, &itypeX,   xlogx,         "nand",    modVtab | modnand,              isScalar, 4, rfGpr | dSwapTA},
    {  31,   487, &itypeX,        0,        "stvxl",   modVtab | modUnimpl | modTVec,  isWrite, 32, rfVpr},
    {  31,   489, &itypeXO,      0,        "divd",     modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   491, &itypeXO,      0,        "divw",     modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   498, &itypeX,  xoponly,        "slbia",   modVtab | modUnimpl | modPrv,   isScalar, 0, dSkip},
    {  31,   512, &itypeX,   xmcrxr,        "mcrxr",   modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   533, &itypeX,       0,         "lswx",    modVtab | modUnimpl,            isRead, 4, rfGpr},
    {  31,   534, &itypeX,        0,        "lwbrx",   modVtab | modUnimpl,            isRead, 4, rfGpr},
    {  31,   535, &itypeX,       0,         "lfsx",    modVtab | modUnimpl,            isRead, 4, rfFpr},
    {  31,   536, &itypeX,   xrot,          "srw",     modVtab | modUseRB | modShift | modShRight | modROTL32, isScalar, 4, rfGpr},
    {  31,   539, &itypeX,   xrot,          "srd",     modVtab | modUseRB | modShift | modShRight, isScalar, 4, rfGpr},
    {  31,   566, &itypeX,   xoponly,       "tlbsync", modVtab | modUnimpl | modPrv,   isScalar, 0, dSkip},
    {  31,   567, &itypeX,        0,        "lfsux",   modVtab | modUpd,               isScalar, 4, rfFpr},
    {  31,   595, &itypeX,   xmfsr,         "mfsr",    modVtab | modUnimpl | modPrv,   isScalar, 4, rfSpr | (sPunimp << 8)},
    {  31,   597, &itypeX,   xlswi,         "lswi",    modVtab | modUnimpl,            isScalar, 4, rfGpr | dNoFmt},
    {  31,   598, &itypeX,   xsync,         "sync",    modVtab | modUnimpl,            isScalar, 0, dNoFmt},
    {  31,   599, &itypeX,       0,         "lfdx",    modVtab | modUnimpl,            isRead, 8, rfFpr},
    {  31,   631, &itypeX,        0,        "lfdux",   modVtab | modUnimpl | modUpd,   isRead, 8, rfFpr},
    {  31,   659, &itypeX,     xrtrb,       "mfsrin",  modVtab | modUnimpl | modPrv,   isScalar, 4, rfGpr},
    {  31,   661, &itypeX,        0,        "stswx",   modVtab | modUnimpl,            isWrite, 4, rfFpr},
    {  31,   662, &itypeX,         0,       "stwbrx",  modVtab,                        isWrite, 4, rfGpr},
    {  31,   663, &itypeX,        0,        "stfsx",   modVtab | modUnimpl,            isWrite, 4, rfFpr},
    {  31,   695, &itypeX,         0,       "stfsux",  modVtab | modUnimpl | modUpd,   isWrite, 4, rfFpr},
    {  31,   725, &itypeX,   xstswi,        "stswi",   modVtab | modUnimpl,            isWrite, 4, rfGpr | dNoFmt},
    {  31,   727, &itypeX,        0,        "stfdx",   modVtab | modUnimpl,            isWrite, 8, rfFpr},
    {  31,   759, &itypeX,         0,       "stfdux",  modVtab | modUnimpl | modUpd,   isWrite, 8, rfFpr},
    {  31,   790, &itypeX,        0,        "lhbrx",   modVtab | modUnimpl,            isScalar, 42, rfGpr},
    {  31,   792, &itypeX,       0,         "sraw",    modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   794, &itypeX,       0,         "srad",    modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   822, &itypeX,   xdss,          "dss",     modVtab | modUnimpl,            isScalar, 4, rfGpr},
    {  31,   824, &itypeX,   xsrawi,        "srawi",   modVtab | modUnimpl,            isScalar, 4, dSwapTA | dNoFmt | rfGpr},
    {  31,   851, &itypeX,      xrtrb,      "slbmfev", modVtab | modUnimpl | modPrv,   isScalar, 0, 0},
    {  31,   854, &itypeX,   xoponly,       "eieio",   modVtab,                        isScalar, 4, dSkip},
    {  31,   915, &itypeX,      xrtrb,      "slbmfee", modVtab | modUnimpl | modPrv,   isScalar, 4, rfGpr},
    {  31,   918, &itypeX,         0,       "sthbrx",  modVtab,                        isWrite, 2, rfGpr},
    {  31,   922, &itypeX,     xext,        "extsh",   modVta  | modSxtnd,             isScalar, 2, dSwapTA | rfGpr},
    {  31,   954, &itypeX,     xext,        "extsb",   modVta  | modSxtnd,             isScalar, 1, dSwapTA | rfGpr},
    {  31,   982, &itypeX,   xrarb,         "icbi",    modVta,                         isRead, 128, dNoFmt},
    {  31,   983, &itypeX,         0,       "stfiwx",  modVtab | modUnimpl,            isWrite, 4, rfFpr},
    {  31,   986, &itypeX,     xext,        "extsw",   modVta  | modSxtnd,             isScalar, 4, dSwapTA | rfGpr},
    {  31,  1014, &itypeX,   xrarb,         "dcbz",    modVab  | modUnimpl,            isWrite, 128, dNoFmt},
     {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm32[] = {
    {  32,     0, &itypeD,      0,          "lwz",     modVta,                         isRead, 4, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm33[] = {
    {  33,     0, &itypeD,       0,         "lwzu",    modVta | modUpd,                isRead, 4, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm34[] = {
    {  34,     0, &itypeD,      0,          "lbz",     modVta,                         isRead, 1, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm35[] = {
    {  35,     0, &itypeD,       0,         "lbzu",    modVta | modUpd,                isRead, 1, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm36[] = {
    {  36,     0, &itypeD,      0,          "stw",     modVta | modTnomod,             isWrite, 4, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm37[] = {
    {  37,     0, &itypeD,       0,         "stwu",    modVta | modUpd | modTnomod,    isWrite, 4, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm38[] = {
    {  38,     0, &itypeD,      0,          "stb",     modVta | modTnomod,             isWrite, 1, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm39[] = {
    {  39,     0, &itypeD,       0,         "stbu",    modVta | modUpd | modTnomod,    isWrite, 1, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm40[] = {
    {  40,     0, &itypeD,      0,          "lhz",     modVta,                         isRead, 2, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm41[] = {
    {  41,     0, &itypeD,       0,         "lhzu",    modVta | modUpd,                isRead, 2, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm42[] = {
    {  42,     0, &itypeD,      0,          "lha",     modVta | modSxtnd,              isRead, 2, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm43[] = {
    {  43,     0, &itypeD,       0,         "lhau",    modVta | modUpd | modSxtnd,     isRead, 2, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm44[] = {
    {  44,     0, &itypeD,      0,          "sth",     modVta | modTnomod,             isWrite, 2, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm45[] = {
    {  45,     0, &itypeD,       0,         "sthu",    modVta | modUpd | modTnomod,    isWrite, 2, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm46[] = {
    {  46,     0, &itypeD,    xmw,          "lmw",     modVta | modM4,                 isRead, 0, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm47[] = {
    {  47,     0, &itypeD,     xmw,         "stmw",    modVta | modM4 | modTnomod,     isWrite, 0, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm48[] = {
    {  48,     0, &itypeD,      0,          "lfs",     modVta | modTFpu,               isRead, 4, rfFpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm49[] = {
    {  49,     0, &itypeD,       0,         "lfsu",    modVta | modUpd | modTFpu,      isRead, 4, rfFpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm50[] = {
    {  50,     0, &itypeD,      0,          "lfd",     modVta | modTFpu,               isRead, 8, rfFpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm51[] = {
    {  51,     0, &itypeD,       0,         "lfdu",    modVta | modUpd | modTFpu,      isRead, 8, rfFpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm52[] = {
    {  52,     0, &itypeD,       0,         "stfs",    modVta | modTnomod | modTFpu,   isWrite, 4, rfFpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm53[] = {
    {  53,     0, &itypeD,        0,        "stfsu",   modVta | modUpd | modTnomod | modTFpu, isWrite, 4, rfFpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm54[] = {
    {  54,     0, &itypeD,       0,         "stfd",    modVta | modTnomod | modTFpu,   isWrite, 8, rfFpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm55[] = {
    {  55,     0, &itypeD,        0,        "stfdu",   modVta | modUpd | modTnomod | modTFpu, isWrite, 8, rfFpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm58[] = {
    {  58,     0, &itypeDS,    0,           "ld",      modVta,                         isRead, 8, rfGpr | dBd},
    {  58,     1, &itypeDS,     0,          "ldu",     modVta | modUpd,                isRead, 8, rfGpr | dBd},
    {  58,     2, &itypeDS,     0,          "lwa",     modVta | modSxtnd,              isRead, 4, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm59[] = {
    {  59,    18, &itypeA,   0,        "fdivs",        modFAtab  | modUnimpl,          isFpu, 4, rfFpr},
    {  59,    20, &itypeA,   0,        "fsubs",        modFAtab  | modUnimpl,          isFpu, 4, rfFpr},
    {  59,    21, &itypeA,   0,        "fadds",        modFAtab  | modUnimpl,          isFpu, 4, rfFpr},
    {  59,    22, &itypeA,   0,       "fsqrts",        modFAtb   | modUnimpl,          isFpu, 4, rfFpr},
    {  59,    24, &itypeA,   0,         "fres",        modFAtb   | modUnimpl,          isFpu, 4, rfFpr},
    {  59,    25, &itypeA,   0,        "fmuls",        modFAtac  | modUnimpl,          isFpu, 4, rfFpr},
    {  59,    28, &itypeA,   0,       "fmsubs",        modFAtabc | modUnimpl,          isFpu, 4, rfFpr},
    {  59,    29, &itypeA,   0,       "fmadds",        modFAtabc | modUnimpl,          isFpu, 4, rfFpr},
    {  59,    30, &itypeA,   0,      "fnmsubs",        modFAtabc | modUnimpl,          isFpu, 4, rfFpr},
    {  59,    31, &itypeA,   0,      "fnmadds",        modFAtabc | modUnimpl,          isFpu, 4, rfFpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm62[] = {
    {  62,     0, &itypeDS,     0,          "std",     modVta | modTnomod,             isWrite, 8, rfGpr | dBd},
    {  62,     1, &itypeDS,      0,         "stdu",    modVta    | modUpd | modTnomod, isWrite, 8, rfGpr | dBd},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst ppcm63[] = {
    {  63,     0, &itypeX,   xfcmpx,        "fcmpu",   modVtab   | modUnimpl | modTnomod,   isFpu, 4, dNoFmt | rfFpr},
    {  63,    12, &itypeX,   xftfb,         "frsp",    modVtab   | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    14, &itypeX,    xftfb,        "fctiw",   modVtab   | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    15, &itypeX,     xftfb,       "fctiwz",  modVtab   | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    18, &itypeA,   0,         "fdiv",        modFAtab  | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    20, &itypeA,   0,         "fsub",        modFAtab  | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    21, &itypeA,   0,         "fadd",        modFAtab  | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    22, &itypeA,   0,        "fsqrt",        modFAtb   | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    23, &itypeA,   0,         "fsel",        modFAtabc | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    25, &itypeA,   0,         "fmul",        modFAtac  | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    26, &itypeA,   0,      "frsqrte",        modFAtb   | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    28, &itypeA,   0,        "fmsub",        modFAtabc | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    29, &itypeA,   0,        "fmadd",        modFAtabc | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    30, &itypeA,   0,       "fnmsub",        modFAtabc | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    31, &itypeA,   0,       "fnmadd",        modFAtabc | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    32, &itypeX,   xfcmpx,        "fcmpo",   modVtab   | modUnimpl | modTnomod,  isFpu, 4, dNoFmt | rfFpr},
    {  63,    38, &itypeX,       xrt,       "mtfsb1",  modVtab   | modUnimpl,          isFpu, 4, rfSpr | (sPfpscr << 8)},
    {  63,    40, &itypeX,   xftfb,         "fneg",    modVtab   | modUnimpl,          isFpu, 4, rfFpr},
    {  63,    64, &itypeX,   xmcrfs,        "mcrfs",   modVtab   | modUnimpl,          isFpu, 4, rfSpr | (sPcr << 8)},
    {  63,    70, &itypeX,       xrt,       "mtfsb0",  modVtab   | modUnimpl,          isFpu, 4, rfSpr | (sPfpscr << 8)},
    {  63,    72, &itypeX,  xftfb,          "fmr",     modVtab   | modUnimpl,          isFpu, 4, rfFpr},
    {  63,   134, &itypeX,   xmtfsfi,       "mtfsfi",  modVtab   | modUnimpl,          isFpu, 4, rfSpr | (sPfpscr << 8)},
    {  63,   136, &itypeX,    xftfb,        "fnabs",   modVtab   | modUnimpl,          isFpu, 4, rfFpr},
    {  63,   264, &itypeX,   xftfb,         "fabs",    modVtab   | modUnimpl,          isFpu, 4, rfFpr},
    {  63,   583, &itypeX,   xmffs,         "mffs",    modVtab   | modUnimpl,          isFpu, 4, rfSpr | (sPfpscr << 8)},
    {  63,   711, &itypeXFL, 0,             "mtfsf",   modVtb    | modUnimpl,          isFpu, 4, rfSpr | (sPfpscr << 8)},
    {  63,   814, &itypeX,   xftfb,         "fctid",   modVtab   | modUnimpl,          isFpu, 4, rfFpr},
    {  63,   815, &itypeX,     xftfb,       "fctidz",  modVtab   | modUnimpl,          isFpu, 4, rfFpr},
    {  63,   846, &itypeX,    xftfb,        "fcfid",   modVtab   | modUnimpl,          isFpu, 4, rfFpr},
    {-1, -1, 0, 0, 0, 0, 0, 0}
};


ppcinst *majops[] = {
    0,
    0,
    ppcm02,
    ppcm03,
    ppcm04,
    0,
    0,
    ppcm07,
    ppcm08,
    0,
    ppcm10,
    ppcm11,
    ppcm12,
    ppcm13,
    ppcm14,
    ppcm15,
    ppcm16,
    ppcm17,
    ppcm18,
    ppcm19,
    ppcm20,
    ppcm21,
    0,
    ppcm23,
    ppcm24,
    ppcm25,
    ppcm26,
    ppcm27,
    ppcm28,
    ppcm29,
    ppcm30,
    ppcm31,
    ppcm32,
    ppcm33,
    ppcm34,
    ppcm35,
    ppcm36,
    ppcm37,
    ppcm38,
    ppcm39,
    ppcm40,
    ppcm41,
    ppcm42,
    ppcm43,
    ppcm44,
    ppcm45,
    ppcm46,
    ppcm47,
    ppcm48,
    ppcm49,
    ppcm50,
    ppcm51,
    ppcm52,
    ppcm53,
    ppcm54,
    ppcm55,
    0,
    0,
    ppcm58,
    ppcm59,
    0,
    0,
    ppcm62,
    ppcm63
};
