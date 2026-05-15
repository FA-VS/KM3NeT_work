#ifndef __RESPONSEDEFAULTS_INCLUDED__
#define __RESPONSEDEFAULTS_INCLUDED__

#include "TString.h"

#include <map>
#include <set>
#include <string>

// Default branch formulas, if not provided in input json
extern std::map<std::string, TString> defaultbranchformulas;

// Branch names with special handling, the rest go to WeightData.Other
extern std::set<std::string> defaultbranchnames;

#endif // __RESPONSEDEFAULTS_INCLUDED__
