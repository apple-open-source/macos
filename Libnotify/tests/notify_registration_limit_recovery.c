#include <darwintest.h>
#include <notify.h>
#include <stdbool.h>

T_DECL(notify_registration_limit_recovery,
       "verify registrations succeed after canceling across multiple batches",
       T_META("as_root", "false"))
{
	const int batch_size = 5000;
	const int num_batches = 5;
	int tokens[batch_size];
	char name[64];

	for (int batch = 0; batch < num_batches; batch++) {
		T_LOG("Starting batch %d", batch);

		/* Register batch - use unique names to avoid coalescing */
		for (int i = 0; i < batch_size; i++) {
			snprintf(name, sizeof(name), "com.apple.notify.recovery.batch%d.%d", batch, i);
			uint32_t status = notify_register_check(name, &tokens[i]);
			T_QUIET;
			T_ASSERT_EQ_INT(status, NOTIFY_STATUS_OK,
				"Batch %d registration %d should succeed", batch, i);
		}

		T_LOG("Batch %d: registered %d tokens successfully", batch, batch_size);

		/* Cancel all tokens in this batch */
		for (int i = 0; i < batch_size; i++) {
			uint32_t status = notify_cancel(tokens[i]);
			T_QUIET;
			T_ASSERT_EQ_INT(status, NOTIFY_STATUS_OK,
				"Batch %d cancel %d should succeed", batch, i);
		}

		T_LOG("Batch %d: canceled all tokens", batch);
	}

	T_PASS("Successfully completed %d batches of %d registrations each",
		num_batches, batch_size);
}
