$(TEST_BUILD_DIR)/engine: tests/engine/engine.c $(LIBGL) libglut.a
	$(CC) tests/engine/engine.c tests/engine/readtex.c tests/engine/trackball.c $(INCLUDE) -o $(TEST_BUILD_DIR)/engine $(LINK_TEST) libglut.a -L$(INSTALL_DIR)/lib -L$(X11_DIR)/lib -lXmu -lGLU
