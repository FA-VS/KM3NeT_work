#ifndef __DSTTCHAINWRAPPER_INCLUDED__
#define __DSTTCHAINWRAPPER_INCLUDED__

#include "Utils/EvBins.hh" //for EvBins (duh)

#include <nlohmann/json.hpp>

#include "TFile.h"
#include "TChain.h"
#include "TTreeFormula.h"
#include "TCut.h"
#include "TString.h"
#include "TObjArray.h"
#include "TEntryList.h"

#include <string>
#include <vector>
#include <map>

using json = nlohmann::json;

// Auxiliary functions
std::string FetchBranchFormulaFromJSON(const std::string& branchname, const std::vector<json>& json_vec);

std::vector<std::string> GetTTreeNamesInTFile(TFile* f);

std::vector<TChain*> MakeTChainsFromTFiles( const std::vector<std::string>& filename_list );


// Main class

class DstTChainWrapper {
  public:
    DstTChainWrapper();
    ~DstTChainWrapper();

    void ResetReader();
    void DeleteTChains();

    void SetSource(TString source) { fSource = source; }
    void LoadTChains( const std::vector<std::string>& ifilename_list );

    int GetEntries();
    TEntryList MakeEntryList(const TCut& selection, const TString& entrylistname, const TString& entrylisttitle);
    void SetEntryList (TEntryList& EvtEntryList);

    void SetupReader(const TString& AnaClass, const std::vector<json>& json_vec);

    void GetEntry(Long64_t i);
    void ReadValues(
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
        std::map<std::string,Double_t>&  WOther //!< Other weight-like branches
        );

    std::vector<std::string> GetWOtherBranchnames() const {
      std::vector<std::string> branchnames;
      for (const auto& [name,formula] : fWOther_formula) branchnames.push_back(name);
      return branchnames;
    }

  private:
    TString fSource;
    TChain* fInputTChain;
    size_t fNFriendChains;
    std::vector<TChain*> fInputTChain_vec;
    TObjArray fNotifyGroup;
    TTreeFormula* fE_reco_formula;
    TTreeFormula* fCt_reco_formula;
    TTreeFormula* fE_true_formula;
    TTreeFormula* fCt_true_formula;
    TTreeFormula* fPdg_formula;
    TTreeFormula* fIsCC_formula;
    TTreeFormula* fBy_reco_formula;
    TTreeFormula* fBy_true_formula;
    TTreeFormula* fW_formula;
    std::map<std::string,TTreeFormula*> fWOther_formula;

};


#endif // __DSTTCHAINWRAPPER_INCLUDED__
