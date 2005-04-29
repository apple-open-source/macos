# class MkMemoIO implements  file-like objects that read/write a
# memo field in a Metakit database (either on file, or in memory).
#
# A simple adaptation of the StringIO.py module, by Jean-Claude Wippler.
#
# This implements (nearly) all stdio methods.
#
# f = MkMemoIO(view, memoprop, rownum) # setup for view[rownum].memoprop
# f.close()       # explicitly release resources held
# flag = f.isatty()   # always false
# pos = f.tell()    # get current position
# f.seek(pos)     # set current position
# f.seek(pos, mode)   # mode 0: absolute; 1: relative; 2: relative to EOF
# buf = f.read()    # read until EOF
# buf = f.read(n)   # read up to n bytes
# buf = f.readline()  # read until end of line ('\n') or EOF
# list = f.readlines()# list of f.readline() results until EOF
# f.write(buf)      # write at current position
# f.writelines(list)  # for line in list: f.write(line)
# f.getvalue()      # return whole file's contents as a string
#
# Notes:
# - fileno() is left unimplemented so that code which uses it triggers
# an exception early.
# - Seeking beyond EOF and then writing will insert garbage, unlike the
# StringIO class which clears, and from which this code was adapted.
# - There's a simple test set (see end of this file).
# - write() has an optional "resize" to insert (>0) or delete (<0) bytes.
# To merely insert/delete, write an empty string.  The resize value is
# adjusted to a "reasonable" range.  Inserted bytes are *not* cleared.

import string, sys

class MkMemoIO:
  def __init__(self, view, memo, row):
    self.view = view
    self.memo = memo
    self.row = row
    self.pos = 0
    self.closed = 0
    self.softspace = 0
  def close(self):
    if not self.closed:
      self.closed = 1
      del self.view, self.memo, self.row, self.pos
  def isatty(self):
    if self.closed:
      raise ValueError, "I/O operation on closed file"
    return 0
  def seek(self, pos, mode = 0):
    if self.closed:
      raise ValueError, "I/O operation on closed file"
    if mode == 1:
      pos = pos + self.pos
    elif mode == 2:
      pos = pos + self.view.itemsize(self.memo, self.row)
    self.pos = max(0, pos)
  def tell(self):
    if self.closed:
      raise ValueError, "I/O operation on closed file"
    return self.pos
  def read(self, n = -1):
    if self.closed:
      raise ValueError, "I/O operation on closed file"
    if n == 0:
      return ""
    r = self.view.access(self.memo, self.row, self.pos, n)
    self.pos = self.pos + len(r)
    return r
  def readline(self, length=None):
    if self.closed:
      raise ValueError, "I/O operation on closed file"
    remain = self.view.itemsize(self.memo, self.row) - self.pos
    if length is None:
      length = remain
    length = min(length, remain)
    i = -1
    oldpos = self.pos
    for o in xrange(0, length, 100):
      i = string.find(self.read(100), '\n')
      if i >= 0:
        break
    self.pos = oldpos
    if i >= 0:
      length = o+i+1
    return self.read(length)
  def readlines(self):
    lines = []
    line = self.readline()
    while line:
      lines.append(line)
      line = self.readline()
    return lines
  def write(self, s, resize=0):
    if self.closed:
      raise ValueError, "I/O operation on closed file"
    self.view.modify(self.memo, self.row, s, self.pos, resize)
    self.pos = self.pos + len(s)
  def writelines(self, list):
    for line in list: self.write(line)
  def flush(self):
    if self.closed:
      raise ValueError, "I/O operation on closed file"
  def getvalue(self):
    return self.view.access(self.memo, self.row, 0)


# A little test suite

def test():
  import Mk4py
  mk=Mk4py
  db=mk.storage()
  vw=db.getas('v[m:M]')
  vw.append()
  if sys.argv[1:]:
     file = sys.argv[1]
  else:
     file = '/etc/passwd'
  lines = open(file, 'r').readlines()
  text = open(file, 'r').read()
  f = MkMemoIO(vw,vw.m,0)
  for line in lines[:-2]:
    f.write(line)
  f.writelines(lines[-2:])
  if f.getvalue() != text:
    raise RuntimeError, 'write failed'
  length = f.tell()
  print 'File length =', length
  f.seek(len(lines[0]))
  f.write(lines[1])
  f.seek(0)
  print 'First line =', `f.readline()`
  here = f.tell()
  line = f.readline()
  print 'Second line =', `line`
  f.seek(-len(line), 1)
  line2 = f.read(len(line))
  if line != line2:
    raise RuntimeError, 'bad result after seek back'
  f.seek(len(line2), 1)
  list = f.readlines()
  line = list[-1]
  f.seek(f.tell() - len(line))
  line2 = f.read()
  if line != line2:
    raise RuntimeError, 'bad result after seek back from EOF'
  print 'Read', len(list), 'more lines'
  print 'File length =', f.tell()
  if f.tell() != length:
    raise RuntimeError, 'bad length'
  f.close()

if __name__ == '__main__':
  test()
