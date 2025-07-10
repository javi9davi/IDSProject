//
// Created by javierdr on 14/02/25.
//

#ifndef SIGNATURES_H
#define SIGNATURES_H

#include <string>
#include <nlohmann/json.hpp>

bool queryMalwareBazaar(const std::string& hash, nlohmann::json& response);

#endif // SIGNATURES_H
