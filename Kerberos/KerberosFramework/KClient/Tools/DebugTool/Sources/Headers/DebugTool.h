#include <KrbDriver.h>

#define SetSelectorBit_(selectors, bit) \
	selectors [3 - ((bit) / 32)] |= 1 << ((bit) % 32)

#define ClearSelectorBit_(selectors, bit) \
	selectors [3 - ((bit) / 32)] &= ~(1 << ((bit) % 32))

#define TestSelectorBit_(selectors, bit) \
	((selectors [3 - ((bit) / 32)] & (1 << ((bit) % 32))) != 0)
	
#define ClearAllSelectorBits_(selectors) \
	selectors [0] = selectors [1] = selectors [2] = selectors [3] = 0

#define SetAllSelectorBits_(selectors) \
	selectors [0] = selectors [1] = selectors [2] = selectors [3] = 0xFFFFFFFF

void PrintAllInfo (void);
void PrintSelectorsCalled (SKClientDebugInfo* inDebugInfo);
void ClearSelectorsCalled (SKClientDebugInfo* inDebugInfo);
void PrintSelectorsDebugged (SKClientDebugInfo* inDebugInfo);
void SetAllSelectorsDebugged (SKClientDebugInfo* inDebugInfo);
void ClearAllSelectorsDebugged (SKClientDebugInfo* inDebugInfo);
void SetSelectorDebugged (SKClientDebugInfo* inDebugInfo, UInt32 selector);
void ClearSelectorDebugged (SKClientDebugInfo* inDebugInfo, UInt32 selector);

OSErr GetDebugInfo (SKClientDebugInfo** outDebugInfoPtr);