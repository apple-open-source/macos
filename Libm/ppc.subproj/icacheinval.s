; sys_icache_invalidate - invalidate instruction cache for an address range.
;       specific to the Power PC and Mac OS X (due to use of
;        __cpu_capabilities).

; The only reason to write this much assembler for this simple job is that
; some revisions of the Power PC processor had bugs in this area that require
; precise ordering and timing of the instructions that affect the cache.
; This isn't in the processor manuals, it's only in the errata sheets.

/* need this to write code that will assemble as dynamic or static */
#include <architecture/ppc/asm_help.h>
#undef  PICIFY_REG
#define PICIFY_REG r6

; Default to 32 bytes per cache line for the cases where  _cpu_capabilities
; is not yet set up.  This gives slower, but correct behaviour, even on
; processors with a different cache-line size.

        .text
        .align 4, 0x0

/* sys_icache_invalidate(char *start, long len) */
        .globl  _sys_icache_invalidate
_sys_icache_invalidate:
        mr.     r4, r4
        beqlr   cr0             ; if (length == 0) return

; we trash r5 - r9, but apparently these are scratch registers

; for this algorithm, we need to know the cache line size.
; get the cpu capabilities info, mask it off & shift.
        EXTERN_TO_REG(r7, __cpu_capabilities)
; cache_line_size = (_cpu_capabilities&(kCache32|kCache64|kCache128))<<3
        rlwinm. r8, r7, 3, 24, 26
        bne+    cr0, 2f         ; if (cache_line_size == 0)
        li      r8, 32          ;       cache_line_size = 32;
2:      neg     r9, r8          ; r9 = cache_line_mask = ~(cache_line_size - 1)
        add     r5, r3, r4      ; r5 = last_cache_line address=start+length-1
        addi    r5, r5, -1
; r3 = first_cache_line address, rounded down to start of line
        and     r3, r3, r9
        mr      r6, r3          ; save start addr.

; loop over address range, flushing data cache
3:      dcbf    0, r6           ; flush data cache for address in r6
        add     r6, r6, r8      ; move to next cache line
        cmplw   r6, r5          ; flush all lines <= addr. in r5
        ble+    3b              ; if more cache lines left, repeat

        sync                    ; wait until all bus operations have finished

; loop over the same range, this time invalidating the instruction cache
4:      icbi    0, r3           ; invalidate instruction cache line for r3
        add     r3, r3, r8      ; move to next cache line
        cmplw   r3, r5          ; invalidate all lines <= addr. in r5
        ble+    4b              ; if more cache lines left, repeat

        sync                    ; wait until all bus operations have finished
        isync                   ; toss any speculatively executed instructions
        blr
