#include "EventLoopThread.hh"
#include "Log.hh"

// therad support
#include <pthread.h>

using namespace net;
using namespace base;
using namespace std;

void* EventLoopThread::loopThreadRoutine(void* args)
{
    // FIXME: unsafe cast
    EventLoopThread* obj = (EventLoopThread*)args;
    EventLoop* loop = new EventLoop();
    obj->_loop = loop;
    obj->_started = true;

    // broadcast thread is ready
    obj->_loopReadyCond.Broadcast();

    // infinite loop here 
    loop->loop();

    return NULL;
}

EventLoopThread::EventLoopThread() : _loop(NULL), _started(false)
{
    // reset class member
    bzero(&_thread, sizeof(_thread));
}

EventLoopThread::~EventLoopThread()
{
    if (_started) {
        stop();
    }
}

void EventLoopThread::start()
{
    if (_started) {
        return;
    }

    if (pthread_create(&this->_thread, NULL, &EventLoopThread::loopThreadRoutine, (void*)this) != 0) { // pass this pointer to static function
        LOG_ERROR("pthread_create failed!");
        perror("pthread_create");
        return;
    }

    // block here until thread is fully created
    while (_loop == NULL) {
        _loopReadyCond.Wait();
    }

#ifdef __linux__ // linux use unsigned long int for pthread_t, other plat. may not
    LOG_INFO("EventLoopThread object start OK! pthread_t: [0x%lx]", _thread);
#else
    LOG_INFO("EventLoopThread object start OK!");
#endif

    // _started = true;
}

void EventLoopThread::stop()
{
    if (this->_loop == NULL) {
        return;
    }

    // set stop flag
    _loop->stop();

    // wait thread to die
    this->join();
}

void EventLoopThread::join()
{
    if (this->_loop == NULL) {
        LOG_WARN("call EventLoopThread::join() while _loop is NULL");
        return;
    }

    pthread_join(this->_thread, NULL);
}

EventLoop* EventLoopThread::getLoop()
{
    return _loop;
}