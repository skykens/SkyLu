#include "util.h"
#include <signal.h>
namespace skylu{

pid_t getThreadId()
{
    return syscall(SYS_gettid);
}
uint32_t getFiberId()
{

    return 0;
}

uint64_t getCurrentMS()
{
    struct timeval tv;
    gettimeofday(&tv,nullptr);
    return tv.tv_sec * 1000ul + tv.tv_usec / 1000;

}



uint64_t getCurrentUS()
{
    struct timeval tv;
    gettimeofday(&tv,nullptr);
    return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
        
}

void FSUtil::ListAllFile(std::vector<std::string> &files, const std::string &path, const std::string &subfix)
{
    /*
    if(assess(path.c_str(),0)!=0)
    {
        return ;
    }*/
    DIR* dir = opendir(path.c_str());
    if(dir == nullptr)
    {
        return ;
    }
    struct dirent* dp =nullptr;
    while((dp=readdir(dir))!=nullptr)
    {
        if(dp->d_type ==DT_DIR){
            if(!strcmp(dp->d_name,".")||!strcmp(dp->d_name,".."))
                continue;
            ListAllFile(files,path+"/"+dp->d_name,subfix);//深度优先搜索
        }else if(dp->d_type == DT_REG){
        std::string filename(dp->d_name);
        if(subfix.empty())
        {
            files.push_back(path+"/"+filename);

        }else {
            if(filename.size()<subfix.size())
            {
                continue;
            }
            if(filename.substr(filename.length()-subfix.size())==subfix)
            {
                files.push_back(path+"/"+filename);
            } //匹配后缀
            }
        }
    }
    closedir(dir);

}

}
