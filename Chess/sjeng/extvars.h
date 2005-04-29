/*
    Sjeng - a chess variants playing program
    Copyright (C) 2000 Gian-Carlo Pascutto

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    File: extvars.h                                        
    Purpose: global data definitions

*/

extern char divider[50];

extern int board[144], moved[144], ep_square, white_to_move, wking_loc,
  bking_loc, white_castled, black_castled, result, ply, pv_length[PV_BUFF],
  squares[144], num_pieces, i_depth, comp_color, fifty, piece_count;

extern long int nodes, raw_nodes, qnodes, killer_scores[PV_BUFF],
  killer_scores2[PV_BUFF], killer_scores3[PV_BUFF], moves_to_tc, min_per_game,
  sec_per_game, inc, time_left, opp_time, time_cushion, time_for_move, cur_score;

extern unsigned long history_h[144][144];

extern bool captures, searching_pv, post, time_exit, time_failure;
extern int xb_mode, maxdepth;

extern move_s pv[PV_BUFF][PV_BUFF], dummy, killer1[PV_BUFF], killer2[PV_BUFF],
  killer3[PV_BUFF];

extern  move_x path_x[PV_BUFF];
extern  move_s path[PV_BUFF];
  
extern rtime_t start_time;

extern int holding[2][16];
extern int num_holding[2];

extern int white_hand_eval;
extern int black_hand_eval;

extern int drop_piece;

extern int pieces[62];
extern int is_promoted[62];

extern int num_makemoves;
extern int num_unmakemoves;
extern int num_playmoves;
extern int num_pieceups;
extern int num_piecedowns;
extern int max_moves;

/* piece types range form 0..16 */
extern unsigned long zobrist[17][144];
extern unsigned long hash;

extern unsigned long ECacheProbes;
extern unsigned long ECacheHits;

extern unsigned long TTProbes;
extern unsigned long TTHits;
extern unsigned long TTStores;

extern unsigned long hold_hash;

extern char book[4000][161];
extern int num_book_lines;
extern int book_ply;
extern int use_book;
extern char opening_history[STR_BUFF];
extern unsigned long bookpos[400], booktomove[400], bookidx;

extern int Material;
extern int material[17];
extern int zh_material[17];
extern int std_material[17];
extern int suicide_material[17];
extern int losers_material[17];

extern int NTries, NCuts, TExt;

extern char ponder_input[STR_BUFF];

extern bool is_pondering;

extern unsigned long FH, FHF, PVS, FULL, PVSF;
extern unsigned long ext_check, ext_recap, ext_onerep;
extern unsigned long razor_drop, razor_material;

extern unsigned long total_moves;
extern unsigned long total_movegens;

extern const int rank[144], file[144], diagl[144], diagr[144], sqcolor[144];

extern int Variant;
extern int Giveaway;
extern int forcedwin;

extern bool is_analyzing;

extern char my_partner[STR_BUFF];
extern bool have_partner;
extern bool must_sit;
extern int must_go;
extern bool go_fast;
extern bool piecedead;
extern bool partnerdead;
extern int tradefreely;

extern char true_i_depth;

extern long fixed_time;

extern int hand_value[];

extern int numb_moves;

extern int phase;

FILE *lrn_standard;
FILE *lrn_zh;
FILE *lrn_suicide;
FILE *lrn_losers;
extern int bestmovenum;

extern int ugly_ep_hack;

extern int root_to_move;

extern int kingcap;

extern int pn_time;
extern move_s pn_move;
extern move_s pn_saver;
extern bool kibitzed;
extern int rootlosers[PV_BUFF];
extern int alllosers;
extern int s_threat;

extern int cfg_booklearn;
extern int cfg_devscale;
extern int cfg_razordrop;
extern int cfg_cutdrop;
extern int cfg_futprune;
extern int cfg_onerep;
extern int cfg_recap;
extern int cfg_smarteval;
extern int cfg_attackeval;
extern float cfg_scalefac;
extern int cfg_ksafety[15][9];
extern int cfg_tropism[5][7];
extern int havercfile;
extern int TTSize;
extern int PBSize;
extern int ECacheSize;

extern int my_rating, opp_rating;
extern int userealholdings;
extern char realholdings[255];

extern int move_number;
extern unsigned long hash_history[600];

extern int moveleft;
extern int movetotal;
extern char searching_move[20];

extern char setcode[30];

extern int cbEGTBCompBytes;
extern int EGTBProbes;
extern int EGTBHits;
extern int EGTBPieces;
extern int EGTBCacheSize;
extern char EGTBDir[STR_BUFF];
extern int SEGTB;






