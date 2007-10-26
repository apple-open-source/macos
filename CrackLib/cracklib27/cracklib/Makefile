###
# This program is copyright Alec Muffett 1993. The author disclaims all
# responsibility or liability with respect to it's usage or its effect
# upon hardware or computer systems, and maintains copyright as set out
# in the "LICENCE" document which accompanies distributions of Crack v4.0
# and upwards.
###

LIB=	libcrack.a
OBJ=	fascist.o packlib.o rules.o stringlib.o
CFLAGS= -O -I../cracklib -DIN_CRACKLIB

$(LIB):	$(OBJ)
	ar rv $(LIB) $?
	-ranlib $(LIB)

clean:
	-rm -f $(OBJ) $(LIB) *~
