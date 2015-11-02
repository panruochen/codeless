#!/usr/bin/python
import os, sys, shutil
import subprocess

def try_git_restore() :
    global baksuffix
    hfiles = ''
    bfiles = []

    sp = subprocess.Popen('git diff --name-only', shell=True, stdout=subprocess.PIPE)
    outs,errs = sp.communicate()
    if sp.returncode :
        print >>sys.stderr, "Got return code" % sp.returncode
        return sp.returncode
    for line in outs.split('\n') :
        file = line.rstrip('\r\n')
        bakfile = file + baksuffix
        if os.path.exists(bakfile) :
            hfiles += ' ' + file
            bfiles.append(bakfile)
    if len(bfiles) :
        sp = subprocess.Popen('git checkout ' + hfiles, shell=True)
        sp.wait()
        if sp.returncode :
            return sp.returncode
        for file in bfiles :
            print "Removing " + file
            os.remove(file)
    return 0

def try_restore() :
    global baksuffix
    bak_list = []

    for root, dirs, files in os.walk('.'):
        for name in files :
            file = os.path.join(root, name)
            if file.endswith(baksuffix) :
                bak_list.append(file)

    for bakfile in bak_list :
        basefile = bakfile[0:len(bakfile)-len(baksuffix)]
        shutil.move(bakfile, basefile)

baksuffix = '.hbak'

try_git_restore()
exit(0)
