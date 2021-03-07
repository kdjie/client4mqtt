#ifndef CTHREADMODEL_H
#define CTHREADMODEL_H

// 一个通用的线程模型实现 =>

#include <string>
#include <map>
#include <list>
#include <functional>
#include <pthread.h>
#include <semaphore.h>

namespace dakuang
{
    // 事件队列
    class CEventQueue
    {
        struct SEvent
        {
            std::string strEvent;
            void* pData;

            SEvent(const std::string& _strEvent = "", void* _pData = NULL) : strEvent(_strEvent), pData(_pData) {}
        };

    public:
        CEventQueue();
        virtual ~CEventQueue();

        void pushEvent(const std::string& strEvent, void* pData = NULL);
        bool popEvent(std::string& strEvent, void** _pData, int nMinSeconds);

    private:
        typedef std::list<SEvent> LIST_EVENT_t;
        LIST_EVENT_t m_listEvent;

        pthread_mutex_t m_mutex;
        sem_t m_sem;
    };

    // 线程模型
    class CThreadModel
    {
        typedef std::function<void(void*)> CALLBACK_FUNC_t;
        typedef std::map<std::string, CALLBACK_FUNC_t> MAP_EVENT_CB_t;

    public:
        CThreadModel();
        virtual ~CThreadModel();

        // 注册事件回调
        void registerEventCB(const std::string& strEvent, CALLBACK_FUNC_t func);
        // 取消注册事件回调
        void unRegisterEventCB(const std::string& strEvent);

        // 发射指定事件
        void emitEvent(const std::string& strEvent, void* pData = NULL);

        // 启动线程
        bool start();
        // 退出线程
        void quit();
        void wait();

    private:
        // 静态线程函数
        static void* __threadbody(void* _lp);

        // 工作线程主体
        void __threadloop();

        // 路由事件
        void __routerEvent(const std::string& strEvent, void* pData);

    private:
        // 事件回调表
        MAP_EVENT_CB_t m_mapEventCB;

        // 事件队列
        CEventQueue m_eventQueue;

        // 工作线程
        pthread_t m_tidWorking;
        bool m_bRuning;
    };

    // 一个基于线程的定时器
    class CThreadTimer
    {
    public:
        typedef std::function<void()> CALLBACK_FUNC_t;

        CThreadTimer();
        virtual ~CThreadTimer() {}

        // 设置定时回调
        void setTimerCB(CALLBACK_FUNC_t cb);

        // 启动/停止定时器
        bool start(int nMinSeconds);
        void stop();

    private:
        // 静态线程函数
        static void* __threadbody(void* _lp);

        // 工作线程主体
        void __threadloop();

        // 等待一个周期
        void __delayInterval();

    private:
        CALLBACK_FUNC_t m_cbTimer;
        int m_nMinSeconds;

        // 工作线程
        pthread_t m_tidWorking;
        bool m_bRunning;
    };

}

#endif // CTHREADMODEL_H
