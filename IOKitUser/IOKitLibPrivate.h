
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
