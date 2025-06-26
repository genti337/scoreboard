#ifndef FETCHDATA_HH
#define FETCHDATA_HH

#include <string>
#include <curl/curl.h>

class FetchData {
public:
    FetchData(const std::string url);
    ~FetchData();

    std::string fetch();

private:
    std::string api_url;
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output);
};

#endif
