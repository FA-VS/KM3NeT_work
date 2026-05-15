import os

from snakemake.logging import logger
from snakemake.report import auto_report

def onsuccess_wrapper(workflow, config):
    logger.info("-"*20 + " On success checks " + "-"*20)
    
    logger.info("-"*20 + "     Completed     " + "-"*20)
