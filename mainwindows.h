#ifndef MAINWINDOWS_H
#define MAINWINDOWS_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QDateTime>
#include <QSettings>
#include <max9850.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <GPIOClass.h>
#include <SPIOLED.h>
#include <linux_spi.h>
#include <SPI.h>
#include <infoDisplay.h>
#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <NetworkMng.h>
#include <QStorageInfo>
#include <GetInputEvent.h>
#include <array>
#include <database.h>
#include <I2CReadWrite.h>
#include <MAX31760.h>
#include <ChatServer.h>
#include <QProcess>
#include <QStringList>
#include <QVector>
#include <sys/statvfs.h>
#include <SocketClient.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/stat.h>
#include "FileDownloader.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#define CODECMAX9850  "7"
#define CODECI2CADDR   0x10
#define GPIO01	"PQ.05"     //"453"		//GPIO33_PQ.05		tegra234-gpio = (15*8)+5 = 125
#define GPIO02	"PP.06"     //"446"		//GPIO33_PP.06		tegra234-gpio = (14*8)+6 = 118
#define GPIO03	"PCC.00"    //"328"		//GPIO33_PCC.00		tegra234-gpio-aon = (02*8)+0 = 16
#define GPIO04	"PCC.01"    //"329"		//GPIO33_PCC.01		tegra234-gpio-aon = (02*8)+1 = 17
#define GPIO05 	"PC.02"    //"330"		//GPIO33_PCC.02		tegra234-gpio-aon = (02*8)+2 = 18
#define GPIO06  "PN.01" //GPIO3_PE.06	tegra-gpio:38
#define GPIO10  "PQ.06" //GPIO3_PE.06	tegra-gpio:38
#define GPIO13  "PH.00" //GPIO3_PE.06	tegra-gpio:38

#define CODEC_RESET_PIN GPIO13

#define OLED2_DC            "1016" //[4]
#define OLED2_RESET         "1017" //[5]
#define OLED2_SPIDEV        "/dev/spidev2.0"
#define FILESETTING         "/etc/irecord.conf"              //"/etc/ifzrecorder/config.conf"
#define FILESETTINGPATH     "/home/orinnx/alphatest/irecd1.0.4/src/config.ini"
#define FILESETTINGIP       "/home/orinnx/recorder/may1/config.conf"
#define FILESETTINGVERSION  "/home/orinnx/README.txt"

class mainwindows: public QObject
{
     Q_OBJECT
public:
    explicit mainwindows(QObject *parent = nullptr);
//    int registerMax9850();
signals:

    void requestUpdateDateTime();
    void recordLevel(int,int);
    void mainwindowsMesg(QString, QWebSocket*);
    void updateNetwork(QString ,QString ,QString ,QString ,QString , QString);
    void triggerRotary(int encoderNum, int val, int inputEventID);
public slots:

    void mesgManagement(QString);
    void getDateTime();
    void readSSDStorage();
    void displayVolumes();
    void rotary(int encoderNum, int val, int InputEventID);
    void updateNewVolumeRecord(QString);
    void handleRecordFiles(const QString &jsonData);
    void manageData(QString, QWebSocket*);
    void dashboardManage(QString, QWebSocket*);
    void DirectoryManagement(QString);
    void editfileConfig(QString);
    void verifySSDandBackupFile();
    void setdefaultPath(QString, QWebSocket*);
    void handleSelectPath(const QJsonObject &obj, QWebSocket* wClient);
    void configDestinationIP(QString, QWebSocket*);
    void updateCurrentPathDirectory(QString, QWebSocket*);
    void liveSteamRecordRaido(QString, QWebSocket*);
    void updateSystem(QString message);
    void downloadCompleted(const QString &outputPath);
    void updateFirmware();
    QStringList findFile();
//    void recordChannel(QString, QWebSocket*);
    void playWavFile(const QString& filePath);
    void formatExternal(const QString &filePath);
    void runBuildAndRestartServices();
private:
    FileDownloader *downloader = nullptr;
    QString SwVersion = "27052025 0.9"; //ในนี้จะต่ำกว่าในเว็บ 0.1 version
    bool foundfileupdate = false;
    int updateStatus = 0;
    Max9850 *max9850;
    GPIOClass *gpioAmp;
    infoDisplay *displayInfo;
    NetworkMng *networking;
    QTimer *Timer;
    QTimer *storageTimer;
    QTimer* dashboardTimer;
    QTimer* diskTimer;
    QWebSocket *wClient = nullptr;
    GetInputEvent *RotaryEncoder01;
    GetInputEvent *RotaryEncoderButtonL;
    Database *mysql;
    Database *mydatabase;
    MAX31760 *max31760;
    ChatServer *SocketServer;
    SocketClient *client;
    QTimer *reconnectTimer;
    QWebSocket *clientSocket = nullptr;
    QString dashboardMsg;
    uint8_t messageToSTM32[50] = {0};
    QString currentDate;
    QString currentTime;
    QString display2line1 = "System Date Time";
    QString display2line2;
    QString display2line3;
    QString display2line4;
    QString newdisplay2line4;
    QList<int> activeStreamIds;
    QString getDeviceFromMount(const QString& mountPath) {
        QString cmd = QString("findmnt -n -o SOURCE --target %1").arg(mountPath);
        FILE* pipe = popen(cmd.toUtf8().data(), "r");
        if (!pipe) return QString();
        char buffer[256];
        QString result;
        if (fgets(buffer, sizeof(buffer), pipe)) {
            result = QString(buffer).trimmed();
        }
        pclose(pipe);
        return result;
    }
    QString getDeviceForMount(const QString& mountPoint) {
        QProcess process;
        process.start("findmnt", {"-n", "-o", "SOURCE", mountPoint});
        process.waitForFinished();
        return QString(process.readAllStandardOutput()).trimmed();
    }

    bool mountIfNotMounted(const QString& device, const QString& mountPoint) {
        QProcess process;
        process.start("mountpoint", {"-q", mountPoint});
        process.waitForFinished();
        if (process.exitCode() != 0) {
            qDebug() << mountPoint << "is not mounted. Trying to mount.";
            QProcess mnt;
            mnt.start("mount", {device, mountPoint});
            mnt.waitForFinished();
            return (mnt.exitCode() == 0);
        }
        return true;
    }
    bool unmountIfNotMounted(const QString& device, const QString& mountPoint) {
        QProcess process;
        process.start("umount", {"-q", mountPoint});
        process.waitForFinished();
        if (process.exitCode() != 0) {
            qDebug() << mountPoint << "is not mounted. Trying to mount.";
            QProcess mnt;
            mnt.start("umount", {device, mountPoint});
            mnt.waitForFinished();
            return (mnt.exitCode() == 0);
        }
        return true;
    }
    double getDiskUsage(const QString& path) {
        struct statvfs stat;
        if (statvfs(path.toUtf8().data(), &stat) != 0)
            return -1.0;
        double total = static_cast<double>(stat.f_blocks) * stat.f_frsize;
        double used = static_cast<double>(stat.f_blocks - stat.f_bfree) * stat.f_frsize;
        return (used / total) * 100.0;
    }
    QString currentBackupPath;
//    boost::atomic_bool m_IsEventThreadRunning;
//    boost::shared_ptr<boost::thread> *m_EventThreadInstance;

    struct phyNetwork{
        QString dhcpmethod;
        QString ipaddress;
        QString subnet;
        QString gateway;
        QString pridns;
        QString secdns;
        QString macAddress;
        QString serviceName = "";
        QString phyNetworkName = "";
        bool lan_ok = false;
    };

    phyNetwork eth0;
    phyNetwork eth1;
    phyNetwork bond0;
    QString datetime;

    static void* ThreadFunc( void* pTr );
    typedef void * (*THREADFUNCPTR)(void *);
    static void* ThreadFunc2( void* pTr );
    typedef void * (*THREADFUNCPTR2)(void *);
    static void* ThreadFunc3(void* pTr);
    typedef void * (*THREADFUNCPTR3)(void *);

    pthread_t idThread;
    pthread_t idThread2;
    pthread_t idThread3;



    unsigned char VolumeOut = 255;
    int currentVolume;
    int updatelevel;

    float CPUtemp, GPUtemp;
//    QDateTime currentDateTime;
    QString date, time;
    QString fileName, filePath;
    unsigned long total;
    unsigned long free;
    unsigned long available;
    unsigned long used;
    int percent;
    struct statvfs stat;
    double ssdUsedGB = 0, ssdTotalGB = 0;
    double rootUsedGB = 0, rootTotalGB = 0;
    double extUsedGB = 0, extTotalGB = 0;


};

#endif // MAINWINDOWS_H
