#ifdef __ppc__

#define N_FP_REGS 14
extern void ppc_fp_save(double *save_area);
extern void ppc_fp_restore(double *save_area);

#define N_VEC_REGS 18
extern void ppc_vec_save(unsigned long *save_area);
extern void ppc_vec_restore(unsigned long *save_area);

#endif /* __ppc__ */
