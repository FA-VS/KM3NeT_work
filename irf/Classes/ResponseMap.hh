#ifndef __RESPONSEMAP_INCLUDED__
#define __RESPONSEMAP_INCLUDED__

#include "Utils/EvBins.hh"
#include "Utils/Point.hh"
#include "Utils/WeightData.hh"

#include "TFile.h"
#include "TTree.h"
#include "TString.h"

#include <set>
#include <vector>
#include <map>
#include <string>



// Auxiliary functions
std::vector<double> GetVarBinsFromResponseCreatorTFile(TFile& fileResponse, TString source, TString AnaClass, TString varname);

EvBins MakeEvBinsFromResponseCreatorTFile(TFile& fileResponse, TString source, TString AnaClass);


// Main class
class ResponseMap {
  public:
    ResponseMap();                                                            ///< Default constructor
    ResponseMap(const ResponseMap& that);                                     ///< Copy constructor
    ResponseMap(const EvBins& evbins);                                        ///< Copy constructor
    ResponseMap(TTree& ResponseTTree, const EvBins& evbins, TString source);  ///< Constructor from ResponseCreator TTree
    ~ResponseMap();                                                           ///< Destructor

    //Member functions
    void InsertPoint(Point point, WeightData weightdata);
    void MergeResponseMap(const ResponseMap& detresponse);
    void LoadResponseTTree(TTree& ResponseTTree, TString source, const EvBins& evbins);
    void LoadResponseTTree(TTree& ResponseTTree, TString source);

    void FillResponseTTree (
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
        ) const;

    EvBins GetEvBins() const { return fEvBins; }
    void SetEvBins(const EvBins& evbins) { fEvBins = evbins; }
    std::set<std::string> GetWOtherNameSet() const { return fWOtherNameSet; }
    void SetWOtherNameSet(const std::set<std::string>& wothernameset) { fWOtherNameSet = wothernameset; }
    std::map<Point, WeightData> GetWeightMap() const { return fWeightMap; }
    void SetWeightMap(const std::map<Point, WeightData>& weightmap) { fWeightMap = weightmap; }

  private:
    EvBins fEvBins;
    std::set< std::string > fWOtherNameSet;
    std::map< Point, WeightData > fWeightMap;
};

#endif // __RESPONSEMAP_INCLUDED__
