$(TEST_BUILD_DIR)/sharedtex: tests/shared/sharedtex.c $(LIBGL)
	$(CC) tests/shared/sharedtex.c $(INCLUDE) -o $(TEST_BUILD_DIR)/sharedtex $(LINK_TEST)

