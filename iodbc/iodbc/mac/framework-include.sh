#!/bin/sh


mkdir -p include
for i in sql.h sqlext.h sqltypes.h iodbcinst.h isql.h isqlext.h isqltypes.h
do
    sed -e "s/<sql\.h/<iODBC\/sql.h/" \
	-e "s/<sqlext\.h/<iODBC\/sqlext.h/" \
	-e "s/<sqltypes\.h/<iODBC\/sqltypes.h/" \
	-e "s/<iodbcinst\.h/<IODBCinst\/iodbcinst.h/" < ../include/$i > include/$i
done
