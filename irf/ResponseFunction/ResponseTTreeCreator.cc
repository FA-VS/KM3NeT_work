#include "ResponseTTreeCreator.hh"

#include "Classes/DstTChainWrapper.hh" //for DstTChainWrapper
#include "Classes/ResponseMap.hh" //for ResponseMap
#include "Utils/Parser.hh" //for ReadJSON, getValueJSON
#include "Utils/EvBins.hh" //for EvBins (duh)
#include "Utils/Point.hh" //for Point, WeightTrio
#include "Utils/Utils.hh" //for file_exists

#include <nlohmann/json.hpp>

#include "TFile.h"
#include "TTree.h"
#include "TTreeFormula.h"
#include "TH1D.h"
#include "TCut.h"
#include "TString.h"
#include "TEntryList.h"

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <getopt.h>  //command line parsing

using json = nlohmann::json;


void ProcessSource(json config, std::string source, TFile* outputFile){

  if ( source != "data" and source != "nu" and source != "mu" and source != "noise" ) {
    std::cout << "Do not recognize source argument for processCut, must be one of 'data', 'nu', 'mu', 'noise'" << std::endl;
    return;
  }

  // Load input TTrees as friended TChains
  std::string input_key = "input_" + source;
  if ( not config.contains(input_key) ) {
    std::cout << "Requested source " << source << " does not have a matching input line in input json" << std::endl;
    std::cout << "Skipping processing" << std::endl;
    return;
  }
  std::vector<std::string> ifilename_list = getValueJSON<std::vector<std::string>>(config, {input_key} );
  DstTChainWrapper dstTChainWrapper;
  dstTChainWrapper.SetSource(source);
  dstTChainWrapper.LoadTChains(ifilename_list);

  bool aggregate_response_bins = true;
  if ( config.contains("aggregate_response_bins") )
    aggregate_response_bins = getValueJSON<bool>(config, {"aggregate_response_bins"} );
  bool keep_under_over_flow = false;
  if ( config.contains("keep_under_over_flow") )
    keep_under_over_flow = getValueJSON<bool>(config, {"keep_under_over_flow"} );


  // Variables to store in the TTree results
  Int_t     E_reco_bin;           //!< reco energy bin index
  Double_t  E_reco_bin_center;    //!< center of the reco energy bin
  Int_t     Ct_reco_bin;          //!< reco costheta bin index
  Double_t  Ct_reco_bin_center;   //!< center of the reco costheta bin
  Int_t     By_reco_bin;          //!< reco Bjorken-y bin index (NOT IMPLEMENTED)
  //Double_t  By_reco_bin_center;   //!< center of the reco Bjorken-y bin (NOT IMPLEMENTED)
  Int_t     Pdg;                  //!< PDG number, (-)12, (-)14, (-)16 for (a)nu e, mu, tau 
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


  // Set working directory to output TFile
  outputFile->cd();

  // Loop over cuts / classes
  json classes = getValueJSON<json>(config, {"classes"});
  TCut defaultFixedSelection;
  if ( config.contains("cuts") ) {
    json default_cuts = getValueJSON<json>(config, {"cuts"});
    if ( default_cuts.contains("fixedcut") ) {
      defaultFixedSelection = getValueJSON<std::string>(default_cuts, {"fixedcut"}).c_str();
    }
  }
  for (const auto& classes_item : classes.items()) { // auto -> pseudopair<string,json>
    TString AnaClass = classes_item.key();
    json sel_class = classes_item.value();
    std::cout << std::endl;
    // Load up the bin definitions
    EvBins evbins(config);
    evbins.LoadJSON(sel_class); // This updates with class-specific bins
    evbins.CheckValidity();

    // Create entry list with selected entries
    json class_cuts = getValueJSON<json>(sel_class, {"cuts"});
    TCut fixedSelection = defaultFixedSelection; //applies both to "true" and "loose" cut
    if ( class_cuts.contains("fixedcut") )
      fixedSelection = getValueJSON<std::string>(class_cuts, {"fixedcut"}).c_str();

    TCut netSelection; //Cut used to get distribution
    bool rescaled_loose_cut = false;
    std::string loosecut_key = "loose_" + source;
    if ( class_cuts.contains(loosecut_key) ) {
      netSelection = getValueJSON<std::string>(class_cuts, {loosecut_key}).c_str();
      rescaled_loose_cut = true;
    } else
      netSelection = getValueJSON<std::string>(class_cuts, {"analysis"}).c_str();
    netSelection = netSelection && fixedSelection;

    std::cout << "Applying cut for class " << AnaClass << "..." << std::endl;
    std::cout << "Cut: " << netSelection << std::endl;
    int allEntries = dstTChainWrapper.GetEntries();
    TEntryList EvtEntryList = dstTChainWrapper.MakeEntryList(netSelection, "EvtEntryList", AnaClass);
    int nentries = EvtEntryList.GetN();
    std::cout << "Number of entries for class " << AnaClass << ": "
      << nentries << " / " << allEntries << std::endl;

    // If using looser cut for increased statistics, compute rescaling factor
    double rescaling;
    if ( rescaled_loose_cut ) {
      TCut netSelection_true = getValueJSON<std::string>(class_cuts, {"analysis"}).c_str();
      netSelection_true = netSelection_true && fixedSelection;
      if (nentries == 0)
        rescaling = 0;
      else {
        std::cout << std::endl;
        std::cout << "  Applying \"true\" cut for class " << AnaClass << "..." << std::endl;
        std::cout << "  Cut: " << netSelection_true << std::endl;
        TEntryList EvtEntryList_true = dstTChainWrapper.MakeEntryList( netSelection_true, "EvtEntryList_true", AnaClass);
        rescaling = double(EvtEntryList_true.GetN()) / nentries;
        std::cout << "  \"True\" number of entries is " << EvtEntryList_true.GetN() << std::endl;
        std::cout << "  Rescaling loose cut results by factor " << rescaling << " to compensate." << std::endl;
        EvtEntryList_true.Delete(); //Not sure if necessary
      }
    } else {
      rescaling = 1;
    }

    // Generate TTreeFormula's to fetch variables from input TTree
    dstTChainWrapper.SetupReader(AnaClass, {sel_class, config}); //Sets up formulae to read E_reco, Ct_reco, etc.

    // Initialize WOther (needs to happen after SetupReader)
    std::cout << "Extra branches in output IRF file:" << std::endl;
    WOther.clear();
    for (const std::string& branchname : dstTChainWrapper.GetWOtherBranchnames() ) {
      std::cout << "  " << branchname << std::endl;
      WOther[branchname] = 0;
    }

    // Prepare accumulator for weights before filling TTree
    ResponseMap responsemap(evbins);

    //Create one output TTree per class
    TString flattreename = "ResponseTTree_" + source + "_" + AnaClass;
    TTree flatTree(flattreename, "TTree with binned E and cosT");
    flatTree.Branch("E_reco_bin",         &E_reco_bin,         "Reconstructed energy bin index/I");
    flatTree.Branch("E_reco_bin_center",  &E_reco_bin_center,  "Reconstructed energy bin center/D");
    flatTree.Branch("Ct_reco_bin",        &Ct_reco_bin,        "Reconstructed cosine of the zenith angle bin index/I");
    flatTree.Branch("Ct_reco_bin_center", &Ct_reco_bin_center, "Reconstructed cosine of the zenith angle bin center/D");
    if (source == "nu" or source == "mu") {
      flatTree.Branch("E_true_bin",         &E_true_bin,         "True energy bin index/I");
      flatTree.Branch("E_true_bin_center",  &E_true_bin_center,  "True energy bin center/D");
      flatTree.Branch("Ct_true_bin",        &Ct_true_bin,        "True cosine of the zenith angle bin index/I");
      flatTree.Branch("Ct_true_bin_center", &Ct_true_bin_center, "True cosine of the zenith angle bin center/D");
      flatTree.Branch("Pdg",                &Pdg,                "Pdg number nue - 12, numu - 14, nutau - 16 (negative for antineutrinos)/I");
    }
    if (source == "nu") {
      flatTree.Branch("IsCC",               &IsCC,               "Charged current flag NC -0 CC - 1/O");
      //flatTree.Branch("By_reco_bin",        &By_reco_bin,        "Reconstructed Bjorken-y bin index/I");
      //flatTree.Branch("By_reco_bin_center", &By_reco_bin_center, "Reconstructed Bjorken-y bin center/D");
      flatTree.Branch("By_true_bin",        &By_true_bin,        "True Bjorken-y bin index/I");
      flatTree.Branch("By_true_bin_center", &By_true_bin_center, "True Bjorken-y bin center/D");
    }
    flatTree.Branch("N",                  &N,                  "Number of (unmerged) entries aggregated into this entry/I");
    flatTree.Branch("W",                  &W,                  "Neutrino weight per unit flux (GeV m^2 sr)/D");
    flatTree.Branch("WE",                 &WE,                 "Variance of the neutrino weight (GeV m^2 sr)^2/D");
    for (const auto& [name,value] : WOther) {
      //flatTree.Branch(name.c_str(), &value, (name+"/D").c_str()); //This fails because of typing issues with value ( std::tuple_element<1, const std::pair<const std::__cxx11::basic_string<char>, double> >::type* )...
      flatTree.Branch(name.c_str(), &WOther[name], (name+"/D").c_str());
    }

    // Loop over the selected entries
    dstTChainWrapper.SetEntryList(EvtEntryList);
    std::cout << "Counting" << std::endl;
    for (Long64_t i = 0; i < nentries; i++) {
      if (i % 50000 == 0)
        std::cout << "Done " << i << "/" << nentries << std::endl;

      dstTChainWrapper.GetEntry(i);
      // Update the output tree variables with the values at the current DST entry,
      // based on the binning in evbins.
      dstTChainWrapper.ReadValues( 
          evbins,
          E_reco_bin,
          Ct_reco_bin,
          By_reco_bin,
          Pdg,
          IsCC,
          E_true_bin,
          Ct_true_bin,
          By_true_bin,
          W,
          WOther
          );
      N = 1;
      W *= rescaling;
      WE = W*W;

      // Skip saving the values if they are in under/over flow for *any* branch
      if (not keep_under_over_flow) {
        if (
            E_reco_bin < 0
            or E_reco_bin >= evbins.E_reco_nbins()
            or Ct_reco_bin < 0
            or Ct_reco_bin >= evbins.Ct_reco_nbins()
            or E_true_bin < 0
            or E_true_bin >= evbins.E_true_nbins()
            or Ct_true_bin < 0
            or Ct_true_bin >= evbins.Ct_true_nbins()
            //or By_reco_bin < 0
            //or By_reco_bin >= evbins.By_reco_nbins()
            or By_true_bin < 0
            or By_true_bin >= evbins.By_true_nbins()
           )
          continue;
      }

      // "Save" entry
      if (aggregate_response_bins) {
        Point point = {E_reco_bin, Ct_reco_bin, By_reco_bin, E_true_bin, Ct_true_bin, By_true_bin, Pdg, IsCC};
        responsemap.InsertPoint(point, {N,W,WE,WOther} );
      } else {
        E_reco_bin_center = evbins.BinCenter_E_reco(E_reco_bin);
        Ct_reco_bin_center = evbins.BinCenter_Ct_reco(Ct_reco_bin);
        E_true_bin_center = evbins.BinCenter_E_true(E_true_bin);
        Ct_true_bin_center = evbins.BinCenter_Ct_true(Ct_true_bin);
        //By_reco_bin_center = evbins.BinCenter_By_reco(By_reco_bin);
        By_true_bin_center = evbins.BinCenter_By_true(By_true_bin);
        flatTree.Fill();
      }

    } // End of loop over entries (for a given class)

    // If aggregate TTree requested, fill flatTree after the loop
    if (aggregate_response_bins) {
      std::cout << nentries << " entries aggregated into " << responsemap.GetWeightMap().size() << " 'bins'" << std::endl;
      responsemap.FillResponseTTree(
          flatTree,
          E_reco_bin,
          E_reco_bin_center,
          Ct_reco_bin,
          Ct_reco_bin_center,
          Pdg,
          IsCC,
          E_true_bin,
          E_true_bin_center,
          Ct_true_bin,
          Ct_true_bin_center,
          //By_reco_bin,
          //By_reco_bin_center,
          By_true_bin,
          By_true_bin_center,
          N,
          W,
          WE,
          WOther
          );
    }

    // Make quick histograms for cross-checks
    TH1D hE_reco("hE_reco", "E reco; Reconstructed energy [GeV]; Neutrino weight per unit flux [GeV.m^{2}.sr]", evbins.E_reco_nbins(), &evbins.E_reco_bins[0]);
    flatTree.Draw("E_reco_bin_center >> hE_reco", "W", "goff");
    hE_reco.SetName( flattreename+"_E_reco" );
    hE_reco.Write();
    TH1D hCt_reco( "hCt_reco", "Ct reco; Reconstructed cosine of the zenith angle; Neutrino weight per unit flux [GeV.m^{2}.sr]", evbins.Ct_reco_nbins(), &evbins.Ct_reco_bins[0]);
    flatTree.Draw("Ct_reco_bin_center >> hCt_reco", "W", "goff");
    hCt_reco.SetName( flattreename+"_Ct_reco" );
    hCt_reco.Write();
    if (source == "nu" or source == "mu") {
      TH1D hE_true("hE_true", "E true; True energy [GeV]; Neutrino weight per unit flux [GeV.m^{2}.sr]", evbins.E_true_nbins(), &evbins.E_true_bins[0]);
      flatTree.Draw("E_true_bin_center >> hE_true", "W", "goff");
      hE_true.SetName( flattreename+"_E_true" );
      hE_true.Write();
      TH1D hCt_true("hCt_true", "Ct true; True cosine of the zenith angle; Neutrino weight per unit flux [GeV.m^{2}.sr]", evbins.Ct_true_nbins(), &evbins.Ct_true_bins[0]);
      flatTree.Draw("Ct_true_bin_center >> hCt_true", "W", "goff");
      hCt_true.SetName( flattreename+"_Ct_true" );
      hCt_true.Write();
    }
    if (source == "nu") {
      //TH1D hBy_reco("hBy_reco", "By reco; Reconstructed Bjorken-y; Neutrino weight per unit flux [GeV.m^{2}.sr]", evbins.By_reco_nbins(), &evbins.By_reco_bins[0]);
      //flatTree.Draw("By_reco_bin_center >> hBy_reco", "W", "goff");
      //hBy_reco.SetName( flattreename+"_By_reco" );
      //hBy_reco.Write();
      TH1D hBy_true("hBy_true", "By true; True Bjorken-y; Neutrino weight per unit flux [GeV.m^{2}.sr]", evbins.By_true_nbins(), &evbins.By_true_bins[0]);
      flatTree.Draw("By_true_bin_center >> hBy_true", "W", "goff");
      hBy_true.SetName( flattreename+"_By_true" );
      hBy_true.Write();
    }

    // Save and clean up after every class
    dstTChainWrapper.ResetReader();
    flatTree.Write();
    std::cout << "Processed class: " << AnaClass << std::endl;
    std::cout << std::endl;
  } // End of loop over classes

  // Clean-up input files
  dstTChainWrapper.DeleteTChains();

  std::cout << "Outputs saved in " << outputFile->GetName() << std::endl;

}


std::string removeSpaces(std::string str) {
    str.erase(std::remove(str.begin(), str.end(), ' '), str.end()); 
    return str; 
}

// From string to vector of strings
std::vector<std::string> stov(std::string str) {
  std::string str_vec = str;
  str_vec.erase(0, str_vec.find_first_not_of(" \t\n\r\f\v")); //remove leading whitespace
  str_vec.erase(str_vec.find_last_not_of(" \t\n\r\f\v") + 1); //remove trailing whitespace
  if (
      (str_vec.front() == '[' and str_vec.back() == ']') or
      (str_vec.front() == '(' and str_vec.back() == ')')
      )
    str_vec = str.substr(1,str.size()-2); //remove starting/ending brackets
  else {
    std::cout << "WARNING: Turning " << str << " into a vector of strings," << std::endl;
    std::cout << "  but it does not start and end with [] or ()." << std::endl;
  }
  std::stringstream ss(str_vec);
  std::vector<std::string> res;
  std::string token;
  while (getline(ss, token, ',')) {
    res.push_back(token);
  }
  return res;
}

// From string to vector of doubles
std::vector<double> stovd(std::string str) {
  std::vector<std::string> stov_str = stov(str);
  std::vector<double> res;
  for (size_t i=0; i<stov_str.size(); i++)
    res.push_back(stod(stov_str[i]));
  return res;
}


int main(int argc, char* argv[]){

  using namespace std;

  cout << endl;

  // Parsing CLI inputs
  json config;
  vector<string> source_vec = {"data", "nu", "mu", "noise"};
  string insertcouple, ptr_string, value;
  size_t pos;
  json json_binning;
  vector<double> vec_values;
  bool modified_json = false;
  int opt;
  // TODO: Improve parsed options: add debug, add N entries to process, add time-computation if needed
  // TODO: add doxygen or something like that, options are not very clear
  while ( (opt = getopt(argc, argv, "j:o:s:b:B:i:I:h")) != EOF) {
    switch (opt) {
      case 'j':
        if (config.is_null())
          config = readJSON(optarg);
        else {
          config.update(readJSON(optarg), true);
          modified_json = true;
        }
        break;
      case 'o':
        config["output"] = optarg; //double-check syntax here
        modified_json = true;
        break;
      case 'b':
      case 'B':
        // Modify bins of a "json pointer" such as `-b "/binning/Ct_true=(nbins,min,max)"`
        // Use -b for (nbins,min,max), or -B for custom [bin_edge1, bin_edge2,... bin_edgeN]
        insertcouple = optarg;
        removeSpaces(insertcouple);
        pos = insertcouple.find('=');
        ptr_string = insertcouple.substr(0, pos);
        if (ptr_string[0] != '/')
          ptr_string = "/" + ptr_string;
        value = insertcouple.substr(pos+1);
        vec_values = stovd(value);
        if (opt == 'b') {
          json_binning = { {"custom",false}, {"nbins",int(vec_values[0])}, {"min",vec_values[1]}, {"max",vec_values[2]} };
        } else if (opt == 'B') {
          json_binning = { {"custom",true}, {"custom_edges",vec_values} };
        }
        config[json::json_pointer(ptr_string)] = json_binning; // This will create intermediate objects automatically
        modified_json = true;
        break;
      case 'i':
      case 'I':
        // insert a "json pointed" value such as `-i "/branch_formulas/E_true=Evt->mc_trks[0].E"`
        // Use -i if the value is a single string, or -I for a vector of strings!!!
        // Note that spaces are removed from the argument!!!
        insertcouple = optarg;
        removeSpaces(insertcouple);
        pos = insertcouple.find('=');
        ptr_string = insertcouple.substr(0, pos);
        if (ptr_string[0] != '/')
          ptr_string = "/" + ptr_string;
        value = insertcouple.substr(pos+1);
        if (opt == 'i') {
          config[json::json_pointer(ptr_string)] = value; // This will create intermediate objects automatically
        } else if (opt == 'I') {
          config[json::json_pointer(ptr_string)] = stov(value); // This will create intermediate objects automatically
        }
        modified_json = true;
        break;
      case 's':
        source_vec = {optarg};
        //modified_json = true;
        break;
      case '?':
      case 'h':
      default :
        cout << "Usage: ResponseTTreeCreator [-j input.json] [-o outputpath] [-s source] \
          [-b \"pointer=(nbins,min,max)\"] [-B \"pointer=[bin_edge1,bin_edge2,bin_edge3...]\"] \
          [-i \"pointer=string\"] [-I \"pointer=[string1,string2,string3...]\"]" << endl;
        return 1;
    }
  }


  // Create output TFile
  string outputfilepath  = getValueJSON<string>(config, {"output"});
  if (file_exists(outputfilepath)) {
    cout << "WARNING: Output file already exists: " << outputfilepath << endl;
    cout << "Proceed? [Y/n]: ";
    string userinput;
    cin >> userinput;
    if (userinput[0] != 'Y') {
      cout << "Aborting..." << endl;
      return 1;
    } else {
      cout << "Overwriting..." << endl;
    }
  }
  if (modified_json) {
    // Save new values in a new json
    string outputjsonfilepath = outputfilepath;
    size_t pos = outputjsonfilepath.rfind(".root");
    outputjsonfilepath = outputjsonfilepath.substr(0, pos) + ".json";
    cout << "Input JSON was modified, saving new values in " << outputjsonfilepath << endl;
    ofstream ojson(outputjsonfilepath);
    ojson << setw(2) << config << endl;
  }
  TFile* newFile = TFile::Open(outputfilepath.c_str(), "RECREATE");

  std::filesystem::path outputfilepath_fs(outputfilepath);
  string config_dump_name = outputfilepath_fs.stem().string() + "_InputJSON";
  TNamed config_dump(config_dump_name,config.dump(2)); //Alternative with TString has max length of 10260?
  config_dump.Write();

  // Loop over sources
  for (string source : source_vec) {
    string input_key = "input_" + source;
    if ( config.contains(input_key) and getValueJSON<vector<string>>(config, {input_key} ).size()>0 ) {
      cout << " STARTING WITH SOURCE " << source << " !!!" << endl;
      ProcessSource(config, source, newFile); //where the actual work happens
      cout << " DONE WITH SOURCE " << source << " !!!" << endl;
      cout << endl;
    }
  }

  newFile->Close();
  return 0;
}


/*************
 * VERSION WITH SPARSE HISTOGRAM
 * This approach would skip the need to manually bin and accumulate the values
 *
 * //Create
 * const Int_t ndim = 6;
 * std::vector<TAxis*> axes;
 * axes.push_back( new TAxis(evbins.E_reco_bins) ); axes.back().SetTitle("EbinsReco");
 * axes.push_back( new TAxis(evbins.Ct_reco_bins) );
 * axes.push_back( new TAxis(evbins.E_true_bins) );
 * axes.push_back( new TAxis(evbins.Ct_true_bins) );
 * axes.push_back( new TAxis({20000001,-10000000.5,10000000.5}) ); //PDG numbering scheme
 * axes.push_back( new TAxis({2,-0.5,1.5}) ); //IsCC
 * THnSparseD* hsparse = new THnSparseD("hsparse", "title", axes); // This convenient constructor only in ROOT 6.36+ 
 * //Alternatively, axes can be redefined after declaration of THnSparseD:
 * Int_t nbins[ndim] = {nEbinsReco, ncosTbinsReco, nEbinsTrue, ncosTbinsTrue, 20000001, 2};
 * Double_t xmins[ndim] = {EminReco, cosTminReco, EminTrue, cosTminTrue, -10000000.5, -0.5};
 * Double_t xmaxs[ndim] = {EmaxReco, cosTmaxReco, EmaxTrue, cosTmaxTrue, +10000000.5, +1.5};
 * THnSparseD* hsparse = new THnSparseD("hsparse", "title", ndim, nbins, xmins, xmaxs));
 * hsparse->GetAxis(0)->Set( evbins.E_reco_nbins(), &evbins.E_reco_bins[0] ); //etc
 *
 * // Fill (make loop)
 * hsparse->Sumw2();
 * Double_t point[ndim];
 * weight = ???
 * hsparse->Fill(point, weight); //repeat this as needed
 *
 * // Recover
 * Int_t coord[ndim];
 * for (int i = 0; i < hsparse->GetNbins(); i++) {
 *   hsparse->GetBinContent(i, coord); //Fills out coord with bin numbers
 *   E_reco_bin = coord[0] - 1; // -1 to account for underflow bin
 *   E_reco_bin_center = evbins.BinCenter_E_reco(E_reco_bin);
 *   Ct_reco_bin = coord[1] - 1;
 *   Ct_reco_bin_center = evbins.BinCenter_Ct_reco(Ct_reco_bin);
 *   E_true_bin = coord[2] - 1;
 *   E_true_bin_center = evbins.BinCenter_E_true(E_true_bin);
 *   Ct_true_bin = coord[3] - 1;
 *   Ct_true_bin_center = evbins.BinCenter_Ct_true(Ct_true_bin);
 *   pdg = hsparse->GetAxis(4)->GetBinCenter( coord[4] );
 *   IsCC = bool(coord[5]-1);
 *   //do sth here with the loaded values
 * }

*************/
