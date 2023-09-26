#include "../zlib/zutil.h"

#if defined(INFFAST_OPT)

/*
 Entry assumptions:
    state->mode == LEN
    strm->avail_in >= INFLATE_MIN_INPUT
    strm->avail_out >= INFLATE_MIN_OUTPUT
    start >= strm->avail_out
    state->bits <= 8
 
 On return, state->mode is one of:
    LEN -- ran out of enough output space or enough available input
    TYPE -- reached end of block code, inflate() to interpret next block
    BAD -- error in block data
 
 Other
    See inffast.c for more information.
    Critical path comments:
      - assume LENBITS=11 and DISTBITS=8
      - e = extended lookup, x = extra bits
 */

#include "../zlib/inffast.h"
#include "../zlib/inftrees.h"
#include "../zlib/inflate.h"
#include "zopt_inffast.h"

#define INFLATE_REFILL(new_bits)                                 \
  {                                                              \
    new_bits = (*(packed_uint64_t*)src_buffer << n_bits) | bits; \
    src_buffer += (63 - n_bits) >> 3;                            \
    n_bits |= 56;                                                \
  }

#define INFLATE_DECODE(sym, table, index) \
  {                                       \
    sym = table[index];                   \
    /* no reordering by compiler */       \
    asm volatile ("" : "+r"(sym));        \
  }

#define INFLATE_CONSUME(sym)      \
  {                               \
    bits_symbol = (uint32_t)bits; \
    bits >>= sym.bits;            \
    n_bits -= sym.bits;           \
  }

void ZLIB_INTERNAL inflate_fast(z_streamp strm, unsigned start) /* inflate()'s starting value for strm->avail_out */
{
  struct inflate_state* state;      // Internal state
  
  const unsigned char* src_buffer;  // Payload
  const unsigned char* src_safe;    // Payload, safe for fast loop
  const unsigned char* src_end;     // End of payload
  
  unsigned char* dst_buffer;        // Output
  unsigned char* dst_begin;         // Start of output
  unsigned char* dst_safe;          // Output, safe for fast loop
  unsigned char* dst_end;           // End of output
  
  const code* bits_to_lenlit;       // Huffman LUTs for lengths/literals
  const code* bits_to_distance;     // Huffman LUTs for distances
  uint32_t lenlit_mask;             // Mask for lengths/literals root table
  uint32_t distance_mask;           // Mask for distances root table
  
  uint64_t bits;                    // Bits available for next decode
  uint32_t bits_symbol;             // Consumed bits of last symbol *_SYM
  uint32_t n_bits;                  // # of bits available in BITS
  
  const unsigned char* window;      // Sliding window, if wsize > 0
  uint32_t win_size;                // Size of sliding window
  uint32_t win_top;                 // Sliding window: next write
  
  // Copy state
  state = (struct inflate_state*)strm->state;
  
  // Setup buffers
  src_buffer = strm->next_in;
  dst_buffer = strm->next_out;
  src_end    = src_buffer + strm->avail_in;
  dst_end    = dst_buffer + strm->avail_out;
  src_safe   = src_end - INFLATE_MIN_INPUT;
  dst_safe   = dst_end - INFLATE_MIN_OUTPUT;
  dst_begin  = dst_end - start;
  
  // Setup window
  window   = state->window;
  win_size = state->wsize;
  win_top  = (state->whave >= win_size) && (state->wnext == 0) ? win_size : state->wnext;
  
  // Setup Huffman tables
  bits_to_lenlit   = state->lencode;
  bits_to_distance = state->distcode;
  lenlit_mask   = ~(-1 << state->lenbits);
  distance_mask = ~(-1 << state->distbits);

  // Refill and enforce some assumptions
  n_bits = state->bits & 63;
  bits   = state->hold & ~(UINT64_C(-1) << n_bits);
  INFLATE_REFILL(bits);
  
  //---------------------------------------------------------------------------- Inner loop
  // Decode until unsafe, end of block or distance error
  for (;;)
  {
    code l_sym;                 // Current/pending length/literal Huffman symbol
    code d_sym;                 // Current/pending distance       Huffman symbol
    uint32_t length;            // Match length
    uint32_t distance;          // Match distance

    // Decode length/literal symbol and refill bits
    INFLATE_DECODE(l_sym, bits_to_lenlit, bits & lenlit_mask);
    INFLATE_REFILL(bits); // We have 56+ bits
    INFLATE_CONSUME(l_sym);

  lenlit_decode:
    //-------------------------------------------------------------------------- LEN/LIT: Root literal?
    if (likely(l_sym.op == 0))
    {
      *dst_buffer++ = (unsigned char)l_sym.val;
      
      // Decode length/literal symbol
      INFLATE_DECODE(l_sym, bits_to_lenlit, bits & lenlit_mask);
      INFLATE_CONSUME(l_sym);
      
      //------------------------------------------------------------------------ LEN/LIT: Root literal?
      if (likely(l_sym.op == 0))
      {
        *dst_buffer++ = (unsigned char)l_sym.val;
        
        // Decode length/literal symbol
        INFLATE_DECODE(l_sym, bits_to_lenlit, bits & lenlit_mask);
        INFLATE_CONSUME(l_sym);

      lit_try_extended:
        //---------------------------------------------------------------------- LEN/LIT: Another literal?
        if (likely(l_sym.op == 0))
        {
          // Critical path: (11) + (11) + (11+4e) = 37 bits
          *dst_buffer++ = (unsigned char)l_sym.val;
          
          // Safe to continue?
          if (likely((src_buffer <= src_safe) && (dst_buffer <= dst_safe))) continue;
          
          // Unsafe, stop.
          break;
        }
      }
    }
    const uint8_t lenlit_op = (unsigned)l_sym.op;
    //-------------------------------------------------------------------------- LEN/LIT: Length decodable?
    if (likely(lenlit_op & 16))
    {
      // Decode distance symbol (root)
      // Critical path: (11) + (11) + (11+4e+5x) = 42 bits
      // Since we started with 56+ bits, we are still good.
      INFLATE_DECODE(d_sym, bits_to_distance, bits & distance_mask);

      // The complete distance needs up to 28 (8+7e+13x) bits.
      if (unlikely(n_bits < 15 + 13))
      {
        INFLATE_REFILL(bits); // (**)
      }

      // Decode length using base and extra bits
      length = ((bits_symbol << (32 - l_sym.bits)) >> 1 >> (31 - (lenlit_op & 15))) + l_sym.val;

      // Consume distance
      INFLATE_CONSUME(d_sym);

    distance_decode:;
      const uint8_t distance_op = (unsigned)(d_sym.op);
      //------------------------------------------------------------------------ DISTANCE: Distance decodable?
      if (likely(distance_op & 16))
      {
        // Refill for length/literal decode?
        if (unlikely(n_bits < INFLATE_LEN_BITS_OPT))
        {
          INFLATE_REFILL(bits); // It's unlikely, that we don't have enough bits for the lookup.
        }

        // Decode distance using base and extra bits
        distance = ((bits_symbol << (32 - d_sym.bits)) >> 1 >> (31 - (distance_op & 15))) + d_sym.val;
        
        // Decode length/literal symbol (root) and refill bits.
        bits_symbol = (uint32_t)bits;
        INFLATE_REFILL(bits); // Buffer bits for next iteration / revert
        INFLATE_DECODE(l_sym, bits_to_lenlit, bits_symbol & lenlit_mask);
        INFLATE_CONSUME(l_sym);
        
        //---------------------------------------------------------------------- Match copy
        // Copy from window?
        const uint32_t dst_available = (uint32_t)(dst_buffer - dst_begin);
        if (distance > dst_available)
        {
          const uint32_t win_distance = distance - dst_available;
          uint32_t match_pos = win_top - win_distance;

          // Match starts in [0, win_top)?
          if ((int32_t)match_pos >= 0)
          {
            // Some output needed?
            if (unlikely(match_pos + length > win_top)) goto match_copy_edge_cases;
          }else
          {
            // Match starts in [win_top, win_size)
            match_pos += win_size;

            // Out of window?
            if (unlikely((int32_t)match_pos < (int32_t)win_top))
            {
              strm->msg = (z_const char*)"invalid distance too far back";
              state->mode = BAD;
              break;
            }
          }
          
          // Unsafe or wrap around?
          if (unlikely(match_pos + length + 63 > win_size)) goto match_copy_edge_cases;

          // Use simple copy
          dst_buffer = inflate_copy_fast(dst_buffer, window + match_pos, length);
          goto match_copy_done;
          
          //----------------------------------------------------------------------
          // This loop covers edge cases, where we start within WINDOW
          //    1) Wrap around in WINDOW
          //    2) Wrap around to DST_BUFFER
          //    3) Unsafe copies
          //----------------------------------------------------------------------
        match_copy_edge_cases:
          for (;;)
          {
            *dst_buffer++ = window[match_pos++];
            if (--length == 0) goto match_copy_done;  // Done
            if (match_pos == win_top) break;          // Wrap around to DST_BUFFER
            if (match_pos == win_size) match_pos = 0; // Wrap around in WINDOW
          }
        }

        // Copy from DST_BUFFER (handle overlapping copies)
        dst_buffer = (length <= distance ?
                      inflate_copy_fast(dst_buffer, dst_buffer - distance, length) :
                      (distance < 16 ?
                       inflate_copy_with_overlap_small(dst_buffer, distance, length) :
                       inflate_copy_with_overlap_large(dst_buffer, distance, length)));
        
      match_copy_done:
        // Safe to continue?
        if (likely((src_buffer <= src_safe) && (dst_buffer <= dst_safe))) goto lenlit_decode;

        // Unsafe, revert pending decode and stop.
        n_bits += l_sym.bits;
        bits = bits_symbol;
        break;
      }
      //------------------------------------------------------------------------ DISTANCE: Need second level table?
      else if (likely((distance_op & 64) == 0))
      {
        // Decode distance symbol
        INFLATE_DECODE(d_sym, bits_to_distance, ((uint32_t)bits & ~(-1 << distance_op)) + d_sym.val);
        INFLATE_CONSUME(d_sym); // Critical path: we have enough bits, see (**)
        goto distance_decode;
      }
      // Bitstream error (***)
    }
    //-------------------------------------------------------------------------- LEN/LIT: Need second level table?
    else if (likely((lenlit_op & 64) == 0))
    {
      // Decode length/literal symbol
      INFLATE_DECODE(l_sym, bits_to_lenlit, ((uint32_t)bits & ~(-1 << lenlit_op)) + l_sym.val);
      INFLATE_CONSUME(l_sym);
      goto lit_try_extended;
    }
    //-------------------------------------------------------------------------- LEN/LIT: End of block?
    else if (lenlit_op & 32)
    {
      state->mode = TYPE;
      break;
    }
    //-------------------------------------------------------------------------- LEN/LIT/DISTANCE: Bitstream error (***)
    {
      strm->msg = (char*)"invalid literal/length/distance code";
      state->mode = BAD;
      break;
    }
  };
  
  // Return unused bytes/bits
  src_buffer -= n_bits >> 3; // Bytes
  n_bits &= 7;
  state->bits = n_bits;
  state->hold = (uint32_t)bits & ~(-1 << n_bits);

  // Update state
  strm->next_in   = (z_const unsigned char*)src_buffer;
  strm->next_out  = dst_buffer;
  strm->avail_in  = (unsigned int)(src_end - src_buffer);
  strm->avail_out = (unsigned int)(dst_end - dst_buffer);
}

// Build Huffman tables. Return 0 on success, 1 on error.
int inffast_tables(z_streamp strm) // OK
{
  struct inflate_state* state = (struct inflate_state*)strm->state;
  int ret = 0;
  
  // Try to build larger tables
  state->lenbits = INFLATE_LEN_BITS_OPT;
  state->distbits = INFLATE_DIST_BITS_OPT;

  // Build tables...
  for (;;)
  {
    // Build length codes
    state->next = state->codes;
    state->lencode = state->next;
    ret = inflate_table(LENS, state->lens, state->nlen,
                        &(state->next), &(state->lenbits), state->work,
                        (unsigned)(state->codes + ENOUGH - state->next));
    if (ret)
    {
      strm->msg = (char *)"invalid literal/lengths set";
      state->mode = BAD;
      return 1;
    }
    
    // Build distance codes
    state->distcode = state->next;
    ret = inflate_table(DISTS, state->lens + state->nlen, state->ndist,
                        &(state->next), &(state->distbits), state->work,
                        (unsigned)(state->codes + ENOUGH - state->next));
    if (ret == 0) return 0; // Done

    // Fallback to default table sizes?
    if ((state->lenbits > INFLATE_LEN_BITS) || (state->distbits > INFLATE_DIST_BITS))
    {
      state->lenbits = INFLATE_LEN_BITS;
      state->distbits = INFLATE_DIST_BITS;
    }else
    {
      // Bad distance set
      strm->msg = (char *)"invalid distances set";
      state->mode = BAD;
      return 1;
    }
  }
}

#endif
