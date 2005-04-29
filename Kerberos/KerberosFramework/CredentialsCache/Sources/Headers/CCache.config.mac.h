/* Platform and runtime macros. Derived from Mac OS Universal Headers 3.2 */

/****************************************************************************************************

	TARGET_CPU_*
	These conditionals specify which microprocessor instruction set is being
	generated.  At most one of these is true, the rest are false.

		TARGET_CPU_PPC			- Compiler is generating PowerPC instructions
		TARGET_CPU_68K			- Compiler is generating 680x0 instructions
		TARGET_CPU_X86			- Compiler is generating x86 instructions
		TARGET_CPU_MIPS			- Compiler is generating MIPS instructions
		TARGET_CPU_SPARC		- Compiler is generating Sparc instructions
		TARGET_CPU_ALPHA		- Compiler is generating Dec Alpha instructions


	TARGET_OS_*
	These conditionals specify in which Operating System the generated code will
	run. At most one of the these is true, the rest are false.

		TARGET_OS_MAC			- Generate code will run under Mac OS
		TARGET_OS_WIN32			- Generate code will run under 32-bit Windows
		TARGET_OS_UNIX			- Generate code will run under some unix 


	TARGET_RT_*	
	These conditionals specify in which runtime the generated code will
	run. This is needed when the OS and CPU support more than one runtime
	(e.g. MacOS on 68K supports CFM68K and Classic 68k).

		TARGET_RT_LITTLE_ENDIAN	- Generated code uses little endian format for integers
		TARGET_RT_BIG_ENDIAN	- Generated code uses big endian format for integers 	
		TARGET_RT_MAC_CFM		- TARGET_OS_MAC is true and CFM68K or PowerPC CFM being used	
		TARGET_RT_MAC_68881		- TARGET_OS_MAC is true and 68881 floating point instructions used	

****************************************************************************************************/

/* On Mac, we get all of those except for PRAGMA_MARK for free */
#include <TargetConditionals.h>
#include <CoreServices/CoreServices.h>
#include <Kerberos/KerberosDebug.h>

#define PRAGMA_MARK		1

/****************************************************************************************************

	Debugging macros
	
	CCIDEBUG_ASSERT				- assertion macro (test a condition, display a message, and abort)
	CCIDEBUG_SIGNAL				- signal macro (display a message and abort)
	CCIDEBUG_VALIDPOINTER		- pointer validation function
	CCIDEBUG					- 1 if debugging inmformation should be compiled in

****************************************************************************************************/

#define	CCIDEBUG_ASSERT                 Assert_
#define CCIDEBUG_SIGNAL                 SignalCStr_
#define CCIDEBUG_THROW                  DebugThrow_
#define CCIDEBUG_VALIDPOINTER(x)        (true)

#define CCI_DEBUG 1  // Always on.  Debugging set at runtime.
