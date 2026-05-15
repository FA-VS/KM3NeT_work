
#include "km3net-dataformat/definitions/weightlist.hh" //WEIGHTLIST_DIFFERENTIAL_EVENT_RATE
#include "km3net-dataformat/definitions/w2list_gseagen.hh" //W2LIST_GSEAGEN_BY, W2LIST_GSEAGEN_CC

#include "TString.h"

#include <map>
#include <set>
#include <string>

// Default branch formulas, if not provided in input json
std::map<std::string, TString> defaultbranchformulas = {
  {"E_reco" , "Evt->trks[%d].E"}, //Note that it needs to be formatted with an index before use, i.e. TString::Format( s, index_trks )
  {"Ct_reco", "-Evt->trks[%d].dir.z"}, //Note that it needs to be formatted with an index before use, i.e. TString::Format (s, index_trks )
  {"E_true" , "Evt->mc_trks[0].E"},
  {"Ct_true", "-Evt->mc_trks[0].dir.z"},
  {"Pdg"    , "Evt->mc_trks[0].type"},
  {"IsCC"   , TString::Format( "(Evt->w2list[%d]==2)", W2LIST_GSEAGEN_CC )},
  //{"By_reco", TString::Format( "???", W2LIST_GSEAGEN_BY )}, //Need a formula for By_reco...
  {"By_true", TString::Format( "Evt->w2list[%d]", W2LIST_GSEAGEN_BY )},
  //{"W"      , TString::Format("Evt->w[%d]", WEIGHTLIST_RUN_BY_RUN_WEIGHT)}, //Nominally equivalent to following formulas, but broke at some point...
  {"W_data" , "1"},
  {"W_nu"   , TString::Format("sum_mc_evt.livetime_DAQ * Evt->w[%d]/sum_mc_evt.n_gen", WEIGHTLIST_DIFFERENTIAL_EVENT_RATE)}, //Note diff rate is in [GeV*m2*sr]
  {"W_mu"   , "sum_mc_evt.livetime_DAQ / sum_mc_evt.livetime_sim"},
  {"W_noise", "sum_mc_evt.livetime_DAQ / sum_mc_evt.livetime_sim"}
};

// Branch names with special handling, the rest go to WeightData.Other
std::set<std::string> defaultbranchnames = {
  "E_reco",
  "E_reco_bin",
  "E_reco_bin_center",
  "Ct_reco",
  "Ct_reco_bin",
  "Ct_reco_bin_center",
  "E_true",
  "E_true_bin",
  "E_true_bin_center",
  "Ct_true",
  "Ct_true_bin",
  "Ct_true_bin_center",
  "Pdg",
  "IsCC",
  "By_reco",
  "By_reco_bin",
  "By_reco_bin_center",
  "By_true",
  "By_true_bin",
  "By_true_bin_center",
  "N",
  "W",
  "W_data",
  "W_nu",
  "W_mu",
  "W_noise",
  "WE"
};

