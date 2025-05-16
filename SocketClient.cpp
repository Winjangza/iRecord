#include "SocketClient.h"
#include <QDebug>

SocketClient::SocketClient(QObject *parent) :
    QObject(parent),
    reconnectTimer(new QTimer(this))
{
    connect(&m_webSocket, &QWebSocket::connected, this, &SocketClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &SocketClient::onDisconnected);
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &SocketClient::onError);
    connect(&m_webSocket, &QWebSocket::textMessageReceived,
            this, &SocketClient::onTextMessageReceived);

    connect(reconnectTimer, &QTimer::timeout, this, &SocketClient::attemptReconnect);
    reconnectTimer->setInterval(5000); // Reconnect every 5 seconds
}

void SocketClient::createConnection(int softphoneID, int channelIdInRole, QString ipaddress, quint16 port)
{
    QString url = QString("ws://%1:%2").arg(ipaddress).arg(port);
    QUrl iGateSocketServerUrl(url);
    qDebug() << "WebSocket server:" << url;

    m_url = iGateSocketServerUrl;
    m_ipaddress = ipaddress;
    softPhoneID = softphoneID;
    m_socketID = channelIdInRole;
    m_webSocket.open(QUrl(iGateSocketServerUrl));
}

void SocketClient::createConnection(QString ipaddress, quint16 port)
{
    qDebug() << "createConnection:" << ipaddress << port;
    QString url = QString("ws://%1:%2").arg(ipaddress).arg(port);
    QUrl iGateSocketServerUrl(url);

    m_url = iGateSocketServerUrl;
    m_ipaddress = ipaddress;
    m_webSocket.open(QUrl(iGateSocketServerUrl));
}

void SocketClient::onConnected()
{
    isConnected = true;
    qDebug() << "WebSocket connected";
    emit Connected();

    // Send a message upon successful connection
    QJsonObject json;
    json["menuID"] = "connectToWebSocket";
    QJsonDocument doc(json);
    QString msg = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    m_webSocket.sendTextMessage(msg);

    // Stop reconnect attempts once connected
    reconnectTimer->stop();
}

void SocketClient::onTextMessageReceived(QString message)
{
    emit newCommandProcess(message);
}

void SocketClient::sendMessage(QString msg)
{
    qDebug() << "send socket:" << msg;
    if (isConnected) {
        m_webSocket.sendTextMessage(msg);
    } else {
        qDebug() << "Cannot send message, WebSocket is not connected.";
    }
}

void SocketClient::onDisconnected()
{
    isConnected = false;
    qDebug() << m_ipaddress << "WebSocket disconnected";
    emit closed(m_socketID, m_ipaddress);
    emit SocketClientError();

    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    emit disconnected(pClient);

    // Start reconnection timer if it's not already active
    if (!reconnectTimer->isActive()) {
        reconnectTimer->start();
    }
}

void SocketClient::onError(QAbstractSocket::SocketError error)
{
    qDebug() << "Connecting Error: " << error;
    isConnected = false;
    emit SocketClientError();
}

void SocketClient::attemptReconnect()
{
    if (!isConnected) {
        qDebug() << "Attempting WebSocket reconnect...";
        m_webSocket.open(m_url);
    }
}
