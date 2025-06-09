#include "database.h"
#include <QDateTime>
#include <QStringList>
#include <QString>
#include <QProcess>
#include <QVariant>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <typeinfo>
#include <QVariant>

Database::Database(QString dbName, QString user, QString password, QString host, QObject *parent) :
    QObject(parent)
{
    qDebug() << "Connecting to MySQL" << dbName << user << password << host;
    db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName(host);
    db.setDatabaseName(dbName);
    db.setUserName(user);
    db.setPassword(password);

    if (!db.open()) {
        qDebug() << "Database connection failed:" << db.lastError().text();
        return;
    }

    qDebug() << "Database connected successfully! ";
}


bool restartMySQLService() {
    qDebug() << "Restarting MySQL service...";
    QProcess process;
    process.start("systemctl restart mysql");
    process.waitForFinished();
    int exitCode = process.exitCode();

    if (exitCode == 0) {
        qDebug() << "MySQL service restarted successfully.";
        return true;
    } else {
        qWarning() << "Failed to restart MySQL service.";
        return false;
    }
}

void Database::lookupDeviceStationByIp(const QString& megs, QWebSocket* wClient) {
    qDebug() << "lookupDeviceStationByIp:" << megs;

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QStringList parts = megs.split(",");
    if (parts.size() != 4) {
        qWarning() << "Invalid message format";
        return;
    }

    QString ip     = parts[0].trimmed();
    QString freq   = parts[1].trimmed();
    QString url    = parts[2].trimmed();
    QString action = parts[3].trimmed();

    QSqlQuery query(db);  // ใช้ db ตัวหลัก
    query.prepare("SELECT id, name FROM device_station WHERE ip = :ip LIMIT 1");
    query.bindValue(":ip", ip);

    if (query.exec()) {
        if (query.next()) {
            int sid = query.value("id").toInt();
            QString name = query.value("name").toString();

            QJsonObject obj;
            obj["ip"] = ip;
            obj["freq"] = freq;
            obj["url"] = url;
            obj["action"] = action;
            obj["sid"] = sid;
            obj["name"] = name;

            qDebug() << "Device found:" << obj;
            QThread::msleep(100);
            recordToRecordChannel(obj);
        } else {
            qWarning() << "Device not found for IP:" << ip;
        }
    } else {
        qWarning() << "Failed to execute query:" << query.lastError().text();
    }
//    db.close();
}

void Database::formatDatabases(QString megs) {
    qDebug() << "Received format command:" << megs;
    QByteArray br = megs.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();
    QJsonObject command = doc.object();
    QString menuID = obj["menuID"].toString().trimmed();

    if (menuID == "formatExternal") {
        qDebug() << "formatDatabases:" << megs;
        qDebug() << "Database path:" << db.databaseName();

        if (!db.isOpen()) {
            if (!db.open()) {
                qDebug() << "❌ Failed to open database:" << db.lastError().text();
                return;
            }
        }

        QSqlQuery pragmaQuery(db);
        pragmaQuery.exec("PRAGMA foreign_keys = OFF;"); // for SQLite

        QSqlQuery query(db);
        QString cmd = "DELETE FROM record_files WHERE 1=1;";
        if (!query.exec(cmd)) {
            qDebug() << "❌ Failed to clear record_files table:" << query.lastError().text();
        } else {
            qDebug() << "✅ Cleared table record_files successfully.";
        }

        // ตรวจสอบว่าลบหมดจริงหรือไม่
        QSqlQuery countQuery("SELECT COUNT(*) FROM record_files;", db);
        if (countQuery.next()) {
            int remaining = countQuery.value(0).toInt();
            qDebug() << "Remaining rows in record_files:" << remaining;
        }

        db.close();
    }
}


void Database::recordToRecordChannel(const QJsonObject& obj) {
    QString ip      = obj.value("ip").toString().trimmed();
    QString url     = obj.value("url").toString().trimmed();
    QString action  = obj.value("action").toString().trimmed();
    QString name    = obj.value("name").toString().trimmed();
    int sid         = obj.value("sid").toInt(-1);
    int freq        = 0;

    if (obj.contains("freq")) {
        if (obj["freq"].isString())
            freq = obj["freq"].toString().toInt();
        else if (obj["freq"].isDouble())
            freq = obj["freq"].toInt();
    }

    if (sid < 0) {
        qWarning() << "Invalid SID, aborting.";
        return;
    }

    if (!db.isValid() || !db.isOpen()) {
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    const QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    QSqlQuery queryCheck(db);
    queryCheck.prepare("SELECT id, sid, ip, url, name, freq FROM record_channel WHERE ip = :ip LIMIT 1");
    queryCheck.bindValue(":ip", ip);


    if (!queryCheck.exec()) {
        qWarning() << "Failed to SELECT from record_channel:" << queryCheck.lastError().text();
        return;
    }

    if (queryCheck.next()) {
        int id = queryCheck.value("id").toInt();
        QString ipDB   = queryCheck.value("ip").toString();
        QString urlDB  = queryCheck.value("url").toString();
        QString nameDB = queryCheck.value("name").toString();
        int freqDB     = queryCheck.value("freq").toInt();

        bool needsUpdate = (ipDB != ip || urlDB != url || nameDB != name || freqDB != freq);

        if (needsUpdate) {
            QSqlQuery updateQuery(db);
            updateQuery.prepare(R"(
                UPDATE record_channel
                SET ip = :ip, url = :url, name = :name, freq = :freq,
                    action = :action, updated_at = :updated
                WHERE id = :id
            )");
            updateQuery.bindValue(":ip", ip);
            updateQuery.bindValue(":url", url);
            updateQuery.bindValue(":name", name);
            updateQuery.bindValue(":freq", freq);
            updateQuery.bindValue(":action", action);
            updateQuery.bindValue(":updated", now);
            updateQuery.bindValue(":id", id);

            if (!updateQuery.exec()) {
                qWarning() << "Failed to update record_channel:" << updateQuery.lastError().text();
            } else {
                qDebug() << "Updated record_channel with SID:" << sid;
            }
        } else {
            qDebug() << "No update needed for SID:" << sid;
        }
    } else {
        QSqlQuery insertQuery(db);
        insertQuery.prepare(R"(
            INSERT INTO record_channel (sid, ip, url, action, name, freq, created_at, updated_at)
            VALUES (:sid, :ip, :url, :action, :name, :freq, :created, :updated)
        )");
        insertQuery.bindValue(":sid", sid);
        insertQuery.bindValue(":ip", ip);
        insertQuery.bindValue(":url", url);
        insertQuery.bindValue(":action", action);
        insertQuery.bindValue(":name", name);
        insertQuery.bindValue(":freq", freq);
        insertQuery.bindValue(":created", now);
        insertQuery.bindValue(":updated", now);

        if (!insertQuery.exec()) {
            qWarning() << "Failed to insert into record_channel:" << insertQuery.lastError().text();
        } else {
            qDebug() << "Inserted new record_channel with SID:" << sid;
        }
    }
}


bool Database::tableExists(const QString &tableName) {
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = :dbName AND TABLE_NAME = :tableName");
    query.bindValue(":dbName", db.databaseName());
    query.bindValue(":tableName", tableName);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    } else {
        qWarning() << "Failed to check table:" << tableName << query.lastError().text();
        return false;
    }
}

void Database::CheckandVerifyDatabases() {
    qDebug() << "CheckAndVerifyDatabases started";

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QMap<QString, QString> tableDefinitions = {
        { "device_access", R"(
            CREATE TABLE device_access (
                id INT AUTO_INCREMENT PRIMARY KEY,
                port_src varchar(30),
                ip_src varchar(30),
                uri varchar(30),
                device int unsigned,
                active tinyint(1)
            )
        )" },
        { "device_group", R"(
            CREATE TABLE device_group (
                id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
                name VARCHAR(255),
                lastupdate DATETIME,
                visible TINYINT(1) NOT NULL DEFAULT 1
            )
        )" },
        { "device_station", R"(
            CREATE TABLE device_station (
                id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
                sid INT UNSIGNED UNIQUE,
                payload_size INT,
                terminal_type INT,
                name VARCHAR(255),
                ip VARCHAR(255),
                uri VARCHAR(255),
                freq DECIMAL(10,0),
                ambient TINYINT(1) DEFAULT 0,
                `group` INT UNSIGNED,
                visible TINYINT(1) DEFAULT 0,
                last_access DATETIME,
                chunk INT DEFAULT 0,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
            )
        )" },

        { "devices", R"(
            CREATE TABLE devices (
                id INT AUTO_INCREMENT PRIMARY KEY,
                deviceId VARCHAR(255),
                frequency VARCHAR(255),
                mode VARCHAR(50),
                ip VARCHAR(50),
                timeInterval INT,
                pathDirectory VARCHAR(255),
                companyName VARCHAR(255)
            )
        )" },
        { "record_channel", R"(
            CREATE TABLE record_channel (
                id SMALLINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                action VARCHAR(255),
                ip VARCHAR(45),
                url VARCHAR(255),
                freq VARCHAR(50),
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
                sid INT,
                name VARCHAR(255)
            )
        )" },
        { "record_files", R"(
            CREATE TABLE record_files (
                id BINARY(16) PRIMARY KEY,
                device INT UNSIGNED,
                filename VARCHAR(255),
                file_path VARCHAR(255),
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                continuous_count INT,
                name VARCHAR(255)
            )
        )" },
        { "volume_log", R"(
            CREATE TABLE volume_log (
                id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                currentVolume INT,
                level INT
            )
        )" }
    };

    for (auto it = tableDefinitions.begin(); it != tableDefinitions.end(); ++it) {
        const QString &tableName = it.key();
        const QString &createSql = it.value();

        if (!tableExists(tableName)) {
            QSqlQuery query;
            if (!query.exec(createSql)) {
                qWarning() << "Failed to create table" << tableName << ":" << query.lastError().text();
            } else {
                qDebug() << "Created missing table:" << tableName;
            }
        } else {
            qDebug() << "Table exists:" << tableName;
        }
    }
    CheckandVerifyTable();
}


void Database::linkRecordChannelWithDeviceStation() {
    if (!db.isOpen() && !db.open()) {
        qDebug() << "❌ Cannot open database:" << db.lastError().text();
        return;
    }

    QSqlQuery selectQuery(db);
    QSqlQuery updateQuery(db);

    QString selectStr = R"(
        SELECT rc.id, ds.name, ds.id AS ds_id
        FROM record_channel rc
        JOIN device_station ds
          ON rc.ip COLLATE utf8mb4_general_ci = ds.ip COLLATE utf8mb4_general_ci
        WHERE (rc.name IS NULL OR rc.name = '')
           OR (rc.sid IS NULL OR rc.sid = 0)
    )";

    if (!selectQuery.exec(selectStr)) {
        qDebug() << "❌ Failed to select unmatched fields:" << selectQuery.lastError().text();
        return;
    }

    while (selectQuery.next()) {
        int rc_id = selectQuery.value("id").toInt();
        QString ds_name = selectQuery.value("name").toString();
        int ds_id = selectQuery.value("ds_id").toInt();

        QString updateStr = R"(
            UPDATE record_channel
            SET name = :name, sid = :sid
            WHERE id = :id
        )";

        updateQuery.prepare(updateStr);
        updateQuery.bindValue(":name", ds_name);
        updateQuery.bindValue(":sid", ds_id);
        updateQuery.bindValue(":id", rc_id);

        if (!updateQuery.exec()) {
            qDebug() << "❌ Failed to update record_channel.id =" << rc_id << ":" << updateQuery.lastError().text();
        } else {
            qDebug() << "✅ Linked record_channel.id =" << rc_id << " with name =" << ds_name << " and sid =" << ds_id;
        }
    }

    db.close();
}


void Database::linkRecordFilesWithDeviceStationOnce() {
    if (!db.isOpen() && !db.open()) {
        qDebug() << "❌ Cannot open database:" << db.lastError().text();
        return;
    }

    QSqlQuery checkQuery(db);
    QString checkStr = R"(
        SELECT rf.id
        FROM record_files rf
        LEFT JOIN device_station ds ON rf.device = ds.id
        WHERE rf.file_path IS NULL OR rf.file_path = ''
           OR rf.name IS NULL OR rf.name = ''
        LIMIT 1
    )";

    if (!checkQuery.exec(checkStr)) {
        qDebug() << "❌ Failed to check record_files:" << checkQuery.lastError().text();
        return;
    }

    if (!checkQuery.next()) {
        qDebug() << "✅ All record_files already linked. Nothing to do.";
        return;  // ทุกแถวมีข้อมูลครบแล้ว
    }

    QSqlQuery updateQuery(db);
    QString updateStr = R"(
        UPDATE record_files rf
        JOIN device_station ds ON rf.device = ds.id
        SET rf.file_path = ds.storage_path,
            rf.name = ds.name
        WHERE rf.file_path IS NULL OR rf.file_path = ''
           OR rf.name IS NULL OR rf.name = ''
    )";

    if (!updateQuery.exec(updateStr)) {
        qDebug() << "❌ Failed to update record_files:" << updateQuery.lastError().text();
    } else {
        qDebug() << "✅ Linked missing record_files with device_station successfully.";
    }

    db.close();
}



void Database::CheckandVerifyTable() {
    qDebug() << "CheckandVerifyTable started";

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }
    QMap<QString, QMap<QString, QString>> expectedSchemas = {
        {
            "devices", {
                { "id", "INT AUTO_INCREMENT PRIMARY KEY" },
                { "deviceId", "VARCHAR(255)" },
                { "frequency", "VARCHAR(255)" },
                { "mode", "VARCHAR(50)" },
                { "ip", "VARCHAR(50)" },
                { "timeInterval", "INT" },
                { "pathDirectory", "VARCHAR(255)" },
                { "companyName", "VARCHAR(255)" }
            }
        },

        {
            "record_files", {
                { "id", "BINARY(16) PRIMARY KEY" },
                { "device", "INT UNSIGNED" },
                { "filename", "VARCHAR(255)" },
                { "file_path", "VARCHAR(255)" },
                { "created_at", "DATETIME DEFAULT CURRENT_TIMESTAMP" },
                { "continuous_count", "INT" },
                { "name", "VARCHAR(255)" }
            }
        },
        {
            "volume_log", {
                { "id", "INT AUTO_INCREMENT PRIMARY KEY" },
                { "currentVolume", "INT" },
                { "level", "INT" }
            }
        },
        {
            "device_station", {
                { "id", "INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY" },
                { "sid", "INT UNSIGNED UNIQUE" },
                { "payload_size", "INT" },
                { "terminal_type", "INT" },
                { "name", "VARCHAR(255)" },
                { "ip", "VARCHAR(255)" },
                { "uri", "VARCHAR(255)" },
                { "freq", "DECIMAL(10,0)" },
                { "ambient", "TINYINT(1) DEFAULT 0" },
                { "group", "INT UNSIGNED" },
                { "visible", "TINYINT(1) DEFAULT 0" },
                { "last_access", "DATETIME" },
                { "chunk", "INT DEFAULT 0" },
                { "updated_at", "TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP" }
            }

        },
        {
            "record_channel", {
                { "id", "INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY" },
                { "action", "VARCHAR(255)" },
                { "ip", "VARCHAR(45)" },
                { "url", "VARCHAR(255)" },
                { "freq", "VARCHAR(50)" },                
                { "created_at", "DATETIME DEFAULT CURRENT_TIMESTAMP" },
                { "updated_at", "DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP" },
                { "sid", "INT" },
                { "name", "VARCHAR(255)" }
            }
        },
        {
            "device_group", {
                { "id", "INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY" },
                { "name", "VARCHAR(255)" },
                { "lastupdate", "DATETIME" },
                { "visible", "TINYINT(1) NOT NULL DEFAULT 1" }
            }
        },
        {
            "device_access", {
                { "id", "INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY" },
                { "port_src", "varchar(30)" },
                { "ip_src", "varchar(30)" },
                { "uri", "varchar(30)" },
                { "device", "int unsigned" },
                { "active", "active" }
            }
        }

    };

//    QSqlQuery checkQuery;
//    if (checkQuery.exec("SELECT COUNT(*) FROM record_files WHERE ip IS NULL OR ip = ''") && checkQuery.next()) {
//        int count = checkQuery.value(0).toInt();
//        if (count > 0) {
//            QSqlQuery updateIpQuery;
//            QString updateSql = R"(
//                UPDATE record_files rf
//                JOIN device_station rc
//                  ON CAST(SUBSTRING_INDEX(rf.filename, '_', 1) AS UNSIGNED) = rc.freq
//                SET rf.ip = rc.ip
//                WHERE rf.ip IS NULL OR rf.ip = ''
//            )";
//            if (!updateIpQuery.exec(updateSql)) {
//                qWarning() << "Failed to sync IP to record_files:" << updateIpQuery.lastError().text();
//            } else {
//                qDebug() << "IP addresses synced to record_files based on freq → ip mapping.";
//            }
//        } else {
//            qDebug() << "All record_files already have IPs. No sync needed.";
//        }
//    }


    for (auto it = expectedSchemas.begin(); it != expectedSchemas.end(); ++it) {
        verifyTableSchema(it.key(), it.value());
    }

}


void Database::verifyTableSchema(const QString &tableName, const QMap<QString, QString> &expectedColumns) {
    QSqlQuery query;
    query.prepare(QString("DESCRIBE %1").arg(tableName));
    if (!query.exec()) {
        qWarning() << "DESCRIBE failed for table" << tableName << ":" << query.lastError().text();
        return;
    }

    QSet<QString> existingColumns;
    while (query.next()) {
        existingColumns.insert(query.value("Field").toString());
    }

    for (auto it = expectedColumns.begin(); it != expectedColumns.end(); ++it) {
        if (!existingColumns.contains(it.key())) {
            QString alter = QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(tableName, it.key(), it.value());
            QSqlQuery alterQuery;
            if (!alterQuery.exec(alter)) {
                qWarning() << "Failed to add column" << it.key() << "to" << tableName << ":" << alterQuery.lastError().text();
            } else {
                qDebug() << "Added missing column" << it.key() << "to" << tableName;
            }
        }
    }
}


void Database::cleanupOldRecordFiles() {
    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database for cleanup:" << db.lastError().text();
        return;
    }

    QSqlQuery query;
    QString cleanupSql = R"(
        DELETE FROM record_files
        WHERE created_at < (SELECT MIN(sub.created_at)
                            FROM (
                                SELECT created_at
                                FROM record_files
                                ORDER BY created_at DESC
                                LIMIT 3
                            ) AS sub)
    )";

    if (!query.exec(cleanupSql)) {
        qWarning() << "Cleanup failed:" << query.lastError().text();
    } else {
        qDebug() << "Cleanup completed: old records deleted, only last 3 days kept.";
    }
    db.close();
}


void Database::maybeRunCleanup() {
    QSqlQuery query;
    if (!query.exec("SELECT MIN(created_at) FROM record_files")) return;

    if (query.next()) {
        QDateTime minDate = query.value(0).toDateTime();
        if (minDate.daysTo(QDateTime::currentDateTime()) >= 7) {
            cleanupOldRecordFiles();
        } else {
            qDebug() << "No cleanup needed. Oldest date is within 7 days.";
        }
    }
}


void Database::CheckAndHandleDevice(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "CheckAndHandleDevice:" << jsonString;

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QJsonObject obj = QJsonDocument::fromJson(jsonString.toUtf8()).object();

    int inputSid = obj["sid"].toInt();
    int inputPayloadSize = obj["payload_size"].toInt();
    int inputTerminalType = obj["terminal_type"].toInt();
    QString inputName = obj["name"].toString();
    QString inputIp = obj["ip"].toString();
    QString inputUri = obj["uri"].toString();
    int inputFreq = obj["freq"].toInt();
    QVariant inputAmbient = obj.contains("ambient") && !obj["ambient"].isNull() ? obj["ambient"].toInt() : QVariant();
    int inputGroup = obj["group"].toInt();
    int inputVisible = obj["visible"].toInt();
    QVariant inputLastAccess = obj.contains("last_access") && !obj["last_access"].isNull() ? obj["last_access"].toString() : QVariant();
    int inputChunk = obj["chunk"].toInt();
    QString inputupdated_at = obj["chunk"].toString();

    QSqlQuery query;
    query.prepare(R"(
        SELECT id, sid, payload_size, terminal_type, name, ip, uri, freq,
               ambient, `group`, visible, last_access, storage_path, chunk, updated_at
        FROM device_station
        WHERE sid = :sid
    )");
    query.bindValue(":sid", inputSid);

    if (!query.exec()) {
        qWarning() << "Check device_station failed:" << query.lastError().text();
        return;
    }

    bool deviceFound = false;

    while (query.next()) {
        deviceFound = true;

        int dbSid = query.value("sid").toInt();
        int dbPayload = query.value("payload_size").toInt();
        int dbTermType = query.value("terminal_type").toInt();
        QString dbName = query.value("name").toString();
        QString dbIp = query.value("ip").toString();
        QString dbUri = query.value("uri").toString();
        int dbFreq = query.value("freq").toInt();
        QString dbAmbient = query.value("ambient").isNull() ? "NULL" : query.value("ambient").toString();
        int dbGroup = query.value("group").toInt();
        int dbVisible = query.value("visible").toInt();
        QString dbLastAccess = query.value("last_access").isNull() ? "NULL" : query.value("last_access").toString();
        QString dbStoragePath = query.value("storage_path").toString();
        int dbChunk = query.value("chunk").toInt();
        QVariant dbUpdatedAt = query.value("updated_at").isNull() ? QVariant() : query.value("updated_at");

        if (
            inputPayloadSize != dbPayload ||
            inputTerminalType != dbTermType ||
            inputName != dbName ||
            inputIp != dbIp ||
            inputUri != dbUri ||
            inputFreq != dbFreq ||
            inputAmbient != dbAmbient ||
            inputGroup != dbGroup ||
            inputVisible != dbVisible ||
            inputLastAccess != dbLastAccess ||
            inputChunk != dbChunk ||
            inputupdated_at != dbChunk

        ) {
            qDebug() << "Data mismatch detected. Updating device_station.";
            UpdateDeviceInDatabase(jsonString, wClient);
        } else {
            qDebug() << "Device already exists with matching data. No update needed.";
        }

        break;
    }

    db.close();

    if (!deviceFound) {
        qDebug() << "Device not found. Proceeding to register new device.";
        RegisterDeviceToDatabase(jsonString, wClient);
    }
}


//void Database::CheckAndHandleDevice(const QString& jsonString, QWebSocket* wClient) {
//    qDebug() << "CheckAndHandleDevice:" << jsonString;
//    if (!db.isOpen()) {
//        qDebug() << "Opening database...";
//        if (!db.open()) {
//            qWarning() << "Failed to open database:" << db.lastError().text();
//            return;
//        }
//    }
//    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
//    if (!doc.isObject()) {
//        qWarning() << "Invalid JSON format!";
//        return;
//    }
//    QJsonObject obj = doc.object();
//    QString deviceId = obj.value("deviceId").toString();
//    double inputFrequency = obj.value("frequency").toDouble();
//    QString inputMode = obj.value("mode").toString();
//    QString inputIp = obj.value("ip").toString();
//    int inputTimeInterval = obj.value("timeInterval").toInt();
//    QString inputPathDirectory = obj.value("pathDirectory").toString();
//    QString inputCompanyName = obj.value("companyName").toString();

//    QSqlQuery query;
//    query.prepare("SELECT id, deviceId, frequency, mode, ip, timeInterval, pathDirectory, companyName "
//                  "FROM devices WHERE deviceId = :deviceId");
//    query.bindValue(":deviceId", deviceId);

//    if (!query.exec()) {
//        QString errorText = query.lastError().text();
//        qWarning() << "Check device failed:" << errorText;
//        if (errorText.contains("Lost connection", Qt::CaseInsensitive)) {
//            qWarning() << "Lost connection detected. Restarting MySQL service...";
//            if (restartMySQLService()) {
//                db.close();
//                if (db.open()) {
//                    qDebug() << "Reconnection successful after restarting MySQL.";
//                    query.prepare("SELECT id, deviceId, frequency, mode, ip, timeInterval, pathDirectory, companyName "
//                                  "FROM devices WHERE deviceId = :deviceId");
//                    query.bindValue(":deviceId", deviceId);
//                    if (!query.exec()) {
//                        qWarning() << "Retry check device failed:" << query.lastError().text();
//                        return;
//                    }
//                } else {
//                    qWarning() << "Reconnection failed after restarting MySQL!";
//                    return;
//                }
//            } else {
//                qWarning() << "Failed to restart MySQL service.";
//                return;
//            }
//        } else {
//            return;
//        }
//    }

//    bool deviceFound = false;

//    while (query.next()) {
//        int dbId = query.value(0).toInt();
//        double dbDeviceId = query.value(1).toDouble();
//        double dbFrequency = query.value(2).toDouble();
//        QString dbMode = query.value(3).toString();
//        QString dbIp = query.value(4).toString();
//        int dbTimeInterval = query.value(5).toInt();
//        QString dbPathDirectory = query.value(6).toString();
//        QString dbCompanyName = query.value(7).toString();

//        qDebug() << "Database values fetched:";
//        qDebug() << "Device ID:" << dbDeviceId;
//        qDebug() << "Frequency:" << dbFrequency;
//        qDebug() << "Mode:" << dbMode;
//        qDebug() << "IP:" << dbIp;
//        qDebug() << "Time Interval:" << dbTimeInterval;
//        qDebug() << "Path Directory:" << dbPathDirectory;
//        qDebug() << "Company Name:" << dbCompanyName;

//        if (deviceId == QString::number(dbDeviceId)) {
//            deviceFound = true;

//            if (inputFrequency != dbFrequency || inputMode != dbMode || inputIp != dbIp ||
//                inputTimeInterval != dbTimeInterval || inputPathDirectory != dbPathDirectory ||
//                inputCompanyName != dbCompanyName) {
//                qDebug() << "Data mismatch detected. Updating device.";
//                UpdateDeviceInDatabase(jsonString,wClient);
//            } else {
//                qDebug() << "Device already exists with matching data. No update needed.";
//            }
//            break;
//        }
//    }

//    db.close();

//    if (!deviceFound) {
//        qDebug() << "Device not found. Proceeding to register new device.";
//        RegisterDeviceToDatabase(jsonString,wClient);
//    }

//    db.close();
//}

//void Database::selectRecordChannel(QString jsonString, QWebSocket* wClient) {
//    qDebug() << "selectRecordChannel:" << jsonString;

//    if (!db.isOpen()) {
//        if (!db.open()) {
//            qWarning() << "Failed to open DB in selectRecordChannel:" << db.lastError().text();
//            return;
//        }
//    }

//    // Extract from jsonString
//    QRegularExpression regex(R"((\w+), conn: (\d+), ip: ([\d\.]+), uri: (\w+), freq: (\d+))");
//    QRegularExpressionMatch match = regex.match(jsonString);
//    if (!match.hasMatch()) {
//        qWarning() << "Pattern doesn't match message format.";
//        db.close();
//        return;
//    }

//    QString expectedRecord = match.captured(1);
//    int expectedConn = match.captured(2).toInt();
//    QString ip = match.captured(3);
//    QString expectedUrl = match.captured(4);
//    QString expectedFreq = match.captured(5);

//    QSqlQuery selectQuery(db);
//    selectQuery.prepare(R"(
//        SELECT id, record, conn, ip, url, freq, created_at, updated_at
//        FROM record_channel
//        WHERE ip = :ip
//    )");
//    selectQuery.bindValue(":ip", ip);

//    if (!selectQuery.exec() || !selectQuery.next()) {
//        qWarning() << "Select failed:" << selectQuery.lastError().text();
//        db.close();
//        return;
//    }

//    QString dbRecord = selectQuery.value("record").toString();
//    int dbConn = selectQuery.value("conn").toInt();
//    QString dbUrl = selectQuery.value("url").toString();
//    QString dbFreq = selectQuery.value("freq").toString();

//    if (dbRecord != expectedRecord || dbConn != expectedConn || dbUrl != expectedUrl || dbFreq != expectedFreq) {
//        qWarning() << "Mismatch detected: DB and expected data are not the same.";
//        db.close();
//        return;
//    }

//    QJsonObject json;
//    json["menuID"] = "getRecordChannel";
//    json["id"] = selectQuery.value("id").toInt();
//    json["record"] = dbRecord;
//    json["conn"] = dbConn;
//    json["ip"] = ip;
//    json["url"] = dbUrl;
//    json["freq"] = dbFreq;
//    json["created_at"] = selectQuery.value("created_at").toString();
//    json["updated_at"] = selectQuery.value("updated_at").toString();

//    QJsonDocument doc(json);
//    QString resultJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

//    if (wClient) {
//        wClient->sendTextMessage(resultJson);
//    }

//    qDebug() << "Sent to WebSocket:" << resultJson;
//    db.close();
//}

void Database::selectRecordChannel(QString jsonString, QWebSocket* wClient) {
    qDebug() << "selectRecordChannel:" << jsonString;

    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "Failed to open DB in selectRecordChannel:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    if (!query.exec("SELECT * FROM record_channel")) {
        qWarning() << "Select all failed:" << query.lastError().text();
        db.close();
        return;
    }

    QJsonArray records;

    while (query.next()) {
        QJsonObject record;
        record["id"] = query.value("id").toInt();
        record["action"] = query.value("action").toString();
        record["ip"] = query.value("ip").toString();
        record["url"] = query.value("url").toString();
        record["freq"] = query.value("freq").toString();
        record["created_at"] = query.value("created_at").toString();
        record["updated_at"] = query.value("updated_at").toString();
        record["name"] = query.value("name").toString();
        record["sid"] = query.value("sid").toInt();
        records.append(record);
    }

    QJsonObject response;
    response["menuID"] = "getRecordChannel";
    response["data"] = records;

    QJsonDocument doc(response);
    QString resultJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    if (wClient) {
        wClient->sendTextMessage(resultJson);
        qDebug() << "Sent to WebSocket:" << resultJson;
    } else {
        qWarning() << "WebSocket client is null!";
    }

    db.close();
}



void Database::recordChannel(QString jsonString, QWebSocket* wClient) {
    qDebug() << "recordChannel:" << jsonString;

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QRegularExpression regex(R"((\w+), conn: (\d+), ip: ([\d\.]+), uri: (\w+), freq: (\d+))");
    QRegularExpressionMatch match = regex.match(jsonString);
    if (!match.hasMatch()) {
        qWarning() << "Pattern doesn't match message format.";
        db.close();
        return;
    }

    QString record = match.captured(1);
    int conn = match.captured(2).toInt();
    QString ip = match.captured(3);
    QString url = match.captured(4);
    QString freq = match.captured(5);

    int device_id = -1;
    QSqlQuery idQuery(db);
    idQuery.prepare("SELECT id FROM device_station WHERE ip = :ip");  // ✅ เปลี่ยน sid -> id
    idQuery.bindValue(":ip", ip);
    if (idQuery.exec() && idQuery.next()) {
        device_id = idQuery.value(0).toInt();
        qDebug() << "Found device_id:" << device_id << "for ip:" << ip;
    } else {
        qWarning() << "Failed to find device_id for ip:" << ip;
        db.close();
        return;
    }

    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM record_channel WHERE ip = :ip");
    checkQuery.bindValue(":ip", ip);
    if (!checkQuery.exec() || !checkQuery.next()) {
        qWarning() << "Failed to check existing IP:" << checkQuery.lastError().text();
        db.close();
        return;
    }

    int count = checkQuery.value(0).toInt();

    if (count > 0) {
        QSqlQuery updateQuery(db);
        updateQuery.prepare(R"(
            UPDATE record_channel
            SET conn = :conn, record = :record, url = :url, freq = :freq, sid = :sid, updated_at = NOW()
            WHERE ip = :ip
        )");
        updateQuery.bindValue(":conn", conn);
        updateQuery.bindValue(":record", record);
        updateQuery.bindValue(":url", url);
        updateQuery.bindValue(":freq", freq);
        updateQuery.bindValue(":sid", device_id);  // ❗️ถ้าใน DB ยังใช้ชื่อ sid ก็ bind ลง sid ไปก่อน        updateQuery.bindValue(":ip", ip);
        if (!updateQuery.exec()) {
            qWarning() << "Update failed:" << updateQuery.lastError().text();
            db.close();
            return;
        } else {
            qDebug() << "Updated record for IP:" << ip;
        }
    } else {
        QSqlQuery insertQuery(db);
        insertQuery.prepare(R"(
            INSERT INTO record_channel (record, conn, ip, url, freq, sid, created_at, updated_at)
            VALUES (:record, :conn, :ip, :url, :freq, :sid, NOW(), NOW())
        )");
        insertQuery.bindValue(":record", record);
        insertQuery.bindValue(":conn", conn);
        insertQuery.bindValue(":ip", ip);
        insertQuery.bindValue(":url", url);
        insertQuery.bindValue(":freq", freq);
        insertQuery.bindValue(":sid", device_id);  // ❗️ถ้ายังไม่ rename column
        if (!insertQuery.exec()) {
            qWarning() << "Insert failed:" << insertQuery.lastError().text();
            db.close();
            return;
        } else {
            qDebug() << "Inserted new record for IP:" << ip;
        }
    }

    db.close();
    selectRecordChannel(jsonString, wClient);
}


//void Database::recordChannel(QString jsonString, QWebSocket* wClient) {
//    qDebug() << "recordChannel:" << jsonString;

//    if (!db.isOpen()) {
//        qDebug() << "Opening database...";
//        if (!db.open()) {
//            qWarning() << "Failed to open database:" << db.lastError().text();
//            return;
//        }
//    }

//    QRegularExpression regex(R"((\w+), conn: (\d+), ip: ([\d\.]+), uri: (\w+), freq: (\d+))");
//    QRegularExpressionMatch match = regex.match(jsonString);
//    if (!match.hasMatch()) {
//        qWarning() << "Pattern doesn't match message format.";
//        db.close();
//        return;
//    }

//    QString record = match.captured(1);
//    int conn = match.captured(2).toInt();
//    QString ip = match.captured(3);
//    QString url = match.captured(4);
//    QString freq = match.captured(5);

//    QSqlQuery checkQuery(db);
//    checkQuery.prepare("SELECT COUNT(*) FROM record_channel WHERE ip = :ip");
//    checkQuery.bindValue(":ip", ip);
//    if (!checkQuery.exec() || !checkQuery.next()) {
//        qWarning() << "Failed to check existing IP:" << checkQuery.lastError().text();
//        db.close();
//        return;
//    }

//    int count = checkQuery.value(0).toInt();

//    if (count > 0) {
//        QSqlQuery updateQuery(db);
//        updateQuery.prepare(R"(
//            UPDATE record_channel
//            SET conn = :conn, record = :record, url = :url, freq = :freq, updated_at = NOW()
//            WHERE ip = :ip
//        )");
//        updateQuery.bindValue(":conn", conn);
//        updateQuery.bindValue(":record", record);
//        updateQuery.bindValue(":url", url);
//        updateQuery.bindValue(":freq", freq);
//        updateQuery.bindValue(":ip", ip);
//        if (!updateQuery.exec()) {
//            qWarning() << "Update failed:" << updateQuery.lastError().text();
//            db.close();
//            return;
//        } else {
//            qDebug() << "Updated record for IP:" << ip;
//        }
//    } else {
//        QSqlQuery insertQuery(db);
//        insertQuery.prepare(R"(
//            INSERT INTO record_channel (record, conn, ip, url, freq, created_at, updated_at)
//            VALUES (:record, :conn, :ip, :url, :freq, NOW(), NOW())
//        )");
//        insertQuery.bindValue(":record", record);
//        insertQuery.bindValue(":conn", conn);
//        insertQuery.bindValue(":ip", ip);
//        insertQuery.bindValue(":url", url);
//        insertQuery.bindValue(":freq", freq);
//        if (!insertQuery.exec()) {
//            qWarning() << "Insert failed:" << insertQuery.lastError().text();
//            db.close();
//            return;
//        } else {
//            qDebug() << "Inserted new record for IP:" << ip;
//        }
//    }

//    db.close();
//    selectRecordChannel(jsonString,wClient);
//}


void Database::UpdateDeviceInDatabase(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "UpdateDeviceInDatabase:" << jsonString;

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QJsonObject obj = QJsonDocument::fromJson(jsonString.toUtf8()).object();

    QSqlQuery updateQuery;
    updateQuery.prepare(R"(
        UPDATE device_station
        SET payload_size = :payload_size,
            terminal_type = :terminal_type,
            ip = :ip,
            uri = :uri,
            freq = :freq,
            ambient = :ambient,
            `group` = :groupVal,
            visible = :visible,
            last_access = :last_access,
            name = :name,
            chunk = :chunk,
            updated_at = :updated_at
        WHERE sid = :sid
    )");

    updateQuery.bindValue(":payload_size", obj["payload_size"].toInt());
    updateQuery.bindValue(":terminal_type", obj["terminal_type"].toInt());
    updateQuery.bindValue(":ip", obj["ip"].toString());
    updateQuery.bindValue(":uri", obj["uri"].toString());
    updateQuery.bindValue(":freq", obj["freq"].toInt());
    updateQuery.bindValue(":ambient", obj.contains("ambient") && !obj["ambient"].isNull() ? obj["ambient"].toString() : QVariant());
    updateQuery.bindValue(":groupVal", obj["group"].toInt());
    updateQuery.bindValue(":visible", obj["visible"].toInt());
    updateQuery.bindValue(":last_access", obj.contains("last_access") && !obj["last_access"].isNull() ? obj["last_access"].toString() : QVariant());
    updateQuery.bindValue(":name", obj["name"].toString());
    updateQuery.bindValue(":chunk", obj["chunk"].toInt());
    updateQuery.bindValue(":sid", obj["sid"].toInt());
    updateQuery.bindValue(":updated_at", obj["updated_at"].toString());



    if (!updateQuery.exec()) {
        qWarning() << "Update failed:" << updateQuery.lastError().text();
    } else {
        qDebug() << "Rows affected:" << updateQuery.numRowsAffected();
        if (updateQuery.numRowsAffected() > 0) {
            qDebug() << "Device station updated successfully!";
        } else {
            qWarning() << "Update executed but no rows were modified.";
        }
    }

    db.close();
    getRegisterDevicePage(jsonString, wClient);
}

void Database::RegisterDeviceToDatabase(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "RegisterDeviceToDatabase";

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QJsonObject obj = QJsonDocument::fromJson(jsonString.toUtf8()).object();

    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO device_station
        (sid, payload_size, terminal_type, name, ip, uri, freq, ambient, `group`, visible, last_access, chunk, updated_at)
        VALUES
        (:sid, :payload_size, :terminal_type, :name, :ip, :uri, :freq, :ambient, :groupVal, :visible, :last_access, :chunk, :updated_at)
    )");

    query.bindValue(":sid", obj["sid"].toInt());
    query.bindValue(":payload_size", obj["payload_size"].toInt());
    query.bindValue(":terminal_type", obj["terminal_type"].toInt());
    query.bindValue(":name", obj["name"].toString());
    query.bindValue(":ip", obj["ip"].toString());
    query.bindValue(":uri", obj["uri"].toString());
    query.bindValue(":freq", obj["freq"].toInt());
    query.bindValue(":ambient", obj.contains("ambient") && !obj["ambient"].isNull() ? obj["ambient"].toInt() : QVariant());
    query.bindValue(":groupVal", obj["group"].toInt());
    query.bindValue(":visible", obj["visible"].toInt());
    query.bindValue(":last_access", obj.contains("last_access") && !obj["last_access"].isNull() ? obj["last_access"].toString() : QVariant());
    query.bindValue(":chunk", obj.contains("chunk") && !obj["chunk"].isNull() ? obj["chunk"].toInt() : QVariant());
    query.bindValue(":updated_at", obj.contains("updated_at") && !obj["updated_at"].isNull() ? obj["updated_at"].toString() : QVariant());

    if (!query.exec()) {
        qWarning() << "Insert failed:" << query.lastError().text();
    } else {
        qDebug() << "Device station registered successfully!";
    }
    db.close();
    getRegisterDevicePage(jsonString, wClient);
}



void Database::updatePath(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "updatePath received:" << jsonString;

    QByteArray br = jsonString.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();

    if (obj["menuID"].toString() != "changePathDirectory") {
        qWarning() << "Invalid menuID.";
        return;
    }

    QString newPath = obj["ChangePathDirectory"].toString();
    if (newPath.isEmpty()) {
        qWarning() << "Empty path received.";
        return;
    }

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    // Update all device_station paths
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE device_station SET storage_path = :path");
    updateQuery.bindValue(":path", newPath);

    if (!updateQuery.exec()) {
        qWarning() << "❌ Failed to update storage_path:" << updateQuery.lastError().text();
    } else {
        qDebug() << "✅ Updated device_station.storage_path to:" << newPath;

        // Now sync record_files.file_path
        QSqlQuery syncQuery(db);
        QString syncSql = R"(
            UPDATE record_files rf
            JOIN device_station ds ON rf.name = ds.name
            SET rf.file_path = ds.storage_path
            WHERE rf.file_path IS NOT NULL
        )";
        if (!syncQuery.exec(syncSql)) {
            qWarning() << "❌ Failed to sync file_path:" << syncQuery.lastError().text();
        } else {
            qDebug() << "✅ record_files.file_path synced successfully.";
        }

        // Send WebSocket reply
        QJsonObject reply;
        reply["menuID"] = "UpdatePathDirectory";
        reply["status"] = "success";
        reply["newPath"] = newPath;
        wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }

    db.close();
}

//void Database::updatePath(const QString& jsonString, QWebSocket* wClient) {
//    qDebug() << "updatePath received:" << jsonString;

//    QByteArray br = jsonString.toUtf8();
//    QJsonDocument doc = QJsonDocument::fromJson(br);
//    QJsonObject obj = doc.object();

//    if (obj["menuID"].toString() != "changePathDirectory") {
//        qWarning() << "Invalid menuID.";
//        return;
//    }

//    QString newPath = obj["ChangePathDirectory"].toString();
//    if (newPath.isEmpty()) {
//        qWarning() << "Empty path received.";
//        return;
//    }

//    if (!db.isOpen() && !db.open()) {
//        qWarning() << "Failed to open database:" << db.lastError().text();
//        return;
//    }

//    QSqlQuery updateQuery(db);
//    updateQuery.prepare(R"(
//        UPDATE device_station SET storage_path = :path
//    )");
//    updateQuery.bindValue(":path", newPath);

//    if (!updateQuery.exec()) {
//        qWarning() << "Failed to update storage_path:" << updateQuery.lastError().text();
//    } else {
//        qDebug() << "All device_station rows updated with new storage_path:" << newPath;

//        QJsonObject reply;
//        reply["menuID"] = "UpdatePathDirectory";
//        reply["status"] = "success";
//        reply["newPath"] = newPath;
//        wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
//    }

//    db.close();
//}




void Database::getRegisterDevicePage(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "Fetching all registered device_station...";

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QSqlQuery query("SELECT * FROM device_station");
    QJsonArray deviceArray;

    while (query.next()) {
        QJsonObject deviceObj;
        deviceObj["id"] = query.value("id").toInt();
        deviceObj["sid"] = query.value("sid").toInt();
        deviceObj["payload_size"] = query.value("payload_size").toInt();
        deviceObj["terminal_type"] = query.value("terminal_type").toInt();
        deviceObj["name"] = query.value("name").toString();
        deviceObj["ip"] = query.value("ip").toString();
        deviceObj["uri"] = query.value("uri").toString();
        deviceObj["freq"] = query.value("freq").toInt();

        deviceObj["ambient"] = query.value("ambient").isNull() ? QJsonValue("NULL") : query.value("ambient").toInt();
        deviceObj["group"] = query.value("group").toInt();
        deviceObj["visible"] = query.value("visible").toInt();
        deviceObj["file_path"] = query.value("storage_path").toString();
        deviceObj["last_access"] = query.value("last_access").isNull() ? QJsonValue("NULL") : query.value("last_access").toString();

        // ✅ เพิ่มใหม่
        deviceObj["chunk"] = query.value("chunk").isNull() ? QJsonValue("NULL") : query.value("chunk").toInt();
        deviceObj["updated_at"] = query.value("updated_at").isNull() ? QJsonValue("NULL") : query.value("updated_at").toString();

        deviceArray.append(deviceObj);
    }

    QJsonObject responseObj;
    responseObj["menuID"] = "deviceList";
    responseObj["devices"] = deviceArray;
    QJsonDocument doc(responseObj);
    QString resultJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    if (wClient) {
        wClient->sendTextMessage(resultJson);
        qDebug() << "Sent to WebSocket:" << resultJson;
    }

    db.close();
}

void Database::removeRegisterDevice(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "Removing registered device_station..." << jsonString;

    QJsonObject obj = QJsonDocument::fromJson(jsonString.toUtf8()).object();
    QString IDToDelete = obj["sid"].toString();

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QSqlQuery deleteQuery;
    deleteQuery.prepare("DELETE FROM device_station WHERE sid = :sid");
    deleteQuery.bindValue(":sid", IDToDelete);

    if (!deleteQuery.exec()) {
        qWarning() << "Failed to delete device_station:" << deleteQuery.lastError().text();
    } else {
        qDebug() << "Deleted device_station with id:" << IDToDelete;
    }

    getRegisterDevicePage(jsonString, wClient);
    db.close();
}





//void Database::UpdateDeviceInDatabase(const QString& jsonString, QWebSocket* wClient) {
//    qDebug() << "UpdateDeviceInDatabase:" << jsonString;

//    if (!db.isOpen()) {
//        qDebug() << "Opening database...";
//        if (!db.open()) {
//            qWarning() << "Failed to open database:" << db.lastError().text();
//            return;
//        }
//    }

//    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
//    if (!doc.isObject()) {
//        qWarning() << "Invalid JSON format!";
//        return;
//    }

//    QJsonObject obj = doc.object();
//    QString deviceId = obj.value("deviceId").toString();
//    QString frequency = obj.value("frequency").toString();
//    QString mode = obj.value("mode").toString();
//    QString ip = obj.value("ip").toString();
//    int timeInterval = obj.value("timeInterval").toInt();
//    QString pathDirectory = obj.value("pathDirectory").toString();
//    QString companyName = obj.value("companyName").toString();

//    QSqlQuery updateQuery;
//    updateQuery.prepare("UPDATE devices "
//                        "SET frequency = :frequency, "
//                        "mode = :mode, "
//                        "ip = :ip, "
//                        "timeInterval = :timeInterval, "
//                        "pathDirectory = :pathDirectory, "
//                        "companyName = :companyName "
//                        "WHERE deviceId = :deviceId");

//    updateQuery.bindValue(":frequency", frequency);
//    updateQuery.bindValue(":mode", mode);
//    updateQuery.bindValue(":ip", ip);
//    updateQuery.bindValue(":timeInterval", timeInterval);
//    updateQuery.bindValue(":pathDirectory", pathDirectory);
//    updateQuery.bindValue(":companyName", companyName);
//    updateQuery.bindValue(":deviceId", deviceId);

//    if (!updateQuery.exec()) {
//        qWarning() << "Update failed:" << updateQuery.lastError().text();
//    } else {
//        if (updateQuery.numRowsAffected() > 0) {
//            qDebug() << "Device updated successfully!";
//        } else {
//            qWarning() << "Update executed but no row affected. Check deviceId!";
//        }
//    }
//    db.close();
//    getRegisterDevicePage(jsonString,wClient);
//}

//void Database::RegisterDeviceToDatabase(const QString& jsonString, QWebSocket* wClient) {
//    qDebug() << "RegisterDeviceToDatabase";

//    if (!db.isOpen()) {
//        qDebug() << "Opening database...";
//        if (!db.open()) {
//            qWarning() << "Failed to open database:" << db.lastError().text();
//            return;
//        }
//    }

//    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
//    if (!doc.isObject()) {
//        qWarning() << "Invalid JSON format!";
//        return;
//    }
//    QJsonObject obj = doc.object();
//    QString deviceId = obj.value("deviceId").toString();
//    QString frequency = obj.value("frequency").toString();
//    QString mode = obj.value("mode").toString();
//    QString ip = obj.value("ip").toString();
//    int timeInterval = obj.value("timeInterval").toInt();
//    QString pathDirectory = obj.value("pathDirectory").toString();
//    QString companyName = obj.value("companyName").toString();

//    QSqlQuery query;
//    query.prepare("INSERT INTO devices (deviceId, frequency, mode, ip, timeInterval, pathDirectory, companyName) "
//                  "VALUES (:deviceId, :frequency, :mode, :ip, :timeInterval, :pathDirectory, :companyName)");

//    query.bindValue(":deviceId", deviceId);
//    query.bindValue(":frequency", frequency);
//    query.bindValue(":mode", mode);
//    query.bindValue(":ip", ip);
//    query.bindValue(":timeInterval", timeInterval);
//    query.bindValue(":pathDirectory", pathDirectory);
//    query.bindValue(":companyName", companyName);

//    if (!query.exec()) {
//        qWarning() << "Insert failed:" << query.lastError().text();
//    } else {
//        qDebug() << "Device registered successfully!";
//    }
//    db.close();
//    getRegisterDevicePage(jsonString,wClient);
//}

//void Database::getRegisterDevicePage(const QString& jsonString, QWebSocket* wClient) {
//    qDebug() << "Fetching all registered devices...";

//    if (!db.isOpen()) {
//        qDebug() << "Opening database...";
//        if (!db.open()) {
//            qWarning() << "Failed to open database:" << db.lastError().text();
//            return;
//        }
//    }

//    QSqlQuery query("SELECT * FROM devices");
//    QJsonArray deviceArray;

//    while (query.next()) {
//        QJsonObject deviceObj;
//        deviceObj["id"] = query.value("id").toInt();
//        deviceObj["deviceId"] = query.value("deviceId").toString();
//        deviceObj["frequency"] = query.value("frequency").toString();
//        deviceObj["mode"] = query.value("mode").toString();
//        deviceObj["ip"] = query.value("ip").toString();
//        deviceObj["timeInterval"] = query.value("timeInterval").toInt();
//        deviceObj["pathDirectory"] = query.value("pathDirectory").toString();
//        deviceObj["companyName"] = query.value("companyName").toString();

//        deviceArray.append(deviceObj);
//    }

//    QJsonObject responseObj;
//    responseObj["menuID"] = "deviceList";  // สำหรับ frontend แยกแยะประเภทข้อมูล
//    responseObj["devices"] = deviceArray;

//    QJsonDocument doc(responseObj);
////    wClient->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
//    emit sendRecordFiles(doc.toJson(QJsonDocument::Compact), wClient);

//    db.close();
//}

//void Database::removeRegisterDevice(const QString& jsonString, QWebSocket* wClient) {
//    qDebug() << "Removing registered device..." << jsonString;

//    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
//    QJsonObject obj = doc.object();
////    int deviceIdToDelete = obj["deviceId"].toString().toInt();
//    QString deviceIdToDelete = obj["deviceId"].toString();

//    if (!db.isOpen()) {
//        qDebug() << "Opening database...";
//        if (!db.open()) {
//            qWarning() << "Failed to open database:" << db.lastError().text();
//            return;
//        }
//    }

//    // Step 1: Delete the device
//    QSqlQuery deleteQuery;
//    deleteQuery.prepare("DELETE FROM devices WHERE deviceId = :deviceId");
//    deleteQuery.bindValue(":deviceId", deviceIdToDelete);

//    if (!deleteQuery.exec()) {
//        qWarning() << "Failed to delete device:" << deleteQuery.lastError().text();
//    } else {
//        qDebug() << "Deleted device with deviceId:" << deviceIdToDelete;
//    }

//    // Step 2: Re-fetch the updated list
//    QSqlQuery query("SELECT * FROM devices");
//    QJsonArray deviceArray;

//    while (query.next()) {
//        QJsonObject deviceObj;
//        deviceObj["id"] = query.value("id").toInt();
//        deviceObj["deviceId"] = query.value("deviceId").toString();
//        deviceObj["frequency"] = query.value("frequency").toString();
//        deviceObj["mode"] = query.value("mode").toString();
//        deviceObj["ip"] = query.value("ip").toString();
//        deviceObj["timeInterval"] = query.value("timeInterval").toInt();
//        deviceObj["pathDirectory"] = query.value("pathDirectory").toString();
//        deviceObj["companyName"] = query.value("companyName").toString();

//        deviceArray.append(deviceObj);
//    }

//    QJsonObject responseObj;
//    responseObj["menuID"] = "deviceList";
//    responseObj["devices"] = deviceArray;

//    QJsonDocument responseDoc(responseObj);
//    emit sendRecordFiles(responseDoc.toJson(QJsonDocument::Compact), wClient);

//    db.close();
//}

void Database::fetchAllRecordFiles(QString msgs, QWebSocket* wClient) {
    QByteArray br = msgs.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();

    if (obj["menuID"].toString() != "getRecordFiles")
        return;

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QSqlQuery query("SELECT * FROM record_files ORDER BY created_at DESC");
    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return;
    }

    QJsonArray recordsArray;
    int count = 0;

    while (query.next()) {
        QString filePath = query.value("file_path").toString().trimmed();
        QString filename = query.value("filename").toString().trimmed();

        // ถ้า file_path เป็น NULL จริง ๆ ก็จะเป็นค่าว่าง ""
        QString fullPath = filePath + "/" + filename;

        QJsonObject record;
        record["id"] = QString(query.value("id").toByteArray().toHex());
        record["device"] = query.value("device").toString();
        record["filename"] = filename;
        record["created_at"] = query.value("created_at").toString();
        record["continuous_count"] = query.value("continuous_count").toInt();
        record["file_path"] = filePath;
        record["full_path"] = fullPath;
        record["name"] = query.value("name").toString();

        recordsArray.append(record);
        count++;
        qDebug() << "#" << count << "File:" << filename << "→" << fullPath;
    }

    QJsonObject mainObj;
    mainObj["objectName"] = "recordFilesList";
    mainObj["records"] = recordsArray;
    mainObj["count"] = count;

    QJsonDocument docOut(mainObj);
    emit sendRecordFiles(docOut.toJson(QJsonDocument::Compact), wClient);

    db.close();
}


//void Database::fetchAllRecordFiles(QString msgs, QWebSocket* wClient) {
//    QByteArray br = msgs.toUtf8();
//    QJsonDocument doc = QJsonDocument::fromJson(br);
//    QJsonObject obj = doc.object();

//    if (obj["menuID"].toString() != "getRecordFiles")
//        return;

//    if (!db.isOpen() && !db.open()) {
//        qWarning() << "Failed to open database:" << db.lastError().text();
//        return;
//    }

//    // สร้าง map จากชื่อ device → storage_path
//    QMap<QString, QString> devicePathMap;
//    QSqlQuery queryPath("SELECT name, storage_path FROM device_station");
//    while (queryPath.next()) {
//        QString name = queryPath.value("name").toString();
//        QString path = queryPath.value("storage_path").toString();
//        devicePathMap[name] = path;
//    }

//    QSqlQuery query("SELECT * FROM record_files ORDER BY created_at DESC");
//    if (!query.exec()) {
//        qWarning() << "Query failed:" << query.lastError().text();
//        return;
//    }

//    QJsonArray recordsArray;
//    int count = 0;

//    while (query.next()) {
//        QString device = query.value("device").toString();
//        QString filename = query.value("filename").toString();
//        QString basePath = devicePathMap.value(device, "");

//        // Default fallback path if not found
//        if (basePath.isEmpty()) basePath = "/media/SSD1";

//        // Parse filename: expected format FREQ_YYYYMMDD_CH_...wav
//        QStringList parts = filename.split('_');
//        QString subPath = "unknown";

//        if (parts.size() >= 3) {
//            QString freq = parts[0];
//            QString date = parts[1];
//            QString ch = parts[2];
//            subPath = freq + "/" + date + "/" + ch;
//        }

//        QString fullPath = basePath + "/" + subPath + "/" + filename;

//        QJsonObject record;
//        record["id"] = QString(query.value("id").toByteArray().toHex());
//        record["device"] = device;
//        record["filename"] = filename;
//        record["created_at"] = query.value("created_at").toString();
//        record["continuous_count"] = query.value("continuous_count").toInt();
//        record["file_path"] = basePath + "/" + subPath;
//        record["full_path"] = fullPath;
//        record["name"] = query.value("name").toString();

//        recordsArray.append(record);
//        count++;
//        qDebug() << "#" << count << "File:" << filename << "→" << fullPath;
//    }

//    QJsonObject mainObj;
//    mainObj["objectName"] = "recordFilesList";
//    mainObj["records"] = recordsArray;
//    mainObj["count"] = count;

//    QJsonDocument docOut(mainObj);
//    emit sendRecordFiles(docOut.toJson(QJsonDocument::Compact), wClient);

//    db.close();
//}

//void Database::fetchAllRecordFiles(QString msgs, QWebSocket* wClient) {
//    QByteArray br = msgs.toUtf8();
//    QJsonDocument doc = QJsonDocument::fromJson(br);
//    QJsonObject obj = doc.object();

//    if (obj["menuID"].toString() == "getRecordFiles") {
//        if (!db.isOpen() && !db.open()) {
//            qWarning() << "Failed to open database:" << db.lastError().text();
//            return;
//        }

//        QSqlQuery queryPath;
//        if (!queryPath.exec("SELECT * FROM device_station")) {
//            qWarning() << "Query failed:" << queryPath.lastError().text();
//            return;
//        }

//        QSqlQuery query;
//        if (!query.exec("SELECT * FROM record_files ORDER BY created_at DESC")) {
//            qWarning() << "Query failed:" << query.lastError().text();
//            return;
//        }

//        QJsonArray recordsArray;
//        int count = 0;

//        while (query.next()) {
//            QJsonObject record;
//            record["id"] = QString(query.value("id").toByteArray().toHex());
//            record["device"] = query.value("device").isNull() ? "" : query.value("device").toString();
//            record["filename"] = query.value("filename").isNull() ? "" : query.value("filename").toString();
//            record["created_at"] = query.value("created_at").isNull() ? "" : query.value("created_at").toString();
//            record["continuous_count"] = query.value("continuous_count").isNull() ? 0 : query.value("continuous_count").toInt();
//            record["file_path"] = query.value("file_path").isNull() ? "" : query.value("file_path").toString();
//            QString fullPath = record["file_path"].toString() + "/" + record["filename"].toString();
//            record["full_path"] = fullPath;
////            record["ip"] = query.value("ip").toString();
//            recordsArray.append(record);
//            count++;
//            qDebug() << "#" << count << "File:" << record["filename"].toString();
//        }

//        QJsonObject mainObj;
//        mainObj["objectName"] = "recordFilesList";
//        mainObj["records"] = recordsArray;
//        mainObj["count"] = count;

//        QJsonDocument docOut(mainObj);
//        QByteArray outBytes = docOut.toJson(QJsonDocument::Compact);

////        qDebug() << "Total records sent:" << count;
//        emit sendRecordFiles(outBytes, wClient);

//        db.close();
//    }
//}


void Database::recordVolume(int currentVolume, int level) {
    qDebug() << "recordVolume:" << currentVolume << level;
    db.close();
    if (!db.isValid()) {
        qDebug() << "Creating database connection...";
    }

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE volume_log SET currentVolume = :currentVolume, level = :level WHERE id = 1");
    updateQuery.bindValue(":currentVolume", currentVolume);
    updateQuery.bindValue(":level", level);

    if (!updateQuery.exec()) {
        qWarning() << "Update failed:" << updateQuery.lastError().text();
    } else {
        if (updateQuery.numRowsAffected() > 0) {
            qDebug() << "Device updated successfully!";
        } else {
            qWarning() << "Update executed but no row affected. Check deviceId!";
        }
    }
    db.close();
    updateRecordVolume();
}

void Database::updateRecordVolume() {
    db.close();
    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query("SELECT currentVolume, level FROM volume_log WHERE id = 1");
    if (!query.exec() || !query.next()) {
        qWarning() << "Select failed:" << query.lastError().text();
        return;
    }

    int currentVolume = query.value(0).toInt();
    int level = query.value(1).toInt();

    QJsonObject mainObject;
    mainObject.insert("objectName", "updateRecordVolume");
    mainObject.insert("currentVolume", currentVolume);
    mainObject.insert("level", level);
    QJsonDocument doc(mainObject);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    qDebug() << "JSON Output:" << jsonString;
    db.close();
    emit previousRecordVolume(jsonString);

}


void Database::RemoveFile(const QString& jsonString, QWebSocket* wClient) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON input";
        return;
    }
    QJsonObject obj = doc.object();
    QString fileName = obj["fileName"].toString();
    QString filePath = obj["filePath"].toString();
    if (fileName.isEmpty() || filePath.isEmpty()) {
        qWarning() << "Missing fileName or filePath in input JSON";
        return;
    }
    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }
    QSqlQuery query;
    query.prepare("DELETE FROM record_files WHERE filename = :filename AND file_path = :file_path");
    query.bindValue(":filename", fileName);
//    query.bindValue(":file_path", filePath);
    if (!query.exec()) {
        qWarning() << "Failed to remove record:" << query.lastError().text();
        db.close();
        return;
    }
    qDebug() << "Successfully removed file:" << fileName << "from path:" << filePath;
    db.close();
    QThread::msleep(100);
    QJsonObject mainObject;
    mainObject.insert("menuID", "getRecordFiles");
    mainObject.insert("fileName", fileName);
//    mainObject.insert("filePath", filePath);
    QJsonDocument docOut(mainObject);
    QString jsonmainString = docOut.toJson(QJsonDocument::Compact);
    fetchAllRecordFiles(jsonmainString,wClient);
}


void Database::reloadDatabase()
{
//    system("/etc/init.d/mysql stop");
//    system("/etc/init.d/mysql start");
}

void Database::hashletPersonalize()
{
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    QString filename = "/tmp/newhashlet/personalize.sh";
    QString data = QString("#!/bin/bash\n"
                           "su - nano2g -s /bin/bash -c \"hashlet -b /dev/i2c-2 personalize\"\n"
                           "echo $? > /tmp/newhashlet/personalize.txt\n");
    system("mkdir -p /tmp/newhashlet");
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    arguments << "-c" << QString("sh /tmp/newhashlet/personalize.sh");
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();
    arguments.clear();
}

void Database::genHashKey()
{
   QString mac = "", challenge = "", meta = "", password = "", serial = "";
   QStringList macList = getMac();
   if (macList.size() >= 3){
       Q_FOREACH (QString macStr, macList)
       {
           if (macStr.contains("mac")){
               mac = macStr.split(":").at(1);
           }
           else if(macStr.contains("challenge")){
               challenge = macStr.split(":").at(1);
           }
           else if(macStr.contains("meta")){
               meta = macStr.split(":").at(1);
           }
       }
       password = getPassword().replace("\n","");
       serial = getSerial().replace("\n","");
   }

   updateHashTable(mac, challenge, meta, serial, password);
}
bool Database::checkHashletNotData()
{
    QString mac = "", challenge = "", meta = "", password = "", serial = "";
    QString query = QString("SELECT mac, challenge, meta, password, serial  FROM hashlet LIMIT 1");
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return false;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qWarning() << "c++: ERROR! "  << qry.lastError();
    }else{
        while (qry.next()) {
            mac         = qry.value(0).toString();
            challenge   = qry.value(1).toString();
            meta        = qry.value(2).toString();
            password    = qry.value(3).toString();
            serial      = qry.value(4).toString();
        }
    }
    db.close();

    return ((mac == "")||(challenge == "")||(meta == "")||(serial == "")||(password == ""));
}

void Database::updateHashTable(QString mac, QString challenge ,QString meta, QString serial, QString password)
{
    if ((mac != "")&(challenge != "")&(meta != "")&(serial != "")&(password != "")){
        QString query = QString("UPDATE hashlet SET mac='%1', challenge='%2', meta='%3', serial='%4', password='%5'")
                .arg(mac).arg(challenge).arg(meta).arg(serial).arg(password);
        if (!db.open()) {
            qWarning() << "c++: ERROR! "  << "database error! database can not open.";
            emit databaseError();
            return ;
        }
        QSqlQuery qry;
        qry.prepare(query);
        if (!qry.exec()){
            qWarning() << "c++: ERROR! "  << qry.lastError();
        }
        db.close();
    }
}

QStringList Database::getMac()
{
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    QString filename = "/tmp/newhashlet/getmac.sh";
    QString data = QString("#!/bin/bash\n"
                           "su - nano2g -s /bin/bash -c \"hashlet -b /dev/i2c-2 mac --file /home/nano2g/.hashlet\"\n"
                           "echo $? > /tmp/newhashlet/mac.txt\n");
    system("mkdir -p /tmp/newhashlet");
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    arguments << "-c" << QString("sh /tmp/newhashlet/getmac.sh");
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();
    arguments.clear();
    output = output.replace(" ","");
    return output.split("\n");
}
QString Database::getPassword()
{
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    QString filename = "/tmp/newhashlet/getpassword.sh";
    QString data = QString("#!/bin/bash\n"
                           "su - nano2g -s /bin/bash -c \"echo ifz8zean6969** | hashlet -b /dev/i2c-2 hmac\"\n"
                           "echo $? > /tmp/newhashlet/password.txt\n");
    system("mkdir -p /tmp/newhashlet");
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    arguments << "-c" << QString("sh /tmp/newhashlet/getpassword.sh");
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();
    arguments.clear();
    return output;
}
QString Database::getSerial()
{
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    QString filename = "/tmp/newhashlet/getserial.sh";
    QString data = QString("#!/bin/bash\n"
                           "su - nano2g -s /bin/bash -c \"hashlet -b /dev/i2c-2 serial-num\"\n"
                           "echo $? > /tmp/newhashlet/password.txt\n");
    system("mkdir -p /tmp/newhashlet");
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    arguments << "-c" << QString("sh /tmp/newhashlet/getserial.sh");
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();
    arguments.clear();
    return output;
}

bool Database::passwordVerify(QString password){
    QString query = QString("SELECT password FROM hashlet LIMIT 1");
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return false;
    }
    QString hashPassword;
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            hashPassword = qry.value(0).toString();
        }
    }
    db.close();
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    arguments.clear();
    arguments << "-c" << QString("echo %1 | hashlet hmac").arg(password);
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(3000);
    output = getAddressProcess.readAll();
    if (output == "") {
        qDebug() << "output == \"\"";
        return false;
    }else if(!output.contains(hashPassword)){
        qDebug() << "output != hashPassword";
        return false;
    }

    system("mkdir -p /etc/ed137");
    if (verifyMac()){
        qDebug() << "mac true";


        if (hashPassword != ""){
            QString filename = "/etc/ed137/checkpass.sh";
            QString data = QString("#!/bin/bash\n"
                                   "su - nano2g -s /bin/bash -c \"echo $1 | hashlet offline-hmac -r $2\"\n"
                                   "echo $? > /etc/ed137/checkpass\n");
            system("mkdir -p /etc/ed137");

            QByteArray dataAyyay(data.toLocal8Bit());
            QFile file(filename);
            file.open(QIODevice::WriteOnly | QIODevice::Text);
            QTextStream out(&file);
            out << dataAyyay;
            file.close();
            arguments.clear();
            arguments << "-c" << QString("sh /etc/ed137/checkpass.sh %1 %2").arg(password).arg(hashPassword);
            getAddressProcess.start(prog , arguments);
            getAddressProcess.waitForFinished(-1);
            output = getAddressProcess.readAll();
            qDebug() << output;

            arguments.clear();
            arguments << "-c" << QString("cat /etc/ed137/checkpass");
            getAddressProcess.start(prog , arguments);
            getAddressProcess.waitForFinished(-1);
            output = getAddressProcess.readAll();
            qDebug() << output;
            system("rm -r /etc/ed137");
            if (output.contains("0\n")){
                return true;
            }
            return false;
        }

    }else{
        qDebug() << "mac false";
    }
    system("rm -r /etc/ed137");
    return false;
}

bool Database::verifyMac(){
    QString query = QString("SELECT mac, challenge FROM hashlet LIMIT 1");
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return false;
    }
    QString mac, challenge;
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            mac = qry.value(0).toString();
            challenge = qry.value(1).toString();
        }
    }
    db.close();

    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    QString filename = "/etc/ed137/checkmac.sh";
    QString data = QString("#!/bin/bash\n"
                           "su - nano2g -s /bin/bash -c \"hashlet offline-verify -c $1 -r $2\"\n"
                           "echo $? > /etc/ed137/checkmac\n");
    system("mkdir -p /etc/ed137");
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    arguments << "-c" << QString("sh /etc/ed137/checkmac.sh %1 %2").arg(challenge).arg(mac);
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();

    arguments.clear();
    arguments << "-c" << QString("cat /etc/ed137/checkmac");
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();
    arguments.clear();

    if (output.contains("0\n"))
        return true;
    return false;
}

bool Database::database_createConnection()
{
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        //emit databaseError();
        return false;
    }
    db.close();
    qDebug() << "Database connected";
    return true;
}
qint64 Database::getTimeDuration(QString filePath)
{
#ifdef HWMODEL_JSNANO
    QString query = QString("SELECT timestamp FROM fileCATISAudio WHERE path='%1' LIMIT 1").arg(filePath);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return 0;

    }
    QDateTime timestamp;
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            timestamp = qry.value(0).toDateTime();
        }
    }
    db.close();
    qint64 duration = QDateTime::currentDateTime().toSecsSinceEpoch() - timestamp.toSecsSinceEpoch();
    if (duration <= 0) duration=5;
    return duration;
#else
    return 0;
#endif

}
void Database::getLastEvent()
{
#ifdef HWMODEL_JSNANO
    QString lastEvent;
    QDateTime timestamp;
    int timeDuration;
    int id;
    QString query = QString("SELECT timestamp, event, id, duration_sec FROM fileCATISAudio ORDER BY id DESC LIMIT 1");
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return ;

    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            timestamp = qry.value(0).toDateTime();
            lastEvent = qry.value(1).toString();
            id = qry.value(2).toInt();
            timeDuration = qry.value(3).toInt();
        }
    }
    db.close();

    if ((lastEvent == "Standby") & (timeDuration == 0)){
        qint64 duration = QDateTime::currentDateTime().toSecsSinceEpoch() - timestamp.toSecsSinceEpoch();
        QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString query = QString("UPDATE fileCATISAudio SET duration_sec='%1' WHERE id='%2'").arg(duration).arg(id);
        if (!db.open()) {
            qWarning() << "c++: ERROR! "  << "database error! database can not open.";
            emit databaseError();
            return ;
        }
        QSqlQuery qry;
        qry.prepare(query);
        if (!qry.exec()){
            qDebug() << qry.lastError();
        }
        db.close();
    }
#else
    return;
#endif
}
void Database::startProject(QString filePath, QString radioEvent)
{

#ifdef HWMODEL_JSNANO
    QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString query = QString("INSERT INTO fileCATISAudio (path, timestamp, duration_sec, event) "
                            "VALUES ('%1', '%2', '%3', '%4')").arg(filePath).arg(timeStamp).arg(0).arg(radioEvent);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return ;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }
    db.close();
#else
    return;
#endif
}

void Database::insertNewAudioRec(QString filePath, QString radioEvent)
{
#ifdef HWMODEL_JSNANO
    if (radioEvent != "Standby")
    {
        getLastEvent();
    }
    QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString query = QString("INSERT INTO fileCATISAudio (path, timestamp, duration_sec, event) "
                            "VALUES ('%1', '%2', '%3', '%4')").arg(filePath).arg(timeStamp).arg(0).arg(radioEvent);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return ;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }
    db.close();
#else
    return;
#endif
}

void Database::updateAudioRec(QString filePath, float avg_level, float max_level)
{
#ifdef HWMODEL_JSNANO
    qint64 duration = getTimeDuration(filePath);
    QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString query = QString("UPDATE fileCATISAudio SET duration_sec='%1',avg_level=%2, max_level=%3 WHERE path='%4'").arg(duration).arg(avg_level).arg(max_level).arg(filePath);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return ;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }
    db.close();
#else
    return;
#endif
}
void Database::removeAudioFile(int lastMin)
{
#ifdef HWMODEL_JSNANO
    QString filePath = "";
    QString timestamp = QDateTime::currentDateTime().addSecs(-(60*lastMin)).toString("yyyy-MM-dd hh:mm:ss");
    QString query = QString("SELECT path FROM fileCATISAudio WHERE timestamp<'%1' ORDER BY id ASC").arg(timestamp);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return ;

    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            filePath = qry.value(0).toString();
            if (filePath.contains("/home/pi/")){
                QString commanRm = QString("rm -f %1*").arg(filePath);
                system(commanRm.toStdString().c_str());
            }
        }
    }
    query = QString("DELETE FROM fileCATISAudio WHERE timestamp<'%1'").arg(timestamp);
    qry.prepare(query);
    if (!qry.exec()){
       qDebug() << qry.lastError();
    }else{
       while (qry.next()) {
           filePath = qry.value(0).toString();
           QString commanRm = QString("rm -f %1*").arg(filePath);
           system(commanRm.toStdString().c_str());
       }
    }
    db.close();
#else
    return;
#endif
}

QString Database::getNewFile(int warnPercentFault)
{
#ifdef HWMODEL_JSNANO
    QString filePath = "";
    QString query = QString("SELECT path, id FROM fileCATISAudio WHERE event='PTT On' AND id>%1 AND avg_level>%2 ORDER BY id ASC LIMIT 1").arg(currentFileID).arg(warnPercentFault);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return "";

    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            filePath = qry.value(0).toString();
            currentFileID = qry.value(1).toInt();
        }
    }
    db.close();
    return filePath;
#else
    return "";
#endif
}

qint64 Database::getStandbyDuration()
{
#ifdef HWMODEL_JSNANO
    qint64 duration_sec = 0;
    QString query = QString("SELECT duration_sec, id FROM fileCATISAudio WHERE event='Standby' AND id>%1  ORDER BY id ASC LIMIT 1").arg(currentFileID);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return 0;

    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            duration_sec = qry.value(0).toLongLong();
            currentFileID = qry.value(1).toInt();
        }
    }
    db.close();
    return duration_sec;
#else
    return 0;
#endif
}

bool Database::getLastEventCheckAudio(int time, int percentFault, int lastPttMinute)
{
#ifdef HWMODEL_JSNANO
//    qDebug() << "check Last Event And Audio Fault.";
    float avg_level = 0;
    float max_level = 0;
    float last_avg_level = 0;
    float last_max_level = 0;
    QDateTime timestamp = QDateTime::fromSecsSinceEpoch(0);
    QString refDateTime = QDateTime::currentDateTime().addSecs(-(60*lastPttMinute)).toString("yyyy-MM-dd hh:mm:ss");
    float count = 0;
    QString query = QString("SELECT avg_level, max_level, timestamp FROM fileCATISAudio WHERE event='PTT On' AND timestamp>'%2' ORDER BY timestamp DESC LIMIT %1").arg(time).arg(refDateTime);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return false;

    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            avg_level += qry.value(0).toFloat();
            max_level += qry.value(1).toFloat();
            last_avg_level = qry.value(0).toFloat();
            last_max_level = qry.value(1).toFloat();
            if (qry.value(2).toDateTime() > timestamp)
                timestamp = qry.value(2).toDateTime();
            count += 1;
        }
    }
    db.close();

    avg_level = avg_level/count;
    max_level = max_level/count;

    if ((last_avg_level >= percentFault) & (QDateTime::currentDateTime().addSecs(-(lastPttMinute*60)) > timestamp)) {
        emit audioFault(false);
        return true;
    }

    if (avg_level < percentFault) {
        emit audioFault(true);
        return false;
    }

    if (QDateTime::currentDateTime().addSecs(-(lastPttMinute*60)) > timestamp) {
        emit audioFault(true);
        return false;
    }
    emit audioFault(false);
    return true;
#else
    return false;
#endif
}
