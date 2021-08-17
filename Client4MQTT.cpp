#include "Client4MQTT.h"

#include <stdarg.h>
#include <string.h>

#ifdef __windows__
#include <windows.h>
#endif

void OutputDebugString(const char* sFormat, ...)
{
    char sLine[128] = {0};
    va_list va;
    va_start(va, sFormat);
    vsnprintf(sLine, sizeof(sLine)-1, sFormat, va);
    va_end(va);

    std::string strOutput(sLine, strlen(sLine));
    strOutput += "\n";

#ifdef __linux__
    ::printf("%s", strOutput.c_str());
#endif

#ifdef __windows__
    ::OutputDebugStringA(strOutput.c_str());
#endif
}

#define OUTPUT OutputDebugString


dakuang::CClient4MQTT::CClient4MQTT()
{
    m_strClientID = "";
    m_strServerAddress = "";
    m_bConnected = false;

    // 向线程模型注册事件路由
    m_thread.registerEventCB("start", std::bind(&CClient4MQTT::__threadStart, this, std::placeholders::_1));
    m_thread.registerEventCB("exit", std::bind(&CClient4MQTT::__threadExit, this, std::placeholders::_1));
    m_thread.registerEventCB("timer", std::bind(&CClient4MQTT::__threadTimer, this, std::placeholders::_1));
    m_thread.registerEventCB("publish", std::bind(&CClient4MQTT::__threadPublish, this, std::placeholders::_1));
    m_thread.registerEventCB("connlost", std::bind(&CClient4MQTT::__threadConnList, this, std::placeholders::_1));
}

dakuang::CClient4MQTT::~CClient4MQTT()
{
    // 向线程模型取消注册事件路由
    m_thread.unRegisterEventCB("start");
    m_thread.unRegisterEventCB("exit");
    m_thread.unRegisterEventCB("timer");
    m_thread.unRegisterEventCB("publish");
    m_thread.unRegisterEventCB("connlost");
}

// 设置客户端ID
void dakuang::CClient4MQTT::setClientID(const std::string& strClientID)
{
    m_strClientID = strClientID;
}

// 设置服务器地址
void dakuang::CClient4MQTT::setServerAddress(const std::string& strAddress)
{
    m_strServerAddress = strAddress;
}

// 设置订阅的主题
void dakuang::CClient4MQTT::cleanSubTopics()
{
    m_vecSubTopics.clear();
}

void dakuang::CClient4MQTT::addSubTopic(const std::string& strTopic)
{
    m_vecSubTopics.push_back(strTopic);
}

// 设置MQTT消息到达回调
void dakuang::CClient4MQTT::setMessageArrivedCB(dakuang::CClient4MQTT::CALLBACK_MessageArrived cb)
{
    m_cbMessageArrived = cb;
}

// 启动客户端
bool dakuang::CClient4MQTT::start()
{
    m_thread.start();
}

// 停止客户端
void dakuang::CClient4MQTT::stop()
{
    m_thread.quit();
    m_thread.wait();
}

// 发布消息
void dakuang::CClient4MQTT::publish(const std::string& strTopic, const std::string& strMessage)
{
    m_thread.emitEvent("publish", new STopicMessage(strTopic, strMessage));
}

// 发送定时器脉冲事件
void dakuang::CClient4MQTT::emitTimerEvent()
{
    m_thread.emitEvent("timer");
}

// 标准事件实现 =》

void dakuang::CClient4MQTT::__threadStart(void* pData)
{
    OUTPUT("@dakuang::CClient4MQTT::__threadStart call");
}

void dakuang::CClient4MQTT::__threadExit(void* pData)
{
    OUTPUT("@dakuang::CClient4MQTT::__threadExit call");
}

void dakuang::CClient4MQTT::__threadTimer(void* pData)
{
    OUTPUT("@dakuang::CClient4MQTT::__threadTimer call");

    // 尝试连接
    if (!m_bConnected)
    {
        if (__tryConnecServer())
        {
            // 订阅主题
            __subscribing();
        }
    }
}

// 发布消息事件实现
void dakuang::CClient4MQTT::__threadPublish(void* pData)
{
    OUTPUT("@dakuang::CClient4MQTT::__threadPublish call");

    STopicMessage* pTopicMessage = (STopicMessage*)pData;

    // 只在连接时发布消息
    if (m_bConnected)
    {
        __publishMsg(pTopicMessage->strTopic, pTopicMessage->strMessage);
    }

    delete pTopicMessage;
}

// 连接丢失事件实现
void dakuang::CClient4MQTT::__threadConnList(void* pData)
{
    OUTPUT("@dakuang::CClient4MQTT::__threadConnList call");

    m_bConnected = false;
    __freeConnection();
}

// 连接服务端
bool dakuang::CClient4MQTT::__tryConnecServer()
{
    MQTTClient_create(&m_client, m_strServerAddress.c_str(), m_strClientID.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL);

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.connectTimeout = 3;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;

    MQTTClient_setCallbacks(m_client, this, __connLostFromMQTT, __msgArrvdFromMQTT, NULL);

    int rc = MQTTClient_connect(m_client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        OUTPUT("@dakuang::CClient4MQTT::__tryConnecServer connect server:[%s] failed, rc:[%d]", m_strServerAddress.c_str(), rc);
        MQTTClient_destroy(&m_client);
        return false;
    }

    OUTPUT("@dakuang::CClient4MQTT::__tryConnecServer connect server:[%s] ok", m_strServerAddress.c_str());

    m_bConnected = true;
    return true;
}

// 释放连接
void dakuang::CClient4MQTT::__freeConnection()
{
    MQTTClient_disconnect(m_client, 3000);
    MQTTClient_destroy(&m_client);
}

// 订阅主题
void dakuang::CClient4MQTT::__subscribing()
{
    for (std::vector<std::string>::const_iterator c_iter = m_vecSubTopics.begin(); c_iter != m_vecSubTopics.end(); ++c_iter)
    {
        int rc = MQTTClient_subscribe(m_client, c_iter->c_str(), 0);
        if (rc != MQTTCLIENT_SUCCESS)
        {
            OUTPUT("@dakuang::CClient4MQTT::__subscribing subscribe topic:[%s] failed, rc:[%d]", c_iter->c_str(), rc);
            continue;
        }

        OUTPUT("@dakuang::CClient4MQTT::__subscribing subscribe topic:[%s] ok", c_iter->c_str());
    }
}

// 发布消息
bool dakuang::CClient4MQTT::__publishMsg(const std::string& strTopic, const std::string& strMessage)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void*)strMessage.data();
    pubmsg.payloadlen = strMessage.size();
    pubmsg.qos = 0;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token = 0;
    int rc = MQTTClient_publishMessage(m_client, strTopic.c_str(), &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        OUTPUT("@dakuang::CClient4MQTT::__publishMsg publish topic:[%s] msg len:[%d] first100:[%s] failed, rc:[%d]",
               strTopic.c_str(), strMessage.size(), strMessage.substr(0, 100).c_str(), rc);
        return false;
    }

    OUTPUT("@dakuang::CClient4MQTT::__publishMsg publish topic:[%s] msg len:[%d] first100:[%s] ok",
           strTopic.c_str(), strMessage.size(), strMessage.substr(0, 100).c_str());
    return true;
}

// 来自MQTT的连接断开回调
void dakuang::CClient4MQTT::__connLostFromMQTT(void* pContext, char* pCause)
{
    CClient4MQTT* pThis = (CClient4MQTT*)pContext;

    OUTPUT("@dakuang::CClient4MQTT::__connLostFromMQTT connection lost, reason:[%s]", pCause);

    pThis->m_thread.emitEvent("connlost");
}

// 来自MQTT的消息到达回调
int dakuang::CClient4MQTT::__msgArrvdFromMQTT(void* pContext, char* pTopicName, int nTopicLen, MQTTClient_message* pMessage)
{
    CClient4MQTT* pThis = (CClient4MQTT*)pContext;

    if (nTopicLen == 0)
        nTopicLen = strlen(pTopicName);
    std::string strTopic(pTopicName, nTopicLen);
    std::string strMessage((const char*)pMessage->payload, pMessage->payloadlen);

    MQTTClient_freeMessage(&pMessage);
    MQTTClient_free(pTopicName);

    OUTPUT("@dakuang::CClient4MQTT::__msgArrvdFromMQTT recv message, topic:[%s] msg len:[%d] first100:[%s]",
           strTopic.c_str(), strMessage.size(), strMessage.substr(0, 100).c_str());

    if (pThis->m_cbMessageArrived)
    {
        pThis->m_cbMessageArrived(strTopic, strMessage);
    }

    return 1;
}
