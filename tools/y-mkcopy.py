#!/usr/bin/python

import sys, os, glob, stat
import ConfigParser, optparse

# create the options parser
optionsparser = optparse.OptionParser()
# define the options we require/support
optionsparser.add_option("-p", "--prefix", action='store', help="Specify the common path prefix", default=[])
optionsparser.add_option("-t", "--target", action='store', help="Specify the target directory", default='')

# parse the options
(options, args) = optionsparser.parse_args()

drop_list = []

if len(args) == 0 :
    fd = sys.stdin
else :
    fd = open(args[0], "r")

print "cd ", options.prefix
if options.prefix[-1] != '/' :
    options.prefix += '/'
for line in fd :
    line = line.rstrip('\r\n')
    if line.find(options.prefix) == 0 :
        print "cp -f --parent %s %s" % (line[len(options.prefix):], options.target)
    else :
        drop_list.append(line)

if len(drop_list) > 0 :
    print >>sys.stderr, "\nFollowing files are not processed:"
for file in drop_list :
    print >>sys.stderr, file
