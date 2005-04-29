#!/usr/bin/env python
"""SPF (Sender-Permitted From) test script.

Copyright (c) 2003, Terence Way
This module is free software, and you may redistribute it and/or modify
it under the same terms as Python itself, so long as this copyright message
and disclaimer are retained in their original form.

IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF
THIS CODE, EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE.  THE CODE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS,
AND THERE IS NO OBLIGATION WHATSOEVER TO PROVIDE MAINTENANCE,
SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
"""

# Changes:
#    None yet

__author__ = "Terence Way"
__email__ = "terry@wayforward.net"
__version__ = "1.0: December 9, 2003"
MODULE = 'spf'

import DNS	# pydns.sourceforge.net

MASK = 0xFFFFFFFFL

def get_spf(domainname):
	"""Get the SPF record recorded in DNS for a specific domain name.
	Returns None if not found.
	"""
	req = DNS.DnsRequest(domainname, qtype='txt')
	resp = req.req()

	for s in resp.answers:
		for d in s['data']:
			if d.startswith('v=spf1'):
				return d
	else:
		# Not found
		return None

def parse_mechanism(str, domainname):
	"""
	Examples:
	>>> parse_mechanism('a', 'foo.com')
	('a', 'foo.com', 32)
	>>> parse_mechanism('a:bar.com', 'foo.com')
	('a', 'bar.com', 32)
	>>> parse_mechanism('a/24', 'foo.com')
	('a', 'foo.com', 24)
	>>> parse_mechanism('a:bar.com/16', 'foo.com')
	('a', 'bar.com', 16)
	"""
	a = str.split('/')
	if len(a) == 2:
		a, port = a[0], int(a[1])
	else:
		a, port = str, 32

	b = a.split(':')
	if len(b) == 2:
		return b[0], b[1], port
	else:
		return a, domainname, port

def check(ipaddr, domainname):
	"""Test an incoming email claiming to be from a domainname,
	coming in from a specific ipaddr.
	"""

	# Get the SPF DNS entry
	return check_spf(ipaddr, domainname, get_spf(domainname))

def check_spf(ipaddr, domainname, spf):
	"""Test an incoming email claiming to be from a domainname,
	passing in the SPF data."""
	if not spf:
		return (False, 200, 'no SPF record')

	mechanisms = spf.split()[1:]

	for m in spf.split()[1:]:
		if m.startswith('include:'):
			result = check(ipaddr, m[len('include:'):])
			if result[0]:
				return result

		if m[0] == '-':
			m = m[1:]
			result = (False, 550, 'access denied')
		elif m[0] == '?' or m[0] == '~':
			m = m[1:]
			result = (False, 250, 'SPF unknown')
		elif m[0] == '+':
			m = m[1:]
			result = (True, 250, 'sender validated via SPF')
		else: # assume + by default
			result = (True, 250, 'sender validated via SPF')

		if m == 'all':
			return result

		if m.startswith('exists:'):
			if len(get_a(m[len('exists:'):])) > 0:
				return result

		m = parse_mechanism(m, domainname)

		if m[0] == 'a':
			if cidrmatch(ipaddr, get_a(m[1]), m[2]):
				return result
		elif m[0] == 'mx':
			if cidrmatch(ipaddr, get_mx(m[1]), m[2]):
				return result
		elif m[0] == 'ip4' and m[1] != domainname:
			if cidrmatch(ipaddr, [m[1]], m[2]):
				return result
		elif m[0] == 'ptr':
			if domainmatch(validated_ptrdnames(ipaddr), m[1]):
				return result

		# unknown mechanisms cause immediate unknown abort result.
		result = (False, 250, 'SPF unknown')
		return result;

def get_a(domainname):
	"""Get a list of IP addresses for a domainname."""
	req = DNS.DnsRequest(domainname)
	resp = req.req()

	return [a['data'] for a in resp.answers]

def get_ptr(ipaddr):
	"""Get a list of domain names for an  IP address."""
	req = DNS.DnsRequest(myreverse(ipaddr) + ".in-addr.arpa", qtype='ptr')
	resp = req.req()

	return [a['data'] for a in resp.answers]

def myreverse(thingy):
	""" why couldn't python's reverse() just return a value just like any other function? """
	mythingy = thingy
	mythingy.reverse()
	return mythingy

def get_mx(domainname):
	"""Get a list of IP addresses for all MX exchanges for a domainname."""
	req = DNS.DnsRequest(domainname, qtype='mx')
	resp = req.req()

	result = []
	for mx in resp.answers:
		result += get_a(mx['data'][1])
	return result


def domainmatch(ptrdnames, domainsuffix):
	"""grep for a given domain suffix against a list of validated PTR domain names.

	Examples:
	>>> domainmatch(['foo.com'], 'foo.com')
	True
	>>> domainmatch(['moo.foo.com',] 'foo.com')
	True
	>>> domainmatch(['moo.bar.com'], 'foo.com')
	False

	"""
	for ptrdname in ptrdnames:
	   if ptrdname.lower() == domainsuffix.lower(): return True
	   if ptrdname.lower.endswith("." + domainsuffix.lower()): return True

	return False

def validated_ptrdnames(ipaddr):
	""" Figure out the validated PTR domain names for a given IP address.

	@ptrdnames = ptr_lookup(ipaddr);
	@validated = grep { ipaddr in a_lookup($_) } @ptrdnames;

	"""

	ptrdnames = get_ptr(ipaddr)
	validated = []
	for ptrdname in ptrdnames:
		ips = get_a(ptrdname)
		if ipaddr in ips:
			validated += ptrdname
	return validated


def cidrmatch(ipaddr, ipaddrs, cidr_length = 32):
	"""Match an IP address against a list of other IP addresses.

	Examples:
	>>> cidrmatch('192.168.0.45', ['192.168.0.44', '192.168.0.45'])
	True
	>>> cidrmatch('192.168.0.43', ['192.168.0.44', '192.168.0.45'])
	False
	>>> cidrmatch('192.168.0.43', ['192.168.0.44', '192.168.0.45'], 24)
	True
	"""
	c = cidr(ipaddr, cidr_length)
	for i in ipaddrs:
		if cidr(i, cidr_length) == c:
			return True
	return False

def cidr(addr, n):
	"""Convert an IP address string with a CIDR mask into a 32-bit
	integer.

	Examples:
	>>> DNS.bin2addr(cidr('192.168.5.45', 32))
	'192.168.5.45'
	>>> DNS.bin2addr(cidr('192.168.5.45', 24))
	'192.168.5.0'
	>>> DNS.bin2addr(cidr('192.168.0.45', 8))
	'192.0.0.0'
	"""
	return ~(MASK >> n) & MASK & DNS.addr2bin(addr)

def _test():
	import doctest, spf
	return doctest.testmod(spf)

if __name__ == '__main__':
	import sys
	if len(sys.argv) == 3:
		DNS.DiscoverNameServers()
		print check(sys.argv[1], sys.argv[2])
	elif len(sys.argv) == 1:
		_test()
	else:
		print "To check an incoming mail request:"
		print "     % python spf.py {ip-addr} {domain-name}"
		print
		print "To test this script:"
		print "     % python spf.py"

