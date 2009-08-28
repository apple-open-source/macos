$(TEST_BUILD_DIR)/triangle_glx_single: tests/triangle_glx_single/triangle_glx.c $(LIBGL)
	$(CC) tests/triangle_glx_single/triangle_glx.c -Iinclude -o $(TEST_BUILD_DIR)/triangle_glx_single $(LINK_TEST)
