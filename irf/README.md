# IRF

Clone the project and start the submodules (km3net-dataformat and json). Note that you need ROOT available for the code to work!

```
$ git clone --recurse-submodules --shallow-submodules git@git.km3net.de:vcarretero/irf.git
$ cd irf
$ source ./setenv.sh
$ make
```

Creating the Response Function file is done in two steps:

### Selector

```
$ cd $IRF_DIR/Selector
$ ./SelectorFromJSON -j Input_expandable.json
```

SelectorFromJSON gathers together the needed TTrees to generate the ResponseFunction, and applies a preliminary cut to reduce the size of the resulting file. 

Input_expandable.json contains the name of the input and output file. The input file must have the TTrees "E" (TTREE_OFFLINE_EVENT) and "T" (not defined in km3net-dataformat as of writing).

The list of friendtrees are only needed for the applied cuts (e.g. on event PIDs), in this step and the next. It's a dictionary, where each key is the path to the TFile, and the value is the list of names of TTrees to be fetched from the files. They will be copied to the outputfile too.

TCut is the preliminary cut to be applied on the input files, to reduce the resulting size.

You can also call SelectorFromJSON with CLI arguments (instead of a JSON) to easily loop over files. See the doall_<ext>.sh scripts in the Selector folder for some examples.

### ResponseTTreeCreator

```
$ cd $IRF_DIR/ResponseFunction
$ ./ResponseTTreeCreator -j Basic_TTreeCreator.json
```

ResponseTTreeCreator generates a ROOT file containing `TTrees ResponseTTree_<source>_<class>`, where source is one of nu, mu, noise, data, and AnaClass is defined in the input json.
The TTrees contain the following branches (depending on type of source file):
- E_reco_bin : Reconstructed energy bin index
- E_reco_bin_center : Reconstructed energy bin center
- Ct_reco_bin : Reconstructed cosine of the zenith angle bin index
- Ct_reco_bin_center : Reconstructed cosine of the zenith angle bin center
- E_true_bin : True energy bin index
- E_true_bin_center : True energy bin center
- Ct_true_bin : True cosine of the zenith angle bin index
- Ct_true_bin_center : True cosine of the zenith angle bin center
- Pdg : Pdg number nue - 12, numu - 14, nutau - 16 (negative for antineutrinos)
- IsCC : Charged current flag NC -0 CC - 1
- By_true_bin : True Bjorken-y bin index
- By_true_bin_center : True Bjorken-y bin center
- W : Neutrino weight per unit flux (GeV m^2 sr)
- WE : Variance of the neutrino weight (GeV m^2 sr)^2

TH1D with distributions for E_reco, Ct_reco, E_true, Ct_true and By_true are also generated, for cross-check purposes only. Note that the errors on the histograms do *not* represent WE!

The input json contains the list of paths to the files produced by Selector to be taken as inputs (one such list per source type), the path to the output file, and the binning to be used for the relevant variables. It gets saved as a TNamed in the output file.

It also contains the "class" definitions, with the relevant name and cut. If a "loose" cut is provided (e.g. "cut_loose_mu") for a given class / source combination, the loose cut will be used for the Response Function, with weights scaled by the ratio of events between the regular and loose cut. The binning can also be re-defined here, and will supercede the general one.

For each class, you can provide "branchformulas", to be used to fill output TTree branches, as shown in `ResponseFunction/Test_TTreeCreator.json`. These will be interpreted as TTreeFormula from the input friended TTrees/TChains. If a branch formula is defined in both a class and in the top level of the json, the one in the class will take priority when processing that class. TTreeFormula can be taken advantage of in a few ways (some of these require further testing):
- to fetch different branches for the reconstructed values, e.g. `Evt->hits.size()` instead of `Evt.trks[0].E`;
- to define simple reconstruction formulae, e.g. `TMath::Sqrt(1.0 + TMath::Power(Evt->mc_trks[0].E,2))`; note that TTreeFormula only has access to the TMath namespace;
- to introduce randomness with the "rndm" keyword, which returns a random value between 0 and 1; e.g. generate a random normally distributed number with `sin(2*pi*rndm)*sqrt(-2*log(rndm))`;

If no such formula is provided, either in the class definition or at the top level of the json input, then the default formulas used are (see Classes/ResponseDefaults.cc):
- E_reco = "Evt.trks[index].E" (index = 1 for showers, 0 otherwise)
- Ct_reco = "-Evt.trks[index].dir.z" (idem)
- E_true = "Evt->mc_trks[0].E"
- Ct_true = "-Evt->mc_trks[0].dir.z"
- Pdg = "Evt->mc_trks[0].type"
- IsCC = "(Evt->w2list[W2LIST_GSEAGEN_CC]==2)"
- By_true = "Evt->w2list[W2LIST_GSEAGEN_BY]"
- W = 
    "1" for data,
    "sum_mc_evt.livetime_DAQ / sum_mc_evt.livetime_sim" for mu or noise,
    "sum_mc_evt.livetime_DAQ * Evt->w[WEIGHTLIST_DIFFERENTIAL_EVENT_RATE]/sum_mc_evt.n_gen" for nu

You can also add new branches to the output IRF file, by adding new names and formulas in the "branchformulas" sections, either the general or only for some classes. These will behave as the "W" branch, i.e. their values will get added up in each bin in "aggregate" mode. Note that for these additional branches, their formulas will be used for all sources! Split into multiple json files if the desired formula is incoherent for some (e.g. weight systematics wouldn't apply for a data file).

There are some additional CLI options for the ResponseTTreeCreator executable:

```
$ ./ResponseTTreeCreator -j input.json -o outputpath -s source -b "pointer=(nbins,min,max)" -B "pointer=[bin_edges]" -i "pointer=value" -I "pointer=[values]"
```

where

- input.json is self-explanatory. You can call this option multiple times, the later jsons will overwrite the values of the previous ones.
- outputpath is the name of the output root file to create.
- source where source is one of 'data', 'nu', 'mu', 'noise' asks ResponseTTreeCreator to only run for one of these sources (ignoring other input files in the json).
- "pointer=(nbins,min,max)", where "pointer" is of the form e.g. /binning/E_true or /classes/showers/binning/E_reco, will overwrite the values of nbins, min and max for those bins.
- "pointer=[bin_edges]", does the same, but with manually given values for the bin edges (e.g. `-B "/binning/E_reco=[2.0,4.0,5.321,7.078,9.415,12.52,16.66,22.16,29.48,50,100,1000]"`).
- i (resp. I) overwrite the value of the string (resp. vector of strings) at the specified json pointer (e.g. `-i "/branch_formulas/E_reco=Evt->hits.size()"`). NOTE: This will only work if the value should indeed be a string!

If any values are modified from the base json, a new json will be written simultaneously with the output file.


### ResponseTTreeMerger

```
$ cd $IRF_DIR/ResponseFunction
$ ./ResponseTTreeMerger -o outputfilename.root -i inputfilename1.root inputfilename2.root ...
```

ResponseTTreeMerger generates a ROOT file containing the properly merged ResponseTTrees from various input files. The key assumptions are that those files include the exact same combinations of sources and class names, and that the classes have the same binning. The jsons saved within each input file are also saved onto the output file. Note that ResponseTTreeMerger *always* assumes aggregate_response_bins == True! Try using hadd if that's not the behaviour you want.


## Deployment

After pushing a tagged version, the CI/CD will automatically create a docker image and push it to the repository's container registry, with the name `$CI_COMMIT_REF_SLUG`. You may then navigate to the pipelines tab in the repository, and press on the `run manual or delayed jobs` button next to the matching pipeline, to release both the docker and singularity images to the [gitlab container registry](https://git.km3net.de/oscillation/irf/container_registry/259) and to our [SFTP](https://sftp.km3net.de/singularity/) respectively.

## Other questions

Any questions you can go to the chat to reach us (Víctor Carretero, Francisco Vazquez) or the email vcuenca@nikhef.nl or pvazquez@nikhef.nl

