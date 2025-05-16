#ifndef SOCKETCLIENT_H
#define SOCKETCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>

class SocketClient : public QObject
{
    Q_OBJECT
public:
    explicit SocketClient(QObject *parent = nullptr);

    void createConnection(int softphoneID, int channelIdInRole, QString ipaddress, quint16 port);
    void createConnection(QString ipaddress, quint16 port);
    void sendMessage(QString msg);

signals:
    void Connected();
    void closed(int socketID, QString ipaddress);
    void SocketClientError();
    void disconnected(QWebSocket *client);
    void newCommandProcess(QString message);

private slots:
    void onConnected();
    void onTextMessageReceived(QString message);
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void attemptReconnect();

private:
    QWebSocket m_webSocket;
    QTimer *reconnectTimer;
    QString m_ipaddress;
    QUrl m_url;
    int m_socketID;
    bool isConnected = false;
    bool m_debug = true;  // Set this as per your needs
    int softPhoneID;

    // Handle connection errors
    void handleError(QAbstractSocket::SocketError error);
};

#endif // SOCKETCLIENT_H
