project=MITKerberosShim

sudo rm -rf /tmp/$project.dst && xcodebuild install HEIMDAL_ROOT=/ && sudo chown -R root:wheel /tmp/$project.dst && sudo ditto /tmp/$project.dst /
