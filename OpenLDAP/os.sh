#!/bin/sh

sw_vers | grep ProductVersion | awk '{print $2}'
