#!/usr/bin/env python3
import os, sys, subprocess
import yzu

def cmd_exec(command):
	sp = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE)
	out, err = sp.communicate()
	if sp.returncode != 0 :
		print("Abort on %d" % sp.returncode, file=sys.stderr)
		assert(0)
	return out.decode("utf-8")

def get_predefines() :
	short_options = "O:"
	long_options  = ["cpu="]
#	print(' '.join(sys.argv[1:]), file=sys.stderr)
	o, a  = yzu.collect_options(short_options, long_options, sys.argv[1:])
	cmd = os.environ["YZ_CC_PATH"] + '\t' + ' '.join(o) + ' --list_macros /dev/null'
#	print(cmd, file=sys.stderr)
	results = cmd_exec(cmd)
	print(results, end=None)

def get_search_dirs() :
	if os.environ['ARMCC41INC'] :
		print(os.environ['ARMCC41INC'])

