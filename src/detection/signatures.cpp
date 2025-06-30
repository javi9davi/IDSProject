#include "signatures.h"

#include <iostream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool queryMalwareBazaar(const std::string& hash, nlohmann::json& response)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[ERROR] Fallo al inicializar CURL" << std::endl;
        return false;
    }

    std::string readBuffer;
    std::string api_url = "https://mb-api.abuse.ch/api/v1/";

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // Preparamos el JSON del body
    std::string json_body = R"({"query":"get_info","hash":")" + hash + R"("})";
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[ERROR] CURL fallo: " << curl_easy_strerror(res) << std::endl;
        return false;
    }

    try {
        response = nlohmann::json::parse(readBuffer);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Fallo al parsear respuesta JSON: " << e.what() << std::endl;
        return false;
    }
}
