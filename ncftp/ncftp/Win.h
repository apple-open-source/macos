/* Win.h */

#ifndef _win_h_
#define _win_h_ 1

/* For Error(): */
#define kDoPerror		1
#define kDontPerror		0

#define kUseDefaultPrompt NULL
#define kVisualModePrompt ""
#define kLineModePrompt "NcFTP"
#define kPromptTail "> "
#define kPromptLimit 45

#define kListWinIndex 0
#define kPromptWinIndex 1
#define kBarWinIndex 2
#define kInputWinIndex 3

#ifndef HAVE_CURS_SET
#	define curs_set(v)
#endif
 
#define kNormal		00000
#define kStandout	00001
#define kUnderline	00002
#define kReverse	00004
#define kBlink		00010
#define kDim		00020
#define kBold		00040

#ifdef HAVE_STDARG_H
void DebugMsg(char *fmt0, ...);
void TraceMsg(char *fmt0, ...);
void PrintF(char *fmt0, ...);
void MultiLinePrintF(char *fmt0, ...);
void BoldPrintF(char *fmt0, ...);
void EPrintF(char *fmt0, ...);
void Error(int pError0, char *fmt0, ...);
#else
void DebugMsg();
void TraceMsg();
void PrintF();
void MultiLinePrintF();
void BoldPrintF();
void EPrintF();
void Error();
#endif

void EndWin(void);
void Exit(int);
void SaveScreen(void);
void RestoreScreen(int);
void Beep(int);
void UpdateScreen(int);
void FlushListWindow(void);
char *Gets(char *, size_t);
void GetAnswer(char *, char *, size_t, int);
void SetBar(char *, char *, char *, int, int);
void SetDefaultBar(void);
void SetScreenInfo(void);
void PrintToListWindow(char *, int);
void MultiLineInit(void);
void MakeBottomLine(char *, int, int);
void SetPrompt(char *);
void InitWindows(void);

#endif	/* _win_h_ */
