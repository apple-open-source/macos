project=MITKerberosShim

sudo rm -rf /tmp/$project.dst && xcodebuild install GCC_OPTIMIZATION_LEVEL=0 HEIMDAL_ROOT=/ && sudo chown -R root:wheel /tmp/$project.dst && sudo ditto /tmp/$project.dst /
