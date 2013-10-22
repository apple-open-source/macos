#!/bin/sh

release=OSX108

buildit . -project=KerberosHelper -release $release -rootsDirectory $HOME/BuildRoots -arch i386 -arch x86_64 -merge /  "$@"
buildit . -project=KerberosHelper_executables -target KerberosHelper_executables -release $release -rootsDirectory $HOME/BuildRoots -arch x86_64 -merge /  "$@"

