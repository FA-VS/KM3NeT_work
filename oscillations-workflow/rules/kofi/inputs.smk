
rule get_default_inputs:
    """
    This rule downloads the default inputs for kofi.
    PLACEHOLDER (currently copies them from the Kofi container)
    """
    localrule: True
    output:
        #kofi_inputs_folder=directory("Analysis/inputs/"), # complains because directory is modified later?
        flux="Analysis/inputs/"+config["KOFI"]["flux"], #Why does this not work?
        success="flags/kofi_inputs_ready.DONE",
    params:
        kofi_inputs_folder="Analysis/inputs/"
    log:
        "logs/Analysis/inputs/default_inputs.log"
    benchmark:
        "benchmarks/Analysis/inputs/default_inputs.tsv"
    container:
        config["KOFI"]["singularity"]
    shell:
        """
        {{
            mkdir -p Analysis/inputs/
            cp /Kofi/inputs/* {params.kofi_inputs_folder}
            #ln -s /Kofi/inputs/* {params.kofi_inputs_folder} #Issues, since we are copying from a container
            ls {params.kofi_inputs_folder}
            touch {output.success}
        }} > {log} 2>&1 | tee {log}
        """


rule generate_flux_json:
    """
    This rule creates a json file with the flux options to be parsed to kofi applications.
    """
    localrule: True
    output:
        flux_json = "Analysis/inputs/Flux/Flux.json"
    params:
        flux_name = config["KOFI"]["flux"]
    shell:
        """
        jq -n '{{
                    "flux": {{
                              	"input_path": "Analysis/inputs/", 
                                "fluxfile": "{params.flux_name}"
                    }}
        }}' > {output.flux_json}
        """

ruleorder: copy_irf_json > generate_irf_json_optimized > generate_irf_json

rule copy_irf_json:
    """
    Copy a JSON file with the Instrument Response Function options to be parsed by KoFi applications.
    Additionally, copy an existing IRF file to Analysis/inputs/, so KOFI can find it.
    """
    localrule: True
    output:
      irf_path = "Analysis/inputs/"+config['ResponseFunction']['name']+".{version}{optimization_wildcard}.root",
      irf_json = "Analysis/inputs/ResponseFunction/"+config['ResponseFunction']['name']+".{version}{optimization_wildcard}.json"
    input:
      irf_path = config['ResponseFunction']['output_path_irf'] if 'output_path_irf' in config['ResponseFunction'] else "",
      irf_json = config['ResponseFunction']['output_path_json']  if 'output_path_json' in config['ResponseFunction'] else ""
    log:
        "logs/Analysis/inputs/ResponseFunction/"+config['ResponseFunction']['name']+".{version}{optimization_wildcard}.log"
    shell:
        """
        {{
        cp {input.irf_path} {output.irf_path}
        cp {input.irf_json} {output.irf_json}
        }} > {log} 2>&1 | tee {log}
        """

rule generate_irf_json:
    """
    Create a JSON file with the Response Function options to be parsed by KoFi applications.
    Additionally, copy the IRF file to Analysis/inputs/, so KOFI can find it.
    """
    localrule: True
    output:
        irf_path="Analysis/inputs/"+config['ResponseFunction']['name']+".{version}{optimization_wildcard}.root",
        irf_json="Analysis/inputs/ResponseFunction/"+config['ResponseFunction']['name']+".{version}{optimization_wildcard}.json"
    input:
        irf_path="ResponseFunction/merged/"+config['ResponseFunction']['name']+".{version}{optimization_wildcard}.root"
    params:
        irf_name = config['ResponseFunction']['name']+".{version}{optimization_wildcard}.root",
        irf_args = config['ResponseFunction']['json_args'],
        exposure = config['ResponseFunction']['exposure']
    log:
        "logs/Analysis/inputs/ResponseFunction/"+config['ResponseFunction']['name']+".{version}{optimization_wildcard}.log"
    run:

      with redirect_to_log(log[0]):
          # Need to copy the created RF into /kofi/inputs/
          shutil.copy(input.irf_path, output.irf_path)
          #os.symlink(input.irf_path, output.irf_path) //does not work because its outside the bounded singularity folders...

          # Write the json with the classes and the irf filename
          classes_json = []
          for class_name, class_args in params.irf_args['classes'].items():
            single_class_dict = {
                "name": class_name,
                "exposure": params.exposure,
                "filename": params.irf_name, # Should this be output.irf_path? Should this be absolute?
                "TTree_nu": f"ResponseTTree_nu_{class_name}",
                "TTree_mu": f"ResponseTTree_mu_{class_name}",
                "histfile": f"test_histdata_{class_name}" # What does this one do again?
                }
            classes_json.append(single_class_dict)

          with open(output.irf_json, 'w') as f:
            json.dump({"responses": classes_json}, f, indent=4)

use rule generate_irf_json as generate_irf_json_optimized with: #Check rules/optimize for inputs
    output:
        irf_path="Analysis/inputs/"+config['ResponseFunction']['name']+".{version}_optimized{kofi_optimize_metric}.root",
        irf_json="Analysis/inputs/ResponseFunction/"+config['ResponseFunction']['name']+".{version}_optimized{kofi_optimize_metric}.json"
    input:
        irf_path="ResponseFunction/merged/Optimized/{kofi_optimize_metric}/"+config['ResponseFunction']['name']+".{version}_optimized{kofi_optimize_metric}.root"
    params:
        irf_name = config['ResponseFunction']['name']+".{version}_optimized{kofi_optimize_metric}.root",
        irf_args = config['ResponseFunction']['json_args'],
        exposure = config['ResponseFunction']['exposure']
    log:
        "logs/Analysis/inputs/ResponseFunction/"+config['ResponseFunction']['name']+".{version}_optimized{kofi_optimize_metric}.log"

rule generate_additionalconfig_kofi_json:
    """
    This rule will generate an additional input json for KOFI based on the oscillation-workflows config yaml.
    """
    localrule: True
    output:
        additional_json="Analysis/inputs/AdditionalConfig.json"
    params:
        kofi_args=lambda wildcards: config['KOFI']['json_args'] if 'json_args' in config['KOFI'] else {}
    log:
        "logs/Analysis/inputs/AdditionalConfig.log"
    benchmark:
        "benchmarks/Analysis/inputs/AdditionalConfig.tsv"
    run:

      with redirect_to_log(log[0]):
        new_args = params.kofi_args

        with open(output.additional_json, 'w') as f:
            json.dump(new_args, f, indent=4)
