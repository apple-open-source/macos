.PHONY : tests

LIBGL=$(TEST_BUILD_DIR)/libGL.dylib
LINK_TEST=-L$(INSTALL_DIR)/lib -L$(X11_DIR)/lib $(LIBGL) -lX11 -lXext -lXplugin -lpthread

include tests/triangle/triangle.mk
include tests/simple/simple.mk
include tests/fbconfigs/fbconfigs.mk
include tests/triangle_glx/triangle_glx.mk
include tests/create_destroy_context/create_destroy_context.mk
include tests/glxgears/glxgears.mk
include tests/glxinfo/glxinfo.mk
include tests/pbuffer/pbuffer.mk
include tests/texenv/texenv.mk
include tests/engine/engine.mk
include tests/glxpixmap/glxpixmap.mk
include tests/triangle_glx_single/triangle_glx.mk
include tests/shared/shared.mk

tests: $(TEST_BUILD_DIR)/simple $(TEST_BUILD_DIR)/fbconfigs $(TEST_BUILD_DIR)/triangle_glx \
  $(TEST_BUILD_DIR)/create_destroy_context $(TEST_BUILD_DIR)/glxgears $(TEST_BUILD_DIR)/glxinfo \
  $(TEST_BUILD_DIR)/pbuffer $(TEST_BUILD_DIR)/pbuffer_destroy \
  $(TEST_BUILD_DIR)/glxpixmap \
  $(TEST_BUILD_DIR)/triangle_glx_single \
  $(TEST_BUILD_DIR)/create_destroy_context_alone \
  $(TEST_BUILD_DIR)/create_destroy_context_with_drawable \
  $(TEST_BUILD_DIR)/create_destroy_context_with_drawable_2 \
  $(TEST_BUILD_DIR)/render_types \
  $(TEST_BUILD_DIR)/glxpixmap_create_destroy \
  $(TEST_BUILD_DIR)/sharedtex \
  $(TEST_BUILD_DIR)/drawable_types \
  $(TEST_BUILD_DIR)/glxpixmap_destroy_invalid \
  $(TEST_BUILD_DIR)/multisample_glx \
  $(TEST_BUILD_DIR)/glthreads \
  $(TEST_BUILD_DIR)/triangle_glx_surface \
  $(TEST_BUILD_DIR)/triangle_glx_surface-2 \
  $(TEST_BUILD_DIR)/triangle_glx_withdraw_remap \
  $(TEST_BUILD_DIR)/triangle_glx_destroy_relation \
  $(TEST_BUILD_DIR)/query_drawable

