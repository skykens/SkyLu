//
// Created by jimlu on 2020/5/11.
//

/*************************************************************************
	> File Name: Daemon.h
	> Author:
	> Mail:
	> Created Time: 2020年05月11日 星期日 13时33分30秒
    守护进程 捕捉信号
 ************************************************************************/
#ifndef KV_DAEMON_H
#define KV_DAEMON_H

#include <functional>
#include <unordered_map>
#include <csignal>
namespace skylu{


    /*
     * @brief 守护进程
     *
     */
class Daemon {
public:
    /*
     * @brief 启动守护进程
     */

    static void  start();


};

static std::unordered_map<int,std::function<void()> > maps;

/*
 *
 * 信号捕捉
 */
class Signal{
public:
    /*
     * @brief 捕捉信号
     */
    static void hook(int sig,std::function<void()> fun){
        maps[sig] = fun;
        ::signal(sig,hanlder);
    }
private:
    /*
     * @brief 实际signal的函数
     */
    static void hanlder(int sig){
      if(maps[sig]) { maps[sig](); }
    }
private:

    /*
     * @brief 存储每个信号对应的function
     */


};







};

#endif //KV_DAEMON_H
