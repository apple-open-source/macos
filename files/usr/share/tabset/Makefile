Destination = $(DSTROOT)/usr/share/tabset

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs = 

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && echo $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_FILE) -c 3101		"$(Destination)/3101"
	$(_v) $(INSTALL_FILE) -c 9837		"$(Destination)/9837"
	$(_v) $(INSTALL_FILE) -c aa		"$(Destination)/aa"
	$(_v) $(INSTALL_FILE) -c aed512		"$(Destination)/aed512"
	$(_v) $(INSTALL_FILE) -c beehive	"$(Destination)/beehive"
	$(_v) $(INSTALL_FILE) -c diablo		"$(Destination)/diablo"
	$(_v) $(INSTALL_FILE) -c dtc382		"$(Destination)/dtc382"
	$(_v) $(INSTALL_FILE) -c hp700-wy	"$(Destination)/hp700-wy"
	$(_v) $(INSTALL_FILE) -c ibm3101	"$(Destination)/ibm3101"
	$(_v) $(INSTALL_FILE) -c std		"$(Destination)/std"
	$(_v) $(INSTALL_FILE) -c stdcrt		"$(Destination)/stdcrt"
	$(_v) $(INSTALL_FILE) -c tandem653	"$(Destination)/tandem653"
	$(_v) $(INSTALL_FILE) -c teleray	"$(Destination)/teleray"
	$(_v) $(INSTALL_FILE) -c vt100		"$(Destination)/vt100"
	$(_v) $(INSTALL_FILE) -c vt100-w	"$(Destination)/vt100-w"
	$(_v) $(INSTALL_FILE) -c wyse-adds	"$(Destination)/wyse-adds"
	$(_v) $(INSTALL_FILE) -c xerox1720	"$(Destination)/xerox1720"
	$(_v) $(INSTALL_FILE) -c xerox1730	"$(Destination)/xerox1730"
	$(_v) $(INSTALL_FILE) -c xerox1730-lm	"$(Destination)/xerox1730-lm"
	$(_v) $(INSTALL_FILE) -c zenith29	"$(Destination)/zenith29"
