DIRS=lib bin etc test

all:
	for i in $(DIRS); do echo " "; echo Building $$i:; cd $$i ; make $(MAKE_ARGS) $@ ; cd .. ; done

install:
	for i in $(DIRS); do echo " "; echo Installing $$i:; cd $$i ; make install $@; cd .. ; done

clean:
	for i in $(DIRS); do echo " "; echo Cleaning $$i:; cd $$i ; make $(MAKE_ARGS) $@; cd .. ; done

