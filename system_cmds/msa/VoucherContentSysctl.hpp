//
//  VoucherContentSysctl.hpp
//  system_cmds
//
//  Created by James McIlree on 4/29/14.
//
//

#ifndef __system_cmds__VoucherContentSysctl__
#define __system_cmds__VoucherContentSysctl__

//
// This class is used to manage the voucher contents sysctl
class VoucherContentSysctl {
    protected:
	int _original_value;
	int _new_value;

    public:
        VoucherContentSysctl(bool is_enabled);
        ~VoucherContentSysctl();
};

#endif /* defined(__system_cmds__VoucherContentSysctl__) */
