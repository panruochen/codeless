#!/usr/bin/python
#
#  Strip files based on the conditional-value file
#

import sys, os, re, shutil
import ConfigParser, optparse
from collections import deque

class CCond :
    def __init__(self, type, value, begin, boff, end, eoff) :
        self.type  = type
        self.value = value
        self.begin = begin
        self.boff  = boff
        self.end   = end
        self.eoff  = eoff

class CodeBlock :
    def __init__(self, begin, end, type) :
        self.begin = begin
        self.end   = end
        self.type  = type
        self.mode  = 0

def cb_compare(x, y) :
    if x.end < y.begin :
        return -1;
    elif y.end < x.begin :
        return 1;
    return 0

def binsearch(key, array) :
    a = 0
    b = len(array) - 1
    while a <= b :
        m = int((a + b) / 2)
        cb = array[m]
        if key < cb.begin :
            b = m - 1
        elif key > cb.end :
            a = m + 1
        else :
            return cb
    return None

def strip_file(options, file, c_blocks, fdump) :
    for i in options.ignlist :
        if file.find(i) >= 0 :
            return
    c_blocks.sort(cb_compare)
    if fdump is not None :
        print >>fdump, "Process file %s" % file
        for cb in c_blocks :
            print >>fdump, "  %s [%u, %u]" % (cb.type, cb.begin, cb.end)
    lnr = 0
    fdr = open(file,  "r")
    fdw = open(file+'.NEW', "w")
    for line in fdr :
        lnr += 1
        cb = binsearch(lnr, c_blocks)
        if not cb :
            fdw.write(line)
        else :
            if lnr == cb.begin and not patterns[cb.type].search(line) :
                print >>sys.stderr, "On %s:%u" % (file, lnr)
                print >>sys.stderr, "%s" % line.rstrip('\r\n')
                print >>sys.stderr, " failed to match pattern \'%s\'" % cb.type
                exit(1)
            if cb.mode == 1 :
                line2 = sub_patterns[1].sub('#if', line)
                fdw.write(line2)
    fdr.close()
    fdw.close()
    shutil.move(file, file + options.baksuffix)
    shutil.move(file + '.NEW', file)

def proc_if(c_blocks, q) :
    tmplist = deque([])
    has_x = False
    first_elif = None
    no_if = False
    while True :
        c = q.pop()
        tmplist.appendleft(c)
        if c.type == 'if' :
            break
    for c in tmplist :

        if c.type == 'elif' :
            first_elif = c
        if c.value == 1 :
            cb = CodeBlock(c.begin + c.boff, c.begin, c.type)
            c_blocks.append(cb)
        elif c.value == 0 :
            cb = CodeBlock(c.begin + c.boff, c.end, c.type)
            c_blocks.append(cb)
            if c.type == 'if' :
                no_if = True
        elif c.value == -1:
            has_x = True
        else :
            print >>sys.stderr, "Unexpected error %d" % c.value
    if not has_x :
        end = tmplist[-1].end
#        print >>sys.stderr, "keep endif on %u" % end
#        cb = CodeBlock(end, end, "endif")
        cb = CodeBlock(tmplist[-1].end + tmplist[-1].eoff, tmplist[-1].end, "endif")
        c_blocks.append(cb)
    else :
        if no_if :
            cb = CodeBlock(first_elif.begin + first_elif.boff, first_elif.begin, first_elif.type)
            cb.mode = 1 ## Transform: #elif xx --> #if xx
            c_blocks.append(cb)
        if tmplist[-1].value == 0 :
            c_blocks[-1].end -= 1 ## keep the #endif

def tr(arg) :
    if arg.find(',') >= 0 :
        fx = arg.split(',')
        a1 = int(fx[0])
        a2 = int(fx[1])
    else :
        a1 = int(arg)
        a2 = 0
    return a1,a2

def run(options, us_files, fdump, processed_files) :
    fd = open(options.cvfile, "r")
    if not fd :
        print >>sys.stderr, "Cannot open %s to analyze" % options.cvfile
        exit(1)

    c_blocks = []
    hfile = None
    levels = dict()
    last_nh = 1

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

            if nh < last_nh :
                q = levels[last_nh]
                while len(q) > 0 :
                    proc_if(c_blocks, q)

            q  = levels[nh]

            a1,a2 = tr(fx[3])
            b1,b2 = tr(fx[4])
            c = CCond(fx[1], int(fx[2]), a1, a2, b1, b2)
            t = fx[1]
            if t == "if" :
                if len(q) > 0 and q[-1].type == 'if' :
                    proc_if(c_blocks, q)
                q.append(c)
            elif t in ("elif", 'else'):
                prev = q[-1]
                if prev.type != 'if' and prev.type != 'elif' :
                    print >> sys.stderr, "Unmatched %s on \"%s\"" % (t, hfile)
                    print >> sys.stderr, " %s" % line
                    exit(1)
                q.append(c)
                if t == 'else' :
                    proc_if(c_blocks, q)
            last_nh = nh
        else :
            if last_nh == 2 :
                q = levels[2]
                while len(q) > 0 :
                    proc_if(c_blocks, q)
            if 1 in levels :
                q = levels[1]
                while len(q) > 0 :
                    proc_if(c_blocks, q)
            if (options.all or hfile in us_files) and len(c_blocks) > 0 :
                strip_file(options, hfile, c_blocks, fdump)
                processed_files.append(hfile)
            elif hfile is not None and fdump is not None:
                print >>fdump, "Nothing to do for \"%s\"" % hfile
            hfile = line
            del c_blocks[:]
            levels.clear()
            last_nh = 1

    if (options.all or hfile in us_files) and len(c_blocks) > 0 :
        strip_file(options, hfile, c_blocks, fdump)
        processed_files.append(hfile)
        del c_blocks

def main(startidx) :
    oparser = optparse.OptionParser()

    oparser.add_option("-b", "--bak-suffix", action='store', help="The suffix name of backup files", dest='baksuffix', default='.bak')
    oparser.add_option("-f", "--condvals-file", action='store', help="The conditional-values(CV) file name", dest='cvfile', default=None)
    oparser.add_option("-d", "", action='store', help="Specify the file to dump", dest='dumpfile', default=None)
    oparser.add_option("-i", "--yz-ignore", action='append', help="Specify the file to be ignored", dest='ignlist', default=[])
    oparser.add_option("-a", "--all", action='store_true', help="Strip all files listed in the CV file", dest='all')
    oparser.add_option("-v", "", action='store_true', help="Verbose mode", dest='verbose')

    (options, files) = oparser.parse_args(sys.argv[startidx:])

    if options.cvfile is None :
        oparser.print_help()
        exit(1)

    fdump = None
    if options.dumpfile is not None :
        fdump = open(options.dumpfile, "w")


    us_files = set()
    for i in files :
        us_files.add(os.path.realpath(i))

    processed_files = []
    run(options, us_files, fdump, processed_files)

    if fdump is not None :
        fdump.close()

    if options.verbose :
        for file in processed_files :
            print >>sys.stderr, "%s stripped" % file

patterns = {}
for i in ('if', 'else', 'elif', 'endif') :
    patterns[i] = re.compile('#\s*' + i)
sub_patterns = [ None, re.compile('#\s*elif') ]
if __name__ == '__main__' :
    main(2)
