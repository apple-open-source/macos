#if defined (__ppc__) || defined(ppc)

	.section	__TEXT, __VLib_Container, regular

	.align	2

VLib_Origin:

	.long	0xF04D6163
	.long	0x564C6962
	.long	1
	.long	VLib_Strings - VLib_Origin
	.long	VLib_HashTable - VLib_Origin
	.long	VLib_HashKeys - VLib_Origin
	.long	VLib_ExportSymbols - VLib_Origin
	.long	VLib_ExportNames - VLib_Origin
	.long	2
	.long	23
	.long	0
	.long	20
	.long	21
	.long	85
	.long	0x70777063
	.long	0x00000000
	.long	0xB66E8054
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000

VLib_Strings:

	.ascii	"DVComponentGlue.vlib"
	.byte	0
	.ascii	"/System/Library/Frameworks/DVComponentGlue.framework/Versions/Current/DVComponentGlue"
	.byte	0

	.align	2

VLib_HashTable:

	.long	0x00140000
	.long	0x00140005
	.long	0x000C000A
	.long	0x0028000D

VLib_HashKeys:

	.long	0x001DBF30
	.long	0x0023594A
	.long	0x00193AD0
	.long	0x00194A10
	.long	0x0013673F
	.long	0x0022697E
	.long	0x00119A64
	.long	0x00123EC1
	.long	0x0015FAC4
	.long	0x00161B81
	.long	0x001236BD
	.long	0x00071DE2
	.long	0x000F2C3D
	.long	0x00228426
	.long	0x002285E6
	.long	0x0010CD23
	.long	0x000D5B63
	.long	0x000EB786
	.long	0x00083B49
	.long	0x0010BECC
	.long	0x0012203C
	.long	0x0013E673
	.long	0x0010CD06

VLib_ExportNames:

	.long	0x49444847, 0x65744465, 0x76696365, 0x54696D65
	.long	0x49444855, 0x70646174, 0x65446576, 0x6963654C
	.long	0x69737449, 0x44484765, 0x74446576, 0x69636543
	.long	0x6F6E7472, 0x6F6C4944, 0x4843616E, 0x63656C50
	.long	0x656E6469, 0x6E67494F, 0x49444852, 0x656C6561
	.long	0x73654275, 0x66666572, 0x49444844, 0x6973706F
	.long	0x73654E6F, 0x74696669, 0x63617469, 0x6F6E4944
	.long	0x4843616E, 0x63656C4E, 0x6F746966, 0x69636174
	.long	0x696F6E49, 0x44484E6F, 0x74696679, 0x4D655768
	.long	0x656E4944, 0x484E6577, 0x4E6F7469, 0x66696361
	.long	0x74696F6E, 0x49444857, 0x72697465, 0x49444852
	.long	0x65616449, 0x4448436C, 0x6F736544, 0x65766963
	.long	0x65494448, 0x4F70656E, 0x44657669, 0x63654944
	.long	0x48476574, 0x44657669, 0x6365436C, 0x6F636B49
	.long	0x44484765, 0x74446576, 0x69636553, 0x74617475
	.long	0x73494448, 0x53657444, 0x65766963, 0x65436F6E
	.long	0x66696775, 0x72617469, 0x6F6E4944, 0x48476574
	.long	0x44657669, 0x6365436F, 0x6E666967, 0x75726174
	.long	0x696F6E49, 0x44484765, 0x74446576, 0x6963654C
	.long	0x69737444, 0x65766963, 0x65436F6E, 0x74726F6C
	.long	0x47657444, 0x65766963, 0x65436F6E, 0x6E656374
	.long	0x696F6E49, 0x44446576, 0x69636543, 0x6F6E7472
	.long	0x6F6C5365, 0x74446576, 0x69636543, 0x6F6E6E65
	.long	0x6374696F, 0x6E494444, 0x65766963, 0x65436F6E
	.long	0x74726F6C, 0x44697361, 0x626C6541, 0x56435472
	.long	0x616E7361, 0x6374696F, 0x6E734465, 0x76696365
	.long	0x436F6E74, 0x726F6C45, 0x6E61626C, 0x65415643
	.long	0x5472616E, 0x73616374, 0x696F6E73, 0x44657669
	.long	0x6365436F, 0x6E74726F, 0x6C446F41, 0x56435472
	.long	0x616E7361
	.long	0x6374696F
	.long	0x6E000000

	.section	__TEXT, __VLib_Exports, symbol_stubs, none, 8

	.align	2

VLib_ExportSymbols:

	.indirect_symbol	_DeviceControlDoAVCTransaction
	.long	0x020001BC
	.long	DeviceControlDoAVCTransaction_bp - VLib_Origin

	.indirect_symbol	_DeviceControlDisableAVCTransactions
	.long	0x02000177
	.long	DeviceControlDisableAVCTransactions_bp - VLib_Origin

	.indirect_symbol	_IDHGetDeviceConfiguration
	.long	0x0200010A
	.long	IDHGetDeviceConfiguration_bp - VLib_Origin

	.indirect_symbol	_IDHSetDeviceConfiguration
	.long	0x020000F1
	.long	IDHSetDeviceConfiguration_bp - VLib_Origin

	.indirect_symbol	_IDHGetDeviceControl
	.long	0x02000023
	.long	IDHGetDeviceControl_bp - VLib_Origin

	.indirect_symbol	_DeviceControlEnableAVCTransactions
	.long	0x0200019A
	.long	DeviceControlEnableAVCTransactions_bp - VLib_Origin

	.indirect_symbol	_IDHGetDeviceClock
	.long	0x020000CE
	.long	IDHGetDeviceClock_bp - VLib_Origin

	.indirect_symbol	_IDHNewNotification
	.long	0x02000092
	.long	IDHNewNotification_bp - VLib_Origin

	.indirect_symbol	_IDHCancelNotification
	.long	0x0200006E
	.long	IDHCancelNotification_bp - VLib_Origin

	.indirect_symbol	_IDHDisposeNotification
	.long	0x02000058
	.long	IDHDisposeNotification_bp - VLib_Origin

	.indirect_symbol	_IDHGetDeviceStatus
	.long	0x020000DF
	.long	IDHGetDeviceStatus_bp - VLib_Origin

	.indirect_symbol	_IDHRead
	.long	0x020000AC
	.long	IDHRead_bp - VLib_Origin

	.indirect_symbol	_IDHNotifyMeWhen
	.long	0x02000083
	.long	IDHNotifyMeWhen_bp - VLib_Origin

	.indirect_symbol	_DeviceControlSetDeviceConnectionID
	.long	0x02000155
	.long	DeviceControlSetDeviceConnectionID_bp - VLib_Origin

	.indirect_symbol	_DeviceControlGetDeviceConnectionID
	.long	0x02000133
	.long	DeviceControlGetDeviceConnectionID_bp - VLib_Origin

	.indirect_symbol	_IDHGetDeviceList
	.long	0x02000123
	.long	IDHGetDeviceList_bp - VLib_Origin

	.indirect_symbol	_IDHOpenDevice
	.long	0x020000C1
	.long	IDHOpenDevice_bp - VLib_Origin

	.indirect_symbol	_IDHCloseDevice
	.long	0x020000B3
	.long	IDHCloseDevice_bp - VLib_Origin

	.indirect_symbol	_IDHWrite
	.long	0x020000A4
	.long	IDHWrite_bp - VLib_Origin

	.indirect_symbol	_IDHReleaseBuffer
	.long	0x02000048
	.long	IDHReleaseBuffer_bp - VLib_Origin

	.indirect_symbol	_IDHCancelPendingIO
	.long	0x02000036
	.long	IDHCancelPendingIO_bp - VLib_Origin

	.indirect_symbol	_IDHUpdateDeviceList
	.long	0x02000010
	.long	IDHUpdateDeviceList_bp - VLib_Origin

	.indirect_symbol	_IDHGetDeviceTime
	.long	0x02000000
	.long	IDHGetDeviceTime_bp - VLib_Origin


	.globl	cfm_stub_binding_helper

	.section	__DATA, __VLib_Func_BPs, lazy_symbol_pointers

	.align	2

IDHGetDeviceTime_bp:
	.indirect_symbol	_IDHGetDeviceTime
	.long	cfm_stub_binding_helper

IDHUpdateDeviceList_bp:
	.indirect_symbol	_IDHUpdateDeviceList
	.long	cfm_stub_binding_helper

IDHGetDeviceControl_bp:
	.indirect_symbol	_IDHGetDeviceControl
	.long	cfm_stub_binding_helper

IDHCancelPendingIO_bp:
	.indirect_symbol	_IDHCancelPendingIO
	.long	cfm_stub_binding_helper

IDHReleaseBuffer_bp:
	.indirect_symbol	_IDHReleaseBuffer
	.long	cfm_stub_binding_helper

IDHDisposeNotification_bp:
	.indirect_symbol	_IDHDisposeNotification
	.long	cfm_stub_binding_helper

IDHCancelNotification_bp:
	.indirect_symbol	_IDHCancelNotification
	.long	cfm_stub_binding_helper

IDHNotifyMeWhen_bp:
	.indirect_symbol	_IDHNotifyMeWhen
	.long	cfm_stub_binding_helper

IDHNewNotification_bp:
	.indirect_symbol	_IDHNewNotification
	.long	cfm_stub_binding_helper

IDHWrite_bp:
	.indirect_symbol	_IDHWrite
	.long	cfm_stub_binding_helper

IDHRead_bp:
	.indirect_symbol	_IDHRead
	.long	cfm_stub_binding_helper

IDHCloseDevice_bp:
	.indirect_symbol	_IDHCloseDevice
	.long	cfm_stub_binding_helper

IDHOpenDevice_bp:
	.indirect_symbol	_IDHOpenDevice
	.long	cfm_stub_binding_helper

IDHGetDeviceClock_bp:
	.indirect_symbol	_IDHGetDeviceClock
	.long	cfm_stub_binding_helper

IDHGetDeviceStatus_bp:
	.indirect_symbol	_IDHGetDeviceStatus
	.long	cfm_stub_binding_helper

IDHSetDeviceConfiguration_bp:
	.indirect_symbol	_IDHSetDeviceConfiguration
	.long	cfm_stub_binding_helper

IDHGetDeviceConfiguration_bp:
	.indirect_symbol	_IDHGetDeviceConfiguration
	.long	cfm_stub_binding_helper

IDHGetDeviceList_bp:
	.indirect_symbol	_IDHGetDeviceList
	.long	cfm_stub_binding_helper

DeviceControlGetDeviceConnectionID_bp:
	.indirect_symbol	_DeviceControlGetDeviceConnectionID
	.long	cfm_stub_binding_helper

DeviceControlSetDeviceConnectionID_bp:
	.indirect_symbol	_DeviceControlSetDeviceConnectionID
	.long	cfm_stub_binding_helper

DeviceControlDisableAVCTransactions_bp:
	.indirect_symbol	_DeviceControlDisableAVCTransactions
	.long	cfm_stub_binding_helper

DeviceControlEnableAVCTransactions_bp:
	.indirect_symbol	_DeviceControlEnableAVCTransactions
	.long	cfm_stub_binding_helper

DeviceControlDoAVCTransaction_bp:
	.indirect_symbol	_DeviceControlDoAVCTransaction
	.long	cfm_stub_binding_helper

	.section	__DATA, __VLib_Data_BPs, non_lazy_symbol_pointers

	.align	2

#else
#endif

