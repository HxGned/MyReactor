#include <assert.h>

#include "EventLoop.hh"
#include "Poller.hh"
#include "EPollPoller.hh"
#include "PollPoller.hh"
#include "Channel.hh"
#include "Log.hh"

#include <stdint.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <fcntl.h>

using namespace std;
using namespace base;
using namespace net;

#define USE_BUILTIN_ASSERT

const int kPollTimeoutMs = 3 * 1000;

EventLoop::EventLoop() : _looping(false), _quit(false), \
    _tid(CurrentThread::GetThreadId()), _eventFd(CreateEventFd()), _eventFdChannel(this, _eventFd), \
    _timerQueue(this)
{
    LOG_INFO("EventLoop object constructed, tid:[%d]", this->_tid);
#ifdef MY_REACTOR_USE_EPOLL
    _poller = new EPollPoller(this);    // use EPollPoller as default
#else
    _poller = new PollPoller(this);    // use PollPoller
#endif

    _eventFdChannel.SetReadCallback(bind(&EventLoop::HandleReadEventFd, this));
    _eventFdChannel.EnableRead();
}

EventLoop::~EventLoop()
{
    _eventFdChannel.DisableAll();
    _eventFdChannel.Remove();
    LOG_INFO("EventLoop object destructed!");
}

void EventLoop::loop()
{
    LOG_INFO("loop begin");
    if (_looping) {
        return;
    }

    // set flags
    _looping = true;
    _quit = false;

    while (_looping) {
        // 1. handle poller event
        _activeChannels.clear();
        Timestamp pollRetTime = _poller->Poll(kPollTimeoutMs, &_activeChannels);
        LOG_DEBUG("pollRetTime: [%s], activeChannels' count: [%lu]", pollRetTime.ConvertToString().c_str(), _activeChannels.size());
        // LOG_DEBUG("activeChannels' count: [%lu]", _activeChannels.size());
        // 1. handle returned event
        for (ChannelList::iterator it = _activeChannels.begin(); it != _activeChannels.end(); ++it) {
            (*it)->HandleEvent(pollRetTime);
        }

        // 2. handle pending functors
        _mutex.Lock();
        vector<Functor> tmpPendingFunctors;
        tmpPendingFunctors.swap(this->_pendingFunctors);
        _mutex.Unlock();

        if (!tmpPendingFunctors.empty()) {
            for (auto it = tmpPendingFunctors.begin(); it != tmpPendingFunctors.end(); ) {
                (*(it))();  // call functor
                it = tmpPendingFunctors.erase(it);    // then remove it from task queue
            }
        }

        // 3. handle cron(timer) job
        this->_timerQueue.runTimers();
    }
    
    LOG_INFO("loop end");
}

void EventLoop::stop()
{
    _looping = false;
    _quit = true;
}

// run callback in eventloop
void EventLoop::RunInLoopThread(const Functor& func)
{
    pid_t currentTid = CurrentThread::GetThreadId();
    
    // caller just in this loop thread
    if (currentTid == _tid) {
        func();
    } else {
        QueueInLoop(func);
        WakeUp();
    }
}

void EventLoop::QueueInLoop(const Functor& func)
{
    // TODO: make loop return from 'Poll' immediately, we need a eventFd to get this done

    _mutex.Lock();
    _pendingFunctors.emplace_back(func);
    _mutex.Unlock();
}

bool EventLoop::UpdateChannel(Channel* channel)
{
    return this->_poller->UpdateChannel(channel);
}

bool EventLoop::RemoveChannel(Channel* ch)
{
    return _poller->RemoveChannel(ch);
}

void EventLoop::AssertInEventLoopThread(void)
{
#ifdef USE_BUILTIN_ASSERT
    assert(CurrentThread::GetThreadId() == this->_tid);
#else
    if (CurrentThread::GetThreadId() != this->_tid) {
        LOG_FATAL("AssertInEventLoopThread!");
    }
#endif
}

bool EventLoop::InEventLoopThread(void)
{
    return CurrentThread::GetThreadId() == this->_tid;
}

int EventLoop::CreateEventFd()
{
    int fd = 0;
    fd = eventfd(1, O_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) {
        perror("eventfd");
        LOG_FATAL("CreateEventFd failed!");
    }
    LOG_DEBUG("create eventfd ok, fd: [%d]", fd);
    return fd;
}

void EventLoop::HandleReadEventFd()
{
    // LOG_INFO("HandleReadEventFd begin");
    uint64_t i = 0;

    int ret = read(this->_eventFd, (void *)&i, sizeof(i));
    if (ret < 0) {
        perror("read");
        LOG_WARN("HandleReadEventFd failed!");
    }
    // LOG_INFO("HandleReadEventFd end");
}

void EventLoop::WakeUp()
{
    uint64_t i = 0;

    int ret = write(_eventFd, (void *)&i, sizeof(i));
    if (ret < 0) {
        perror("write");
        LOG_WARN("WakeUp failed!");
    }
}

// timer related
TimerId EventLoop::runAt(const TimerCallback& cb, Timestamp when)
{
    return this->_timerQueue.addTimer(cb, when, 0, 0);
}

TimerId EventLoop::runEvery(const TimerCallback& cb, Timestamp when, int intervalMs)
{
    return this->_timerQueue.addTimer(cb, when, intervalMs, -1);
}

void EventLoop::deleteAllTimers(void)
{
    this->_timerQueue.deleteAllTimers();
}
