#include "ThreadModel.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include <time.h>

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
    absTime.tv_sec = time(NULL) + nMinSeconds / 1000;
    absTime.tv_nsec = (nMinSeconds % 1000) * 1000000;
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
    m_bRuning = false;
}

dakuang::CThreadModel::~CThreadModel()
{
}

// 注册事件回调
void dakuang::CThreadModel::registerEventCB(const std::string& strEvent, dakuang::CThreadModel::CALLBACK_FUNC_t func)
{
    m_mapEventCB.insert( std::make_pair(strEvent, func) );
}

// 取消注册事件回调
void dakuang::CThreadModel::unRegisterEventCB(const std::string& strEvent)
{
    m_mapEventCB.erase(strEvent);
}

// 发射指定事件
void dakuang::CThreadModel::emitEvent(const std::string& strEvent, void* pData)
{
    m_eventQueue.pushEvent(strEvent, pData);
}

// 启动线程
bool dakuang::CThreadModel::start()
{
    if (m_bRuning)
        return true;

    if (pthread_create(&m_tidWorking, NULL, __threadbody, this) != 0)
        return false;

    m_bRuning = true;
    return true;
}

// 退出线程
void dakuang::CThreadModel::quit()
{
    if (!m_bRuning)
        return;

    m_eventQueue.pushEvent("quit");
}

void dakuang::CThreadModel::wait()
{
    if (!m_bRuning)
        return;

    pthread_join(m_tidWorking, NULL);

    m_bRuning = false;
}

// 静态线程函数
void *dakuang::CThreadModel::__threadbody(void *_lp)
{
    dakuang::CThreadModel* pThis = (dakuang::CThreadModel*)_lp;

    pThis->__threadloop();
    return NULL;
}

// 工作线程主体
void dakuang::CThreadModel::__threadloop()
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
    MAP_EVENT_CB_t::iterator iter = m_mapEventCB.find(strEvent);
    if (iter != m_mapEventCB.end())
    {
        iter->second(pData);
    }
}

// 设置定时回调
dakuang::CThreadTimer::CThreadTimer()
{
    m_nMinSeconds = 10000;
    m_bRunning = false;
}

void dakuang::CThreadTimer::setTimerCB(dakuang::CThreadTimer::CALLBACK_FUNC_t cb)
{
    m_cbTimer = cb;
}

// 启动/停止定时器
bool dakuang::CThreadTimer::start(int nMinSeconds)
{
    m_nMinSeconds = nMinSeconds;
    m_bRunning = true;

    if (pthread_create(&m_tidWorking, NULL, __threadbody, this) != 0)
        return false;

    return true;
}

void dakuang::CThreadTimer::stop()
{
    m_bRunning = false;
    pthread_join(m_tidWorking, NULL);
}

// 静态线程函数
void *dakuang::CThreadTimer::__threadbody(void *_lp)
{
    dakuang::CThreadTimer* pThis = (dakuang::CThreadTimer*)_lp;

    pThis->__threadloop();
    return NULL;
}

// 工作线程主体
void dakuang::CThreadTimer::__threadloop()
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
        usleep(100000);
#else
        Sleep(100);
#endif
    }
}
