//
//  MachineMachMsg.hpp
//  KDBG
//
//  Created by James McIlree on 2/20/14.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef kernel_perf_cmds_MachineMachMsg_hpp
#define kernel_perf_cmds_MachineMachMsg_hpp

enum kMachineMachMsgFlag {
	HasSender		= 0x00000001,
	HasReceiver		= 0x00000002,
	IsVoucherRefused	= 0x00000004
};

template <typename SIZE>
class MachineMachMsg {
    protected:

	// POTENTIAL ISSUE:
	//
	// We could have a case where a sender queue's a message, then dies before the receiver picks it up.
	// With a dead thread, the next MachineState will not include the tid, and then we'll be unable to look it up.

	// NOTE NOTE NOTE!
	//
	// The instance vars are sorted by size to avoid wasting space.
	// Don't change without good reason, and add new vars to the
	// correct location!

	/*
	 * ALWAYS 64b
	 */

	AbsTime			_send_time;
	AbsTime			_recv_time;

	/*
	 * LP64 - HOST
	 *
	 * I'm going to assume the most common asymetric pattern is a 64 bit desktop
	 * looking at a 32 bit device...
	 */

	MachineVoucher<SIZE>*	_send_voucher;
	MachineVoucher<SIZE>*	_recv_voucher;

	/*
	 * LP64 - SIZE
	 */

	typename SIZE::ptr_t	_send_tid;
	typename SIZE::ptr_t	_recv_tid;
	typename SIZE::ptr_t	_kmsg_addr;


	/*
	 * ALWAYS 32b
	 */
	
	uint32_t		_id;				// This is globally unique for EACH message.
	uint32_t		_send_msgh_bits;		// msgh_bits is modified between send/recv
	uint32_t		_recv_msgh_bits;
	uint32_t		_flags;

    public:
	MachineMachMsg(uint32_t id,
		       typename SIZE::ptr_t kmsg_addr,
		       uint32_t flags,
		       AbsTime send_time,
		       typename SIZE::ptr_t send_tid,
		       uint32_t send_msgh_bits,
		       MachineVoucher<SIZE>* send_voucher,
		       AbsTime recv_time,
		       typename SIZE::ptr_t recv_tid,
		       uint32_t recv_msgh_bits,
		       MachineVoucher<SIZE>* recv_voucher) :
		_send_time(send_time),
		_recv_time(recv_time),
		_send_voucher(send_voucher),
		_recv_voucher(recv_voucher),
		_send_tid(send_tid),
		_recv_tid(recv_tid),
		_kmsg_addr(kmsg_addr),
		_id(id),
		_send_msgh_bits(send_msgh_bits),
		_recv_msgh_bits(recv_msgh_bits),
		_flags(flags)
	{
		// Should always have a valid pointer, but may be Machine<SIZE>::NullVoucher
		ASSERT(send_voucher, "Sanity");
		ASSERT(recv_voucher, "Sanity");

		ASSERT(send_voucher->is_unset() == (MACH_MSGH_BITS_VOUCHER(_send_msgh_bits) == MACH_MSGH_BITS_ZERO), "voucher state disagrees with msgh_bits");
		ASSERT(recv_voucher->is_unset() == (MACH_MSGH_BITS_VOUCHER(_recv_msgh_bits) == MACH_MSGH_BITS_ZERO), "voucher state disagrees with msgh_bits");
	}

	bool has_sender() const				{ return (_flags & kMachineMachMsgFlag::HasSender) > 0; }
	bool has_receiver() const			{ return (_flags & kMachineMachMsgFlag::HasReceiver) > 0; }

	uint32_t id() const				{ return _id; }

	typename SIZE::ptr_t send_tid() const		{ ASSERT(has_sender(), "No Sender"); return _send_tid; }
	typename SIZE::ptr_t recv_tid() const		{ ASSERT(has_receiver(), "No Receiver"); return _recv_tid; }
	
	AbsTime send_time() const			{ ASSERT(has_sender(), "No Sender"); return _send_time; }
	AbsTime recv_time() const			{ ASSERT(has_receiver(), "No Receiver"); return _recv_time; }
	
	MachineVoucher<SIZE>* send_voucher() const	{ ASSERT(has_sender(), "No Sender"); return _send_voucher; }
	MachineVoucher<SIZE>* recv_voucher() const	{ ASSERT(has_receiver(), "No Receiver"); return _recv_voucher; }
	
        uint32_t send_msgh_bits() const			{ ASSERT(has_sender(), "No Sender"); return _send_msgh_bits; }
	uint32_t recv_msgh_bits() const			{ ASSERT(has_receiver(), "No Receiver"); return _recv_msgh_bits; }
	
	bool is_voucher_refused() const			{ ASSERT(has_receiver(), "No Receiver"); return (_flags & kMachineMachMsgFlag::IsVoucherRefused) > 0; }
	
	bool has_send_voucher() const			{ return has_sender() && MACH_MSGH_BITS_VOUCHER(_send_msgh_bits) != MACH_MSGH_BITS_ZERO; }
	bool has_recv_voucher() const			{ return has_receiver() && MACH_MSGH_BITS_VOUCHER(_recv_msgh_bits) != MACH_MSGH_BITS_ZERO; }

	bool has_non_null_send_voucher() const		{ return has_sender() && MACH_MSGH_BITS_VOUCHER(_send_msgh_bits) != MACH_MSGH_BITS_ZERO && !_send_voucher->is_null(); }
	bool has_non_null_recv_voucher() const		{ return has_receiver() && MACH_MSGH_BITS_VOUCHER(_recv_msgh_bits) != MACH_MSGH_BITS_ZERO && !_recv_voucher->is_null(); }
};

#endif
