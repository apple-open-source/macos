$(TEST_BUILD_DIR)/glxinfo: tests/glxinfo/glxinfo.c $(LIBGL)
	$(CC) tests/glxinfo/glxinfo.c $(INCLUDE) -o $(TEST_BUILD_DIR)/glxinfo $(LINK_TEST)
