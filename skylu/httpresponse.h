//
// Created by jimlu on 2020/5/24.
//

#ifndef HASHTEST_HTTPRESPONSE_H
#define HASHTEST_HTTPRESPONSE_H

#include "buffer.h"
#include <unordered_map>

#define HTTP_TIMEOUT 500
namespace skylu{
class HttpResponse {
public:
    static const std::unordered_map<int,std::string> statusCodeAndMessage;
    static const std::unordered_map<std::string,std::string> suffixAndName;
    HttpResponse(int statusCode,const std::string &path,bool keepAlive);
    ~HttpResponse() = default;

    Buffer initResponse();
    void initErrorResponse(Buffer & output,const std::string & message);
    void initStaticRequest(Buffer & output,int64_t filesize);

private:
    std::string getFileType();


private:
    std::map<std::string,std::string> m_header;
    int m_status;
    std::string m_path;
    bool isKeepAlive;

};
}


#endif //HASHTEST_HTTPRESPONSE_H
