#!/usr/bin/python

import sys, os, re
import ConfigParser, optparse
from collections import deque

class CCond :
    def __init__(self, type, value, begin, end) :
        self.type  = type
        self.value = value
        self.begin = begin
        self.end   = end

class Interval :
    def __init__(self, begin, end, type) :
        self.begin = begin
        self.end   = end
        self.type  = type

def iv_compare(x, y) :
    if x.end < y.begin :
        return -1;
    elif y.end < x.begin :
        return 1;
    return 0

def check_against(key, array) :
    a = 0
    b = len(array) - 1
    while a <= b :
        m = int((a + b) / 2)
        iv = array[m]
        if key < iv.begin :
            b = m - 1
        elif key > iv.end :
            a = m + 1
        else :
            return iv
    return None

def handle_file(file, ignores, fdump) :
    global patterns
    ignores.sort(iv_compare)
    if fdump is not None :
        print >>fdump, "Process file %s" % file
        for iv in ignores :
            print >>fdump, "  [%u, %u]" % (iv.begin, iv.end)
    lnr = 0
    fdr = open(file,  "r")
    fdw = open(file+'.NEW', "w")
    for line in fdr :
        lnr += 1
        iv = check_against(lnr, ignores)
        if not iv :
            fdw.write(line)
        elif lnr == iv.begin and not patterns[iv.type].match(line) :
            print >>sys.stderr, "On %s:%u" % (file, lnr)
            print >>sys.stderr, "%s" % line.rstrip('\r\n')
            print >>sys.stderr, " failed to match pattern \'%s\'" % iv.type
            exit(1)
    fdr.close()
    fdw.close()

def proc_if(ignores, q) :
    has_else = False
    has_elif = False
    while True :
        c = q.pop()
        if c.type == 'else' :
            has_else = True
        if c.type == 'elif' :
            has_elif = True
        if c.value == 1 :
            iv = Interval(c.begin, c.begin, c.type)
            ignores.append(iv)
            if (c.type == 'if' and (not has_elif) and (not has_else)) or (c.type == 'elif' and not has_else) :
                iv = Interval(c.end, c.end, "endif")
                ignores.append(iv)
        elif c.value == 0 :
            iv = Interval(c.begin, c.end, c.type)
            ignores.append(iv)
        if c.type == 'if' :
            break

def clean_header_files(cerfile, fdump) :
    fd = open(cerfile, "r")
    if not fd :
        print >>sys.stderr, "Cannot open %s to analyze" % cerfile

    hfile = None
    ignores = []
    levels = dict()
    for line in fd :
        if len(line) == 0 :
            continue
        line = line.rstrip('\r\n')
        if line[0] in (' ', '\t') :
            fx = line.split()
            if len(fx) < 5 :
                continue
            nh = int(fx[0])
            if nh not in levels :
                levels[nh] = deque([])

            q = levels[nh]
            c = CCond(fx[1], int(fx[2]), int(fx[3]), int(fx[4]))
            t = fx[1]
            if t == "if" :
                if len(q) > 0 and q[-1].type == 'if' :
                    proc_if(ignores, q)
                q.append(c)
            elif t in ("elif", 'else'):
                prev = q[-1]
                if prev.type != 'if' and prev.type != 'elif' :
                    print >> sys.stderr, "Unmatched %s on \"%s\"" % (t, hfile)
                    print >> sys.stderr, " %s" % line
                    exit(1)
                q.append(c)
                if t == 'else' :
                    proc_if(ignores, q)
        else :
            if len(ignores) > 0:
                handle_file(hfile, ignores, fdump)
            elif hfile is not None and fdump is not None:
                print >>fdump, "Nothing to do for \"%s\"" % hfile
            hfile = line
            del ignores[:]
            levels.clear()

    if len(ignores) > 0:
        handle_file(hfile, ignores, fdump)
        del ignores

optionsparser = optparse.OptionParser()

optionsparser.add_option("-c", "--cer-file", action='store', help="Specify the conditional evaluation result file", dest='cerfile', default=None)
optionsparser.add_option("-d", "", action='store', help="Specify the file to dump", dest='dumpfile', default=None)

(options, args) = optionsparser.parse_args()

if options.cerfile is None :
    print >>sys.stderr, "Usage: %s CONDITIONAL_RESULT_FILE" % os.path.basename(sys.argv[0])
    exit(1)

fdump = None
if options.dumpfile is None :
    fdump = open(options.dumpfile, "w")

patterns = {}
for i in ('if', 'else', 'elif', 'endif') :
    patterns[i] = re.compile('#\s*' + i)

clean_header_files(options.cerfile, fdump)
if fdump is not None :
    fdump.close()
