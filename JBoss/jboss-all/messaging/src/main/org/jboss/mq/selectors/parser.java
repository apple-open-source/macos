//### This file created by BYACC 1.8(/Java extension  0.92)
//### Java capabilities added 7 Jan 97, Bob Jamison
//### Updated : 27 Nov 97  -- Bob Jamison, Joe Nieten
//###           01 Jan 98  -- Bob Jamison -- fixed generic semantic constructor
//###           01 Jun 99  -- Bob Jamison -- added Runnable support
//### Please send bug reports to rjamison@lincom-asg.com
//### static char yysccsid[] = "@(#)yaccpar	1.8 (Berkeley) 01/20/90";



//#line 2 "jms.y"

package org.jboss.mq.selectors;
import java.util.HashMap;
import java.util.HashSet;

import java.util.StringTokenizer;

//#line 18 "parser.java"

//#####################################################################
// class: parser
// does : encapsulates yacc() parser functionality in a Java
//        class for quick code development
//#####################################################################
/**
 * @created    August 16, 2001
 */
public class parser implements ISelectorParser
{

   boolean          yydebug;
   //do I want debug output?
   int              yynerrs;
   //number of errors so far
   int              yyerrflag;
   //was there an error?
   int              yychar;
   //maximum stack size
   int              statestk[], stateptr;

//########## SEMANTIC VALUES ##########
//public class parsersemantic is defined in parserval.java


   String           yytext;
   //user variable to return contextual strings
   parserval        yyval;
   //used to return semantic vals from action routines
   parserval        yylval;
   //the 'lval' (result) I got from yylex()
   parserval        valstk[];
   int              valptr;

//#line 74 "jms.y"

   StringTokenizer  st;
   String           nextToken = null;
   Object           selector;
   HashMap          map;
//#### end semantic value section ####
   public final static short IDENTIFIER = 257;
   public final static short STRING = 258;
   public final static short DOUBLE = 259;
   public final static short LONG = 260;
   public final static short CST = 261;
   public final static short NOT = 262;
   public final static short EQUAL = 263;
   public final static short GT = 264;
   public final static short GE = 265;
   public final static short LT = 266;
   public final static short LE = 267;
   public final static short DIFFERENT = 268;
   public final static short NEG = 269;
   public final static short BETWEEN = 270;
   public final static short AND2 = 271;
   public final static short ESCAPE = 272;
   public final static short LIKE = 273;
   public final static short NULL = 274;
   public final static short IN = 275;
   public final static short IS = 276;
   public final static short OR = 277;
   public final static short AND = 278;
   public final static short YYERRCODE = 256;

//########## STATE STACK ##########
   final static int YYSTACKSIZE = 500;
   final static short yylhs[] = {-1,
         0, 2, 2, 2, 2, 2, 2, 1, 1, 1,
         1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         1, 1, 1, 3, 3,
         };
   final static short yylen[] = {2,
         2, 1, 1, 1, 1, 1, 3, 1, 3, 3,
         3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
         3, 4, 2, 2, 2, 5, 6, 3, 4, 5,
         6, 5, 6, 1, 3,
         };
   final static short yydefred[] = {0,
         2, 3, 4, 5, 6, 0, 0, 0, 0, 0,
         0, 8, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 1, 7, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 21, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         34, 0, 22, 0, 0, 0, 0, 0, 32, 0,
         0, 0, 33, 35,
         };
   final static short yydgoto[] = {10,
         11, 12, 62,
         };
   final static short yysindex[] = {345,
         0, 0, 0, 0, 0, 345, 345, 345, 345, 0,
         160, 0, -254, -271, -271, 43, -263, 345, 345, 345,
         345, 345, 345, 345, 345, -31, -259, 345, 345, 345,
         345, 345, 345, 0, 0, 345, 345, -20, 144, 144,
         144, 144, 144, 144, 374, 129, -234, -249, 0, 431,
         448, 107, 107, -254, -254, 397, 455, -234, 345, 345,
         0, -30, 0, 345, 345, -28, 414, 414, 0, -232,
         414, 414, 0, 0,
         };
   final static short yyrindex[] = {0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, -8, -41, -24, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 84, 86,
         88, 96, 104, 121, 0, 109, 0, 0, 0, 174,
         137, 60, 76, 8, 24, 0, 126, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 32, 176, 0, 0,
         271, 276, 0, 0,
         };
   final static short yygindex[] = {0,
         497, 0, -29,
         };
   final static int YYTABLESIZE = 731;
   final static short yytable[] = {25,
         25, 25, 48, 25, 27, 25, 36, 17, 47, 37,
         69, 38, 73, 70, 49, 70, 24, 24, 24, 58,
         24, 27, 24, 61, 63, 74, 0, 0, 66, 0,
         0, 0, 23, 23, 23, 0, 23, 0, 23, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 11, 11,
         11, 0, 11, 0, 11, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 12, 12, 12, 0, 12, 0,
         12, 0, 26, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 25, 35, 32, 30, 0, 31, 0, 33,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 24,
         9, 0, 9, 0, 9, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 23, 10, 0, 10, 0,
         10, 0, 0, 0, 15, 0, 16, 0, 17, 0,
         0, 11, 0, 0, 0, 0, 18, 0, 0, 0,
         0, 0, 0, 0, 19, 0, 0, 12, 32, 28,
         0, 0, 0, 33, 0, 26, 0, 0, 0, 0,
         0, 20, 0, 0, 0, 0, 29, 0, 0, 0,
         32, 30, 0, 31, 0, 33, 0, 13, 0, 0,
         0, 0, 0, 9, 0, 32, 30, 0, 31, 0,
         33, 0, 0, 0, 0, 0, 0, 0, 0, 10,
         0, 32, 30, 0, 31, 0, 33, 15, 0, 16,
         0, 17, 0, 0, 14, 0, 30, 0, 0, 18,
         25, 25, 25, 25, 25, 25, 25, 19, 25, 25,
         25, 25, 28, 25, 0, 25, 25, 24, 24, 24,
         24, 24, 24, 24, 20, 24, 24, 24, 24, 29,
         24, 0, 24, 24, 23, 23, 23, 23, 23, 23,
         13, 23, 23, 23, 23, 0, 23, 0, 23, 23,
         11, 11, 11, 11, 11, 11, 0, 11, 11, 11,
         11, 0, 11, 34, 11, 11, 12, 12, 12, 12,
         12, 12, 0, 12, 12, 12, 12, 14, 12, 30,
         12, 12, 26, 26, 17, 18, 19, 20, 21, 22,
         23, 27, 24, 0, 0, 25, 31, 26, 27, 28,
         29, 0, 9, 9, 9, 9, 9, 9, 0, 9,
         9, 9, 9, 0, 9, 0, 9, 9, 10, 10,
         10, 10, 10, 10, 0, 10, 10, 10, 10, 0,
         10, 0, 10, 10, 15, 15, 16, 16, 17, 17,
         15, 15, 16, 16, 17, 17, 18, 18, 17, 0,
         0, 0, 18, 18, 19, 19, 0, 0, 0, 28,
         19, 19, 27, 0, 9, 28, 28, 7, 0, 8,
         17, 20, 20, 0, 27, 0, 29, 20, 20, 31,
         60, 0, 29, 29, 27, 17, 0, 13, 13, 0,
         0, 0, 0, 13, 13, 32, 30, 0, 31, 27,
         33, 17, 18, 19, 20, 21, 22, 23, 0, 24,
         0, 0, 25, 0, 26, 27, 28, 29, 32, 30,
         0, 31, 0, 33, 14, 14, 30, 30, 0, 0,
         14, 0, 0, 0, 0, 32, 30, 0, 31, 0,
         33, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 32, 30, 0, 31, 0, 33, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 32,
         30, 0, 31, 0, 33, 0, 32, 30, 0, 31,
         0, 33, 13, 14, 15, 16, 0, 0, 0, 0,
         0, 0, 0, 0, 39, 40, 41, 42, 43, 44,
         45, 46, 0, 0, 50, 51, 52, 53, 54, 55,
         0, 0, 56, 57, 0, 0, 0, 0, 0, 0,
         0, 27, 27, 0, 0, 0, 31, 31, 0, 0,
         0, 0, 0, 0, 0, 67, 68, 0, 0, 0,
         71, 72, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 1, 2, 3, 4, 5, 6, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 17, 18, 19, 20, 21,
         22, 23, 0, 24, 59, 0, 25, 0, 26, 27,
         28, 29, 0, 0, 0, 0, 0, 0, 17, 18,
         19, 20, 21, 22, 23, 0, 24, 64, 0, 25,
         0, 26, 27, 28, 29, 17, 18, 19, 20, 21,
         22, 23, 0, 24, 0, 0, 25, 0, 26, 27,
         28, 29, 17, 18, 19, 20, 21, 22, 23, 0,
         24, 0, 0, 25, 0, 26, 27, 0, 29, 17,
         18, 19, 20, 21, 22, 23, 17, 24, 0, 0,
         25, 0, 26, 27, 0, 0, 65, 0, 0, 0,
         27,
         };
   final static short yycheck[] = {41,
         42, 43, 262, 45, 276, 47, 270, 262, 40, 273,
         41, 275, 41, 44, 274, 44, 41, 42, 43, 40,
         45, 276, 47, 258, 274, 258, -1, -1, 58, -1,
         -1, -1, 41, 42, 43, -1, 45, -1, 47, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, 41, 42,
         43, -1, 45, -1, 47, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, 41, 42, 43, -1, 45, -1,
         47, -1, 41, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, 124, 41, 42, 43, -1, 45, -1, 47,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, 124,
         41, -1, 43, -1, 45, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, 124, 41, -1, 43, -1,
         45, -1, -1, -1, 41, -1, 41, -1, 41, -1,
         -1, 124, -1, -1, -1, -1, 41, -1, -1, -1,
         -1, -1, -1, -1, 41, -1, -1, 124, 42, 41,
         -1, -1, -1, 47, -1, 124, -1, -1, -1, -1,
         -1, 41, -1, -1, -1, -1, 41, -1, -1, -1,
         42, 43, -1, 45, -1, 47, -1, 41, -1, -1,
         -1, -1, -1, 124, -1, 42, 43, -1, 45, -1,
         47, -1, -1, -1, -1, -1, -1, -1, -1, 124,
         -1, 42, 43, -1, 45, -1, 47, 124, -1, 124,
         -1, 124, -1, -1, 41, -1, 41, -1, -1, 124,
         262, 263, 264, 265, 266, 267, 268, 124, 270, 271,
         272, 273, 124, 275, -1, 277, 278, 262, 263, 264,
         265, 266, 267, 268, 124, 270, 271, 272, 273, 124,
         275, -1, 277, 278, 263, 264, 265, 266, 267, 268,
         124, 270, 271, 272, 273, -1, 275, -1, 277, 278,
         263, 264, 265, 266, 267, 268, -1, 270, 271, 272,
         273, -1, 275, 124, 277, 278, 263, 264, 265, 266,
         267, 268, -1, 270, 271, 272, 273, 124, 275, 124,
         277, 278, 271, 272, 262, 263, 264, 265, 266, 267,
         268, 41, 270, -1, -1, 273, 41, 275, 276, 277,
         278, -1, 263, 264, 265, 266, 267, 268, -1, 270,
         271, 272, 273, -1, 275, -1, 277, 278, 263, 264,
         265, 266, 267, 268, -1, 270, 271, 272, 273, -1,
         275, -1, 277, 278, 271, 272, 271, 272, 271, 272,
         277, 278, 277, 278, 277, 278, 271, 272, 262, -1,
         -1, -1, 277, 278, 271, 272, -1, -1, -1, 271,
         277, 278, 276, -1, 40, 277, 278, 43, -1, 45,
         262, 271, 272, -1, 124, -1, 271, 277, 278, 124,
         272, -1, 277, 278, 276, 262, -1, 271, 272, -1,
         -1, -1, -1, 277, 278, 42, 43, -1, 45, 276,
         47, 262, 263, 264, 265, 266, 267, 268, -1, 270,
         -1, -1, 273, -1, 275, 276, 277, 278, 42, 43,
         -1, 45, -1, 47, 271, 272, 271, 272, -1, -1,
         277, -1, -1, -1, -1, 42, 43, -1, 45, -1,
         47, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, 42, 43, -1, 45, -1, 47, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, 42,
         43, -1, 45, -1, 47, -1, 42, 43, -1, 45,
         -1, 47, 6, 7, 8, 9, -1, -1, -1, -1,
         -1, -1, -1, -1, 18, 19, 20, 21, 22, 23,
         24, 25, -1, -1, 28, 29, 30, 31, 32, 33,
         -1, -1, 36, 37, -1, -1, -1, -1, -1, -1,
         -1, 271, 272, -1, -1, -1, 271, 272, -1, -1,
         -1, -1, -1, -1, -1, 59, 60, -1, -1, -1,
         64, 65, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, 257, 258, 259, 260, 261, 262, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, 262, 263, 264, 265, 266,
         267, 268, -1, 270, 271, -1, 273, -1, 275, 276,
         277, 278, -1, -1, -1, -1, -1, -1, 262, 263,
         264, 265, 266, 267, 268, -1, 270, 271, -1, 273,
         -1, 275, 276, 277, 278, 262, 263, 264, 265, 266,
         267, 268, -1, 270, -1, -1, 273, -1, 275, 276,
         277, 278, 262, 263, 264, 265, 266, 267, 268, -1,
         270, -1, -1, 273, -1, 275, 276, -1, 278, 262,
         263, 264, 265, 266, 267, 268, 262, 270, -1, -1,
         273, -1, 275, 276, -1, -1, 272, -1, -1, -1,
         276,
         };
   final static short YYFINAL = 10;
   final static short YYMAXTOKEN = 278;
   final static String yyname[] = {
         "end-of-file", null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, "'('", "')'", "'*'", "'+'", "','",
         "'-'", null, "'/'", null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         "'|'", null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
         null, null, null, null, null, null, "IDENTIFIER", "STRING", "DOUBLE", "LONG", "CST", "NOT",
         "EQUAL", "GT", "GE", "LT", "LE", "DIFFERENT", "NEG", "BETWEEN", "AND2", "ESCAPE", "LIKE",
         "NULL", "IN", "IS", "OR", "AND",
         };
   final static String yyrule[] = {
         "$accept : total",
         "total : comp '|'",
         "unary : IDENTIFIER",
         "unary : STRING",
         "unary : DOUBLE",
         "unary : LONG",
         "unary : CST",
         "unary : '(' comp ')'",
         "comp : unary",
         "comp : comp '+' comp",
         "comp : comp '-' comp",
         "comp : comp '*' comp",
         "comp : comp '/' comp",
         "comp : comp AND comp",
         "comp : comp OR comp",
         "comp : comp EQUAL comp",
         "comp : comp GT comp",
         "comp : comp GE comp",
         "comp : comp LT comp",
         "comp : comp LE comp",
         "comp : comp DIFFERENT comp",
         "comp : comp IS NULL",
         "comp : comp IS NOT NULL",
         "comp : NOT comp",
         "comp : '-' comp",
         "comp : '+' comp",
         "comp : comp BETWEEN comp AND2 comp",
         "comp : comp NOT BETWEEN comp AND2 comp",
         "comp : comp LIKE comp",
         "comp : comp NOT LIKE comp",
         "comp : comp LIKE comp ESCAPE comp",
         "comp : comp NOT LIKE comp ESCAPE comp",
         "comp : comp IN '(' strings ')'",
         "comp : comp NOT IN '(' strings ')'",
         "strings : STRING",
         "strings : strings ',' STRING",
         };

   //the current working character

//########## MESSAGES ##########
//###############################################################
// method: debug
//###############################################################
   void debug( String msg ) {
      if ( yydebug ) {
         System.out.println( msg );
      }
   }

   //state stack

//###############################################################
// methods: state stack push,pop,drop,peek
//###############################################################
   void state_push( int state ) {
      if ( stateptr >= YYSTACKSIZE ) {
         //overflowed?
         return;
      }
      statestk[++stateptr] = state;
   }

   int state_pop() {
      if ( stateptr < 0 ) {
         //underflowed?
         return -1;
      }
      return statestk[stateptr--];
   }

   void state_drop( int cnt ) {
      int ptr;
      ptr = stateptr - cnt;
      if ( ptr < 0 ) {
         return;
      }
      stateptr = ptr;
   }

   int state_peek( int relative ) {
      int ptr;
      ptr = stateptr - relative;
      if ( ptr < 0 ) {
         return -1;
      }
      return statestk[ptr];
   }

//###############################################################
// method: init_stacks : allocate and prepare stacks
//###############################################################
   boolean init_stacks() {
      statestk = new int[YYSTACKSIZE];
      stateptr = -1;
      val_init();
      return true;
   }

//###############################################################
// method: dump_stacks : show n levels of the stacks
//###############################################################
   void dump_stacks( int count ) {
      int i;
      System.out.println( "=index==state====value=     s:" + stateptr + "  v:" + valptr );
      for ( i = 0; i < count; i++ ) {
         System.out.println( " " + i + "    " + statestk[i] + "      " + valstk[i] );
      }
      System.out.println( "======================" );
   }

//###############################################################
// methods: value stack push,pop,drop,peek.
//###############################################################
   void val_init() {
      valstk = new parserval[YYSTACKSIZE];
      yyval = new parserval( 0 );
      yylval = new parserval( 0 );
      valptr = -1;
   }

   void val_push( parserval val ) {
      if ( valptr >= YYSTACKSIZE ) {
         return;
      }
      valstk[++valptr] = val;
   }

   parserval val_pop() {
      if ( valptr < 0 ) {
         return new parserval( -1 );
      }
      return valstk[valptr--];
   }

   void val_drop( int cnt ) {
      int ptr;
      ptr = valptr - cnt;
      if ( ptr < 0 ) {
         return;
      }
      valptr = ptr;
   }

   parserval val_peek( int relative ) {
      int ptr;
      ptr = valptr - relative;
      if ( ptr < 0 ) {
         return new parserval( -1 );
      }
      return valstk[ptr];
   }

   void yyerror( String s ) {
      throw new RuntimeException( "PARSER ERROR: " + s );
   }

   void aff( parserval val, String st ) {
      System.out.print( "[" + st + "] " );
      if ( val == null ) {
         System.out.println( "null" );
      } else {
         if ( val.obj == null ) {
            System.out.println( ".obj=null" );
         } else {
            System.out.println( ".obj=" + val.obj.toString() );
         }
      }
   }

   void next() {
      if ( st.hasMoreTokens() ) {
         nextToken = st.nextToken();
      } else {
         nextToken = null;
      }
   }

   int yylex() {
      yylval = null;

      String s = nextToken;
      if ( s == null ) {
         return 0;
      }
      next();

      //Whitespace
      while ( s.equals( " " ) ) {
         s = nextToken;
         if ( s == null ) {
            return 0;
         }
         next();
      }

      //String
      if ( s.equals( "'" ) ) {

         String string = "";
         while ( ( nextToken != null ) && ( !nextToken.equals( "'" ) ) ) {
            string += nextToken;
            next();
         }

         if ( nextToken == null ) {
            return -1;
         }
         next();

         yylval = new parserval( ( Object )string );
         return STRING;
      }

      //Is it an integer/double ?
      if ( Character.isDigit( s.charAt( 0 ) ) ) {
         try {
            yylval = new parserval( Long.valueOf( s ) );
            return LONG;
         } catch ( NumberFormatException e ) {
         }

         try {
            yylval = new parserval( Double.valueOf( s ) );
            return DOUBLE;
         } catch ( NumberFormatException e ) {
            return -1;
         }
      }

      //CST group
      if ( s.equals( "TRUE" ) || s.equals("true") ) {
         yylval = new parserval( ( Object )Boolean.TRUE );
         return CST;
      }
      if ( s.equals( "FALSE" ) || s.equals("false") ) {
         yylval = new parserval( ( Object )Boolean.FALSE );
         return CST;
      }

      //OPERATOR group
      if ( s.equals( "NOT" ) ) {
         return NOT;
      }
      if ( s.equals( "AND" ) ) {
         return AND;
      }
      if ( s.equals( "OR" ) ) {
         return OR;
      }
      if ( s.equals( "BETWEEN" ) ) {
         return BETWEEN;
      }
      if ( s.equals( "and" ) ) {
         return AND2;
      }
      if ( s.equals( "LIKE" ) ) {
         return LIKE;
      }
      if ( s.equals( "IN" ) ) {
         return IN;
      }
      if ( s.equals( "NULL" ) ) {
         return NULL;
      }
      if ( s.equals( "IS" ) ) {
         return IS;
      }
      if ( s.equals( "ESCAPE" ) ) {
         return ESCAPE;
      }

      //BRACKET group
      if ( s.equals( "(" ) ) {
         return '(';
      }
      if ( s.equals( ")" ) ) {
         return ')';
      }
      if ( s.equals( "," ) ) {
         return ',';
      }
      if ( s.equals( "|" ) ) {
         return '|';
      }

      //COMP group
      if ( s.equals( "=" ) ) {
         return EQUAL;
      }
      if ( s.equals( ">" ) ) {
         if ( nextToken != null && nextToken.equals( "=" ) ) {
            next();
            return GE;
         }
         return GT;
      }
      if ( s.equals( "<" ) ) {
         if ( nextToken != null && nextToken.equals( ">" ) ) {
            next();
            return DIFFERENT;
         }
         if ( nextToken != null && nextToken.equals( "=" ) ) {
            next();
            return LE;
         }
         return LT;
      }

      //CALC group
      if ( s.equals( "+" ) ) {
         return '+';
      }
      if ( s.equals( "-" ) ) {
         return '-';
      }
      if ( s.equals( "*" ) ) {
         return '*';
      }
      if ( s.equals( "/" ) ) {
         return '/';
      }

      //We should check if s is a _correct_ string
      Identifier id = ( Identifier )map.get( s );
      if ( id == null ) {
         id = new Identifier( s );
         map.put( s, id );
      }
      yylval = new parserval( id );
      return IDENTIFIER;
   }

   public Object parse(String selector, HashMap identifierMap, boolean trace) throws Exception
   {
      return parse(selector, identifierMap);
   }
   public Object parse(String sel, HashMap map) throws Exception
   {
      selector = null;
      nextToken = null;
      this.map = map;

      sel += "|";
      st = new StringTokenizer( sel, " '(),=><+-*/|", true );
      next();
      yyparse();
      return selector;
   }

//#line 528 "parser.java"
//###############################################################
// method: yylexdebug : check lexer state
//###############################################################
   void yylexdebug( int state, int ch ) {
      String s = null;
      if ( ch < 0 ) {
         ch = 0;
      }
      if ( ch <= YYMAXTOKEN ) {
         //check index bounds
         s = yyname[ch];
      }
      //now get it
      if ( s == null ) {
         s = "illegal-symbol";
      }
      debug( "state " + state + ", reading " + ch + " (" + s + ")" );
   }


//###############################################################
// method: yyparse : parse input and execute indicated items
//###############################################################
   int yyparse() {
      int yyn;
      //next next thing to do
      int yym;
      //
      int yystate;
      //current parsing state from state table
      String yys;
      //current token string
      boolean doaction;
      init_stacks();
      yynerrs = 0;
      yyerrflag = 0;
      yychar = -1;
      //impossible char forces a read
      yystate = 0;
      //initial state
      state_push( yystate );
      //save it
      while ( true ) {
         //until parsing is done, either correctly, or w/error

         doaction = true;
         if ( yydebug ) {
            debug( "loop" );
         }
         //#### NEXT ACTION (from reduction table)
         for ( yyn = yydefred[yystate]; yyn == 0; yyn = yydefred[yystate] ) {
            if ( yydebug ) {
               debug( "yyn:" + yyn + "  state:" + yystate + "  char:" + yychar );
            }
            if ( yychar < 0 ) {
               //we want a char?

               yychar = yylex();
               //get next token
               //#### ERROR CHECK ####
               if ( yychar < 0 ) {
                  //it it didn't work/error

                  yychar = 0;
                  //change it to default string (no -1!)
                  if ( yydebug ) {
                     yylexdebug( yystate, yychar );
                  }
               }
            }
            //yychar<0
            yyn = yysindex[yystate];
            //get amount to shift by (shift index)
            if ( ( yyn != 0 ) && ( yyn += yychar ) >= 0 &&
                  yyn <= YYTABLESIZE && yycheck[yyn] == yychar ) {
               if ( yydebug ) {
                  debug( "state " + yystate + ", shifting to state " + yytable[yyn] + "" );
               }
               //#### NEXT STATE ####
               yystate = yytable[yyn];
               //we are in a new state
               state_push( yystate );
               //save it
               val_push( yylval );
               //push our lval as the input for next rule
               yychar = -1;
               //since we have 'eaten' a token, say we need another
               if ( yyerrflag > 0 ) {
                  //have we recovered an error?
                  --yyerrflag;
               }
               //give ourselves credit
               doaction = false;
               //but don't process yet
               break;
               //quit the yyn=0 loop
            }

            yyn = yyrindex[yystate];
            //reduce
            if ( ( yyn != 0 ) && ( yyn += yychar ) >= 0 &&
                  yyn <= YYTABLESIZE && yycheck[yyn] == yychar ) {
               //we reduced!
               if ( yydebug ) {
                  debug( "reduce" );
               }
               yyn = yytable[yyn];
               doaction = true;
               //get ready to execute
               break;
               //drop down to actions
            } else {
               //ERROR RECOVERY

               if ( yyerrflag == 0 ) {
                  yyerror( "syntax error" );
                  yynerrs++;
               }
               if ( yyerrflag < 3 ) {
                  //low error count?

                  yyerrflag = 3;
                  while ( true ) {
                     //do until break

                     if ( stateptr < 0 ) {
                        //check for under & overflow here

                        yyerror( "stack underflow. aborting..." );
                        //note lower case 's'
                        return 1;
                     }
                     yyn = yysindex[state_peek( 0 )];
                     if ( ( yyn != 0 ) && ( yyn += YYERRCODE ) >= 0 &&
                           yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE ) {
                        if ( yydebug ) {
                           debug( "state " + state_peek( 0 ) + ", error recovery shifting to state " + yytable[yyn] + " " );
                        }
                        yystate = yytable[yyn];
                        state_push( yystate );
                        val_push( yylval );
                        doaction = false;
                        break;
                     } else {
                        if ( yydebug ) {
                           debug( "error recovery discarding state " + state_peek( 0 ) + " " );
                        }
                        if ( stateptr < 0 ) {
                           //check for under & overflow here

                           yyerror( "Stack underflow. aborting..." );
                           //capital 'S'
                           return 1;
                        }
                        state_pop();
                        val_pop();
                     }
                  }
               } else {
                  //discard this token

                  if ( yychar == 0 ) {
                     return 1;
                  }
                  //yyabort
                  if ( yydebug ) {
                     yys = null;
                     if ( yychar <= YYMAXTOKEN ) {
                        yys = yyname[yychar];
                     }
                     if ( yys == null ) {
                        yys = "illegal-symbol";
                     }
                     debug( "state " + yystate + ", error recovery discards token " + yychar + " (" + yys + ")" );
                  }
                  yychar = -1;
                  //read another
               }
            }
            //end error recovery
         }
         //yyn=0 loop
         if ( !doaction ) {
            //any reason not to proceed?
            continue;
         }
         //skip action
         yym = yylen[yyn];
         //get count of terminals on rhs
         if ( yydebug ) {
            debug( "state " + yystate + ", reducing " + yym + " by rule " + yyn + " (" + yyrule[yyn] + ")" );
         }
         if ( yym > 0 ) {
            //if count of rhs not 'nil'
            yyval = val_peek( yym - 1 );
         }
         //get current semantic value
         switch ( yyn ) {
//########## USER-SUPPLIED ACTIONS ##########
            case 1:
            {
//#line 29 "jms.y"
               selector = val_peek( 1 ).obj;
            }
               break;
            case 7:
            {
//#line 37 "jms.y"
               yyval = val_peek( 1 );
            }
               break;
            case 9:
            {
//#line 41 "jms.y"
               yyval.obj = new Operator( Operator.ADD, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 10:
            {
//#line 42 "jms.y"
               yyval.obj = new Operator( Operator.SUB, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 11:
            {
//#line 43 "jms.y"
               yyval.obj = new Operator( Operator.MUL, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 12:
            {
//#line 44 "jms.y"
               yyval.obj = new Operator( Operator.DIV, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 13:
            {
//#line 45 "jms.y"
               yyval.obj = new Operator( Operator.AND, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 14:
            {
//#line 46 "jms.y"
               yyval.obj = new Operator( Operator.OR, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 15:
            {
//#line 47 "jms.y"
               yyval.obj = new Operator( Operator.EQUAL, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 16:
            {
//#line 48 "jms.y"
               yyval.obj = new Operator( Operator.GT, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 17:
            {
//#line 49 "jms.y"
               yyval.obj = new Operator( Operator.GE, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 18:
            {
//#line 50 "jms.y"
               yyval.obj = new Operator( Operator.LT, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 19:
            {
//#line 51 "jms.y"
               yyval.obj = new Operator( Operator.LE, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 20:
            {
//#line 52 "jms.y"
               yyval.obj = new Operator( Operator.DIFFERENT, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 21:
            {
//#line 53 "jms.y"
               yyval.obj = new Operator( Operator.IS_NULL, val_peek( 2 ).obj );
            }
               break;
            case 22:
            {
//#line 54 "jms.y"
               yyval.obj = new Operator( Operator.IS_NOT_NULL, val_peek( 3 ).obj );
            }
               break;
            case 23:
            {
//#line 55 "jms.y"
               yyval = new parserval( new Operator( Operator.NOT, val_peek( 0 ).obj ) );
            }
               break;
            case 24:
            {
//#line 56 "jms.y"
               yyval = new parserval( new Operator( Operator.NEG, val_peek( 0 ).obj ) );
            }
               break;
            case 25:
            {
//#line 57 "jms.y"
               yyval = val_peek( 0 );
            }
               break;
            case 26:
            {
//#line 58 "jms.y"
               yyval.obj = new Operator( Operator.BETWEEN, val_peek( 4 ).obj, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 27:
            {
//#line 59 "jms.y"
               yyval.obj = new Operator( Operator.NOT_BETWEEN, val_peek( 5 ).obj, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 28:
            {
//#line 60 "jms.y"
               yyval.obj = new Operator( Operator.LIKE, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 29:
            {
//#line 61 "jms.y"
               yyval.obj = new Operator( Operator.NOT_LIKE, val_peek( 3 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 30:
            {
//#line 62 "jms.y"
               yyval.obj = new Operator( Operator.LIKE_ESCAPE, val_peek( 4 ).obj, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 31:
            {
//#line 63 "jms.y"
               yyval.obj = new Operator( Operator.NOT_LIKE_ESCAPE, val_peek( 5 ).obj, val_peek( 2 ).obj, val_peek( 0 ).obj );
            }
               break;
            case 32:
            {
//#line 64 "jms.y"
               yyval.obj = new Operator( Operator.IN, val_peek( 4 ).obj, val_peek( 1 ).obj );
            }
               break;
            case 33:
            {
//#line 65 "jms.y"
               yyval.obj = new Operator( Operator.NOT_IN, val_peek( 5 ).obj, val_peek( 1 ).obj );
            }
               break;
            case 34:
            {
//#line 68 "jms.y"
               HashSet tmp = new HashSet();
               tmp.add( val_peek( 0 ).obj );
               yyval.obj = tmp;
            }
               break;
            case 35:
            {
//#line 69 "jms.y"
               ( ( HashSet )val_peek( 2 ).obj ).add( val_peek( 0 ).obj );
               yyval = val_peek( 2 );
            }
               break;
//#line 787 "parser.java"
//########## END OF USER-SUPPLIED ACTIONS ##########
         }
         //switch
         //#### Now let's reduce... ####
         if ( yydebug ) {
            debug( "reduce" );
         }
         state_drop( yym );
         //we just reduced yylen states
         yystate = state_peek( 0 );
         //get new state
         val_drop( yym );
         //corresponding value drop
         yym = yylhs[yyn];
         //select next TERMINAL(on lhs)
         if ( yystate == 0 && yym == 0 ) {
            //done? 'rest' state and at first TERMINAL

            debug( "After reduction, shifting from state 0 to state " + YYFINAL + "" );
            yystate = YYFINAL;
            //explicitly say we're done
            state_push( YYFINAL );
            //and save it
            val_push( yyval );
            //also save the semantic value of parsing
            if ( yychar < 0 ) {
               //we want another character?

               yychar = yylex();
               //get next character
               if ( yychar < 0 ) {
                  yychar = 0;
               }
               //clean, if necessary
               if ( yydebug ) {
                  yylexdebug( yystate, yychar );
               }
            }
            if ( yychar == 0 ) {
               //Good exit (if lex returns 0 ;-)
               break;
            }
            //quit the loop--all DONE
         }
         //if yystate
         else {
            //else not done yet
            //get next state and push, for next yydefred[]
            yyn = yygindex[yym];
            //find out where to go
            if ( ( yyn != 0 ) && ( yyn += yystate ) >= 0 &&
                  yyn <= YYTABLESIZE && yycheck[yyn] == yystate ) {
               yystate = yytable[yyn];
            }
            //get new state
            else {
               yystate = yydgoto[yym];
            }
            //else go to new defred
            debug( "after reduction, shifting from state " + state_peek( 0 ) + " to state " + yystate + "" );
            state_push( yystate );
            //going again, so push state & val...
            val_push( yyval );
            //for next action
         }
      }
      //main loop
      return 0;
      //yyaccept!!
   }

//## end of method parse() ######################################


}
