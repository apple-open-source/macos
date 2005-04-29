/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

/* File - TrieFA.h
 *
 *    The data types for the generated trie-based finite automata.
 */

struct TrieState {				/* An entry in the FA state table			*/
	short			def;		/* 	If this state is an accepting state then*/
								/*	this is the definition, otherwise -1.	*/
	short			trans_base;	/* The base index into the transition table.*/
	long			mask;		/* The transition mask. 					*/
};

struct TrieTrans {				/* An entry in the FA transition table.		*/
	short			c;				/* The transition character (lowercase).*/
	short			next_state;		/* The next state.						*/
};

typedef struct TrieState TrieState;
typedef struct TrieTrans TrieTrans;

extern TrieState	TrieStateTbl[];
extern TrieTrans	TrieTransTbl[];
