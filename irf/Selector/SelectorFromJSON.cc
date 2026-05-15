#include "km3net-dataformat/definitions/root.hh" //TTREE_OFFLINE_EVENT

#include "Utils/Parser.hh" //for readJSON, getValueJSON

#include <nlohmann/json.hpp>

#include "TFile.h"
#include "TTree.h"
#include "TCut.h"
#include "TKey.h"

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <getopt.h> // command line parsing

using json = nlohmann::json;

std::vector<std::string> SplitString(std::string input, char c) {
  std::stringstream istream(input);
  std::vector<std::string> seglist;

  std::string segment;
  while(std::getline(istream, segment, c))
    seglist.push_back(segment);
  return seglist;
}

void PrintTTreeNamesInTFile(TFile* f)
{
  //List available TTree keys for debugging (placeholder before more permanent solution?)
  TList* List = f->GetListOfKeys();
  if ( List == nullptr)
    std::cerr << " No keys in " << f->GetPath() << " !!" << std::endl;
  else {
    std::cerr << " Available TTrees are: " << std::endl;
    TKey* key;
    TIter nextkey(List);
    TString oldkeyname;
    while ( (key = (TKey*) nextkey())) {
      // This works because TTrees are stored consecutively in decreasing order of cycles
      bool alreadyseen = (oldkeyname == key->GetName());
      if (alreadyseen) continue;

      if ( (std::string) key->GetClassName() == "TTree")
        std::cerr << "  -> " << key->GetName() << std::endl;
      oldkeyname = key->GetName();
    }
  }
}


int main(int argc, char* argv[]){

  // Input parser (needed for scripting) TODO: Use JParser instead? Debug? entries to process, time-computation?
  // TODO: add option for user to give multiple input files at once?
  std::string ifilepath, ofilepath, friendpath; //friendpath must be defined outside swith
  std::vector<std::string> friendtreenames; //Must be defined outside switch
  std::map<std::string, std::vector<std::string>> friendtree_map;
  TCut selection;
  json config;
  int opt;
  while ( (opt = getopt(argc, argv, "j:i:o:c:f:h")) != EOF) {
    switch (opt) {
      case 'j':
        config = readJSON(optarg);
        if ( config.contains("outputfile") )
          ofilepath = getValueJSON<std::string>(config, {"outputfile"});
        if ( config.contains("inputfile") )
          ifilepath = getValueJSON<std::string>(config, {"inputfile"});
        if ( config.contains("friendtrees") )
          friendtree_map = getValueJSON< std::map<std::string,std::vector<std::string>> >(config, {"friendtrees"});
        if ( config.contains("TCut") )
          selection = getValueJSON<std::string>(config, {"TCut"}).c_str();
        break;
      case 'o':
        ofilepath = optarg;
        break;
      case 'i':
        ifilepath = optarg;
        break;
      case 'c':
        selection = optarg;
        break;
      case 'f':
        friendtreenames = SplitString(optarg, ',');
        friendpath = friendtreenames[0];
        friendtreenames.erase(friendtreenames.begin());
        friendtree_map[friendpath] = friendtreenames;
        break;
      case '?':
      case 'h':
      default :
        std::cout << "Usage: SelectorFromJSON [-j input.json] [-o outputpath] [-c cut] [-i inputpath] [-f friendpath,treename1,treename2...]" << std::endl;
        return 1;

    }
  }

  if ( ofilepath == "" ) {
    std::cout << "Empty ofilepath argument, aborting SelectorFromJSON." << std::endl;
    return 1;
  }

  // Add input file and its TTrees to list of files/trees to friend
  if (ifilepath != "") {
    // TODO: add ttree_dst_name to json? or to km3net-dataformat
    char const* ttree_dst_name = "T";
    friendtree_map[ifilepath] = {TTREE_OFFLINE_EVENT, ttree_dst_name};
  }

  // Loop over friends to open their TFiles and load the TTrees
  std::vector<TFile*> friendfile_vec(0);
  std::vector<TTree*> friendtree_vec(0);
  TFile* friendfile = nullptr;
  TTree* friendtree = nullptr;
  for (auto const& entry : friendtree_map )
  {
    std::string treepath = entry.first;
    std::vector<std::string> treename_list = entry.second;

    friendfile = TFile::Open( treepath.c_str() );
    if (friendfile == nullptr) {
      std::cerr << "ERROR: File " << treepath << " can't be opened." << std::endl;
      return 1;
    }
    friendfile_vec.push_back( (TFile*) friendfile );

    for (std::string treename : treename_list) 
    {
      friendtree = (TTree*) friendfile->Get( treename.c_str() );
      if (friendtree==nullptr or friendtree==0) {
        std::cerr << "ERROR: File " << treepath << " has no '" << treename << "' tree, no cuts will be available on related variables" << std::endl;
        PrintTTreeNamesInTFile(friendfile);
        return 1;
      }
      friendtree_vec.push_back( friendtree );
    }
  }
  if ( friendtree_vec.size() == 0 ) {
    std::cout << "List of TTrees to select from is empty, aborting SelectorFromJSON." << std::endl;
    return 1;
  }


  // Prepare for filtering by friending the TTrees
  for ( TTree* friendtree : friendtree_vec ) {
    // This nonsense is necessary because CopyTree does not accept EventLists as input, only cuts,
    // so all TTrees must be friends of all other TTrees...
    for ( TTree* friendtree_inloop : friendtree_vec ) {
      if ( friendtree != friendtree_inloop)
        friendtree->AddFriend( friendtree_inloop );
    }
  }

  // Copy the filtered entries to new trees
  TFile *newFile = TFile::Open(ofilepath.c_str(), "RECREATE");
  newFile->cd();

  std::cout << " Copying " << friendtree_vec.size() << " friend TTree(s)..." << std::endl;
  for ( TTree* friendtree : friendtree_vec ) {
    std::cout << "   " << friendtree->GetName() << " from " << friendtree->GetDirectory()->GetName() << " ..." << std::endl;
    TTree *newfriendtree = friendtree->CopyTree(selection);
    newfriendtree->GetListOfFriends()->Clear();
    newfriendtree->Write();
  }
  for ( TFile* friendfile : friendfile_vec ) {
    friendfile->Close();
  }

  newFile->Close();
  std::cout << "Created " << ofilepath << std::endl;

  return 0;
}
