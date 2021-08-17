#include "ThreadModel.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/time.h>
#endif

#ifdef __windows__
#include <windows.h>
#include <sys/timeb.h>
#endif

dakuang::CEventQueue::CEventQueue()
{
    pthread_mutex_init(&m_mutex, NULL);
    sem_init(&m_sem, 0, 0);
}

dakuang::CEventQueue::~CEventQueue()
{
    sem_destroy(&m_sem);
    pthread_mutex_destroy(&m_mutex);
}

void dakuang::CEventQueue::pushEvent(const std::string& strEvent, void* pData)
{
    pthread_mutex_lock(&m_mutex);
    m_listEvent.push_back( SEvent(strEvent, pData) );
    pthread_mutex_unlock(&m_mutex);

    sem_post(&m_sem);
}

bool dakuang::CEventQueue::popEvent(std::string& strEvent, void** _pData, int nMinSeconds)
{
    struct timespec absTime;

#ifdef __linux__
    struct timeval tv;
    gettimeofday(&tv, NULL);
    absTime.tv_sec = tv.tv_sec + nMinSeconds / 1000;
    absTime.tv_nsec = tv.tv_usec * 1000 + (nMinSeconds % 1000) * 1000000;
#endif

#ifdef __windows__
    timeb tb;
    ftime(&tb);
    absTime.tv_sec = tb.time + nMinSeconds / 1000;
    absTime.tv_nsec = tb.millitm * 1000000 + (nMinSeconds % 1000) * 1000000;
#endif

    if (absTime.tv_nsec >= 1000000000)
    {
        absTime.tv_sec += 1;
        absTime.tv_nsec -= 1000000000;
    }
    sem_timedwait(&m_sem, &absTime);

    pthread_mutex_lock(&m_mutex);

    if (m_listEvent.empty())
    {
        pthread_mutex_unlock(&m_mutex);
        return false;
    }

    SEvent& stEvent = m_listEvent.front();
    strEvent = stEvent.strEvent;
    *_pData = stEvent.pData;
    m_listEvent.pop_front();

    pthread_mutex_unlock(&m_mutex);
    return true;
}

dakuang::CThreadModel::CThreadModel()
{
    pthread_mutex_init(&m_mutexEventCB, NULL);

    m_bRunning = false;
}

dakuang::CThreadModel::~CThreadModel()
{
    // 确保线程安全退出
    this->quit();
    this->wait();

    pthread_mutex_destroy(&m_mutexEventCB);
}

// 注册事件回调
void dakuang::CThreadModel::registerEventCB(const std::string& strEvent, dakuang::CThreadModel::CALLBACK_FUNC_t func)
{
    pthread_mutex_lock(&m_mutexEventCB);
    m_mapEventCB.insert( std::make_pair(strEvent, func) );
    pthread_mutex_unlock(&m_mutexEventCB);
}

// 取消注册事件回调
void dakuang::CThreadModel::unRegisterEventCB(const std::string& strEvent)
{
    pthread_mutex_lock(&m_mutexEventCB);
    m_mapEventCB.erase(strEvent);
    pthread_mutex_unlock(&m_mutexEventCB);
}

// 发射指定事件
void dakuang::CThreadModel::emitEvent(const std::string& strEvent, void* pData)
{
    m_eventQueue.pushEvent(strEvent, pData);
}

// 启动线程
bool dakuang::CThreadModel::start()
{
    if (m_bRunning)
        return true;

    if (pthread_create(&m_tidWorking, NULL, __threadBody, this) != 0)
        return false;

    m_bRunning = true;
    return true;
}

// 退出线程
void dakuang::CThreadModel::quit()
{
    if (!m_bRunning)
        return;

    m_eventQueue.pushEvent("quit");
}

void dakuang::CThreadModel::wait()
{
    if (!m_bRunning)
        return;

    pthread_join(m_tidWorking, NULL);

    m_bRunning = false;
}

// 静态线程函数
void *dakuang::CThreadModel::__threadBody(void *_lp)
{
    dakuang::CThreadModel* pThis = (dakuang::CThreadModel*)_lp;

    pThis->__threadLoop();
    return NULL;
}

// 工作线程主体
void dakuang::CThreadModel::__threadLoop()
{
    __routerEvent("start", NULL);

    while (true)
    {
        std::string strEvent = "";
        void* pData = NULL;
        if ( m_eventQueue.popEvent(strEvent, &pData, 10000) )
        {
            if (strEvent == "quit")
                break;

            __routerEvent(strEvent, pData);
        }
    }

    __routerEvent("exit", NULL);
}

// 路由事件
void dakuang::CThreadModel::__routerEvent(const std::string& strEvent, void* pData)
{
    pthread_mutex_lock(&m_mutexEventCB);

    MAP_EVENT_CB_t::iterator iter = m_mapEventCB.find(strEvent);
    if (iter != m_mapEventCB.end())
    {
        iter->second(pData);
    }

    pthread_mutex_unlock(&m_mutexEventCB);
}

// 设置定时回调
dakuang::CThreadTimer::CThreadTimer()
{
    m_nMinSeconds = 1000;
    m_bRunning = false;
}

void dakuang::CThreadTimer::setTimerCB(dakuang::CThreadTimer::CALLBACK_FUNC_t cb)
{
    m_cbTimer = cb;
}

// 启动/停止定时器
bool dakuang::CThreadTimer::start(int nMinSeconds)
{
    if (m_bRunning)
        return true;

    m_nMinSeconds = nMinSeconds;

    if (pthread_create(&m_tidWorking, NULL, __threadBody, this) != 0)
        return false;

    m_bRunning = true;
    return true;
}

void dakuang::CThreadTimer::stop()
{
    if (!m_bRunning)
        return;

    pthread_join(m_tidWorking, NULL);

    m_bRunning = false;
}

// 静态线程函数
void *dakuang::CThreadTimer::__threadBody(void *_lp)
{
    dakuang::CThreadTimer* pThis = (dakuang::CThreadTimer*)_lp;

    pThis->__threadLoop();
    return NULL;
}

// 工作线程主体
void dakuang::CThreadTimer::__threadLoop()
{
    while (m_bRunning)
    {
        __delayInterval();

        if (m_bRunning)
        {
            m_cbTimer();
        }
    }
}

// 等待一个周期
void dakuang::CThreadTimer::__delayInterval()
{
    int nTimes = m_nMinSeconds / 100;
    if (m_nMinSeconds % 100)
        nTimes++;

    for (int i = 0; i < nTimes && m_bRunning; ++i)
    {
#ifdef __linux__
        ::usleep(100000);
#endif

#ifdef __windows__
        ::Sleep(100);
#endif
    }
}
