//
//  NurseryMachMsg.hpp
//  KDBG
//
//  Created by James McIlree on 2/20/14.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef kernel_perf_cmds_NurseryMachMsg_hpp
#define kernel_perf_cmds_NurseryMachMsg_hpp

enum class kNurseryMachMsgState : std::uint32_t {
	Uninitialized = 1,
	Send,
	Recv,
	Free
};

template <typename SIZE>
class NurseryMachMsg {
    protected:
	AbsTime			_send_time;
	MachineVoucher<SIZE>*	_send_voucher;

	typename SIZE::ptr_t	_send_tid;
	typename SIZE::ptr_t	_kmsg_addr;

	uint32_t		_id;				// This is globally unique for EACH message.
	uint32_t		_send_msgh_id;
	uint32_t		_send_msgh_bits;		// msgh_bits is modified between send/recv
	kNurseryMachMsgState	_state;

	// These are intptr_t's so they can be set to -1, indicating "no index"
	intptr_t		_send_event_index;

    public:
	static uint32_t message_id();

	NurseryMachMsg(typename SIZE::ptr_t kmsg_addr) :
		_kmsg_addr(kmsg_addr),
		_state(kNurseryMachMsgState::Uninitialized)
	{
	}

	void send(uintptr_t index, AbsTime time, typename SIZE::ptr_t tid, typename SIZE::ptr_t kmsg_addr, uint32_t msgh_bits, uint32_t msgh_id, MachineVoucher<SIZE>* voucher);

	kNurseryMachMsgState state() const			{ return _state; }
	void set_state(kNurseryMachMsgState state)		{ _state = state; }

	AbsTime send_time() const				{ return _send_time; }
	typename SIZE::ptr_t send_tid() const			{ return _send_tid; }

	typename SIZE::ptr_t kmsg_addr() const			{ return _kmsg_addr; }
	MachineVoucher<SIZE>* send_voucher() const		{ return _send_voucher; }

	uint32_t id() const					{ return _id; }
	uint32_t send_msgh_id() const				{ return _send_msgh_id; }
	uint32_t send_msgh_bits() const				{ return _send_msgh_bits; }

	void set_send_event_index(intptr_t value)		{ _send_event_index = value; }
	intptr_t send_event_index() const			{ return _send_event_index; }
};

template <typename SIZE>
uint32_t NurseryMachMsg<SIZE>::message_id() {
	static uint32_t message_id = 1;
	return OSAtomicIncrement32Barrier((volatile int32_t*)&message_id);
}

template <typename SIZE>
void NurseryMachMsg<SIZE>::send(uintptr_t index, AbsTime time, typename SIZE::ptr_t tid, typename SIZE::ptr_t kmsg_addr, uint32_t msgh_bits, uint32_t msgh_id, MachineVoucher<SIZE>* voucher) {
	ASSERT(_state == kNurseryMachMsgState::Uninitialized || _state == kNurseryMachMsgState::Free, "Calling send when msg is not in Uninitialized/Free state");
	ASSERT(kmsg_addr == _kmsg_addr, "Sanity");

	ASSERT(tid, "Sanity");
	ASSERT(msgh_bits, "Sanity");
	
	_id = NurseryMachMsg::message_id();

	_send_event_index = index;
	_send_time = time;
	_send_tid = tid;
	// _kmsg_addr = kmsg_addr;
	_send_msgh_bits = msgh_bits;
	_send_msgh_id = msgh_id;
	_send_voucher = voucher;
}

#endif
