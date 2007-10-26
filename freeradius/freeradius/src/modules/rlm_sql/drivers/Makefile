SQL_MODULES		=  rlm_sql_iodbc rlm_sql_mysql rlm_sql_postgresql rlm_sql_oracle rlm_sql_unixodbc

all:
	$(MAKE) $(MFLAGS) WHAT_TO_MAKE=$@ common

clean:
	$(MAKE) $(MFLAGS) WHAT_TO_MAKE=$@ common
	@rm -f lib/rlm_* lib/*.o
	@rmdir lib

install:
	$(MAKE) $(MFLAGS) WHAT_TO_MAKE=$@ common

reconfig:
	$(MAKE) $(MFLAGS) WHAT_TO_MAKE=$@ common

common:
	@[ -d lib/ ] || mkdir lib
	@for mod in $(SQL_MODULES); do \
		echo "Making $(WHAT_TO_MAKE) in $$mod..."; \
		$(MAKE) $(MFLAGS) -C $$mod $(WHAT_TO_MAKE) || exit $$?; \
	done
