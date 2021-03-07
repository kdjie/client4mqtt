#ifndef CCLIENT4MQTT_H
#define CCLIENT4MQTT_H

// 一个基于paho.mqtt.c库实现的MQTT客户端封装 =》

#include <string>
#include <vector>

#include "MQTTClient.h"
#include "ThreadModel.h"

namespace dakuang
{

    class CClient4MQTT
    {
        // 内部发布消息事件参数结构
        struct STopicMessage
        {
            std::string strTopic;
            std::string strMessage;

            STopicMessage(const std::string& _strTopic, const std::string& _strMessage) : strTopic(_strTopic), strMessage(_strMessage) {}
        };

    public:
        // MQTT消息到达回调
        typedef std::function<void(const std::string&, const std::string&)> CALLBACK_MessageArrived;

        CClient4MQTT();
        virtual ~CClient4MQTT();

        // 设置客户端ID
        void setClientID(const std::string& strClientID);

        // 设置服务器地址
        void setServerAddress(const std::string& strAddress = "tcp://127.0.0.1:1883");

        // 设置订阅的主题
        void cleanSubTopics();
        void addSubTopic(const std::string& strTopic);

        // 设置MQTT消息到达回调
        void setMessageArrivedCB(CALLBACK_MessageArrived cb);

        // 启动客户端
        bool start();
        // 停止客户端
        void stop();

        // 发布消息
        void publish(const std::string& strTopic, const std::string& strMessage);

        // 发送定时器脉冲事件
        void emitTimerEvent();

    private:

        // 标准事件实现 =》
        void __threadStart(void* pData);
        void __threadExit(void* pData);
        void __threadTimer(void* pData);

        // 发布消息事件实现
        void __threadPublish(void* pData);

        // 连接丢失事件实现
        void __threadConnList(void* pData);

        // 连接服务端
        bool __tryConnecServer();

        // 释放连接
        void __freeConnection();

        // 订阅主题
        void __subscribing();

        // 发布消息
        bool __publishMsg(const std::string& strTopic, const std::string& strMessage);

        // 来自MQTT的连接断开回调
        static void __connLostFromMQTT(void* pContext, char* pCause);
        // 来自MQTT的消息到达回调
        static int __msgArrvdFromMQTT(void* pContext, char* pTopicName, int nTopicLen, MQTTClient_message* pMessage);

    private:
        // 线程模型
        CThreadModel m_thread;

        // MQTTClient相关
        MQTTClient m_client;
        std::string m_strClientID;
        std::string m_strServerAddress;
        std::vector<std::string> m_vecSubTopics;

        // MQTT接收消息回调
        CALLBACK_MessageArrived m_cbMessageArrived;

        // 连接状态
        bool m_bConnected;
    };

}

#endif // CCLIENT4MQTT_H
