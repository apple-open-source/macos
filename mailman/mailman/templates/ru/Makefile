# FILES=admindbpreamble.html adminsubscribeack.txt adminunsubscribeack.txt admlogin.html approve.txt article.html bounce.txt checkdbs.txt convert.txt cronpass.txt headfoot.html help.txt listinfo.html masthead.txt newlist.txt nopass.txt options.html postack.txt postauth.txt postheld.txt private.html reenable.txt refuse.txt roster.html subauth.txt subscribeack.txt subscribe.html unsub.txt userpass.txt verify.txt
FILES=$(shell cat status | sed -e 's/ .*$$//')
TARGET=/var/home/mailman/templates/ru/
WEB=/var/www/mailman-ru
WFILES_DIR=/var/www/mailman-ru/files

TFILES=$(addprefix $(TARGET),$(FILES))
WFILES=$(addprefix $(WEB),$(FILES))

diff_args=-ub

all:
	@echo there is nothing to do

diff-%: %
	@-(cd ../en/ ; cvs -z6 diff $(diff_args) -r$(shell fgrep $< status | awk '{ print $$2; }') $<)

check:
	@(cd ../en/ ; cvs -z6 -q status ) | ./s2s.py

todo:
	@cat status | while read name version status rest; do [ "$$status" == "done" ] || echo $$name; done | sort

done:
	@cat status | while read name version status rest; do [ "$$status" == "done" ] && echo $$name; done | sort

started:
	@cat status | while read name version status rest; do [ "$$status" == "started" ] && echo $$name; done | sort

status-%: %
	@ cat status | grep "^$< "

$(WEB)/%: %
	cp $< $@

#update: $(WFILES)
update: update-files update-status

update-files:
	install -d $(WFILES_DIR)
	@for file in $(FILES); do if [ ! -e $(WFILES_DIR)/$$file -o $$file -nt $(WFILES_DIR)/$$file ]; then cp -vf $$file $(WFILES_DIR)/$$file; fi; done

update-status: $(WEB)/status.html

$(WEB)/status.html: status s2h.py
	./s2h.py > $@

install:
	sudo -u mailman install -d $(TARGET)
	sudo -u mailman install $(FILES) $(TARGET)
