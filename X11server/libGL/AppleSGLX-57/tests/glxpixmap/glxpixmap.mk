$(TEST_BUILD_DIR)/glxpixmap: tests/glxpixmap/glxpixmap.c $(LIBGL)
	$(CC) tests/glxpixmap/glxpixmap.c $(INCLUDE) -o $(TEST_BUILD_DIR)/glxpixmap $(LINK_TEST)

$(TEST_BUILD_DIR)/glxpixmap_create_destroy: tests/glxpixmap/glxpixmap_create_destroy.c $(LIBGL)
	$(CC) tests/glxpixmap/glxpixmap_create_destroy.c $(INCLUDE) -o $(TEST_BUILD_DIR)/glxpixmap_create_destroy $(LINK_TEST)

$(TEST_BUILD_DIR)/glxpixmap_destroy_invalid: tests/glxpixmap/glxpixmap_destroy_invalid.c $(LIBGL)
	$(CC) tests/glxpixmap/glxpixmap_destroy_invalid.c $(INCLUDE) -o $(TEST_BUILD_DIR)/glxpixmap_destroy_invalid $(LINK_TEST)
