#!/usr/bin/python

import sys, os, glob, stat
import shutil
import ConfigParser, optparse
import subprocess

def find_dependencies(sources, depfile) :
    fd = open(depfile, "r")
    if not fd :
        print >>sys.stderr, "Cannot open %s to analyze" % depfile

    deps = dict()
    curfile = None
    for line in fd :
        if len(line) == 0 :
            continue
        line = line.rstrip('\r\n')

        if line[0] == ' ' :
            if curfile is not None :
                a = line.split()
                deps[curfile].append(a[0])
        else :
            curfile = None
            for x in sources :
                if line.endswith(x) :
                    curfile = line
                    deps[curfile] = []
    return deps

# create the options parser
optionsparser = optparse.OptionParser()

# define the options we require/support
optionsparser.add_option("-s", "--source", action='append', help="Specify the source file name to search", default=[])
optionsparser.add_option("-S", "--from-file", action='store', help="Get source file name from file", dest='from_file', default=None)
optionsparser.add_option("-f", "--format", action='store', help="Specify the output format", default='all')

# parse the options
(options, args) = optionsparser.parse_args()

if not options.format in { 'all' : 1, 'only-dep' : 2} :
    print >>sys.stderr, "supported format could be: all, only-dep"
    exit(1)

sources = options.source
#exit(0)

if options.from_file is not None :
    fd = open(options.from_file, "r")
    if fd is not None :
        for line in fd :
            sources.append(line.rstrip('\r\n'))

set1 = set()
for file in args :
    deps = find_dependencies(sources, file)

    if options.format == 'all' :
        for s in deps :
            print s
            for d in deps[s] :
                print '  ', d
    elif options.format == 'only-dep' :
        for s in deps :
            for d in deps[s] :
                set1.add(d)

if options.format == 'only-dep' :
    for d in set1 :
        print d

