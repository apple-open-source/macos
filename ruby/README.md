Building:

The build requires a case sensitive filesystem and to be built from x86_64.
Build under rosetta and with a root location You can specify a location to
xbs with: `--rootsDirectory /Volumes/BUILDS/ruby/`

Example:

`arch -x86_64 xbs buildit -update CurrentRome -project ruby ./ --rootsDirectory /Volumes/BUILDS/ruby/ --archive`
