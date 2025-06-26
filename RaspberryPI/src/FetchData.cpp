#include "../include/FetchData.hh"
#include <iostream>
#include <sstream>

FetchData::FetchData(const std::string url) {
    api_url = url;

    curl_global_init(CURL_GLOBAL_DEFAULT);
}

FetchData::~FetchData() {
    curl_global_cleanup();
}

size_t FetchData::writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string FetchData::fetch() {
    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) {
        std::cerr << "Failed to initialize CURL\n";
        return "";
    }

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FetchData::writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << "\n";
    }

    curl_easy_cleanup(curl);
    return response;
}
