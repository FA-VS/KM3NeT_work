# Utility functions that don't explicitly require access to snakemake config

import copy
import sys
from contextlib import contextmanager

def deep_update(original, updates):
    """
    Recursively updates the `original` dictionary with values from `updates`.
    If both values are dicts, merges them recursively instead of overwriting.
    """
    for key, value in updates.items():
        if (
            key in original
            and isinstance(original[key], dict)
            and isinstance(value, dict)
        ):
            deep_update(original[key], value)
        else:
            original[key] = value
    return original

def get_detector_config(config, det):
    out_config = copy.deepcopy(config)
    if 'detectors' in config.keys():
        if det in config['detectors'].keys():
            deep_update(out_config, config['detectors'][det])
    return out_config

def get_inputselector_list(wildcards, source):  # To emulate "branch" behaviour available in snakemake 8+ ...
    if source in config['ResponseFunction']['sources_dtype_full']:
        return [
            f"Selector/{detid}/merged/KM3NeT_{detid}.{config['ResponseFunction']['sources_dtype_full'][source]}.{wildcards.version}.selection.root"
            for detid in extended_config['detidgroups_detids_to_do'][wildcards.detidgroup]
        ]
    else:
        # print("WARNING: Calling for source", source, "but it is not in ResponseFunction.sources_dtype_full in the config!")
        return []

@contextmanager
def redirect_to_log(log_path):
    """Redirect stdout and stderr to the given log file."""
    with open(log_path, "w") as log_file:
        old_stdout, old_stderr = sys.stdout, sys.stderr
        sys.stdout = sys.stderr = log_file
        try:
            yield
        finally:
            sys.stdout = old_stdout
            sys.stderr = old_stderr

def make_compact_inputlist(wildcards, input):
  message_lines = []
  for key, item in input.items():
    if isinstance(item, list) and len(item)>1:
      message_lines.append("        {}: [{}, ... ({} files)]".format(key, item[0], len(item)))
    else:
      message_lines.append("        {}: {}".format(key, item))
  return "\n".join(message_lines)

compact_message_template = """
rule {rule}:
    input:
{params.compact_inputlist}
    output: {output}
    log: {log}
    resources: runtime={resources.runtime}, mem_mb={resources.mem_mb}, disk_mb={resources.disk_mb}, tmpdir={resources.tmpdir}
    wildcards: {wildcards}
"""

