no-target:
	@make -f Makefile $(MAKEFLAGS)

# autoconf like to scribble in the source directory, so we clone it first,
# and run out of the cloned directory
install:
	ditto $(SRCROOT) $(OBJROOT)/SRC
	make -C $(OBJROOT)/SRC -f Makefile $@ $(MAKEFLAGS) SRCROOT="$(OBJROOT)/SRC"

.DEFAULT:
	@make -f Makefile $@ $(MAKEFLAGS)
