#!/usr/bin/env python
#
# Client-side utility for DAV lock manipulation
#

import sys
import davlib
import getopt
import urlparse
import urllib
import string

def usage():
  print 'USAGE: %s [-s] [-d depth] <command> <url> [locktoken]' % sys.argv[0]
  print '  where <command> is one of:'
  print '    view   -- view locks on the specified URL'
  print '    lock   -- add a lock to the URL'
  print '    unlock -- remove a lock from the URL (requires locktoken)'
  print
  print '  -s can be used with the lock command to create a shared lock'
  print '  -d can be used with view or lock and specifies the depth for'
  print '     the operation. It can be one of: 0, 1, infinity'
  sys.exit(1)

def _print_status(ob):
  print '    STATUS:', ob.status[0], ob.status[1]
  if ob.responsedescription:
    print '           ', ob.responsedescription

def _print_response(response):
  print 'ERROR: unexpected response:'
  print '  Status:', response.status, response.reason
  print '  Body:'
  print '-' * 70
  print response.read()
  print '-' * 70

def do_view(conn, url, shared, depth, extra):
  response = conn.getprops(url, 'lockdiscovery', ns='DAV:', depth=depth)
  if response.status != 207:
    _print_response(response)
    return

  response.parse_multistatus()

  for resp in response.msr.responses:
    if len(resp.href) != 1:
      # 0 or more than 1 DAV:href values were given(!)
      print 'ERROR: strange response found: expected 1 DAV:href, but ' \
            'found %d.' % len(resp.href)
      continue
    if resp.status and resp.status[0] != 200:
      print 'HREF:', resp.href[0]
      _print_status(resp)
      continue

    if len(resp.propstat) != 1:
      # 0 or more than 1 DAV:propstat elements were given(!)
      print 'ERROR: strange response found: expected 1 DAV:propstat, but ' \
            'found %d.' % len(resp.propstat)
      continue
    ps = resp.propstat[0]
    ld = ps.prop.get(('DAV:', 'lockdiscovery'))
    if len(ps.prop) != 1 or not ld:
      print 'ERROR: strange response found: asked for DAV:lockdiscovery, '
      print '       but did not get it back in the response, and/or'
      print '       received other properties.'
      continue

    if ps.status and ps.status[0] == 404:
      print 'HREF:', resp.href[0], '(no locks)'
      continue

    print 'HREF:', resp.href[0]

    if ps.status and ps.status[0] != 200:
      print '    for the DAV:lockdiscovery property:'
      _print_status(ps)
      continue

    for active in ld.children:
      if active.ns != 'DAV:' or active.name != 'activelock':
        continue
      scope = active.find('lockscope', 'DAV:')
      type = active.find('locktype', 'DAV:')
      depth = active.find('depth', 'DAV:')
      owner = active.find('owner', 'DAV:')
      timeout = active.find('timeout', 'DAV:')
      locktoken = active.find('locktoken', 'DAV:')

      if scope.find('exclusive', 'DAV:'):
        scope = 'exclusive'
      elif scope.find('shared', 'DAV:'):
        scope = 'shared'
      else:
        scope = 'unknown scope'

      if type.find('write', 'DAV:'):
        type = 'write'
      else:
        type = 'unknown type'

      depth = depth.textof()

      if owner:
        owner = ' owner: (owner)'	### fix this
      else:
        owner = ''

      if timeout:
        timeout = ' timeout: ' + timeout.textof()
      else:
        timeout = ''

      tokens = []
      if locktoken:
        for href in locktoken.children:
          if href.ns == 'DAV:' and href.name == 'href':
            tokens.append(href.textof())

      print '    LOCK: %s, %s, depth=%s.%s%s' % \
            (scope, type, depth, owner, timeout)
      for token in tokens:
        print '         ', token

def do_lock(conn, url, shared, depth, extra):
  if shared:
    scope = 'shared'
  else:
    scope = 'exclusive'
  response = conn.lock(url, depth=depth, scope=scope)
  if response.status != 200:
    _print_response(response)
  else:
    response.parse_lock_response()
    print 'Locked. Token:', response.locktoken

def do_unlock(conn, url, shared, depth, extra):
  if len(extra) != 1:
    usage()

  response = conn.unlock(url, extra[0])
  if response.status != 204:
    _print_response(response)
  else:
    print 'Unlocked.'

commands = {
  'view' : do_view,
  'lock' : do_lock,
  'unlock' : do_unlock,
  }

def main():

  optlist, args = getopt.getopt(sys.argv[1:], 'sd:')

  shared = 0
  depth = 'infinity'
  for opt, value in optlist:
    if opt == '-d':
      if value not in ['0', '1', 'infinity']:
        print 'ERROR: "%s" is not a valid depth' % value
        sys.exit(1)
      depth = value
    elif opt == '-s':
      shared = 1

  if len(args) < 2 or len(args) > 3:
    usage()

  command = args[0]
  if not commands.has_key(command):
    usage()

  url = args[1]
  extra = args[2:]

  scheme, netloc, path, params, query, fragment = urlparse.urlparse(url)
  host, port = urllib.splitnport(netloc, 80)
  conn = davlib.DAV(host, port)

  commands[command](conn, path, shared, depth, extra)

if __name__ == '__main__':
  main()
