$(TEST_BUILD_DIR)/glxgears: tests/glxgears/glxgears.c $(LIBGL)
	$(CC) tests/glxgears/glxgears.c -Iinclude -o $(TEST_BUILD_DIR)/glxgears $(LINK_TEST)
