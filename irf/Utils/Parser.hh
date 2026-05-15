#ifndef __PARSER_INCLUDED__
#define __PARSER_INCLUDED__

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <iostream>
#include <vector>
#include <string>

template<typename T>
static T getValueJSON(const nlohmann::json& jsonObject, const std::vector<std::string>& keys) {
    try {
      nlohmann::json currentObject = jsonObject;
      for (const auto& key : keys) {
        currentObject = currentObject.at(key);
      }
        return currentObject.get<T>();
    } catch (const std::exception& e) {
      std::cerr << "Cannot access the keys: ";
      for (const auto& key : keys) {
        std::cerr << key << ":";
      }
      throw;
    }
}

nlohmann::json readJSON(std::string path);

#endif // __PARSER_INCLUDED__
