/*************************************************************************
	> File Name: log.h
	> Author: 
	> Mail: 
	> Created Time: 2020年04月03日 星期五 11时47分43秒
 ************************************************************************/

#ifndef _LOG_H
#define _LOG_H
#include <string> 
#include <stdint.h>
#include <memory>
#include <fstream>
#include <sstream>
#include <ostream>
#include <list>
#include <map>
#include <vector>
#include <stdarg.h>
#include "util.h"
#include "singleton.h"
#include "thread.h"
#include "mutex.h"


#define SKYLU_LOG_LEVEL(logger,level)  \
    if(logger->getLevel()<=level) \
        skylu::LogEventWrap(skylu::LogEvent::ptr(new skylu::LogEvent(logger,level,\
                        __FILE__,__LINE__,0,skylu::getThreadId(),\
                    skylu::getFiberId(),time(0),skylu::Thread::GetName()))).getSS()

    #define SKYLU_LOG_DEBUG(logger) SKYLU_LOG_LEVEL(logger,skylu::LogLevel::DEBUG)

    #define SKYLU_LOG_INFO(logger) SKYLU_LOG_LEVEL(logger,skylu::LogLevel::INFO)

    #define SKYLU_LOG_WARN(logger) SKYLU_LOG_LEVEL(logger,skylu::LogLevel::WARN)
    
    #define SKYLU_LOG_FATAL(logger) SKYLU_LOG_LEVEL(logger,skylu::LogLevel::FATAL)

    #define SKYLU_LOG_ERROR(logger) SKYLU_LOG_LEVEL(logger,skylu::LogLevel::ERROR) 

    #define SKYLU_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLevel() <= level) \
        skylu::LogEventWrap(skylu::LogEvent::ptr(new skylu::LogEvent(logger, level, \
                        __FILE__, __LINE__, 0, skylu::getThreadId(),\
                skylu::getFiberId(), time(0), skylu::Thread::GetName()))).getEvent()->format(fmt, __VA_ARGS__)

    #define SKYLU_LOG_FMT_DEBUG(logger,fmt,...) SKYLU_LOG_FMT_LEVEL(logger,skylu::LogLevel::DEBUG,fmt,__VA_ARGS__)
    
    #define SKYLU_LOG_FMT_INFO(logger,fmt,...) SKYLU_LOG_FMT_LEVEL(logger,skylu::LogLevel::INFO,fmt,__VA_ARGS__)
    #define SKYLU_LOG_FMT_WARN(logger,fmt,...) SKYLU_LOG_FMT_LEVEL(logger,skylu::LogLevel::WARN,fmt,__VA_ARGS__)
    #define SKYLU_LOG_FMT_ERROR(logger,fmt,...) SKYLU_LOG_FMT_LEVEL(logger,skylu::LogLevel::ERROR,fmt,__VA_ARGS__)
    #define SKYLU_LOG_FMT_FATAL(logger,fmt,...) SKYLU_LOG_FMT_FATAL(logger,skylu::LogLevel::WARN,fmt,__VA_ARGS__)

    
    #define SKYLU_LOG_ROOT() skylu::LoggerMgr::GetInstance()->getRoot()
    
    #define SKYLU_LOG_NAME(name) skylu::LoggerMgr::GetInstance()->getLogger(name)

    #define G_LOGGER SKYLU_LOG_NAME("system")
namespace skylu{

class LoggerManager;
class Logger;


// 日志级别
class LogLevel{
public : 
    enum Level{
        UNKNOW = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };
    static const char * ToString(LogLevel::Level level);
    static Level FromString(const std::string &str);

        
};
    //日志事件
class LogEvent{
    public:
        typedef std::shared_ptr<LogEvent> ptr;
        LogEvent(std::shared_ptr<Logger> logger,
                LogLevel::Level level,const char* file, int32_t line, uint32_t elapse
            ,uint32_t thread_id, uint32_t fiber_id, uint64_t time
            ,const std::string& thread_name);
        const char *getFile()const {return m_file;}
        int32_t getLine() const {return m_line;}
        uint32_t getElapse() const {return m_elapse;}
        uint32_t getThreadId() const {return m_threadId;}
        uint32_t getFiberId() const {return m_fiberId;}
        uint64_t getTime() const {return m_time;}
        const std::string getContent() const {return m_ss.str();}
        
        std::shared_ptr<Logger> getLogger() const { return m_logger;}
        LogLevel::Level getLevel() const { return m_level;}

    std::stringstream& getSS() { return m_ss;}

    void format(const char* fmt, ...);

    void format(const char* fmt, va_list al);

    private:
        
        /// 文件名
    const char* m_file = nullptr;
    /// 行号
    int32_t m_line = 0;
    /// 程序启动开始到现在的毫秒数
    uint32_t m_elapse = 0;
    /// 线程ID
    uint32_t m_threadId = 0;
    /// 协程ID
    uint32_t m_fiberId = 0;
    /// 时间戳
    uint64_t m_time = 0;
    /// 线程名称
    std::string m_threadName;
    /// 日志内容流
    std::stringstream m_ss;
    /// 日志器
    std::shared_ptr<Logger> m_logger;
    /// 日志等级
    LogLevel::Level m_level;
    
};

class LogEventWrap{
public:
    LogEventWrap(LogEvent::ptr e);

    ~LogEventWrap();

    LogEvent::ptr getEvent()const {return m_event;}

    std::stringstream& getSS(){return m_event->getSS();}
private:
    LogEvent::ptr m_event;

};

//日志格式器

class LogFormatter{
public:
    typedef std::shared_ptr<LogFormatter> ptr;

    LogFormatter(const std::string & pattern);
    const std::string getPattern()const {return m_pattern;} 
    std::ostream& format(std::ostream&ofs,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event);
    //%t  %thread_id %m%n
    std::string format(std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event);

public:
    class FormatItem{
    public: 
        typedef std::shared_ptr<FormatItem> ptr;
        FormatItem(const std::string & str = ""){}
        virtual ~FormatItem(){}
        virtual  void format(std::ostream &os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) = 0;

    };
    void init();
    bool isError() const {return m_error;}
private:
    bool m_error=false;
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;

};


//日志输出地

class LogAppender{
public: 
    friend class Logger;
    typedef skylu::Mutex MutexType;
    typedef std::shared_ptr<LogAppender> ptr;
    virtual ~LogAppender(){}
    virtual void log(std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event)=0 ;
    void setFormatter(LogFormatter::ptr val){m_formatter = val;}
    LogFormatter::ptr getFormatter()const {return m_formatter;}
protected:
    LogLevel::Level m_level=LogLevel::DEBUG;
    bool m_hasFormatter=false;
    MutexType m_mutex;
    LogFormatter::ptr m_formatter;
};

//日志器

class Logger : public std::enable_shared_from_this<Logger> {
public: 

    friend class LoggerManager;
    typedef std::shared_ptr<Logger> ptr;
    typedef skylu::Mutex MutexType;
    Logger(const std::string & name = "root");
    void log(LogLevel::Level level ,LogEvent::ptr event);

    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppenders();


    LogLevel::Level getLevel()const {return m_level;}
    void setLevel(LogLevel::Level val){m_level=val;}


    const std::string& getName()const {return m_name;}

    void setFormatter(LogFormatter::ptr val);
    void setFormatter(const std::string& val);
    LogFormatter::ptr getFormatter();
private:
    std::string m_name;
    LogLevel::Level m_level; //日志级别
    std::list<LogAppender::ptr> m_appenders; //Appender 集合
    LogFormatter::ptr m_formatter;
    MutexType m_mutex;
    Logger::ptr m_root;
};

//输出到控制台的appender
//

class StdoutLogAppender : public LogAppender{
public: 
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    void log(Logger::ptr logger,LogLevel::Level level, LogEvent::ptr event) override;
    
    
    
};


//输出到文件的appender

class FileLogAppender : public LogAppender{
public: 
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string & filename);
    void log(Logger::ptr logger,LogLevel::Level level,LogEvent::ptr event) override;

    //重新打开文件 文件打开成功返回true
     
    bool reopen();
private:
    uint64_t m_lastTime = 0;
    std::string m_filename;
    std::ofstream m_filestream;
};


class LoggerManager{
public:
    typedef skylu::Mutex MutexType; //自旋锁
    LoggerManager();

    Logger::ptr getLogger(const std::string& name);

    void init();

    Logger::ptr getRoot()const {return m_root;}

private:
    MutexType m_mutex;
    std::map<std::string,Logger::ptr> m_loggers;
    Logger::ptr m_root;

    
};

//日志管理其单例模式
typedef skylu::Singleton<LoggerManager> LoggerMgr;
}
#endif
