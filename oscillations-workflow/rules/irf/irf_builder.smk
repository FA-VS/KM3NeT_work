rule generate_additionalconfig_irf_json:
    """
    This rule will generate an additional input json for IRF based on the oscillation-workflows config yaml.
    Additional parameters are updated based on optimization requests.
    """
    localrule: True
    output:
        json="ResponseFunction/json/{detidgroup}/AdditionalConfig.{detidgroup}.{version}"+optimization_wildcard_explicit()+".json"
    params:
        irf_args=lambda wildcards: get_detector_config(config['ResponseFunction'], wildcards.detidgroup)['json_args'],
        optimization_cut_names = extended_config['optimization_values_to_do']['cuts'].keys()
    log:
        "logs/ResponseFunction/json/{detidgroup}/AdditionalConfig.{detidgroup}.{version}"+optimization_wildcard_explicit()+".log"
    benchmark:
        "benchmarks/ResponseFunction/json/{detidgroup}/AdditionalConfig.{detidgroup}.{version}"+optimization_wildcard_explicit()+".tsv"
    run:

      with redirect_to_log(log[0]):
        new_args = params.irf_args
        opt_cut_names = params.optimization_cut_names

        if len(opt_cut_names)>0:
          optimization_dict = {k: wildcards[k] for k in opt_cut_names} # Subset of wildcards relevant to optimization
          # Replace wildcards/placeholders in all cuts by explicit values
          if 'cuts' in new_args.keys():
            for cutname, cut in new_args['cuts'].items():
              new_args['cuts'][cutname] = cut.format(**optimization_dict)
          for anaclass in new_args['classes'].values():
            if 'cuts' in anaclass.keys():
              for cutname, cut in anaclass['cuts'].items():
                anaclass['cuts'][cutname] = cut.format(**optimization_dict)

        with open(output.json, 'w') as f:
            json.dump(new_args, f, indent=4)

def generate_inputselector_list_json_input(wildcards):
    detids = extended_config['detidgroups_detids_to_do'][wildcards.detidgroup]
    input_dtypes = config['ResponseFunction']['sources_dtype_full']
    output_dict = {}
    for name, dtype in input_dtypes.items():
        output_dict[name] = expand(
           "Selector/{detid}/merged/KM3NeT_{detid}.{dtype}.{version}.selection.root",
           detid = detids, dtype = dtype, **wildcards
        )
    return output_dict

rule generate_inputselector_list_json:
    """
    This rule will generate an additional input JSON for IRF that collates all the necessary input selector files.
    Necessary to get around the maximum command line argument length.
    """
    localrule: True
    output:
        json = temp("ResponseFunction/json/{detidgroup}/InputSelectorList.{detidgroup}.{version}.json")
    input:  # Could have been params instead, but this way the workflow tree is more explicit
        unpack(generate_inputselector_list_json_input)
    log:
        "logs/ResponseFunction/json/{detidgroup}/InputSelectorList.{detidgroup}.{version}.log"
    run:
        with redirect_to_log(log[0]):
            inputs_dict = {
                "input_data": [],
                "input_mu": [], 
                "input_nu": [], 
                "input_noise": [],  
            }
            for name in input.keys():
                inputs_dict[f"input_{name}"] = input[name]   
     
            with open(output.json, 'w') as f:
                json.dump(inputs_dict, f, indent=4)

rule build_response_function:
    """
    This rule builds the response function for one detector and the classes defined in the json
    by merging and "pre-binning" the selector files for all runs of that detector.
    """
    output:
        irf_root = "ResponseFunction/unmerged/{detidgroup}/ResponseFunction.{detidgroup}.{version}{optimization_wildcard}.root",
        irf_json = "ResponseFunction/unmerged/{detidgroup}/ResponseFunction.{detidgroup}.{version}{optimization_wildcard}.json"
    input:
        unpack(generate_inputselector_list_json_input), #Making implicit input, explicit
        json_preconfig=config['ResponseFunction']['json_preconfig'], #Update? e.g. "Response_Function/json_pre/{detidgroup}/TTree_Creator.{detidgroup}.{version}.pre.json"
        json_additional = "ResponseFunction/json/{detidgroup}/AdditionalConfig.{detidgroup}.{version}{optimization_wildcard}.json",
        json_inputselectorlist = "ResponseFunction/json/{detidgroup}/InputSelectorList.{detidgroup}.{version}.json",
    container:
        config["ResponseFunction"]["singularity"]
    log:
        "logs/ResponseFunction/unmerged/{detidgroup}/ResponseFunction.{detidgroup}.{version}{optimization_wildcard}.log"
    benchmark:
        "benchmarks/ResponseFunction/unmerged/{detidgroup}/ResponseFunction.{detidgroup}.{version}{optimization_wildcard}.tsv"
    shell:
        """
        {{

        source /IRF/setenv.sh

        /IRF/ResponseFunction/ResponseTTreeCreator \
            -j "{input.json_preconfig}"\
            -j "{input.json_additional}"\
            -j "{input.json_inputselectorlist}"\
            -o {output.irf_root}

        }} > {log} 2>&1 | tee {log}
        """

rule merge_response_functions:
    localrule: True
    output:
        "ResponseFunction/merged/"+config['ResponseFunction']['name']+".{version}{optimization_wildcard}.root"
    input:
        lambda wildcards: expand(
            "ResponseFunction/unmerged/{detidgroup}/ResponseFunction.{detidgroup}.{version}{optimization_wildcard}.root",
            detidgroup=extended_config['detidgroups_detids_to_do'].keys(),
            version=wildcards.version,
            optimization_wildcard=wildcards.optimization_wildcard
            )
    params:
        aggregate=lambda wildcards: config['ResponseFunction']['json_args']['aggregate_response_bins'] if 'aggregate_response_bins' in config['ResponseFunction']['json_args'] else "true" # HACKY
    container:
        config["ResponseFunction"]["singularity"]
    log:
        "logs/ResponseFunction/merged/ResponseFunction.{version}{optimization_wildcard}.log"
    benchmark:
        "benchmarks/ResponseFunction/merged/ResponseFunction.{version}{optimization_wildcard}.tsv"
    shell:
        """
        {{
        source /IRF/setenv.sh

        if [[ "{params.aggregate}" == "true" ]]; then
          echo "Merging IRFs with aggregated bins"
          /IRF/ResponseFunction/ResponseTTreeMerger -o {output} -i {input}
        else
          echo "Merging IRFs without aggregated bins"
          hadd -f {output} {input}
        fi

        }} > {log} 2>&1 | tee {log}
        """
