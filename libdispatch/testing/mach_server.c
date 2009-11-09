#include <stdio.h>
#include <stdlib.h>
#include <launch.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_port.h>
#include <asl.h>
#include <mach-o/dyld.h>
#include <limits.h>

void
load(void)
{
	launch_data_t msg, config, dict, array, val;
	
	config = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	
	val = launch_data_new_string("com.apple.test.mach-server");
	launch_data_dict_insert(config, val, LAUNCH_JOBKEY_LABEL);
	
	val = launch_data_new_bool(1);
	dict = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	launch_data_dict_insert(dict, val, "com.apple.test.mach-server");
	launch_data_dict_insert(config, dict, LAUNCH_JOBKEY_MACHSERVICES);

	char path[PATH_MAX];
	uint32_t size = sizeof(path);
	_NSGetExecutablePath(path, &size);
	
	array = launch_data_alloc(LAUNCH_DATA_ARRAY);
	val = launch_data_new_string(path);
	launch_data_array_set_index(array, val, 0);
	val = launch_data_new_string("-launchd");
	launch_data_array_set_index(array, val, 1);
	launch_data_dict_insert(config, array, LAUNCH_JOBKEY_PROGRAMARGUMENTS);
	
	
	msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	launch_data_dict_insert(msg, config, LAUNCH_KEY_SUBMITJOB);
	fprintf(stderr, "loading launchd job com.apple.test.mach-server\n");
	msg = launch_msg(msg);
	if (msg && launch_data_get_type(config) == LAUNCH_DATA_ERRNO) {
		fprintf(stderr, "launch load failed: %s\n", strerror(launch_data_get_errno(msg)));
	} else {
		fprintf(stderr, "successful\n");
	}
}

mach_port_t
checkin(void)
{
	launch_data_t config = NULL, checkin = NULL;
	checkin = launch_data_new_string(LAUNCH_KEY_CHECKIN);
	config = launch_msg(checkin);
	if (!config || launch_data_get_type(config) == LAUNCH_DATA_ERRNO)
		return MACH_PORT_NULL;
	
	launch_data_t svc;
	svc = launch_data_dict_lookup(config, LAUNCH_JOBKEY_MACHSERVICES);
	if (!svc) return MACH_PORT_NULL;

	svc = launch_data_dict_lookup(svc, "com.apple.test.mach-server");
	if (!svc) return MACH_PORT_NULL;

	mach_port_t mp = launch_data_get_machport(svc);
	return mp;
}


#include <CoreFoundation/CoreFoundation.h>

void my_cf_callback(CFMachPortRef mp, void *msg, CFIndex size, void *info)
{
	char *payload = (char *)((uintptr_t)msg + sizeof(mach_msg_header_t));
	asl_log(NULL, NULL, ASL_LEVEL_NOTICE, "message received: %s %s", payload, (char *)info);
}

DISPATCH_CFMACHPORT_CALLBACK_DECL(my_mig_callback, my_cf_callback);

struct strmsg {
	mach_msg_header_t header;
	char payload[32];
	mach_msg_security_trailer_t trailer;
};

int
client(int argc, char* argv[])
{
	mach_port_t mp;
	kern_return_t kr;
	struct strmsg msg;

	char *str = (argc > 1) ? argv[1] : "<unspecified>";

	kr = bootstrap_look_up(bootstrap_port, "com.apple.test.mach-server",
		&mp);
	printf("lookup %s\n", !kr ? "successful" : mach_error_string(kr));

	strlcpy(msg.payload, str, sizeof(msg.payload));
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,MACH_MSG_TYPE_MAKE_SEND_ONCE);
	msg.header.msgh_size = round_msg(sizeof(msg.header) + strlen(msg.payload) + 1);
	msg.header.msgh_remote_port = mp;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_id = 0x12345678;
	kr = mach_msg(&msg.header, MACH_SEND_MSG, msg.header.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	printf("client send %s\n", !kr ? "successful" : mach_error_string(kr));
}

int main(int argc, char* argv[])
{
	if (argc > 1 && strcmp(argv[1], "-launchd") == 0) {
		mach_port_t mp = checkin();
		printf("checkin %s\n", mp ? "successful" : "failed");
		if (mp) {
			kern_return_t kr;
			kr = mach_port_set_context(mach_task_self(), mp, (mach_vm_address_t)"phear");
			
			dispatch_source_t mig = dispatch_source_mig_create(mp, 0 /* max msg size */,
															   NULL, dispatch_get_main_queue(), my_mig_callback);
			dispatch_main();
		}
	} else if (argc > 1) {
		client(argc, argv);
	} else {
		// load the job
		load();
	}

	return 0;
}

