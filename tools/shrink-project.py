#!/usr/bin/python

import sys, os, glob, stat
import shutil
import ConfigParser, optparse
import subprocess

class Interval :
    def __init__(self, begin, end) :
        self.begin = begin
        self.end   = end

def iv_compare(x, y) :
    if x.end <= y.begin :
        return -1;
    elif y.end <= x.begin :
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
        elif key >= iv.end :
            a = m + 1
        else :
            return True
    return False

def handle_file(file, ignores) :
    lnr = 0
    fdr = open(file,  "r")
    fdw = open(file+'.NEW', "w")
    ignores.sort(iv_compare)
    for line in fdr :
        lnr += 1
        if not check_against(lnr, ignores) :
            fdw.write(line)
    fdr.close()
    fdw.close()


def clean_header_files(cerfile) :
    fd = open(cerfile, "r")
    if not fd :
        print >>sys.stderr, "Cannot open %s to analyze" % cerfile

    hfile = None
    ignores = []
    for line in fd :
        if len(line) == 0 :
            continue
        line = line.rstrip('\r\n')

        if line[0] in (' ', '\t') :
            fx = line.split()
            if len(fx) < 4 :
                continue
            val = int(fx[1])
            if val == 0 :
                iv = Interval(int(fx[2]), int(fx[3]))
                ignores.append(iv)
            elif val == 1 :
                val = int(fx[2])
                iv = Interval(val, val+1)
                ignores.append(iv)
                val = int(fx[3]) - 1
                iv = Interval(val, val+1)
                ignores.append(iv)
        else :
            if len(ignores) > 0:
                handle_file(hfile, ignores)
                del ignores
            hfile = line
            ignores = []

    if len(ignores) > 0:
        handle_file(hfile, ignores)
        del ignores

optionsparser = optparse.OptionParser()

optionsparser.add_option("-c", "--cer-file", action='store', help="Specify the conditional evaluation result file", dest='cerfile', default=None)
optionsparser.add_option("-d", "--dep-file", action='store', help="Specify the dependency file", dest='depfile', default=None)

(options, args) = optionsparser.parse_args()

clean_header_files(options.cerfile)
