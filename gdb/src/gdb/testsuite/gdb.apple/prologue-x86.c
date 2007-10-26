#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

double func_over_signal_thrower (double, float, const char *);
int signal_thrower (int);
void handler (int);

int func_over_mov_imm32_addr_in_prologue (int);
int mov_imm32_reg_in_prologue (int);
int func_under_mov_imm32_reg_in_prologue (int);

double func_over_cmpl_in_prologue (double, float);
int cmpl_in_prologue (void);
int func_under_cmpl_in_prologue (int);

char *func_over_objc_msgForward_pattern (char *);
int objc_msgForward_pattern (int);
int func_under_objc_msgForward_pattern (int);

main (int argc, char **argv, char **envp)
{
  int ret;
  char *sret;

  ret = func_over_signal_thrower (argc * 5.2, argc * 9999.33, argv[0]);
  printf ("%d\n", ret);

  ret = func_over_mov_imm32_addr_in_prologue (argc * 2);
  printf ("%d\n", ret);

  ret = func_over_cmpl_in_prologue (5, 10);
  printf ("%d\n", ret);

  char buf [80];
  strcpy (buf, "hummingbirds are cool");
  sret = func_over_objc_msgForward_pattern (buf);
  printf ("%s\n", sret);
}

/* Throw a signal while down on the stack, make sure we can find 
   our way through _sigtramp() and get all the necessary information
   out of the sigcontext struct to backtrace our way out. */

double func_over_signal_thrower (double d, float f, const char *s)
{
  int c = d * f;
  c += s[0];
  f = f * c;
  c -= signal_thrower (c);
  return (d * f) / c;
}

int signal_thrower (int in)
{
  in--;
  signal (SIGALRM, handler);
  alarm (1);
  sleep (2);
  in = in - 2;
  return in;
}

void handler (int sig)
{
  signal (sig, handler);
}



/*  mov imm32, reg in a prologue.  e.g.
       mov    $0xffffffce,%eax
    reg can be e[abcd]x.  Bytes are
    0xb[8-b] 4-byte-immediate value.

 From _ZN18MacFile_DataSource10WriteBytesEtxmPKvPm() aka
 MacFile_DataSource::WriteBytes(unsigned short, long long, unsigned long, void const*, unsigned long*)
 in /System/Library/Frameworks/AudioToolbox.framework/Versions/A/AudioToolbox 

*/

int
func_over_mov_imm32_addr_in_prologue (int in)
{
  in *= in;
  in = mov_imm32_reg_in_prologue (in);
  return in;
}

asm(".text\n"
    "    .align 8\n"
    "_mov_imm32_reg_in_prologue:\n"
    "    push  %ebp\n"
    "    mov   $0xffffffce,%eax\n"
    "    mov   %esp,%ebp\n"
    "    sub   $0x18,%esp\n"
    "    mov   0x8(%ebp),%eax\n"
    "    addl  $0x99,%eax\n"
    "    mov   %eax,(%esp)\n"
    "    call  _func_under_mov_imm32_reg_in_prologue\n"
    "    mov   %eax,0x8(%ebp)\n"
    "    leave\n"
    "    ret\n");

int
func_under_mov_imm32_reg_in_prologue (int in)
{
   in /= 2;
   return in + 5;
}



/* `cmp imm32,m32' in a prologue, e.g.
    cmpl   $0x0,0xc03a4254
   Bytes are
     0x83 4-byte-imm 4-byte-addr
   From Debugger() in xnu. 

*/

double 
func_over_cmpl_in_prologue (double d, float f)
{
  double c = d * f;
  c = c * cmpl_in_prologue ();
  return c;
}

asm(".text\n"
    "    .align 8\n"
    "_cmpl_in_prologue:\n"
    "    push  %ebp\n"
    "    cmpl  $0x0,_main\n"
    "    mov   %esp,%ebp\n"
    "    push  %ebx\n"
    "    mov   $0x5,%eax\n"
    "    push  %eax\n"
    "    call  _func_under_cmpl_in_prologue\n"
    "    pop   %ebx\n"
    "    pop   %ebx\n"
    "    pop   %ebp\n"
    "    ret\n");

int
func_under_cmpl_in_prologue (int in)
{
   in++;
   in++;
   return in + 4;
}


char *
func_over_objc_msgForward_pattern (char * in)
{
  int len = strlen (in);
  char *p;
  p = &in[objc_msgForward_pattern (3) % len];
  return p;
}

/* objc_msgForward () has some hand-written assembly at the front
   as a little optimization.  So we'll need to recognize that.
     cmp $0x0,%edx          [ 0x83 0xfa 0x00 ]
     je _objc_msgForward+89 [ 0x74 0x54 ]
   This function is in /usr/lib/libobjc.dylib.  */

asm(".text\n"
    "   .align 8\n"
    "_objc_msgForward_pattern:\n"
    "   cmp   $0x0, %edx\n"
    "   je    LM998\n"
    "LM998:\n"
    "   push  %ebp\n"
    "   mov %esp, %ebp\n"
    "   sub $4, %esp\n"
    "   movl $99, (%esp)\n"
    "   call _func_under_objc_msgForward_pattern\n"
    "   add $4, %esp\n"
    "   pop %ebp\n"
    "   ret\n");

int
func_under_objc_msgForward_pattern (int in)
{
  in += 10;
  return in;
}
