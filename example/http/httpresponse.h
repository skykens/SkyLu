//
// Created by jimlu on 2020/5/24.
//

#ifndef HASHTEST_HTTPRESPONSE_H
#define HASHTEST_HTTPRESPONSE_H

#include "skylu/net/buffer.h"
#include <unordered_map>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>


namespace skylu{
class HttpResponse {
  static const int kHttpTimeout = 500;
public:
    static const std::unordered_map<int,std::string> statusCodeAndMessage;
    static const std::unordered_map<std::string,std::string> suffixAndName;
    HttpResponse(std::string version,int statusCode,bool isKeepActive,const std::string &method = "",const std::string &path= "",
                    const std::string &args="",Buffer *body= nullptr);

    ~HttpResponse() = default;

    Buffer initResponse();
    void initErrorResponse(Buffer & output,const std::string & message);
    void initStaticRequest(Buffer & output,int64_t filesize);


private:
    std::string getFileType();
    bool doCGI(Buffer&  outputBody);


private:
    std::string m_version;
    std::map<std::string,std::string> m_header;
    int m_status;
    const std::string m_method;
    const std::string m_path;
    const std::string m_args;
    Buffer* m_input_body;
    bool isKeepAlive;

};
}


#endif //HASHTEST_HTTPRESPONSE_H
