

/*************************************************************************
	> File Name: nocopyable.h
	> Author: 
	> Mail: 
	> Created Time: 2020年04月13日 星期一 10时45分40秒
    用于屏蔽默认拷贝构造函数与=运算符
 ************************************************************************/


#ifndef _NOCOPYABLE_H
#define _NOCOPYABLE_H
class Nocopyable{
protected:
    Nocopyable() = default;
    ~Nocopyable() = default;
    Nocopyable(const Nocopyable &) = delete;
    Nocopyable & operator=(const Nocopyable&) = delete;

};
#endif
