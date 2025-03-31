# directories that will be created by this script
COVERITY_CONFIG_DIR=coverity/config
COVERITY_IDIR=coverity/idir
COVERITY_RESULTS_DIR=coverity/results

# make sure we're in the root directory
if test ! -d icu/icu4c/source; then
    echo "Please run this script from the root directory of the Apple ICU repo."
    exit
fi

# make sure we don't already have results from Coverity
if test -d $COVERITY_RESULTS_DIR; then
    echo "$COVERITY_RESULTS_DIR exists. Please rename or remove your old results directory before running this script again."
    exit
fi

# function to print usage info and exit
function usage {
    echo "Usage: $(basename $0) [json]"
    echo "       (default format is html)"
    exit 1
}

# check the command line arguments count
if [ "$#" -gt 1 ]; then
    usage
fi

# check if the user is asking for json
coverity_output_format='html'
if [ "$#" -eq 1 ]; then
    if [ "$1" == 'json' ]; then
        coverity_output_format='json'
    else
       echo "unrecognized format: $1"
       usage
    fi
fi

# work around the fact that we're on bleeding edge software
export COVERITY_FORCE_XCODEBUILD_WORKAROUND=1
export COVERITY_UNSUPPORTED=1

# set RC_ARCHS for use by ICU's makefile
export RC_ARCHS=`arch`

# Xcode masquerades clang as GCC and G++, so configure Coverity appropriately
cov-configure --config $COVERITY_CONFIG_DIR/coverity_config.xml --template --compiler gcc --comptype clangcc
cov-configure --config $COVERITY_CONFIG_DIR/coverity_config.xml --template --compiler cc --comptype clangcc
cov-configure --config $COVERITY_CONFIG_DIR/coverity_config.xml --template --compiler c++ --comptype clangcxx
cov-configure --config $COVERITY_CONFIG_DIR/coverity_config.xml --clang

# do the actual static analysis
coverity scan --compiler-config-file $COVERITY_CONFIG_DIR/coverity_config.xml --dir $COVERITY_IDIR --local $COVERITY_RESULTS_DIR --local-format $coverity_output_format -- make icu
