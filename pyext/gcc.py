#!/usr/bin/env python3
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
		print("Abort on %d" % sp.returncode, file=sys.stderr)
		assert(0)
	return out.decode("utf-8")

def get_predefines() :
        short_options = "m:f:O:"
        long_options  = []
        o, a  = yzu.collect_options(short_options, long_options, sys.argv[1:])
        cmd = os.environ["YZ_CC_PATH"] + '\t' + ' '.join(o) + ' -E -x' + ext_get(a) + ' -dM /dev/null'
        results = cmd_exec(cmd)
        print(results, end=None)

def get_search_dirs() :
	flag = False
	short_options = "m:f:O:"
	long_options  = []
	o, a  = yzu.collect_options(short_options, long_options, sys.argv[1:])
	cmd = os.environ["YZ_CC_PATH"] + ' -E -x' + ext_get(a) + ' -v /dev/null 2>&1'
	results = cmd_exec(cmd)
	for line in results.split('\n') :
		if line == '#include <...> search starts here:' :
			flag = True
		elif line == 'End of search list.' :
			break
		elif flag :
			fx = line.split()
			if len(fx) > 0 :
				print(fx[0])
