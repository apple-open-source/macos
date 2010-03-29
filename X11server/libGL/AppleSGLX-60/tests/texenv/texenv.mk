$(TEST_BUILD_DIR)/texenv: tests/texenv/texenv.c $(LIBGL)
	$(CC) tests/texenv/texenv.c $(INCLUDE) -o $(TEST_BUILD_DIR)/texenv -L$(INSTALL_DIR)/lib -L$(X11_DIR)/lib -lXmu -lglu libglut.a $(LINK_TEST)
