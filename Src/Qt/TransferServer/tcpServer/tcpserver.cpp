﻿#include "tcpserver.h"
#include "threadhandle.h"
#include "infoOutput.h"

TcpServer::TcpServer(QObject *parent,int numConnections) :
    QTcpServer(parent)
{
     tcpClient = new  QHash<int,TcpSocket *>;
     setMaxPendingConnections(numConnections);
}

TcpServer::~TcpServer()
{
    emit this->sentDisConnect(-1);
    delete tcpClient;
}

void TcpServer::setMaxPendingConnections(int numConnections)
{
    //调用Qtcpsocket函数，设置最大连接数，主要是使maxPendingConnections()依然有效
    this->QTcpServer::setMaxPendingConnections(numConnections);
    this->maxConnections = numConnections;
}

//多线程必须在此函数里捕获新连接
void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    //继承重写此函数后，QTcpServer默认的判断最大连接数失效，自己实现
    if (tcpClient->size() > maxPendingConnections())
    {
        QTcpSocket tcp;
        tcp.setSocketDescriptor(socketDescriptor);
        tcp.disconnectFromHost();
        return;
    }
    auto th = ThreadHandle::getClass().getThread();
    auto tcpTemp = new TcpSocket(socketDescriptor);
    QString ip =  tcpTemp->peerAddress().toString();
    qint16 port = tcpTemp->peerPort();
    //NOTE:断开连接的处理，从列表移除，并释放断开的Tcpsocket，此槽必须实现，线程管理计数也是靠的他
    connect(tcpTemp,SIGNAL(sockDisConnect(int,QString,quint16,QThread*)),
            this,SLOT(sockDisConnectSlot(int,QString,quint16,QThread*)));
    //接受数据信号绑定
    connect(tcpTemp,SIGNAL(readData(int,QString,quint16,QJsonObject)),
            this,SIGNAL(readData(int,QString,quint16,QJsonObject)));
    //断开信号
    connect(this,SIGNAL(sentDisConnect(int)),
            tcpTemp,SLOT(disConTcp(int)));
    //新用户成功登录
    connect(tcpTemp,SIGNAL(oneUserLoginSucceed(int,QString)),
            this,SIGNAL(oneUserLoginSuccessed(int,QString)));
    //新管理员成功登录
    connect(tcpTemp,SIGNAL(oneAdminLoginSucceed(int,QString)),
            this,SIGNAL(oneAdminLoginSuccessed(int,QString)));
    //用户请求数据
    connect(tcpTemp,SIGNAL(requestSensingData(int,int,int,QString)),
            this,SIGNAL(requestSensingData(int,int,int,QString)));
    //用户请求控制
    connect(tcpTemp,SIGNAL(controlCmd(int,QJsonObject)),
            this,SIGNAL(controlCmd(int,QJsonObject)));
    //用户登录成功，旧用户掉线
    connect(this,SIGNAL(oneUserLoginSuccessed(int,QString)),
            tcpTemp,SLOT(serverOneUserLoginSucceedSlot(int,QString)));
    //管理员登录成功，旧用户掉线
    connect(this,SIGNAL(oneAdminLoginSuccessed(int,QString)),
            tcpTemp,SLOT(serverOneAdminLoginSucceedSlot(int,QString)));
    //请求结果
    connect(this,SIGNAL(reRequestSensingData(int,int,bool,QJsonObject)),
            tcpTemp,SLOT(reRequestSensingDataSlot(int,int,bool,QJsonObject)));
    //发送数据
    connect(this,SIGNAL(sentData(QJsonObject,int)),
            tcpTemp,SLOT(sentData(QJsonObject,int)));
    //控制结果返回
    connect(this,SIGNAL(reControlCmd(int,int,bool)),
            tcpTemp,SLOT(reControlCmdSlot(int,int,bool)));
    //把tcp类移动到新的线程，从线程管理类中获取
    tcpTemp->moveToThread(th);
    //插入到连接信息中
    tcpClient->insert(socketDescriptor,tcpTemp);
    //发送新客户端连接信号
    emit connectClient(socketDescriptor,ip,port);
}

void TcpServer::sockDisConnectSlot(int handle,const QString & ip, quint16 prot,QThread * th)
{
    //连接管理中移除断开连接的socket
    tcpClient->remove(handle);
    //告诉线程管理类那个线程里的连接断开了
    ThreadHandle::getClass().removeThread(th);
    emit sockDisConnect(handle,ip,prot);
}

void TcpServer::clear()
{
    emit this->sentDisConnect(-1);
    ThreadHandle::getClass().clear();
    tcpClient->clear();
}
