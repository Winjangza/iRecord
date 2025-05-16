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
#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QList>
#include <QByteArray>
#include "QWebSocketServer"
#include "QWebSocket"
#include <QDebug>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

class ChatServer : public QObject
{
    Q_OBJECT
public:
    explicit ChatServer(quint16 port, QObject *parent = Q_NULLPTR);
    virtual ~ChatServer();
    void updateNetworkInfo(uint8_t dhcpmethod, QString ipaddress, QString subnet, QString gateway, QString pridns, QString secdns, QString ntpServer, QWebSocket *newClient, QString phyNetworkName);
    void updateSystemInfo(QString SwVersion, QString HwVersion, bool ntp, QString ntpServer, QString timeLocation, QWebSocket *newClient);
    void broadcastSystemInfo(QString memmory, QString storage, QString cupusage, QString currentTime, QString currentDate);
    void reloadIndex();
    int clientNum = 0;
    QString msg;
    bool interlock;
    bool interlock_uptoweb;
    QWebSocket *m_MonitorSocketClients;
    QList<QWebSocket *> m_Monitor;


signals:
//    void newCommandProcess(QString);
    void newConnection(QString menuIndex, QWebSocket *newClient);
    void updateNetwork(uint8_t dhcpmethod, QString ipaddress, QString subnet, QString gateway, QString pridns, QString secdns, QString phyNetworkName);
    void updateFirmware();
    void updateNTPServer(QString value);
    void setcurrentDatetime(QString value);
//    void setLocation(QString value);
    void restartnetwork();
    void rebootSystem();
    void disconnected(QWebSocket *);
    void onNewMessage(QString message);
    void getMonitorPage(QWebSocket *pSender, int roleID);
    void getNetworkPage(QWebSocket *pSender);
    void getSystemPage(QWebSocket *pSender);
    void newCommandProcess(QString command, QWebSocket *pSender);
    void onNumClientChanged(int numclient);
    void clientDisconnect(QWebSocket *pSender);
private Q_SLOTS:
    void onNewConnection();
//    void commandProcess(QString message, QWebSocket *pSender);
    void processMessage(QString message);
    void socketDisconnected();
    void sendMessage(QString, QWebSocket *);


public slots:
    void sendMessageToMonitor(QString);
    void broadcastMessage(QString message);

private:
    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> m_clients;
    QList<QWebSocket *> m_iConSocketClients;
    QList<QWebSocket *> m_WebSocketClients;
    QList<QWebSocket *> m_WebGPSSocketClients;
    QList<QWebSocket *> m_WebModemSocketClients;
    QList<QWebSocket *> m_IndexSocketClients;

    QWebSocket *newSocket;

};

#endif //CHATSERVER_H
