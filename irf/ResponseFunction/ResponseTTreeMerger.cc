//#include "ResponseTTreeMerger.hh"

#include "Classes/ResponseMap.hh" //for ResponseMap, makeEvBinsFromResponseCreatorTFile
#include "Utils/EvBins.hh" //for EvBins (duh)
#include "Utils/WeightData.hh" //for WeightData
#include "Utils/Utils.hh" //for file_exists, split, GetObjectNamesInTFile, GetMatchingTTreesFromTFiles

#include "TFile.h"
#include "TTree.h"
#include "TH1D.h"

#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <getopt.h>  //command line parsing
#include <algorithm>  // count
#include <utility>    // std::pair

//Assumptions:
// Same class names across all files (and sources?)
// Same binning for each class across all files
// Could get that info from the json's saved... but it's easier to use the TH1Ds

int main(int argc, char* argv[]) {

  // Parsing CLI arguments
  std::vector<std::string> filename_list;
  std::string outputfilepath;
  int opt;
  while ( (opt = getopt(argc, argv, "o:i:h")) != EOF) {
    switch (opt) {
      case 'o':
        outputfilepath = optarg;
        break;
      case 'i':
        filename_list.push_back(optarg);
        break;
      case '?':
      case 'h':
      default :
        std::cout << "Usage: ResponseTTreeMerger -o outputpath -i inputpath1 inputpath2 inputpath3..." << std::endl;
        return 1;
    }
  }
  // Leftover positional arguments taken as additional input files
  for (int i = optind; i < argc; i++) {
    filename_list.push_back( argv[i] );
  }

  // Open the output file!
  if (file_exists(outputfilepath)) {
    std::cout << "WARNING: Output file already exists: " << outputfilepath << std::endl;
    std::cout << "Proceed? [Y/n]: ";
    std::string userinput;
    std::cin >> userinput;
    if (userinput[0] != 'Y') {
      std::cout << "Aborting..." << std::endl;
      return 1;
    } else {
      std::cout << "Overwriting..." << std::endl;
    }
  }
  TFile mergedFile(outputfilepath.c_str(), "RECREATE");


  // Find list of TTree names in the ResponseTTree files
  // -> Raises error if a file does not exist, or if the TTrees in all TFiles don't all match
  std::vector<std::string> ttreename_vec = GetMatchingTTreesFromTFiles(filename_list);
  TFile tfile_0( filename_list[0].c_str() );

  std::vector<std::string> source_vec = {"data", "nu", "mu", "noise"};
  //vector<string> binnedbranch_vec = {"E_reco", "Ct_reco", "E_true", "Ct_true", "By_true"};

  // Set up dictionary with the ResponseMaps to be used for merging
  std::map< std::pair<std::string,std::string>, ResponseMap > responsemap_dict;
  for (std::string ttreename : ttreename_vec) {
    std::vector<std::string> ttreename_split = split(ttreename, '_');
    if (ttreename_split.size() >= 3 and ttreename_split[0] == "ResponseTTree"
        and std::count(source_vec.begin(), source_vec.end(), ttreename_split[1])>0 ) 
    {
      std::string source = ttreename_split[1];
      std::string AnaClass = ttreename_split[2];
      for (size_t i_split=3; i_split<ttreename_split.size(); i_split++)
        AnaClass += "_" + ttreename_split[i_split];
      EvBins evbins = MakeEvBinsFromResponseCreatorTFile(tfile_0, source, AnaClass); // Uses TH1Ds in file0 to determine the bins
      responsemap_dict[ {source, AnaClass} ] = ResponseMap(evbins);
    } else {
      std::cout << "ERROR: Naming of ResponseTTrees do not match expected ResponseTTree_<source>_<AnaClass> format: " << ttreename << std:: endl;
      return 1;
    }
  }
  // Set up list of TH1Ds to merge
  std::vector<std::string> histname_vec = GetObjectNamesInTFile(tfile_0, "TH1D");
  std::vector<TH1D*> hist_vec;
  for (std::string histname : histname_vec) {
    TH1D* hist = tfile_0.Get<TH1D>( histname.c_str() );
    mergedFile.cd();
    hist_vec.push_back( (TH1D*) hist->Clone() );
    hist_vec.back()->Reset("ICESM"); //Clear out the contents and all the stats (leave only axes, names)
  }

  tfile_0.Close();
  std::cout << std::endl;

  // Loop over input files, merging as we go
  for (std::string filename : filename_list ) {
    TFile tfile_i(filename.c_str(), "READ");

    // Merge the ResponseMaps, looping over source, AnaClass combinations
    for ( auto& [source_class_pair, responsemap] : responsemap_dict) { // pair< pair<string,string>, ResponseMap >, cannot be made const because we modify responsemap
      std::string source = source_class_pair.first;
      std::string AnaClass = source_class_pair.second;
      EvBins evbins_new = MakeEvBinsFromResponseCreatorTFile(tfile_i, source, AnaClass); // Uses TH1Ds in file0 to determine the bins
      std::string treename = "ResponseTTree_" + source + "_" + AnaClass;
      TTree* responsettree_new = tfile_i.Get<TTree>( treename.c_str() );
      responsemap.LoadResponseTTree( *responsettree_new , source, evbins_new); //TODO: Verify that WOther matches existing branches...
      delete responsettree_new;
    }

    // Merge the histograms, looping over their names
    // Check same binning first?
    for (TH1D* hist : hist_vec) {
      TH1D* newhist = tfile_i.Get<TH1D>( hist->GetName() );
      hist->Add( newhist ); //TODO: Check if this adds errors properly
      delete newhist;
    }

    // SAVE JSONS HERE
    for (std::string jsonname : GetObjectNamesInTFile(tfile_i, "TNamed") ) {
      mergedFile.cd();
      tfile_i.Get<TNamed>( jsonname.c_str() )->Write();
    }

    // Clean up
    tfile_i.Close(); //Is this necessary?
  }


  // Variables to store in the TTree results
  Int_t     E_reco_bin;           //!< reco energy bin index
  Double_t  E_reco_bin_center;    //!< center of the reco energy bin
  Int_t     Ct_reco_bin;          //!< reco costheta bin index
  Double_t  Ct_reco_bin_center;   //!< center of the reco costheta bin
  //Int_t     By_reco_bin;          //!< reco Bjorken-y bin index (NOT IMPLEMENTED)
  //Double_t  By_reco_bin_center;   //!< center of the reco Bjorken-y bin (NOT IMPLEMENTED)
  Int_t     Pdg;                  //!< PDG number, (-)12, (-)14, (-)16 for (a)nu e, mu, tau·
  Bool_t    IsCC;                 //!< is cc (0 or 1)
  Int_t     E_true_bin;           //!< true energy bin index
  Double_t  E_true_bin_center;    //!< center of the true energy bin
  Int_t     Ct_true_bin;          //!< true costheta bin index
  Double_t  Ct_true_bin_center;   //!< center of the true costheta bin
  Int_t     By_true_bin;          //!< true Bjorken-y bin index
  Double_t  By_true_bin_center;   //!< center of the true Bjorken-y bin
  Int_t  N;                       //!< number of (unmerged) entries aggregated into this one
  Double_t  W;                    //!< weight from true bin that contribute to reco bin
  Double_t  WE;                   //!< MC statistical error of the weight
  std::map<std::string,Double_t> WOther; //!< Other variables to save

  // Write out merged ResponseTTrees:
  mergedFile.cd();
  for ( const auto& [source_class_pair, responsemap] : responsemap_dict) {
    std::string source = source_class_pair.first;
    std::string AnaClass = source_class_pair.second;
    std::cout << "Writing out ResponseTTree for source " << source << " and class " << AnaClass << std::endl;

    //Initialize WOther
    WOther.clear();
    for (const std::string& branchname : responsemap.GetWOtherNameSet()) {
      WOther[branchname] = 0;
    }

    // Initialize merget output TTree
    std::string mergedtreename = "ResponseTTree_" + source + "_" + AnaClass;
    TTree mergedTree(mergedtreename.c_str(), "TTree with binned E and cosT");
    mergedTree.Branch("E_reco_bin",         &E_reco_bin,         "Reconstructed energy bin index/I");
    mergedTree.Branch("E_reco_bin_center",  &E_reco_bin_center,  "Reconstructed energy bin center/D");
    mergedTree.Branch("Ct_reco_bin",        &Ct_reco_bin,        "Reconstructed cosine of the zenith angle bin index/I");
    mergedTree.Branch("Ct_reco_bin_center", &Ct_reco_bin_center, "Reconstructed cosine of the zenith angle bin center/D");
    if (source == "nu" or source == "mu") {
      mergedTree.Branch("E_true_bin",         &E_true_bin,         "True energy bin index/I");
      mergedTree.Branch("E_true_bin_center",  &E_true_bin_center,  "True energy bin center/D");
      mergedTree.Branch("Ct_true_bin",        &Ct_true_bin,        "True cosine of the zenith angle bin index/I");
      mergedTree.Branch("Ct_true_bin_center", &Ct_true_bin_center, "True cosine of the zenith angle bin center/D");
      mergedTree.Branch("Pdg",                &Pdg,                "Pdg number nue - 12, numu - 14, nutau - 16 (negative for antineutrinos)/I");
    }
    if (source == "nu") {
      mergedTree.Branch("IsCC",               &IsCC,               "Charged current flag NC -0 CC - 1/O");
      //mergedTree.Branch("By_reco_bin",        &By_reco_bin,        "Reconstructed Bjorken-y bin index/I");
      //mergedTree.Branch("By_reco_bin_center", &By_reco_bin_center, "Reconstructed Bjorken-y bin center/D");
      mergedTree.Branch("By_true_bin",        &By_true_bin,        "True Bjorken-y bin index/I");
      mergedTree.Branch("By_true_bin_center", &By_true_bin_center, "True Bjorken-y bin center/D");
    }
    mergedTree.Branch("N",                  &N,                  "Number of (unmerged) entries aggregated into this entry/I");
    mergedTree.Branch("W",                  &W,                  "Neutrino weight per unit flux (GeV m^2 sr)/D");
    mergedTree.Branch("WE",                 &WE,                 "Variance of the neutrino weight (GeV m^2 sr)^2/D");
    for (const auto& [name,value] : WOther) {
      mergedTree.Branch(name.c_str(), &WOther[name], (name+"/D").c_str());
    }


    std::cout << "  " << filename_list.size() << " response files aggregated into a single one with " << responsemap.GetWeightMap().size() << " 'bins'" << std::endl;
    responsemap.FillResponseTTree(
        mergedTree,
        E_reco_bin,
        E_reco_bin_center,
        Ct_reco_bin,
        Ct_reco_bin_center,
        //By_reco_bin,
        //By_reco_bin_center,
        Pdg,
        IsCC,
        E_true_bin,
        E_true_bin_center,
        Ct_true_bin,
        Ct_true_bin_center,
        By_true_bin,
        By_true_bin_center,
        N,
        W,
        WE,
        WOther
        );
    mergedTree.Write();
  }

  // Save merged histograms
  for (TH1D* hist : hist_vec) {
    hist->Write();
  }

  // Close the output file!
  std::cout << "Outputs saved in " << mergedFile.GetName() << std::endl;
  mergedFile.Close();
  std::cout << std::endl;

  return 0;
}
