include ../../../../../Make.inc

TARGET         = rlm_sql_mysql
SRCS           = sql_mysql.c
RLM_SQL_CFLAGS =  -I/usr/include/mysql -fno-omit-frame-pointer  -arch ppc64  -arch x86_64 -pipe $(INCLTDL)
RLM_SQL_LIBS   = -arch ppc64  -arch x86_64 -pipe -L/usr/lib/mysql -lmysqlclient_r -lz -lm 

include ../rules.mak
