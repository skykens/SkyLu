//
// Created by jimlu on 2020/5/22.
//

#ifndef HASHTEST_POLL_H
#define HASHTEST_POLL_H

#include "../base/nocopyable.h"
#include "timestamp.h"
#include <vector>
#include <map>


namespace skylu {
    class Channel;
    class EventLoop;
    /**
     * IO复用 的接口类
     */
    class Poll : Nocopyable {
      public:
        typedef std::vector<Channel*> ChannelList;

        explicit Poll(EventLoop *loop):ownerLoop(loop) {}
        virtual ~Poll() = default;
        /**
         * 主要的监听  如epoll_wait
         * @param timeoutMS
         * @param activeChannel  唤醒后存放活跃事件的通道
         * @return  时间戳 (有些函数需要打印时间来更好debug)
         */
        virtual Timestamp poll(int timeoutMS,ChannelList& activeChannel) = 0;
        /**
         * 将通道加入监听事件中
         * @param channel
         */
        virtual  void updateChannel(Channel * channel) = 0;
        /**
         * 将通道从监听事件中移除
         * @param channel
         */
        virtual void removeChannel(Channel *channel) = 0;

      protected:
      /**
       * 有事件到来时必须调用这个函数填充活跃的通道
       * @param numEvents
       * @param activeChannels  传出参数
       */
        virtual void fillActiveChannels(int numEvents,ChannelList& activeChannels)const = 0;

      protected:
        typedef std::map<int,Channel *> ChannelMap; // <fd , channel>
        EventLoop * ownerLoop;
        ChannelMap m_channels;






    };

}

#endif //HASHTEST_POLL_H
