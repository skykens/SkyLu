/*************************************************************************
	> File Name: singleton.h
	> Author: 
	> Mail: 
	> Created Time: 2020年04月17日 星期五 10时57分21秒
    单例模式的封装
 ************************************************************************/

#ifndef _SINGLETON_H
#define _SINGLETON_H
#include <memory>
namespace skylu{

/*
 * 单例模式的封装 
 * T ： 类型
 */
template<class T>
class Singleton{
public: 
    //返回单例裸指针
    static T * GetInstance(){
        static T v;
        return &v;
    }
};
template<class T>
class SingletonPtr{
public:
    static std::shared_ptr<T> GetInstance(){
        static std::shared_ptr<T> v(new T);
        return v;
    }


        
};

    
        
}




#endif
