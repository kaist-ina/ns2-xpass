#!/usr/bin/env python
import os
import sys
from random import randint

if len(sys.argv) != 2:
  print("Usage: ./run_dumbell.py {experiment_name}")
  exit(1)

exp_id = randint(1, 10000)
cmd = "./ns scripts/dumbell-topology.tcl %d" % exp_id
print(cmd)
ret = os.system(cmd)

if ret != 0:
  exit(1)

cmd = "mv outputs/fct_%d.out outputs/fct_%s.csv" % (exp_id, str(sys.argv[1]))
print(cmd)
os.system(cmd)

cmd = "mv outputs/trace_%d.out outputs/trace_%s.out" % (exp_id, str(sys.argv[1]))
print(cmd)
os.system(cmd)

print("Finished")
