# Get the rules needed to create response functions
include: "irf/class_selector.smk"
include: "irf/irf_builder.smk"

# Get the rules needed to execute kofi
include: "kofi/inputs.smk"
include: "kofi/applications.smk"

# Get the rules needed to do the optimization
include: "optimization/optimization.smk"
