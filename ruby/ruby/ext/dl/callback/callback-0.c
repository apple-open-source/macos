#include "dl.h"

extern VALUE rb_DLCdeclCallbackAddrs, rb_DLCdeclCallbackProcs;
#ifdef FUNC_STDCALL
extern VALUE rb_DLStdcallCallbackAddrs, rb_DLStdcallCallbackProcs;
#endif
extern ID   rb_dl_cb_call;

static void
FUNC_CDECL(rb_dl_callback_void_0_0_cdecl)()
{
    VALUE cb;

    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 0);
    rb_funcall2(cb, rb_dl_cb_call, 0, NULL);
}


static void
FUNC_CDECL(rb_dl_callback_void_0_1_cdecl)()
{
    VALUE cb;

    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 20);
    rb_funcall2(cb, rb_dl_cb_call, 0, NULL);
}


static void
FUNC_CDECL(rb_dl_callback_void_0_2_cdecl)()
{
    VALUE cb;

    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 40);
    rb_funcall2(cb, rb_dl_cb_call, 0, NULL);
}


static void
FUNC_CDECL(rb_dl_callback_void_0_3_cdecl)()
{
    VALUE cb;

    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 60);
    rb_funcall2(cb, rb_dl_cb_call, 0, NULL);
}


static void
FUNC_CDECL(rb_dl_callback_void_0_4_cdecl)()
{
    VALUE cb;

    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 80);
    rb_funcall2(cb, rb_dl_cb_call, 0, NULL);
}


static void
FUNC_CDECL(rb_dl_callback_void_1_0_cdecl)(DLSTACK_TYPE stack0)
{
    VALUE cb, args[1];

    args[0] = PTR2NUM(stack0);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 1);
    rb_funcall2(cb, rb_dl_cb_call, 1, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_1_1_cdecl)(DLSTACK_TYPE stack0)
{
    VALUE cb, args[1];

    args[0] = PTR2NUM(stack0);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 21);
    rb_funcall2(cb, rb_dl_cb_call, 1, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_1_2_cdecl)(DLSTACK_TYPE stack0)
{
    VALUE cb, args[1];

    args[0] = PTR2NUM(stack0);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 41);
    rb_funcall2(cb, rb_dl_cb_call, 1, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_1_3_cdecl)(DLSTACK_TYPE stack0)
{
    VALUE cb, args[1];

    args[0] = PTR2NUM(stack0);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 61);
    rb_funcall2(cb, rb_dl_cb_call, 1, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_1_4_cdecl)(DLSTACK_TYPE stack0)
{
    VALUE cb, args[1];

    args[0] = PTR2NUM(stack0);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 81);
    rb_funcall2(cb, rb_dl_cb_call, 1, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_2_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1)
{
    VALUE cb, args[2];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 2);
    rb_funcall2(cb, rb_dl_cb_call, 2, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_2_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1)
{
    VALUE cb, args[2];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 22);
    rb_funcall2(cb, rb_dl_cb_call, 2, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_2_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1)
{
    VALUE cb, args[2];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 42);
    rb_funcall2(cb, rb_dl_cb_call, 2, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_2_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1)
{
    VALUE cb, args[2];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 62);
    rb_funcall2(cb, rb_dl_cb_call, 2, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_2_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1)
{
    VALUE cb, args[2];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 82);
    rb_funcall2(cb, rb_dl_cb_call, 2, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_3_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2)
{
    VALUE cb, args[3];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 3);
    rb_funcall2(cb, rb_dl_cb_call, 3, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_3_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2)
{
    VALUE cb, args[3];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 23);
    rb_funcall2(cb, rb_dl_cb_call, 3, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_3_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2)
{
    VALUE cb, args[3];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 43);
    rb_funcall2(cb, rb_dl_cb_call, 3, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_3_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2)
{
    VALUE cb, args[3];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 63);
    rb_funcall2(cb, rb_dl_cb_call, 3, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_3_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2)
{
    VALUE cb, args[3];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 83);
    rb_funcall2(cb, rb_dl_cb_call, 3, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_4_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3)
{
    VALUE cb, args[4];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 4);
    rb_funcall2(cb, rb_dl_cb_call, 4, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_4_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3)
{
    VALUE cb, args[4];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 24);
    rb_funcall2(cb, rb_dl_cb_call, 4, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_4_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3)
{
    VALUE cb, args[4];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 44);
    rb_funcall2(cb, rb_dl_cb_call, 4, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_4_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3)
{
    VALUE cb, args[4];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 64);
    rb_funcall2(cb, rb_dl_cb_call, 4, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_4_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3)
{
    VALUE cb, args[4];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 84);
    rb_funcall2(cb, rb_dl_cb_call, 4, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_5_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4)
{
    VALUE cb, args[5];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 5);
    rb_funcall2(cb, rb_dl_cb_call, 5, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_5_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4)
{
    VALUE cb, args[5];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 25);
    rb_funcall2(cb, rb_dl_cb_call, 5, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_5_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4)
{
    VALUE cb, args[5];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 45);
    rb_funcall2(cb, rb_dl_cb_call, 5, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_5_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4)
{
    VALUE cb, args[5];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 65);
    rb_funcall2(cb, rb_dl_cb_call, 5, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_5_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4)
{
    VALUE cb, args[5];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 85);
    rb_funcall2(cb, rb_dl_cb_call, 5, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_6_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5)
{
    VALUE cb, args[6];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 6);
    rb_funcall2(cb, rb_dl_cb_call, 6, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_6_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5)
{
    VALUE cb, args[6];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 26);
    rb_funcall2(cb, rb_dl_cb_call, 6, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_6_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5)
{
    VALUE cb, args[6];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 46);
    rb_funcall2(cb, rb_dl_cb_call, 6, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_6_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5)
{
    VALUE cb, args[6];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 66);
    rb_funcall2(cb, rb_dl_cb_call, 6, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_6_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5)
{
    VALUE cb, args[6];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 86);
    rb_funcall2(cb, rb_dl_cb_call, 6, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_7_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6)
{
    VALUE cb, args[7];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 7);
    rb_funcall2(cb, rb_dl_cb_call, 7, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_7_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6)
{
    VALUE cb, args[7];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 27);
    rb_funcall2(cb, rb_dl_cb_call, 7, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_7_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6)
{
    VALUE cb, args[7];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 47);
    rb_funcall2(cb, rb_dl_cb_call, 7, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_7_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6)
{
    VALUE cb, args[7];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 67);
    rb_funcall2(cb, rb_dl_cb_call, 7, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_7_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6)
{
    VALUE cb, args[7];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 87);
    rb_funcall2(cb, rb_dl_cb_call, 7, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_8_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7)
{
    VALUE cb, args[8];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 8);
    rb_funcall2(cb, rb_dl_cb_call, 8, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_8_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7)
{
    VALUE cb, args[8];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 28);
    rb_funcall2(cb, rb_dl_cb_call, 8, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_8_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7)
{
    VALUE cb, args[8];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 48);
    rb_funcall2(cb, rb_dl_cb_call, 8, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_8_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7)
{
    VALUE cb, args[8];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 68);
    rb_funcall2(cb, rb_dl_cb_call, 8, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_8_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7)
{
    VALUE cb, args[8];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 88);
    rb_funcall2(cb, rb_dl_cb_call, 8, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_9_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8)
{
    VALUE cb, args[9];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 9);
    rb_funcall2(cb, rb_dl_cb_call, 9, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_9_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8)
{
    VALUE cb, args[9];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 29);
    rb_funcall2(cb, rb_dl_cb_call, 9, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_9_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8)
{
    VALUE cb, args[9];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 49);
    rb_funcall2(cb, rb_dl_cb_call, 9, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_9_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8)
{
    VALUE cb, args[9];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 69);
    rb_funcall2(cb, rb_dl_cb_call, 9, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_9_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8)
{
    VALUE cb, args[9];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 89);
    rb_funcall2(cb, rb_dl_cb_call, 9, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_10_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9)
{
    VALUE cb, args[10];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 10);
    rb_funcall2(cb, rb_dl_cb_call, 10, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_10_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9)
{
    VALUE cb, args[10];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 30);
    rb_funcall2(cb, rb_dl_cb_call, 10, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_10_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9)
{
    VALUE cb, args[10];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 50);
    rb_funcall2(cb, rb_dl_cb_call, 10, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_10_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9)
{
    VALUE cb, args[10];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 70);
    rb_funcall2(cb, rb_dl_cb_call, 10, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_10_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9)
{
    VALUE cb, args[10];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 90);
    rb_funcall2(cb, rb_dl_cb_call, 10, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_11_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10)
{
    VALUE cb, args[11];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 11);
    rb_funcall2(cb, rb_dl_cb_call, 11, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_11_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10)
{
    VALUE cb, args[11];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 31);
    rb_funcall2(cb, rb_dl_cb_call, 11, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_11_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10)
{
    VALUE cb, args[11];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 51);
    rb_funcall2(cb, rb_dl_cb_call, 11, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_11_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10)
{
    VALUE cb, args[11];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 71);
    rb_funcall2(cb, rb_dl_cb_call, 11, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_11_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10)
{
    VALUE cb, args[11];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 91);
    rb_funcall2(cb, rb_dl_cb_call, 11, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_12_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11)
{
    VALUE cb, args[12];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 12);
    rb_funcall2(cb, rb_dl_cb_call, 12, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_12_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11)
{
    VALUE cb, args[12];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 32);
    rb_funcall2(cb, rb_dl_cb_call, 12, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_12_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11)
{
    VALUE cb, args[12];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 52);
    rb_funcall2(cb, rb_dl_cb_call, 12, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_12_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11)
{
    VALUE cb, args[12];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 72);
    rb_funcall2(cb, rb_dl_cb_call, 12, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_12_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11)
{
    VALUE cb, args[12];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 92);
    rb_funcall2(cb, rb_dl_cb_call, 12, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_13_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12)
{
    VALUE cb, args[13];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 13);
    rb_funcall2(cb, rb_dl_cb_call, 13, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_13_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12)
{
    VALUE cb, args[13];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 33);
    rb_funcall2(cb, rb_dl_cb_call, 13, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_13_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12)
{
    VALUE cb, args[13];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 53);
    rb_funcall2(cb, rb_dl_cb_call, 13, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_13_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12)
{
    VALUE cb, args[13];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 73);
    rb_funcall2(cb, rb_dl_cb_call, 13, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_13_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12)
{
    VALUE cb, args[13];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 93);
    rb_funcall2(cb, rb_dl_cb_call, 13, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_14_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13)
{
    VALUE cb, args[14];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 14);
    rb_funcall2(cb, rb_dl_cb_call, 14, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_14_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13)
{
    VALUE cb, args[14];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 34);
    rb_funcall2(cb, rb_dl_cb_call, 14, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_14_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13)
{
    VALUE cb, args[14];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 54);
    rb_funcall2(cb, rb_dl_cb_call, 14, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_14_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13)
{
    VALUE cb, args[14];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 74);
    rb_funcall2(cb, rb_dl_cb_call, 14, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_14_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13)
{
    VALUE cb, args[14];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 94);
    rb_funcall2(cb, rb_dl_cb_call, 14, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_15_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14)
{
    VALUE cb, args[15];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 15);
    rb_funcall2(cb, rb_dl_cb_call, 15, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_15_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14)
{
    VALUE cb, args[15];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 35);
    rb_funcall2(cb, rb_dl_cb_call, 15, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_15_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14)
{
    VALUE cb, args[15];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 55);
    rb_funcall2(cb, rb_dl_cb_call, 15, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_15_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14)
{
    VALUE cb, args[15];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 75);
    rb_funcall2(cb, rb_dl_cb_call, 15, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_15_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14)
{
    VALUE cb, args[15];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 95);
    rb_funcall2(cb, rb_dl_cb_call, 15, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_16_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15)
{
    VALUE cb, args[16];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 16);
    rb_funcall2(cb, rb_dl_cb_call, 16, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_16_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15)
{
    VALUE cb, args[16];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 36);
    rb_funcall2(cb, rb_dl_cb_call, 16, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_16_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15)
{
    VALUE cb, args[16];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 56);
    rb_funcall2(cb, rb_dl_cb_call, 16, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_16_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15)
{
    VALUE cb, args[16];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 76);
    rb_funcall2(cb, rb_dl_cb_call, 16, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_16_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15)
{
    VALUE cb, args[16];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 96);
    rb_funcall2(cb, rb_dl_cb_call, 16, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_17_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16)
{
    VALUE cb, args[17];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 17);
    rb_funcall2(cb, rb_dl_cb_call, 17, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_17_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16)
{
    VALUE cb, args[17];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 37);
    rb_funcall2(cb, rb_dl_cb_call, 17, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_17_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16)
{
    VALUE cb, args[17];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 57);
    rb_funcall2(cb, rb_dl_cb_call, 17, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_17_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16)
{
    VALUE cb, args[17];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 77);
    rb_funcall2(cb, rb_dl_cb_call, 17, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_17_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16)
{
    VALUE cb, args[17];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 97);
    rb_funcall2(cb, rb_dl_cb_call, 17, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_18_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17)
{
    VALUE cb, args[18];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 18);
    rb_funcall2(cb, rb_dl_cb_call, 18, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_18_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17)
{
    VALUE cb, args[18];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 38);
    rb_funcall2(cb, rb_dl_cb_call, 18, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_18_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17)
{
    VALUE cb, args[18];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 58);
    rb_funcall2(cb, rb_dl_cb_call, 18, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_18_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17)
{
    VALUE cb, args[18];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 78);
    rb_funcall2(cb, rb_dl_cb_call, 18, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_18_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17)
{
    VALUE cb, args[18];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 98);
    rb_funcall2(cb, rb_dl_cb_call, 18, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_19_0_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17, DLSTACK_TYPE stack18)
{
    VALUE cb, args[19];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    args[18] = PTR2NUM(stack18);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 19);
    rb_funcall2(cb, rb_dl_cb_call, 19, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_19_1_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17, DLSTACK_TYPE stack18)
{
    VALUE cb, args[19];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    args[18] = PTR2NUM(stack18);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 39);
    rb_funcall2(cb, rb_dl_cb_call, 19, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_19_2_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17, DLSTACK_TYPE stack18)
{
    VALUE cb, args[19];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    args[18] = PTR2NUM(stack18);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 59);
    rb_funcall2(cb, rb_dl_cb_call, 19, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_19_3_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17, DLSTACK_TYPE stack18)
{
    VALUE cb, args[19];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    args[18] = PTR2NUM(stack18);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 79);
    rb_funcall2(cb, rb_dl_cb_call, 19, args);
}


static void
FUNC_CDECL(rb_dl_callback_void_19_4_cdecl)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17, DLSTACK_TYPE stack18)
{
    VALUE cb, args[19];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    args[18] = PTR2NUM(stack18);
    cb = rb_ary_entry(rb_ary_entry(rb_DLCdeclCallbackProcs, 0), 99);
    rb_funcall2(cb, rb_dl_cb_call, 19, args);
}


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_0_0_stdcall)()
{
    VALUE cb;

    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 0);
    rb_funcall2(cb, rb_dl_cb_call, 0, NULL);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_0_1_stdcall)()
{
    VALUE cb;

    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 20);
    rb_funcall2(cb, rb_dl_cb_call, 0, NULL);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_0_2_stdcall)()
{
    VALUE cb;

    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 40);
    rb_funcall2(cb, rb_dl_cb_call, 0, NULL);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_0_3_stdcall)()
{
    VALUE cb;

    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 60);
    rb_funcall2(cb, rb_dl_cb_call, 0, NULL);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_0_4_stdcall)()
{
    VALUE cb;

    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 80);
    rb_funcall2(cb, rb_dl_cb_call, 0, NULL);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_1_0_stdcall)(DLSTACK_TYPE stack0)
{
    VALUE cb, args[1];

    args[0] = PTR2NUM(stack0);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 1);
    rb_funcall2(cb, rb_dl_cb_call, 1, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_1_1_stdcall)(DLSTACK_TYPE stack0)
{
    VALUE cb, args[1];

    args[0] = PTR2NUM(stack0);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 21);
    rb_funcall2(cb, rb_dl_cb_call, 1, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_1_2_stdcall)(DLSTACK_TYPE stack0)
{
    VALUE cb, args[1];

    args[0] = PTR2NUM(stack0);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 41);
    rb_funcall2(cb, rb_dl_cb_call, 1, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_1_3_stdcall)(DLSTACK_TYPE stack0)
{
    VALUE cb, args[1];

    args[0] = PTR2NUM(stack0);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 61);
    rb_funcall2(cb, rb_dl_cb_call, 1, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_1_4_stdcall)(DLSTACK_TYPE stack0)
{
    VALUE cb, args[1];

    args[0] = PTR2NUM(stack0);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 81);
    rb_funcall2(cb, rb_dl_cb_call, 1, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_2_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1)
{
    VALUE cb, args[2];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 2);
    rb_funcall2(cb, rb_dl_cb_call, 2, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_2_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1)
{
    VALUE cb, args[2];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 22);
    rb_funcall2(cb, rb_dl_cb_call, 2, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_2_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1)
{
    VALUE cb, args[2];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 42);
    rb_funcall2(cb, rb_dl_cb_call, 2, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_2_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1)
{
    VALUE cb, args[2];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 62);
    rb_funcall2(cb, rb_dl_cb_call, 2, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_2_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1)
{
    VALUE cb, args[2];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 82);
    rb_funcall2(cb, rb_dl_cb_call, 2, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_3_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2)
{
    VALUE cb, args[3];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 3);
    rb_funcall2(cb, rb_dl_cb_call, 3, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_3_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2)
{
    VALUE cb, args[3];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 23);
    rb_funcall2(cb, rb_dl_cb_call, 3, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_3_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2)
{
    VALUE cb, args[3];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 43);
    rb_funcall2(cb, rb_dl_cb_call, 3, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_3_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2)
{
    VALUE cb, args[3];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 63);
    rb_funcall2(cb, rb_dl_cb_call, 3, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_3_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2)
{
    VALUE cb, args[3];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 83);
    rb_funcall2(cb, rb_dl_cb_call, 3, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_4_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3)
{
    VALUE cb, args[4];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 4);
    rb_funcall2(cb, rb_dl_cb_call, 4, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_4_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3)
{
    VALUE cb, args[4];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 24);
    rb_funcall2(cb, rb_dl_cb_call, 4, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_4_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3)
{
    VALUE cb, args[4];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 44);
    rb_funcall2(cb, rb_dl_cb_call, 4, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_4_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3)
{
    VALUE cb, args[4];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 64);
    rb_funcall2(cb, rb_dl_cb_call, 4, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_4_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3)
{
    VALUE cb, args[4];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 84);
    rb_funcall2(cb, rb_dl_cb_call, 4, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_5_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4)
{
    VALUE cb, args[5];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 5);
    rb_funcall2(cb, rb_dl_cb_call, 5, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_5_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4)
{
    VALUE cb, args[5];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 25);
    rb_funcall2(cb, rb_dl_cb_call, 5, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_5_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4)
{
    VALUE cb, args[5];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 45);
    rb_funcall2(cb, rb_dl_cb_call, 5, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_5_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4)
{
    VALUE cb, args[5];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 65);
    rb_funcall2(cb, rb_dl_cb_call, 5, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_5_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4)
{
    VALUE cb, args[5];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 85);
    rb_funcall2(cb, rb_dl_cb_call, 5, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_6_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5)
{
    VALUE cb, args[6];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 6);
    rb_funcall2(cb, rb_dl_cb_call, 6, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_6_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5)
{
    VALUE cb, args[6];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 26);
    rb_funcall2(cb, rb_dl_cb_call, 6, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_6_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5)
{
    VALUE cb, args[6];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 46);
    rb_funcall2(cb, rb_dl_cb_call, 6, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_6_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5)
{
    VALUE cb, args[6];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 66);
    rb_funcall2(cb, rb_dl_cb_call, 6, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_6_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5)
{
    VALUE cb, args[6];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 86);
    rb_funcall2(cb, rb_dl_cb_call, 6, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_7_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6)
{
    VALUE cb, args[7];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 7);
    rb_funcall2(cb, rb_dl_cb_call, 7, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_7_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6)
{
    VALUE cb, args[7];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 27);
    rb_funcall2(cb, rb_dl_cb_call, 7, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_7_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6)
{
    VALUE cb, args[7];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 47);
    rb_funcall2(cb, rb_dl_cb_call, 7, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_7_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6)
{
    VALUE cb, args[7];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 67);
    rb_funcall2(cb, rb_dl_cb_call, 7, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_7_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6)
{
    VALUE cb, args[7];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 87);
    rb_funcall2(cb, rb_dl_cb_call, 7, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_8_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7)
{
    VALUE cb, args[8];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 8);
    rb_funcall2(cb, rb_dl_cb_call, 8, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_8_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7)
{
    VALUE cb, args[8];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 28);
    rb_funcall2(cb, rb_dl_cb_call, 8, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_8_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7)
{
    VALUE cb, args[8];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 48);
    rb_funcall2(cb, rb_dl_cb_call, 8, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_8_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7)
{
    VALUE cb, args[8];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 68);
    rb_funcall2(cb, rb_dl_cb_call, 8, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_8_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7)
{
    VALUE cb, args[8];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 88);
    rb_funcall2(cb, rb_dl_cb_call, 8, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_9_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8)
{
    VALUE cb, args[9];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 9);
    rb_funcall2(cb, rb_dl_cb_call, 9, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_9_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8)
{
    VALUE cb, args[9];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 29);
    rb_funcall2(cb, rb_dl_cb_call, 9, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_9_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8)
{
    VALUE cb, args[9];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 49);
    rb_funcall2(cb, rb_dl_cb_call, 9, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_9_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8)
{
    VALUE cb, args[9];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 69);
    rb_funcall2(cb, rb_dl_cb_call, 9, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_9_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8)
{
    VALUE cb, args[9];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 89);
    rb_funcall2(cb, rb_dl_cb_call, 9, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_10_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9)
{
    VALUE cb, args[10];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 10);
    rb_funcall2(cb, rb_dl_cb_call, 10, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_10_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9)
{
    VALUE cb, args[10];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 30);
    rb_funcall2(cb, rb_dl_cb_call, 10, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_10_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9)
{
    VALUE cb, args[10];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 50);
    rb_funcall2(cb, rb_dl_cb_call, 10, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_10_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9)
{
    VALUE cb, args[10];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 70);
    rb_funcall2(cb, rb_dl_cb_call, 10, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_10_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9)
{
    VALUE cb, args[10];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 90);
    rb_funcall2(cb, rb_dl_cb_call, 10, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_11_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10)
{
    VALUE cb, args[11];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 11);
    rb_funcall2(cb, rb_dl_cb_call, 11, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_11_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10)
{
    VALUE cb, args[11];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 31);
    rb_funcall2(cb, rb_dl_cb_call, 11, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_11_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10)
{
    VALUE cb, args[11];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 51);
    rb_funcall2(cb, rb_dl_cb_call, 11, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_11_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10)
{
    VALUE cb, args[11];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 71);
    rb_funcall2(cb, rb_dl_cb_call, 11, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_11_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10)
{
    VALUE cb, args[11];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 91);
    rb_funcall2(cb, rb_dl_cb_call, 11, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_12_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11)
{
    VALUE cb, args[12];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 12);
    rb_funcall2(cb, rb_dl_cb_call, 12, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_12_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11)
{
    VALUE cb, args[12];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 32);
    rb_funcall2(cb, rb_dl_cb_call, 12, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_12_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11)
{
    VALUE cb, args[12];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 52);
    rb_funcall2(cb, rb_dl_cb_call, 12, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_12_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11)
{
    VALUE cb, args[12];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 72);
    rb_funcall2(cb, rb_dl_cb_call, 12, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_12_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11)
{
    VALUE cb, args[12];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 92);
    rb_funcall2(cb, rb_dl_cb_call, 12, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_13_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12)
{
    VALUE cb, args[13];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 13);
    rb_funcall2(cb, rb_dl_cb_call, 13, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_13_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12)
{
    VALUE cb, args[13];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 33);
    rb_funcall2(cb, rb_dl_cb_call, 13, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_13_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12)
{
    VALUE cb, args[13];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 53);
    rb_funcall2(cb, rb_dl_cb_call, 13, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_13_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12)
{
    VALUE cb, args[13];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 73);
    rb_funcall2(cb, rb_dl_cb_call, 13, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_13_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12)
{
    VALUE cb, args[13];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 93);
    rb_funcall2(cb, rb_dl_cb_call, 13, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_14_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13)
{
    VALUE cb, args[14];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 14);
    rb_funcall2(cb, rb_dl_cb_call, 14, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_14_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13)
{
    VALUE cb, args[14];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 34);
    rb_funcall2(cb, rb_dl_cb_call, 14, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_14_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13)
{
    VALUE cb, args[14];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 54);
    rb_funcall2(cb, rb_dl_cb_call, 14, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_14_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13)
{
    VALUE cb, args[14];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 74);
    rb_funcall2(cb, rb_dl_cb_call, 14, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_14_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13)
{
    VALUE cb, args[14];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 94);
    rb_funcall2(cb, rb_dl_cb_call, 14, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_15_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14)
{
    VALUE cb, args[15];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 15);
    rb_funcall2(cb, rb_dl_cb_call, 15, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_15_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14)
{
    VALUE cb, args[15];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 35);
    rb_funcall2(cb, rb_dl_cb_call, 15, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_15_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14)
{
    VALUE cb, args[15];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 55);
    rb_funcall2(cb, rb_dl_cb_call, 15, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_15_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14)
{
    VALUE cb, args[15];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 75);
    rb_funcall2(cb, rb_dl_cb_call, 15, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_15_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14)
{
    VALUE cb, args[15];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 95);
    rb_funcall2(cb, rb_dl_cb_call, 15, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_16_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15)
{
    VALUE cb, args[16];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 16);
    rb_funcall2(cb, rb_dl_cb_call, 16, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_16_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15)
{
    VALUE cb, args[16];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 36);
    rb_funcall2(cb, rb_dl_cb_call, 16, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_16_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15)
{
    VALUE cb, args[16];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 56);
    rb_funcall2(cb, rb_dl_cb_call, 16, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_16_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15)
{
    VALUE cb, args[16];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 76);
    rb_funcall2(cb, rb_dl_cb_call, 16, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_16_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15)
{
    VALUE cb, args[16];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 96);
    rb_funcall2(cb, rb_dl_cb_call, 16, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_17_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16)
{
    VALUE cb, args[17];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 17);
    rb_funcall2(cb, rb_dl_cb_call, 17, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_17_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16)
{
    VALUE cb, args[17];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 37);
    rb_funcall2(cb, rb_dl_cb_call, 17, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_17_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16)
{
    VALUE cb, args[17];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 57);
    rb_funcall2(cb, rb_dl_cb_call, 17, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_17_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16)
{
    VALUE cb, args[17];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 77);
    rb_funcall2(cb, rb_dl_cb_call, 17, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_17_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16)
{
    VALUE cb, args[17];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 97);
    rb_funcall2(cb, rb_dl_cb_call, 17, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_18_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17)
{
    VALUE cb, args[18];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 18);
    rb_funcall2(cb, rb_dl_cb_call, 18, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_18_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17)
{
    VALUE cb, args[18];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 38);
    rb_funcall2(cb, rb_dl_cb_call, 18, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_18_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17)
{
    VALUE cb, args[18];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 58);
    rb_funcall2(cb, rb_dl_cb_call, 18, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_18_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17)
{
    VALUE cb, args[18];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 78);
    rb_funcall2(cb, rb_dl_cb_call, 18, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_18_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17)
{
    VALUE cb, args[18];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 98);
    rb_funcall2(cb, rb_dl_cb_call, 18, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_19_0_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17, DLSTACK_TYPE stack18)
{
    VALUE cb, args[19];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    args[18] = PTR2NUM(stack18);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 19);
    rb_funcall2(cb, rb_dl_cb_call, 19, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_19_1_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17, DLSTACK_TYPE stack18)
{
    VALUE cb, args[19];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    args[18] = PTR2NUM(stack18);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 39);
    rb_funcall2(cb, rb_dl_cb_call, 19, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_19_2_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17, DLSTACK_TYPE stack18)
{
    VALUE cb, args[19];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    args[18] = PTR2NUM(stack18);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 59);
    rb_funcall2(cb, rb_dl_cb_call, 19, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_19_3_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17, DLSTACK_TYPE stack18)
{
    VALUE cb, args[19];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    args[18] = PTR2NUM(stack18);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 79);
    rb_funcall2(cb, rb_dl_cb_call, 19, args);
}
#endif


#ifdef FUNC_STDCALL
static void
FUNC_STDCALL(rb_dl_callback_void_19_4_stdcall)(DLSTACK_TYPE stack0, DLSTACK_TYPE stack1, DLSTACK_TYPE stack2, DLSTACK_TYPE stack3, DLSTACK_TYPE stack4, DLSTACK_TYPE stack5, DLSTACK_TYPE stack6, DLSTACK_TYPE stack7, DLSTACK_TYPE stack8, DLSTACK_TYPE stack9, DLSTACK_TYPE stack10, DLSTACK_TYPE stack11, DLSTACK_TYPE stack12, DLSTACK_TYPE stack13, DLSTACK_TYPE stack14, DLSTACK_TYPE stack15, DLSTACK_TYPE stack16, DLSTACK_TYPE stack17, DLSTACK_TYPE stack18)
{
    VALUE cb, args[19];

    args[0] = PTR2NUM(stack0);
    args[1] = PTR2NUM(stack1);
    args[2] = PTR2NUM(stack2);
    args[3] = PTR2NUM(stack3);
    args[4] = PTR2NUM(stack4);
    args[5] = PTR2NUM(stack5);
    args[6] = PTR2NUM(stack6);
    args[7] = PTR2NUM(stack7);
    args[8] = PTR2NUM(stack8);
    args[9] = PTR2NUM(stack9);
    args[10] = PTR2NUM(stack10);
    args[11] = PTR2NUM(stack11);
    args[12] = PTR2NUM(stack12);
    args[13] = PTR2NUM(stack13);
    args[14] = PTR2NUM(stack14);
    args[15] = PTR2NUM(stack15);
    args[16] = PTR2NUM(stack16);
    args[17] = PTR2NUM(stack17);
    args[18] = PTR2NUM(stack18);
    cb = rb_ary_entry(rb_ary_entry(rb_DLStdcallCallbackProcs, 0), 99);
    rb_funcall2(cb, rb_dl_cb_call, 19, args);
}
#endif

void
rb_dl_init_callbacks_0()
{
    rb_ary_push(rb_DLCdeclCallbackProcs, rb_ary_new3(100,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil));
    rb_ary_push(rb_DLCdeclCallbackAddrs, rb_ary_new3(100,PTR2NUM(rb_dl_callback_void_0_0_cdecl),PTR2NUM(rb_dl_callback_void_1_0_cdecl),PTR2NUM(rb_dl_callback_void_2_0_cdecl),PTR2NUM(rb_dl_callback_void_3_0_cdecl),PTR2NUM(rb_dl_callback_void_4_0_cdecl),PTR2NUM(rb_dl_callback_void_5_0_cdecl),PTR2NUM(rb_dl_callback_void_6_0_cdecl),PTR2NUM(rb_dl_callback_void_7_0_cdecl),PTR2NUM(rb_dl_callback_void_8_0_cdecl),PTR2NUM(rb_dl_callback_void_9_0_cdecl),PTR2NUM(rb_dl_callback_void_10_0_cdecl),PTR2NUM(rb_dl_callback_void_11_0_cdecl),PTR2NUM(rb_dl_callback_void_12_0_cdecl),PTR2NUM(rb_dl_callback_void_13_0_cdecl),PTR2NUM(rb_dl_callback_void_14_0_cdecl),PTR2NUM(rb_dl_callback_void_15_0_cdecl),PTR2NUM(rb_dl_callback_void_16_0_cdecl),PTR2NUM(rb_dl_callback_void_17_0_cdecl),PTR2NUM(rb_dl_callback_void_18_0_cdecl),PTR2NUM(rb_dl_callback_void_19_0_cdecl),PTR2NUM(rb_dl_callback_void_0_1_cdecl),PTR2NUM(rb_dl_callback_void_1_1_cdecl),PTR2NUM(rb_dl_callback_void_2_1_cdecl),PTR2NUM(rb_dl_callback_void_3_1_cdecl),PTR2NUM(rb_dl_callback_void_4_1_cdecl),PTR2NUM(rb_dl_callback_void_5_1_cdecl),PTR2NUM(rb_dl_callback_void_6_1_cdecl),PTR2NUM(rb_dl_callback_void_7_1_cdecl),PTR2NUM(rb_dl_callback_void_8_1_cdecl),PTR2NUM(rb_dl_callback_void_9_1_cdecl),PTR2NUM(rb_dl_callback_void_10_1_cdecl),PTR2NUM(rb_dl_callback_void_11_1_cdecl),PTR2NUM(rb_dl_callback_void_12_1_cdecl),PTR2NUM(rb_dl_callback_void_13_1_cdecl),PTR2NUM(rb_dl_callback_void_14_1_cdecl),PTR2NUM(rb_dl_callback_void_15_1_cdecl),PTR2NUM(rb_dl_callback_void_16_1_cdecl),PTR2NUM(rb_dl_callback_void_17_1_cdecl),PTR2NUM(rb_dl_callback_void_18_1_cdecl),PTR2NUM(rb_dl_callback_void_19_1_cdecl),PTR2NUM(rb_dl_callback_void_0_2_cdecl),PTR2NUM(rb_dl_callback_void_1_2_cdecl),PTR2NUM(rb_dl_callback_void_2_2_cdecl),PTR2NUM(rb_dl_callback_void_3_2_cdecl),PTR2NUM(rb_dl_callback_void_4_2_cdecl),PTR2NUM(rb_dl_callback_void_5_2_cdecl),PTR2NUM(rb_dl_callback_void_6_2_cdecl),PTR2NUM(rb_dl_callback_void_7_2_cdecl),PTR2NUM(rb_dl_callback_void_8_2_cdecl),PTR2NUM(rb_dl_callback_void_9_2_cdecl),PTR2NUM(rb_dl_callback_void_10_2_cdecl),PTR2NUM(rb_dl_callback_void_11_2_cdecl),PTR2NUM(rb_dl_callback_void_12_2_cdecl),PTR2NUM(rb_dl_callback_void_13_2_cdecl),PTR2NUM(rb_dl_callback_void_14_2_cdecl),PTR2NUM(rb_dl_callback_void_15_2_cdecl),PTR2NUM(rb_dl_callback_void_16_2_cdecl),PTR2NUM(rb_dl_callback_void_17_2_cdecl),PTR2NUM(rb_dl_callback_void_18_2_cdecl),PTR2NUM(rb_dl_callback_void_19_2_cdecl),PTR2NUM(rb_dl_callback_void_0_3_cdecl),PTR2NUM(rb_dl_callback_void_1_3_cdecl),PTR2NUM(rb_dl_callback_void_2_3_cdecl),PTR2NUM(rb_dl_callback_void_3_3_cdecl),PTR2NUM(rb_dl_callback_void_4_3_cdecl),PTR2NUM(rb_dl_callback_void_5_3_cdecl),PTR2NUM(rb_dl_callback_void_6_3_cdecl),PTR2NUM(rb_dl_callback_void_7_3_cdecl),PTR2NUM(rb_dl_callback_void_8_3_cdecl),PTR2NUM(rb_dl_callback_void_9_3_cdecl),PTR2NUM(rb_dl_callback_void_10_3_cdecl),PTR2NUM(rb_dl_callback_void_11_3_cdecl),PTR2NUM(rb_dl_callback_void_12_3_cdecl),PTR2NUM(rb_dl_callback_void_13_3_cdecl),PTR2NUM(rb_dl_callback_void_14_3_cdecl),PTR2NUM(rb_dl_callback_void_15_3_cdecl),PTR2NUM(rb_dl_callback_void_16_3_cdecl),PTR2NUM(rb_dl_callback_void_17_3_cdecl),PTR2NUM(rb_dl_callback_void_18_3_cdecl),PTR2NUM(rb_dl_callback_void_19_3_cdecl),PTR2NUM(rb_dl_callback_void_0_4_cdecl),PTR2NUM(rb_dl_callback_void_1_4_cdecl),PTR2NUM(rb_dl_callback_void_2_4_cdecl),PTR2NUM(rb_dl_callback_void_3_4_cdecl),PTR2NUM(rb_dl_callback_void_4_4_cdecl),PTR2NUM(rb_dl_callback_void_5_4_cdecl),PTR2NUM(rb_dl_callback_void_6_4_cdecl),PTR2NUM(rb_dl_callback_void_7_4_cdecl),PTR2NUM(rb_dl_callback_void_8_4_cdecl),PTR2NUM(rb_dl_callback_void_9_4_cdecl),PTR2NUM(rb_dl_callback_void_10_4_cdecl),PTR2NUM(rb_dl_callback_void_11_4_cdecl),PTR2NUM(rb_dl_callback_void_12_4_cdecl),PTR2NUM(rb_dl_callback_void_13_4_cdecl),PTR2NUM(rb_dl_callback_void_14_4_cdecl),PTR2NUM(rb_dl_callback_void_15_4_cdecl),PTR2NUM(rb_dl_callback_void_16_4_cdecl),PTR2NUM(rb_dl_callback_void_17_4_cdecl),PTR2NUM(rb_dl_callback_void_18_4_cdecl),PTR2NUM(rb_dl_callback_void_19_4_cdecl)));
#ifdef FUNC_STDCALL
    rb_ary_push(rb_DLStdcallCallbackProcs, rb_ary_new3(100,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil));
    rb_ary_push(rb_DLStdcallCallbackAddrs, rb_ary_new3(100,PTR2NUM(rb_dl_callback_void_0_0_stdcall),PTR2NUM(rb_dl_callback_void_1_0_stdcall),PTR2NUM(rb_dl_callback_void_2_0_stdcall),PTR2NUM(rb_dl_callback_void_3_0_stdcall),PTR2NUM(rb_dl_callback_void_4_0_stdcall),PTR2NUM(rb_dl_callback_void_5_0_stdcall),PTR2NUM(rb_dl_callback_void_6_0_stdcall),PTR2NUM(rb_dl_callback_void_7_0_stdcall),PTR2NUM(rb_dl_callback_void_8_0_stdcall),PTR2NUM(rb_dl_callback_void_9_0_stdcall),PTR2NUM(rb_dl_callback_void_10_0_stdcall),PTR2NUM(rb_dl_callback_void_11_0_stdcall),PTR2NUM(rb_dl_callback_void_12_0_stdcall),PTR2NUM(rb_dl_callback_void_13_0_stdcall),PTR2NUM(rb_dl_callback_void_14_0_stdcall),PTR2NUM(rb_dl_callback_void_15_0_stdcall),PTR2NUM(rb_dl_callback_void_16_0_stdcall),PTR2NUM(rb_dl_callback_void_17_0_stdcall),PTR2NUM(rb_dl_callback_void_18_0_stdcall),PTR2NUM(rb_dl_callback_void_19_0_stdcall),PTR2NUM(rb_dl_callback_void_0_1_stdcall),PTR2NUM(rb_dl_callback_void_1_1_stdcall),PTR2NUM(rb_dl_callback_void_2_1_stdcall),PTR2NUM(rb_dl_callback_void_3_1_stdcall),PTR2NUM(rb_dl_callback_void_4_1_stdcall),PTR2NUM(rb_dl_callback_void_5_1_stdcall),PTR2NUM(rb_dl_callback_void_6_1_stdcall),PTR2NUM(rb_dl_callback_void_7_1_stdcall),PTR2NUM(rb_dl_callback_void_8_1_stdcall),PTR2NUM(rb_dl_callback_void_9_1_stdcall),PTR2NUM(rb_dl_callback_void_10_1_stdcall),PTR2NUM(rb_dl_callback_void_11_1_stdcall),PTR2NUM(rb_dl_callback_void_12_1_stdcall),PTR2NUM(rb_dl_callback_void_13_1_stdcall),PTR2NUM(rb_dl_callback_void_14_1_stdcall),PTR2NUM(rb_dl_callback_void_15_1_stdcall),PTR2NUM(rb_dl_callback_void_16_1_stdcall),PTR2NUM(rb_dl_callback_void_17_1_stdcall),PTR2NUM(rb_dl_callback_void_18_1_stdcall),PTR2NUM(rb_dl_callback_void_19_1_stdcall),PTR2NUM(rb_dl_callback_void_0_2_stdcall),PTR2NUM(rb_dl_callback_void_1_2_stdcall),PTR2NUM(rb_dl_callback_void_2_2_stdcall),PTR2NUM(rb_dl_callback_void_3_2_stdcall),PTR2NUM(rb_dl_callback_void_4_2_stdcall),PTR2NUM(rb_dl_callback_void_5_2_stdcall),PTR2NUM(rb_dl_callback_void_6_2_stdcall),PTR2NUM(rb_dl_callback_void_7_2_stdcall),PTR2NUM(rb_dl_callback_void_8_2_stdcall),PTR2NUM(rb_dl_callback_void_9_2_stdcall),PTR2NUM(rb_dl_callback_void_10_2_stdcall),PTR2NUM(rb_dl_callback_void_11_2_stdcall),PTR2NUM(rb_dl_callback_void_12_2_stdcall),PTR2NUM(rb_dl_callback_void_13_2_stdcall),PTR2NUM(rb_dl_callback_void_14_2_stdcall),PTR2NUM(rb_dl_callback_void_15_2_stdcall),PTR2NUM(rb_dl_callback_void_16_2_stdcall),PTR2NUM(rb_dl_callback_void_17_2_stdcall),PTR2NUM(rb_dl_callback_void_18_2_stdcall),PTR2NUM(rb_dl_callback_void_19_2_stdcall),PTR2NUM(rb_dl_callback_void_0_3_stdcall),PTR2NUM(rb_dl_callback_void_1_3_stdcall),PTR2NUM(rb_dl_callback_void_2_3_stdcall),PTR2NUM(rb_dl_callback_void_3_3_stdcall),PTR2NUM(rb_dl_callback_void_4_3_stdcall),PTR2NUM(rb_dl_callback_void_5_3_stdcall),PTR2NUM(rb_dl_callback_void_6_3_stdcall),PTR2NUM(rb_dl_callback_void_7_3_stdcall),PTR2NUM(rb_dl_callback_void_8_3_stdcall),PTR2NUM(rb_dl_callback_void_9_3_stdcall),PTR2NUM(rb_dl_callback_void_10_3_stdcall),PTR2NUM(rb_dl_callback_void_11_3_stdcall),PTR2NUM(rb_dl_callback_void_12_3_stdcall),PTR2NUM(rb_dl_callback_void_13_3_stdcall),PTR2NUM(rb_dl_callback_void_14_3_stdcall),PTR2NUM(rb_dl_callback_void_15_3_stdcall),PTR2NUM(rb_dl_callback_void_16_3_stdcall),PTR2NUM(rb_dl_callback_void_17_3_stdcall),PTR2NUM(rb_dl_callback_void_18_3_stdcall),PTR2NUM(rb_dl_callback_void_19_3_stdcall),PTR2NUM(rb_dl_callback_void_0_4_stdcall),PTR2NUM(rb_dl_callback_void_1_4_stdcall),PTR2NUM(rb_dl_callback_void_2_4_stdcall),PTR2NUM(rb_dl_callback_void_3_4_stdcall),PTR2NUM(rb_dl_callback_void_4_4_stdcall),PTR2NUM(rb_dl_callback_void_5_4_stdcall),PTR2NUM(rb_dl_callback_void_6_4_stdcall),PTR2NUM(rb_dl_callback_void_7_4_stdcall),PTR2NUM(rb_dl_callback_void_8_4_stdcall),PTR2NUM(rb_dl_callback_void_9_4_stdcall),PTR2NUM(rb_dl_callback_void_10_4_stdcall),PTR2NUM(rb_dl_callback_void_11_4_stdcall),PTR2NUM(rb_dl_callback_void_12_4_stdcall),PTR2NUM(rb_dl_callback_void_13_4_stdcall),PTR2NUM(rb_dl_callback_void_14_4_stdcall),PTR2NUM(rb_dl_callback_void_15_4_stdcall),PTR2NUM(rb_dl_callback_void_16_4_stdcall),PTR2NUM(rb_dl_callback_void_17_4_stdcall),PTR2NUM(rb_dl_callback_void_18_4_stdcall),PTR2NUM(rb_dl_callback_void_19_4_stdcall)));
#endif
}
