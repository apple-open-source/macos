/*
 *  Hello World for the CodeWarrior
 *  © 1997 Metrowerks Corp.
 *
 *  Questions and comments to:
 *       <mailto:support@metrowerks.com>
 *       <http://www.metrowerks.com/>
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "DebugTool.h"

int main(void)
{
	OSErr err;
	SKClientDebugInfo* debugInfo;
	int choice, selector;

	for (;;) {
		printf ("KClient debugging tool 1.0\n");
		printf ("\t1. Print selectors called\n");
		printf ("\t2. Clear selectors called\n");
		printf ("\t3. Print selectors debugged\n");
		printf ("\t4. Set selector debugged\n");
		printf ("\t5. Set selector not debugged\n");
		printf ("\t6. Set all selectors debugged\n");
		printf ("\t7. Set all selectors not debugged\n");
		scanf ("%d", &choice);

		err = GetDebugInfo (&debugInfo);
		if (err != noErr) {
			printf ("Got error %d from KClient when trying to get debug info.\n", err);
			continue;
		}
		
		if (debugInfo -> version != 1) {
			printf ("Unknown DebugInfo version returned from KClient.%n");
			continue;
		}

		switch (choice) {
			case 1:
				PrintSelectorsCalled (debugInfo);
				break;
			
			case 2:
				ClearSelectorsCalled (debugInfo);
				break;
			
			case 3:
				PrintSelectorsDebugged (debugInfo);
				break;
			
			case 4:
				scanf ("%d", &selector);
				SetSelectorDebugged (debugInfo, selector);
				break;
			
			case 5:
				scanf ("%d", &selector);
				ClearSelectorDebugged (debugInfo, selector);
				break;
				
			case 6:
				SetAllSelectorsDebugged (debugInfo);
				break;
				
			case 7:
				ClearAllSelectorsDebugged (debugInfo);
				break;

			default:
				printf ("Invalid choice.\n");
		}
	}
	return 0;
}

