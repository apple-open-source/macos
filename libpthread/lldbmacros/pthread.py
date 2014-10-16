from xnu import *
import struct

@header("{0: <24s} {1: <16s} {2: <16s} {3: <16s} {4: <16s}".format('sig', 'tid', 'options', 'lseq', 'useq'))
def GetUserMutexSummary(task, uaddr):
	if int(task.t_flags) & 0x1:
		mtxlayout = "QIIhhIQIII"
		padoffset = 1
	else:
		mtxlayout = "QIIhhQIII"
		padoffset = 0

	data = GetUserDataAsString(task, uaddr, struct.calcsize(mtxlayout))
	info = struct.unpack(mtxlayout, data)

	format = "{0: <24s} {1: <16s} {2: <16s} {3: <16s} {4: <16s}"
	sigstr = str("{0: <#020x}".format(info[0]))

	# the options field dictates whether we were created misaligned
	if info[2] & 0x800:
		lseq = info[7+padoffset]
		useq = info[8+padoffset]
	else:
		lseq = info[6+padoffset]
		useq = info[7+padoffset]

	return format.format(sigstr, hex(info[5+padoffset]), hex(info[2]), hex(lseq), hex(useq))

@lldb_command('showusermutex')
def PthreadShowUserMutex(cmd_args=None):
	"""
	display information about a userspace mutex at a given address
	Syntax: (lldb) showusermutex <task_t> <uaddr>
	"""
	if not cmd_args:
		raise ArgumentError("No arguments passed")
	task = kern.GetValueFromAddress(cmd_args[0], "task_t")
	uaddr = kern.GetValueFromAddress(cmd_args[1], "user_addr_t")

	print GetUserMutexSummary.header
	print GetUserMutexSummary(task, uaddr)

@lldb_type_summary(['ksyn_waitq_element *', 'ksyn_waitq_element_t'])
@header("{0: <24s} {1: <24s} {2: <24s} {3: <10s}".format('kwe', 'kwq', 'uaddr', 'type'))
def GetKweSummary(kwe):
	format = "{0: <24s} {1: <24s} {2: <24s} {3: <10s}"
	kwe = Cast(addressof(kwe), "ksyn_waitq_element_t")
	kwestr = str("{0: <#020x}".format(kwe))

	kwq = Cast(kwe.kwe_kwqqueue, "ksyn_wait_queue_t")
	kwqstr = str("{0: <#020x}".format(kwq))
	uaddrstr = str("{0: <#020x}".format(kwq.kw_addr))

	kwqtype = ""
	if kwq.kw_type & 0xff == 0x01:
		kwqtype = "mtx"
	if kwq.kw_type & 0xff == 0x02:
		kwqtype = "cvar"
	if kwq.kw_type & 0xff == 0x04:
		kwqtype = "rwlock"
	if kwq.kw_type & 0xff == 0x05:
		kwqtype = "sema"

	return format.format(kwestr, kwqstr, uaddrstr, kwqtype)

@header("{0: <24s} {1: <24s} {2: <24s}".format('thread', 'thread_id', 'uthread'))
def GetPthreadSummary(thread):
	format = "{0: <24s} {1: <24s} {2: <24s}"

	threadstr = str("{0: <#020x}".format(thread))
	if int(thread.static_param):
		threadstr += "[WQ]"

	uthread = Cast(thread.uthread, "uthread_t")
	uthreadstr = str("{0: <#020x}".format(uthread))


	return format.format(threadstr, hex(thread.thread_id), uthreadstr)

@header("{0: <24s} {1: <24s} {2: <10s} {3: <10s} {4: <10s} {5: <10s} {6: <10s}".format('proc', 'wq', 'sched', 'req', 'idle', 'flags', 'wqflags'))
def GetPthreadWorkqueueSummary(wq):
	format = "{0: <24s} {1: <24s} {2: <10d} {3: <10d} {4: <10d} {5: <10s} {6: <10s}"
	procstr = str("{0: <#020x}".format(wq.wq_proc))
	wqstr = str("{0: <#020x}".format(wq))
	
	flags = []
	if wq.wq_flags & 0x1:
		flags.append("I")
	if wq.wq_flags & 0x2:
		flags.append("R")
	if wq.wq_flags & 0x4:
		flags.append("E")
		
	wqflags = []
	if wq.wq_lflags & 0x1:
		wqflags.append("B")
	if wq.wq_lflags & 0x2:
		wqflags.append("W")
	if wq.wq_lflags & 0x4:
		wqflags.append("C")
	if wq.wq_lflags & 0x8:
		wqflags.append("L")
	
	return format.format(procstr, wqstr, wq.wq_threads_scheduled, wq.wq_reqcount, wq.wq_thidlecount, "".join(flags), "".join(wqflags))

@header("{0: <24s} {1: <5s} {2: <5s} {3: <5s} {4: <5s} {5: <5s} {6: <5s}".format('category', 'uint', 'uinit', 'lgcy', 'util', 'bckgd', 'maint'))
def GetPthreadWorkqueueDetail(wq):
	format = "  {0: <22s} {1: <5d} {2: <5d} {3: <5d} {4: <5d} {5: <5d} {6: <5d}"
	# requests
	reqstr = format.format('requests', wq.wq_requests[0], wq.wq_requests[1], wq.wq_requests[2], wq.wq_requests[3], wq.wq_requests[4], wq.wq_requests[5])
	ocstr = format.format('ocreqs', wq.wq_ocrequests[0], wq.wq_ocrequests[1], wq.wq_ocrequests[2], wq.wq_ocrequests[3], wq.wq_ocrequests[4], wq.wq_ocrequests[5])
	schedstr = format.format('scheduled', wq.wq_thscheduled_count[0], wq.wq_thscheduled_count[1], wq.wq_thscheduled_count[2], wq.wq_thscheduled_count[3], wq.wq_thscheduled_count[4], wq.wq_thscheduled_count[5])
	activestr = format.format('active', wq.wq_thactive_count[0], wq.wq_thactive_count[1], wq.wq_thactive_count[2], wq.wq_thactive_count[3], wq.wq_thactive_count[4], wq.wq_thactive_count[5])
	return "\n".join([reqstr, ocstr, schedstr, activestr])

@lldb_command('showpthreadstate')
def PthreadCurrentMutex(cmd_args=None):
	"""
	display information about a thread's pthread state
	Syntax: (lldb) showpthreadstate <thread_t>
	"""
	if not cmd_args:
		raise ArgumentError("No arguments passed")

	thread = kern.GetValueFromAddress(cmd_args[0], "thread_t")
	print GetPthreadSummary.header
	print GetPthreadSummary(thread)

	uthread = Cast(thread.uthread, "uthread_t")
	kwe = addressof(uthread.uu_kevent.uu_kwe)
	print GetKweSummary.header
	print GetKweSummary(kwe)

@lldb_command('showpthreadworkqueue')
def ShowPthreadWorkqueue(cmd_args=None):
	"""
	display information about a processes' pthread workqueue
	Syntax: (lldb) showpthreadworkqueue <proc_t>
	"""
	
	if not cmd_args:
		raise ArgumentError("No arguments passed")
		
	proc = kern.GetValueFromAddress(cmd_args[0], "proc_t")
	wq = Cast(proc.p_wqptr, "struct workqueue *");
	
	print GetPthreadWorkqueueSummary.header
	print GetPthreadWorkqueueSummary(wq)
	
	print GetPthreadWorkqueueDetail.header
	print GetPthreadWorkqueueDetail(wq)

def __lldb_init_module(debugger, internal_dict):
	pass