project=Heimdal

sudo rm -rf /tmp/$project.dst && \
    time xcodebuild install -target HeimdalCompilers -configuration Release \
        "$@" \
    && \
    sudo chown -R root:wheel /tmp/$project.dst && \
    sudo ditto /tmp/$project.dst /
