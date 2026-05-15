#ifndef __POINT_H_INCLUDED__
#define __POINT_H_INCLUDED__


struct Point {
  int E_reco_bin;
  int Ct_reco_bin;
  int By_reco_bin;
  int E_true_bin;
  int Ct_true_bin;
  int By_true_bin;
  int Pdg;
  bool IsCC;

  // Following operator is necessary to use Point in std::map
  bool operator<(const Point& other) const {
    if (E_reco_bin != other.E_reco_bin)
      return E_reco_bin < other.E_reco_bin;
    if (Ct_reco_bin != other.Ct_reco_bin)
      return Ct_reco_bin < other.Ct_reco_bin;
    if (By_reco_bin != other.By_reco_bin)
      return By_reco_bin < other.By_reco_bin;
    if (E_true_bin != other.E_true_bin)
      return E_true_bin < other.E_true_bin;
    if (Ct_true_bin != other.Ct_true_bin)
      return Ct_true_bin < other.Ct_true_bin;
    if (By_true_bin != other.By_true_bin)
      return By_true_bin < other.By_true_bin;
    if (Pdg != other.Pdg)
      return Pdg < other.Pdg;
   return IsCC < other.IsCC;
  }
};


#endif // __POINT_H_INCLUDED__
