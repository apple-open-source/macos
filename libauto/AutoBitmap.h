/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */
/*
    AutoBitmap.h
    High Performance Bitmap
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_BITMAP__
#define __AUTO_BITMAP__

#include "AutoDefs.h"
#include "AutoRange.h"


namespace Auto {

    //----- Bitmap -----//

    //
    // Bitmaps (bit vectors) are a fast and lightweight means of showing set
    // representation.  A single bit is used to indicate membership in the set; 1
    // indicating membership and 0 indicating exclusion.  In this representation we use
    // an array of unsigned long words. Each word represents a unit of bits_per_word members
    // numbered from the least significant bit (endian representation is not
    // important.)  Thus for any member k its word index would be equal to (k / bits_per_word) or
    // (k >> ilog2(bits_per_word)) and its bit position would be equal to (k % bits_per_word).
    //
    
    class Bitmap : public Range {

     private:         

        //
        // index
        //
        // Returns the word index of a specified bit.
        //
        static inline const usword_t index(const usword_t bp) { return bp >> bits_per_word_log2; }
        
        
        //
        // shift
        //
        // Returns the shift of a specified bit.
        //
        static inline const usword_t shift(const usword_t bp) { return bp & bits_mask; }
        
        
        //
        // cursor_bp
        //
        // Returns the bit position of the specified cursor.
        //
        inline usword_t cursor_bp(const usword_t *cursor) const { return ((uintptr_t)cursor - (uintptr_t)address()) << bits_per_byte_log2; }
        
        
        //
        // end_cursor
        //
        // Returns the end of the bits as a word address.
        //
        inline usword_t *end_cursor() const { return (usword_t *)Range::end(); }
        
        
     public:
     
        //
        // Constructors
        //
        Bitmap() {}
        Bitmap(usword_t n, void *bits) {
            // keep aligned for vector operations
            ASSERTION(!((uintptr_t)bits & mask(bytes_per_quad_log2)) && !(bytes_needed(n) & mask(bytes_per_quad_log2)));
            set_range(bits, bytes_needed(n));
        }
        
        
        //
        // bp_cursor
        //
        // Returns the word address containing a specified bit.
        //
        inline usword_t *bp_cursor(const usword_t bp) const {
            return (usword_t *)displace(address(), (bp >> (bits_per_word_log2 - bytes_per_word_log2)) & ~mask(bytes_per_word_log2));
        }
        
        
        //
        // size_in_bits
        //
        // Return the number of bits in the bitmap.
        //
        inline usword_t size_in_bits() const { return size() << bits_per_byte_log2; }
        
        
        //
        // initialize
        //
        // Set up the bit map for use.
        //
        inline void initialize(usword_t n, void *bits) { set_range(bits, bytes_needed(n)); }
        

        //
        // bytes_needed
        //
        // Returns the number of bytes needed to represent 'n' bits.
        //
        static inline const usword_t bytes_needed(usword_t n) { return partition2(n, bits_per_word_log2) << bytes_per_word_log2; }
        
        
        //
        // bit
        //
        // Returns the state of a bit in the bit map.
        //
        inline usword_t bit(const usword_t bp) const { return (*bp_cursor(bp) >> shift(bp)) & 1; }

        
        //
        // set_bit
        //
        // Set a bit in the bit map to 1.
        //
        inline void set_bit(const usword_t bp) const {
            usword_t *cursor = bp_cursor(bp);
            *cursor |= 1L << shift(bp);
        }

        
        //
        // set_bit_atomic
        //
        // Set a bit in the bit map to 1 atomically.
        //
        inline void set_bit_atomic(const usword_t bp) const {
            usword_t *cursor = bp_cursor(bp);
            __sync_or_and_fetch(cursor,  1L << shift(bp));
        }

        
        //
        // set_bits_large
        //
        // Set n bits in the bit map to 1 spanning more than one word.
        // Assumes that range spans more than one word.
        //
        void set_bits_large(const usword_t bp, const usword_t n) const;
        

        //
        // set_bits
        //
        // Set bits in the bit map to 1.
        //
        inline void set_bits(const usword_t bp, const usword_t n) const {
            const usword_t sh = shift(bp);                  // bit shift
            
            if ((sh + n) > bits_per_word) {
                set_bits_large(bp, n);
            } else {
                usword_t m = mask(n);                       // mask of n bits
                *bp_cursor(bp) |= (m << sh);                // set bits to 1
            }
        }

        //
        // clear_bit
        //
        // Set a bit in the bit map to 0.
        //
        inline void clear_bit(const usword_t bp) const {
            usword_t *cursor = bp_cursor(bp);
            *cursor &= ~(1L << shift(bp));
        }
                
        
        //
        // clear_bits_large
        //
        // Set n bits in the bit map to 0 spanning more than one word.
        // Assumes that range spans more than one word.
        //
        void clear_bits_large(const usword_t bp, const usword_t n) const; 
               
        //
        // clear_bits
        //
        // Set n bits in the bit map to 0.
        //
        inline void clear_bits(const usword_t bp, const usword_t n) const {
            const usword_t sh = shift(bp);                  // bit shift
            
            if ((sh + n) > bits_per_word) {
                clear_bits_large(bp, n);
            } else {
                usword_t m = mask(n);                       // mask of n bits
                *bp_cursor(bp) &= ~(m << sh);               // set bits to 0
            }
        }
        
        //
        // bits_are_clear_large
        //
        // Checks to see if a range of bits, spanning more than one word, are all 0.
        //
        bool bits_are_clear_large(const usword_t bp, const usword_t n) const;
        
        
        //
        // bits_are_clear
        //
        inline bool bits_are_clear(const usword_t bp, const usword_t n) const {
            const usword_t sh = shift(bp);                  // bit shift

            if ((sh + n) > bits_per_word) {
                return bits_are_clear_large(bp, n);
            } else {
                usword_t m = mask(n);                       // mask of n bits
                return (*bp_cursor(bp) & (m << sh)) == 0;   // see if bits are 0
            }
        }
        

        //
        // skip_all_zeros
        //
        // Rapidly skips through words of all zeros.
        //
        static inline usword_t *skip_all_zeros(usword_t *cursor, usword_t *end) {
            // near end address (allows multiple reads)
            usword_t *near_end = end - 4;
            
            // skip through as many all zeros as we can
            while (cursor < near_end) {
                // prefetch four words
                usword_t word0 = cursor[0];
                usword_t word1 = cursor[1];
                usword_t word2 = cursor[2];
                usword_t word3 = cursor[3];

                // assume they are all filled with zeros
                cursor += 4;

                // backtrack if we find out otherwise
                if (!is_all_zeros(word0)) { cursor -= 4; break; }
                if (!is_all_zeros(word1)) { cursor -= 3; break; }
                if (!is_all_zeros(word2)) { cursor -= 2; break; }
                if (!is_all_zeros(word3)) { cursor -= 1; break; }
            }
            
            // finish off the rest
            while (cursor < end) {
                usword_t word = *cursor++;
                if (!is_all_zeros(word)) { cursor--; break; }
            }

            return cursor;
        }
        
        
        //
        // skip_backward_all_zeros
        //
        // Rapidly skips through words of all zeros.
        //
        static inline usword_t *skip_backward_all_zeros(usword_t *cursor, usword_t *first) {
            // near first address (allows multiple reads)
            usword_t *near_first = first + 3;
            
            // skip through as many all zeros as we can
            while (near_first <= cursor) {
                // prefetch four words
                usword_t word0 = cursor[0];
                usword_t word1 = cursor[-1];
                usword_t word2 = cursor[-2];
                usword_t word3 = cursor[-3];

                // assume they are all filled with zeros
                cursor -= 4;

                // backtrack if we find out otherwise
                if (!is_all_zeros(word0)) { cursor += 4; break; }
                if (!is_all_zeros(word1)) { cursor += 3; break; }
                if (!is_all_zeros(word2)) { cursor += 2; break; }
                if (!is_all_zeros(word3)) { cursor += 1; break; }
            }
            
            // finish off the rest
            while (first <= cursor) {
                usword_t word = *cursor--;
                if (!is_all_zeros(word)) { cursor++; break; }
            }

            return cursor;
        }
 
 
        //
        // count_set
        // 
        // Returns the number of bits set (one) in the bit map using
        // a standard bit population algoirithm.
        //
        usword_t count_set() const ;


        //
        // previous_set
        //
        // Return the bit postion of the 1 that comes at or prior to the bit position 'bp'.
        //
        usword_t previous_set(const usword_t bp) const ;
        
        
        //
        // next_set
        //
        // Return the bit postion of the 1 that comes at or after to the bit position 'bp'.
        //
        usword_t next_set(const usword_t bp) const;
        
       
    };


};


#endif // __AUTO_BITMAP__
