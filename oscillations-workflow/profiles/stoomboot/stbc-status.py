#!/usr/bin/env python

import sys
import htcondor
from htcondor import JobEventType
import os
from os.path import join


def print_and_exit(s):
    print(s)
    exit()

argv_split = sys.argv[1].split("_")
rulename = "_".join(argv_split[:-3])
jobID, timestamp, ClusterID = argv_split[-3:]

jobDir = join(os.getcwd(),"logs","jobs","{}_{}_{}".format(rulename, jobID, timestamp))
jobLog = join(jobDir, "condor.log")

failed_states = [
    JobEventType.JOB_HELD,
    JobEventType.JOB_ABORTED,
    JobEventType.EXECUTABLE_ERROR,
]

# We check condor's event file for the job, to know its status
# without having to constantly query the scheduler
try:
    jel = htcondor.JobEventLog(join(jobLog))
    for event in jel.events(stop_after=5):
        if event.type in failed_states:
            print_and_exit("failed")
        if event.type is JobEventType.JOB_TERMINATED:
            if event["ReturnValue"] == 0:
                print_and_exit("success")
            print_and_exit("failed")
except OSError as e:
    print_and_exit("failed: {}".format(e))

print_and_exit("running")

