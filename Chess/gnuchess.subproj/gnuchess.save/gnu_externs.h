/*
 * gnu_extern.h
 *
 * glenn reid @ next       Wed Apr 18 20:09:25 PDT 1990
 *
 * These definitions are included in gnuchess.c if the NeXTMOD flag is set
 * during compile.  These prevent warning message from the compiler about
 * implicitly declared external functions.  This is better than compiling
 * with the "No warnings" flag which was done previously.
 */
    extern malloc ();
    extern UnmakeMove ();
    extern MoveList ();
    extern algbr ();
    extern strcmp ();
    extern MakeMove ();
    extern SqAtakd ();
    extern ShowMessage ();
    extern UpdateDisplay ();
    extern ElapsedTime ();
    extern rand ();
    extern ClrScreen ();
    extern SetTimeControl ();
    extern SelectLevel ();
    extern InitializeStats ();
    extern time ();
    extern GetOpenings ();
    extern strcpy ();
    extern ShowDepth ();
    extern search ();
    extern ZeroTTable ();
    extern pick ();
    extern ShowResults ();
    extern abs ();
    extern SelectMoveStart ();
    extern SelectMoveEnd ();
    extern ExaminePosition ();
    extern ScorePosition ();
    extern ShowSidetomove ();
    extern SearchStartStuff ();
    extern bzero ();
    extern OpeningBook ();
    extern OutputMove ();
    extern GameEnd ();
    extern srand ();
    extern repetition ();
    extern evaluate ();
    extern ProbeTTable ();
    extern CaptureList ();
    extern ShowCurrentMove ();
    extern PutInTTable ();
    extern ataks ();
    extern ScoreLoneKing ();
    extern GenMoves ();
    extern castle ();
    extern LinkMove ();
    extern UpdateHashbd ();
    extern UpdatePieceList ();
    extern distance ();
    extern UpdateWeights ();
    extern SqValue ();
    extern ScoreKPK ();
    extern ScoreKBNK ();
    extern BRscan ();
    extern KingScan ();
    extern trapped ();
    extern CopyBoard ();
    extern BlendBoard ();

