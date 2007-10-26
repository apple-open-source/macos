#!/bin/sh

set -ex

cover -delete
env HARNESS_PERL_SWITCHES=-MDevel::Cover make test
cover
perl -wIlib -MPod::Coverage=Graph -e1 | tee podcoverage.out

exit 0
