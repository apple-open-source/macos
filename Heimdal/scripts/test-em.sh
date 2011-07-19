#!/bin/sh

sudo buildit . \
    -target HeimdalEmbedded \
    -project Heimdal \
    -configuration Release-Embedded "$@"
