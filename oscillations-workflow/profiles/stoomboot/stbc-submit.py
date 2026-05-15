#!/usr/bin/env python3

import sys
import htcondor
import os
from os.path import join
from datetime import datetime

from snakemake.utils import read_job_properties


jobscript = sys.argv[1]
job_properties = read_job_properties(jobscript)

###############
# KM3NeT specific part
rulename = "{name}"
if job_properties['type'] == 'single': rulename = job_properties['rule']
elif job_properties['type'] == "group": rulename = job_properties['groupid']
else : raise NotImplementedError("Job type {} is not supported".format(job_properties['type']))
###############

################
# Nikhef specific part

# See https://kb.nikhef.nl/ct/Batch_jobs.html?h=batch#submitting-jobs-to-the-batch-system for details on Stoomboot specificities
stbc_queues = { 'express' : 10, 'short' : 240, 'medium' : 1440, 'long' : 5760} # Time in minutes

def parse_timestr_to_seconds(timestr):
    try:
        time_obj = datetime.strptime(timestr, "%H:%M:%S")
        sec = time_obj.hour * 3600 + time_obj.minute * 60 + time_obj.second
    except ValueError:
        time_obj = datetime.strptime(timestr, "%d-%H:%M:%S")
        sec = time_obj.day * 24*3600 + time_obj.hour * 3600 + time_obj.minute * 60 + time_obj.second
    return sec

request_time = job_properties["resources"].get("runtime", None)
request_queue = job_properties["resources"].get("queue", None)

# Double check queue consistent with time requested...
if request_time is not None:
    overwrite_queue = False
    if request_queue is None:
        overwrite_queue = True
    for queue_name in stbc_queues.keys():
        if request_queue == queue_name:
            overwrite_queue = True
        if request_time <= stbc_queues[request_queue]:
            if overwrite_queue:
                request_queue = queue_name
            break

SingularityImage = "/data/km3net/users/fvazquez/images/snakemake.sif" #TODO: Make this more flexible / give url?
###############

timestamp = int(datetime.now().timestamp())
jobDir = join(os.getcwd(),"logs","jobs","{}_{}_{}".format(rulename, job_properties["jobid"], timestamp))
os.makedirs(jobDir, exist_ok=True)

sub = htcondor.Submit(
    {
        "executable": jobscript,
        "max_retries": "5",
        "log": join(jobDir, "condor.log"),
        "output": join(jobDir, "condor.out"),
        "error": join(jobDir, "condor.err"),
        #"getenv": "True", # Leads to image driver mount failure, something to do with squashfuse...?
        "request_cpus": str(job_properties["threads"]),
        r"+JobCategory": f'"{request_queue}"',
        r"+SingularityImage": f'"{SingularityImage}"',
    }
)

request_memory = job_properties["resources"].get("mem_mb", None)
if request_memory is not None:
    sub["request_memory"] = str(request_memory)

request_disk = job_properties["resources"].get("disk_mb", None)
if request_disk is not None:
    sub["request_disk"] = str(request_disk)

c = htcondor.Collector().locate(htcondor.DaemonTypes.Schedd, "taai-007.nikhef.nl")
schedd = htcondor.Schedd(c)
sub_result = schedd.submit(sub)
clusterID = sub_result.cluster()

# print jobid for use in Snakemake
print("{}_{}_{}_{}".format(rulename, job_properties["jobid"], timestamp, clusterID))

