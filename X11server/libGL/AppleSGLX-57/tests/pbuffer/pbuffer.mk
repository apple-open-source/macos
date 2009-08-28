$(TEST_BUILD_DIR)/pbuffer: tests/pbuffer/pbuffer.c $(LIBGL)
	$(CC) tests/pbuffer/pbuffer.c -Iinclude -o $(TEST_BUILD_DIR)/pbuffer $(LINK_TEST)

$(TEST_BUILD_DIR)/pbuffer_destroy: tests/pbuffer/pbuffer_destroy.c $(LIBGL)
	$(CC) tests/pbuffer/pbuffer_destroy.c -Iinclude -o $(TEST_BUILD_DIR)/pbuffer_destroy $(LINK_TEST)
