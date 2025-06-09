#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QtSql>
#include <QString>
#include <QWebSocket>
#include <QProcess>

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QString dbName, QString user, QString password, QString host, QObject *parent = nullptr);
    bool database_createConnection();
    bool passwordVerify(QString password);
    void genHashKey();
    void hashletPersonalize();
    bool checkHashletNotData();
    void insertNewAudioRec(QString filePath, QString radioEvent);
    void updateAudioRec(QString filePath, float avg_level, float max_level);
    bool getLastEventCheckAudio(int time, int percentFault, int lastPttMinute);
    QString getNewFile(int warnPercentFault);
    qint64 getStandbyDuration();
    void removeAudioFile(int lastMin);
    int currentFileID = 0;
    QString loadlog = "load_";
    QString filelog;
    QString logdata;
    int Serial_ID;
    bool isCheck = false;
    QString USER;
    QString IP_MASTER;
    QString IP_SLAVE;
    QString IP_SNMP;
    QString IP_TIMERSERVER;
    QString swversion;

signals:
    void audioFault(bool fault);
    void setupinitialize(QString);
    void databaseError();

    void sendRecordFiles(QString jsonData, QWebSocket* wClient);
    void previousRecordVolume(QString);
public slots:
    void recordVolume(int,int);
    void updateRecordVolume();
    void fetchAllRecordFiles(QString msgs, QWebSocket* wClient);
    void CheckAndHandleDevice(const QString& jsonString, QWebSocket* wClient);
    void RegisterDeviceToDatabase(const QString& jsonString, QWebSocket* wClient);
    void UpdateDeviceInDatabase(const QString& jsonString, QWebSocket* wClient);
    void RemoveFile(const QString& jsonString, QWebSocket* wClient);
    void getRegisterDevicePage(const QString& jsonString, QWebSocket* wClient);
    void removeRegisterDevice(const QString& jsonString, QWebSocket* wClient);
    void recordChannel(QString, QWebSocket*);
    void selectRecordChannel(QString,QWebSocket* wClient);
    void CheckandVerifyDatabases();
    void CheckandVerifyTable();
    void verifyTableSchema(const QString &tableName, const QMap<QString, QString> &expectedColumns);
//    void selectRecordChannel(QString, QString, int, QString, QString, QWebSocket* wClient);
    bool tableExists(const QString &tableName);
    void cleanupOldRecordFiles();
    void maybeRunCleanup();
    void updatePath(const QString& jsonString, QWebSocket* wClient);
//    void mysqlRecordDevice(const QJsonObject &obj);
    void lookupDeviceStationByIp(const QString& megs, QWebSocket* wClient);
    void recordToRecordChannel(const QJsonObject& obj);
    void formatDatabases(QString);
    void linkRecordChannelWithDeviceStation();
    void linkRecordFilesWithDeviceStationOnce();

private:
    void addMissingColumn(const QString &tableName, const QString &columnName, const QString &columnType);  // ✅ เพิ่มบรรทัดนี้

    QSqlDatabase db;
    bool verifyMac();
    QString getPassword();
    qint64 getTimeDuration(QString filePath);
    void getLastEvent();
    void startProject(QString filePath, QString radioEvent);

    QString getSerial();
    QStringList getMac();
    void updateHashTable(QString mac, QString challenge ,QString meta, QString serial, QString password);
//    int deviceId;
//    double inputFrequency;
//    QString inputMode;
//    QString inputIp;
//    int inputTimeInterval;
//    QString inputPathDirectory,inputCompanyName;
private slots:
    void reloadDatabase();
//    void getEventandAlarm(QString msg);
};

#endif // DATABASE_H
