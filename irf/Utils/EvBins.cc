#include "Utils/EvBins.hh"

#include "Utils/Parser.hh" //for ReadJSON, getValueJSON

#include <vector>
#include <string>
#include <math.h> //for sqrt

using json = nlohmann::json;


// Auxiliary functions, not actually member functions

std::vector<double> MakeVarBinsFromJSON(const json& config_var)
{
  if (getValueJSON<bool>(config_var, {"custom"})) {
    return getValueJSON<std::vector<double>>(config_var, {"custom_edges"});
  } else {
    int var_nbins = getValueJSON<int>(config_var,        {"nbins"});
    double var_min = getValueJSON<double>(config_var,    {"min"});
    double var_max = getValueJSON<double>(config_var,    {"max"});
    if ( config_var.contains("log") and getValueJSON<bool>(config_var, {"log"}))
      return MakeLogBins(var_nbins, var_min, var_max);
    else
      return MakeUniformBins(var_nbins, var_min, var_max);
  }
}

std::vector<double> MakeUniformBins(int nbins, double min, double max)
{
  double width = (max-min)/nbins;
  std::vector<double> bins;
  for (int i = 0; i < nbins; i++) {
    bins.push_back(min + i * width);
  }
  bins.push_back(max);
  return bins;
}

std::vector<double> MakeLogBins(int nbins, double min, double max)
{
  double DeltaLog = log(max / min);
  std::vector<double> bins;
  for (int i = 0; i < nbins; i++) {
    bins.push_back(min * exp(DeltaLog * double(i) / double(nbins)));
  }
  bins.push_back(max);
  return bins;
}


int FindBin( double value, const std::vector<double>& bins, bool skipsortedcheck /*=false*/)
{
  // we assume that bins are in increasing order...
  if (not skipsortedcheck and not std::is_sorted(std::begin(bins), std::end(bins)) ) {
    std::cout << "WARNING: Input vector of bin edges not ordered!!! Likely error incoming" << std::endl;
  }

  if (value < bins[0] ) {
    //std::cout << "ERROR: value " << value << " input to FindBin smaller than smallest bin edge " << bins[0] << std::endl;
    return -1;
  } else if (bins.back() <= value) {
    //std::cout << "ERROR: value " << value << " input to FindBin larger than largest bin edge " << bins.back() << std::endl;
    return bins.size();
  } else {
    size_t index = 0;
    while ( bins[index] <= value and index < bins.size() ){ //TODO: double-check this...
      index++;
    }
    return int(index)-1;
  }

}

double BinCenter( int index, const std::vector<double>& bins)
{
  if (bins.size() < 2) {
    return 0;
  }

  if (index>=0 and index < (int) bins.size()) {
    // Regular case
    return ( bins[index] + bins[index+1] )/2;
  } else {
    // Overflow outside bins
    double halfstep = ( bins[1]-bins[0] )/2;
    if (index<0) {
      return bins[0]-halfstep;
    } else {
      return bins.back()+halfstep;
    }
  }
}

double BinLogCenter( int index, const std::vector<double>& bins)
{
  if (index>=0 and index < (int) bins.size()) {
    // Regular case
    return sqrt( bins[index] * bins[index+1] );
  } else {
    // Overflow outside bins
    double halflogstep = sqrt( bins[1]/bins[0] ) ;
    if (index<0) {
      return bins[0]/halflogstep;
    } else {
      return bins.back()*halflogstep;
    }
  }
}


// Class member functions
EvBins::EvBins()
{
}

EvBins::EvBins(int E_true_nbins, double E_true_min, double E_true_max,
           int E_reco_nbins, double E_reco_min, double E_reco_max,
           int Ct_true_nbins, double Ct_true_min, double Ct_true_max,
           int Ct_reco_nbins, double Ct_reco_min, double Ct_reco_max)
{
  //int By_true_nbins   = 1;
  //double By_true_min  = 0;
  //double By_true_max  = 1;
  //int By_reco_nbins   = 1;
  //double By_reco_min  = 0;
  //double By_reco_max  = 1;


  E_true_bins = MakeLogBins(E_true_nbins, E_true_min, E_true_max);
  E_reco_bins = MakeLogBins(E_reco_nbins, E_reco_min, E_reco_max);
  Ct_true_bins = MakeUniformBins(Ct_true_nbins, Ct_true_min, Ct_true_max);
  Ct_reco_bins = MakeUniformBins(Ct_reco_nbins, Ct_reco_min, Ct_reco_max);
  By_true_bins = MakeUniformBins(1, 0, 1);
  By_reco_bins = MakeUniformBins(1, 0, 1);

}

EvBins::EvBins(const EvBins& that)
{
  ///< Explicit copy constructor
  // Recall that std::vector copies with assignment operator are deep copies

  this->E_true_bins   = that.E_true_bins;
  this->Ct_true_bins  = that.Ct_true_bins;
  this->By_true_bins  = that.By_true_bins;
  this->E_reco_bins   = that.E_reco_bins;
  this->Ct_reco_bins  = that.Ct_reco_bins;
  this->By_reco_bins  = that.By_reco_bins;
}

EvBins::EvBins(const json& config)
{
  // Extract binning information and setup EvBins
  LoadJSON(config);

}

void EvBins::LoadJSON(const json& config)
{
  if (config.contains("binning")) {
    json config_binning = getValueJSON<json>(config, {"binning"});

    if ( config_binning.contains("E_true") ) {
      json config_binning_E_true = getValueJSON<json>(config_binning, {"E_true"});
      E_true_bins = MakeVarBinsFromJSON( config_binning_E_true);
    }
    if ( config_binning.contains("Ct_true") ) {
      json config_binning_Ct_true = getValueJSON<json>(config_binning, {"Ct_true"});
      Ct_true_bins = MakeVarBinsFromJSON( config_binning_Ct_true);
    }
    if ( config_binning.contains("By_true") ) {
      json config_binning_By_true = getValueJSON<json>(config_binning, {"By_true"});
      By_true_bins = MakeVarBinsFromJSON( config_binning_By_true);
    }

    if ( config_binning.contains("E_reco") ) {
      json config_binning_E_reco = getValueJSON<json>(config_binning, {"E_reco"});
      E_reco_bins = MakeVarBinsFromJSON( config_binning_E_reco);
    }
    if ( config_binning.contains("Ct_reco") ) {
      json config_binning_Ct_reco = getValueJSON<json>(config_binning, {"Ct_reco"});
      Ct_reco_bins = MakeVarBinsFromJSON( config_binning_Ct_reco);
    }
    if ( config_binning.contains("By_reco") ) {
      json config_binning_By_reco = getValueJSON<json>(config_binning, {"By_reco"});
      By_reco_bins = MakeVarBinsFromJSON( config_binning_By_reco);
    }

  }
}

EvBins::~EvBins()
{
    ///< Destructor
}

bool CheckBinsValidity( const std::vector<double>& bins, std::string name )
{
  bool check = true;
  if (bins.size() == 0) {
    std::cout << "WARNING: " << name << " bins are empty!!" << std::endl;
    check = false;
  } else if (not std::is_sorted(std::begin(bins), std::end(bins)) ) {
    std::cout << "WARNING: " << name << "  bins are not ordered!!" << std::endl;
    check = false;
  }
  return check;
}

bool EvBins::CheckValidity(std::string source /*="nu"*/) const
{
  bool check = 
    CheckBinsValidity(E_reco_bins, "E_reco")
    & CheckBinsValidity(Ct_reco_bins, "Ct_reco");
  if (source == "nu" or source == "mu") {
    check = check 
      & CheckBinsValidity(E_true_bins, "E_true")
      & CheckBinsValidity(Ct_true_bins, "Ct_true");
  }
  if (source == "nu") {
    check = check 
      //& CheckBinsValidity(By_reco_bins, "By_reco")
      & CheckBinsValidity(By_true_bins, "By_true");
  }

  return check;
}


