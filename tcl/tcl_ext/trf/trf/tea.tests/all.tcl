# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.test" when running tcltest
# in this directory.
#
# Copyright (c) 1998-2000 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: all.tcl,v 1.1 2002/08/23 18:04:40 andreas_kupries Exp $

package require tcltest 2.2
namespace import -force tcltest::*

source test.setup

eval tcltest::configure $argv
tcltest::configure -testdir [file normalize [file dirname [info script]]]
tcltest::configure -singleproc 1
tcltest::runAllTests
