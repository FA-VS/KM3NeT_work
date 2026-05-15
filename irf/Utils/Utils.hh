#ifndef __UTILS_INCLUDED__
#define __UTILS_INCLUDED__

#include "TFile.h"

#include <string>
#include <vector>

bool file_exists(const std::string& filename);
std::vector<std::string> split(std::string inputstring, char delimiter);

std::vector<std::string> GetObjectNamesInTFile( const TFile& f, std::string classname = "TTree");
std::vector<std::string> GetMatchingTTreesFromTFiles( const std::vector<std::string>& filename_list );


#endif // __UTILS_INCLUDED__
