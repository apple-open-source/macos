#!/usr/local/bin/python3

# Executing this file directly will run tests against the functions herein.

import sys

def parse_primary_appleid(output):
    idx = output.find(b'accountClass = primary')
    if idx == -1:
        raise ValueError("Couldn't find primary accountClass to get Primary AppleID!")

    appleIdBytes = b'appleId = '
    idx = output.find(appleIdBytes)
    if idx == -1:
        raise ValueError("Couldn't find appleId to get Primary AppleID!")

    qStart = idx + len(appleIdBytes)

    qEnd = output.find(b';',qStart)
    if qEnd == -1:
        raise ValueError("Couldn't find EOL to get Primary AppleID!")

    qEnd -= 1 # remove ;

    if output[qStart] == b'"'[0]:
        qStart += 1

    if output[qEnd] == b'"'[0]:
        qEnd -= 1

    idBytes = output[qStart:qEnd+1]

    try:
        return idBytes.decode()
    except:
        print("Couldn't decode Primary AppleID:", idBytes)
        print("Exception follows:")
        raise

def test_parse_primary_appleid():
  tests = [
            {'name':"empty bytes", 'bytes':b'', 'throws':"Couldn't find primary accountClass to get Primary AppleID!"},
            {'name':"accountClass but nothing else", 'bytes':b'    accountClass = primary;\n', 'throws':"Couldn't find appleId to get Primary AppleID!"},
            {'name':"simple phone number appleID", 'bytes':b'    accountClass = primary;\n        appleId = 8618511878342;\n', 'expected':"8618511878342"},
            {'name':"simple email appleID", 'bytes':b'    accountClass = primary;\n    appleId = "peter.931432.n65@icloud.com";\n', 'expected':"peter.931432.n65@icloud.com"},
          ]

  failures = []

  for x in tests:
    try:
      o = parse_primary_appleid(x['bytes'])
      if o != x['expected']:
        print("parse_primary_appleid '{}' failed to produce expected output. Got '{}' expected '{}'".format(x['name'], o, x['expected']))
        failures.append(x['name'])
    except ValueError as e:
      if x['throws'] != e.args[0]:
        print("parse_primary_appleid '{}' threw unexpected ValueError: {}".format(x['name'],e))
        failures.append(x['name'])
    except Exception as e:
        print("parse_primary_appleid '{}' threw unexpectedly: {}".format(x['name'], e))
        failures.append(x['name'])

  print("Ran {} tests, {} failed".format(len(tests), len(failures)))
  if len(failures) > 0:
    sys.exit(-1)

if __name__ == '__main__':
  test_parse_primary_appleid()
