# -*-makefile-*-

APPNAME = CocoHako
BUNDLEID = rubyapp.$(APPNAME)

RUBYSRCS = rb_main.rb cocoa_hako.rb hako.rb
OBJS = main_0.3.o

TARGET = $(APPNAME).app
CFLAGS = -Wall

SED_CMD_0 = -e "s/%%%APPNAME%%%/$(APPNAME)/"
SED_CMD_1 = -e "s/%%%BUNDLEID%%%/$(BUNDLEID)/"


$(TARGET): $(OBJS) $(RUBYSRCS)
	$(CC) $(OBJS) -framework Foundation
	-/bin/rm -rf $(APPNAME).app
	mkdir $(APPNAME).app
	mkdir $(APPNAME).app/Contents
	mkdir $(APPNAME).app/Contents/MacOS
	mkdir $(APPNAME).app/Contents/Resources
	mv a.out $(APPNAME).app/Contents/MacOS/$(APPNAME)
	sed $(SED_CMD_0) $(SED_CMD_1) Info.plist.tmpl > $(APPNAME).app/Contents/Info.plist
	echo -n "APPL????" > $(APPNAME).app/Contents/PkgInfo
	cp -p $(RUBYSRCS) $(APPNAME).app/Contents/Resources/

clean:
	-/bin/rm -rf $(APPNAME).app *.o a.out *~ core
