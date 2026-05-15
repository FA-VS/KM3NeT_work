# Workflow for KOFI-based Oscillations Analyses with automatic generation of (optimal) Instrument Response Functions

## Environment

You must have snakemake available in your environment. If you intend to use an HTCondor cluster (e.g. Stoomboot, CNAF), you also need htcondor bindings for Python.

### Pre-build environments

In any machine with access to CVMFS, you can call the following KM3NeT environment (uses snakemake 7.32.4 at time of writing):
```
 source /cvmfs/km3net.egi.eu/micromamba/micromamba_x86.sh && micromamba activate km3env_v11.0.0-rc7_<OS>-x86
```
where OS is one of `redhat9`, `alma8` or `rocky9` (if unsure, run `grep "^ID=" /etc/os-release | cut -c 4-` to know which).
This is the recommended way to access our environment. If you want the environment for more than just running the oscillations workflow,
and some package is missing, open an issue on the [repo](https://git.km3net.de/workflow-management/km3env).

Alternatively, in IN2P3-CC, you may access a working environment with (uses snakemake 7.32.4 at time of writing):
```
 module load km3net_soft_env
```
This will eventually become deprecated.

### Build your own environment

You can create your own conda environment with the following:
```
 conda config --set channel_priority strict
 conda create --name workflow_env -c conda-forge -c bioconda -c nodefaults snakemake=7.32.4 python=3.9.20 python-htcondor
```

or, for snakemake 9:
```
 conda config --set channel_priority strict
 conda create --name workflow_env -c conda-forge -c bioconda -c nodefaults snakemake=9.15.0 python=3.13.11 python-htcondor
```

Once the environment has been created, you can activate it with:
```
 conda activate workflow_env
```

If your home folder has limited space, then use `--prefix` instead of `--name` in the create command, to save the files in a more appropriate location, e.g.:
```
 conda create --prefix /scratch/$USER/conda/envs/workflow_env [...]
```

Note that then you need to activate the env with its full path (or mess around with your `.condarc` file), i.e.:
```
conda activate /scratch/$USER/conda/envs/workflow_env
```

### Manage downloaded singularity/apptainer images

Additionally, if you are running in a machine with limited home space, you need to set up the path to the cache of singularity/apptainer to some scratch space.
In IN2P3-CC, add the following lines to your `.bashrc`:
```
 export SINGULARITY_CACHEDIR=/scratch/$USER/cache/singularity_cache
 export APPTAINER_CACHEDIR=/scratch/$USER/cache/apptainer_cache
```
For Stoomboot, add the following lines to your `.bashrc`:
```
 export SINGULARITY_CACHEDIR=/tmp/$USER/cache/singularity_cache
 export APPTAINER_CACHEDIR=/tmp/$USER/cache/apptainer_cache
```

*Make sure those folders exist before running!*


## How to run

Steps:
```
 git clone git@git.km3net.de:oscillation/oscillations-workflow.git
 cd oscillations-workflow/
 snakemake <output_rule> -c <cores> -d <work_dir> --configfile <config.yaml> --config runs_list=<full_path_to_runs_list>
```

Before executing it, it is recommended to do a dry run by adding `-np` at the end of the previous line. This will not launch the workflow, it will display the files that need to be created and which rules will be called.

At the moment, `output_rule` can be one of:
- `make_selector`, `make_irf`, `make_kofi_benchmark`, `make_kofi_fit`, `make_kofi_interval`, `make_kofi_contour`. A non-optimization config file must be provided (i.e. no wildcards in the cut definitions, and no "Optimization" section).
- `make_irf_alloptvalues`, `optimize_contour`, `optimize_interval`. An optimization config file must be provided (i.e. with wildcards in the cut definitions and an "Optimization" section). Development is ongoing.

If no `output_rule` is given, then the workflow will instead generate all the files in the `to_produce` field of the input config yaml. You generally want a non-optimization config file for this. Note that the only wildcard that can appear in the files listed in `to_produce` is {version}.

A folder with Score trees needed to run the example can be found in CC-IN2P3, `/sps/km3net/users/imozun/oscillations_smk/Scores_Test/`. Another can be found in `/pbs/home/j/jprado/sps/pid_2_dst/Apply_model/scoreTrees_results/v9.2/{detid}/` ({detid} can be left as-is in the config file, the workflow will automatically fill in the value).

## Yaml input

If you want to run Kofi (or just IRF) with fixed parameters, the config yaml should include the following information:
- Version is the version of the processed files
- to_produce lists the output files to generate, if no rule or output file is specified. This is optional.
- DetIDgroups is an optional dictionary with a way to group detid's for convenience. For each user-defined group name (e.g. "D0ORCA6), add a list of detids (e.g. ["00000049"]).
- Selector lists the parameters for the Selector stage of IRF:
  - dst_common_path gives the path to the DST files to be used as input. The filenames inside are expected to be of the form `KM3NeT_{detid}_{run}.{dtype_full}.{version}.root`. If there are different paths for data, nu, mu and noise files, you can add these terms as keys and add a path for each. The path can use `{version}` and `{detid}` (but not {run}), they will get auto-filled by the workflow.
  - scores_common_path gives the path to the score files to be used as input, and works the same way as dst_common_path. The expected filename inside the folder is `ScoreTree_PID_KM3NeT_{detid}_{run}.{dtype_full}.{version}.scoretree.root`.
  - scores_ttrename is the name of the TTree inside the scores TFile.
  - cut is the string with the preliminary cut to apply to all of the selector files.
  - output_common_path is where the output files of the Selector stage will be saved. Note that it *has* to be a single path. `{detid}/{run}` gets added automatically to the end of this path. Note that you can use an absolute path here, if you want to use pre-existing Selector files.
- ResponseFunction lists the parameters for the ResponseFunction of IRF:
  - singularity is the path to the IRF image. This image is also used for the Selector stage.
  - json_preconfig is the absolute path to a config file with the base parameters for IRF. You can find an example in the configs folder. Make sure this path is absolute! Otherwise the workflow will likely fail.
  - selector_scratch_path, if specified, will temporarily copy the Selector files to this path before passing them to irf_builder. Useful if output_common_path is slow storage.
  - sources_dtype_full is used to find the input dst and score files. It expects up to four keys (from data, nu, mu, noise) with the extended `dtype` following the naming convention for the run-based data processing. *Note that IRF will only process input files if their respective key appears here!* Note also that Kofi expects at least nu and mu information to produce its outputs.
  - name is what the output merged IRF file will be called, adding the processing version and `.root` at the end.
  - exposure is only used by Kofi (TODO: actually do something with this?) to scale the output of IRF.
  - the contents of json_args get turned into a json, which overwrites any values from the `pre_config` json before being provided to IRF. Check [the IRF repository](https://git.km3net.de/oscillation/irf) for details on the options!
  - detectors has a list of keys, one per detid or detid group, and each one just has one key, `json_args`, which overwrites both the `pre_config` and the previously mentioned json_args. Note that this list does *not* define which detectors the workflow will run over - that is determined by the input runlist. But the workflow will (likely) fail if the detid (or relevant detid group name) is missing from the yaml config.
- Kofi lists the parameters for Kofi. Right now, that's just the singularity image, flux (as a filename, works only for the files included in Kofi itself), and the name of the output file. This requires some additional work.
  - the contents of the (optional) json_args get turned into a json, which overwrites any values from the default json provided with the KOFI repository. Check [that repository](https://git.km3net.de/oscillation/kofi) out for details on the options!

If you want to run an optimization workflow (which currently compares the area of the Kofi contour to find the `best` cut parameter values), you need some additional parameters in the configuration. You can define arbitrary wildcards, and use them in the cut definitions that appear in the json_args mentioned earlier (e.g. `cuts:analysis: "{muonscore_low} < muonscore && muonscore < {muonscore_high}`); the loose cuts can also use wildcards. Those wildcards are then set based on:
- Optimization:
  - cuts:
    - wildcardname (e.g. muonscore_high from the previous example): is a dictionary which can be filled one of two ways:
      - custom, which is a list with the values to try for the wildcard. If defined, "custom" takes priority in the workflow over the second approach:
      - start, end and nsamples, which will generate a list of nsample values automatically, including both start and end. Optionally, you might also include `log: true` if you want the values logarithmically spaced, instead of linearly.

Checking the two example yaml's might help make sense of the structure of this part of the yaml. Note that the values adopted by the optimization wildcards are fixed simultaneously for all detid's and all detid groups! That is, the contour will be computed *once* for each combination of values of the wildcards, with the merged IRF from all the detectors. If you want to optimize detector groups which have different parameters (e.g. you expect, say, the trackscore cut to be different for ORCA6 and ORCA11), then you will need to make each optimization separately, with one runlist and (probably) one config file per detector group.


## Running on a cluster

Use the included profiles if you want to run on a cluster. For example, if you want to use the IN2P3-CC batch system, add the following argument to your snakemake call:

`--profile $PWD/profiles/cc-in2p3` (the $PWD part is assuming you are calling it from within the top folder of this repo).

... more profiles incoming.
