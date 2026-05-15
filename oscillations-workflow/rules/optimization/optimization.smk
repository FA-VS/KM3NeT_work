wildcard_constraints:
  kofi_optimize_metric="Interval|Contour"


rule find_optimal_irf:
    localrule: True
    output:
        irf = temp("ResponseFunction/merged/Optimized/{kofi_optimize_metric}/"+config['ResponseFunction']['name']+".{version}_optimized{kofi_optimize_metric}.root"),
        mfile = "ResponseFunction/merged/Optimized/{kofi_optimize_metric}/"+config['KOFI']['output']['common']+".{version}_optimized{kofi_optimize_metric}.Metric.txt"
    input:
        irf_list=expand_on_optimization_values("ResponseFunction/merged/"+config['ResponseFunction']['name']+".{{version}}{optimization_wildcard}.root"),
        mfile_list=expand_on_optimization_values("Analysis/Metric/{{kofi_optimize_metric}}/"+config['KOFI']['output']['common']+".{{version}}{optimization_wildcard}.Metric.txt")
    #params:
    #    compact_inputlist = lambda wildcards, input: make_compact_inputlist(wildcards, input)
    #message: compact_message_template # requires params.compact_inputlist and resources.{runtime,mem_mb,disk_mb}
    #resources:
    #    runtime = 10, #minutes (make it dynamic)
    #    mem_mb = 1,
    #    disk_mb = 1
    log:
       "logs/ResponseFunction/merged/Optimized/{kofi_optimize_metric}/"+config['ResponseFunction']['name']+".{version}_optimized.log"
    benchmark:
       "benchmarks/ResponseFunction/merged/Optimized/{kofi_optimize_metric}/"+config['ResponseFunction']['name']+".{version}_optimized.tsv"
    run:
        import os
        import shutil

        mfile_list = input.mfile_list
        metric_min_value = None
        metric_min_index = None
        metric_values = []

        # read all optimization files
        for i, filepath in enumerate(mfile_list):
            with open(filepath, "r") as fin:
                rawvalue = fin.read().strip()
                try:
                    metric = float(rawvalue)
                    metric_values.append((os.path.basename(filepath), metric))
                    if metric_min_value is None or metric < metric_min_value:
                        metric_min_value = metric
                        metric_min_index = i
                except ValueError:
                    metric_values.append((os.path.basename(filepath), rawvalue))


        # write merged metrics
        with open(output.mfile, "w") as fout:
            fout.write("filename\tmetric_value\n")
            for filename, metric in metric_values:
                fout.write(f"{filename}\t{metric}\n")
            fout.write("\nBEST VALUE\n")
            (filename_min, metric_min) = metric_values[metric_min_index]
            fout.write(f"{filename_min}\t{metric_min}\n")

        # copy the corresponding root files for the lowest optimization
        shutil.copy( input.irf_list[metric_min_index], output.irf)

