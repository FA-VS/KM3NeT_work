import os
import shutil
import pandas as pd

from snakemake.logging import logger
from snakemake.io import expand

def set_singularity_args(workflow, config, extended_config):
    """    
    Set singularity arguments for the workflow
    - Not very nice, bypass property interface for workflow properties,
      but no work around to set dynamic default parameters with 
      snakemake>=7.30.2 and python>=3.11
    """
    #  - Force the use of singularity
    workflow._use_singularity = True
    #  - Using singularity, does not export host environment
    workflow._singularity_args = "--cleanenv"
    #  - Export proxy variables if sets:
    if "http_proxy" in os.environ:
        workflow._singularity_args += " --env http_proxy=" + os.environ['http_proxy']
    if "https_proxy" in os.environ:
        workflow._singularity_args += " --env https_proxy=" + os.environ['https_proxy']

    # Append the paths for input files and selector files
    # (this would be much better to be done per-rule, but unfortunately that is not an option with snakemake right now...)
    # Note that this only expands detid and version, NOT run!!
    mountpoints_list = []
    if os.path.isabs(config['ResponseFunction']['json_preconfig']):
        path = os.path.dirname(config['ResponseFunction']['json_preconfig'])
        mountpoints_list.append(path)
        workflow._singularity_args += f" -B {path}"

    subconfig_path_list = [ config["Selector"][pathtype_key] for pathtype_key in ["dst_common_path", "scores_common_path", "output_common_path"] ]
    if 'selector_scratch_path' in config['ResponseFunction'].keys():
        subconfig_path_list.append( config['ResponseFunction']['selector_scratch_path'])

    for subconfig_path in subconfig_path_list:
        if isinstance(subconfig_path, dict):
            for wc_path in subconfig_path.values():
                if os.path.isabs(wc_path):
                    # TODO: could we also replace {run} wildcards if present? Bit tricky...
                    path_list = expand(wc_path, version = config['version'], detid = extended_config['detids_runs_to_do'].keys() )
                    for path in path_list:
                        if path not in mountpoints_list:
                            mountpoints_list.append(path)
                            workflow._singularity_args += f" -B {path}"
        elif isinstance(subconfig_path, str):
            wc_path = subconfig_path
            if os.path.isabs(wc_path):
                path_list = expand(wc_path, version = config['version'], detid = extended_config['detids_runs_to_do'].keys() )
                for path in path_list:
                    if path not in mountpoints_list:
                        mountpoints_list.append(path)
                        workflow._singularity_args += f" -B {path}"

    # For some reason, if only one arg is provided we need quotes
    if " " not in workflow.singularity_args:
        workflow._singularity_args = "\"" + workflow.singularity_args + "\""

def parse_run_list(filepath):
    """ Return dict(detid:runs) object from file """
    runs_df = pd.read_csv(filepath,
                      sep = " ",
                      names = ["detid","run"])
    detids_runs_to_do = {}

    for detid in runs_df['detid'].unique():
        runs = runs_df.set_index("detid").loc[[detid]]['run'].values
        runs = ["{:08d}".format(r) for r in runs]
        detids_runs_to_do["{:08d}".format(detid)] = runs
    return detids_runs_to_do


def parse_detidgroup_list(config, detids_runs_to_do):
    """
    Return dict(detidgroup:detids) object from the config,
    and a 'detids_runs_to_do' dictionary
    """
    detidgroups_detids_to_do = {}
    if 'DetIDgroups' in config.keys():
        for detid in detids_runs_to_do.keys():
            for detidgroup, detid_list in config['DetIDgroups'].items():
                if detid in detid_list:
                    if detidgroup not in detidgroups_detids_to_do.keys():
                        # If not already present, add detidgroup to list of groups to do
                        detidgroups_detids_to_do[detidgroup] = []
                    detidgroups_detids_to_do[detidgroup].append(detid)
                    break
            else: #i.e. if nobreak
                # DetID missing from DetID group definitions,
                # Turn it into a singleton "group"
                # (this way workflow can be used without groups)
                detidgroups_detids_to_do[detid] = [detid]
        logger.info("DetIDs grouped as follows:")
        for detidgroup, detid_list in detidgroups_detids_to_do.items():
            logger.info(f"{detidgroup}: {detid_list}")
    else:
        # (idem: this way workflow can be used without groups)
        for detid in detids_runs_to_do.keys():
                detidgroups_detids_to_do[detid] = [detid]

    return detidgroups_detids_to_do


def parse_cutvar_values(config):
    """
        This function creates a dictionary with one key per cut
        variable to be optimized (as defined in the Optimization
        section of the config yaml). The dictionary values are
        lists with the cut variable values to be tested. These are
        then used to replace the wildcards in the cuts in the class
        definitions (for the purpose of optimizing those definitions).
    """

    if 'Optimization' in config and 'cuts' in config['Optimization']:
        import numpy as np
        optimization_dict = config['Optimization']['cuts']
        cutvar_values_dict = {}

        for cutvar_name, cutvar_config in optimization_dict.items():
            if 'custom' in cutvar_config:
                cutvar_values_dict[cutvar_name]  = cutvar_config['custom']
            else:
                if 'log' in cutvar_config and cutvar_config['log']:
                    values_array = np.logspace(
                        np.log10(np.float64(cutvar_config["start"])),
                        np.log10(np.float64(cutvar_config["end"])),
                        int(cutvar_config["nsamples"]),
                    )
                else:
                    values_array = np.linspace(
                        np.float64(cutvar_config["start"]),
                        np.float64(cutvar_config["end"]),
                        int(cutvar_config["nsamples"]),
                    )

                cutvar_values_dict[cutvar_name] = [
                        np.format_float_scientific(val, precision=2, exp_digits=2)
                        for val in values_array
                        ]

        return cutvar_values_dict
    else:
        return {}

def make_extended_config(workflow, config):
    """ Create extended config object """

    # This function has to be independent of the workflow status,
    # otherwise input functions in rules are non-deterministic.

    ext_config = {}

    # Check the processing version
    logger.info("Configuration file version is {}".format(config["version"]))

    # Set the detid:runs to process
    if "runs_list" in config:
        logger.info("Runs list provided: " + config["runs_list"])
        ext_config['detids_runs_to_do'] = parse_run_list(config["runs_list"])
    else:
        logger.warning("No runs list provided. Generic rule like process_all wont yield any result, only explicit target will")
        ext_config['detids_runs_to_do'] = {}

    ext_config['detidgroups_detids_to_do'] = parse_detidgroup_list(config, ext_config['detids_runs_to_do'])

    ext_config['optimization_values_to_do'] = {
            "cuts": parse_cutvar_values(config),
            "binning": {} #TODO: implement...
            }
    
    return ext_config

