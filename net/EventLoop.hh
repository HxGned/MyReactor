#ifndef __EVENT_LOOP_HH__
#define __EVENT_LOOP_HH__

#include "NonCopyable.hh"
#include "Mutex.hh"
#include "CurrentThread.hh"
#include "Channel.hh"
#include "TimerQueue.hh"

#include <functional>
#include <vector>
#include <atomic>

#include <unistd.h>
#include <sys/syscall.h>

using std::vector;
using std::function;
using base::NonCopyable;

namespace net {

class Poller;
class Channel;

typedef function<void (void)> Functor;
// channel list
typedef vector<Channel *> ChannelList;

class EventLoop : public NonCopyable {
public:
    EventLoop();
    ~EventLoop();

    bool UpdateChannel(Channel* channel);
    bool RemoveChannel(Channel* ch);
    // 开始事件循环
    void loop();
    // 结束事件循环
    void stop();
    // 在EventLoop中运行
    void RunInLoopThread(const Functor& func);
    // 加入待执行回调队列
    void QueueInLoop(const Functor& func);

    void AssertInEventLoopThread(void);
    bool InEventLoopThread(void);

public:
    static int CreateEventFd();
private:
    void HandleReadEventFd();
    void WakeUp();

public:
    // Timer related stuff
    TimerId runAt(const TimerCallback& cb, Timestamp when);
    TimerId runEvery(const TimerCallback& cb, Timestamp when, int intervalMs);
    // trun off timers
    void deleteAllTimers(void);

private:
    Poller *_poller;    // multiplexer
    vector<Functor> _pendingFunctors;   // pending jobs to run in eventloop thread

    // 状态相关
    std::atomic_bool _looping;
    std::atomic_bool _quit;

    // 互斥锁
    base::Mutex _mutex;

    // 本loop的tid, only Linux
    const pid_t _tid;

    // active channels
    ChannelList _activeChannels;

private:
    int _eventFd;
    Channel _eventFdChannel;

private:
    TimerQueue _timerQueue;
};

};

#endif