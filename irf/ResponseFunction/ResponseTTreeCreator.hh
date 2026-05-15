#ifndef RESPONSETTREECREATOR_H_INCLUDED
#define RESPONSETTREECREATOR_H_INCLUDED


#include <vector>
#include <string>

#include <TFile.h>
#include <TChain.h>

#include <nlohmann/json.hpp>


/**
 * @brief Loads input ROOT files and returns the contained TTress (Evt, DST, etc.) as a vector of friended TChains.
 * 
 * @param filename_list vector<string> that contains the list of ROOT files with the TTrees to be loaded.
 */
std::vector<TChain*> LoadTTree( std::vector<std::string> filename_list );


/**
 * @brief Writes a TFile containing a TTree with binned E / Ct information, based on input config
 * 
 * @param config JSON containing information on bins and cuts
 * @param source "data", "nu", "mu" or "noise"
 * @param outputFile TFile where the output TTrees will be saved
 */
void ProcessSource(nlohmann::json config, std::string source, TFile* outputFile);

#endif
