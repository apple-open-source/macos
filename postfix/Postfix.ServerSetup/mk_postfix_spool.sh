#!/bin/sh
#
# Copyright (c) 2016, 2021 Apple Inc. All rights reserved.
# 
# IMPORTANT NOTE: This file is licensed only for use on Apple-branded
# computers and is subject to the terms and conditions of the Apple Software
# License Agreement accompanying the package this file is a part of.
# You may not port this file to another platform without Apple's written consent.
# 
# Redistribution and use in source and binary forms, with or without  
# modification, are permitted provided that the following conditions  
# are met:
# 
# 1.  Redistributions of source code must retain the above copyright  
# notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above  
# copyright notice, this list of conditions and the following  
# disclaimer in the documentation and/or other materials provided  
# with the distribution.
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
# contributors may be used to endorse or promote products derived  
# from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND  
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF  
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
# SUCH DAMAGE.

_vol_path=$1
_spool_path=/var/spool/postfix
_spool_dirs="active bounce corrupt defer deferred flush hold incoming maildrop pid private public saved trace"
_public_pipes="pickup qmgr"
_public_sockets="cleanup flush showq"
_private_sockets="anvil discard error policy relay sacl-cache smtpd trace bounce dnsblog lmtp proxymap retry scache tlsmgr verify defer dovecot local proxywrite rewrite smtp tlsproxy virtual"

if [ $# -ne 1 ]; then
	echo "Usage: $0 volume-path" >&2
	exit 1
fi

# check for volume path
if [ ! -d "$_vol_path" ] ; then
	echo "no volume: $_vol_path" >&2
	exit 1
fi

_full_path="$_vol_path$_spool_path"

# create full path on volume
if [ ! -d "$_full_path" ] ; then
	mkdir -p $_full_path
	if [ ! -d "$_full_path" ] ; then
		echo "cannot create $_full_path" >&2
		exit 1
	fi
fi

# create postfix spool directories
for _spool_dir_name in $_spool_dirs ; do
	_spool_dir="$_full_path/$_spool_dir_name"
	if [ ! -d "$_spool_dir" ] ; then
		mkdir -p $_spool_dir
		if [ ! -d "$_spool_dir" ] ; then
			echo "cannot create $_full_path" >&2
			exit 1
		fi
		case "$_spool_dir_name" in 
		  "pid")
			chown root:wheel "$_spool_dir"
			chmod 755 "$_spool_dir"
			;;
		  "maildrop")
			chown _postfix:_postdrop "$_spool_dir"
			chmod 730 "$_spool_dir"
			;;
		  "public")
			chown _postfix:_postdrop "$_spool_dir"
			chmod 710 "$_spool_dir"
			;;
		  *)
			chown _postfix:wheel "$_spool_dir"
			chmod 700 "$_spool_dir"
			;;
		esac
	fi
done

# create public sockets
for _socket in $_public_sockets ; do
	_socket_path="$_full_path/public/$_socket"
	if [ ! -e "$_socket_path" ] ; then
		/usr/libexec/postfix/bind_unix_socket $_socket_path
		chmod 666 $_socket_path
	fi
done

# create public pipes
for _pipe in $_public_pipes ; do
	_pipe_path="$_full_path/public/$_pipe"
	if [ ! -e "$_pipe_path" ] ; then
		mkfifo -m 622 "$_pipe_path"
	fi
done

# create private sockets
for _socket in $_private_sockets ; do
	_socket_path="$_full_path/private/$_socket"
	if [ ! -e "$_socket_path" ] ; then
		/usr/libexec/postfix/bind_unix_socket $_socket_path
		chmod 666 $_socket_path
	fi
done

