#include "Utils/Utils.hh"

#include "TFile.h"
#include "TKey.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <stdexcept> //for throw, invalid_argument
//#include <algorithm> //for find (?)


bool file_exists(const std::string& filename)
{
  //C++11
  //std::ifstream streamfile(filename.c_str());
  //return streamfile.good();
  //C++17
  return std::filesystem::is_regular_file(filename);
}

std::vector<std::string> split(std::string inputstring, char delimiter)
{
  std::istringstream ss(inputstring);
  std::string token;
  std::vector<std::string> token_vec;

  while ( getline(ss, token, delimiter)) {
    token_vec.push_back(token);
  }
  return token_vec;
}

std::vector<std::string> GetObjectNamesInTFile(const TFile& f, std::string classname /*="TTree"*/)
{
  //List available TTree keys
  std::vector<std::string> ttreename_vec(0);
  TKey* key;
  TIter nextkey( f.GetListOfKeys() );
  std::string newkeyname, oldkeyname;
  while ( (key = (TKey*) nextkey())) {
    // This works because TTrees are stored consecutively in decreasing order of cycles
    newkeyname = key->GetName();
    bool alreadyseen = (oldkeyname == newkeyname);
    if (alreadyseen) continue;

    if ( (std::string) key->GetClassName() == classname )
      ttreename_vec.push_back( newkeyname );
    oldkeyname = newkeyname;
  }
  return ttreename_vec;
}


std::vector<std::string> GetMatchingTTreesFromTFiles( const std::vector<std::string>& filename_list )
{

  // Check all files exist:
  for (std::string filename : filename_list ) {
    if (not file_exists(filename) ) {
      throw std::invalid_argument( "Input file " + filename + " does not exist!" );
    }
  }

  // Find list of TTree names from the first file
  TFile file_0( filename_list[0].c_str() );
  std::vector<std::string> ttreename_vec = GetObjectNamesInTFile(file_0, "TTree");
  file_0.Close();

  std::cout << "List of input files read:" << std::endl;
  for (std::string filename : filename_list ) {
    std::cout << "  " << filename << std::endl;

    // Check that TTrees in this file matches original
    TFile file_i( filename.c_str() );
    std::vector<std::string> ttreename_vec_i = GetObjectNamesInTFile(file_i, "TTree");
    file_i.Close();
    bool ttreename_vec_match = ( ttreename_vec.size() == ttreename_vec_i.size() );
    if ( ttreename_vec_match ) {
      for (std::string ttreename : ttreename_vec) {
        bool name_present = ( std::find(ttreename_vec_i.begin(), ttreename_vec_i.end(), ttreename) != ttreename_vec_i.end() );
        if (not name_present) {
          ttreename_vec_match = false;
          break;
        }
      }
    }
    if (not ttreename_vec_match) {
      std::cout << "ERROR: TTrees inside " << filename << " do not match those in " << filename_list[0] << " !!!" << std::endl;
      throw std::invalid_argument( "All input files must have matching TTrees" );
    }
  }
  return ttreename_vec;
}

