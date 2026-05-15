
#include "Classes/DstTChainWrapper.hh"
#include "Classes/ResponseDefaults.hh" //for defaultbranchnames, defaultbranchformulas

#include "Utils/Parser.hh" //for getValueJSON
#include "Utils/EvBins.hh" //for EvBins (duh)
#include "Utils/Utils.hh" //for file_exists, GetMatchingTTreesFromTFiles

#include "km3net-dataformat/offline/Evt.hh"
#include "km3net-dataformat/definitions/weightlist.hh" //WEIGHTLIST_DIFFERENTIAL_EVENT_RATE
#include "km3net-dataformat/definitions/w2list_gseagen.hh" //W2LIST_GSEAGEN_BY, W2LIST_GSEAGEN_CC

#include <nlohmann/json.hpp>

#include "TFile.h"
#include "TTree.h"
#include "TChain.h"
#include "TTreeFormula.h"
#include "TCut.h"
#include "TString.h"
#include "TObjArray.h"
#include "TEntryList.h"

#include <string>
#include <iostream>
#include <filesystem>
#include <vector>
#include <set>
#include <map>

using json = nlohmann::json;

// Auxiliary functions

std::string FetchBranchFormulaFromJSON(const std::string& branchname, const std::vector<json>& json_vec) {

  for (json config : json_vec) {
    if (config.contains("branch_formulas") and getValueJSON<json>(config, {"branch_formulas"}).contains(branchname)) {
      // Take user choice
      std::string branch_formula = getValueJSON<std::string>(config, {"branch_formulas", branchname});
      std::cout << "Using user's formula for " << branchname << ": " << branch_formula << std::endl;
      return branch_formula;
    }
  }
  return "";
}

std::vector<TChain*> MakeTChainsFromTFiles( const std::vector<std::string>& filename_list ){

  // Find list of TTree names in the Selector file
  // -> Raises error if a file does not exist, or if the TTrees in all TFiles don't all match
  std::vector<std::string> ttreename_vec = GetMatchingTTreesFromTFiles(filename_list);

  // Create TChain vector (one entry per TTree name present in files)
  std::vector<TChain*> tchain_vec(0);
  for (std::string ttreename : ttreename_vec ) {
    tchain_vec.push_back( new TChain( ttreename.c_str() ) );
  }

  // Connect the TChains with the list of files
  for (std::string filename : filename_list ) {
    for (size_t j=0; j < ttreename_vec.size(); j++) {
      tchain_vec[j]->Add(filename.c_str());
    }
  }

  // Make trees friends so they can be queried together
  for (size_t j=1; j < ttreename_vec.size(); j++) {
    tchain_vec[0]->AddFriend( tchain_vec[j] );
  }

  return tchain_vec;

}

// Main class

DstTChainWrapper::DstTChainWrapper()
{
  fSource = "";

  fInputTChain = new TChain();
  fInputTChain_vec = std::vector<TChain*>();
  fNFriendChains = 0;

  fNotifyGroup = TObjArray();
  fNotifyGroup.SetOwner(true);
}


DstTChainWrapper::~DstTChainWrapper()
{
  //ResetReader();
  //DeleteTChains(); // <- To avoid memory leaks
}


void DstTChainWrapper::ResetReader() {
    // Save and clean up after every class
    fInputTChain->SetEntryList(0);
    fInputTChain->SetNotify(nullptr);
    fNotifyGroup.Delete();
    fWOther_formula.clear();
}

void DstTChainWrapper::DeleteTChains() {
  // Clean-up input files
  for (size_t i=0; i <fNFriendChains; i++) 
    delete fInputTChain_vec[i]; //This includes fInputTChain ( = fInputTChain_vec[0] )
  fNFriendChains = 0;
  fInputTChain = nullptr;

}


void DstTChainWrapper::LoadTChains( const std::vector<std::string>& ifilename_list)
{
  //std::vector<std::string> ifilename_list = getValueJSON<std::vector<std::string>>(config, {input_key} );
  if ( ifilename_list.size() == 0 )
    std::cout << "WARNING: file list for source " << fSource << " is empty!!!" << std::endl;
  fInputTChain_vec = MakeTChainsFromTFiles(ifilename_list); //( std::vector<TChain*>
  fNFriendChains = fInputTChain_vec.size(); // (size_t) InputTChain_vec.size() gets messed up by the entry loop later...?
  fInputTChain = fInputTChain_vec[0]; //(TChain*) The TChain at zero is the only one friended with the rest
}


int DstTChainWrapper::GetEntries()
{
  // This must be called BEFORE SetupReader,
  // or we get a segfault.
  return fInputTChain->GetEntries();
}

TEntryList DstTChainWrapper::MakeEntryList(const TCut& selection, const TString& entrylistname, const TString& entrylisttitle)
{
  TEntryList EvtEntryList(entrylistname, entrylisttitle);
  fInputTChain->Draw(">>+"+entrylistname, selection, "entrylist");
  return EvtEntryList;
}

void DstTChainWrapper::SetEntryList(TEntryList& EvtEntryList)
{
  // This must be called BEFORE SetupReader,
  // or we get a segfault.
  fInputTChain->SetEntryList(&EvtEntryList); //Necessary for GetEntryNumber to work
}

void DstTChainWrapper::SetupReader(const TString& AnaClass, const std::vector<json>& json_vec)
{

  // Generate TTreeFormula's to fetch variables from input TTree
  TString AnaClass_lower = AnaClass;
  AnaClass_lower.ToLower();
  int index_trks = ( (AnaClass_lower == "showers" or AnaClass_lower == "s" ) ? 1 : 0 ); //Reco vars index (tracks:0, showers:1) //HACKY!! TODO: improve?

  TString E_reco_string = FetchBranchFormulaFromJSON("E_reco", json_vec);
  if (E_reco_string == "") E_reco_string = TString::Format(defaultbranchformulas["E_reco"], index_trks);
  fE_reco_formula = new TTreeFormula("E_reco_formula", E_reco_string, fInputTChain);
  //size_t count_missing_E_reco = 0; //for debugging

  TString Ct_reco_string = FetchBranchFormulaFromJSON("Ct_reco", json_vec);
  if (Ct_reco_string == "")  Ct_reco_string = TString::Format(defaultbranchformulas["Ct_reco"], index_trks);
  fCt_reco_formula = new TTreeFormula("Ct_reco_formula", Ct_reco_string, fInputTChain);
  //size_t count_missing_Ct_reco = 0; //for debugging

  TString E_true_string = FetchBranchFormulaFromJSON("E_true", json_vec);
  if (E_true_string == "") E_true_string = defaultbranchformulas["E_true"];
  fE_true_formula = new TTreeFormula("E_true_formula", E_true_string, fInputTChain);

  TString Ct_true_string = FetchBranchFormulaFromJSON("Ct_true", json_vec);
  if (Ct_true_string == "") Ct_true_string = defaultbranchformulas["Ct_true"];
  fCt_true_formula = new TTreeFormula("Ct_true_formula", Ct_true_string, fInputTChain);

  TString Pdg_string = FetchBranchFormulaFromJSON("Pdg", json_vec);
  if (Pdg_string == "") Pdg_string = defaultbranchformulas["Pdg"];
  fPdg_formula = new TTreeFormula("Pdg_formula", Pdg_string, fInputTChain);

  TString IsCC_string = FetchBranchFormulaFromJSON("IsCC", json_vec);
  if (IsCC_string == "") IsCC_string = defaultbranchformulas["IsCC"];
  fIsCC_formula = new TTreeFormula("IsCC_formula", IsCC_string, fInputTChain);

  //TString By_reco_string = FetchBranchFormulaFromJSON("By_reco", json_vec);
  //if (By_reco_string == "") By_reco_string = defaultbranchformulas["By_reco"];
  //fBy_reco_formula = new TTreeFormula("By_reco_formula", By_reco_string, fInputTChain);

  TString By_true_string = FetchBranchFormulaFromJSON("By_true", json_vec);
  if (By_true_string == "") By_true_string = defaultbranchformulas["By_true"];
  fBy_true_formula = new TTreeFormula("By_true_formula", By_true_string, fInputTChain);

  TString W_string = FetchBranchFormulaFromJSON("W_"+std::string(fSource.Data()), json_vec);
  if (W_string == "") {
    W_string = defaultbranchformulas["W_"+std::string(fSource.Data())];
  }
  fW_formula = new TTreeFormula("W_formula", W_string, fInputTChain);

  for (json config : json_vec) {
    if (config.contains("branch_formulas")) {
      json config_formulas = getValueJSON<json>(config, {"branch_formulas"});
      for ( const auto& [branchname, formula] : config_formulas.items() ) {
        if ( fWOther_formula.find(branchname) == fWOther_formula.end()
            and defaultbranchnames.find(branchname) == defaultbranchnames.end() ) {
          TString wother_string = FetchBranchFormulaFromJSON(branchname, json_vec);
          fWOther_formula[branchname] = new TTreeFormula(branchname.c_str(), wother_string, fInputTChain);
        }
      }
    }
  }

  //This part only needed for TChains, TTrees would work without this...
  fNotifyGroup.Add(fE_reco_formula);
  fNotifyGroup.Add(fCt_reco_formula);
  fNotifyGroup.Add(fE_true_formula);
  fNotifyGroup.Add(fCt_true_formula);
  fNotifyGroup.Add(fPdg_formula);
  fNotifyGroup.Add(fIsCC_formula);
  //fNotifyGroup.Add(fBy_reco_formula);
  fNotifyGroup.Add(fBy_true_formula);
  fNotifyGroup.Add(fW_formula);
  for (const auto& [name,formula] : fWOther_formula) //std::pair<std::string,TTreeFormula*> (might not be possible for it to be const...)
    fNotifyGroup.Add(formula);
  fInputTChain->SetNotify(&fNotifyGroup); //NOTE: This needs to be done AFTER we set EvtEntryList or do GetEntries part on fInputTChain, otherwise get segfault
}


void DstTChainWrapper::GetEntry(Long64_t i) {
  size_t entry = fInputTChain->GetEntryNumber(i); //EvtEntryList.GetEntry(i) would give local TTree entry instead
  fInputTChain->GetEntry(entry);
}

void DstTChainWrapper::ReadValues(
    const EvBins& evbins,
    Int_t&     E_reco_bin,           //!< true energy bin index
    Int_t&     Ct_reco_bin,          //!< true costheta bin index
    Int_t&     By_reco_bin,          //!< reco Bjorken-y bin index (NOT IMPLEMENTED)
    Int_t&     Pdg,                  //!< PDG number, (-)12, (-)14, (-)16 for (a)nu e, mu, tau·
    Bool_t&    IsCC,                 //!< is cc (0 or 1)
    Int_t&     E_true_bin,           //!< true energy bin index
    Int_t&     Ct_true_bin,          //!< true costheta bin index
    Int_t&     By_true_bin,          //!< true Bjorken-y bin index
    Double_t&  W,                    //!< weight from true bin that contribute to reco bin
    std::map<std::string,Double_t>&  WOther //!< other "weight-like" branches
    )
{

  // Reco Evt values
  if ( fE_reco_formula->GetNdata() > 0 ) {// You need to call GetNdata in case formula's path contains dynamic-sized objects
    E_reco_bin = evbins.FindBin_E_reco( fE_reco_formula->EvalInstance() );
  } else {
    E_reco_bin = -1;
    //count_missing_E_reco += 1; //debug
  }
  if ( fCt_reco_formula->GetNdata() > 0 ) {// You need to call GetNdata in case formula's path contains dynamic-sized objects
    Ct_reco_bin = evbins.FindBin_Ct_reco( fCt_reco_formula->EvalInstance() );
  } else {
    Ct_reco_bin = -1;
    //count_missing_Ct_reco += 1; //debug
  }

  // True Evt values (or fix them if N/A, so accumulation in ResponseMap works properly)
  if (fSource == "nu" or fSource == "mu") {
    if ( fE_true_formula->GetNdata() > 0 ) E_true_bin = evbins.FindBin_E_true( fE_true_formula->EvalInstance() );
    else E_true_bin = -1;
    if (fCt_true_formula->GetNdata() > 0) Ct_true_bin = evbins.FindBin_Ct_true( fCt_true_formula->EvalInstance() );
    else Ct_true_bin = -1;
    if (fPdg_formula->GetNdata() > 0 ) Pdg = int(fPdg_formula->EvalInstance());
    else Pdg = 0;
  } else {
    E_true_bin = 0;
    Ct_true_bin = 0;
    Pdg = 0;
  }
  if (fSource == "nu") {
    if (fIsCC_formula->GetNdata() > 0 ) IsCC = bool( fIsCC_formula->EvalInstance());
    else IsCC = -1;
    //if (fBy_reco_formula->GetNdata() > 0 ) By_reco_bin = evbins.FindBin_By_reco( fBy_reco_formula->EvalInstance() );
    //else By_reco_bin = -1;
    By_reco_bin = 0; // fBy_reco_bin formula not implemented yet!!!
    if (fBy_true_formula->GetNdata() > 0 ) By_true_bin = evbins.FindBin_By_true( fBy_true_formula->EvalInstance() );
    else By_true_bin = -1;
  } else {
    IsCC = false;
    By_reco_bin = 0;
    By_true_bin = 0;
  }

  // Evt weight
  if ( fW_formula->GetNdata() > 0 ) // You need to call GetNdata in case formula's path contains dynamic-sized objects
    W = fW_formula->EvalInstance();
  else
    W = 0;

  for (const auto& [name,formula] : fWOther_formula) { //std::pair<std::string,TTreeFormula*>
    if ( formula->GetNdata() > 0 ) // You need to call GetNdata in case formula's path contains dynamic-sized objects
      WOther[name] = formula->EvalInstance();
    else
      WOther[name] = 0;
  }
}



