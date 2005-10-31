# APPLE LOCAL file 4099000
#define THUNK(REG)				\
.private_extern ___i686.get_pc_thunk.REG	;\
___i686.get_pc_thunk.REG:			;\
	movl    (%esp,1),%REG			;\
	ret					;
	
#ifdef L_get_pc_thunk_ax
THUNK(ax)
#endif
#ifdef L_get_pc_thunk_dx
THUNK(dx)
#endif
#ifdef L_get_pc_thunk_cx
THUNK(cx)
#endif
#ifdef L_get_pc_thunk_bx
THUNK(bx)
#endif
#ifdef L_get_pc_thunk_si
THUNK(si)
#endif
#ifdef L_get_pc_thunk_di
THUNK(di)
#endif
#ifdef L_get_pc_thunk_bp
THUNK(bp)
#endif
