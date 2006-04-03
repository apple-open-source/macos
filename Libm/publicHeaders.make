#########################################################
###
###  variable
###
PROJECT_HDRROOT		=	$(DSTROOT)/usr/include/
PROJECT_HDRROOT_PPC	=	$(PROJECT_HDRROOT)/architecture/ppc/
PROJECT_HDRROOT_I386	=	$(PROJECT_HDRROOT)/architecture/i386/


	
#########################################################
###
###  build target
###
main:	installhdrs

installsrc:
	
installhdrs:	create_dir
	@echo Generate headers
	@cp $(SRCROOT)/fenv.h $(PROJECT_HDRROOT)
	@cp $(SRCROOT)/math.h $(PROJECT_HDRROOT)
	@cp $(SRCROOT)/xmmLibm.subproj/Headers/architecture/ppc/fenv.h $(PROJECT_HDRROOT_PPC)
	@cp $(SRCROOT)/ppc.subproj/math.h $(PROJECT_HDRROOT_PPC)
	@cp $(SRCROOT)/xmmLibm.subproj/Headers/architecture/i386/fenv.h $(PROJECT_HDRROOT_I386)
	@cp $(SRCROOT)/i386.subproj/math.h $(PROJECT_HDRROOT_I386)


install: installhdrs main			

clean: 	
	
#########################################################
###
###  directory target
###
create_dir: $(PROJECT_HDRROOT) $(PROJECT_HDRROOT_PPC) $(PROJECT_HDRROOT_I386)		
	
$(PROJECT_HDRROOT):	
	@/bin/mkdir -p $(PROJECT_HDRROOT)
        
$(PROJECT_HDRROOT_PPC):
	@/bin/mkdir -p $(PROJECT_HDRROOT_PPC)
        
$(PROJECT_HDRROOT_I386):
	@/bin/mkdir -p $(PROJECT_HDRROOT_I386)
	
remove_dir:
	@/bin/rm -rf $(PROJECT_HDRROOT) $(PROJECT_HDRROOT_PPC) $(PROJECT_HDRROOT_I386)
	
