#! /usr/bin/env python

"""Convert the plain text FAQ file to its .ht template.
"""

import sys
import os
import re



def main():
    faqfile = sys.argv[1]
    fp = open(faqfile)
    lines = fp.readlines()
    fp.close()

    outfile = sys.argv[2]
    if outfile == '-':
        closep = 0
        out = sys.stdout
    else:
        closep = 1
        out = open(outfile, 'w')

    # skip over cruft in FAQ file
    lineno = 0
    while not lines[lineno].startswith('FREQUENTLY'):
        lineno += 1
    lineno += 1

    # skip blanks
    while not lines[lineno].strip():
        lineno += 1

    # first print out standard .ht boilerplate
    print >> out, '''\
Title: Mailman Frequently Asked Questions

See also the <a href="http://www.python.org/cgi-bin/faqw-mm.py">Mailman
FAQ Wizard</a> for more information.

 <h3>Mailman Frequently Asked Questions</h3>
'''
    first = 1
    question = []
    answer = []
    faq = []
    while 1:
        line = lines[lineno][:-1]

        if line.startswith('Q.'):
            inquestion = 1
            if not first:
                faq.append((question, answer))
                question = []
                answer = []
            else:
                first = 0
        elif line.startswith('A.'):
            inquestion = 0
        elif line.startswith('\f'):
            break

        if inquestion:
            question.append(line)
        else:
            # watch for lists
            if line.lstrip().startswith('*'):
                answer.append('<li>')
                line = line.replace('*', '', 1)
            # substitute <...>
            line = re.sub(r'<(?P<var>[^>]+)>',
                          '<em>\g<var></em>',
                          line)
            # make links active
            line = re.sub(r'(?P<url>http://\S+)',
                          '<a href="\g<url>">\g<url></a>',
                          line)
            answer.append(line)

        lineno += 1
    faq.append((question, answer))

    for question, answer in faq:
        print >> out, '<b>',
        for line in question:
            print >> out, line
        print >> out, '</b><br>',
        for line in answer:
            if not line:
                print >> out, '<p>',
            else:
                print >> out, line

    if closep:
        out.close()



if __name__ == '__main__':
    main()
