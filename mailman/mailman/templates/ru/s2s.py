#! /usr/bin/python

# A simple script to check the status of the translation.

import sys, string
from pprint import pprint

def chop (line):
    if line[-2:] == '\r\n':
        line = line[:-2]

    if line[-1:] == '\n':
        line = line[:-1]

    return line

def cmprevision (a, b):
    '''revisions are something delimited with dots'''

    return cmp (map (lambda x: x.lower (), a.split ('.')), map (lambda x: x.lower (), b.split ('.')))

name = None
revision = None

files = {}

for line in sys.stdin.readlines ():
    parts = string.split (chop (line))

    if len (parts) > 0:
        if parts[0] == 'File:':
            name = parts[1]
        elif parts[0] == 'Repository':
            files[name] = parts[2]

# pprint (files)

action = 0

for line in open ('status', 'r').readlines ():
    parts = string.split (chop (line))

    if len (parts) > 0:
        if files.has_key (parts[0]):
            pass    # check the version

            relation = cmprevision (parts[1], files[parts[0]])

            if relation < 0:
                print 'Update: %s (%s -> %s)' % (parts[0], parts[1], files[parts[0]])
                action = 1
            elif relation > 0:
                print 'Downgrade?: %s (%s -> %s)' % (parts[0], parts[1], files[parts[0]])
                action = 1

            del files[parts[0]] # delete the item
        else:
            print 'Delete:', parts[0]
            action = 1

for file in files.keys ():
    print 'New:', file
    action = 1

if not action:
    print 'You are translating the latest versions'
