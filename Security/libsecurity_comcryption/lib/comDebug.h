/* Copyright (c) 1997 Apple Computer, Inc.
 *
 * comDebug.h
 */

#ifndef	_COM_DEBUG_H_
#define _COM_DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * enable general debugging printfs and error checking.
 */
#define COM_DEBUG				0
#if		COM_DEBUG
#include <stdio.h>

#define	ddprintf(x)				printf x
#else
#define	ddprintf(x)
#endif

/*
 * block parsing debug
 */
#define COM_SCAN_DEBUG			0
#if		COM_SCAN_DEBUG
#define	scprintf(x)				printf x
#else
#define	scprintf(x)
#endif

/*
 * 2nd-level comcrypt debug
 */
#define LEVEL2_DEBUG			0
#if		LEVEL2_DEBUG
#include <stdio.h>

#define	l2printf(x)				printf x
#else
#define	l2printf(x)
#endif

/*
 * lookahead queue debug
 */
#define COM_LA_DEBUG				0
#define COM_LA_PRINTF				0
#if		COM_LA_PRINTF
#define	laprintf(x)					printf x
#else
#define	laprintf(x)
#endif

/*
 * Statistics measurements. This is a private API.
 */
#if		COM_DEBUG
#define	COM_STATS		0
#else
#define	COM_STATS		0
#endif

#if		COM_STATS

/*
 * Info obtained via a call to getComStats()
 */
typedef struct {
	unsigned	level1blocks;
	unsigned	plaintextBytes;
	unsigned	ciphertextBytes;
	unsigned	oneByteFrags;			// 1st level only
	unsigned	twoByteFrags;			// ditto
	unsigned	level2oneByteFrags;		// second level only
	unsigned	level2twoByteFrags;		// ditto
	unsigned	level2byteCode;			// bytes, pre-encrypted
	unsigned	level2cipherText;		// bytes, post-encrypt
	unsigned	level2blocks;			// 2nd-level blocks
	unsigned 	level2jmatch;			// total jmatch (at first level) of
										//     2nd level blocks
} comStats;

extern comStats _comStats;
#define	incrComStat(stat, num)	_comStats.stat += num;

#define incr1byteFrags(recursLevel)		{		\
	if(recursLevel == 1) {						\
		incrComStat(level2oneByteFrags, 1);		\
	}											\
	else {										\
		incrComStat(oneByteFrags, 1);			\
	}											\
}
#define incr2byteFrags(recursLevel)		{		\
	if(recursLevel == 1) {						\
		incrComStat(level2twoByteFrags, 1);		\
	}											\
	else {										\
		incrComStat(twoByteFrags, 1);			\
	}											\
}

extern void resetComStats();
extern void getComStats(comStats *stats);

#else
#define	incrComStat(stat, num)
#define incr1byteFrags(recursLevel)
#define incr2byteFrags(recursLevel)
#endif

/*
 * Profiling measurement. A private API when enabled.
 */
#if		COM_DEBUG
#define COM_PROFILE			0
#else
#define	COM_PROFILE			0
#endif

#if		COM_PROFILE

#include <kern/time_stamp.h>

/*
 * Global profiling enable. It turns out the the cost of doing the
 * kern_timestamp() call twice per codeword is way more expensive
 * than the actual comcryption code. Setting this variable to zero
 * avoids the cost of all the timestamps for reference without
 * rebuilding. Also, the cmcPerWordOhead calibrates the actual
 * cost of the two kern_timestamp() calls per word.
 */
extern unsigned comProfEnable;

/*
 * Profiling accumulators.
 */
typedef	unsigned comprof_t;

extern comprof_t cmcTotal;
extern comprof_t cmcQueSearch;
extern comprof_t cmcQueMatchMove;
extern comprof_t cmcQueMissMove;
extern comprof_t cmcPerWordOhead;
extern comprof_t cmcLevel2;


/*
 * Place one of these in the local variable declaration list of each routine
 * which will do profiling.
 */
#define COMPROF_LOCALS					\
	struct tsval _profStartTime;		\
	struct tsval _profEndTime;

/*
 * Start the clock.
 */
#define COMPROF_START 						\
	if(comProfEnable) {						\
		kern_timestamp(&_profStartTime);	\
	}

/*
 * Stop the clock and gather elapsed time to specified accumulator.
 */
#define COMPROF_END(accum)											\
	if(comProfEnable) {												\
		kern_timestamp(&_profEndTime);								\
		accum += (_profEndTime.low_val - _profStartTime.low_val);	\
	}


#else

#define	COMPROF_LOCALS
#define COMPROF_START
#define COMPROF_END(accum)

#endif	/* COM_PROFILE */

/*
 * Get/set parameter API, private, for debug only.
 */
#if		COM_DEBUG
#define	COM_PARAM_ENABLE	1
#else
#define	COM_PARAM_ENABLE	0
#endif	/*COM_DEBUG*/

#if		COM_PARAM_ENABLE

extern unsigned	getF1(comcryptObj cobj);
extern void	setF1(comcryptObj cobj, unsigned f1);
extern unsigned	getF2(comcryptObj cobj);
extern void	setF2(comcryptObj cobj, unsigned f2);
extern unsigned	getJmatchThresh(comcryptObj cobj);
extern void	setJmatchThresh(comcryptObj cobj, unsigned jmatchThresh);
extern unsigned	getMinByteCode(comcryptObj cobj);
extern void	setMinByteCode(comcryptObj cobj, unsigned minByteCode);

#endif	/*COM_PARAM_ENABLE*/

#ifdef __cplusplus
}
#endif

#endif	/*_COM_DEBUG_H_*/
