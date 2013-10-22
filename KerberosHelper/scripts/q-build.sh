project=KerberosHelper

sudo rm -rf /Heimdal/KerberosHelper{_frameworks,_executables,Merged}
test -d /Heimdal || sudo mkdir /Heimdal

(sudo rm -rf /tmp/$project.dst && \
    time xcodebuild install  -target KerberosHelper_frameworks \
	&& \
    sudo chown -R root:wheel /tmp/$project.dst && \
    sudo ditto /tmp/$project.dst /Heimdal/KerberosHelper_frameworks && \
    sudo ditto /tmp/$project.dst /Heimdal/KerberosHelperMerged && \
    sudo ditto /tmp/$project.dst /) || { echo "KerberosHelper_frameworks failed" ; exit 1; }

(sudo rm -rf /tmp/$project.dst && \
    time xcodebuild install -target KerberosHelper_executables \
	&& \
    sudo chown -R root:wheel /tmp/$project.dst && \
    sudo ditto /tmp/$project.dst /Heimdal/KerberosHelper_executables && \
    sudo ditto /tmp/$project.dst /Heimdal/KerberosHelperMerged && \
    sudo ditto /tmp/$project.dst / ) || { echo "KerberosHelper_executables failed" ; exit 1; }

