//
// Created by jimlu on 2020/5/25.
//

#include "memcachedserver.h"
#include "skylu/log.h"
#include "skylu/daemon.h"
#include <sys/mman.h>
namespace skylu{
    const char * MemcachedServer::aof_file_name = "memcached_aof_";
    const char * MemcachedServer::aof_written_file_name = "memcached_written_aof_";
    const size_t MemcachedServer::aof_file_length = 1024*1024*512;



    MemcachedServer::MemcachedServer(EventLoop *loop, const Address::ptr &addr,const std::string & name)
            : server(loop,addr,name)
            ,m_name(name)
            ,m_loop(loop)
            ,m_threads(0)
            ,isStart(false)
            ,m_aof_state(Init)
            ,m_aof_rewritten_buff(655360)
            ,m_aof_session(this)
            {
        server.setConnectionCallback(std::bind(&MemcachedServer::NewSession,this,std::placeholders::_1));
        server.setCloseCallback([this](const TcpConnection::ptr & conne){
            m_sessions.erase(conne);
            SKYLU_LOG_INFO(G_LOGGER)<<"close conne :"<<conne->getName();
        });

        loop->runEvery(1,std::bind(&MemcachedServer::AofSync,this));
        Signal::hook(SIGCHLD,std::bind(&MemcachedServer::setReWrittened,this));

    }
    void MemcachedServer::setReWrittened(){
        m_aof_state = ReWrittened;
        SKYLU_LOG_INFO(G_LOGGER)<<"Rewritten AOF Finnish.Current m_aof_seek:"<<m_aof_seek;
        assert(m_aof_state == ReWrittened);

    }
    void MemcachedServer::setThreadNums(int num) {
        m_threads = num;
        server.setThreadNum(num);

    }


    void MemcachedServer::run() {
        init();
        Timestamp when = Timestamp::now();
        LoadAof(when);
        m_aof_state = Append;
        isStart = true;
        server.start();
        m_loop->loop();
        isStart = false;

    }

    void MemcachedServer::init(){

        int fd = open(aof_file_name,O_CREAT|O_RDWR,S_IRUSR|S_IWUSR);
        if( fd < 0 )
        {
            perror("open()");
            exit(1);
        }
        if(File::getFilesize(aof_file_name)!=aof_file_length){
            size_t seek = lseek(fd,aof_file_length-1,SEEK_SET);
            if(seek != aof_file_length -1){
                perror("lseek()");
                exit(1);
            }
            write(fd,"\0",1);
        }

        if((m_aof_peek=mmap(NULL,aof_file_length,PROT_READ | PROT_WRITE ,MAP_SHARED,fd,0)) == MAP_FAILED){
            perror("mmap()");
            exit(1);
        }
        close(fd);
        assert(m_aof_peek != NULL);
    }

    void MemcachedServer::LoadAof(Timestamp &when) {
        SKYLU_LOG_INFO(G_LOGGER)<<"LoadAof Bgein:"<<when.getMicroSeconds();
        Buffer buf(aof_file_length);
        buf.append(static_cast<char*>(m_aof_peek),aof_file_length);
        size_t cur = 0;
        do{
            cur = buf.getRPos();
            m_aof_session.onMessage(nullptr,&buf); //Read Pos会一直移动直到没有数据可以读
        }while(cur != buf.getRPos());

        m_aof_seek = cur;
        SKYLU_LOG_INFO(G_LOGGER)<<"LoadAof End:"<<Timestamp::now().getMicroSeconds();



    }



    bool MemcachedServer::del(const ByteString &key) {
            Item::ptr tmp(new Item(key));
            size_t n = m_maps[tmp->hash() % SHARED_NUM].erase(tmp);
            return n==1;
    }

    bool MemcachedServer::set(const ByteString &key, const ByteString &value, int flag) {

        size_t hashcode = Item::hash(key);
        auto it = m_maps[hashcode%SHARED_NUM].insert(Item::makeItem(key,value,flag,hashcode));
        if(!it.second){
            it.first->get()->setValue(value);
        }
        return true;

    }

    Item* MemcachedServer::get(const ByteString &key) {

        Item::ptr tmp(new Item(key));
        auto it = m_maps[tmp->hash()%SHARED_NUM].find(tmp);
        if(it != m_maps[tmp->hash()%SHARED_NUM].end()){
            return it->get();
        }else{
            return nullptr;
        }


    }


    void MemcachedServer::NewSession(const TcpConnection::ptr & conne) {
        Session * session = new Session(this,conne);
        SKYLU_LOG_INFO(G_LOGGER)<<"new Session create  from conne:"<<conne->getName();
        m_sessions.insert({conne,session});
    }


    void MemcachedServer::appendAof(const ByteString &content) {
        if(m_aof_state == Init){
            return ;
        }
        assert(m_aof_state != Init);
        if(m_aof_state == Append &&m_aof_seek+content.getLen()*2 > aof_file_length/2){
            m_aof_state = aofState::ReWrittening; // 开始重写
            SKYLU_LOG_INFO(G_LOGGER)<<"Start ReWrittened.";
            RewrittenAOF();
        }
        if(m_aof_state == aofState::ReWrittened) {
            assert(m_aof_state == ReWrittened);
            munmap(m_aof_peek, aof_file_length);
            File::rename(aof_written_file_name, aof_file_name);
            m_aof_seek = File::getFilesize(aof_file_name);
            init();
            memcpy(static_cast<char*>(m_aof_peek) + m_aof_seek,
                    m_aof_rewritten_buff.curRead(),m_aof_rewritten_buff.readableBytes());
            m_aof_seek+=m_aof_rewritten_buff.readableBytes();
            m_aof_rewritten_buff.resetAll();
            m_aof_state = aofState::Sync;
            SKYLU_LOG_INFO(G_LOGGER)<<"Written to AOF";
        }

        if(m_aof_state == aofState::Append || m_aof_state== aofState::ReWrittening){

            assert(m_aof_seek+content.getLen() < aof_file_length);
            memcpy(static_cast<char*>(m_aof_peek) + m_aof_seek,content.getData(),content.getLen());
            m_aof_seek += content.getLen();
            m_aof_state = aofState::Sync;

        }
        if(m_aof_state == aofState::ReWrittening){
            m_aof_rewritten_buff.append(content.getData(),content.getLen());
        }
    }

    void MemcachedServer::AofSync() {
        if(m_aof_state == aofState::Sync) {
            int res = msync(m_aof_peek, m_aof_seek, 0);
            if (res < 0) {
                perror(strerror(errno));
            }
            SKYLU_LOG_INFO(G_LOGGER)<<"AofSync";
        }
        if(m_aof_state!= aofState::ReWrittened)
            m_aof_state = aofState::Append;

    }

    void MemcachedServer::RewrittenAOF() {
        assert(m_aof_state == aofState::ReWrittening);
        if(fork()>0){
            SKYLU_LOG_INFO(G_LOGGER)<<"clone child process.";
            return ;
        }else{  //子进程
            SKYLU_LOG_INFO(G_LOGGER)<<"Rewritten AOF Start.";
            int fd = open(aof_written_file_name,O_CREAT|O_RDWR|O_TRUNC);
            for(auto it: m_maps){
                for(auto item : it){
                    std::stringstream ss;
                    ss<<"set "<<item->getKey().getData()<<" 0 "<<item->getFlag()<<" "<<item->getValueLen()-2<<"\r\n"<<item->getValue();
                    write(fd,ss.str().data(),ss.str().size());
                    SKYLU_LOG_INFO(G_LOGGER)<<"Item :"<<ss.str()<<"Load Rewritten File.";
                }
            }
            SKYLU_LOG_INFO(G_LOGGER)<<"AOF bak has been written.";
            exit(0);
        }

    }




}