project=KerberosHelper

sudo rm -rf /tmp/$project.dst && \
    time xcodebuild install \
    	CODE_SIGN_IDENTITY=lha-codesign-cert \
	&& \
    sudo chown -R root:wheel /tmp/$project.dst && \
    sudo ditto /tmp/$project.dst /
