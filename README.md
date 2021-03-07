# client4mqtt
本项目是基于paho.mqtt.c库封装的一个mqtt客户端实现，以类的方式提供，有需要的朋友请直接将ThreadModel.h ThreadModel.cpp Client4MQTT.h Client4MQTT.cpp这4个文件拷贝到自己的项目中。

本项目支持跨平台，可在windows和linux上进行编译，但是需要依赖以下几个库：
- paho.mqtt.c
- pthread-w32
- openssl

以上几个库，paho.mqtt.c是所有平台必需的，pthread-w32只在windows平台需要，openssl是paho.mqtt.c编译ssl版本间接要求的。

### 用法举例：
```
#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include <unistd.h>
#endif

#include "Client4MQTT.h"

dakuang::CClient4MQTT* g_pClient4MQTT = NULL;

// 线程定时器回调
void CB_Timer()
{
    printf("@CB_Timer call \n");
    
    // 向MQTT客户端对象发射一个定时器脉冲
    g_pClient4MQTT->emitTimerEvent();
}

// 接收消息回调
void CB_MessageArriv(const std::string& strTopic, const std::string& strMessage)
{
    printf("@CB_MessageArriv => topic:[%s] message:[%s] \n", strTopic.c_str(), strMessage.c_str());
}

int main(int argc, char* argv[])
{
    // 定义MQTT客户端对象
    dakuang::CClient4MQTT client4MQTT;
    client4MQTT.setClientID("abc123");
    client4MQTT.setServerAddress("tcp://127.0.0.1:1883");
    client4MQTT.addSubTopic("test");
    client4MQTT.setMessageArrivedCB(CB_MessageArriv);
    client4MQTT.start();

    g_pClient4MQTT = &client4MQTT;

    // 定义线程定时器对象
    dakuang::CThreadTimer threadTimer;
    threadTimer.setTimerCB(CB_Timer);
    threadTimer.start(10000);

#ifdef __linux__
    sleep(60);
#else
    Sleep(60*1000);
#endif

    client4MQTT.stop();
    threadTimer.stop();

    return 0;
}
```

### paho.mqtt.c

下载：https://github.com/eclipse/paho.mqtt.c

编译：<br>
在linux平台直接使用make命令编译。<br>
在windows平台编译，建议使用cmake编译。

### pthread-w32

下载：https://sourceware.org/pthreads-win32

根据需要下载对应的版本，我下载的是pthreads-w32-2-9-1-release.zip。解压文件，得到3个子目录，其中Pre-built.2是已经编译好的头文件和库，库又包括动态库和静态库。我建议使用动态库链接方式，以规避windows运行时不同造成无法链接的问题。

### openssl

下载：https://www.openssl.org/source

linux平台编译：<br>
解压，进入源码目录，运行：<br>
./config --prefix=/usr/local/openssl <br>
make <br>
sudo make install <br>
安装位置: /usr/local/openssl

windows平台，建议下载现成的安装包，地址：http://slproweb.com/products/Win32OpenSSL.html


