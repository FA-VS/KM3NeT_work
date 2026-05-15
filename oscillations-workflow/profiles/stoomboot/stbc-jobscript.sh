#!/bin/bash
# properties = {properties}
# 
# condor-jobscript.sh
#
# Wrapper for snakemake jobs

set -e

echo "hostname:"
hostname -f
echo "pwd:"
pwd

{exec_job}
