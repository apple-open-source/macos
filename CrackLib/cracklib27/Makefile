###
# This program is copyright Alec Muffett 1993. The author disclaims all
# responsibility or liability with respect to it's usage or its effect
# upon hardware or computer systems, and maintains copyright as set out
# in the "LICENCE" document which accompanies distributions of Crack v4.0
# and upwards.
###

###
# set this to the absolute path (less extn) of compressed dict.

DICTPATH="/usr/share/cracklib/pw_dict"

###
# Set this to the path of one or more files continaing wordlists.

SRCDICTS=/usr/dict/words

###
# If you have installed the cracklib-dicts directory, use this
#SRCDICTS=/usr/dict/words cracklib-dicts/*

default:
	@echo "you evidently don't know what you're doing. go read the README"

all:
	( cd cracklib && make && exit $$? )
	( cd util && make DICTPATH=$(DICTPATH) && exit $$? )
###	( cd passwd && make DICTPATH=$(DICTPATH) passwd && exit $$? )
###	touch all

clean:
	-( cd cracklib && make clean && exit $$? )
	-( cd util && make clean && exit $$? )
###	-( cd passwd && make clean && exit $$? )
	-rm -f all installed Part* *.BAK *.bak *~

install: all
	@echo 'if "sort" dies from lack of space, see "util/mkdict"'
	util/mkdict $(SRCDICTS) | util/packer $(DICTPATH)
	touch installed
###	@echo 'now go install passwd/passwd where you want it'
