//
// Created by jimlu on 2020/5/24.
//

#include "httpresponse.h"

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


HttpResponse::HttpResponse(std::string version,int statusCode,bool keepActive,const std::string &method,
                           const std::string &path,const std::string& args,Buffer* body)
    :m_version(version)
    ,m_status(statusCode)
    ,m_method(method)
    ,m_path(path)
    ,m_args(args)
    ,m_input_body(body)
    ,isKeepAlive(keepActive){

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
    output.append("Keep-Alive: timeout=" + std::to_string(kHttpTimeout));

  }else{
    output.append("Connection: close\r\n");
  }

  output.append("Content-type: " + getFileType() + "\r\n");
  output.append("Content-length: " + std::to_string(filesize) + "\r\n");

  output.append("Server: skylu\r\n");
  output.append("\r\n");

  if(m_method == "POST" || (m_method =="GET" && !m_args.empty())){

    if(!doCGI(output)){
      //TODO 失敗情況

    }

  }else{
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
bool HttpResponse::doCGI(Buffer& outputBody) {
  //导入环境变量
  std::stringstream  ss[3];
  ss[0]<<"METHOD="<<m_method;
  putenv(const_cast<char *>(ss[0].str().c_str()));

  ss[1]<<"QUERY_STRING="<<m_args;
  putenv(const_cast<char *>(ss[1].str().c_str()));

  ss[2]<<"CONTENT_LEN="<<m_input_body->readableBytes();
  putenv(const_cast<char *>(ss[2].str().c_str()));

  int input[2];
  int output[2];

  pipe(input);
  pipe(output);
  pid_t id = fork();

  if(id < 0){
    SKYLU_LOG_ERROR(G_LOGGER)<<"fork("<<id<<") failed. errno ="<<errno<<"   strerror="<<strerror(errno);
    return false;
  }else if(id == 0){

    close(input[1]); //关闭写端
    close(output[0]); //关闭读端

    dup2(input[0],0);  //重定向文件描述符到标准输入
    dup2(output[1],1); //标准输出

    execl(m_path.c_str(), m_args.c_str(),nullptr);
    /// FIXME 这里可能需要屏蔽信号
    exit(1);
  }else{
    close(input[0]);
    close(output[1]);

    if(m_method == "POST"){
      write(input[1],m_input_body->curRead(),m_input_body->readableBytes());
    }  /// 向进程发送数据

    int length = 0;

    while((length = read(output[0], outputBody.curWrite(),outputBody.writeableBytes()))){
      ///读取到0 结束
      outputBody.updatePos(length);


      if(!outputBody.writeableBytes()){
        outputBody.ensureWriteableBytes(outputBody.readableBytes() * 2); //扩容
      }


    } // TODO 这里可能会导致陷入一个阻塞的情况
    waitpid(id,NULL,0);

  }
  return true;


}
}