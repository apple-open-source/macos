/* APPLE LOCAL: A tdep file used by both the i386 and the amd64 (x86_64) 
   targets.  These two architectures are similar enough that we can share 
   the same algorithms between them easily.  */

#ifndef X86_SHARED_TDEP_H
#define X86_SHARED_TDEP_H

struct gdbarch;

int x86_length_of_this_instruction (CORE_ADDR memaddr);

struct type *build_builtin_type_vec128i_big (void);

/* Keep track of how much prologue scanning we've done so far with
   this cache.  */

enum prologue_scan_state {
  no_scan_done = 0,
  quick_scan_succeeded,
  quick_scan_failed,
  full_scan_succeeded,
  full_scan_failed
};

/* A very specific meaning of "frame base" here in the x86_frame cache
   structure:  The value of the stack pointer on function entry.  When 
   dereferenced, the caller's saved EIP is found.  If this frame sets 
   up a frame pointer (ebp), its value is FRAME_BASE + 4.  e.g. on i386
                             ; FRAME_BASE value   |   EBP value   | ESP value
     foo+0:  push %ebp       ; 0xbc00000             caller's val   0xbc00000
     foo+1:  mov %esp, %ebp  ; 0xbc00000             caller's val   0xbbffffc
     foo+2:  push %ebx       ; 0xbc00000             0xbbffffc      0xbbffffc
     foo+3:  push %esi       ; 0xbc00000             0xbbffffc      0xbbffff8
     foo+6:  sub $0x12, %esp ; 0xbc00000             0xbbffffc      0xbbffff4
     foo+8:  mov %edi, -0xc(%ebp);0xbc00000          0xbbffffc      0xbbfffe6

   Prologue parsing is done in two parts:  The part that analyzes the
   instructions abstractly and the part that has actual register values
   to work with.  In the initial abstract form of this structure
   (where SAVED_REGS_ARE_ABSOLUTE == 0), all of the saved register values
   are recorded as offsets from the frame base, whatever it may be.  When
   we go to fetch a saved register value we call 
   x86_finalize_saved_reg_locations() which uses actual register values
   to set the FRAME_BASE, converts all the saved regs to proper CORE_ADDRs 
   and sets SAVED_REGS_ARE_ABSOLUTE to 1.

   In the above instruction sequence, if we've executed all the instructions,
   the relative offsets from FRAME_BASE look like this:
     EIP is saved at offset 0 (by definition)
     EBP is saved at offset 4
     EBX is saved at offset 8
     ESI is saved at offset 12
     EDI is saved at offset 16 (EBP - 12 aka FRAME_BASE - 16)
     ESP is computed as FRAME_BASE + 4
*/

struct x86_frame_cache
{
  CORE_ADDR frame_base;
  int sp_offset;        /* Record any stack pointer changes seen so far */
  CORE_ADDR func_start_addr;

 /* I only record these next two for debugging purposes so I can
    figure out what function the cache was analyzing after the fact
    when debugging.  */
  CORE_ADDR scanned_limit; 
  CORE_ADDR pc;

  /* Indicate whether this function has set up the EBP as the frame pointer
     or not.
     (The "not" cases would be a -fomit-frame-pointer function that only
      uses ESP and a function where we have yet to execute the insns that
      set up the EBP.)  */
  int ebp_is_frame_pointer;

  /* Saved registers.
     A value of INVALID_ADDRESS indicates that a given reg is uninitialized.
     Until SAVED_REGS_ARE_ABSOLUTE is set to 1, the values of the SAVED_REGS
     array are relative as the comment up above describes.  */
  CORE_ADDR *saved_regs;
  CORE_ADDR saved_sp;
  int saved_regs_are_absolute;

  int wordsize;        /* 4 or 8 */
  int num_savedregs;   /* number of saved registers */
  int eip_regnum;
  int ebp_regnum;
  int esp_regnum;

  enum prologue_scan_state prologue_scan_status;

  int (*volatile_reg_p)(int);
  int (*argument_reg_p)(int);
  int (*machine_regno_to_gdb_regno)(int);
};

void x86_initialize_frame_cache (struct x86_frame_cache *cache, int wordsize);
struct x86_frame_cache *x86_alloc_frame_cache (int wordsize);

CORE_ADDR x86_analyze_prologue (CORE_ADDR func_start_addr, CORE_ADDR limit, struct x86_frame_cache *cache);

void x86_finalize_saved_reg_locations (struct frame_info *next_frame, struct x86_frame_cache *cache);

struct x86_frame_cache *x86_frame_cache (struct frame_info *next_frame, void **this_cache, int wordsize);

void x86_frame_this_id (struct frame_info *next_frame, void **this_cache, struct frame_id *this_id);

void x86_frame_prev_register (struct frame_info *next_frame, void **this_cache, int regnum, enum opt_state *optimizedp, enum lval_type *lvalp, CORE_ADDR *addrp, int *realnump, gdb_byte *valuep);


#endif /* X86_SHARED_TDEP_H */
