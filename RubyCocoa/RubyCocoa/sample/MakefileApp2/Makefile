# -*-makefile-*-

APPNAME = SimpleApp1
BUNDLEID = rubyapp.$(APPNAME)
NIBDIR = English.lproj

RUBYSRCS = rb_main.rb AppController.rb MyView.rb
OBJS = main.o

TARGET = $(APPNAME).app
CFLAGS = -Wall
LIBS = -lobjc -framework RubyCocoa

SED_CMD_0 = -e "s/%%%APPNAME%%%/$(APPNAME)/"
SED_CMD_1 = -e "s/%%%BUNDLEID%%%/$(BUNDLEID)/"


$(TARGET): $(OBJS) $(RUBYSRCS)
	$(CC) $(OBJS) $(LIBS)
	-/bin/rm -rf $(APPNAME).app
	mkdir $(APPNAME).app
	mkdir $(APPNAME).app/Contents
	mkdir $(APPNAME).app/Contents/MacOS
	mkdir $(APPNAME).app/Contents/Resources
	mv a.out $(APPNAME).app/Contents/MacOS/$(APPNAME)
	sed $(SED_CMD_0) $(SED_CMD_1) Info.plist.tmpl > $(APPNAME).app/Contents/Info.plist
	printf "APPL????" > $(APPNAME).app/Contents/PkgInfo
	cp -p $(RUBYSRCS) $(APPNAME).app/Contents/Resources/
	cp -R $(NIBDIR) $(APPNAME).app/Contents/Resources/

clean:
	-/bin/rm -rf $(APPNAME).app *.o a.out *~ core
