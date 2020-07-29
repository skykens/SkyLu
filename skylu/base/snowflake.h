//
// Created by jimlu on 2020/7/25.
//

#ifndef SNOWFLAKE_H
#define SNOWFLAKE_H

#include <chrono>
#include <exception>
#include <sstream>
#include "mutex.h"
#include "log.h"
#include "nocopyable.h"
#include "singleton.h"
// #define SNOWFLAKE_ID_WORKER_NO_LOCK

namespace skylu {

/**
 * @brief 分布式id生成类
 * https://segmentfault.com/a/1190000011282426
 * https://github.com/twitter/snowflake/blob/snowflake-2010/src/main/scala/com/twitter/service/snowflake/IdWorker.scala
 *
 * 64bit id: 0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000
 *           ||                                                           ||     ||     |  |              |
 *           |└---------------------------时间戳--------------------------┘└中心-┘└机器-┘  └----序列号----┘
 *           |
 *         不用
 * SnowFlake的优点: 整体上按照时间自增排序, 并且整个分布式系统内不会产生ID碰撞(由数据中心ID和机器ID作区分), 并且效率较高, 经测试, SnowFlake每秒能够产生26万ID左右.
 */
class SnowflakeIdWorker :Nocopyable {

  // 实现单例
  friend class Singleton<SnowflakeIdWorker>;

public:

  typedef Singleton<SnowflakeIdWorker>  IdWorker;

  void setWorkerId(uint32_t workerId) {
    m_workerId = workerId;
  }

  void setDatacenterId(uint32_t datacenterId) {
    m_datacenterId = datacenterId;
  }
  uint64_t getId() {
    return nextId();
  }

  /**
   * 获得下一个ID (该方法是线程安全的)
   *
   * @return SnowflakeId
   */
  uint64_t nextId() {
    Mutex::Lock lock(m_mutex);
    uint64_t timestamp{ 0 };

    timestamp = timeGen();

    // 如果当前时间小于上一次ID生成的时间戳，说明系统时钟回退过这个时候应当抛出异常
    if (timestamp < m_lastTimestamp) {
      std::ostringstream s;
      s << "clock moved backwards.  Refusing to generate id for " << m_lastTimestamp - timestamp << " milliseconds";
      throw std::exception(std::runtime_error(s.str()));
    }

    if (m_lastTimestamp == timestamp) {
      // 如果是同一时间生成的，则进行毫秒内序列
      m_sequence = (m_sequence + 1) & sequenceMask;
      if (0 == m_sequence) {
        // 毫秒内序列溢出, 阻塞到下一个毫秒,获得新的时间戳
        timestamp = tilNextMillis(m_lastTimestamp);
      }
    } else {
      m_sequence = 0;
    }

    m_lastTimestamp = timestamp;

    // 移位并通过或运算拼到一起组成64位的ID
    return ((timestamp - twepoch) << timestampLeftShift)
           | (m_datacenterId << datacenterIdShift)
           | (m_workerId << workerIdShift)
           | m_sequence;
  }

protected:
  SnowflakeIdWorker()  {
    srand(time(NULL));
    m_workerId = rand() % maxWorkerId;
    m_datacenterId = rand() % maxDatacenterId;
    m_sequence = 0;

  }

  /**
   * 返回以毫秒为单位的当前时间
   *
   * @return 当前时间(毫秒)
   */
  uint64_t timeGen() const {
    auto t = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now());
    return t.time_since_epoch().count();
  }

  /**
   * 阻塞到下一个毫秒，直到获得新的时间戳
   *
   * @param lastTimestamp 上次生成ID的时间截
   * @return 当前时间戳
   */
  uint64_t tilNextMillis(uint64_t lastTimestamp) const {
    uint64_t timestamp = timeGen();
    while (timestamp <= lastTimestamp) {
      timestamp = timeGen();
    }
    return timestamp;
  }

private:

  Mutex m_mutex;

  /**
   * 开始时间截 (2018-01-01 00:00:00.000)
   */
  const uint64_t twepoch = 1514736000000;

  /**
   * 机器id所占的位数
   */
  const uint32_t workerIdBits = 5;

  /**
   * 数据中心id所占的位数
   */
  const uint32_t datacenterIdBits = 5;

  /**
   * 序列所占的位数
   */
  const uint32_t sequenceBits = 12;

  /**
   * 机器ID向左移12位
   */
  const uint32_t workerIdShift = sequenceBits;

  /**
   * 数据标识id向左移17位
   */
  const uint32_t datacenterIdShift = workerIdShift + workerIdBits;

  /**
   * 时间截向左移22位
   */
  const uint32_t timestampLeftShift = datacenterIdShift + datacenterIdBits;

  /**
   * 支持的最大机器id，结果是31
   */
  const uint32_t maxWorkerId = -1 ^ (-1 << workerIdBits);

  /**
   * 支持的最大数据中心id，结果是31
   */
  const uint32_t maxDatacenterId = -1 ^ (-1 << datacenterIdBits);

  /**
   * 生成序列的掩码，这里为4095
   */
  const uint32_t sequenceMask = -1 ^ (-1 << sequenceBits);

  /**
   * 工作机器id(0~31)
   */
  uint32_t m_workerId;

  /**
   * 数据中心id(0~31)
   */
  uint32_t m_datacenterId;

  /**
   * 毫秒内序列(0~4095)
   */
  uint32_t m_sequence{ 0 };

  /**
   * 上次生成ID的时间截
   */
  uint64_t m_lastTimestamp{ 0 };

};

}

#endif
