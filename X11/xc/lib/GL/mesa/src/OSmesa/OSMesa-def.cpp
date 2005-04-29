LIBRARY libOSmesa
VERSION LIBRARY_VERSION
EXPORTS
	OSMesaCreateContext
	OSMesaDestroyContext
	OSMesaGetColorBuffer
	OSMesaGetCurrentContext
	OSMesaGetDepthBuffer
	OSMesaGetIntegerv
	OSMesaMakeCurrent
	OSMesaPixelStore
#ifndef __UNIXOS2__
	_glapi_Context 
	_glapi_noop_enable_warnings
	_glapi_add_entrypoint
	_glapi_get_dispatch_table_size
	_glapi_set_dispatch
	_glapi_check_multithread
	_glapi_set_context
	glTexCoordPointer
	glColorPointer
	glNormalPointer
	glVertexPointer
	glDrawElements
#else
	OSMesaCreateContextExt
#endif  /* __UNIXOS2__ */

/* $XFree86: xc/lib/GL/mesa/src/OSmesa/OSMesa-def.cpp,v 1.2 2003/03/25 04:18:08 dawes Exp $ */
