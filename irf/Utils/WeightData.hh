#ifndef __WEIGHTDATA_H_INCLUDED__
#define __WEIGHTDATA_H_INCLUDED__

#include <map>
#include <string>

struct WeightData {
  int N;
  double W;
  double WE;
  std::map< std::string, double > Other;

  WeightData& operator+=(const WeightData& wd) {
    N += wd.N;
    W += wd.W;
    WE += wd.WE;
    for (const auto& [name, value] : wd.Other) { //std::pair<std::string,double>
      Other[name] += value;
    }
    return *this;

  }

};

#endif // __WEIGHTDATA_H_INCLUDED__
