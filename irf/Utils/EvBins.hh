//..............................................................................
///
/// \class EvBins
///
/// \brief Simple class to hold bin settings for the true and reconstructed
/// energy, zenith angle, and Bjorken-y.
///
/// Simple class to hold bin settings for the true and reconstructed
/// energy, zenith angle, and Bjorken-y.
///
///
//..............................................................................

#ifndef __EVBINS_H_INCLUDED__
#define __EVBINS_H_INCLUDED__

#include <vector>
#include <string>

#include <nlohmann/json.hpp>


// Auxiliary functions
std::vector<double> MakeVarBinsFromJSON(const nlohmann::json& config_var);
std::vector<double> MakeUniformBins(int nbins, double min, double max);
std::vector<double> MakeLogBins    (int nbins, double min, double max);
int FindBin( double value, const std::vector<double>& bins, bool skipsortedcheck = false);
double BinCenter( int bin, const std::vector<double>& bins);
double BinLogCenter( int bin, const std::vector<double>& bins);


// Main class
class EvBins {

  public:
    EvBins();             ///< Default constructor
    EvBins(const EvBins& that); ///< Copy constructor
    ~EvBins();            ///< Destructor

    /// All-in-one constructor (uses default binning shape)
    EvBins(int E_true_nbins, double E_true_min, double E_true_max,
	   int E_reco_nbins, double E_reco_min, double E_reco_max,
	   int Ct_true_nbins, double Ct_true_min, double Ct_true_max,
     int Ct_reco_nbins, double Ct_reco_min, double Ct_reco_max
     );

    EvBins( const nlohmann::json& config);
    void LoadJSON(const nlohmann::json& config);
    bool CheckValidity(std::string source = "nu") const;

    bool operator == (const EvBins& that) const {
      return (
          (this->E_true_bins == that.E_true_bins) and
          (this->Ct_true_bins == that.Ct_true_bins) and
          (this->By_true_bins == that.By_true_bins) and
          (this->E_reco_bins == that.E_reco_bins) and
          (this->Ct_reco_bins == that.Ct_reco_bins) and
          (this->By_reco_bins == that.By_reco_bins)
          );
    }

    // Bin vectors: lower edges of the bins, PLUS UPPER EDGE OF LAST BIN!
    // Note: These are the only member variables of EvBins
    std::vector<double> E_true_bins;  // True Energy
    std::vector<double> Ct_true_bins; // True cos(theta)
    std::vector<double> By_true_bins; // True Bjorken-y
    std::vector<double> E_reco_bins;  // Reconstructed Energy
    std::vector<double> Ct_reco_bins; // Reconstructed cos(theta)
    std::vector<double> By_reco_bins; // Reconstructed Bjorken-y

    // Manipulate / check the bin vectors
    // True energy binning
    double E_true_min() const { return E_true_bins.front(); }
    double E_true_max() const { return E_true_bins.back(); } // bounds for True energy in the EvStore
    int E_true_nbins() const { return E_true_bins.size()-1; }
    int FindBin_E_true( double value ) const { return FindBin( value, E_true_bins, true);}
    double BinCenter_E_true( int bin ) const { return BinCenter( bin, E_true_bins);}
      //TODO: should this be BinLogCenter instead?
    // True Ct binning
    double Ct_true_min()  const { return Ct_true_bins.front(); }
    double Ct_true_max()  const { return Ct_true_bins.back(); } // bounds for True cos theta in the EvStore
    int Ct_true_nbins()  const { return Ct_true_bins.size()-1; }
    int FindBin_Ct_true( double value ) const { return FindBin( value, Ct_true_bins, true);}
    double BinCenter_Ct_true( int bin ) const { return BinCenter( bin, Ct_true_bins);}
    // True By binning
    double By_true_min()  const { return By_true_bins.front(); }
    double By_true_max()  const { return By_true_bins.back(); } // bounds for True By in the EvStore
    int By_true_nbins()  const { return By_true_bins.size()-1; }
    int FindBin_By_true( double value ) const { return FindBin( value, By_true_bins, true);}
    double BinCenter_By_true( int bin ) const { return BinCenter( bin, By_true_bins);}
    // Reco energy binning
    double E_reco_min()  const { return E_reco_bins.front(); }
    double E_reco_max()  const { return E_reco_bins.back(); } // bounds for Reco energy in the EvStore
    int E_reco_nbins()  const { return E_reco_bins.size()-1; }
    int FindBin_E_reco( double value ) const { return FindBin( value, E_reco_bins, true);}
    double BinCenter_E_reco( int bin ) const { return BinCenter( bin, E_reco_bins);}
      //TODO: should this be BinLogCenter instead?
    // Reco Ct binning
    double Ct_reco_min()  const { return Ct_reco_bins.front(); }
    double Ct_reco_max()  const { return Ct_reco_bins.back(); } // bounds for Reco cos theta in the EvStore
    int Ct_reco_nbins()  const { return Ct_reco_bins.size()-1; }
    int FindBin_Ct_reco( double value ) const { return FindBin( value, Ct_reco_bins, true);}
    double BinCenter_Ct_reco( int bin ) const { return BinCenter( bin, Ct_reco_bins);}
    // Reco By binning
    double By_reco_min()  const { return By_reco_bins.front(); }
    double By_reco_max()  const { return By_reco_bins.back(); } // bounds for Reco By in the EvStore
    int By_reco_nbins()  const { return By_reco_bins.size()-1; }
    int FindBin_By_reco( double value ) const { return FindBin( value, By_reco_bins, true);}
    double BinCenter_By_reco( int bin ) const { return BinCenter( bin, By_reco_bins);}
};

#endif // __EVBINS_H_INCLUDED__

