#!/bin/sh -x
PROJROOT=`pwd`
NOKOGIRIGEM=`find ./ -name 'nokogiri*.gem' | xargs basename`

mkdir unpack
pushd unpack
tar xvf "$PROJROOT/gems/$NOKOGIRIGEM"
mkdir data
pushd data
tar xvf ../data.tar.gz
popd
gunzip metadata.gz checksums.yaml.gz
patch < "$PROJROOT/patches/0001-Patch-nokogiri-gem-to-build-correctly.patch"
rm data.tar.gz
pushd data
patch -p1 < "$PROJROOT/patches/0002-Patch-nokogiri-103206449.patch"
tar cvf ../data.tar *
popd
rm -rf data
gzip metadata data.tar
sha256data=`shasum -a 256 ./data.tar.gz | cut -d ' ' -f 1`
sha512data=`shasum -a 512 ./data.tar.gz | cut -d ' ' -f 1`
sha256meta=`shasum -a 256 ./metadata.gz | cut -d ' ' -f 1`
sha512meta=`shasum -a 512 ./metadata.gz | cut -d ' ' -f 1`

cat <<EOF > ./checksums.yaml
---
SHA256:
  metadata.gz: $sha256meta
  data.tar.gz: $sha256data
SHA512:
  metadata.gz: $sha512meta
  data.tar.gz: $sha512data
EOF

gzip checksums.yaml
tar cvf "$PROJROOT/gems/$NOKOGIRIGEM" *
popd
rm -rf unpack
