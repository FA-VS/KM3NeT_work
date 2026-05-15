import os
import pandas as pd
import numpy as np
from snakemake.logging import logger
from snakemake.jobs import Job, GroupJob

def onerror_wrapper(workflow, config):
    logger.info("-"*20 + " On error actions  " + "-"*20)
    logger.error("Execution ended with error, printing some custom details ...")
    onerror_report(workflow, max_printed_details = -1)
    logger.info("-"*20 + " Completed  " + "-"*20)


def jobs2df(jobs, tag = "#", return_tag_only = False):
    """Convert jobs iterable in a pandas dataframe"""
    
    df = []
    for j in jobs:
        if isinstance(j, Job):
            df.append({
                **{
                    "rule":j.name,
                    "logs":j.log,
                    "jobobj":j,
                    "job id":j.jobid,
                    "inputs":j.input,
                    "outputs":j.output
                },
                **j.wildcards})
        elif isinstance(j, GroupJob):
            df.append({
                **{
                    "rule":j.name,
                    "logs":j.log,
                    "jobobj":j,
                    #"job id":j.jobid,
                    "group id": j.jobid,
                    "inputs":j.input,
                    "outputs":j.output
                }})
        else :
            raise Exception("Unknown job format: {type(j)}")
        
    if len(df) == 0:
        return None
    df = pd.DataFrame(df).set_index("jobobj")
    df[tag] = 1
    if return_tag_only:
        df = df[[tag]]
    return df

def update_flag_failed_group_jobs(jobs, df, tag = "Failed group"):
    """Update failed tag of rules if part of a failed group job"""
    for i, gj in enumerate(jobs):
        if isinstance(gj, GroupJob):
            for j in gj.jobs:
                df.at[j, tag] = True

def sort_subjobs_outputs(groupjob):
    """ Sort information about subjobs for grouped jobs """
    df = jobs2df(groupjob.jobs)
    full_products = []
    last_time_updated = []
    wildcards = []
    for ind, row in df.iterrows():
        wildcards += list(ind.wildcards.keys())
        latest = [os.path.getmtime(f) for f in ind.products() if os.path.exists(f)]
        if len(latest) < len(ind.products()):
            full_products.append(False)
        else :
            full_products.append(True)
        if len(latest) == 0:
            last_time_updated.append(np.nan)
        else:
            last_time_updated.append(np.max(latest))
    df['Products available'] = full_products
    df['Latest output'] = pd.to_datetime(last_time_updated, unit = "s")
    wildcards = set(wildcards)
    wildcards.remove("detid")
    wildcards.remove("run")
    df = df.sort_values(["detid","run","rule","Latest output"])
    df = df.set_index(["detid","run","rule"])
    return df

def onerror_report(workflow, max_printed_details = 10):
    """
    Print a report about failure.
    """
    tags = ["Job in DAG", "Planned", "Waiting", "Failed", "Failed group"]

    # Collect information about jobs in DAG, torun, failed etc ...
    df = pd.concat((
        jobs2df(workflow.persistence.dag.jobs, "Job in DAG"),
        jobs2df(workflow.persistence.dag.needrun_jobs(exclude_finished=False), "Planned", True),
        jobs2df(workflow.persistence.dag.needrun_jobs(), "Waiting", True),
        jobs2df(workflow.scheduler.failed, "Failed", True)
    ), axis=1)    

    # Check which rule was part of a failing group
    update_flag_failed_group_jobs(workflow.scheduler.failed, df)
    
    
    # Somehow group job are missing in needrun_jobs
    if 'group id' in df.columns:
        group_jobs = df.dropna(subset=['group id']).index
        df.loc[group_jobs, 'Planned'] = 1
    
    # Make a boolean matrix for the tags
    for tag in tags:
        if tag not in df.columns:
            df[tag] = False
        else:
            df[tag] = df[tag].replace(np.nan, 0).astype(bool)
            
    df['Waiting'] = df["Waiting"].values & (df["Failed"] == False) & (df["Failed group"] == False)
    
    # Create rule-wise and run-wise summary table
    df_rules = df.groupby("rule").sum(numeric_only=True)[tags]
    df_rules['Failure [%]'] = 100. * df_rules['Failed'] / (df_rules['Planned'] - df_rules['Waiting'])
    
    df_run = df.groupby(["detid","run"]).sum(numeric_only=True)[tags]

    # Print details about the failing rules
    n_failed_rules = df["Failed"].sum()
    if max_printed_details < 0:
        max_printed_details = n_failed_rules
    if n_failed_rules > max_printed_details and n_failed_rules > 0:
        n_failed_rules = max_printed_details

    logger.info(f"Print short details for {n_failed_rules} failed rules ...")
    list_print = ['logs','inputs','outputs']
    for i in range(n_failed_rules):
        row = df[df["Failed"]].iloc[i].drop(index = tags)
        job = row.name
        logger.info("\n"*2+"-"*10 + f" Error in {row.name} " + "-"*10)
        logger.info(row.dropna().drop(index=list_print).to_string())
        # Handle grouped jobs details
        if isinstance(job, GroupJob):
            df_group = sort_subjobs_outputs(job)
            with pd.option_context('display.max_rows', None):
                logger.info(df_group.drop(columns=['inputs','outputs','#']))
                
        # Handle single task details
        else:
            for l in list_print:
                if len(l) > 0:
                    logger.info(f"{l} :")
                    for el in row[l]:
                        logger.info(f"\t - {el}")
                        
    # Print a generic summary about failure rate etc ...
    logger.info("\n"+"-"*30)
    logger.info("Rules-wise summary")
    logger.info(df_rules.replace(np.nan,"-"))
    logger.info("\n"+"-"*30)
    logger.info("Run-wise failure summary")
    logger.info(df_run[df_run['Failed'].astype(bool)])
