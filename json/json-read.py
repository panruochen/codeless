#!/usr/bin/python
import sys, os, glob, stat
import json, collections

json_data = open(sys.argv[1]).read()

inventory = json.loads(json_data, object_pairs_hook=collections.OrderedDict)

for k in inventory.iteritems() :
    print k

