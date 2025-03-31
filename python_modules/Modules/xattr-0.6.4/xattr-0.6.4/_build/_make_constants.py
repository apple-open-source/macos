infile = '/usr/include/sys/xattr.h'
out = file('Lib/xattr/constants.py', 'w')
for line in file(infile):
    line = line.rstrip()
    if line.startswith('/*') and line.endswith('*/'):
        print >>out,  ''
        print >>out,  '# ' + line[2:-2].strip()
    elif line.startswith('#define'):
        if lastblank:
            print >>out,  ''
        chunks = line.split(None, 3)
        if len(chunks) == 3:
            print >>out,  '%s = %s' % (chunks[1], chunks[2])
        elif len(chunks) == 4:
            comment = chunks[3].replace('/*', '').replace('*/', '').strip()
            print >>out,  '%s = %s # %s' % (chunks[1], chunks[2], comment)
    if not line:
        lastblank = True
    else:
        lastblank = False
