project=Heimdal

sudo rm -rf /tmp/${project}-em.dst && \
    time xcodebuild install \
	-target HeimdalEmbedded -configuration Release-Embedded \
        DSTROOT="/tmp/\$PROJECT_NAME-em.dst" \
        CODE_SIGN_IDENTITY='iPhone Developer:' \
	RC_PURPLE="YES" \
        "$@" \
    && \
    sudo chown -R root:wheel /tmp/${project}-em.dst && \
    sudo ditto /tmp/${project}-em.dst /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.Internal.sdk && \
    sudo sh scripts/em-ent.sh
