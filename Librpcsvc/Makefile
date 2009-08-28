Project = rpcsvc
Install_Dir = /usr/include/rpcsvc

XFILES = bootparam_prot.x klm_prot.x mount.x nfs_prot.x\
	nlm_prot.x rex.x rnusers.x rquota.x rstat.x rusers.x\
	rwall.x sm_inter.x spray.x yp.x yppasswd.x

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

DESTDIR=$(DSTROOT)$(Install_Dir)

install:: installhdrs

installhdrs::
	$(INSTALL_DIRECTORY) $(DESTDIR)
	@for FILE in $(XFILES); do \
		CMD="$(INSTALL_FILE) $${FILE} $(DESTDIR)" ; \
		echo $${CMD} ; $${CMD} || exit 1 ; \
		\
		OUT=`basename $${FILE} .x` ; \
		CMD="$(RPCGEN) $(RPCFLAGS) -h \
		-o $(OBJROOT)/$${OUT}.h $${FILE}"; \
		echo $${CMD} ; $${CMD} || exit 1 ; \
		\
		CMD="$(INSTALL_FILE) $(OBJROOT)/$${OUT}.h $(DESTDIR)" ; \
		echo $${CMD} ; $${CMD} || exit 1 ; \
	done
