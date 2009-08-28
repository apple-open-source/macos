#!/usr/sbin/dtrace -s
/*
 * newproc.d - snoop new processes as they are executed. DTrace OneLiner.
 *
 * This is a DTrace OneLiner from the DTraceToolkit.
 *
 * 15-May-2005	Brendan Gregg	Created this.
 */

/*
 * Updated to capture arguments in OS X. Unfortunately this isn't straight forward...
 */

#pragma D option quiet

this unsigned long long argv_ptr; /* Wide enough for 64 bit user procs */
this char *psargs;

proc:::exec-success
{
	print_pid[pid] = 1; /* This pid emerged from an exec, make a note of that. */
}

/*
 * The "this" variables are local to (all) of the following syscall::mmap:return probes,
 * and only those probes. They must be initialized before use in each new firing.
 */
syscall::mmap:return
{
	this->argc = 0; /* Disable argument collection until we notice an exec-success */
	this->psargs = 0; 
}

syscall::mmap:return
/ print_pid[pid] /
{
	print_pid[pid] = 0;

	this->is64Bit = curpsinfo->pr_dmodel == PR_MODEL_ILP32 ? 0 : 1;
	this->wordsize = this->is64Bit ? 8 : 4;

	this->argc = curpsinfo->pr_argc; 
	this->argc = (this->argc < 0) ? 0 : this->argc; /* Safety */

	this->argv_ptr = curpsinfo->pr_argv;

	this->psargs = "";
	printf("%d %s ", pid, this->is64Bit ? "64b" : "32b");
}

syscall::mmap:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg);
	this->psargs = strjoin(strjoin(this->psargs," "), this->here_arg);

	this->argv_ptr += this->wordsize;
	this->argc--;
}

syscall::mmap:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg);
	this->psargs = strjoin(strjoin(this->psargs," "), this->here_arg);

	this->argv_ptr += this->wordsize;
	this->argc--;
}

syscall::mmap:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg);
	this->psargs = strjoin(strjoin(this->psargs," "), this->here_arg);

	this->argv_ptr += this->wordsize;
	this->argc--;
}

syscall::mmap:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg);
	this->psargs = strjoin(strjoin(this->psargs," "), this->here_arg);

	this->argv_ptr += this->wordsize;
	this->argc--;
}

syscall::mmap:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg);
	this->psargs = strjoin(strjoin(this->psargs," "), this->here_arg);

	this->argv_ptr += this->wordsize;
	this->argc--;
}

syscall::mmap:return
/ this->argc /
{
	this->here_argv = copyin(this->argv_ptr, this->wordsize);
	this->arg = this->is64Bit ? *(unsigned long long*)(this->here_argv) : *(unsigned long*)(this->here_argv);
	this->here_arg = copyinstr(this->arg);
	this->psargs = strjoin(strjoin(this->psargs," "), this->here_arg);

	this->argv_ptr += this->wordsize;
	this->argc--;
}


syscall::mmap:return
/ this->psargs /
{
	printf("%s%s\n",stringof(this->psargs), this->argc > 0 ? " (...)" : " ");
	this->psargs = 0;
	this->argc = 0;
}

