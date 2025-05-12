#!/usr/bin/env python3

from sys import stderr
from xml.parsers import expat

xml = "<?xml version='1.0' encoding='iso8859'?><s>%s</s>" % ('a' * 1025)

class ExpectedException(Exception):
	pass

def handler(text):
	raise ExpectedException

try:
	parser = expat.ParserCreate()
	parser.CharacterDataHandler = handler
	parser.Parse(xml)
	print("FAIL: Expected to receive an exception", file=stderr)
	exit(1)
except ExpectedException:
	print("PASS: Received the expected exception")
	exit(0)
except:
	print("FAIL: Unexpected exception received", file=stderr)
	exit(1)
