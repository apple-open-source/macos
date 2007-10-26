NAME = bonjour-py
VERSION = 0.2

include ../Makefile.inc

custompatching:
	ed - $(DIR)/bonjour.i < bonjour.i.ed
	ed - $(DIR)/setup.py < setup.py.ed
