//
// Created by jimlu on 2020/5/24.
//

#include "httpresponse.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>

namespace skylu{
    const std::unordered_map<int,std::string> HttpResponse::statusCodeAndMessage = {
            {200, "OK"},
            {400,"Bad Request"},
            {403,"Forbidden"},
            {404,"Not Found"}
    };
    const std::unordered_map<std::string,std::string> HttpResponse::suffixAndName = {

            {".html", "text/html"},
            {".xml", "text/xml"},
            {".xhtml", "application/xhtml+xml"},
            {".txt", "text/plain"},
            {".rtf", "application/rtf"},
            {".pdf", "application/pdf"},
            {".word", "application/nsword"},
            {".png", "image/png"},
            {".gif", "image/gif"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".au", "audio/basic"},
            {".mpeg", "video/mpeg"},
            {".mpg", "video/mpeg"},
            {".avi", "video/x-msvideo"},
            {".gz", "application/x-gzip"},
            {".tar", "application/x-tar"},
            {".css", "text/css"}
    };


    HttpResponse::HttpResponse(std::string version,int statusCode, const std::string &path, bool keepAlive)
            :m_version(version)
            ,m_status(statusCode)
            ,m_path(path)
            ,isKeepAlive(keepAlive){

    }



    Buffer HttpResponse::initResponse() {
        Buffer output;
        if(m_status == 400){
            initErrorResponse(output,"Can't parse the message");
            return output;
        }
        struct stat stfd;

        if(::stat(m_path.data(),&stfd) < 0){
            m_status = 404;
            initErrorResponse(output,"Can't find the file");
            return output;
        }

        if(!(S_ISREG(stfd.st_mode)||!(S_IRUSR && stfd.st_mode))){
            m_status = 403;
            initErrorResponse(output,"Can't read the file");
            return output;
        }


        initStaticRequest(output,stfd.st_size);
        return output;

    }


    void HttpResponse::initErrorResponse(Buffer &output, const std::string &message) {
        std::string body;
        auto it = statusCodeAndMessage.find(m_status);
        if(it == statusCodeAndMessage.end()){
            return ;
        }
        body += "<html><title>Skylu Error</title>";
        body += "<body bgcolor=\"ffffff\">";
        body += std::to_string(m_status) + " : " + it -> second + "\n";
        body += "<p>" + message + "</p>";
        body += "<hr><em>Skylu web server</em></body></html>";

        // 响应行
        output.append(m_version+" " + std::to_string(m_status) + " " + it -> second + "\r\n");
        // 报文头
        output.append("Server: Skylu\r\n");
        output.append("Content-type: text/html\r\n");
        if(isKeepAlive){
            output.append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
        }else{
            output.append("\r\n");
        }
           // output.append("Connection: close\r\n");
        // 报文体
        output.append(body.data(),body.size());
    }


    void HttpResponse::initStaticRequest(Buffer &output, int64_t filesize) {
        assert(filesize >= 0);

        auto it = statusCodeAndMessage.find(m_status);
        if(it == statusCodeAndMessage.end()){
            m_status = 400;
            initErrorResponse(output,"Unknow status code");
            return ;
        }

        output.append("HTTP/1.1 " + std::to_string(m_status) + " " + it->second + "\r\n");
        if(isKeepAlive){
            output.append("Connection: Keep-Alive\r\n");
            output.append("Keep-Alive: timeout=" + std::to_string(HTTP_TIMEOUT));

        }else{
            output.append("Connection: close\r\n");
        }

        output.append("Content-type: " + getFileType() + "\r\n");
        output.append("Content-length: " + std::to_string(filesize) + "\r\n");

        output.append("Server: skylu\r\n");
        output.append("\r\n");





        int srcfd = ::open(m_path.data(),O_RDONLY,0);

        if(srcfd < 0){
            output.resetAll();
            m_status = 404;
            initErrorResponse(output,"Can't find the file");
            return ;
        }
        void * datafd = ::mmap(NULL,filesize,PROT_READ,MAP_PRIVATE,srcfd,0);

        ::close(srcfd);
        if(datafd == (void *)-1){
            munmap(datafd,filesize);
            output.resetAll();
            m_status = 403;
            initErrorResponse(output,"Can't access.");
            return ;
        }

        output.append(static_cast<char *>(datafd),filesize);
        munmap(datafd,filesize);
    }




    std::string HttpResponse::getFileType() {
        int index = m_path.find_last_of('.');

        std::string suffix;
        if(static_cast<size_t>(index ) == std::string::npos){
            return "text/plain";
        }

        suffix = m_path.substr(index);

        auto it = suffixAndName.find(suffix);
        if(it == suffixAndName.end()){
            return "text/plain";
        }

        return it->second;




    }
}