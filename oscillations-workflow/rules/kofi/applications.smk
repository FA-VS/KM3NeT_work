wildcard_constraints:
  kofi_output="Benchmark|Fit|Interval|Contour"

rule json_outputname: # Kofi should be updated so this isn't necessary anymore...
    """
    This rule creates a json file with the output name to be parsed by kofi applications.
    """
    localrule: True
    output:
        outputname_json = temp("Analysis/inputs/OutputName/OutputName_{kofi_output}.{version}{optimization_wildcard}.json")
    params:
        output_folder = "Analysis/{kofi_output}",
        output_name = config["KOFI"]["output"]["common"] + ".{version}{optimization_wildcard}" # the main output file will have" _Kofi{kofi_output}.root" added at the end of this, except for benchmark
    shell:
        """
        jq -n '{{
                    "fit": {{
                              "output": "{params.output_folder}/{params.output_name}"
                    }}
        }}' > {output.outputname_json}
        """

rule run_kofi:
    """
    Calls one of KofiBenchmark, KofiFit, KofiInterval or KofiContour, and creates a flag file when complete.
    """
    wildcard_constraints:
        kofi_output="Fit|Interval|Contour" # HACK...
    output:
        #kofi_folder = directory("Analysis/{kofi_output}/{version}"),
        files = "Analysis/{kofi_output}/" + config["KOFI"]["output"]["common"] + ".{version}{optimization_wildcard}_Kofi{kofi_output}.root", # Not exhaustive?
        success_flag = "flags/{kofi_output}.{version}{optimization_wildcard}.DONE"
    input:
        kofi_inputs="flags/kofi_inputs_ready.DONE", # Is this redundant?
        additional_json="Analysis/inputs/AdditionalConfig.json",
        flux_json="Analysis/inputs/Flux/Flux.json",
        response_json= "Analysis/inputs/ResponseFunction/"+config['ResponseFunction']['name']+".{version}{optimization_wildcard}.json",
        flux_path= "Analysis/inputs/"+config['KOFI']['flux'],
        irf_path= "Analysis/inputs/"+config['ResponseFunction']['name']+".{version}{optimization_wildcard}.root",
        outputname_json="Analysis/inputs/OutputName/OutputName_{kofi_output}.{version}{optimization_wildcard}.json"
    container:
        config["KOFI"]["singularity"]
    resources:
        runtime = lambda wc: 120 if wc.kofi_output == "Interval" else 5,
        mem_mb = 500,
        disk_mb = 10
    log:
        "logs/Analysis/{kofi_output}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}.log"
    benchmark:
        "benchmarks/Analysis/{kofi_output}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}.tsv"
    shell:
        """
        source /Kofi/setenv.sh

        mkdir -p Analysis/{wildcards.kofi_output}
        /Kofi/Applications/Kofi{wildcards.kofi_output} {input.additional_json} {input.flux_json} {input.response_json} {input.outputname_json} > {log} 2>&1

        touch {output.success_flag}
        """

use rule run_kofi as run_kofi_benchmark with:
#    """
#    Calls KofiBenchmark, and creates a flag file when complete.
#    """
    wildcard_constraints:
        kofi_output="Benchmark" # HACK...
    output:
        "Analysis/{kofi_output}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_Kofi{kofi_output}_flux.root",
        "Analysis/{kofi_output}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_Kofi{kofi_output}_osc.root",
        "Analysis/{kofi_output}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_Kofi{kofi_output}_oscillatedflux.root",
        #[ # TEST THIS PART FIRST!!!
        #  "Analysis/{kofi_output}/{version}/"+config["KOFI"]["output"]["common"]+"_" + classname +".{version}{optimization_wildcard}_Kofi{kofi_output}_" + otype +" .root"
        #  for otype in ["data", "detresp", "prediction", "background"]
        #  for classname in config['ResponseFunction']['json_args']['classes'].keys() # UPDATE THIS!!
        #],
        success_flag = "flags/{kofi_output}.{version}{optimization_wildcard}.DONE"

rule parse_contour_results:
    localrule: True
    output:
        "Analysis/Metric/Contour/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}.Metric.txt"
    input:
        "Analysis/Contour/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_KofiContour.root"
    container:
        config["KOFI"]["singularity"]
    log:
        "logs/Analysis/Metric/Contour/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}.Metric.log"
    benchmark:
        "benchmarks/Analysis/Metric/Contour/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}.Metric.tsv"
    shell:
        """
        {{

        # This line reads the output file and extracts the area of the contour.
        # If not computed, it saves it as a 0 value instead
        if [ -f {input} ]; then
            root -l -b -n -q {input} \
            -e 'auto p=((TParameter<double>*)_file0->Get("ContourArea")); if(p) std::cout << p->GetVal() << std::endl;' \
            | tail -n 1 > {output}
        else
            echo "0" > {output}
        fi
        }} > {log} 2>&1
        """

rule parse_interval_results:
    localrule: True
    output:
        "Analysis/Metric/Interval/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}.Metric.txt"
    input:
        "Analysis/Interval/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_KofiInterval.root"
    container:
        config["KOFI"]["singularity"]
    log:
        "logs/Analysis/Metric/Interval/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}.Metric.log"
    benchmark:
        "benchmarks/Analysis/Metric/Interval/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}.Metric.tsv"
    shell:
        """
        {{

        # This line reads the output file and extracts the interval of the profile.
        # If not computed, it saves it as a 0 value instead
        if [ -f {input} ]; then
            root -l -b -n -q {input} \
                 -e 'auto p = (TParameter<double>*)_file0->Get("Interval");
                    if (p) std::cout << p->GetVal() << std::endl;
                    else std::cout << 0 << std::endl;' \
                 2>/dev/null | grep -E '^[0-9]' > {output}
        else
            echo "0" > {output}
        fi
        }} > {log} 2>&1
        """

# This dictionary is not used at the moment (TODO: can we use it in a rule output?)
kofi_output_dict = {
    "Benchmark" : [ #HPT, LPT, S are the classes...
        "Analysis/Benchmark/{version}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_KofiBenchmark_flux.root",
        "Analysis/Benchmark/{version}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_KofiBenchmark_osc.root",
        "Analysis/Benchmark/{version}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_KofiBenchmark_oscillatedflux.root"
        ] + [
          "Analysis/Benchmark/{version}/"+config["KOFI"]["output"]["common"]+"_" + classname +".{version}{optimization_wildcard}_KofiBenchmark_" + otype +" .root"
          for otype in ["data", "detresp", "prediction", "background"]
          for classname in config['ResponseFunction']['json_args']['classes'].keys() # UPDATE THIS!!
        ],
    "Fit" : [
        "Analysis/Fit/{version}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_KofiFit.root"
        ],
    "Interval" : [
        "Analysis/Interval/{version}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_KofiInterval.root",
        "Analysis/Interval/{version}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_KofiInterval.png"
        ],
    "Contour" : [
        "Analysis/Contour/{version}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_KofiContour.root",
        "Analysis/Contour/{version}/"+config["KOFI"]["output"]["common"]+".{version}{optimization_wildcard}_KofiContour.png"
        ]
    }
#make_kofi_output_list(wildcards):
#  prelist = kofi_output_dict[wildcards.kofi_output]
#  return expand(prelist, version = wildcards.version, optimization_wildcard=wildcards.optimization_wildcard)
