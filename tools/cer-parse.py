#!/usr/bin/python
#
#  Reparse the conditional evalution results and keep each file unique.
#

import sys, os
from collections import OrderedDict
import optparse

TS_0 = 0
TS_1 = 1
TS_X = -1

def str2bool(v):
    return v.lower() in ("yes", "true", "t", "1")

def hash_key(begin, end) :
    return (end * 0x100000000) + begin;

class CConditional :
    def __init__(self) :
        self.type  = None
        self.begin = -1
        self.end   = -1
        self.value = TS_X
        self.level = -1

class CHFile :
    def __init__(self) :
        self.conds = OrderedDict()
        self.filename = None

    def proc_filename(self, line) :
        self.filename = line

    def proc_conditional(self, line) :
        fx = line.split()
        new_cc = CConditional()
        new_cc.level = int(fx[0])
        new_cc.type  = fx[1]
        new_cc.value = str2bool(fx[2])
        new_cc.begin = int(fx[3])
        new_cc.end   = int(fx[4])
        hkey = hash_key(new_cc.begin, new_cc.end)
        if hkey in self.conds :
            old_cc = self.conds[hkey]
            if old_cc.level != new_cc.level or old_cc.begin != new_cc.begin or old_cc.end != new_cc.end or old_cc.type != new_cc.type :
                print >>sys.stderr, "Mismatch on \"%s\"" % self.filename
                print >>sys.stderr, "  [%2u] %s %u %u %u" % (old_cc.level, old_cc.type, old_cc.value, old_cc.begin, old_cc.end)
                print >>sys.stderr, "  [%2u] %s %u %u %u" % (new_cc.level, new_cc.type, new_cc.value, new_cc.begin, new_cc.end)
                exit(1)
            if old_cc.value != new_cc.value :
                self.conds[hkey].value = TS_X
            del new_cc
        else :
            self.conds[hkey] = new_cc

optionsparser = optparse.OptionParser()

optionsparser.add_option("-o", "--output", action='store', help="specify the output file", default=None)

(options, args) = optionsparser.parse_args()

fd = open(args[0], "r")
if fd is None:
    print >>sys.sterr, "Cannot open %s" % args[0]
    exit(1)

file = None
hfiles = OrderedDict()
for line in fd :
    line = line.rstrip('\r\n')
    if line[0] not in (' ', '\t') :
        if line in hfiles :
            file = hfiles[line]
        else :
            file = CHFile()
            hfiles[line] = file
        file.proc_filename(line)
    else :
        file.proc_conditional(line)

if options.output is not None :
    ofile = open(options.output, "w")
else :
    ofile = sys.stdout

for name in hfiles :
    print >> ofile, name
    for cc in hfiles[name].conds.values() :
        print >> ofile, "  %2u %-4s %2d %7u %7u" % (cc.level, cc.type, cc.value, cc.begin, cc.end)

if options.output is not None :
    ofile.close()

