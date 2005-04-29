; NB: This file was obsoleted by the -freplace-objc-classes flag added
;     to gcc.

;
; This is a special section of flags to the ObjC runtime.
; The structure the objc runtime expects to see is this:
;
;	struct objc_image_info  {
;		uint32_t	version;	// initially 0
;		uint32_t	flags;	// 0'th bit means ignore classes and categories in this image
;	};
;

.section __OBJC, __image_info
	.align 2
	.long 0
	.long 1
