#pragma once

#include <QObject>
#include <QSocketNotifier>

class UnixSocketListener : public QObject {
    Q_OBJECT
public:
    explicit UnixSocketListener(QObject* parent = nullptr);
    ~UnixSocketListener();

signals:
    void messageReceived(const QString& message);

private:
    int socketFd;
    QSocketNotifier* notifier;
    QString socketPath = "/tmp/recd_status.sock";

    void setupSocket();

private slots:
    void handleReadyRead();
};
