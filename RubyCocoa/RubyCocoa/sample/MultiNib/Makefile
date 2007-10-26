# -*-makefile-*-

APPNAME = MultiNibSample
RUBYSRCS = ApplicationDelegate.rb WindowAController.rb WindowBController.rb

NIBDIR = English.lproj
RUBYSRC_MAIN = rb_main.rb
OBJS = main.o
LIBS = -lobjc -framework RubyCocoa

TARGET = $(APPNAME).app
CFLAGS = -Wall


$(TARGET): $(OBJS) $(RUBYSRCS) $(RUBYSRC_MAIN)
	$(CC) $(OBJS) $(LIBS)
	-/bin/rm -rf $(APPNAME).app
	mkdir $(APPNAME).app
	mkdir $(APPNAME).app/Contents
	mkdir $(APPNAME).app/Contents/MacOS
	mkdir $(APPNAME).app/Contents/Resources
	mv a.out $(APPNAME).app/Contents/MacOS/$(APPNAME)
	cp -p Info.plist $(APPNAME).app/Contents/
	printf "APPL????" > $(APPNAME).app/Contents/PkgInfo
	cp -p $(RUBYSRC_MAIN) $(RUBYSRCS) $(APPNAME).app/Contents/Resources/
	cp -R $(NIBDIR) $(APPNAME).app/Contents/Resources/

clean:
	-/bin/rm -rf $(APPNAME).app *.o a.out *~ core
