
struct IONotificationPort
{
    mach_port_t		masterPort;
    mach_port_t		wakePort;
    CFRunLoopSourceRef	source;
};
typedef struct IONotificationPort IONotificationPort;

CFMutableDictionaryRef
IOMakeMatching(
	mach_port_t	masterPort,
	unsigned int	type,
	unsigned int	options,
	void *		args,
	unsigned int	argsSize );

void
IODispatchCalloutFromCFMessage(
        CFMachPortRef port,
        void *msg,
        CFIndex size,
        void *info );

kern_return_t
iokit_user_client_trap(
                       io_connect_t	connect,
                       unsigned int	index,
                       void *p1,
                       void *p2,
                       void *p3,
                       void *p4,
                       void *p5,
                       void *p6 );

kern_return_t
IOServiceGetState(
	io_service_t    service,
	uint64_t *	state );

// masks for getState()
enum {
    kIOServiceInactiveState	= 0x00000001,
    kIOServiceRegisteredState	= 0x00000002,
    kIOServiceMatchedState	= 0x00000004,
    kIOServiceFirstPublishState	= 0x00000008,
    kIOServiceFirstMatchState	= 0x00000010
};
