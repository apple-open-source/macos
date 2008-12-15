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

    File: protos.h                                        
    Purpose: function prototypes

*/

#ifndef PROTOS_H
#define PROTOS_H

#include <stdint.h>

int32_t allocate_time (void);
bool check_legal (move_s moves[], int m, int incheck);
void comp_to_coord (move_s move, char str[]);
void display_board (FILE *stream, int color);
int32_t end_eval (void);
int32_t seval(void);
int32_t std_eval (void);
int32_t suicide_eval (void);
int32_t losers_eval (void);
int32_t eval (void);
void gen (move_s moves[]);
void ics_game_end (void);
bool in_check (void);
bool f_in_check (move_s moves[], int m);
int extended_in_check (void);
void init_game (void);
bool is_attacked (int square, int color);
bool nk_attacked (int square, int color);
bool is_move (char str[]);
void make (move_s moves[], int i);
void order_moves (move_s moves[], int32_t move_ordering[], int32_t see_values[], int num_moves, int best);
int32_t mid_eval (void);
int32_t opn_eval (void);
int32_t suicide_mid_eval(void);
void check_phase(void);
void perft (int depth);
void speed_test(void);
void perft_debug (void);
void post_thinking (int32_t score);
void post_fl_thinking (int32_t score, move_s *failmove);
void post_fh_thinking (int32_t score, move_s *failmove);
void post_fail_thinking(int32_t score, move_s *failmove);
void print_move (move_s moves[], int m, FILE *stream);
void push_pawn (int target, bool is_ep); 
void push_king_castle (int target, int castle_type);
void push_pawn_simple (int target);
void push_king (int target);
void push_knighT (int target);

void try_drop (int ptype);
		

void push_slidE (int target);
int32_t qsearch (int alpha, int beta, int depth);
void rdelay (int time_in_s);
int32_t rdifftime (rtime_t end, rtime_t start);
bool remove_one (int *marker, int32_t move_ordering[], int num_moves);
void reset_piece_square (void);
void check_piece_square (void);
void rinput (char str[], int n, FILE *stream);
rtime_t rtime (void);
int32_t search (int alpha, int beta, int depth, int is_null);
move_s search_root (int alpha, int beta, int depth);
void start_up (void);
move_s think (void);
void toggle_bool (bool *var);
void tree (int depth, int indent, FILE *output, char *disp_b);
void tree_debug (void);
void unmake (move_s moves[], int i);
bool verify_coord (char input[], move_s *move);

bool is_draw(void);

void ProcessHoldings(char line[]);
void addHolding(int what, int who);
void removeHolding(int what, int who);
void DropaddHolding(int what, int who);
void DropremoveHolding(int what, int who);

void printHolding(void);

int SwitchColor(int piece);
int SwitchPromoted(int piece);

int evalHolding(void);

void initialize_zobrist(void);
void initialize_hash(void);
void initialize_eval(void);

void checkECache(int32_t *score, int *in_cache);
void storeECache(int32_t score);

int init_book(void);
move_s choose_book_move(void);
move_s choose_binary_book_move(void);

void StoreTT(int score, int alpha, int beta, int best , int threat, int depth);
void QStoreTT(int score, int alpha, int beta, int best);
int ProbeTT(int *score, int alpha, int beta, int *best, int *threat, int *donull, int depth);
int QProbeTT(int *score, int alpha, int beta, int *best);
void LearnStoreTT(int score, unsigned nhash, unsigned hhash, int tomove, int best, int depth);

void LoadLearn(void);
void Learn(int score, int best, int depth);

void pinput (int n, FILE *stream);

int calc_attackers(int square, int color);

int interrupt(void);

void PutPiece(int color, char piece, char file, int rank);
void reset_board(void);

void reset_ecache(void);

void HandlePartner(char *input);
void HandlePtell(char *input);
void BegForPartner(void);
void CheckBadFlow(bool reset);

void run_epd_testsuite(void);

void ResetHandValue(void);

void build_book(void);
void comp_to_san (move_s move, char str[]);
void stringize_pv (char str[]);
  
void clear_tt(void);
void clear_dp_tt(void);

move_s proofnumbercheck(move_s compmove);
void proofnumbersearch(void);
void proofnumberscan(void);

void alloc_hash(void);
void alloc_ecache(void);
void free_hash(void);
void free_ecache(void);
void read_rcfile(void);

void book_learning(int result);
void seedMT(uint32_t seed);
uint32_t randomMT(void);

void setup_epd_line(char* inbuff);

int see(int color, int square, int from);

void init_egtb(void);
int probe_egtb(void);
void gen_all_tables(void);
int egtb(int s);

#endif

