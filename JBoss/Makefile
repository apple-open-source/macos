TOMCAT=jakarta-tomcat-LE-jdk14
TARGETDIR:=$(DSTROOT)/Library/JBoss/3.2
LOGDIR:=$(DSTROOT)/Library/Logs/JBoss
JBOSS_ROOT:=$(SYMROOT)/jboss-all
JAVA_HOME:=/Library/Java/Home

install:
	ditto $(SRCROOT) $(SYMROOT)
	cd $(JBOSS_ROOT); /bin/sh ./build/build.sh
	mkdir -p $(LOGDIR)
	mkdir -p $(TARGETDIR)/deploy $(TARGETDIR)/services
	ditto $(JBOSS_ROOT)/build/output/jboss-3.2.3  $(TARGETDIR)
	#snip	
	cp $(TARGETDIR)/docs/examples/jmx/ejb-management.jar $(TARGETDIR)/services
	cp $(TARGETDIR)/docs/examples/jmx/ejb-management.jar $(TARGETDIR)/server/all/deploy/management
	#remove obsolete files in all configs
	rm -rf $(TARGETDIR)/docs $(TARGETDIR)/bin/*.bat $(TARGETDIR)/bin/jboss_init_redhat.sh
	cd $(TARGETDIR)/server; rm -rf minimal default
	cd $(TARGETDIR)/server/all/lib; rm -f scheduler-plugin-example.jar jnet.jar jcert.jar tyrex.jar jsse.jar
	rm $(TARGETDIR)/server/all/conf/jboss-minimal.xml
	rm $(TARGETDIR)/server/all/deploy/{user-service.xml,jms/jbossmq-destinations-service.xml}
	cd $(TARGETDIR)/server/all/deploy; cp -r *.xml *.sar *.rar jms/* management/* deploy.last/* $(TARGETDIR)/services
	cd $(JBOSS_ROOT); cp -pr ./jetty/output/lib/jetty-plugin.sar ./varia/output/lib/foe-deployer-3.2.sar $(TARGETDIR)/services
	rm $(TARGETDIR)/services/*-ds.xml
	TMPDIR=/tmp/ajhsgd$$$$; mkdir $$TMPDIR; cd $(TARGETDIR)/services; SARS=`find . -type d -name \*.sar`; mv $$SARS $$TMPDIR; cd $$TMPDIR; for i in * ; do cd $$i; jar cf $(TARGETDIR)/services/$$i *; cd ..; done
	rm -rf $(TARGETDIR)/server/all/farm
	cd $(TARGETDIR)/server/all/deploy; rm scheduler-service.xml schedule-manager-service.xml
	cd $(TARGETDIR)/server/all/; ln -s /Library/Logs/JBoss "log"
	cd $(TARGETDIR)/server; mv all deploy-cluster; ditto deploy-cluster deploy-standalone; ditto deploy-standalone develop
	# develop config
	cd $(TARGETDIR)/server/develop/deploy; rm -r cluster-service.xml deploy.last/farm-service.xml cache-invalidation-service.xml jbossha-httpsession.sar jboss-net.sar http-invoker.sar jms/jbossmq-hail.sar
	# deploy-standalone
	cd $(TARGETDIR)/server/deploy-standalone/deploy; rm -r cluster-service.xml jmx-console.war deploy.last/farm-service.xml cache-invalidation-service.xml jbossha-httpsession.sar jboss-net.sar http-invoker.sar management/{web-console.war,console-mgr.sar} jms/jbossmq-hail.sar
        #
        # deploy-cluster
	cd $(TARGETDIR)/server/deploy-cluster/deploy; rm -r http-invoker.sar jmx-console.war jboss-net.sar management/{web-console.war,console-mgr.sar}
	# JDK 1.4 stuff we can remove
	#cd $(TARGETDIR)/lib; rm xercesImpl.jar xml-apis.jar
	cd $(TARGETDIR)/client; rm jnet.jar jcert.jar gnu-regexp.jar jsse.jar
	gnutar cf - server bin lib deploy | (cd $(TARGETDIR); gnutar xf -)
	chmod +x $(TARGETDIR)/bin/*.sh
	find $(TARGETDIR) -name CVS | xargs rm -rf
	#snip
	cp -pr $(SRCROOT)/$(TOMCAT) $(DSTROOT)/Library/Tomcat
	rm $(DSTROOT)/Library/Tomcat/bin/*.bat $(DSTROOT)/Library/Tomcat/bin/*.exe
	cp -r tomcat/* $(DSTROOT)/Library/Tomcat
	find $(DSTROOT)/Library/Tomcat -name CVS | xargs rm -rf
	rm -rf $(DSTROOT)/Library/JBoss/3.2/server/*/deploy/jbossweb-tomcat.sar
	rm -rf $(DSTROOT)/Library/Tomcat/logs
	mkdir $(DSTROOT)/Library/JBoss/3.2/run $(DSTROOT)/Library/JBoss/3.2/farm
	ln -s "/Library/Logs/JBoss" $(DSTROOT)/Library/Tomcat/logs

magnum: install
	cd $(TARGETDIR)/server; ln -s deploy-standalone default

devtools: install
	cd $(TARGETDIR)/server; ln -s develop default

installhdrs: 

installsrc:
	gnutar cf - bin deploy lib server tomcat Makefile | (cd $(SRCROOT); gnutar xf -)
	gnutar -xz -C $(SRCROOT) -f jb.tar.gz
	gnutar -xz -C $(SRCROOT) -f $(TOMCAT).tar.gz
	cp local.properties $(SRCROOT)/jboss-all/build/etc/local.properties-example

clean:

