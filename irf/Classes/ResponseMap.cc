
#include "Classes/ResponseMap.hh"
#include "Classes/ResponseDefaults.hh" //for defaultbranchnames

#include "Utils/EvBins.hh"
#include "Utils/Point.hh"
#include "Utils/WeightData.hh"

#include "TFile.h"
#include "TTree.h"
#include "TH1D.h"
#include "TString.h"
#include "TKey.h"

#include <iostream>
#include <stdexcept> //for throw, invalid_argument
#include <vector>
#include <set>
#include <map>


// Class member functions

// Constructors
ResponseMap::ResponseMap() : fEvBins(), fWeightMap() {}

ResponseMap::ResponseMap(const ResponseMap& that)
{
  this->fEvBins = that.fEvBins;
  this->fWOtherNameSet = that.fWOtherNameSet;
  this->fWeightMap = that.fWeightMap;
}

ResponseMap::ResponseMap(const EvBins& evbins) : fEvBins(evbins), fWeightMap() {}

ResponseMap::ResponseMap(TTree& ResponseTTree, const EvBins& evbins, TString source) : fEvBins(evbins), fWOtherNameSet(), fWeightMap()
{
  LoadResponseTTree(ResponseTTree, source);
}

ResponseMap::~ResponseMap()
{
  ///< Destructor
  //TODO: Verify this does not leak...? Check destructor of map, Point and WeightData...
}

// Other member functions
void ResponseMap::InsertPoint(Point point, WeightData weightdata) {
  // Adds the weight information of <point,weightdata> to the matching point in this class.
  // If Point not already in fWeightMap, insert it

  auto result_insertion = fWeightMap.insert( {point, weightdata} );  //std::pair< map<>::iterator, bool >

  if ( result_insertion.second == false ) { //Point was already present, add its weight
     auto mapiter = result_insertion.first; //iterator at existing point
     WeightData& weights = mapiter->second;
     //weights.N += weighttrio.N;
     //weights.W += weighttrio.W;
     //weights.WE += weighttrio.WE;
     weights += weightdata;
  }
}


void ResponseMap::MergeResponseMap(const ResponseMap& detresponse){
  // Merges the points and weight information from another ResponseMap into this one.

  if (fEvBins == detresponse.fEvBins) {
    for (const auto& [point, weightdata] : detresponse.fWeightMap) { //std::pair<Point,WeightData> ?
      InsertPoint(point, weightdata);
    }
  } else {
    throw std::runtime_error("ERROR: EvBins of the two ResponseMap don't match! Aborting the merge"); //TODO: would a different exception match better?
  }

}

void ResponseMap::LoadResponseTTree(TTree& ResponseTTree, TString source, const EvBins& evbins){
  // Reads a ResponseTTree, and updates the internal weightmap of this class accordingly.
  // Can be done with multiple ResponseTTrees, one after another, to effectively merge them.
  // Checks first that the EvBins stored in this class match the input evbins.
  // (likely generated from MakeEvBinsFromResponseCreatorTFile on the file ResponseTTree comes from)

  if (fEvBins == evbins) {
    LoadResponseTTree(ResponseTTree, source);
  } else {
    throw std::runtime_error("ERROR: Provided EvBins doesn't match existing one! Aborting laoding of the TTree"); //TODO: would a different exception match better?
  }

}
void ResponseMap::LoadResponseTTree(TTree& ResponseTTree, TString source){

  // Reads a ResponseTTree, and updates the internal weightmap of this class accordingly.
  // Can be done with multiple ResponseTTrees, one after another, to effectively merge them.

  // Set up branch readers here
  Int_t     E_reco_bin;           //!< true energy bin index
  Int_t     Ct_reco_bin;          //!< true costheta bin index
  Int_t     By_reco_bin;          //!< reco Bjorken-y bin index (NOT IMPLEMENTED)
  Int_t     Pdg;                  //!< PDG number, (-)12, (-)14, (-)16 for (a)nu e, mu, tau 
  Bool_t    IsCC;                 //!< is cc (0 or 1)
  Int_t     E_true_bin;           //!< true energy bin index
  Int_t     Ct_true_bin;          //!< true costheta bin index
  Int_t     By_true_bin;          //!< true Bjorken-y bin index
  Int_t  N;                       //!< number of (unmerged) entries aggregated into this one
  Double_t  W;                    //!< weight from true bin that contribute to reco bin
  Double_t  WE;                   //!< MC statistical error of the weight
  std::map<std::string,double> WOther; // Other linear weight-related variables

  ResponseTTree.SetBranchAddress("E_reco_bin",  &E_reco_bin);
  ResponseTTree.SetBranchAddress("Ct_reco_bin", &Ct_reco_bin);
  if (source == "nu" or source == "mu") {
    ResponseTTree.SetBranchAddress("E_true_bin",  &E_true_bin);
    ResponseTTree.SetBranchAddress("Ct_true_bin", &Ct_true_bin);
    ResponseTTree.SetBranchAddress("Pdg",         &Pdg);
  } else {
    E_true_bin = 0;
    Ct_true_bin = 0;
    Pdg = 0;
  }
  if (source == "nu") {
    ResponseTTree.SetBranchAddress("IsCC",        &IsCC);
    By_reco_bin = 0; // By_reco_bin formula not implemented yet!!!
    ResponseTTree.SetBranchAddress("By_true_bin", &By_true_bin);
  }
  else {
    IsCC = false;
    By_reco_bin = 0;
    By_true_bin = 0;
  }
  ResponseTTree.SetBranchAddress("N",   &N);
  ResponseTTree.SetBranchAddress("W",   &W);
  ResponseTTree.SetBranchAddress("WE",  &WE);

  std::set<std::string> branchnames_wother_new;
  TObjArray* branches = ResponseTTree.GetListOfBranches();
  for (const auto& branch : *branches ) { // Hacky, branch is a TObject here
    std::string branchname = branch->GetName();
    if ( defaultbranchnames.find(branchname) == defaultbranchnames.end() ) {
      branchnames_wother_new.insert(branchname);
      WOther[branchname] = 0;
      ResponseTTree.SetBranchAddress(branchname.c_str(),  &WOther[branchname]);
    }
  }
  // Check new WOther is consistent with pre-existing one
  if (fWOtherNameSet.size() == 0) {
    fWOtherNameSet == branchnames_wother_new;
  } else if (branchnames_wother_new != fWOtherNameSet ) {
    std::cout << "Previous WOther names:" << std::endl;
    for (const std::string& branchname : fWOtherNameSet) { std::cout << "  " << branchname << std::endl; }
    std::cout << "New WOther names:" << std::endl;
    for (const std::string& branchname : branchnames_wother_new) { std::cout << "  " << branchname << std::endl; }
    throw std::runtime_error("ERROR: WOther of the two ResponseMap don't match! Aborting the merge"); //TODO: would a different exception match better?
  }

  // Loop over TTree entries
  size_t nentries = ResponseTTree.GetEntries();
  for (size_t i=0; i<nentries; i++) {
    ResponseTTree.GetEntry(i); // Somewhat annoyingly, this prevents ResponseTTree from being passed as a const TTree& (instead of just TTree&)
    Point point = {E_reco_bin, Ct_reco_bin, By_reco_bin, E_true_bin, Ct_true_bin, By_true_bin, Pdg, IsCC};
    WeightData weightdata = {N, W, WE, WOther};
    InsertPoint(point, weightdata);
  }

}


void ResponseMap::FillResponseTTree(
    // Can these references be managed more easily with an RDataFrame?
    TTree&    ttree,
    Int_t&    E_reco_bin,
    Double_t& E_reco_bin_center,
    Int_t&    Ct_reco_bin,
    Double_t& Ct_reco_bin_center,
    //Int_t&    By_reco_bin,
    //Double_t& By_reco_bin_center,
    Int_t&    Pdg,
    Bool_t&   IsCC,
    Int_t&    E_true_bin,
    Double_t& E_true_bin_center,
    Int_t&    Ct_true_bin,
    Double_t& Ct_true_bin_center,
    Int_t&    By_true_bin,
    Double_t& By_true_bin_center,
    Int_t& N,
    Double_t& W,
    Double_t& WE,
    std::map<std::string,double>& WOther
    ) const {

  // Takes a reference to a ResponseTTree and all of its branch "fillers", 
  // and fills the TTree with the contents of the internal weightmap.
  // This will *not* work properly if the TTree already contained entries!!

  for (const auto& [point, weightdata] : fWeightMap) { //std::pair<Point,WeightData> ?
    E_reco_bin = point.E_reco_bin;
    Ct_reco_bin = point.Ct_reco_bin;
    E_true_bin = point.E_true_bin;
    Ct_true_bin = point.Ct_true_bin;
    //By_reco_bin = point.By_reco_bin;
    By_true_bin = point.By_true_bin;
    E_reco_bin_center = fEvBins.BinCenter_E_reco(E_reco_bin);
    Ct_reco_bin_center = fEvBins.BinCenter_Ct_reco(Ct_reco_bin);
    E_true_bin_center = fEvBins.BinCenter_E_true(E_true_bin);
    Ct_true_bin_center = fEvBins.BinCenter_Ct_true(Ct_true_bin);
    //By_reco_bin_center = fEvBins.BinCenter_By_reco(By_reco_bin);
    By_true_bin_center = fEvBins.BinCenter_By_true(By_true_bin);
    Pdg = point.Pdg;
    IsCC = point.IsCC;

    // TODO: Could the following just use a single WeightData instead?
    N = weightdata.N;
    W = weightdata.W;
    WE = weightdata.WE;
    for (const auto& [name, value]: weightdata.Other) { //This way (instead of just WOther = weightdata?) preserves references of WOther, hopefully
      WOther[name] = value;
    }

    ttree.Fill();
  }

}



// Auxiliary functions

std::vector<double> GetVarBinsFromResponseCreatorTFile(TFile& fileResponse, TString source, TString AnaClass, TString varname)
{
  // Generates a vector with the bin edges of one branch, based on the matching histogram in a ResponseTTreeCreator file.

  std::vector<double> bin_edges = {};
  TString histname = "ResponseTTree_" + source + "_" + AnaClass + "_" + varname;
  TH1D* hist = fileResponse.Get<TH1D>( histname.Data() ); // "Get" prevents fileResponse from being const TFile& (instead of just TFile&)
  if ( hist ) { // Histogram was found (otherwise hist = nullptr)
    const TArrayD* bin_edges_array = hist->GetXaxis()->GetXbins();
    for (int i = 0; i < bin_edges_array->GetSize(); i++) {
      bin_edges.push_back( bin_edges_array->GetAt(i) );
    }
    delete hist; // This also deletes bin_edges_array
  } else {
    std::cout << "WARNING: Response Histogram " << histname << " not present in " << fileResponse.GetName() << std::endl;
  }
  return bin_edges;
}

EvBins MakeEvBinsFromResponseCreatorTFile(TFile& fileResponse, TString source, TString AnaClass)
{
  // Generates EvBins based on the per-branch histograms in a ResponseTTreeCreator file.

  EvBins evbins;
  evbins.E_reco_bins = GetVarBinsFromResponseCreatorTFile( fileResponse, source, AnaClass, "E_reco" );
  evbins.Ct_reco_bins = GetVarBinsFromResponseCreatorTFile( fileResponse, source, AnaClass, "Ct_reco" );
  if (source == "nu" or source == "mu") {
    evbins.E_true_bins = GetVarBinsFromResponseCreatorTFile( fileResponse, source, AnaClass, "E_true" );
    evbins.Ct_true_bins = GetVarBinsFromResponseCreatorTFile( fileResponse, source, AnaClass, "Ct_true" );
  }
  if (source == "nu") {
    //evbins.By_reco_bins = GetVarBinsFromResponseCreatorTFile( fileResponse, source, AnaClass, "By_reco" );
    evbins.By_true_bins = GetVarBinsFromResponseCreatorTFile( fileResponse, source, AnaClass, "By_true" );
  }
  evbins.CheckValidity(source.Data());
  return evbins;
}
