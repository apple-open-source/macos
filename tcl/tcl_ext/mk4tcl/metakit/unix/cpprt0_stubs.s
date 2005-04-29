
        .code
; stubs for static constructors in a.out
	.export __StaticCtorTable_Start,data
        .export __StaticCtorTable_End,data
__StaticCtorTable_Start
__StaticCtorTable_End

	.data
; stubs for static constructors in a.out, compiled with +z/+Z
	.export __ZStaticCtorTable_Start,data
        .export __ZStaticCtorTable_End,data
__ZStaticCtorTable_Start
__ZStaticCtorTable_End
