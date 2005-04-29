/* File - TrieFA.h
 *
 *    The data types for the generated trie-baseed finite automata.
 */

struct TrieState {  /* An entry in the FA state table */
    short def;         /* If this state is an accepting state then */
                       /* this is the definition, otherwise -1.    */
    short trans_base;  /* The base index into the transition table.*/
    long  mask;        /* The transition mask.                     */
};

struct TrieTrans {  /* An entry in the FA transition table. */
    short c;           /* The transition character (lowercase). */
    short next_state;  /* The next state.                       */
};

typedef struct TrieState TrieState;
typedef struct TrieTrans TrieTrans;

extern TrieState TrieStateTbl[];
extern TrieTrans TrieTransTbl[];
