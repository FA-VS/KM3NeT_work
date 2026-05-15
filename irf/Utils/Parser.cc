#include "Utils/Parser.hh"

#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <fstream>

using json = nlohmann::json;

nlohmann::json readJSON(std::string path)
{
  json input_json;
  std::ifstream f(path);
  if (f.good()) {
    input_json = json::parse(f);
  }
  else {
    std::cerr << "Error: file " << path << " does not exist.\n";
    exit(1);
  }
  return input_json;
}
