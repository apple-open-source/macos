$(TEST_BUILD_DIR)/create_destroy_context: tests/create_destroy_context/create_destroy_context.c $(LIBGL)
	$(CC) tests/create_destroy_context/create_destroy_context.c -Iinclude \
    -o $(TEST_BUILD_DIR)/create_destroy_context $(LINK_TEST)

$(TEST_BUILD_DIR)/create_destroy_context_alone: tests/create_destroy_context/create_destroy_context_alone.c $(LIBGL)
	$(CC) tests/create_destroy_context/create_destroy_context_alone.c -Iinclude \
    -o $(TEST_BUILD_DIR)/create_destroy_context_alone $(LINK_TEST)

$(TEST_BUILD_DIR)/create_destroy_context_with_drawable: tests/create_destroy_context/create_destroy_context_with_drawable.c $(LIBGL)
	$(CC) tests/create_destroy_context/create_destroy_context_with_drawable.c -Iinclude \
    -o $(TEST_BUILD_DIR)/create_destroy_context_with_drawable $(LINK_TEST)

$(TEST_BUILD_DIR)/create_destroy_context_with_drawable_2: tests/create_destroy_context/create_destroy_context_with_drawable_2.c $(LIBGL)
	$(CC) tests/create_destroy_context/create_destroy_context_with_drawable_2.c -Iinclude \
    -o $(TEST_BUILD_DIR)/create_destroy_context_with_drawable_2 $(LINK_TEST)

