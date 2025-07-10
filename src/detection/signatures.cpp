#include "signatures.h"

#include <iostream>
#include <string>
#include <fstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Carga AUTH_KEY desde el fichero .env
static bool loadAuthKey(std::string& outKey, const std::string& envPath = ".env") {
    std::ifstream envFile(envPath);
    if (!envFile.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir " << envPath << std::endl;
        return false;
    }
    std::string line;
    while (std::getline(envFile, line)) {
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        auto end = line.find_last_not_of(" \t");
        std::string trimmed = line.substr(start, end - start + 1);
        const std::string prefix = "AUTH_KEY=";
        if (trimmed.rfind(prefix, 0) == 0) {
            outKey = trimmed.substr(prefix.size());
            std::cout << "[DEBUG] AUTH_KEY encontrada en " << envPath << std::endl;
            return true;
        }
    }
    std::cerr << "[ERROR] AUTH_KEY no encontrada en " << envPath << std::endl;
    return false;
}

bool queryMalwareBazaar(const std::string& hash, nlohmann::json& response) {
    std::cout << "[DEBUG] queryMalwareBazaar called with hash: " << hash << std::endl;

    // 1) Cargo la clave de .env
    std::string authKey;
    if (!loadAuthKey(authKey, "/root/CLionProjects/IDSProject/.env")) {
        return false;
    }
    std::cout << "[DEBUG] Usando AUTH_KEY: " << authKey << std::endl;

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[ERROR] Fallo al inicializar CURL" << std::endl;
        return false;
    }

    std::string readBuffer;
    const std::string api_url = "https://mb-api.abuse.ch/api/v1/";
    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // 2) Construyo el body form-urlencoded tal cual en wget --post-data
    //    query=get_info&hash=<hash>

    // 2) Construyo el body form-urlencoded tal cual en wget --post-data
    //    query=get_info&hash=<hash>
    std::string form;
    if (char* escaped = curl_easy_escape(curl, hash.c_str(), hash.size())) {
        const auto QUERY_TYPE = "query=get_info&hash=";
        form = std::string(QUERY_TYPE) + escaped;
        curl_free(escaped);

        std::cout << "[DEBUG] form data: " << form << std::endl;
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form.c_str());
        // opcionalmente:
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)form.size());
    } else {
        std::cerr << "[ERROR] Error escapando el hash" << std::endl;
    }

    // 3) Cabeceras HTTP
    struct curl_slist* headers = nullptr;
    // Content-Type para form-urlencoded
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    // Auth-Key
    const std::string authHeader = "Auth-Key: " + authKey;
    headers = curl_slist_append(headers, authHeader.c_str());
    std::cout << "[DEBUG] Headers añadidos:\n"
              << "  Content-Type: application/x-www-form-urlencoded\n"
              << "  " << authHeader << std::endl;
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);


    // 4) Callback y buffer
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    // 5) Ejecuto la petición
    const CURLcode res = curl_easy_perform(curl);
    std::cout << "[DEBUG] curl_easy_perform result code: " << res << std::endl;
    if (res != CURLE_OK) {
        std::cerr << "[ERROR] CURL fallo: " << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    std::cout << "[DEBUG] Raw response: " << readBuffer << std::endl;

    // 6) Parseo respuesta como JSON
    try {
        response = nlohmann::json::parse(readBuffer);
        std::cout << "[DEBUG] Parsed JSON response: " << response.dump(2) << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Fallo al parsear respuesta JSON: " << e.what() << std::endl;
        std::cerr << "[DEBUG] Response buffer was: " << readBuffer << std::endl;
        return false;
    }
}
