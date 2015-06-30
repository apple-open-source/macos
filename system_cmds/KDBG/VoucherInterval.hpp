//
//  VoucherInterval.hpp
//  KDBG
//
//  Created by James McIlree on 2/18/14.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef kernel_perf_cmds_Header_h
#define kernel_perf_cmds_Header_h

template <typename SIZE>
class VoucherInterval : public AbsInterval {
	MachineVoucher<SIZE>*	_voucher;

    public:
	VoucherInterval(MachineVoucher<SIZE>* voucher, AbsInterval interval) :
	        AbsInterval(interval),
		_voucher(voucher)
	{
	}

	const MachineVoucher<SIZE>* voucher() const	{ return _voucher; }
};

#endif
