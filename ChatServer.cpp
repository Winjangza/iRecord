/****************************************************************************
  **
  ** Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
  ** Contact: https://www.qt.io/licensing/
  **
  ** This file is part of the QtWebSockets module of the Qt Toolkit.
  **
  ** $QT_BEGIN_LICENSE:BSD$
  ** Commercial License Usage
  ** Licensees holding valid commercial Qt licenses may use this file in
  ** accordance with the commercial license agreement provided with the
  ** Software or, alternatively, in accordance with the terms contained in
  ** a written agreement between you and The Qt Company. For licensing terms
  ** and conditions see https://www.qt.io/terms-conditions. For further
  ** information use the contact form at https://www.qt.io/contact-us.
  **
  ** BSD License Usage
  ** Alternatively, you may use this file under the terms of the BSD license
  ** as follows:
  **
  ** "Redistribution and use in source and binary forms, with or without
  ** modification, are permitted provided that the following conditions are
  ** met:
  **   * Redistributions of source code must retain the above copyright
  **     notice, this list of conditions and the following disclaimer.
  **   * Redistributions in binary form must reproduce the above copyright
  **     notice, this list of conditions and the following disclaimer in
  **     the documentation and/or other materials provided with the
  **     distribution.
  **   * Neither the name of The Qt Company Ltd nor the names of its
  **     contributors may be used to endorse or promote products derived
  **     from this software without specific prior written permission.
  **
  **
  ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  ** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  ** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  ** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  ** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  ** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  ** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
  **
  ** $QT_END_LICENSE$
  **
  ****************************************************************************/
#include "ChatServer.h"
#include <QDateTime>
#include <QThread>
QT_USE_NAMESPACE

ChatServer::ChatServer(quint16 port, QObject *parent) :
    QObject(parent),
    m_pWebSocketServer(Q_NULLPTR),
    m_clients()
{
    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("Chat Server"),
                                              QWebSocketServer::NonSecureMode,
                                              this);
    if (m_pWebSocketServer->listen(QHostAddress::Any, port))
    {
        qDebug() << "Chat Server listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &ChatServer::onNewConnection);
    }

}

ChatServer::~ChatServer()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void ChatServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();
    connect(pSocket, &QWebSocket::textMessageReceived, this, &ChatServer::processMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &ChatServer::socketDisconnected);
    m_clients << pSocket;
    m_Monitor.append(m_clients);
    qDebug() << "On New Connection from address : " << pSocket->peerName();
    emit getMonitorPage(pSocket,-2);
    qDebug() << "onNewConnection_m_clients:" << "m_clients" << m_clients<< "m_Monitor" << m_Monitor;
    if (clientNum <= 0)
    {
        qDebug() << "om_clients <= 0" << m_clients;

        clientNum = m_clients.length();
        emit onNumClientChanged(clientNum);
    }
    else {
        qDebug() << "om_clients >= 0" << m_clients;

        clientNum = m_clients.length();
    }

}

void ChatServer::sendMessage(QString message, QWebSocket *pSender){
    qDebug() << "sendMessage:"<< message << " by pSender" << pSender;
//    pSender->sendTextMessage(message);
    if(pSender->state()){
        qDebug() << "sendMessage TRUE";
        pSender->sendTextMessage(message);
    }
}

void ChatServer::sendMessageToMonitor(QString message){
    qDebug() << "sendMessage:"<< message << " by m_MonitorSocketClients" << m_MonitorSocketClients;
//    if(m_MonitorSocketClients != nullptr){
//        m_MonitorSocketClients->sendTextMessage(message);
//    }
    Q_FOREACH (QWebSocket *pClient, m_Monitor)
    {
        qDebug() << "broadcastMessage:" << m_clients;
        if(pClient->state()){
            qDebug() << "broadcastMessage TRUE";
            pClient->sendTextMessage(message);
        }
    }
}

void ChatServer::broadcastMessage(QString message){
    qDebug() << "broadcastMessage:" << m_clients << message;

    Q_FOREACH (QWebSocket *pClient, m_clients)
    {
        if(pClient->state()){
//            qDebug() << "broadcastMessage TRUE";
            pClient->sendTextMessage(message);
        }

    }
}

void ChatServer::processMessage(QString message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
//    if (pClient) {
//        qDebug() << "broadcastMessage:"<< pClient;
//        pClient->sendTextMessage(message);
//    }
//    qDebug() << "processMessage:"<< message;
    emit newCommandProcess(message,pClient);
}

void ChatServer::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    qDebug() << "socketDisconnected pClient" << pClient;
    emit disconnected(pClient);
    if (pClient)
    {
        emit clientDisconnect(pClient);
        m_clients.removeAll(pClient);
//        m_Monitor.removeAll(pClient);
//        m_clients.removeLast();
//        m_iConSocketClients.removeAll(pClient);
//        m_WebSocketClients.removeAll(pClient);
//        m_WebGPSSocketClients.removeAll(pClient);
//        m_WebModemSocketClients.removeAll(pClient);
//        m_IndexSocketClients.removeAll(pClient);
//        pClient->deleteLater();
        qDebug() << pClient->localAddress().toString() << "has disconect";
    }
    clientNum = m_clients.length();
    qDebug() << "clientNum:" << clientNum;
    if (clientNum <= 0)
    {
        emit onNumClientChanged(clientNum);
    }
    Q_FOREACH (QWebSocket *client, m_Monitor)
    {
        if(client == pClient){
            m_Monitor.removeOne(client);
        }
    }
}
