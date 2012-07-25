/*	This module contains routines to measure execution time of a routine.  See
	additional information in Clock Services.h.

	Written by Eric Postpischil, Apple, Inc.
*/


#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ClockServices.h"

#define	IntelProcessor	(defined __i386__ || defined __x86_64__)


// Select the timing method to be used.

	// Define constants to name the methods.
	#define	TM_TB	0	// PowerPC's time-base registers.
	#define	TM_TSC	1	// IA-32's time-stamp counter.
	#define	TM_PMC	2	// Performance monitor counters.  Works only on G5.
	#define	TM_UT	3	// Carbon's UpTime.
	#define TM_TOD	4	// Open Unix's gettimeofday.
	#define	TM_MACH	5	// mach_absolute_time.

	// Set the method in TimingMethod.
	#if defined __ppc__ || defined __ppc64__
		#define	TimingMethod	TM_MACH	// Method to use on PowerPC.
	#elif defined __i386__
		#define	TimingMethod	TM_MACH	// Method to use on IA-32.
	#elif defined __x86_64__
		#define	TimingMethod	TM_MACH	// Method to use on 64-bit Intel.
	#else
		#define	TimingMethod	TM_MACH	// Method to use elsewhere.
	#endif

	/*	Using the PMCs on earlier processors than the G5's requires changing
		the event number in the first chudSetPMCEvent and using only one PMC to
		count CPU cycles, because there is no event for overflow into another
		PMC.
	*/

	// Set a flag based on the timing method in use.
	#if TimingMethod == TM_PMC
		// To use the performance monitor counters, we need CHUD facilities.
		#define	NeedCHUD
	#endif


#if TimingMethod == TM_UT

	#include <Carbon/Carbon.h>	// For Uptime.

#elif TimingMethod == TM_TOD

	#include <sys/time.h>		// For gettimeofday.

#elif TimingMethod == TM_MACH

	#include <mach/mach_time.h>		// For mach_absolute_time.

#endif


/*	Define ClockValue type.  Four things are defined:

   		ClockValue is a type name.

		ClockMax is a value that can be used when initializing a ClockValue
		that acts as the maximum value a ClockValue can have.

		upper and lower are member names used to access parts of a ClockValue.
*/
#if TimingMethod == TM_UT

	typedef AbsoluteTime ClockValue;
	#define ClockMax	{ UINT32_MAX, UINT32_MAX }
	#define	upper		hi
	#define	lower		lo

#elif TimingMethod == TM_TOD

	typedef struct timeval ClockValue;
	#define ClockMax	{ LONG_MAX, INT_MAX };
		// (Relies on knowledge of timeval definition, unfortunately.)
	#define upper		tv_sec
	#define	lower		tv_usec

#elif TimingMethod == TM_MACH

	typedef uint64_t ClockValue;
	#define ClockMax	UINT64_MAX

#else

	typedef struct { uint32_t upper, lower; } ClockValue;
	#define ClockMax	{ UINT32_MAX, UINT32_MAX }

#endif	// TimingMethod.


// Return the current value of the clock.
static inline ClockValue ReadClock(void)
{
	#if TimingMethod == TM_UT

		// Use Carbon's UpTime.
		return UpTime();

	#elif TimingMethod == TM_TSC

		// Read two values simultaneously from time stamp counter.
		volatile ClockValue result;
		__asm__ volatile("rdtsc" : "=d" (result.upper), "=a" (result.lower));
		return result;

	#elif TimingMethod == TM_TOD

		// Use Open Unix's gettimeofday.
		ClockValue result;
		gettimeofday(&result, NULL);
		return result;

	#elif TimingMethod == TM_MACH

		return mach_absolute_time();

	#else	// TimingMethod.

		/*	In these cases, we must read the time as two values from some
			source, but we cannot read them simultaneously, so the low value
			might roll over while we are reading them.  We will read the upper
			value twice and choose a correct one afterward -- see below.
		*/
		ClockValue result;
		uint32_t upper0, upper1;

		/*	Get values from the upper and lower count or time-base registers.

			The lower register might overflow into the upper register while we
			are doing this, so we will read the upper register twice.  We will
			assume the overflow can occur only once during these three
			instructions, so one of the two values of the upper register is
			correct.  If the lower value is small, it will not overflow for a
			while, so the upper value we read immediately after it must be
			correct.  Conversely, if the lower value is large, it must not have
			overflowed recently, so the upper value we read immediately before
			it must be correct.
		*/
		#if TimingMethod == TM_PMC

			// Read values from performance monitor counters.
			__asm__ volatile("\
					mfspr	%[upper0], 772	\n\
					mfspr	%[lower] , 771	\n\
					mfspr	%[upper1], 772	"
				:	[lower]  "=r" (result.lower),	// result.lower is output.
					[upper0] "=r" (upper0),			// upper0 is output.
					[upper1] "=r" (upper1)			// upper1 is output.
			);

		#elif TimingMethod == TM_TB

			// Read values from time-base registers.
			__asm__ volatile("\
					mfspr	%[upper0], 269	\n\
					mfspr	%[lower] , 268	\n\
					mfspr	%[upper1], 269	"
				:	[lower]  "=r" (result.lower),	// result.lower is output.
					[upper0] "=r" (upper0),			// upper0 is output.
					[upper1] "=r" (upper1)			// upper1 is output.
			);

		#else	// TimingMethod.

			#error "Code is not defined for selected timing method."

		#endif	// TimingMethod.

		/*	Choose which upper value to use.  We could do this with a
			conditional expression:

				result.upper = result.lower < 2147483648u
					? upper1
					: upper0;

			However, the execution time might change depending on whether the
			branch were correctly predicted or not.  Instead, we will use a
			calculation with no branches.
		*/

		// Use a signed shift to copy lower's high bit to all 32 bits.
		uint32_t mask = (int32_t) result.lower >> 31;

		result.upper = upper1 ^ ((upper0 ^ upper1) & mask);
			/*	If mask is all zeroes, the above statement reduces:

					result.upper = upper1 ^ ((upper0 ^ upper1) & mask);
					result.upper = upper1 ^ ((upper0 ^ upper1) & 0);
					result.upper = upper1 ^ (0);
					result.upper = upper1;

				If mask is all ones, the above statement reduces:

					result.upper = upper1 ^ ((upper0 ^ upper1) & mask);
					result.upper = upper1 ^ ((upper0 ^ upper1) & ~0);
					result.upper = upper1 ^ (upper0 ^ upper1);
					result.upper = upper0;
			*/

		return result;

	#endif	// TimingMethod.
}


/* Subtract two clock values, t1 and t0, and return t1-t0.

   Since some ClockValue implementations are unsigned, t1 should be not less
   than t0.
*/
static ClockValue SubtractClock(const ClockValue t1, const ClockValue t0)
{
#if TimingMethod == TM_MACH

	return t1 - t0;

#else // TimingMethod

	ClockValue result;
   
	result.upper = t1.upper - t0.upper;
	result.lower = t1.lower - t0.lower;

	// If necessary, "borrow" from upper word.
	if (t1.lower < t0.lower)
		--result.upper;

	return result;

#endif // TimingMethod
}


/*	Compare two clock values.  Return true iff t0 < t1.

	This should only be used to compare relative times, such as durations of
	code execution, which are known to be small relative to the number of bits
	in a ClockValue.  If absolute times are compared, there is no guarantee
	about the value of the upper word.  It might be very high at one time and
	overflow to a very small value at a later time.
*/
static bool ClockLessThan(const ClockValue t0, const ClockValue t1)
{
#if TimingMethod == TM_MACH

	return t0 < t1;

#else // TimingMethod

	if (t0.upper == t1.upper)
		return t0.lower < t1.lower;
	else
		return t0.upper < t1.upper;

#endif // TimingMethod
}


/*	Convert clock value to double, without changing units.  That is, both the
	input and the return value are the same number of clock ticks.
*/
static double ClockToDouble(const ClockValue t)
{
#if TimingMethod == TM_TOD
	return t.upper * 1e6 + t.lower;
#elif TimingMethod == TM_MACH
	return t;
#else
	return t.upper * 4294967296. + t.lower;
#endif
}


#include <math.h>
#include <sys/sysctl.h>	// Declare things needed for sysctlbyname.
#include <mach/mach.h>	// Define constants for CPU types.


#if IntelProcessor	// Conditionalize on architecture.


	// Describe the layout of the flags register (EFLAGS or RFLAGS).
	typedef struct
	{
		unsigned int
			CF		:  1,
					:  1,
			PF		:  1,
					:  1,
			AF		:  1,
					:  1,
			ZF		:  1,
			SF		:  1,
			TF		:  1,
			IF		:  1,
			DF		:  1,
			OF		:  1,
			IOPL	:  2,
			NT		:  1,
					:  1,
			RF		:  1,
			VM		:  1,
			AC		:  1,
			VIF		:  1,
			VIP		:  1,
			ID		:  1,
					: 10;
		#if defined __x86_64__
			unsigned int dummy : 32;
		#endif
	} Flags;


	// Define structure to hold register contents.
	typedef struct 
	{
		uint32_t eax, ebx, ecx, edx;
	} Registers;


	// Get the EFLAGS register.
	static Flags GetFlags(void)
	{
		Flags flags;
		__asm__ volatile("pushf; pop %[flags]" : [flags] "=rm" (flags));
		return flags;
	}


	// Set the EFLAGS register.
	static void SetFlags(Flags flags)
	{
		__asm__ volatile("push %[flags]; popf" : : [flags] "rmi" (flags));

	}


	// Return true if the cpuid instruction is available.
	static bool cpuidIsSupported(void)
	{
		// The cpuid instruction is supported iff we can change the ID flag.

		// Record and copy the old flags.
		Flags old = GetFlags();
		Flags new = old;

		// Change the ID flag.
		new.ID ^= 1;
		SetFlags(new);

		// Get the altered flags and restore the old flags.
		Flags newer = GetFlags();
		SetFlags(old);

		// Test whether the flag changed.
		return new.ID == newer.ID;
	}


	/*	Execute cpuid instruction with Ieax input and return results in
		structure.
	*/
	static Registers cpuid(uint32_t Ieax)
	{
		Registers result;
		__asm__ volatile(
				#if defined __i386__
					"\
					push	%%eax				\n\
					push	%%ebx				\n\
					push	%%ecx				\n\
					push	%%edx				\n\
					mov		%[Ieax], %%eax		\n\
					cpuid						\n\
					mov		%%eax, %[eax]		\n\
					mov		%%ebx, %[ebx]		\n\
					mov		%%ecx, %[ecx]		\n\
					mov		%%edx, %[edx]		\n\
					pop		%%edx				\n\
					pop		%%ecx				\n\
					pop		%%ebx				\n\
					pop		%%eax"
				#else	// defined __i386__
					"\
					push	%%rax				\n\
					push	%%rbx				\n\
					push	%%rcx				\n\
					push	%%rdx				\n\
					mov		%[Ieax], %%eax		\n\
					cpuid						\n\
					mov		%%eax, %[eax]		\n\
					mov		%%ebx, %[ebx]		\n\
					mov		%%ecx, %[ecx]		\n\
					mov		%%edx, %[edx]		\n\
					pop		%%rdx				\n\
					pop		%%rcx				\n\
					pop		%%rbx				\n\
					pop		%%rax"
				#endif	// defined __i386__
			:
				[eax] "=m" (result.eax),
				[ebx] "=m" (result.ebx),
				[ecx] "=m" (result.ecx),
				[edx] "=m" (result.edx)
			:
				[Ieax] "m" (Ieax)
			);
		return result;
	}


	/*	Compose the family and model identification from cpuid information,
		returning them in the supplied objects.  Return 0 for success and
		non-zero for failure.
	*/
	static int GetCPUFamilyAndModel(unsigned int *family,
		unsigned int *model)
	{
		// Fail if cpuid instruction is not supported.
		if (!cpuidIsSupported())
			return 1;

		// Get maximum input to cpuid instruction.
		Registers registers = cpuid(0);
		uint32_t maximum = registers.eax;

		// Fail if input 1 to cpuid instruction is not supported.
		if (maximum < 1)
			return 1;

		// Get results containing family and model information.
		registers = cpuid(1);

		// Declare type to access family and model information.
		union
		{
			uint32_t u;
			struct
			{
				unsigned int SteppingID		: 4;
				unsigned int Model			: 4;
				unsigned int Family			: 4;
				unsigned int Type			: 2;
				unsigned int filler14		: 2;
				unsigned int ExtendedModel	: 4;
				unsigned int ExtendedFamily	: 8;
				unsigned int filler28		: 4;
			} s;
		} Version = { registers.eax };

		// Compose the family ID from the Family and ExtendedFamily fields.
		*family = Version.s.Family;
		if (Version.s.Family == 0x15)
			*family += Version.s.ExtendedFamily;

		// Compose the model ID from the Model and ExtendedModel fields.
		*model = Version.s.Model;
		if (Version.s.Family == 0x6 || Version.s.Family == 0xf)
			*model += (unsigned int) Version.s.ExtendedModel << 4;

		// Return success.
		return 0;
	}


#endif	// IntelProcessor	// Conditionalize on architecture.


/*	Return the number of CPU cycles per iteration that the following
	routine, ConsumeCPUCycles, consumes.  Zero is returned if the
	information is not known.
*/
static unsigned int GetCPUCyclesCycles(void)
{
	#if IntelProcessor	// Conditionalize on architecture.
		unsigned int family, model;
		if (GetCPUFamilyAndModel(&family, &model))
		{
			fprintf(stderr,
"Warning, unknown CPU, does not support cpuid instruction.\n");
			return 0;
		}

		switch (family)
		{
			case 6: switch (model)
				{
					case 14:	return 33;	// Core.
					case 15:	return 22;	// Core 2.
					case 23:	return 22;	// Penryn.
					case 26:	return 20;	// Nehalem.
					case 28:	return 33;	// Atom.
				}
				break;
			case 15: switch (model)
				{
					case 4:		return 24;	// Celeron.
				}
				break;
		}
		fprintf(stderr, "Warning, unknown CPU, family %u, model %u.\n",
			family, model);
		return 0;
	#else
		// Ask system for CPU identification.
		unsigned int CPUSubtype;
		size_t SizeOfCPUSubtype = sizeof CPUSubtype;
		if (0 == sysctlbyname("hw.cpusubtype",
				&CPUSubtype, &SizeOfCPUSubtype, NULL, 0))
			// Set number of cycles per iteration according to CPU model.
			switch (CPUSubtype)
			{
				#if defined __ppc__ || defined __ppc64__
											// Conditionalize on architecture.
					case CPU_SUBTYPE_POWERPC_750:
						return 32;
					case CPU_SUBTYPE_POWERPC_7450:
						return 65;
					case CPU_SUBTYPE_POWERPC_970:
					case 10:
						return 16;
				#elif defined __x86_64__	// Conditionalize on architecture.
					case 4:
						return 22;		// Core 2 in 64-bit mode.
					/*	If hw.cpusubtype does not provide sufficient
						distinction, use hw.cpufamily.
					*/
				#elif defined __arm__		// Conditionalize on architecture.
					#if defined __thumb__
						case 6:
							return 17;		// 1176.
						case 9:
							return 16;		// Cortex-A8.
						case 10:
							return 16;		// Cortex-A9.
					#else // defined __thumb__
						case 6:
							return 17;		// 1176.
						case 9:
							return 18;		// Cortex-A8.
						case 10:
							return 17;		// Cortex-A9.
					#endif // defined __thumb__
				#endif						// Conditionalize on architecture.
					default:
						fprintf(stderr,
							"Warning, unknown CPU, subtype %d.\n",
							CPUSubtype);
						return 0;
			}
		else
		{
			fprintf(stderr,
"Warning, sysctlbyname(\"hw.cpusubtype\") failed with errno = %d.\n",
				errno);
			return 0;
		}
	#endif	// IntelProcessor	// Conditionalize on architecture.
	return 0;
}


/*	ConsumeCPUCycles keeps the CPU busy for a while.  The intent is to
	execute a loop that consumes a known number of CPU cycles, so the
	time taken can be compared to time reported by other time-measuring
	facilities, such as the time-base registers.

	For example, something like the following might happen if the time-base
	registers are being used:

		Call this routine with 1 for iterations so that all instructions
		and data are loaded into cache.
		
		Call this routine with 2 for iterations and record the time-base
		registers before and after that call.  Let m0 be the difference.

		Call this routine with 1002 for iterations and record the time-base
		registers before and after that call.  Let m1 be the difference.

		m1-m0 is the number of ticks of the time base registers that 1000
		iterations takes.  Suppose, for example only, this is 296.

		This routine returns a value in *CPUCyclesPerIteration.  1000 times
		that value is the number of CPU cycles that 1000 iterations takes.
		Suppose this is 16,000.

		In this example, the measurements would show there are about 54
		(16,000 / 296) CPU cycles per tick of the time-base registers.

	Input:

		unsigned int iterations
			The number of iterations to execute.

		unsigned int *CPUCyclesPerIteration
			Address of one unsigned int to be used for output.

	Output:

		None.

	When measuring, this routine should be called three times:

		Once with a small number of iterations to load all instructions
		and data into cache.  (One iteration might be sufficient.)

		Once with a small number of iterations to measure overhead.  (Two
		iterations is might be a good choice to allow for the first time
		the loop branches back being different from subsequent times.)

		Once with a large number of iterations to measure the loop.

	The difference between execution times of the last two calls should be
	used.

	Interrupts should be disabled while the three calls are performed.  If
	that is not possible, the sequence of three calls should be restarted
	if an interrupt occurs.

	This routine is declared noinline to work around a compiler shortcoming;
	the compiler is unable to determine how many bytes inline assembly uses,
	so it does not know when offsets to labels it is using exceed the limits
	of what can be encoded in instructions.
*/
static void ConsumeCPUCycles(unsigned int iterations, void *UnusedPointer)
	__attribute__((noinline));
static void ConsumeCPUCycles(unsigned int iterations, void *UnusedPointer)
{
	#if defined __ppc__ || defined __ppc64__	// Architecture.
		/*	Instructions are executed in a loop.  These instructions are
			expected to take a known amount of CPU cycles on specific
			processors, but this should be checked with some other source,
			such as the operating system's report of clock frequency.
		*/
		__asm__ volatile(
			"	mtctr	%[iterations]	\n\
				.align	5				\n\
				nop						\n\
			0:							\n\
				cror	 0,  0,  0		\n\
				cror	 1,  1,  1		\n\
				cror	 2,  2,  2		\n\
				cror	 3,  3,  3		\n\
				cror	 4,  4,  4		\n\
				cror	 5,  5,  5		\n\
				cror	 6,  6,  6		\n\
				cror	 7,  7,  7		\n\
				cror	 8,  8,  8		\n\
				cror	 9,  9,  9		\n\
				cror	10, 10, 10		\n\
				cror	11, 11, 11		\n\
				cror	12, 12, 12		\n\
				cror	13, 13, 13		\n\
				cror	14, 14, 14		\n\
				cror	15, 15, 15		\n\
				bdnz	0b				"
			: 									// No outputs.
			:	[iterations] "r" (iterations)	// iterations is input.
			:	"ctr"							// Counter is modified.
		);
	#elif IntelProcessor						// Architecture.
		__asm__ volatile("					\n\
				.align	5					\n\
			0:								\n\
				decl	%[iterations]		\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				nop							\n\
				jne		0b					"
			:	[iterations] "+r" (iterations)
					// iterations is altered input.
		);
	#else if defined __arm__					// Architecture.
		#if defined __thumb__
			register unsigned int r0 __asm__("r0") = iterations;
			/*	Use machine language because we do not have control over which
				assembly language variant is used.  GCC is sometimes putting in
				".syntax unified" (as when target armv7) and sometimes not (as
				when target armv6).  So we use hard-coded instructions to
				ensure we get the same instructions every time.
			*/
			__asm__ volatile(
				"	.align	5									\n"
				"0:												\n"
				"	.short	0x3801	\n"		// subs r0, #1.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0x46c0	\n"		// nop.
				"	.short	0xd1ed	\n"		// bne.n 0b.
				:	[r0] "+r" (r0)
						// r0 is altered input.
			);
		#else // defined __thumb__
			__asm__ volatile(
				"	.align	5									\n"
				"0:												\n"
				"	adds	%[iterations], %[iterations], #-1	\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	nop											\n"
				"	bne		0b									\n"
				:	[iterations] "+r" (iterations)
						// iterations is altered input.
			);
		#endif // defined __thumb__
	#endif										// Architecture.

	return;
}


#if defined NeedCHUD

	#include <CHUD/CHUD.h>


	/*	CheckCHUDStatus.

		If CHUD status is not success, print an error.
	*/
	static void CheckCHUDStatus(int status, const char *routine)
	{
		if (status != chudSuccess)
			fprintf(stderr,
				"Error, %s returned %d.\nCHUD status string is \"%s\".\n",
				routine, status, chudGetStatusStr());
	}


	/*	RequireCHUDStatus.

		If CHUD status is not success, print an error and exit.
	*/
	static void RequireCHUDStatus(int status, const char *routine)
	{
		CheckCHUDStatus(status, routine);
		if (status != chudSuccess)
			exit(EXIT_FAILURE);
	}


	/*	StopClockServices.

		Release any facilities acquired for clock services.
	*/
	static void StopClockServices(void)
	{
		int result = chudStopPMCs();
		CheckCHUDStatus(result, "chudStopPMCs");

		result = chudReleaseSamplingFacility(0);
		CheckCHUDStatus(result, "chudReleaseSamplingFacility");

		// Make all processors available again.
		result = chudSetNumPhysicalProcessors(chudPhysicalProcessorCount());
		CheckCHUDStatus(result, "chudSetNumPhysicalProcessors");
		result = chudSetNumLogicalProcessors(chudLogicalProcessorCount());
		CheckCHUDStatus(result, "chudSetNumLogicalProcessors");

		chudCleanup();
	}


#endif	// defined NeedCHUD


// Include things needed by set_time_constraint_policy routine below.
#include <mach/mach.h>
#include <mach/host_info.h>
#include <mach/mach_error.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/mach_syscalls.h>


/*	Set this thread to "time constraint" policy, that is, real-time priority.
	The hope is this will improve timing by preventing time-sharing threads
	from interrupting.
*/
static kern_return_t set_time_constraint_policy()
{
	thread_time_constraint_policy_data_t info;
	mach_msg_type_number_t count = THREAD_TIME_CONSTRAINT_POLICY_COUNT;
	boolean_t get_default = TRUE;

	kern_return_t result = thread_policy_get(mach_thread_self(),
		THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t) &info, &count,
		&get_default);
	if (result != KERN_SUCCESS)
		return result;

	return thread_policy_set(mach_thread_self(), THREAD_TIME_CONSTRAINT_POLICY,
		(thread_policy_t) &info, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
}


/*	StartClockServices.

	Initialize the environment for timing routines.  If resources are acquired
	that should be free, an exit handler is set to release them.  (In
	particular, if we set the system to use a single CPU while timing, we want
	to reset it afterward to use all CPUs.)
*/
static void StartClockServices(void)
{
	// Execute this routine only once.
	static bool started = false;
	if (started)
		return;
	started = true;

	// Set this thread to real-time scheduling.
	if (KERN_SUCCESS != set_time_constraint_policy())
		fprintf(stderr,
			"Warning, unable to set thread to real-time scheduling.\n");

#if defined NeedCHUD

		// Initialize CHUD facilities.

		int result = chudInitialize();
		RequireCHUDStatus(result, "chudInitialize");

		// Restrict system to one processor.
		// (What we really want is to bind this process to one processor.)
		result = chudSetNumPhysicalProcessors(1);
		CheckCHUDStatus(result, "chudSetNumPhysicalProcessors");
		result = chudSetNumLogicalProcessors(1);
		CheckCHUDStatus(result, "chudSetNumLogicalProcessors");

		result = chudAcquireSamplingFacility(CHUD_NONBLOCKING);
		RequireCHUDStatus(result, "chudAcquireSamplingFacility");

		// On a 970 CPU, PMC1 event 15 is CPU cycles.
		result = chudSetPMCEvent(chudCPU1Dev, PMC_1, 15);
		RequireCHUDStatus(result, "chudSetPMCEvent");

		result = chudSetPMCMode(chudCPU1Dev, PMC_1, chudCounter);
		RequireCHUDStatus(result, "chudSetPMCMode");

		// On a 970 CPU, PMC2 event 10 is PMC1 overflow.
		result = chudSetPMCEvent(chudCPU1Dev, PMC_2, 10);
		RequireCHUDStatus(result, "chudSetPMCEvent");

		result = chudSetPMCMode(chudCPU1Dev, PMC_2, chudCounter);
		RequireCHUDStatus(result, "chudSetPMCMode");

		result = chudClearPMCs();
		RequireCHUDStatus(result, "chudClearPMCs");

		result = chudStartPMCs();
		RequireCHUDStatus(result, "chudStartPMCs");

		// Set exit handler to release resources.
		result = atexit(StopClockServices);
		if (result != 0)
		{
			fprintf(stderr, "Error, atexit returned %d.\n", result);
			StopClockServices();
			exit(EXIT_FAILURE);
		}

#endif	// defined NeedCHUD
}


/*	Define IgnoreInterrupts to ignore whether interrupts occur.  Omit it to
	test for interrupts and rerun a timing if an interrupt occurred during it.
*/
#define	IgnoreInterrupts


#if !defined IgnoreInterrupts
	/*	We are using lwarx and stwcx. instructions below, and they maintain a
		reservation address with a resolution of 128-byte blocks of memory on a
		PowerPC 970, and no larger on other processors we are currently
		interested in.  It is recommended that block be used only for locks and
		protected data.  We might not need to do that for our purposes
		(detecting context switches), but it is safest to keep the block from
		being used for anything else.

		An easy way to get a word in its own block of 128 bytes is to allocate
		almost twice as much and use the word in the middle.  Non-standard
		compiler features might do this with less wasted space, but this is
		just a timing program, so space is not critical.
	*/
	static unsigned int ReserveBlock[2*128 / sizeof(unsigned int) - 1];
	static unsigned int * const ReservedWord =
		&ReserveBlock[128 / sizeof *ReserveBlock - 1];
#endif


// Execute a lwarx instruction on ReservedWord.
static inline void LoadAndReserve(void)
{
	#if !defined IgnoreInterrupts
		__asm__ volatile(
			"	lwarx	r0, 0, %[ReservedWord]		"
			:										// No output.
			:	[ReservedWord] "r" (ReservedWord)	// ReservedWord is input.
			:	"r0"								// r0 is modified.
		);
	#endif
}


/*	Execute a stwcx. instruction on ReservedWord and return the bit it sets in
	the condition register.
*/
static inline unsigned int StoreConditional(void)
{
	#if !defined IgnoreInterrupts
		uint32_t result;

		__asm__ volatile(
			"	stwcx.	r0, 0, %[ReservedWord]		\n\
				mfcr	%[result]					"
			: 	[result] "=r" (result)				// result is output.
			:	[ReservedWord] "r" (ReservedWord)	// ReservedWord is input.
		);

		return result & 0x20000000;
	#else	// !defined IgnoreInterrupts
		return 1;
	#endif	// !defined IgnoreInterrupts
}


/*	This routine measures the gross amount of time it takes to execute an
	arbitrary routine, including whatever overhead there is, such as reading
	the clock value and loading the routine into cache.  The number of clock
	ticks execution takes is returned.

	Input:

		RoutineToBeTimedType routine.
			Address of a routine to measure.  (See typedef of this type.)

		unsigned int iterations.
			Number of iterations to tell the routine to perform.

		void *data.
			Pointer to be passed to the routine.

	Output:

		Return value.
			The number of clock ticks execution takes.
*/
static ClockValue MeasureGrossTime(
		RoutineToBeTimedType routine,
		unsigned int iterations,
		void *data
	)
{
	ClockValue t0, t1;

	// Get time before executing routine.
	t0 = ReadClock();

	// Execute routine.
	routine(iterations, data);

	// Get time after executing routine.
	t1 = ReadClock();

	// Return difference in times.
	return SubtractClock(t1, t0);
}


/*	MeasureNetTime.
	
	This routine measures the net amount of time it takes to execute multiple
	iterations in an arbitrary routine, excluding overhead such as reading the
	clock value and loading the routine into cache.  The number of clock ticks
	taken is returned.

	Input:

		RoutineToBeTimedType routine.
			Address of a routine to measure.  (See typedef of this type.)

		unsigned int iterations.
			Number of iterations to tell the routine to perform.

		void *data.
			Pointer to be passed to the routine.

		samples.
			Number of samples to take.  Some statistical analysis might be
			performed to try to divine the true execution time, filtering out
			noise from interrupts and other sources.  For now, we'll just take
			the minimum of all the samples.

	Output:

		Return value.
			Number of clock ticks to execute the specified number of
			iterations.
*/
static ClockValue MeasureNetTime(
		RoutineToBeTimedType routine,
		unsigned int iterations,
		void *data,
		unsigned int samples
	)
{
	StartClockServices();

	/*	Step 0:  Load cache with a single iteration of the routine.
	*/
	MeasureGrossTime(routine, 1, data);


	/*	Step 1:  Record the minimum time in <samples> samples to execute two
		iterations.
	*/

	// Initialize the minimum execution time seen so far.
	ClockValue m0 = ClockMax;

	// Collect samples.
	for (unsigned int i = 0; i < samples; ++i)
	{
		ClockValue d0;

		/*	Repeat measurement until it is completed without an intervening
			process context switch.
		*/
		do
		{
			/*	Set a reservation, which the operating system will clear if it
				switches processes.
			*/
			LoadAndReserve();

			// Measure time for routine overhead.
			d0 = MeasureGrossTime(routine, 2, data);

		// Repeat the loop if the reservation was lost.
		} while (0 == StoreConditional());

		// Keep track of the minimum time seen so far.
		if (ClockLessThan(d0, m0))
			 m0 = d0;
	}


	/*	Step 2:  Record the minimum time in <samples> samples to execute
		2+<iterations> iterations.
	*/

	// Initialize the minimum execution time seen so far.
	ClockValue m1 = ClockMax;

	for (unsigned int i = 0; i < samples; ++i)
	{
		ClockValue d1;

		/*	Repeat measurement until it is completed without an intervening
			process context switch.
		*/
		do
		{
			/*	Set a reservation, which the operating system will clear if it
				switches processes.
			*/
			LoadAndReserve();

			// Measure time for routine overhead and loop execution.
			d1 = MeasureGrossTime(routine, 2+iterations, data);

		// Repeat the loop if the reservation was lost.
		} while (0 == StoreConditional());

		// Keep track of the minimum time seen so far.
		if (ClockLessThan(d1, m1))
			 m1 = d1;
	}


	/*	Step 3:  Calculate the time to execute <iterations> iterations as the
		difference between the time to execute 2 iterations and the time to
		execute 2+<iterations> iterations.
	*/
	return SubtractClock(m1, m0);
}


/*	Return the number of CPU cycles in one clock tick determined by hardware
	means, such as executing code with a known execution time.
*/
static double CPUCyclesPerTickViaHardware(void)
{
	/*	Get the number of CPU cycles one iteration of the loop in
		ConsumeCPUCycles takes.
	*/
	unsigned int Cycles = GetCPUCyclesCycles();

	// Give up if we do not know how long the loop takes on this CPU.
	if (Cycles == 0)
		return 0;

	static const int Iterations = 5400;

	// Get the number of clock ticks Iterations iterations of the loop take.
	ClockValue Ticks = MeasureNetTime(ConsumeCPUCycles, Iterations, NULL, 1000);

	// Return cycles per tick.
	return Cycles * Iterations / ClockToDouble(Ticks);
}


/*	Return the number of CPU cycles in one clock tick determined by asking
	the operating system.
*/
static double CPUCyclesPerTickViaSystem(void)
{
	#if TimingMethod == TM_TB || TimingMethod == TM_UT

		/*	The time-base registers (which UpTime uses) should tick at
			hw.tbfrequency, and the CPU should tick at hw.cpufrequency, so the
			CPU cycles per time-base tick should be hw.cpufrequency /
			hw.tbfrequency.
		*/
		uint64_t CPUFrequency, TBFrequency;
		size_t SizeOf = sizeof CPUFrequency;
		if (0 != sysctlbyname("hw.cpufrequency",
				&CPUFrequency, &SizeOf, NULL, 0))
		{
			fprintf(stderr,
"Warning, unable to get hw.cpufrequency from sysctlbyname.\n");
			return 0;
		}
		if (0 != sysctlbyname("hw.tbfrequency",
				&TBFrequency, &SizeOf, NULL, 0))
		{
			fprintf(stderr,
"Warning, unable to get hw.tbfrequency from sysctlbyname.\n");
			return 0;
		}
		return (double) CPUFrequency / TBFrequency;

	#elif TimingMethod == TM_PMC

		/*	The PowerPC performance monitor counter event we use should be one
			CPU cycle per tick.
		*/
		return 1;

	#elif TimingMethod == TM_TOD

		/*	The CPU should tick at hw.cpufrequency, so the CPU cycles per
			microsecond tick should be hw.cpufrequency / 1e6.
		*/
		uint64_t CPUFrequency;
		size_t SizeOf = sizeof CPUFrequency;
		if (0 != sysctlbyname("hw.cpufrequency",
				&CPUFrequency, &SizeOf, NULL, 0))
		{
			fprintf(stderr,
"Warning, unable to get hw.cpufrequency from sysctlbyname.\n");
			return 0;
		}

		return CPUFrequency / 1e6;

	#elif TimingMethod == TM_MACH

		/*	Get ratio of mach_absolute_time ticks to nanoseconds.
			(One nanosecond is numer/denom times one tick.)
		*/
		mach_timebase_info_data_t data;
		mach_timebase_info(&data);

		// Get what system thinks CPU frequency is.
		uint64_t CPUFrequency;
		size_t SizeOf = sizeof CPUFrequency;
		if (0 != sysctlbyname("hw.cpufrequency",
				&CPUFrequency, &SizeOf, NULL, 0))
		{
			fprintf(stderr,
"Warning, unable to get hw.cpufrequency from sysctlbyname.\n");
			return 0;
		}
		
		/*	Calculate expected value as number of CPU cycles in a second
			(CPUFrequency) divided by number of mach_absolute_time ticks
			in a billion nanoseconds.
		*/
		return CPUFrequency / (1e9 * data.denom / data.numer);

	#else	// TimingMethod.

		#error "Code is not defined for selected timing method."

		/*	TM_TSC is not supported.  If we want to check the processor
			model, we could return 1 for:

				family 6, models  9 and 13;
				family 15, models 0, 1, and 2;
				P6 family processors.

			For other models, we have no way to convert the time stamp counter
			to cycles, because, according to Intel's specification, the
			increment rate "may be set" by the maximum core-clock to bus-clock
			ratio of the processor or by the frequency at which the processor
			is booted.  Additional examination of the documentation for each
			processor model would be needed, and it would be impossible to
			support unexamined processors.

			We could return 0 and let the hardware CPUCyclesPerTickViaHardware
			routine control the value used, but it also cannot support
			unexamined processors.
		*/

	#endif	// TimingMethod.
}


// Return the number of CPU cycles in one clock tick.
static double CPUCyclesPerTick(void)
{
	/*	Cache the number in a static object and return it if we have
		computed it previously.
	*/
	static double CachedCPUCyclesPerTick = 0;

	if (CachedCPUCyclesPerTick != 0)
		return CachedCPUCyclesPerTick;

	double CyclesPerTickViaHardware = CPUCyclesPerTickViaHardware();
	double CyclesPerTickViaSystem   = CPUCyclesPerTickViaSystem  ();

	if (CyclesPerTickViaHardware == 0)
	{
		// This must be a CPU model we are unfamiliar with.

		if (CyclesPerTickViaSystem == 0)
		{
			fprintf(stderr, "Error, no value is available for the number of CPU cycles per clock tick.\n");
			exit(EXIT_FAILURE);
		}
		else
			CachedCPUCyclesPerTick = CyclesPerTickViaSystem;
	}
	else
		if (CyclesPerTickViaSystem == 0)
			CachedCPUCyclesPerTick = CyclesPerTickViaHardware;
		else
		{
			const double Tolerance = .004;	// Maximum relative error allowed.

			// Compare observed and expected ratios.
			if (Tolerance <
					fabs(CyclesPerTickViaSystem / CyclesPerTickViaHardware - 1))
				fprintf(stderr,
"Warning, hardware and operating system disagree about the number of CPU\n"
"cycles per clock tick.\n"
"\tHardware indicates %g cycles per tick.\n"
"\tSystem indicates %g cycles per tick.\n"
"\tUsing system value.\n",
				CyclesPerTickViaHardware,
				CyclesPerTickViaSystem);

			CachedCPUCyclesPerTick = CyclesPerTickViaSystem;
		}

	return CachedCPUCyclesPerTick;
}


/*	ClockToCPUCycles.

	Convert time (in a ClockValue object) to number of CPU cycles in that time.
*/
double ClockToCPUCycles(ClockValue t)
{
	return ClockToDouble(t) * CPUCyclesPerTick();
}


#include <time.h>


// See header file for description of this routine.
double MeasureNetTimeInCPUCycles(
		RoutineToBeTimedType routine,
		unsigned int iterations,
		void *data,
		unsigned int samples
	)
{
	return ClockToCPUCycles(MeasureNetTime(routine, iterations, data, samples))
		/ iterations;
}
