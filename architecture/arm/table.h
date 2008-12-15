/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
 */

#include <architecture/arm/desc.h>
#include <architecture/arm/tss.h>

/*
 * A totally generic descriptor
 * table entry.
 */

typedef union dt_entry {
    code_desc_t		code;
    data_desc_t		data;
    ldt_desc_t		ldt;
    tss_desc_t		task_state;
    call_gate_t		call_gate;
    trap_gate_t		trap_gate;
    intr_gate_t		intr_gate;
    task_gate_t		task_gate;
} dt_entry_t;

#define DESC_TBL_MAX	8192

/*
 * Global descriptor table.
 */

typedef union gdt_entry {
    code_desc_t		code;
    data_desc_t		data;
    ldt_desc_t		ldt;
    call_gate_t		call_gate;
    task_gate_t		task_gate;
    tss_desc_t		task_state;
} gdt_entry_t;

typedef gdt_entry_t	gdt_t;

/*
 * Interrupt descriptor table.
 */

typedef union idt_entry {
    trap_gate_t		trap_gate;
    intr_gate_t		intr_gate;
    task_gate_t		task_gate;
} idt_entry_t;

typedef idt_entry_t	idt_t;

/*
 * Local descriptor table.
 */

typedef union ldt_entry {
    code_desc_t		code;
    data_desc_t		data;
    call_gate_t		call_gate;
    task_gate_t		task_gate;
} ldt_entry_t;

typedef ldt_entry_t	ldt_t;
