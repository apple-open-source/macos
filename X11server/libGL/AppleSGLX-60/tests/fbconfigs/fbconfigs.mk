$(TEST_BUILD_DIR)/fbconfigs: tests/fbconfigs/fbconfigs.c $(LIBGL)
	$(CC) tests/fbconfigs/fbconfigs.c $(INCLUDE) -o $(TEST_BUILD_DIR)/fbconfigs $(LINK_TEST)
