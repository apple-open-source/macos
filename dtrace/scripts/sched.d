#pragma D depends_on module mach_kernel
#pragma D depends_on provider sched

struct _processor_info {
    int pi_state;           /* processor state, see above */
    char    pi_processor_type[32];  /* ASCII CPU type */
    char    pi_fputypes[32];    /* ASCII FPU types */
    int pi_clock;           /* CPU clock freq in MHz */
};

typedef struct _processor_info _processor_info_t;

typedef int chipid_t;
typedef int lgrp_id_t;

struct cpuinfo {
	processorid_t cpu_id;		/* CPU identifier */
	psetid_t cpu_pset;		/* processor set identifier */
	chipid_t cpu_chip;		/* chip identifier */
	lgrp_id_t cpu_lgrp;		/* locality group identifer */
	_processor_info_t cpu_info;	/* CPU information */
};

typedef struct cpuinfo cpuinfo_t;

translator cpuinfo_t < processor_t P > {
	cpu_id = P->cpu_num;
	cpu_pset = P->processor_set;
	cpu_chip = P->cpu_num; /* XXX */
	cpu_lgrp = 0; /* XXX */
	cpu_info = *((_processor_info_t *)`dtrace_zero); /* ` */ /* XXX */
}; 

inline cpuinfo_t *curcpu = xlate <cpuinfo_t *> (curthread->last_processor);
#pragma D attributes Stable/Stable/Common curcpu
#pragma D binding "1.0" curcpu

inline processorid_t cpu = curcpu->cpu_id;
#pragma D attributes Stable/Stable/Common cpu
#pragma D binding "1.0" cpu

inline psetid_t pset = curcpu->cpu_pset;
#pragma D attributes Stable/Stable/Common pset
#pragma D binding "1.0" pset

inline chipid_t chip = curcpu->cpu_chip;
#pragma D attributes Stable/Stable/Common chip
#pragma D binding "1.0" chip

inline lgrp_id_t lgrp = curcpu->cpu_lgrp;
#pragma D attributes Stable/Stable/Common lgrp
#pragma D binding "1.0" lgrp

