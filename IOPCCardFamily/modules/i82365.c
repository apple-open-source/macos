/*======================================================================

    Device driver for Intel 82365 and compatible PC Card controllers,
    and Yenta-compatible PCI-to-CardBus controllers.

    i82365.c 1.326 2000/10/02 20:27:49

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Contributor:  Apple Computer, Inc.  Portions © 2000 Apple Computer, 
    Inc. All rights reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.

======================================================================*/

#include <IOKit/pccard/config.h>
#include <IOKit/pccard/k_compat.h>

#ifdef __LINUX__
#include <linux/module.h>
#include <linux/init.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/malloc.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/segment.h>
#include <asm/system.h>
#endif

#define IN_CARD_SERVICES
#include <IOKit/pccard/version.h>
#include <IOKit/pccard/cs_types.h>
#include <IOKit/pccard/ss.h>
#include <IOKit/pccard/cs.h>

/* ISA-bus controllers */
#include "i82365.h"
#include "cirrus.h"
#include "vg468.h"
#include "ricoh.h"
#include "o2micro.h"

/* PCI-bus controllers */
#include "yenta.h"
#include "ti113x.h"
#include "smc34c90.h"
#include "topic.h"

#ifdef PCMCIA_DEBUG
#ifdef __MACOSX__
#define pc_debug i82365_debug
int pc_debug = PCMCIA_DEBUG;
#else
static int pc_debug = PCMCIA_DEBUG;
#endif
MODULE_PARM(pc_debug, "i");
#define DEBUG(n, args...) if (pc_debug>(n)) printk(KERN_DEBUG args)
static const char *version =
"i82365.c 1.326 2000/10/02 20:27:49 (David Hinds)";
#else
#define DEBUG(n, args...) do { } while (0)
#endif

#ifdef __BEOS__
typedef int32 irq_ret_t;
#define _request_irq(i, h, f, n) \
    install_io_interrupt_handler(i, h, irq_list+i, 0)
#define _free_irq(i, h) remove_io_interrupt_handler(i, h, irq_list+i)
#define _check_irq(i, f) check_irq(i)
static cs_socket_module_info *cs;
static isa_module_info *isa;
static pci_module_info *pci;
#define RSRC_MGR cs->
#define register_ss_entry	cs->_register_ss_entry
#define unregister_ss_entry	cs->_unregister_ss_entry
#define add_timer		cs->_add_timer
#define del_timer		cs->_del_timer
#endif

#ifdef __LINUX__  //macosx
typedef void irq_ret_t;
#define _request_irq(i, h, f, n) request_irq(i, h, f, n, socket)
#define _free_irq(i, h) free_irq(i, socket)
#endif

MODULE_AUTHOR("David Hinds <dahinds@users.sourceforge.net>");
MODULE_DESCRIPTION("Intel ExCA/Yenta PCMCIA socket driver");

/*====================================================================*/

/* Parameters that can be set with 'insmod' */

#define INT_MODULE_PARM(n, v) static int n = v; MODULE_PARM(n, "i")

/* General options */
INT_MODULE_PARM(poll_interval, 0);	/* in ticks, 0 means never */
INT_MODULE_PARM(cycle_time, 120);	/* in ns, 120 ns = 8.33 MHz */
#ifndef __MACOSX__
INT_MODULE_PARM(do_scan, 1);		/* Probe free interrupts? */
#endif

/* Cirrus options */
INT_MODULE_PARM(has_dma, -1);
INT_MODULE_PARM(has_led, -1);
INT_MODULE_PARM(has_ring, -1);
INT_MODULE_PARM(dynamic_mode, 0);
INT_MODULE_PARM(freq_bypass, -1);
INT_MODULE_PARM(setup_time, -1);
INT_MODULE_PARM(cmd_time, -1);
INT_MODULE_PARM(recov_time, -1);

#ifdef CONFIG_ISA
INT_MODULE_PARM(i365_base, 0x3e0);	/* IO address for probes */
INT_MODULE_PARM(extra_sockets, 0);	/* Probe at i365_base+2? */
INT_MODULE_PARM(ignore, -1);		/* Ignore this socket # */
INT_MODULE_PARM(cs_irq, 0);		/* card status irq */
INT_MODULE_PARM(irq_mask, 0xffff);	/* bit map of irq's to use */
static int irq_list[16] = { -1 };
MODULE_PARM(irq_list, "1-16i");
INT_MODULE_PARM(async_clock, -1);	/* Vadem specific */
INT_MODULE_PARM(cable_mode, -1);
INT_MODULE_PARM(wakeup, 0);		/* Cirrus specific */
#endif

#ifdef CONFIG_PCI
static int pci_irq_list[8] = { 0 };	/* PCI interrupt assignments */
MODULE_PARM(pci_irq_list, "1-8i");
#ifndef __MACOSX__
INT_MODULE_PARM(do_pci_probe, 1);	/* Scan for PCI bridges? */
#endif
INT_MODULE_PARM(fast_pci, -1);
INT_MODULE_PARM(cb_write_post, -1);
#ifdef __MACOSX__
INT_MODULE_PARM(zv_mode, 1);		/* MACOSX zoom video mode */
INT_MODULE_PARM(irq_mode, 0);		/* MACOSX pci, enable INTA */
#else
INT_MODULE_PARM(irq_mode, -1);		/* Override BIOS routing? */
#endif
INT_MODULE_PARM(hold_time, -1);		/* Ricoh specific */
INT_MODULE_PARM(p2cclk, -1);		/* TI specific */
#endif

#if defined(CONFIG_ISA) && defined(CONFIG_PCI)
INT_MODULE_PARM(pci_csc, 1);		/* PCI card status irqs? */
INT_MODULE_PARM(pci_int, 1);		/* PCI IO card irqs? */
#elif defined(CONFIG_ISA) && !defined(CONFIG_PCI)
#define pci_csc		0
#define pci_int		0
#elif !defined(CONFIG_ISA) && defined(CONFIG_PCI)
#ifdef __MACOSX__
#define pci_csc		1
#else
#define pci_csc		0
#endif
#define pci_int		1		/* We must use PCI irq's */
#else
#error "No bus architectures defined!"
#endif

/*====================================================================*/

typedef struct socket_info_t {
    u_short		type, flags;
    socket_cap_t	cap;
#ifndef __MACOSX__
    ioaddr_t		ioaddr;
    u_short		psock;
#endif
    u_char		cs_irq, intr;
    void		(*handler)(void *info, u_int events);
    void		*info;
#ifdef __MACOSX__
    u_int		events;
#endif
#ifdef HAS_PROC_BUS
    struct proc_dir_entry *proc;
#endif
    u_char		pci_irq_code;
#ifdef CONFIG_PCI
    u_short		vendor, device;
#ifdef __MACOSX__
    u_char		revision;
#else
    u_char		revision, bus, devfn;
#endif
    u_short		bcr;
    u_char		pci_lat, cb_lat, sub_bus, cache;
    u_int		cb_phys;
    char		*cb_virt;
#endif
    union {
	cirrus_state_t		cirrus;
	vg46x_state_t		vg46x;
	o2micro_state_t		o2micro;
	ti113x_state_t		ti113x;
	ricoh_state_t		ricoh;
	topic_state_t		topic;
    } state;
} socket_info_t;

/* Where we keep track of our sockets... */
static int sockets = 0;
static socket_info_t socket[8] = {
    { 0, }, /* ... */
};

#ifdef CONFIG_ISA
static int grab_irq;
#ifdef USE_SPIN_LOCKS
static spinlock_t isa_lock = SPIN_LOCK_UNLOCKED;
#endif
#define ISA_LOCK(s, f) \
    if (!((s)->flags & IS_CARDBUS)) spin_lock_irqsave(&isa_lock, f)
#define ISA_UNLOCK(n, f) \
    if (!((s)->flags & IS_CARDBUS)) spin_unlock_irqrestore(&isa_lock, f)
#else
#define ISA_LOCK(n, f) do { } while (0)
#define ISA_UNLOCK(n, f) do { } while (0)
#endif

static void pcic_interrupt_wrapper(u_long data);
static struct timer_list poll_timer = {
    function: pcic_interrupt_wrapper
};

#define flip(v,b,f) (v = ((f)<0) ? v : ((f) ? ((v)|(b)) : ((v)&(~b))))

/*====================================================================*/

/* Some PCI shortcuts */

#ifdef CONFIG_PCI
#ifdef __MACOSX__
#define pci_readb(s, r, v)	IOPCCardReadConfigByte(s->cap.bridge_nub, r, v)
#define pci_writeb(s, r, v)	IOPCCardWriteConfigByte(s->cap.bridge_nub, r, v)
#define pci_readw(s, r, v)	IOPCCardReadConfigWord(s->cap.bridge_nub, r, v)
#define pci_writew(s, r, v)	IOPCCardWriteConfigWord(s->cap.bridge_nub, r, v)
#define pci_readl(s, r, v)	IOPCCardReadConfigLong(s->cap.bridge_nub, r, v)
#define pci_writel(s, r, v)	IOPCCardWriteConfigLong(s->cap.bridge_nub, r, v)
#else
static int pci_readb(socket_info_t *s, int r, u_char *v)
{ return pcibios_read_config_byte(s->bus, s->devfn, r, v); }
static int pci_writeb(socket_info_t *s, int r, u_char v)
{ return pcibios_write_config_byte(s->bus, s->devfn, r, v); }
static int pci_readw(socket_info_t *s, int r, u_short *v)
{ return pcibios_read_config_word(s->bus, s->devfn, r, v); }
static int pci_writew(socket_info_t *s, int r, u_short v)
{ return pcibios_write_config_word(s->bus, s->devfn, r, v); }
static int pci_readl(socket_info_t *s, int r, u_int *v)
{ return pcibios_read_config_dword(s->bus, s->devfn, r, v); }
static int pci_writel(socket_info_t *s, int r, u_int v)
{ return pcibios_write_config_dword(s->bus, s->devfn, r, v); }
#endif
#endif

#define cb_readb(s, r)		readb((s)->cb_virt + (r))
#define cb_readl(s, r)		readl((s)->cb_virt + (r))
#define cb_writeb(s, r, v)	writeb(v, (s)->cb_virt + (r))
#define cb_writel(s, r, v)	writel(v, (s)->cb_virt + (r))

/*====================================================================*/

/* These definitions must match the pcic table! */
typedef enum pcic_id {
#ifdef CONFIG_ISA
    IS_I82365A, IS_I82365B, IS_I82365DF, IS_IBM, IS_RF5Cx96,
    IS_VLSI, IS_VG468, IS_VG469, IS_PD6710, IS_PD672X, IS_VT83C469,
#endif
#ifdef CONFIG_PCI
    IS_I82092AA, IS_OM82C092G, CIRRUS_PCIC_ID, O2MICRO_PCIC_ID,
    RICOH_PCIC_ID, SMC_PCIC_ID, TI_PCIC_ID, TOPIC_PCIC_ID,
    IS_UNK_PCI, IS_UNK_CARDBUS
#endif
} pcic_id;

/* Flags for classifying groups of controllers */
#define IS_VADEM	0x0001
#define IS_CIRRUS	0x0002
#define IS_TI		0x0004
#define IS_O2MICRO	0x0008
#define IS_TOPIC	0x0020
#define IS_RICOH	0x0040
#define IS_UNKNOWN	0x0400
#define IS_VG_PWR	0x0800
#define IS_DF_PWR	0x1000
#define IS_PCI		0x2000
#define IS_CARDBUS	0x4000
#define IS_ALIVE	0x8000

typedef struct pcic_t {
    char		*name;
    u_short		flags;
#ifdef CONFIG_PCI
    u_short		vendor, device;
#endif
} pcic_t;

#define ID(a,b) PCI_VENDOR_ID_##a,PCI_DEVICE_ID_##a##_##b

static pcic_t pcic[] = {
#ifdef CONFIG_ISA
    { "Intel i82365sl A step", 0 },
    { "Intel i82365sl B step", 0 },
    { "Intel i82365sl DF", IS_DF_PWR },
    { "IBM Clone", 0 },
    { "Ricoh RF5C296/396", 0 },
    { "VLSI 82C146", 0 },
    { "Vadem VG-468", IS_VADEM },
    { "Vadem VG-469", IS_VADEM|IS_VG_PWR },
    { "Cirrus PD6710", IS_CIRRUS },
    { "Cirrus PD672x", IS_CIRRUS },
    { "VIA VT83C469", IS_CIRRUS },
#endif
#ifdef CONFIG_PCI
    { "Intel 82092AA", IS_PCI, ID(INTEL, 82092AA_0) },
    { "Omega Micro 82C092G", IS_PCI, ID(OMEGA, 82C092G) },
    CIRRUS_PCIC_INFO, O2MICRO_PCIC_INFO, RICOH_PCIC_INFO,
    SMC_PCIC_INFO, TI_PCIC_INFO, TOPIC_PCIC_INFO,
    { "Unknown", IS_PCI|IS_UNKNOWN, 0, 0 },
    { "Unknown", IS_CARDBUS|IS_UNKNOWN, 0, 0 }
#endif
};

#define PCIC_COUNT	(sizeof(pcic)/sizeof(pcic_t))

/*====================================================================*/

static u_char i365_get(socket_info_t *s, u_short reg)
{
#ifdef __MACOSX__
    return cb_readb(s, 0x0800 + reg);
#else
#ifdef CONFIG_PCI
    if (s->cb_virt)
	return cb_readb(s, 0x0800 + reg);
#endif
    outb(I365_REG(s->psock, reg), s->ioaddr);
    return inb(s->ioaddr+1);
#endif
}

static void i365_set(socket_info_t *s, u_short reg, u_char data)
{
#ifdef __MACOSX__
    cb_writeb(s, 0x0800 + reg, data);
#else
#ifdef CONFIG_PCI
    if (s->cb_virt) {
	cb_writeb(s, 0x0800 + reg, data);
	return;
    }
#endif
    outb(I365_REG(s->psock, reg), s->ioaddr);
    outb(data, s->ioaddr+1);
#endif
}

static void i365_bset(socket_info_t *s, u_short reg, u_char mask)
{
    i365_set(s, reg, i365_get(s, reg) | mask);
}

static void i365_bclr(socket_info_t *s, u_short reg, u_char mask)
{
    i365_set(s, reg, i365_get(s, reg) & ~mask);
}

static void i365_bflip(socket_info_t *s, u_short reg, u_char mask, int b)
{
    u_char d = i365_get(s, reg);
    i365_set(s, reg, (b) ? (d | mask) : (d & ~mask));
}

static u_short i365_get_pair(socket_info_t *s, u_short reg)
{
    return (i365_get(s, reg) + (i365_get(s, reg+1) << 8));
}

static void i365_set_pair(socket_info_t *s, u_short reg, u_short data)
{
    i365_set(s, reg, data & 0xff);
    i365_set(s, reg+1, data >> 8);
}

/*======================================================================

    Code to save and restore global state information for Cirrus
    PD67xx controllers, and to set and report global configuration
    options.

    The VIA controllers also use these routines, as they are mostly
    Cirrus lookalikes, without the timing registers.
    
======================================================================*/

#ifdef CONFIG_PCI

static int __init get_pci_irq(socket_info_t *s)
{
    u8 irq;
#ifdef __MACOSX__
    pci_readb(s, PCI_INTERRUPT_LINE, &irq);
#endif
#ifdef __BEOS__
    pci_readb(s, PCI_INTERRUPT_LINE, &irq);
#endif
#ifdef __LINUX__ //macosx
    irq = pci_find_slot(s->bus, s->devfn)->irq;
#endif
    if ((irq == 0) && (pci_csc || pci_int))
	irq = pci_irq_list[s - socket];
    if (irq >= NR_IRQS) irq = 0;
    s->cap.pci_irq = irq;
    return irq;
}

#endif

static void __init cirrus_get_state(socket_info_t *s)
{
    cirrus_state_t *p = &s->state.cirrus;
    int i;

    p->misc1 = i365_get(s, PD67_MISC_CTL_1);
    p->misc1 &= (PD67_MC1_MEDIA_ENA | PD67_MC1_INPACK_ENA);
    p->misc2 = i365_get(s, PD67_MISC_CTL_2);
    if (s->flags & IS_PCI)
	p->ectl1 = pd67_ext_get(s, PD67_EXT_CTL_1);
    for (i = 0; i < 6; i++)
	p->timer[i] = i365_get(s, PD67_TIME_SETUP(0)+i);
}

static void cirrus_set_state(socket_info_t *s)
{
    cirrus_state_t *p = &s->state.cirrus;
    u_char misc;
    int i;

    misc = i365_get(s, PD67_MISC_CTL_2);
    i365_set(s, PD67_MISC_CTL_2, p->misc2);
    if (misc & PD67_MC2_SUSPEND) mdelay(50);
    misc = i365_get(s, PD67_MISC_CTL_1);
    misc &= ~(PD67_MC1_MEDIA_ENA | PD67_MC1_INPACK_ENA);
    i365_set(s, PD67_MISC_CTL_1, misc | p->misc1);
    if (s->flags & IS_PCI)
	pd67_ext_set(s, PD67_EXT_CTL_1, p->ectl1);
    for (i = 0; i < 6; i++)
	i365_set(s, PD67_TIME_SETUP(0)+i, p->timer[i]);
}

#ifdef CONFIG_PCI
static int cirrus_set_irq_mode(socket_info_t *s, int pcsc, int pint)
{
    flip(s->bcr, PD6832_BCR_MGMT_IRQ_ENA, !pcsc);
    return 0;
}
#endif

static u_int __init cirrus_set_opts(socket_info_t *s, char *buf)
{
    cirrus_state_t *p = &s->state.cirrus;
    u_int mask = 0xffff;

    p->misc1 |= PD67_MC1_SPKR_ENA;
    if (has_ring == -1) has_ring = 1;
    flip(p->misc2, PD67_MC2_IRQ15_RI, has_ring);
    flip(p->misc2, PD67_MC2_DYNAMIC_MODE, dynamic_mode);
    if (p->misc2 & PD67_MC2_IRQ15_RI)
	strcat(buf, " [ring]");
    if (p->misc2 & PD67_MC2_DYNAMIC_MODE)
	strcat(buf, " [dyn mode]");
    if (p->misc1 & PD67_MC1_INPACK_ENA)
	strcat(buf, " [inpack]");
    if (!(s->flags & (IS_PCI|IS_CARDBUS))) {
	if (p->misc2 & PD67_MC2_IRQ15_RI)
	    mask &= ~0x8000;
	if (has_led > 0) {
	    strcat(buf, " [led]");
	    mask &= ~0x1000;
	}
	if (has_dma > 0) {
	    strcat(buf, " [dma]");
	    mask &= ~0x0600;
	flip(p->misc2, PD67_MC2_FREQ_BYPASS, freq_bypass);
	if (p->misc2 & PD67_MC2_FREQ_BYPASS)
	    strcat(buf, " [freq bypass]");
	}
#ifdef CONFIG_PCI
    } else {
	p->misc1 &= ~PD67_MC1_MEDIA_ENA;
	p->misc1 &= ~(PD67_MC1_PULSE_MGMT | PD67_MC1_PULSE_IRQ);
	p->ectl1 &= ~(PD67_EC1_INV_MGMT_IRQ | PD67_EC1_INV_CARD_IRQ);
	flip(p->misc2, PD67_MC2_FAST_PCI, fast_pci);
	if (p->misc2 & PD67_MC2_IRQ15_RI)
	    mask &= (s->type == IS_PD6730) ? ~0x0400 : ~0x8000;
	if ((s->flags & IS_PCI) && (irq_mode == 1) && get_pci_irq(s)) {
	    /* Configure PD6729 bridge for PCI interrupts */
	    p->ectl1 |= PD67_EC1_INV_MGMT_IRQ | PD67_EC1_INV_CARD_IRQ;
	    s->pci_irq_code = 3; /* PCI INTA = "irq 3" */
	    buf += strlen(buf);
	    sprintf(buf, " [pci irq %d]", s->cap.pci_irq);
	    mask = 0;
	}
#endif
    }
#ifdef CONFIG_ISA
    if (s->type != IS_VT83C469)
#endif
    {
	if (setup_time >= 0)
	    p->timer[0] = p->timer[3] = setup_time;
	if (cmd_time > 0) {
	    p->timer[1] = cmd_time;
	    p->timer[4] = cmd_time*2+4;
	}
	if (p->timer[1] == 0) {
	    p->timer[1] = 6; p->timer[4] = 16;
	    if (p->timer[0] == 0)
		p->timer[0] = p->timer[3] = 1;
	}
	if (recov_time >= 0)
	    p->timer[2] = p->timer[5] = recov_time;
	buf += strlen(buf);
	sprintf(buf, " [%d/%d/%d] [%d/%d/%d]", p->timer[0], p->timer[1],
		p->timer[2], p->timer[3], p->timer[4], p->timer[5]);
    }
    return mask;
}

/*======================================================================

    Code to save and restore global state information for Vadem VG468
    and VG469 controllers, and to set and report global configuration
    options.
    
======================================================================*/

#ifdef CONFIG_ISA

static void __init vg46x_get_state(socket_info_t *s)
{
    vg46x_state_t *p = &s->state.vg46x;
    p->ctl = i365_get(s, VG468_CTL);
    if (s->type == IS_VG469)
	p->ema = i365_get(s, VG469_EXT_MODE);
}

static void vg46x_set_state(socket_info_t *s)
{
    vg46x_state_t *p = &s->state.vg46x;
    i365_set(s, VG468_CTL, p->ctl);
    if (s->type == IS_VG469)
	i365_set(s, VG469_EXT_MODE, p->ema);
}

static u_int __init vg46x_set_opts(socket_info_t *s, char *buf)
{
    vg46x_state_t *p = &s->state.vg46x;
    
    flip(p->ctl, VG468_CTL_ASYNC, async_clock);
    flip(p->ema, VG469_MODE_CABLE, cable_mode);
    if (p->ctl & VG468_CTL_ASYNC)
	strcat(buf, " [async]");
    if (p->ctl & VG468_CTL_INPACK)
	strcat(buf, " [inpack]");
    if (s->type == IS_VG469) {
	u_char vsel = i365_get(s, VG469_VSELECT);
	if (vsel & VG469_VSEL_EXT_STAT) {
	    strcat(buf, " [ext mode]");
	    if (vsel & VG469_VSEL_EXT_BUS)
		strcat(buf, " [isa buf]");
	}
	if (p->ema & VG469_MODE_CABLE)
	    strcat(buf, " [cable]");
	if (p->ema & VG469_MODE_COMPAT)
	    strcat(buf, " [c step]");
    }
    return 0xffff;
}

#endif

/*======================================================================

    Code to save and restore global state information for TI 1130 and
    TI 1131 controllers, and to set and report global configuration
    options.
    
======================================================================*/

#ifdef CONFIG_PCI

static void __init ti113x_get_state(socket_info_t *s)
{
    ti113x_state_t *p = &s->state.ti113x;
    pci_readl(s, TI113X_SYSTEM_CONTROL, &p->sysctl);
    pci_readb(s, TI113X_CARD_CONTROL, &p->cardctl);
    pci_readb(s, TI113X_DEVICE_CONTROL, &p->devctl);
    pci_readb(s, TI1250_DIAGNOSTIC, &p->diag);
    pci_readl(s, TI12XX_IRQMUX, &p->irqmux);
}

static void ti113x_set_state(socket_info_t *s)
{
    ti113x_state_t *p = &s->state.ti113x;
    pci_writel(s, TI113X_SYSTEM_CONTROL, p->sysctl);
    pci_writeb(s, TI113X_CARD_CONTROL, p->cardctl);
    pci_writeb(s, TI113X_DEVICE_CONTROL, p->devctl);
    pci_writeb(s, TI1250_MULTIMEDIA_CTL, 0);
    pci_writeb(s, TI1250_DIAGNOSTIC, p->diag);
    pci_writel(s, TI12XX_IRQMUX, p->irqmux);
    i365_set_pair(s, TI113X_IO_OFFSET(0), 0);
    i365_set_pair(s, TI113X_IO_OFFSET(1), 0);
}

static int ti113x_set_irq_mode(socket_info_t *s, int pcsc, int pint)
{
    ti113x_state_t *p = &s->state.ti113x;
    s->intr = (pcsc) ? I365_INTR_ENA : 0;
    if (s->type <= IS_TI1131) {
	p->cardctl &= ~(TI113X_CCR_PCI_IRQ_ENA |
			TI113X_CCR_PCI_IREQ | TI113X_CCR_PCI_CSC);
	if (pcsc)
	    p->cardctl |= TI113X_CCR_PCI_IRQ_ENA | TI113X_CCR_PCI_CSC;
	if (pint)
	    p->cardctl |= TI113X_CCR_PCI_IRQ_ENA | TI113X_CCR_PCI_IREQ;
    } else if (s->type == IS_TI1250A) {
	p->diag &= TI1250_DIAG_PCI_CSC | TI1250_DIAG_PCI_IREQ;
	if (pcsc)
	    p->diag |= TI1250_DIAG_PCI_CSC;
	if (pint)
	    p->diag |= TI1250_DIAG_PCI_IREQ;
    }
    return 0;
}

static u_int __init ti113x_set_opts(socket_info_t *s, char *buf)
{
    ti113x_state_t *p = &s->state.ti113x;
    u_int mask = 0xffff;
    int old = (s->type <= IS_TI1131);
    
    flip(p->cardctl, TI113X_CCR_RIENB, has_ring);
    p->cardctl &= ~TI113X_CCR_ZVENABLE;
    p->cardctl |= TI113X_CCR_SPKROUTEN;
    if (!old) flip(p->sysctl, TI122X_SCR_P2CCLK, p2cclk);
#ifdef __MACOSX__
    p->sysctl |= TI122X_SCR_MRBURSTUP | TI113X_SCR_KEEPCLK | TI113X_SCR_VCCPROT;
#endif
    switch (irq_mode) {
    case 0:
	p->devctl &= ~TI113X_DCR_IMODE_MASK;
	p->irqmux |= 0x02; /* minimal routing for INTA */
	break;
    case 1:
	p->devctl &= ~TI113X_DCR_IMODE_MASK;
	p->devctl |= TI113X_DCR_IMODE_ISA;
	break;
    case 2:
	p->devctl &= ~TI113X_DCR_IMODE_MASK;
	p->devctl |= TI113X_DCR_IMODE_SERIAL;
	break;
    case 3:
	p->devctl &= ~TI113X_DCR_IMODE_MASK;
	p->devctl |= TI12XX_DCR_IMODE_ALL_SERIAL;
	break;
    default:
	/* Feeble fallback: if PCI-only but no PCI irq, try ISA */
	if (((p->devctl & TI113X_DCR_IMODE_MASK) == 0) &&
	    (s->cap.pci_irq == 0))
	    p->devctl |= TI113X_DCR_IMODE_ISA;
    }
#ifdef __MACOSX__
    switch (zv_mode) {
    case 1:
	p->irqmux |= 0x70;	// enable zoom video
	break;
    }
#endif
    if (p->cardctl & TI113X_CCR_RIENB) {
	strcat(buf, " [ring]");
	if (old) mask &= ~0x8000;
    }
    if (old && (p->sysctl & TI113X_SCR_CLKRUN_ENA)) {
	if (p->sysctl & TI113X_SCR_CLKRUN_SEL) {
	    strcat(buf, " [clkrun irq 12]");
	    mask &= ~0x1000;
	} else {
	    strcat(buf, " [clkrun irq 10]");
	    mask &= ~0x0400;
	}
    }
    switch (p->devctl & TI113X_DCR_IMODE_MASK) {
    case TI12XX_DCR_IMODE_PCI_ONLY:
	strcat(buf, " [pci only]");
	mask = 0;
	break;
    case TI113X_DCR_IMODE_ISA:
	strcat(buf, " [isa irq]");
	if (old) mask &= ~0x0018;
	break;
    case TI113X_DCR_IMODE_SERIAL:
	strcat(buf, " [pci + serial irq]");
	mask = 0xffff;
	break;
    case TI12XX_DCR_IMODE_ALL_SERIAL:
	strcat(buf, " [serial pci & irq]");
	mask = 0xffff;
	break;
    }
    return mask;
}

#endif

/*======================================================================

    Code to save and restore global state information for the Ricoh
    RL5C4XX controllers, and to set and report global configuration
    options.

    The interrupt test doesn't seem to be reliable with Ricoh
    bridges.  It seems to depend on what type of card is in the
    socket, and on the history of that socket, in some way that
    doesn't show up in the current socket state.

======================================================================*/

#ifdef CONFIG_PCI

static void __init ricoh_get_state(socket_info_t *s)
{
    ricoh_state_t *p = &s->state.ricoh;
    pci_readw(s, RL5C4XX_CONFIG, &p->config);
    pci_readw(s, RL5C4XX_MISC, &p->misc);
    pci_readw(s, RL5C4XX_16BIT_CTL, &p->ctl);
    pci_readw(s, RL5C4XX_16BIT_IO_0, &p->io);
    pci_readw(s, RL5C4XX_16BIT_MEM_0, &p->mem);
}

static void ricoh_set_state(socket_info_t *s)
{
    ricoh_state_t *p = &s->state.ricoh;
    pci_writew(s, RL5C4XX_CONFIG, p->config);
    pci_writew(s, RL5C4XX_MISC, p->misc);
    pci_writew(s, RL5C4XX_16BIT_CTL, p->ctl);
    pci_writew(s, RL5C4XX_16BIT_IO_0, p->io);
    pci_writew(s, RL5C4XX_16BIT_MEM_0, p->mem);
}

static u_int __init ricoh_set_opts(socket_info_t *s, char *buf)
{
    ricoh_state_t *p = &s->state.ricoh;
    u_int mask = 0xffff;
    int old = (s->type < IS_RL5C475);

    p->ctl = RL5C4XX_16CTL_IO_TIMING | RL5C4XX_16CTL_MEM_TIMING;
    if (old)
	p->ctl |= RL5C46X_16CTL_LEVEL_1 | RL5C46X_16CTL_LEVEL_2;
    else
	p->config |= RL5C4XX_CONFIG_PREFETCH;

    if (setup_time >= 0) {
	p->io = (p->io & ~RL5C4XX_SETUP_MASK) +
	    ((setup_time+1) << RL5C4XX_SETUP_SHIFT);
	p->mem = (p->mem & ~RL5C4XX_SETUP_MASK) +
	    (setup_time << RL5C4XX_SETUP_SHIFT);
    }
    if (cmd_time >= 0) {
	p->io = (p->io & ~RL5C4XX_CMD_MASK) +
	    (cmd_time << RL5C4XX_CMD_SHIFT);
	p->mem = (p->mem & ~RL5C4XX_CMD_MASK) +
	    (cmd_time << RL5C4XX_CMD_SHIFT);
    }
    if (hold_time >= 0) {
	p->io = (p->io & ~RL5C4XX_HOLD_MASK) +
	    (hold_time << RL5C4XX_HOLD_SHIFT);
	p->mem = (p->mem & ~RL5C4XX_HOLD_MASK) +
	    (hold_time << RL5C4XX_HOLD_SHIFT);
    }
    if (irq_mode == 0) {
	mask = 0;
	sprintf(buf, " [pci only]");
	buf += strlen(buf);
    } else if (!old) {
	switch (irq_mode) {
	case 1:
	    p->misc &= ~RL5C47X_MISC_SRIRQ_ENA; break;
	case 2:
	    p->misc |= RL5C47X_MISC_SRIRQ_ENA; break;
	}
	if (p->misc & RL5C47X_MISC_SRIRQ_ENA)
	    sprintf(buf, " [serial irq]");
	else
	    sprintf(buf, " [isa irq]");
	buf += strlen(buf);
    }
    sprintf(buf, " [io %d/%d/%d] [mem %d/%d/%d]",
	    (p->io & RL5C4XX_SETUP_MASK) >> RL5C4XX_SETUP_SHIFT,
	    (p->io & RL5C4XX_CMD_MASK) >> RL5C4XX_CMD_SHIFT,
	    (p->io & RL5C4XX_HOLD_MASK) >> RL5C4XX_HOLD_SHIFT,
	    (p->mem & RL5C4XX_SETUP_MASK) >> RL5C4XX_SETUP_SHIFT,
	    (p->mem & RL5C4XX_CMD_MASK) >> RL5C4XX_CMD_SHIFT,
	    (p->mem & RL5C4XX_HOLD_MASK) >> RL5C4XX_HOLD_SHIFT);
    return mask;
}

#endif

/*======================================================================

    Code to save and restore global state information for O2Micro
    controllers, and to set and report global configuration options.
    
======================================================================*/

#ifdef CONFIG_PCI

static void __init o2micro_get_state(socket_info_t *s)
{
    o2micro_state_t *p = &s->state.o2micro;
    if ((s->revision == 0x34) || (s->revision == 0x62) ||
	(s->type == IS_OZ6812)) {
	p->mode_a = i365_get(s, O2_MODE_A_2);
	p->mode_b = i365_get(s, O2_MODE_B_2);
    } else {
	p->mode_a = i365_get(s, O2_MODE_A);
	p->mode_b = i365_get(s, O2_MODE_B);
    }
    p->mode_c = i365_get(s, O2_MODE_C);
    p->mode_d = i365_get(s, O2_MODE_D);
    if (s->flags & IS_CARDBUS) {
	p->mhpg = i365_get(s, O2_MHPG_DMA);
	p->fifo = i365_get(s, O2_FIFO_ENA);
	p->mode_e = i365_get(s, O2_MODE_E);
    }
}

static void o2micro_set_state(socket_info_t *s)
{
    o2micro_state_t *p = &s->state.o2micro;
    if ((s->revision == 0x34) || (s->revision == 0x62) ||
	(s->type == IS_OZ6812)) {
	i365_set(s, O2_MODE_A_2, p->mode_a);
	i365_set(s, O2_MODE_B_2, p->mode_b);
    } else {
	i365_set(s, O2_MODE_A, p->mode_a);
	i365_set(s, O2_MODE_B, p->mode_b);
    }
    i365_set(s, O2_MODE_C, p->mode_c);
    i365_set(s, O2_MODE_D, p->mode_d);
    if (s->flags & IS_CARDBUS) {
	i365_set(s, O2_MHPG_DMA, p->mhpg);
	i365_set(s, O2_FIFO_ENA, p->fifo);
	i365_set(s, O2_MODE_E, p->mode_e);
    }
}

static u_int __init o2micro_set_opts(socket_info_t *s, char *buf)
{
    o2micro_state_t *p = &s->state.o2micro;
    u_int mask = 0xffff;

    p->mode_b = (p->mode_b & ~O2_MODE_B_IDENT) | O2_MODE_B_ID_CSTEP;
    flip(p->mode_b, O2_MODE_B_IRQ15_RI, has_ring);
    p->mode_c &= ~(O2_MODE_C_ZVIDEO | O2_MODE_C_DREQ_MASK);
    if (s->flags & IS_CARDBUS) {
	p->mode_d &= ~O2_MODE_D_W97_IRQ;
	p->mode_e &= ~O2_MODE_E_MHPG_DMA;
	p->mhpg = O2_MHPG_CINT_ENA | O2_MHPG_CSC_ENA;
	if (s->revision == 0x34)
	    p->mode_c = 0x20;
    } else {
	if (p->mode_b & O2_MODE_B_IRQ15_RI) mask &= ~0x8000;
    }
    if (p->mode_b & O2_MODE_B_IRQ15_RI)
	strcat(buf, " [ring]");
    if (irq_mode != -1)
	p->mode_d = irq_mode;
    if (p->mode_d & O2_MODE_D_ISA_IRQ) {
	strcat(buf, " [pci+isa]");
    } else {
	switch (p->mode_d & O2_MODE_D_IRQ_MODE) {
	case O2_MODE_D_IRQ_PCPCI:
	    strcat(buf, " [pc/pci]"); break;
	case O2_MODE_D_IRQ_PCIWAY:
	    strcat(buf, " [pci/way]"); break;
	case O2_MODE_D_IRQ_PCI:
	    strcat(buf, " [pci only]"); mask = 0; break;
	}
    }
    if (s->flags & IS_CARDBUS) {
	if (p->mode_d & O2_MODE_D_W97_IRQ)
	    strcat(buf, " [win97]");
    }
    return mask;
}

#endif

/*======================================================================

    Code to save and restore global state information for the Toshiba
    ToPIC 95 and 97 controllers, and to set and report global
    configuration options.
    
======================================================================*/

#ifdef CONFIG_PCI

static void __init topic_get_state(socket_info_t *s)
{
    topic_state_t *p = &s->state.topic;
    pci_readb(s, TOPIC_SLOT_CONTROL, &p->slot);
    pci_readb(s, TOPIC_CARD_CONTROL, &p->ccr);
    pci_readb(s, TOPIC_CARD_DETECT, &p->cdr);
    pci_readl(s, TOPIC_REGISTER_CONTROL, &p->rcr);
}

static void topic_set_state(socket_info_t *s)
{
    topic_state_t *p = &s->state.topic;
    pci_writeb(s, TOPIC_SLOT_CONTROL, p->slot);
    pci_writeb(s, TOPIC_CARD_CONTROL, p->ccr);
    pci_writeb(s, TOPIC_CARD_DETECT, p->cdr);
    pci_writel(s, TOPIC_REGISTER_CONTROL, p->rcr);
}

static u_int __init topic_set_opts(socket_info_t *s, char *buf)
{
    topic_state_t *p = &s->state.topic;

    p->slot |= TOPIC_SLOT_SLOTON|TOPIC_SLOT_SLOTEN|TOPIC_SLOT_ID_LOCK;
    p->cdr |= TOPIC_CDR_MODE_PC32;
    p->cdr &= ~(TOPIC_CDR_SW_DETECT);
    sprintf(buf, " [slot 0x%02x] [ccr 0x%02x] [cdr 0x%02x] [rcr 0x%02x]",
	    p->slot, p->ccr, p->cdr, p->rcr);
    return 0xffff;
}

#endif

/*======================================================================

    Routines to handle common CardBus options
    
======================================================================*/

/* Default settings for PCI command configuration register */
#define CMD_DFLT (PCI_COMMAND_IO|PCI_COMMAND_MEMORY| \
		  PCI_COMMAND_MASTER|PCI_COMMAND_WAIT)

#ifdef CONFIG_PCI

static void __init cb_get_state(socket_info_t *s)
{
    pci_readb(s, PCI_CACHE_LINE_SIZE, &s->cache);
    pci_readb(s, PCI_LATENCY_TIMER, &s->pci_lat);
    pci_readb(s, CB_LATENCY_TIMER, &s->cb_lat);
    pci_readb(s, CB_CARDBUS_BUS, &s->cap.cardbus);
    pci_readb(s, CB_SUBORD_BUS, &s->sub_bus);
    pci_readw(s, CB_BRIDGE_CONTROL, &s->bcr);
    get_pci_irq(s);
}

static void cb_set_state(socket_info_t *s)
{
#ifdef __LINUX__
    pci_set_power_state(pci_find_slot(s->bus, s->devfn), 0);
#endif
    pci_writel(s, CB_LEGACY_MODE_BASE, 0);
    pci_writel(s, PCI_BASE_ADDRESS_0, s->cb_phys);
    pci_writew(s, PCI_COMMAND, CMD_DFLT);
    pci_writeb(s, PCI_CACHE_LINE_SIZE, s->cache);
    pci_writeb(s, PCI_LATENCY_TIMER, s->pci_lat);
    pci_writeb(s, CB_LATENCY_TIMER, s->cb_lat);
    pci_writeb(s, CB_CARDBUS_BUS, s->cap.cardbus);
    pci_writeb(s, CB_SUBORD_BUS, s->sub_bus);
    pci_writew(s, CB_BRIDGE_CONTROL, s->bcr);
}

static int cb_get_irq_mode(socket_info_t *s)
{
    return (!(s->bcr & CB_BCR_ISA_IRQ));
}

static int cb_set_irq_mode(socket_info_t *s, int pcsc, int pint)
{
    flip(s->bcr, CB_BCR_ISA_IRQ, !(pint));
    if (s->flags & IS_CIRRUS)
	return cirrus_set_irq_mode(s, pcsc, pint);
    else if (s->flags & IS_TI)
	return ti113x_set_irq_mode(s, pcsc, pint);
    /* By default, assume that we can't do ISA status irqs */
    return (!pcsc);
}

static void __init cb_set_opts(socket_info_t *s, char *buf)
{
    s->bcr |= CB_BCR_WRITE_POST;
    /* some TI1130's seem to exhibit problems with write posting */
    if (((s->type == IS_TI1130) && (s->revision == 4) &&
	 (cb_write_post < 0)) || (cb_write_post == 0))
	s->bcr &= ~CB_BCR_WRITE_POST;
    if (s->cache == 0) s->cache = 8;
    if (s->pci_lat == 0) s->pci_lat = 0xa8;
    if (s->cb_lat == 0) s->cb_lat = 0xb0;
    if (s->cap.pci_irq == 0)
	strcat(buf, " [no pci irq]");
    else
	sprintf(buf, " [pci irq %d]", s->cap.pci_irq);
    buf += strlen(buf);
    if (!(s->flags & IS_TOPIC))
	s->cap.features |= SS_CAP_PAGE_REGS;
    sprintf(buf, " [lat %d/%d] [bus %d/%d]",
	    s->pci_lat, s->cb_lat, s->cap.cardbus, s->sub_bus);
}

#endif

/*======================================================================

    Power control for Cardbus controllers: used both for 16-bit and
    Cardbus cards.
    
======================================================================*/

#ifdef CONFIG_PCI

static void cb_get_power(socket_info_t *s, socket_state_t *state)
{
    u_int reg = cb_readl(s, CB_SOCKET_CONTROL);
    state->Vcc = state->Vpp = 0;
    switch (reg & CB_SC_VCC_MASK) {
    case CB_SC_VCC_3V:		state->Vcc = 33; break;
    case CB_SC_VCC_5V:		state->Vcc = 50; break;
    }
    switch (reg & CB_SC_VPP_MASK) {
    case CB_SC_VPP_3V:		state->Vpp = 33; break;
    case CB_SC_VPP_5V:		state->Vpp = 50; break;
    case CB_SC_VPP_12V:		state->Vpp = 120; break;
    }
}

static int cb_set_power(socket_info_t *s, socket_state_t *state)
{
    u_int reg = 0;
    /* restart card voltage detection if it seems appropriate */
    if ((state->Vcc == 0) && (state->Vpp == 0) &&
	!(cb_readl(s, CB_SOCKET_STATE) & CB_SS_VSENSE))
	cb_writel(s, CB_SOCKET_FORCE, CB_SF_CVSTEST);
    switch (state->Vcc) {
    case 0:		reg = 0; break;
    case 33:		reg = CB_SC_VCC_3V; break;
    case 50:		reg = CB_SC_VCC_5V; break;
    default:		return -EINVAL;
    }
    switch (state->Vpp) {
    case 0:		break;
    case 33:		reg |= CB_SC_VPP_3V; break;
    case 50:		reg |= CB_SC_VPP_5V; break;
    case 120:		reg |= CB_SC_VPP_12V; break;
    default:		return -EINVAL;
    }
    if (reg != cb_readl(s, CB_SOCKET_CONTROL))
	cb_writel(s, CB_SOCKET_CONTROL, reg);
    return 0;
}

#endif

/*======================================================================

    Generic routines to get and set controller options
    
======================================================================*/

static void __init get_bridge_state(socket_info_t *s)
{
    if (s->flags & IS_CIRRUS)
	cirrus_get_state(s);
#ifdef CONFIG_ISA
    else if (s->flags & IS_VADEM)
	vg46x_get_state(s);
#endif
#ifdef CONFIG_PCI
    else if (s->flags & IS_O2MICRO)
	o2micro_get_state(s);
    else if (s->flags & IS_TI)
	ti113x_get_state(s);
    else if (s->flags & IS_RICOH)
	ricoh_get_state(s);
    else if (s->flags & IS_TOPIC)
	topic_get_state(s);
    if (s->flags & IS_CARDBUS)
	cb_get_state(s);
#endif
}

static void set_bridge_state(socket_info_t *s)
{
#ifdef CONFIG_PCI
    if (s->flags & IS_CARDBUS)
	cb_set_state(s);
#endif
    if (s->flags & IS_CIRRUS) {
	cirrus_set_state(s);
    } else {
	i365_set(s, I365_GBLCTL, 0x00);
	i365_set(s, I365_GENCTL, 0x00);
	/* Trouble: changes timing of memory operations */
	/* i365_set(s, I365_ADDRWIN, I365_ADDR_MEMCS16); */
    }
    i365_bflip(s, I365_INTCTL, I365_INTR_ENA, s->intr);
#ifdef CONFIG_ISA
    if (s->flags & IS_VADEM)
	vg46x_set_state(s);
#endif
#ifdef CONFIG_PCI
    if (s->flags & IS_O2MICRO)
	o2micro_set_state(s);
    else if (s->flags & IS_TI)
	ti113x_set_state(s);
    else if (s->flags & IS_RICOH)
	ricoh_set_state(s);
    else if (s->flags & IS_TOPIC)
	topic_set_state(s);
#endif
}

static u_int __init set_bridge_opts(socket_info_t *s, u_short ns)
{
    u_short i;
    u_int m = 0xffff;
    char buf[128];

    for (i = 0; i < ns; i++) {
	if (s[i].flags & IS_ALIVE) {
	    printk(KERN_INFO "    host opts [%d]: already alive!\n", i);
	    continue;
	}
	buf[0] = '\0';
	get_bridge_state(s+i);
	if (s[i].flags & IS_CIRRUS)
	    m = cirrus_set_opts(s+i, buf);
#ifdef CONFIG_ISA
	else if (s[i].flags & IS_VADEM)
	    m = vg46x_set_opts(s+i, buf);
#endif
#ifdef CONFIG_PCI
	else if (s[i].flags & IS_O2MICRO)
	    m = o2micro_set_opts(s+i, buf);
	else if (s[i].flags & IS_TI)
	    m = ti113x_set_opts(s+i, buf);
	else if (s[i].flags & IS_RICOH)
	    m = ricoh_set_opts(s+i, buf);
	else if (s[i].flags & IS_TOPIC)
	    m = topic_set_opts(s+i, buf);
	if (s[i].flags & IS_CARDBUS)
	    cb_set_opts(s+i, buf+strlen(buf));
#endif
	set_bridge_state(s+i);
	printk(KERN_INFO "    host opts [%d]:%s\n", i,
	       (*buf) ? buf : " none");
    }
#ifdef CONFIG_PCI
//    m &= ~pci_irq_mask;	macosx
#endif
    return m;
}

/*======================================================================

    Interrupt testing code, for ISA and PCI interrupts
    
======================================================================*/

#ifndef __MACOSX__
static volatile u_int irq_hits, irq_shared;
static volatile socket_info_t *irq_sock;

static irq_ret_t irq_count IRQ(int irq, void *dev, struct pt_regs *regs)
{
    irq_hits++;
#ifndef __BEOS__
    DEBUG(2, "-> hit on irq %d\n", irq);
#endif
    if (!irq_shared && (irq_hits > 100)) {
	printk(KERN_INFO "    PCI irq %d seems to be wedged!\n", irq);
	disable_irq(irq);
	return (irq_ret_t)0;
    }
#ifdef CONFIG_PCI
    if (irq_sock->flags & IS_CARDBUS) {
	cb_writel(irq_sock, CB_SOCKET_EVENT, -1);
    } else
#endif
    i365_get((socket_info_t *)irq_sock, I365_CSC);
    return (irq_ret_t)1;
}

static u_int __init test_irq(socket_info_t *s, int irq, int pci)
{
    u_char csc = (pci) ? 0 : irq;

#ifdef CONFIG_PNP_BIOS
    extern int check_pnp_irq(int);
    if (!pci && check_pnp_irq(irq)) return 1;
#endif

    DEBUG(2, "  testing %s irq %d\n", pci ? "PCI" : "ISA", irq);
    irq_sock = s; irq_shared = irq_hits = 0;
    if (_request_irq(irq, irq_count, 0, "scan")) {
	irq_shared++;
	if (!pci || _request_irq(irq, irq_count, SA_SHIRQ, "scan"))
	    return 1;
    }
    irq_hits = 0;
    __set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_timeout(HZ/100);
    if (irq_hits && !irq_shared) {
	_free_irq(irq, irq_count);
	DEBUG(2, "    spurious hit!\n");
	return 1;
    }

    /* Generate one interrupt */
#ifdef CONFIG_PCI
    if (s->flags & IS_CARDBUS) {
	cb_writel(s, CB_SOCKET_EVENT, -1);
	i365_set(s, I365_CSCINT, I365_CSC_STSCHG | (csc << 4));
	cb_writel(s, CB_SOCKET_EVENT, -1);
	cb_writel(s, CB_SOCKET_MASK, CB_SM_CSTSCHG);
	cb_writel(s, CB_SOCKET_FORCE, CB_SE_CSTSCHG);
	mdelay(1);
	cb_writel(s, CB_SOCKET_EVENT, -1);
	cb_writel(s, CB_SOCKET_MASK, 0);
    } else
#endif
    {
	i365_set(s, I365_CSCINT, I365_CSC_DETECT | (csc << 4));
	i365_bset(s, I365_GENCTL, I365_CTL_SW_IRQ);
	mdelay(1);
    }

    _free_irq(irq, irq_count);

    /* mask all interrupts */
    i365_set(s, I365_CSCINT, 0);
    DEBUG(2, "    hits = %d\n", irq_hits);
    
    return pci ? (irq_hits == 0) : (irq_hits != 1);
}
#endif __MACOSX__

#ifdef CONFIG_ISA
#ifdef __LINUX__
static int _check_irq(int irq, int flags)
{
#ifdef CONFIG_PNP_BIOS
    extern int check_pnp_irq(int);
    if ((flags != SA_SHIRQ) && check_pnp_irq(irq))
	return -1;
#endif
    if (request_irq(irq, irq_count, flags, "x", irq_count) != 0)
	return -1;
    free_irq(irq, irq_count);
    return 0;
}
#endif

static u_int __init isa_scan(socket_info_t *s, u_int mask0)
{
    u_int mask1 = 0;
    int i;

#ifdef CONFIG_PCI
    /* Only scan if we can select ISA csc irq's */
    if (!(s->flags & IS_CARDBUS) || (cb_set_irq_mode(s, 0, 0) == 0))
#endif
    if (do_scan) {
	set_bridge_state(s);
	i365_set(s, I365_CSCINT, 0);
	for (i = 0; i < 16; i++)
	    if ((mask0 & (1 << i)) && (test_irq(s, i, 0) == 0))
		mask1 |= (1 << i);
	for (i = 0; i < 16; i++)
	    if ((mask1 & (1 << i)) && (test_irq(s, i, 0) != 0))
		mask1 ^= (1 << i);
    }
    
    printk(KERN_INFO "    ISA irqs (");
    if (mask1) {
	printk("scanned");
    } else {
	/* Fallback: just find interrupts that aren't in use */
	for (i = 0; i < 16; i++)
	    if ((mask0 & (1 << i)) && (_check_irq(i, 0) == 0))
		mask1 |= (1 << i);
	printk("default");
	/* If scan failed, default to polled status */
	if (!cs_irq && (poll_interval == 0)) poll_interval = HZ;
    }
    printk(") = ");
    
    for (i = 0; i < 16; i++)
	if (mask1 & (1<<i))
	    printk("%s%d", ((mask1 & ((1<<i)-1)) ? "," : ""), i);
    if (mask1 == 0) printk("none!");
    
    return mask1;
}
#endif /* CONFIG_ISA */

#ifdef CONFIG_PCI
static int __init pci_scan(socket_info_t *s)
{
#ifndef __MACOSX__
    int ret;
    if ((s->flags & IS_RICOH) || !(s->flags & IS_CARDBUS) || !do_scan) {
	/* for PCI-to-PCMCIA bridges, just check for wedged irq */
	irq_sock = s; irq_hits = 0;
	if (_request_irq(s->cap.pci_irq, irq_count, 0, "scan"))
	    return 1;
	udelay(50);
	_free_irq(s->cap.pci_irq, irq_count);
	return (!irq_hits);
    }
#endif
    cb_set_irq_mode(s, 1, 0);
    set_bridge_state(s);
    i365_set(s, I365_CSCINT, 0);
#ifndef __MACOSX__
    ret = ((test_irq(s, s->cap.pci_irq, 1) == 0) &&
	   (test_irq(s, s->cap.pci_irq, 1) == 0));
    if (!ret)
	printk(KERN_INFO "    PCI irq %d test failed\n",
	       s->cap.pci_irq);
    return ret;
#else
    return 1;
#endif
}
#endif /* CONFIG_PCI */

/*====================================================================*/

#ifdef CONFIG_ISA

static int __init isa_identify(u_short port, u_short sock)
{
    socket_info_t *s = socket+sockets;
    u_char val;
    int type = -1;

    /* Use the next free entry in the socket table */
    s->ioaddr = port;
    s->psock = sock;
    
    /* Wake up a sleepy Cirrus controller */
    if (wakeup) {
	i365_bclr(s, PD67_MISC_CTL_2, PD67_MC2_SUSPEND);
	/* Pause at least 50 ms */
	mdelay(50);
    }
    
    if ((val = i365_get(s, I365_IDENT)) & 0x70)
	return -1;
    switch (val) {
    case 0x82:
	type = IS_I82365A; break;
    case 0x83:
	type = IS_I82365B; break;
    case 0x84:
	type = IS_I82365DF; break;
    case 0x88: case 0x89: case 0x8a:
	type = IS_IBM; break;
    }
    
    /* Check for Vadem VG-468 chips */
    outb(0x0e, port);
    outb(0x37, port);
    i365_bset(s, VG468_MISC, VG468_MISC_VADEMREV);
    val = i365_get(s, I365_IDENT);
    if (val & I365_IDENT_VADEM) {
	i365_bclr(s, VG468_MISC, VG468_MISC_VADEMREV);
	type = ((val & 7) >= 4) ? IS_VG469 : IS_VG468;
    }

    /* Check for Ricoh chips */
    val = i365_get(s, RF5C_CHIP_ID);
    if ((val == RF5C_CHIP_RF5C296) || (val == RF5C_CHIP_RF5C396))
	type = IS_RF5Cx96;
    
    /* Check for Cirrus CL-PD67xx chips */
    i365_set(s, PD67_CHIP_INFO, 0);
    val = i365_get(s, PD67_CHIP_INFO);
    if ((val & PD67_INFO_CHIP_ID) == PD67_INFO_CHIP_ID) {
	val = i365_get(s, PD67_CHIP_INFO);
	if ((val & PD67_INFO_CHIP_ID) == 0) {
	    type = (val & PD67_INFO_SLOTS) ? IS_PD672X : IS_PD6710;
	    i365_set(s, PD67_EXT_INDEX, 0xe5);
	    if (i365_get(s, PD67_EXT_INDEX) != 0xe5)
		type = IS_VT83C469;
	}
    }
    return type;
} /* isa_identify */

#endif

/*======================================================================

    See if a card is present, powered up, in IO mode, and already
    bound to a (non PC Card) Linux driver.  We leave these alone.

    We make an exception for cards that seem to be serial devices.
    
======================================================================*/

static int __init is_alive(socket_info_t *s)
{
    u_char stat;
    u_short start, stop;
    
    stat = i365_get(s, I365_STATUS);
    start = i365_get_pair(s, I365_IO(0)+I365_W_START);
    stop = i365_get_pair(s, I365_IO(0)+I365_W_STOP);
    if ((stop - start < 0x40) && (stop - start >= 0x07) &&
	((start & 0xfeef) != 0x02e8) && (start >= 0x100) &&
	(stat & I365_CS_DETECT) && (stat & I365_CS_POWERON) &&
	(i365_get(s, I365_INTCTL) & I365_PC_IOCARD) &&
	(i365_get(s, I365_ADDRWIN) & I365_ENA_IO(0)) &&
#ifndef __MACOSX__
	(check_region(start, stop-start+1) != 0))
#else
	1) 
#endif
	return 1;
    else
	return 0;
}

/*====================================================================*/

static void __init add_socket(u_short port, int psock, int type)
{
    socket_info_t *s = socket+sockets;
#ifndef __MACOSX__
    s->ioaddr = port;
    s->psock = psock;
#endif
    s->type = type;
    s->flags = pcic[type].flags;
    if (is_alive(s))
	s->flags |= IS_ALIVE;
    sockets++;
}

static void __init add_pcic(int ns, int type)
{
    u_int mask = 0, i;
    int use_pci = 0, isa_irq = 0;
    socket_info_t *s = &socket[sockets-ns];

#ifndef __MACOSX__
    if (s->ioaddr > 0) request_region(s->ioaddr, 2, "i82365");
#endif
    
#ifdef __MACOSX__
    printk("\n");
#else
    if (sockets == ns) printk("\n");
#endif
    printk(KERN_INFO "  %s", pcic[type].name);
#ifdef __MACOSX__
    if (s->flags & IS_UNKNOWN)
	printk(" [0x%04x 0x%04x]", s->vendor, s->device);
    printk(" rev %02x", s->revision);
    if (s->flags & IS_CARDBUS)
	printk(" PCI-to-CardBus phys mem 0x%08x virt mem 0x%08x\n", s->cb_phys, s->cb_virt);
    else
	printk("This code currently only supports cardbus controllers, s=0x%lx", (u_long)s);
#else
#ifdef CONFIG_PCI
    if (s->flags & IS_UNKNOWN)
	printk(" [%04x %04x]", s->vendor, s->device);
    printk(" rev %02x", s->revision);
    if (s->flags & IS_CARDBUS)
	printk(" PCI-to-CardBus at slot %02x:%02x, mem %#08x\n",
	       s->bus, PCI_SLOT(s->devfn), s->cb_phys);
    else if (s->flags & IS_PCI)
	printk(" PCI-to-PCMCIA at slot %02x:%02x, port %#x\n",
	       s->bus, PCI_SLOT(s->devfn), s->ioaddr);
    else
#endif
	printk(" ISA-to-PCMCIA at port %#x ofs 0x%02x\n",
	       s->ioaddr, s->psock*0x40);
#endif // __MACOSX__

#ifdef CONFIG_ISA
    if (irq_list[0] == -1)
	mask = irq_mask;
    else
	for (i = mask = 0; i < 16; i++)
	    mask |= (1<<irq_list[i]);
#endif
    /* Set host options, build basic interrupt mask */
    mask &= I365_ISA_IRQ_MASK & set_bridge_opts(s, ns);

#ifdef CONFIG_PCI
    /* Can we use PCI interrupts for card status changes? */
    if (pci_csc || pci_int) {
	for (i = 0; i < ns; i++)
	    if (!s[i].cap.pci_irq || !pci_scan(&s[i])) break;
	use_pci = (i == ns);
    }
#endif
#ifdef CONFIG_ISA
    /* Scan, report ISA card interrupts */
    if (mask)
	mask = isa_scan(s, mask);
#endif

#ifdef CONFIG_PCI
    if (!mask)
	printk(KERN_INFO "    %s card interrupts,",
	       (use_pci && pci_int) ? "PCI" : "*NO*");
    if (use_pci && pci_csc)
	printk(" PCI status changes\n");
#endif

#ifdef CONFIG_ISA
    /* Poll if only two sensible interrupts available */
    if (!(use_pci && pci_csc) && !poll_interval) {
	u_int tmp = (mask & 0xff20);
	tmp = tmp & (tmp-1);
	if ((tmp & (tmp-1)) == 0)
	    poll_interval = HZ;
    }
    /* Only try an ISA cs_irq if this is the first controller */
    if (!(use_pci && pci_csc) && !grab_irq &&
	(cs_irq || !poll_interval)) {
	/* Avoid irq 12 unless it is explicitly requested */
	u_int cs_mask = mask & ((cs_irq) ? (1<<cs_irq) : ~(1<<12));
	for (isa_irq = 15; isa_irq > 0; isa_irq--)
	    if (cs_mask & (1 << isa_irq)) break;
	if (isa_irq) {
	    grab_irq = 1;
	    cs_irq = isa_irq;
	    printk(" status change on irq %d\n", isa_irq);
	}
    }
#endif
    
    if (!(use_pci && pci_csc) && !isa_irq) {
	if (poll_interval == 0)
	    poll_interval = HZ;
	printk(" polling interval = %d ms\n", poll_interval*1000/HZ);
    }
    
    /* Update socket interrupt information, capabilities */
    for (i = 0; i < ns; i++) {
	s[i].cap.features |= SS_CAP_PCCARD;
	s[i].cap.map_size = 0x1000;
	s[i].cap.irq_mask = mask;
	if (!use_pci)
	    s[i].cap.pci_irq = 0;
	s[i].cs_irq = isa_irq;
#ifdef CONFIG_PCI
	if (s[i].flags & IS_CARDBUS) {
	    s[i].cap.features |= SS_CAP_CARDBUS;
	    cb_set_irq_mode(s+i, pci_csc && s[i].cap.pci_irq, 0);
	}
#endif
    }

} /* add_pcic */

/*====================================================================*/

#ifdef CONFIG_PCI

#ifdef __BEOS__
typedef u_short pci_id_t;
static int pci_lookup(u_int class, pci_id_t *id,
		      u_char *bus, u_char *devfn)
{
    pci_info info;
    while (pci->get_nth_pci_info((*id)++, &info) == 0) {
	if (((info.class_base<<8) + info.class_sub) == class) {
	    *bus = info.bus;
	    *devfn = PCI_DEVFN(info.device, info.function);
	    return 0;
	}
    }
    return -1;
}
#endif

#ifdef __LINUX__ //macosx
typedef struct pci_dev *pci_id_t;
static int __init pci_lookup(u_int class, pci_id_t *id,
			     u_char *bus, u_char *devfn)
{
    if ((*id = pci_find_class(class<<8, *id)) != NULL) {
	*bus = (*id)->bus->number;
	*devfn = (*id)->devfn;
	return 0;
    } else return -1;
}
#endif

static void __init add_pci_bridge(int type, u_short v, u_short d)
{
    socket_info_t *s = &socket[sockets];
    u_int addr, ns;

#ifdef __LINUX__
    pci_enable_device(pci_find_slot(s->bus, s->devfn));
#endif

    if (type == PCIC_COUNT) type = IS_UNK_PCI;
    pci_readl(s, PCI_BASE_ADDRESS_0, &addr);
    addr &= ~0x1;
    for (ns = 0; ns < ((type == IS_I82092AA) ? 4 : 2); ns++) {
#ifndef __MACOSX__
	s[ns].bus = s->bus; s[ns].devfn = s->devfn;
#endif
	s[ns].vendor = v; s[ns].device = d;
	add_socket(addr, ns, type);
    }
    add_pcic(ns, type);
}

static int check_cb_mapping(socket_info_t *s)
{
    u_int state = cb_readl(s, CB_SOCKET_STATE) >> 16;
    /* A few sanity checks to validate the bridge mapping */
    if ((cb_readb(s, 0x800+I365_IDENT) & 0x70) ||
	(cb_readb(s, 0x800+I365_CSC) && cb_readb(s, 0x800+I365_CSC) &&
	 cb_readb(s, 0x800+I365_CSC)) || cb_readl(s, CB_SOCKET_FORCE) ||
	((state & ~0x3000) || !(state & 0x3000)))
	return 1;
    return 0;
}

static void __init add_cb_bridge(int type, u_short v, u_short d0)
{
    socket_info_t *s = &socket[sockets];
#ifdef __MACOSX__
    u_short ns;
    u_char r;
#else
    u_char bus = s->bus, devfn = s->devfn;
    u_short d, ns;
    u_char a, r, max;
#endif
    
#ifdef __MACOSX__
    // the following code was checking if the device is multifunction and adding
    // sockets on the fly for each device it finds, iokit is handling that
    // for us, and hopefully its enumeration isn't broken :-)

    if (type == PCIC_COUNT) type = IS_UNK_CARDBUS;
    pci_readb(s, PCI_CLASS_REVISION, &r);
    s->vendor = v; s->device = d0; s->revision = r;
    
    do {
#else
    /* PCI bus enumeration is broken on some systems */
    for (ns = 0; ns < sockets; ns++)
	if ((socket[ns].bus == bus) &&
	    (socket[ns].devfn == devfn))
	    return;
    
    if (type == PCIC_COUNT) type = IS_UNK_CARDBUS;
    pci_readb(s, PCI_HEADER_TYPE, &a);
    pci_readb(s, PCI_CLASS_REVISION, &r);
    max = (a & 0x80) ? 8 : 1;
    for (ns = 0; ns < max; ns++, s++, devfn++) {
	s->bus = bus; s->devfn = devfn;
	if (pci_readw(s, PCI_DEVICE_ID, &d) || (d != d0))
	    break;
	s->vendor = v; s->device = d; s->revision = r;
#endif    

#ifdef __LINUX__
	pci_enable_device(pci_find_slot(bus, devfn));
	pci_set_power_state(pci_find_slot(bus, devfn), 0);
#endif
	/* Set up CardBus register mapping */
	pci_writel(s, CB_LEGACY_MODE_BASE, 0);
	pci_readl(s, PCI_BASE_ADDRESS_0, &s->cb_phys);
	if (s->cb_phys == 0) {
	    printk("\n" KERN_NOTICE "  Bridge register mapping failed:"
		   " check cb_mem_base setting\n");
	    break;
	}
#ifndef __MACOSX__
	s->cb_virt = ioremap(s->cb_phys, 0x1000);
	if (check_cb_mapping(s) != 0) {
	    printk("\n" KERN_NOTICE "  Bad bridge mapping at "
		   "0x%08x!\n", s->cb_phys);
	    break;
	}

	request_mem_region(s->cb_phys, 0x1000, "i82365");
	add_socket(0, 0, type);
    }
    if (ns == 0) return;
    
#else
	// it is easier to map cardbus registers from the shim.
	// lets make sure we did it right
	if (check_cb_mapping(s) != 0) {
	    printk("\n" KERN_NOTICE "  Bad bridge mapping at "
		   "0x%08x!\n", s->cb_phys);
	    return;
	}

	add_socket(0, 0, type);
	
    } while (0);

    // HACK - don't scan for next socket, iokit does that for us
    ns = 1;	
	
#endif __MACOSX__

    add_pcic(ns, type);

// macosx - we handle this on the fly from cb_alloc()
#ifdef __LINUX__
    /* Look up PCI bus bridge structures if needed */
    s -= ns;
    for (a = 0; a < ns; a++) {
	struct pci_dev *self = pci_find_slot(bus, s[a].devfn);
#if (LINUX_VERSION_CODE >= VERSION(2,3,40))
	s[a].cap.cb_bus = self->subordinate;
#else
	struct pci_bus *child;
	for (child = self->bus->children; child; child = child->next)
	    if (child->number == s[a].cap.cardbus) break;
	s[a].cap.cb_bus = child;
#endif
    }
#endif
}

#ifdef __LINUX__ //macosx
static void __init pci_probe(u_int class)
{
    socket_info_t *s = &socket[sockets];
    u_short i, v, d;
    pci_id_t id;
    
    id = 0;
    while (pci_lookup(class, &id, &s->bus, &s->devfn) == 0) {
	if (PCI_FUNC(s->devfn) != 0) continue;
	pci_readw(s, PCI_VENDOR_ID, &v);
	pci_readw(s, PCI_DEVICE_ID, &d);
	for (i = 0; i < PCIC_COUNT; i++)
	    if ((pcic[i].vendor == v) && (pcic[i].device == d)) break;
	if (((i < PCIC_COUNT) && (pcic[i].flags & IS_CARDBUS)) ||
	    (class == PCI_CLASS_BRIDGE_CARDBUS))
	    add_cb_bridge(i, v, d);
	else
	    add_pci_bridge(i, v, d);
	s = &socket[sockets];
    }
}
#endif __LINUX__ //macosx

#ifdef __MACOSX__
// in macosx, we already know that we have a valid device
// so this code doesn't really "probe" anymore, it just
// looks up the correct controller

static void __init
pci_probe(u_int class)
{
    socket_info_t *s = &socket[sockets];
    u_short i, v, d;
    u_char base_class, sub_class;
    
    pci_readb(s, 0xb, &base_class);
    pci_readb(s, 0xa, &sub_class);

    if (class == ((base_class << 8) | sub_class)) {
	pci_readw(s, PCI_VENDOR_ID, &v);
	pci_readw(s, PCI_DEVICE_ID, &d);
	for (i = 0; i < PCIC_COUNT; i++)
	    if ((pcic[i].vendor == v) && (pcic[i].device == d)) break;
	if (pcic[i].flags & IS_CARDBUS)
	    add_cb_bridge(i, v, d);
	else
	    add_pci_bridge(i, v, d);
    }
}
#endif __MACOSX__

#endif

/*====================================================================*/

#ifdef CONFIG_ISA

static void __init isa_probe(ioaddr_t base)
{
    int i, j, sock, k, ns, id;
    ioaddr_t port;

#ifndef __BEOS__
    if (check_region(base, 2) != 0) {
	if (sockets == 0)
	    printk("port conflict at %#x\n", i365_base);
	return;
    }
#endif

    id = isa_identify(base, 0);
    if ((id == IS_I82365DF) && (isa_identify(base, 1) != id)) {
	for (i = 0; i < 4; i++) {
	    if (i == ignore) continue;
	    port = i365_base + ((i & 1) << 2) + ((i & 2) << 1);
	    sock = (i & 1) << 1;
	    if (isa_identify(port, sock) == IS_I82365DF) {
		add_socket(port, sock, IS_VLSI);
		add_pcic(1, IS_VLSI);
	    }
	}
    } else {
	for (i = 0; i < 4; i += 2) {
	    port = base + 2*(i>>2);
	    sock = (i & 3);
	    id = isa_identify(port, sock);
	    if (id < 0) continue;

	    for (j = ns = 0; j < 2; j++) {
		/* Does the socket exist? */
		if ((ignore == i+j) || (isa_identify(port, sock+j) < 0))
		    continue;
		/* Check for bad socket decode */
		for (k = 0; k <= sockets; k++)
		    i365_set(socket+k, I365_MEM(0)+I365_W_OFF, k);
		for (k = 0; k <= sockets; k++)
		    if (i365_get(socket+k, I365_MEM(0)+I365_W_OFF) != k)
			break;
		if (k <= sockets) break;
		add_socket(port, sock+j, id); ns++;
	    }
	    if (ns != 0) add_pcic(ns, id);
	}
    }
}

#endif

/*======================================================================

    The card status event handler.  This may either be interrupt
    driven or polled.  It monitors mainly for card insert and eject
    events; there are various other kinds of events that can be
    monitored (ready/busy, status change, etc), but they are almost
    never used.
    
======================================================================*/

#ifdef __MACOSX__

/* This code is here to support the IOKit style interrupt mechanism.
   Interrupts are taken in the bottom handler, but are processed in
   the top handler which does not run in the interrupt context.  This
   means that there must be some way to clear or disable the interrupt
   until the top half gets a chance to look at the interrupt. Since
   this is a bus driver we really don't know how to clear the
   interrupt on the card so we end disabling it at a higher level.
   It's actually worse than it sounds...  
*/

static u_int
pcic_interrupt_top(u_int socket_index)
{
    u_int events;
    socket_info_t *s = &socket[socket_index];

    DEBUG(4, "i82365: pcic_interrupt_top(%d)\n", socket_index);

    if (!s->handler) return 0;

//MACOSXXX grab lock
    events = s->events;
    s->events = 0;
//MACOSXXX drop lock

    if (events)
	s->handler(s->info, events);

    return 0;
}

static u_int
pcic_interrupt(u_int socket_index)
{
    int csc;
    u_int events;
#ifdef CONFIG_ISA
    u_long flags = 0;
#endif
    socket_info_t *s = &socket[socket_index];

    DEBUG(4, "i82365: pcic_interrupt(%d)\n", socket_index);

    ISA_LOCK(s, flags);
    csc = i365_get(s, I365_CSC);
#ifdef CONFIG_PCI
    if ((s->flags & IS_CARDBUS) &&
	(cb_readl(s, CB_SOCKET_EVENT) & CB_SE_CCD)) {
	cb_writel(s, CB_SOCKET_EVENT, CB_SE_CCD);
	csc |= I365_CSC_DETECT;
    }
#endif
    if ((csc == 0) || (!s->handler) ||
	(i365_get(s, I365_IDENT) & 0x70)) {
	ISA_UNLOCK(s, flags);
	return 0;
    }
    events = (csc & I365_CSC_DETECT) ? SS_DETECT : 0;
    if (i365_get(s, I365_INTCTL) & I365_PC_IOCARD)
	events |= (csc & I365_CSC_STSCHG) ? SS_STSCHG : 0;
    else {
	events |= (csc & I365_CSC_BVD1) ? SS_BATDEAD : 0;
	events |= (csc & I365_CSC_BVD2) ? SS_BATWARN : 0;
	events |= (csc & I365_CSC_READY) ? SS_READY : 0;
    }
    ISA_UNLOCK(s, flags);
    DEBUG(2, "i82365: pcic_interrupt - socket %d event 0x%02x\n", socket_index, events);

//MACOSXXX grab lock
    s->events |= events;
//MACOSXXX drop lock

    return events;
} /* pcic_interrupt */


// MACOSXXX This should probably be broken up in chip specific handlers
// that can be registered individually by configure_i82365.

static u_int
pcic_enable_functional_interrupt(u_int socket_index)
{
    socket_info_t *s = &socket[socket_index];

    if (s->type <= IS_TI1131) {
	u_char cardctl;
	pci_readb(s, TI113X_CARD_CONTROL, &cardctl);
	cardctl |= TI113X_CCR_PCI_IREQ;
	pci_writeb(s, TI113X_CARD_CONTROL, cardctl);
    } else if (s->type == IS_TI1210 || 
	       s->type == IS_TI1211 || 
	       s->type == IS_TI1410 ||
	       s->type == IS_TI1420 ||
	       s->type == IS_TI1510) {
    	u_short bcr;
	pci_readw(s, CB_BRIDGE_CONTROL, &bcr);
	bcr &= ~CB_BCR_ISA_IRQ;
	pci_writew(s, CB_BRIDGE_CONTROL, bcr);
    }

    return 0;
}

static u_int
pcic_disable_functional_interrupt(u_int socket_index)
{
    socket_info_t *s = &socket[socket_index];

    if (s->type <= IS_TI1131) {
	u_char cardctl;
	pci_readb(s, TI113X_CARD_CONTROL, &cardctl);
	cardctl &= ~TI113X_CCR_PCI_IREQ;
	pci_writeb(s, TI113X_CARD_CONTROL, cardctl);
    } else if (s->type == IS_TI1210 || 
	       s->type == IS_TI1211 || 
	       s->type == IS_TI1410 ||
	       s->type == IS_TI1420 ||
	       s->type == IS_TI1510) {
    	u_short bcr;
	pci_readw(s, CB_BRIDGE_CONTROL, &bcr);
	bcr |= CB_BCR_ISA_IRQ;
	pci_writew(s, CB_BRIDGE_CONTROL, bcr);
    }

    return 0;
}

#else

static irq_ret_t pcic_interrupt IRQ(int irq, void *dev,
				    struct pt_regs *regs)
{
#ifdef __BEOS__
    int irq = (int *)dev - irq_list;
#endif
    int i, j, csc;
    u_int events, active;
#ifdef CONFIG_ISA
    u_long flags = 0;
#endif
    
    DEBUG(2, "i82365: pcic_interrupt(%d)\n", irq);

    for (j = 0; j < 20; j++) {
	active = 0;
	for (i = 0; i < sockets; i++) {
	    socket_info_t *s = &socket[i];
	    if ((s->cs_irq != irq) && (s->cap.pci_irq != irq))
		continue;
	    ISA_LOCK(s, flags);
	    csc = i365_get(s, I365_CSC);
#ifdef CONFIG_PCI
	    if ((s->flags & IS_CARDBUS) &&
		(cb_readl(s, CB_SOCKET_EVENT) & CB_SE_CCD)) {
		cb_writel(s, CB_SOCKET_EVENT, CB_SE_CCD);
		csc |= I365_CSC_DETECT;
	    }
#endif
	    if ((csc == 0) || (!s->handler) ||
		(i365_get(s, I365_IDENT) & 0x70)) {
		ISA_UNLOCK(s, flags);
		continue;
	    }
	    events = (csc & I365_CSC_DETECT) ? SS_DETECT : 0;
	    if (i365_get(s, I365_INTCTL) & I365_PC_IOCARD) {
		events |= (csc & I365_CSC_STSCHG) ? SS_STSCHG : 0;
	    } else {
		events |= (csc & I365_CSC_BVD1) ? SS_BATDEAD : 0;
		events |= (csc & I365_CSC_BVD2) ? SS_BATWARN : 0;
		events |= (csc & I365_CSC_READY) ? SS_READY : 0;
	    }
	    ISA_UNLOCK(s, flags);
	    DEBUG(1, "i82365: socket %d event 0x%04x\n", i, events);
	    if (events)
		s->handler(s->info, events);
	    active |= events;
	}
	if (!active) break;
    }
    if (j == 20)
	printk(KERN_NOTICE "i82365: infinite loop in interrupt "
	       "handler: active = 0x%04x\n", active);

    DEBUG(2, "i82365: interrupt done\n");
    return (irq_ret_t)1;
} /* pcic_interrupt */
#endif

static void pcic_interrupt_wrapper(u_long data)
{
#ifdef __BEOS__
    pcic_interrupt IRQ(0, irq_list, NULL);
#endif
#ifdef __LINUX__ //macosx
    pcic_interrupt(0, NULL, NULL);
#endif
#ifdef __MACOSX__
    pcic_interrupt(0);
#endif
    poll_timer.expires = jiffies + poll_interval;
    add_timer(&poll_timer);
}

/*====================================================================*/

static int pcic_register_callback(socket_info_t *s, ss_callback_t *call)
{
    if (call == NULL) {
	s->handler = NULL;
	MOD_DEC_USE_COUNT;
    } else {
	MOD_INC_USE_COUNT;
	s->handler = call->handler;
	s->info = call->info;
    }
    return 0;
} /* pcic_register_callback */

/*====================================================================*/

static int pcic_inquire_socket(socket_info_t *s, socket_cap_t *cap)
{
    *cap = s->cap;
    return 0;
}

/*====================================================================*/

static int i365_get_status(socket_info_t *s, u_int *value)
{
    u_int status;
    
    status = i365_get(s, I365_STATUS);
    *value = ((status & I365_CS_DETECT) == I365_CS_DETECT)
	? SS_DETECT : 0;
    if (i365_get(s, I365_INTCTL) & I365_PC_IOCARD) {
	*value |= (status & I365_CS_STSCHG) ? 0 : SS_STSCHG;
    } else {
	*value |= (status & I365_CS_BVD1) ? 0 : SS_BATDEAD;
	*value |= (status & I365_CS_BVD2) ? 0 : SS_BATWARN;
    }
    *value |= (status & I365_CS_WRPROT) ? SS_WRPROT : 0;
    *value |= (status & I365_CS_READY) ? SS_READY : 0;
    *value |= (status & I365_CS_POWERON) ? SS_POWERON : 0;

#ifdef CONFIG_PCI
    if (s->flags & IS_CARDBUS) {
	status = cb_readl(s, CB_SOCKET_STATE);
	*value |= (status & CB_SS_32BIT) ? SS_CARDBUS : 0;
#ifdef __MACOSX__
	// MACOSXXX the current code assumes that we always can
	// always support 5V, that may change in the future!
	if ((status & CB_SS_5VSOCKET) == 0)
	    panic("ss: controller doesn't support 5V cards\n");
	if (status & CB_SS_3VSOCKET) 
	    *value |= (status & CB_SS_3VCARD) ? SS_3VCARD : 0;
	if (status & CB_SS_XVSOCKET) 
	    *value |= (status & CB_SS_XVCARD) ? SS_XVCARD : 0;
	// set this to XV for now, we are running out bits
	if (status & CB_SS_YVSOCKET) 
	    *value |= (status & CB_SS_YVCARD) ? SS_XVCARD : 0;
#else
	*value |= (status & CB_SS_3VCARD) ? SS_3VCARD : 0;
	*value |= (status & CB_SS_XVCARD) ? SS_XVCARD : 0;
#endif
	*value |= (status & CB_SS_VSENSE) ? 0 : SS_PENDING;
    } else if (s->flags & IS_O2MICRO) {
	status = i365_get(s, O2_MODE_B);
	*value |= (status & O2_MODE_B_VS1) ? 0 : SS_3VCARD;
	*value |= (status & O2_MODE_B_VS2) ? 0 : SS_XVCARD;
#ifndef __MACOSX__	// psock is not defined
    } else if (s->type == IS_PD6729) {
	socket_info_t *t = (s->psock) ? s : s+1;
	status = pd67_ext_get(t, PD67_EXTERN_DATA);
	*value |= (status & PD67_EXD_VS1(s->psock)) ? 0 : SS_3VCARD;
	*value |= (status & PD67_EXD_VS2(s->psock)) ? 0 : SS_XVCARD;
#endif
    }
    /* For now, ignore cards with unsupported voltage keys */
    if (*value & SS_XVCARD)
	*value &= ~(SS_DETECT|SS_3VCARD|SS_XVCARD);
#endif
#ifdef CONFIG_ISA
    if (s->type == IS_VG469) {
	status = i365_get(s, VG469_VSENSE);
	if (s->psock & 1) {
	    *value |= (status & VG469_VSENSE_B_VS1) ? 0 : SS_3VCARD;
	    *value |= (status & VG469_VSENSE_B_VS2) ? 0 : SS_XVCARD;
	} else {
	    *value |= (status & VG469_VSENSE_A_VS1) ? 0 : SS_3VCARD;
	    *value |= (status & VG469_VSENSE_A_VS2) ? 0 : SS_XVCARD;
	}
    }
#endif
    DEBUG(1, "i82365: GetStatus(%d) = %#4.4x\n", s-socket, *value);
    return 0;
} /* i365_get_status */

/*====================================================================*/

static int i365_get_socket(socket_info_t *s, socket_state_t *state)
{
    u_char reg, vcc, vpp;
    
    reg = i365_get(s, I365_POWER);
    state->flags = (reg & I365_PWR_AUTO) ? SS_PWR_AUTO : 0;
    state->flags |= (reg & I365_PWR_OUT) ? SS_OUTPUT_ENA : 0;
    vcc = reg & I365_VCC_MASK; vpp = reg & I365_VPP1_MASK;
    state->Vcc = state->Vpp = 0;
#ifdef CONFIG_PCI
    if (s->flags & IS_CARDBUS) {
	cb_get_power(s, state);
    } else
#endif
    if (s->flags & IS_CIRRUS) {
	if (i365_get(s, PD67_MISC_CTL_1) & PD67_MC1_VCC_3V) {
	    if (reg & I365_VCC_5V) state->Vcc = 33;
	    if (vpp == I365_VPP1_5V) state->Vpp = 33;
	} else {
	    if (reg & I365_VCC_5V) state->Vcc = 50;
	    if (vpp == I365_VPP1_5V) state->Vpp = 50;
	}
	if (vpp == I365_VPP1_12V) state->Vpp = 120;
    } else if (s->flags & IS_VG_PWR) {
	if (i365_get(s, VG469_VSELECT) & VG469_VSEL_VCC) {
	    if (reg & I365_VCC_5V) state->Vcc = 33;
	    if (vpp == I365_VPP1_5V) state->Vpp = 33;
	} else {
	    if (reg & I365_VCC_5V) state->Vcc = 50;
	    if (vpp == I365_VPP1_5V) state->Vpp = 50;
	}
	if (vpp == I365_VPP1_12V) state->Vpp = 120;
    } else if (s->flags & IS_DF_PWR) {
	if (vcc == I365_VCC_3V) state->Vcc = 33;
	if (vcc == I365_VCC_5V) state->Vcc = 50;
	if (vpp == I365_VPP1_5V) state->Vpp = 50;
	if (vpp == I365_VPP1_12V) state->Vpp = 120;
    } else {
	if (reg & I365_VCC_5V) {
	    state->Vcc = 50;
	    if (vpp == I365_VPP1_5V) state->Vpp = 50;
	    if (vpp == I365_VPP1_12V) state->Vpp = 120;
	}
    }

    /* IO card, RESET flags, IO interrupt */
    reg = i365_get(s, I365_INTCTL);
    state->flags |= (reg & I365_PC_RESET) ? 0 : SS_RESET;
    state->flags |= (reg & I365_PC_IOCARD) ? SS_IOCARD : 0;
#ifdef CONFIG_PCI
    if (cb_get_irq_mode(s) != 0)
	state->io_irq = s->cap.pci_irq;
    else
#endif
	state->io_irq = reg & I365_IRQ_MASK;
    
    /* Card status change mask */
    reg = i365_get(s, I365_CSCINT);
    state->csc_mask = (reg & I365_CSC_DETECT) ? SS_DETECT : 0;
    if (state->flags & SS_IOCARD) {
	state->csc_mask |= (reg & I365_CSC_STSCHG) ? SS_STSCHG : 0;
    } else {
	state->csc_mask |= (reg & I365_CSC_BVD1) ? SS_BATDEAD : 0;
	state->csc_mask |= (reg & I365_CSC_BVD2) ? SS_BATWARN : 0;
	state->csc_mask |= (reg & I365_CSC_READY) ? SS_READY : 0;
    }
    
    DEBUG(2, "i82365: GetSocket(%d) = flags %#3.3x, Vcc %d, Vpp %d, "
	  "io_irq %d, csc_mask %#2.2x\n", s-socket, state->flags,
	  state->Vcc, state->Vpp, state->io_irq, state->csc_mask);
    return 0;
} /* i365_get_socket */

/*====================================================================*/

static int i365_set_socket(socket_info_t *s, socket_state_t *state)
{
    u_char reg;
    
    DEBUG(2, "i82365: SetSocket(%d, flags %#3.3x, Vcc %d, Vpp %d, "
	  "io_irq %d, csc_mask %#2.2x)\n", s-socket, state->flags,
	  state->Vcc, state->Vpp, state->io_irq, state->csc_mask);
    
    /* First set global controller options */
#ifdef CONFIG_PCI
    if (s->cap.pci_irq)
	cb_set_irq_mode(s, pci_csc, (s->cap.pci_irq == state->io_irq));
    s->bcr &= ~CB_BCR_CB_RESET;
#endif
    set_bridge_state(s);
    
    /* IO card, RESET flag, IO interrupt */
    reg = s->intr | ((state->io_irq == s->cap.pci_irq) ?
		     s->pci_irq_code : state->io_irq);
    reg |= (state->flags & SS_RESET) ? 0 : I365_PC_RESET;
    reg |= (state->flags & SS_IOCARD) ? I365_PC_IOCARD : 0;
    i365_set(s, I365_INTCTL, reg);
    
    reg = I365_PWR_NORESET;
    if (state->flags & SS_PWR_AUTO) reg |= I365_PWR_AUTO;
    if (state->flags & SS_OUTPUT_ENA) reg |= I365_PWR_OUT;

#ifdef CONFIG_PCI
    if (s->flags & IS_CARDBUS) {
	cb_set_power(s, state);
	reg |= i365_get(s, I365_POWER) & (I365_VCC_MASK|I365_VPP1_MASK);
    } else
#endif
    if (s->flags & IS_CIRRUS) {
	if (state->Vpp != 0) {
	    if (state->Vpp == 120)
		reg |= I365_VPP1_12V;
	    else if (state->Vpp == state->Vcc)
		reg |= I365_VPP1_5V;
	    else return -EINVAL;
	}
	if (state->Vcc != 0) {
	    reg |= I365_VCC_5V;
	    if (state->Vcc == 33)
		i365_bset(s, PD67_MISC_CTL_1, PD67_MC1_VCC_3V);
	    else if (state->Vcc == 50)
		i365_bclr(s, PD67_MISC_CTL_1, PD67_MC1_VCC_3V);
	    else return -EINVAL;
	}
    } else if (s->flags & IS_VG_PWR) {
	if (state->Vpp != 0) {
	    if (state->Vpp == 120)
		reg |= I365_VPP1_12V;
	    else if (state->Vpp == state->Vcc)
		reg |= I365_VPP1_5V;
	    else return -EINVAL;
	}
	if (state->Vcc != 0) {
	    reg |= I365_VCC_5V;
	    if (state->Vcc == 33)
		i365_bset(s, VG469_VSELECT, VG469_VSEL_VCC);
	    else if (state->Vcc == 50)
		i365_bclr(s, VG469_VSELECT, VG469_VSEL_VCC);
	    else return -EINVAL;
	}
    } else if (s->flags & IS_DF_PWR) {
	switch (state->Vcc) {
	case 0:		break;
	case 33:   	reg |= I365_VCC_3V; break;
	case 50:	reg |= I365_VCC_5V; break;
	default:	return -EINVAL;
	}
	switch (state->Vpp) {
	case 0:		break;
	case 50:   	reg |= I365_VPP1_5V; break;
	case 120:	reg |= I365_VPP1_12V; break;
	default:	return -EINVAL;
	}
    } else {
	switch (state->Vcc) {
	case 0:		break;
	case 50:	reg |= I365_VCC_5V; break;
	default:	return -EINVAL;
	}
	switch (state->Vpp) {
	case 0:		break;
	case 50:	reg |= I365_VPP1_5V | I365_VPP2_5V; break;
	case 120:	reg |= I365_VPP1_12V | I365_VPP2_12V; break;
	default:	return -EINVAL;
	}
    }
    
    if (reg != i365_get(s, I365_POWER))
	i365_set(s, I365_POWER, reg);

    /* Card status change interrupt mask */
    reg = (s->cap.pci_irq ? s->pci_irq_code : s->cs_irq) << 4;
    if (state->csc_mask & SS_DETECT) reg |= I365_CSC_DETECT;
    if (state->flags & SS_IOCARD) {
	if (state->csc_mask & SS_STSCHG) reg |= I365_CSC_STSCHG;
    } else {
	if (state->csc_mask & SS_BATDEAD) reg |= I365_CSC_BVD1;
	if (state->csc_mask & SS_BATWARN) reg |= I365_CSC_BVD2;
	if (state->csc_mask & SS_READY) reg |= I365_CSC_READY;
    }
    i365_set(s, I365_CSCINT, reg);
    i365_get(s, I365_CSC);
#ifdef CONFIG_PCI
    if (s->flags & IS_CARDBUS) {
	if (s->cs_irq || (pci_csc && s->cap.pci_irq))
	    cb_writel(s, CB_SOCKET_MASK, CB_SM_CCD);
	cb_writel(s, CB_SOCKET_EVENT, -1);
    }
#endif

    return 0;
} /* i365_set_socket */

/*====================================================================*/

static int i365_get_io_map(socket_info_t *s, struct pccard_io_map *io)
{
    u_char map, ioctl, addr;
    
    map = io->map;
    if (map > 1) return -EINVAL;
    io->start = i365_get_pair(s, I365_IO(map)+I365_W_START);
    io->stop = i365_get_pair(s, I365_IO(map)+I365_W_STOP);
    ioctl = i365_get(s, I365_IOCTL);
    addr = i365_get(s, I365_ADDRWIN);
    io->speed = (ioctl & I365_IOCTL_WAIT(map)) ? cycle_time : 0;
    io->flags  = (addr & I365_ENA_IO(map)) ? MAP_ACTIVE : 0;
    io->flags |= (ioctl & I365_IOCTL_0WS(map)) ? MAP_0WS : 0;
    io->flags |= (ioctl & I365_IOCTL_16BIT(map)) ? MAP_16BIT : 0;
    io->flags |= (ioctl & I365_IOCTL_IOCS16(map)) ? MAP_AUTOSZ : 0;
    DEBUG(3, "i82365: GetIOMap(%d, %d) = %#2.2x, %d ns, %#4.4x-%#4.4x\n",
	  s-socket, map, io->flags, io->speed, io->start, io->stop);
    return 0;
} /* i365_get_io_map */

/*====================================================================*/

static int i365_set_io_map(socket_info_t *s, struct pccard_io_map *io)
{
    u_char map, ioctl;
    
    DEBUG(3, "i82365: SetIOMap(%d, %d, %#2.2x, %d ns, %#4.4x-%#4.4x)\n",
	  s-socket, io->map, io->flags, io->speed, io->start, io->stop);
    map = io->map;
    if ((map > 1) || (io->start > 0xffff) || (io->stop > 0xffff) ||
	(io->stop < io->start)) return -EINVAL;
    /* Turn off the window before changing anything */
    if (i365_get(s, I365_ADDRWIN) & I365_ENA_IO(map))
	i365_bclr(s, I365_ADDRWIN, I365_ENA_IO(map));
    i365_set_pair(s, I365_IO(map)+I365_W_START, io->start);
    i365_set_pair(s, I365_IO(map)+I365_W_STOP, io->stop);
    ioctl = i365_get(s, I365_IOCTL) & ~I365_IOCTL_MASK(map);
    if (io->speed) ioctl |= I365_IOCTL_WAIT(map);
    if (io->flags & MAP_0WS) ioctl |= I365_IOCTL_0WS(map);
    if (io->flags & MAP_16BIT) ioctl |= I365_IOCTL_16BIT(map);
    if (io->flags & MAP_AUTOSZ) ioctl |= I365_IOCTL_IOCS16(map);
    i365_set(s, I365_IOCTL, ioctl);
    /* Turn on the window if necessary */
    if (io->flags & MAP_ACTIVE)
	i365_bset(s, I365_ADDRWIN, I365_ENA_IO(map));
    return 0;
} /* i365_set_io_map */

/*====================================================================*/

static int i365_get_mem_map(socket_info_t *s, struct pccard_mem_map *mem)
{
    u_short base, i;
    u_char map, addr;
    
    map = mem->map;
    if (map > 4) return -EINVAL;
    addr = i365_get(s, I365_ADDRWIN);
    mem->flags = (addr & I365_ENA_MEM(map)) ? MAP_ACTIVE : 0;
    base = I365_MEM(map);
    
    i = i365_get_pair(s, base+I365_W_START);
    mem->flags |= (i & I365_MEM_16BIT) ? MAP_16BIT : 0;
    mem->flags |= (i & I365_MEM_0WS) ? MAP_0WS : 0;
    mem->sys_start = ((u_long)(i & 0x0fff) << 12);
    
    i = i365_get_pair(s, base+I365_W_STOP);
    mem->speed  = (i & I365_MEM_WS0) ? 1 : 0;
    mem->speed += (i & I365_MEM_WS1) ? 2 : 0;
    mem->speed *= cycle_time;
    mem->sys_stop = ((u_long)(i & 0x0fff) << 12) + 0x0fff;
    
    i = i365_get_pair(s, base+I365_W_OFF);
    mem->flags |= (i & I365_MEM_WRPROT) ? MAP_WRPROT : 0;
    mem->flags |= (i & I365_MEM_REG) ? MAP_ATTRIB : 0;
    mem->card_start = ((u_int)(i & 0x3fff) << 12) + mem->sys_start;
    mem->card_start &= 0x3ffffff;

#ifdef CONFIG_PCI
    /* Take care of high byte, for PCI controllers */
    if (s->type == IS_PD6729) {
	i365_set(s, PD67_EXT_INDEX, PD67_MEM_PAGE(map));
	addr = i365_get(s, PD67_EXT_DATA) << 24;
    } else if (s->flags & IS_CARDBUS) {
	addr = i365_get(s, CB_MEM_PAGE(map)) << 24;
	mem->sys_stop += addr; mem->sys_start += addr;
    }
#endif
    
    DEBUG(3, "i82365: GetMemMap(%d, %d) = %#2.2x, %d ns, %#5.5lx-%#5."
	  "5lx, %#5.5x\n", s-socket, mem->map, mem->flags, mem->speed,
	  mem->sys_start, mem->sys_stop, mem->card_start);
    return 0;
} /* i365_get_mem_map */

/*====================================================================*/
  
static int i365_set_mem_map(socket_info_t *s, struct pccard_mem_map *mem)
{
    u_short base, i;
    u_char map;
    
    DEBUG(3, "i82365: SetMemMap(%d, %d, %#2.2x, %d ns, %#5.5lx-%#5.5"
	  "lx, %#5.5x)\n", s-socket, mem->map, mem->flags, mem->speed,
	  mem->sys_start, mem->sys_stop, mem->card_start);

    map = mem->map;
    if ((map > 4) || (mem->card_start > 0x3ffffff) ||
	(mem->sys_start > mem->sys_stop) || (mem->speed > 1000))
	return -EINVAL;
    if (!(s->flags & (IS_PCI|IS_CARDBUS)) &&
	((mem->sys_start > 0xffffff) || (mem->sys_stop > 0xffffff)))
	return -EINVAL;
	
    /* Turn off the window before changing anything */
    if (i365_get(s, I365_ADDRWIN) & I365_ENA_MEM(map))
	i365_bclr(s, I365_ADDRWIN, I365_ENA_MEM(map));

#ifdef CONFIG_PCI
    /* Take care of high byte, for PCI controllers */
    if (s->type == IS_PD6729) {
	i365_set(s, PD67_EXT_INDEX, PD67_MEM_PAGE(map));
	i365_set(s, PD67_EXT_DATA, (mem->sys_start >> 24));
    } else if (s->flags & IS_CARDBUS)
	i365_set(s, CB_MEM_PAGE(map), mem->sys_start >> 24);
#endif
    
    base = I365_MEM(map);
    i = (mem->sys_start >> 12) & 0x0fff;
    if (mem->flags & MAP_16BIT) i |= I365_MEM_16BIT;
    if (mem->flags & MAP_0WS) i |= I365_MEM_0WS;
    i365_set_pair(s, base+I365_W_START, i);
    
    i = (mem->sys_stop >> 12) & 0x0fff;
    switch (mem->speed / cycle_time) {
    case 0:	break;
    case 1:	i |= I365_MEM_WS0; break;
    case 2:	i |= I365_MEM_WS1; break;
    default:	i |= I365_MEM_WS1 | I365_MEM_WS0; break;
    }
    i365_set_pair(s, base+I365_W_STOP, i);
    
    i = ((mem->card_start - mem->sys_start) >> 12) & 0x3fff;
    if (mem->flags & MAP_WRPROT) i |= I365_MEM_WRPROT;
    if (mem->flags & MAP_ATTRIB) i |= I365_MEM_REG;
    i365_set_pair(s, base+I365_W_OFF, i);
    
    /* Turn on the window if necessary */
    if (mem->flags & MAP_ACTIVE)
	i365_bset(s, I365_ADDRWIN, I365_ENA_MEM(map));
    return 0;
} /* i365_set_mem_map */

/*======================================================================

    The few things that are strictly for Cardbus cards goes here.

======================================================================*/

#ifdef CONFIG_CARDBUS

static int cb_get_status(socket_info_t *s, u_int *value)
{
    u_int state = cb_readl(s, CB_SOCKET_STATE);
    *value = (state & CB_SS_32BIT) ? SS_CARDBUS : 0;
    *value |= (state & CB_SS_CCD) ? 0 : SS_DETECT;
    *value |= (state & CB_SS_CSTSCHG) ? SS_STSCHG : 0;
    *value |= (state & CB_SS_PWRCYCLE) ? (SS_POWERON|SS_READY) : 0;
    *value |= (state & CB_SS_3VCARD) ? SS_3VCARD : 0;
    *value |= (state & CB_SS_XVCARD) ? SS_XVCARD : 0;
    *value |= (state & CB_SS_VSENSE) ? 0 : SS_PENDING;
    DEBUG(1, "yenta: GetStatus(%d) = %#4.4x\n", s-socket, *value);
    return 0;
} /* cb_get_status */

static int cb_get_socket(socket_info_t *s, socket_state_t *state)
{
    u_short bcr;

    cb_get_power(s, state);
    pci_readw(s, CB_BRIDGE_CONTROL, &bcr);
    state->flags |= (bcr & CB_BCR_CB_RESET) ? SS_RESET : 0;
    if (cb_get_irq_mode(s) != 0)
	state->io_irq = s->cap.pci_irq;
    else
	state->io_irq = i365_get(s, I365_INTCTL) & I365_IRQ_MASK;
    DEBUG(2, "yenta: GetSocket(%d) = flags %#3.3x, Vcc %d, Vpp %d"
	  ", io_irq %d, csc_mask %#2.2x\n", s-socket, state->flags,
	  state->Vcc, state->Vpp, state->io_irq, state->csc_mask);
    return 0;
} /* cb_get_socket */

static int cb_set_socket(socket_info_t *s, socket_state_t *state)
{
    u_int reg;
    
    DEBUG(2, "yenta: SetSocket(%d, flags %#3.3x, Vcc %d, Vpp %d, "
	  "io_irq %d, csc_mask %#2.2x)\n", s-socket, state->flags,
	  state->Vcc, state->Vpp, state->io_irq, state->csc_mask);
    
    /* First set global controller options */
    if (s->cap.pci_irq)
	cb_set_irq_mode(s, pci_csc, (s->cap.pci_irq == state->io_irq));
    s->bcr &= ~CB_BCR_CB_RESET;
    s->bcr |= (state->flags & SS_RESET) ? CB_BCR_CB_RESET : 0;
    set_bridge_state(s);
    
    cb_set_power(s, state);
    
    /* Handle IO interrupt using ISA routing */
    reg = s->intr;
    if (state->io_irq != s->cap.pci_irq) reg |= state->io_irq;
    i365_set(s, I365_INTCTL, reg);
    
    /* Handle CSC mask */
    if (!s->cs_irq && (!pci_csc || !s->cap.pci_irq))
	return 0;
    reg = (s->cs_irq << 4);
    if (state->csc_mask & SS_DETECT) reg |= I365_CSC_DETECT;
    i365_set(s, I365_CSCINT, reg);
    i365_get(s, I365_CSC);
    cb_writel(s, CB_SOCKET_MASK, CB_SM_CCD);
    cb_writel(s, CB_SOCKET_EVENT, -1);
    
    return 0;
} /* cb_set_socket */

static int cb_get_bridge(socket_info_t *s, struct cb_bridge_map *m)
{
    u_char map = m->map;

    if (map > 1) return -EINVAL;
    m->flags &= MAP_IOSPACE;
    map += (m->flags & MAP_IOSPACE) ? 2 : 0;
    pci_readl(s, CB_MEM_BASE(map), &m->start);
    pci_readl(s, CB_MEM_LIMIT(map), &m->stop);
    if (m->start || m->stop) {
	m->flags |= MAP_ACTIVE;
	m->stop |= (map > 1) ? 3 : 0x0fff;
    }
    if (map > 1) {
	u_short bcr;
	pci_readw(s, CB_BRIDGE_CONTROL, &bcr);
	m->flags |= (bcr & CB_BCR_PREFETCH(map)) ? MAP_PREFETCH : 0;
    }
    DEBUG(3, "yenta: GetBridge(%d, %d) = %#2.2x, %#4.4x-%#4.4x\n",
	  s-socket, map, m->flags, m->start, m->stop);
    return 0;
}

static int cb_set_bridge(socket_info_t *s, struct cb_bridge_map *m)
{
    u_char map;
    
    DEBUG(3, "yenta: SetBridge(%d, %d, %#2.2x, %#4.4x-%#4.4x)\n",
	  s-socket, m->map, m->flags, m->start, m->stop);
    map = m->map;
    if (!(s->flags & IS_CARDBUS) || (map > 1) || (m->stop < m->start))
	return -EINVAL;
    if (m->flags & MAP_IOSPACE) {
	if ((m->stop > 0xffff) || (m->start & 3) ||
	    ((m->stop & 3) != 3))
	    return -EINVAL;
	map += 2;
    } else {
	u_short bcr;
	if ((m->start & 0x0fff) || ((m->stop & 0x0fff) != 0x0fff))
	    return -EINVAL;
	pci_readw(s, CB_BRIDGE_CONTROL, &bcr);
	bcr &= ~CB_BCR_PREFETCH(map);
	bcr |= (m->flags & MAP_PREFETCH) ? CB_BCR_PREFETCH(map) : 0;
	pci_writew(s, CB_BRIDGE_CONTROL, bcr);
    }
    if (m->flags & MAP_ACTIVE) {
	pci_writel(s, CB_MEM_BASE(map), m->start);
	pci_writel(s, CB_MEM_LIMIT(map), m->stop);
    } else {
	pci_writel(s, CB_MEM_BASE(map), 0);
	pci_writel(s, CB_MEM_LIMIT(map), 0);
    }
    return 0;
}

#endif /* CONFIG_CARDBUS */

/*======================================================================

    Routines for accessing socket information and register dumps via
    /proc/bus/pccard/...
    
======================================================================*/

#ifdef HAS_PROC_BUS

static int proc_read_info(char *buf, char **start, off_t pos,
			  int count, int *eof, void *data)
{
    socket_info_t *s = data;
    char *p = buf;
    p += sprintf(p, "type:     %s\npsock:    %d\n",
		 pcic[s->type].name, s->psock);
#ifdef CONFIG_PCI
    if (s->flags & (IS_PCI|IS_CARDBUS))
	p += sprintf(p, "bus:      %02x\ndevfn:    %02x.%1x\n",
		     s->bus, PCI_SLOT(s->devfn), PCI_FUNC(s->devfn));
    if (s->flags & IS_CARDBUS)
	p += sprintf(p, "cardbus:  %02x\n", s->cap.cardbus);
#endif
    return (p - buf);
}

static int proc_read_exca(char *buf, char **start, off_t pos,
			  int count, int *eof, void *data)
{
    socket_info_t *s = data;
    char *p = buf;
    int i, top;
    
#ifdef CONFIG_ISA
    u_long flags = 0;
#endif
    ISA_LOCK(s, flags);
    top = 0x40;
    if (s->flags & IS_CARDBUS)
	top = (s->flags & IS_CIRRUS) ? 0x140 : 0x50;
    for (i = 0; i < top; i += 4) {
	if (i == 0x50) {
	    p += sprintf(p, "\n");
	    i = 0x100;
	}
	p += sprintf(p, "%02x %02x %02x %02x%s",
		     i365_get(s,i), i365_get(s,i+1),
		     i365_get(s,i+2), i365_get(s,i+3),
		     ((i % 16) == 12) ? "\n" : " ");
    }
    ISA_UNLOCK(s, flags);
    return (p - buf);
}

#ifdef CONFIG_PCI
static int proc_read_pci(char *buf, char **start, off_t pos,
			 int count, int *eof, void *data)
{
    socket_info_t *s = data;
    char *p = buf;
    u_int a, b, c, d;
    int i;
    
    for (i = 0; i < 0xc0; i += 0x10) {
	pci_readl(s, i, &a);
	pci_readl(s, i+4, &b);
	pci_readl(s, i+8, &c);
	pci_readl(s, i+12, &d);
	p += sprintf(p, "%08x %08x %08x %08x\n", a, b, c, d);
    }
    return (p - buf);
}

static int proc_read_cardbus(char *buf, char **start, off_t pos,
			     int count, int *eof, void *data)
{
    socket_info_t *s = data;
    char *p = buf;
    int i, top;

    top = (s->flags & IS_O2MICRO) ? 0x30 : 0x20;
    for (i = 0; i < top; i += 0x10)
	p += sprintf(p, "%08x %08x %08x %08x\n",
		     cb_readl(s,i+0x00), cb_readl(s,i+0x04),
		     cb_readl(s,i+0x08), cb_readl(s,i+0x0c));
    return (p - buf);
}
#endif

static void pcic_proc_setup(socket_info_t *s, struct proc_dir_entry *base)
{
    create_proc_read_entry("info", 0, base, proc_read_info, s);
    create_proc_read_entry("exca", 0, base, proc_read_exca, s);
#ifdef CONFIG_PCI
    if (s->flags & (IS_PCI|IS_CARDBUS))
	create_proc_read_entry("pci", 0, base, proc_read_pci, s);
    if (s->flags & IS_CARDBUS)
	create_proc_read_entry("cardbus", 0, base, proc_read_cardbus, s);
#endif
    s->proc = base;
}

static void pcic_proc_remove(socket_info_t *s)
{
    struct proc_dir_entry *base = s->proc;
    if (base == NULL) return;
    remove_proc_entry("info", base);
    remove_proc_entry("exca", base);
#ifdef CONFIG_PCI
    if (s->flags & (IS_PCI|IS_CARDBUS))
	remove_proc_entry("pci", base);
    if (s->flags & IS_CARDBUS)
	remove_proc_entry("cardbus", base);
#endif
}

#endif /* HAS_PROC_BUS */

/*====================================================================*/

typedef int (*subfn_t)(socket_info_t *, void *);

static subfn_t pcic_service_table[] = {
    (subfn_t)&pcic_register_callback,
    (subfn_t)&pcic_inquire_socket,
    (subfn_t)&i365_get_status,
    (subfn_t)&i365_get_socket,
    (subfn_t)&i365_set_socket,
    (subfn_t)&i365_get_io_map,
    (subfn_t)&i365_set_io_map,
    (subfn_t)&i365_get_mem_map,
    (subfn_t)&i365_set_mem_map,
#ifdef CONFIG_CARDBUS
    (subfn_t)&cb_get_bridge,
    (subfn_t)&cb_set_bridge,
#else
    NULL, NULL,
#endif
#ifdef HAS_PROC_BUS
    (subfn_t)&pcic_proc_setup
#endif
};

#define NFUNC (sizeof(pcic_service_table)/sizeof(subfn_t))

static int pcic_service(u_int sock, u_int cmd, void *arg)
{
    socket_info_t *s = &socket[sock];
    subfn_t fn;
    int ret;
#ifdef CONFIG_ISA
    u_long flags = 0;
#endif
    
#ifdef __MACOSX__
    DEBUG(2, "pcic_service(%d, %d, 0x%p)\n", sock, cmd, arg);
#endif

    if (cmd >= NFUNC)
	return -EINVAL;

    if (s->flags & IS_ALIVE) {
	if (cmd == SS_GetStatus)
	    *(u_int *)arg = 0;
	return -EINVAL;
    }
    
    fn = pcic_service_table[cmd];
#ifdef CONFIG_CARDBUS
    if ((s->flags & IS_CARDBUS) &&
	(cb_readl(s, CB_SOCKET_STATE) & CB_SS_32BIT)) {
	if (cmd == SS_GetStatus)
	    fn = (subfn_t)&cb_get_status;
	else if (cmd == SS_GetSocket)
	    fn = (subfn_t)&cb_get_socket;
	else if (cmd == SS_SetSocket)
	    fn = (subfn_t)&cb_set_socket;
    }
#endif

    ISA_LOCK(s, flags);
    ret = (fn == NULL) ? -EINVAL : fn(s, arg);
    ISA_UNLOCK(s, flags);
    return ret;
} /* pcic_service */

/*====================================================================*/

#ifndef __MACOSX__
static int __init init_i82365(void)
{
#ifdef __LINUX__
    servinfo_t serv;
    CardServices(GetCardServicesInfo, &serv);
    if (serv.Revision != CS_RELEASE_CODE) {
	printk(KERN_NOTICE "i82365: Card Services release "
	       "does not match!\n");
	return -1;
    }
#endif
    DEBUG(0, "%s\n", version);
    printk(KERN_INFO "Intel PCIC probe: ");
    sockets = 0;

    ACQUIRE_RESOURCE_LOCK;

#ifdef CONFIG_PCI
    if (do_pci_probe && pcibios_present()) {
	pci_probe(PCI_CLASS_BRIDGE_CARDBUS);
	pci_probe(PCI_CLASS_BRIDGE_PCMCIA);
    }
#endif

#ifdef CONFIG_ISA
    isa_probe(i365_base);
    if (!sockets || extra_sockets)
	isa_probe(i365_base+2);
#endif

    RELEASE_RESOURCE_LOCK;

    if (sockets == 0) {
	printk("not found.\n");
	return -ENODEV;
    }

    /* Set up interrupt handler(s) */
#ifdef CONFIG_ISA
    if (grab_irq != 0)
	_request_irq(cs_irq, pcic_interrupt, 0, "i82365");
#endif
#ifdef CONFIG_PCI
    if (pci_csc) {
	u_int i, irq, mask = 0;
	for (i = 0; i < sockets; i++) {
	    irq = socket[i].cap.pci_irq;
	    if (irq && !(mask & (1<<irq)))
		_request_irq(irq, pcic_interrupt, SA_SHIRQ, "i82365");
	    mask |= (1<<irq);
	}
    }
#endif

    if (register_ss_entry(sockets, &pcic_service) != 0)
	printk(KERN_NOTICE "i82365: register_ss_entry() failed\n");

    /* Finally, schedule a polling interrupt */
    if (poll_interval != 0) {
    	poll_timer.expires = jiffies + poll_interval;
	add_timer(&poll_timer);
    }

    return 0;

} /* init_i82365 */

static void __exit exit_i82365(void)
{
    int i;
#ifdef HAS_PROC_BUS
    for (i = 0; i < sockets; i++)
	pcic_proc_remove(&socket[i]);
#endif
    unregister_ss_entry(&pcic_service);
    if (poll_interval != 0)
	del_timer(&poll_timer);
#ifdef CONFIG_ISA
    if (grab_irq != 0)
	_free_irq(cs_irq, pcic_interrupt);
#endif
#ifdef CONFIG_PCI
    if (pci_csc) {
	u_int irq, mask = 0;
	for (i = 0; i < sockets; i++) {
	    irq = socket[i].cap.pci_irq;
	    if (irq && !(mask & (1<<irq)))
		_free_irq(irq, pcic_interrupt);
	    mask |= (1<<irq);
	}
    }
#endif
    for (i = 0; i < sockets; i++) {
	socket_info_t *s = &socket[i];
	/* Turn off all interrupt sources! */
	i365_set(s, I365_CSCINT, 0);
#ifdef CONFIG_PCI
	if (s->flags & IS_CARDBUS)
	    cb_writel(s, CB_SOCKET_MASK, 0);
	if (s->cb_virt) {
	    iounmap(s->cb_virt);
	    release_mem_region(s->cb_phys, 0x1000);
	} else
#endif
	    release_region(s->ioaddr, 2);
    }
} /* exit_i82365 */
#endif

#ifdef __LINUX__

module_init(init_i82365);
module_exit(exit_i82365);

#endif

/*====================================================================*/

#ifdef __BEOS__

static status_t std_ops(int32 op)
{
    int ret;
    switch (op) {
    case B_MODULE_INIT:
	ret = get_module(CS_SOCKET_MODULE_NAME, (struct module_info **)&cs);
	if (ret != B_OK) return ret;
	ret = get_module(B_ISA_MODULE_NAME, (struct module_info **)&isa);
	if (ret != B_OK) return ret;
	ret = get_module(B_PCI_MODULE_NAME, (struct module_info **)&pci);
	if (ret != B_OK) return ret;
	return pcic_init();
	break;
    case B_MODULE_UNINIT:
	exit_i82365();
	if (pci) put_module(B_PCI_MODULE_NAME);
	if (isa) put_module(B_ISA_MODULE_NAME);
	if (cs) put_module(CS_SOCKET_MODULE_NAME);
	break;
    }
    return B_OK;
}

static module_info pcic_mod_info = {
    SS_MODULE_NAME("i82365"), 0, &std_ops
};

_EXPORT module_info *modules[] = {
    &pcic_mod_info,
    NULL
};

#endif


#ifdef __MACOSX__
int
init_i82365(IOPCCardBridge *pccard_nub, IOPCIDevice *bridge_nub, void * device_regs)
{
    int starting_socket = sockets;

    servinfo_t serv;
    CardServices(GetCardServicesInfo, &serv, NULL, NULL);
    if (serv.Revision != CS_RELEASE_CODE) {
	printk(KERN_NOTICE "i82365: Card Services release "
	       "does not match!\n");
	return -1;
    }
    DEBUG(0, "%s\n", version);
    printk(KERN_INFO "Intel PCIC probe: ");

    // for macosx this is all evil, the original code assumes it is
    // called once during init, and all the "sockets" are found at
    // once, unfortunately it doesn't work that way in macosx. in
    // macos9 there is the concept of logical socket indexes, CS deals
    // in logical sockets and SS deals in physical sockets.  This
    // makes the code equally ugly due the constant need to convert
    // back and forth between physical and logical.  For now at least,
    // this code needs make sure that the indexes in DS, CS, SS all
    // refer to same sockets.  The bridge object insures that this
    // code and the ds registration code get called synchronously.
    // this code registers with CS via register_ss_entry

    // setup the next socket so probe can do its thing
    socket[sockets].cap.pccard_nub = pccard_nub;
    socket[sockets].cap.bridge_nub = bridge_nub;
    socket[sockets].cb_virt = device_regs;
    
    pci_probe(PCI_CLASS_BRIDGE_CARDBUS);
//  pci_probe(PCI_CLASS_BRIDGE_PCMCIA);		// MACOSXXX - later   

    if (starting_socket == sockets) {
	printk("not found.\n");
	return -ENODEV;
    }

    /* Set up interrupt handler(s) */
#ifdef CONFIG_ISA
    if (grab_irq != 0)
	_request_irq(cs_irq, pcic_interrupt, 0, "i82365");
#endif
#ifdef CONFIG_PCI
    if (pci_csc) {
	u_int i, irq, mask = 0; socket_info_t *s;
	for (i = starting_socket; i < sockets; i++) {
	    s = &socket[i];
	    irq = socket[i].cap.pci_irq;
	    if (irq && !(mask & (1<<irq)))
//		_request_irq(irq, pcic_interrupt, SA_SHIRQ, "i82365");
		IOPCCardAddCSCInterruptHandlers(s->cap.pccard_nub, i, irq,
						pcic_interrupt_top, pcic_interrupt, 
						pcic_enable_functional_interrupt,
						pcic_disable_functional_interrupt, 
						"i82365");
	    mask |= (1<<irq);
	}
    }
#endif
    
    if (register_ss_entry(starting_socket, sockets, &pcic_service) != 0)
	printk(KERN_NOTICE "i82365: register_ss_entry() failed\n");

    if (!starting_socket) {    // poll_timer is a global
	/* Finally, schedule a polling interrupt */
	if (poll_interval != 0) {
	    poll_timer.function = pcic_interrupt_wrapper;
	    poll_timer.data = 0;
//	    poll_timer.prev = poll_timer.next = NULL;
	    poll_timer.expires = jiffies + poll_interval;
	    add_timer(&poll_timer);
	}
    }

    return 0;
    
} /* init_i82365 */
#endif /* MACOSX */
