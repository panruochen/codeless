#!/usr/bin/env python
import os, sys, subprocess
import yzu

def ext_get(args) :
    for x in args.split() :
        z = x[::-1]
        if z.find('ppc.') == 0 or z.find('cc.') == 0 or z.find('xxc.') == 0 :
            return 'c++'
    return 'c'

def cmd_exec(command):
    sp = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE)
    out, err = sp.communicate()
    if sp.returncode != 0 :
        print >>sys.stderr, "Abort on %d" % sp.returncode
        assert(0)
    return out

def get_predefines() :
    short_options = "m:f:O:"
    long_options  = []
    o, a  = yzu.collect_options(short_options, long_options, sys.argv[1:])
    commands  = [os.environ["YZ_CC_PATH"]]
    commands += o
    commands += ('-E', '-x', ext_get(a), '-dM', '/dev/null')

    sp = subprocess.Popen(commands, stdout=subprocess.PIPE)
    results, err = sp.communicate()
    if sp.returncode != 0 :
        print >>sys.stderr, "Got %d on executing:" % sp.returncode
        print >>sys.stderr, "  %s" % ' '.join(commands)
        exit(1)

    sys.stdout.write(results)

def get_search_dirs() :
    flag = False
    short_options = "m:f:O:"
    long_options  = []
    o, a  = yzu.collect_options(short_options, long_options, sys.argv[1:])
    commands = [os.environ["YZ_CC_PATH"], '-E', '-x', ext_get(a), '-v', '/dev/null']

    sp = subprocess.Popen(commands, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    null, results = sp.communicate()
    if sp.returncode != 0 :
        print >>sys.stderr, "Got %d on executing:" % sp.returncode
        print >>sys.stderr, "  %s" % ' '.join(commands)
        exit(1)

    for line in results.split('\n') :
        if line == '#include <...> search starts here:' :
            flag = True
        elif line == 'End of search list.' :
            break
        elif flag :
            fx = line.split()
            if len(fx) > 0 :
                print fx[0]
