/* Automatically generated.  Do not edit */
/* See the mkopcodeh.awk script for details */
#define OP_ReadCookie                           1
#define OP_AutoCommit                           2
#define OP_Found                                3
#define OP_NullRow                              4
#define OP_Lt                                  72   /* same as TK_LT       */
#define OP_MoveLe                               5
#define OP_Variable                             6
#define OP_Pull                                 7
#define OP_RealAffinity                         8
#define OP_Sort                                 9
#define OP_IfNot                               10
#define OP_Gosub                               11
#define OP_Add                                 79   /* same as TK_PLUS     */
#define OP_NotFound                            12
#define OP_IsNull                              66   /* same as TK_ISNULL   */
#define OP_MoveLt                              13
#define OP_Rowid                               14
#define OP_CreateIndex                         15
#define OP_Push                                17
#define OP_Explain                             18
#define OP_Statement                           19
#define OP_Callback                            20
#define OP_MemLoad                             21
#define OP_DropIndex                           22
#define OP_Null                                23
#define OP_ToInt                              142   /* same as TK_TO_INT   */
#define OP_Int64                               24
#define OP_LoadAnalysis                        25
#define OP_IdxInsert                           26
#define OP_VUpdate                             27
#define OP_Next                                28
#define OP_SetNumColumns                       29
#define OP_ToNumeric                          141   /* same as TK_TO_NUMERIC*/
#define OP_Ge                                  73   /* same as TK_GE       */
#define OP_BitNot                              87   /* same as TK_BITNOT   */
#define OP_MemInt                              30
#define OP_Dup                                 31
#define OP_Rewind                              32
#define OP_Multiply                            81   /* same as TK_STAR     */
#define OP_ToReal                             143   /* same as TK_TO_REAL  */
#define OP_Gt                                  70   /* same as TK_GT       */
#define OP_Last                                33
#define OP_MustBeInt                           34
#define OP_Ne                                  68   /* same as TK_NE       */
#define OP_MoveGe                              35
#define OP_String                              36
#define OP_VFilter                             37
#define OP_ForceInt                            38
#define OP_Close                               39
#define OP_AggFinal                            40
#define OP_AbsValue                            41
#define OP_RowData                             42
#define OP_IdxRowid                            43
#define OP_BitOr                               76   /* same as TK_BITOR    */
#define OP_NotNull                             67   /* same as TK_NOTNULL  */
#define OP_MoveGt                              44
#define OP_Not                                 16   /* same as TK_NOT      */
#define OP_OpenPseudo                          45
#define OP_Halt                                46
#define OP_MemMove                             47
#define OP_NewRowid                            48
#define OP_Real                               126   /* same as TK_FLOAT    */
#define OP_IdxLT                               49
#define OP_Distinct                            50
#define OP_MemMax                              51
#define OP_Function                            52
#define OP_IntegrityCk                         53
#define OP_Remainder                           83   /* same as TK_REM      */
#define OP_HexBlob                            127   /* same as TK_BLOB     */
#define OP_ShiftLeft                           77   /* same as TK_LSHIFT   */
#define OP_FifoWrite                           54
#define OP_BitAnd                              75   /* same as TK_BITAND   */
#define OP_Or                                  61   /* same as TK_OR       */
#define OP_NotExists                           55
#define OP_VDestroy                            56
#define OP_MemStore                            57
#define OP_IdxDelete                           58
#define OP_Vacuum                              59
#define OP_If                                  60
#define OP_Destroy                             63
#define OP_AggStep                             64
#define OP_Clear                               65
#define OP_Insert                              74
#define OP_VBegin                              86
#define OP_IdxGE                               89
#define OP_OpenEphemeral                       90
#define OP_Divide                              82   /* same as TK_SLASH    */
#define OP_String8                             88   /* same as TK_STRING   */
#define OP_IfMemZero                           91
#define OP_Concat                              84   /* same as TK_CONCAT   */
#define OP_VRowid                              92
#define OP_MakeRecord                          93
#define OP_SetCookie                           94
#define OP_Prev                                95
#define OP_ContextPush                         96
#define OP_DropTrigger                         97
#define OP_IdxGT                               98
#define OP_MemNull                             99
#define OP_IfMemNeg                           100
#define OP_And                                 62   /* same as TK_AND      */
#define OP_VColumn                            101
#define OP_Return                             102
#define OP_OpenWrite                          103
#define OP_Integer                            104
#define OP_Transaction                        105
#define OP_CollSeq                            106
#define OP_ToBlob                             140   /* same as TK_TO_BLOB  */
#define OP_Sequence                           107
#define OP_ContextPop                         108
#define OP_ShiftRight                          78   /* same as TK_RSHIFT   */
#define OP_VCreate                            109
#define OP_CreateTable                        110
#define OP_AddImm                             111
#define OP_ToText                             139   /* same as TK_TO_TEXT  */
#define OP_IdxIsNull                          112
#define OP_DropTable                          113
#define OP_IsUnique                           114
#define OP_VOpen                              115
#define OP_Noop                               116
#define OP_RowKey                             117
#define OP_Expire                             118
#define OP_FifoRead                           119
#define OP_Delete                             120
#define OP_IfMemPos                           121
#define OP_Subtract                            80   /* same as TK_MINUS    */
#define OP_MemIncr                            122
#define OP_Blob                               123
#define OP_MakeIdxRec                         124
#define OP_Goto                               125
#define OP_Negative                            85   /* same as TK_UMINUS   */
#define OP_ParseSchema                        128
#define OP_Eq                                  69   /* same as TK_EQ       */
#define OP_VNext                              129
#define OP_Pop                                130
#define OP_Le                                  71   /* same as TK_LE       */
#define OP_TableLock                          131
#define OP_VerifyCookie                       132
#define OP_Column                             133
#define OP_OpenRead                           134
#define OP_ResetCount                         135

/* The following opcode values are never used */
#define OP_NotUsed_136                        136
#define OP_NotUsed_137                        137
#define OP_NotUsed_138                        138

/* Opcodes that are guaranteed to never push a value onto the stack
** contain a 1 their corresponding position of the following mask
** set.  See the opcodeNoPush() function in vdbeaux.c  */
#define NOPUSH_MASK_0 0x3fbc
#define NOPUSH_MASK_1 0x3e5b
#define NOPUSH_MASK_2 0x71ef
#define NOPUSH_MASK_3 0x7fce
#define NOPUSH_MASK_4 0xffff
#define NOPUSH_MASK_5 0xceef
#define NOPUSH_MASK_6 0xb6d7
#define NOPUSH_MASK_7 0x275f
#define NOPUSH_MASK_8 0xf8df
#define NOPUSH_MASK_9 0x0000
