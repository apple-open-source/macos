#!/bin/sh

set -ex

perl -MDevel::Cover -e 1 || exit 1

cover -delete
env HARNESS_PERL_SWITCHES=-MDevel::Cover make test
cover
perl -wIlib -MPod::Coverage=Graph -e1 | tee podcoverage.out

exit 0
