/*
** xgc
**
** xgc.h
**
** Initialization of structures, etc.
*/
/* $XFree86: xc/programs/xgc/main.h,v 1.3 2000/02/17 14:00:36 dawes Exp $ */

/* The three columns in the XgcData arrays are:
**   name: the name of the toggle button
**   text: the corresponding text in the xgc syntax
**   code: the integer that the text corresponds to, for sending stuff
**         to X calls, etc.
*/

XgcData FunctionData[NUM_FUNCTIONS] = {
  {"clear",        "clear",        GXclear},
  {"and",          "and",          GXand},
  {"andReverse",   "andReverse",   GXandReverse},
  {"copy",         "copy",         GXcopy},
  {"andInverted",  "andInverted",  GXandInverted},
  {"noop",         "noop",         GXnoop},
  {"xor",          "xor",          GXxor},
  {"or",           "or",           GXor},
  {"nor",          "nor",          GXnor},
  {"equiv",        "equiv",        GXequiv},
  {"invert",       "invert",       GXinvert},
  {"orReverse",    "orReverse",    GXorReverse},
  {"copyInverted", "copyInverted", GXcopyInverted},
  {"orInverted",   "orInverted",   GXorInverted},
  {"nand",         "nand",         GXnand},
  {"set",          "set",          GXset}
}; 

/* The two rows in the XgcStuff structure are:
**   name of label, xgc syntax text, # of toggles, # of columns of toggles
**     (0 columns means 1 row, as many columns as necessary)
**   appropriate XgcData
*/

XgcStuff FunctionStuff = {
  {"Function","function",NUM_FUNCTIONS,4},
  FunctionData
};

XgcData TestData[NUM_TESTS] = {
  {"Copy Area",          "CopyArea",      CopyArea},
  {"Copy Plane",         "CopyPlane",     CopyPlane},
  {"Points",             "PolyPoint",     PolyPoint},
  {"Lines",              "PolyLine",      PolyLine},
  {"Segments",           "PolySegment",   PolySegment},
  {"Rectangles",         "PolyRectangle", PolyRectangle},
  {"Arcs",               "PolyArc",       PolyArc},
  {"(Filled Polygons)",  "FillPolygon",   FillPolygon},
  {"Filled Rectangles",  "PolyFillRect",  PolyFillRect},
  {"Filled Arcs",        "PolyFillArc",   PolyFillArc},
  {"Put Image",          "PutImage",      PutImage},
  {"(Get Image)",        "GetImage",      GetImage},
  {"Text 8",             "PolyText8",     PolyText8},
  {"Image Text 8",       "ImageText8",    ImageText8},
  {"Text 16",            "PolyText16",    PolyText16},
  {"Image Text 16",      "ImageText16",   ImageText16}
};

XgcStuff TestStuff = {
  {"Test","test",NUM_TESTS,2},
  TestData
};

XgcData LinestyleData[NUM_LINESTYLES] = {
  {"Solid",      "Solid",       LineSolid},
  {"OnOffDash",  "OnOffDash",   LineOnOffDash},
  {"DoubleDash", "DoubleDash",  LineDoubleDash}
};

XgcStuff LinestyleStuff = {
  {"LineStyle","linestyle",NUM_LINESTYLES,0},
  LinestyleData
};

XgcData CapstyleData[NUM_CAPSTYLES] = {
  {"NotLast",    "NotLast",     CapNotLast},
  {"Butt",       "Butt",        CapButt},
  {"Round",      "Round",       CapRound},
  {"Projecting", "Projecting",  CapProjecting}
};

XgcStuff CapstyleStuff = {
  {"CapStyle","capstyle",NUM_CAPSTYLES,2},
  CapstyleData
};

XgcData JoinstyleData[NUM_JOINSTYLES] = {
  {"Miter",   "Miter",   JoinMiter},
  {"Round",   "Round",   JoinRound},
  {"Bevel",   "Bevel",   JoinBevel}
};

XgcStuff JoinstyleStuff = {
  {"JoinStyle","joinstyle",NUM_JOINSTYLES,0},
  JoinstyleData
};

XgcData FillstyleData[NUM_FILLSTYLES] = {
  {"Solid",          "Solid",          FillSolid},
  {"Tiled",          "Tiled",          FillTiled},
  {"Stippled",       "Stippled",       FillStippled},
  {"OpaqueStippled", "OpaqueStippled", FillOpaqueStippled}
};

XgcStuff FillstyleStuff = {
  {"FillStyle","fillstyle",NUM_FILLSTYLES,2},
  FillstyleData
};

XgcData FillruleData[NUM_FILLRULES] = {
  {"EvenOdd",  "EvenOdd",  EvenOddRule},
  {"Winding",  "Winding",  WindingRule}
};

XgcStuff FillruleStuff = {
  {"FillRule","fillrule",NUM_FILLRULES,0},
  FillruleData
};

XgcData ArcmodeData[NUM_ARCMODES] = {
  {"Chord",    "Chord",    ArcChord},
 {"PieSlice", "PieSlice", ArcPieSlice}
};

XgcStuff ArcmodeStuff = {
  {"ArcMode","arcmode",NUM_ARCMODES,0},
  ArcmodeData
};

/* Pointers to all the Xgcstuffs so we can run them through a loop */

static XgcStuff *Everything[8] = {
  &FunctionStuff,
  &LinestyleStuff,
  &CapstyleStuff,
  &JoinstyleStuff,
  &FillstyleStuff,
  &FillruleStuff,
  &ArcmodeStuff,
  &TestStuff
};
