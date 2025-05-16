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

void Database::CheckAndHandleDevice(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "CheckAndHandleDevice:" << jsonString;
    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format!";
        return;
    }
    QJsonObject obj = doc.object();
    QString deviceId = obj.value("deviceId").toString();
    double inputFrequency = obj.value("frequency").toDouble();
    QString inputMode = obj.value("mode").toString();
    QString inputIp = obj.value("ip").toString();
    int inputTimeInterval = obj.value("timeInterval").toInt();
    QString inputPathDirectory = obj.value("pathDirectory").toString();
    QString inputCompanyName = obj.value("companyName").toString();

    QSqlQuery query;
    query.prepare("SELECT id, deviceId, frequency, mode, ip, timeInterval, pathDirectory, companyName "
                  "FROM devices WHERE deviceId = :deviceId");
    query.bindValue(":deviceId", deviceId);

    if (!query.exec()) {
        QString errorText = query.lastError().text();
        qWarning() << "Check device failed:" << errorText;
        if (errorText.contains("Lost connection", Qt::CaseInsensitive)) {
            qWarning() << "Lost connection detected. Restarting MySQL service...";
            if (restartMySQLService()) {
                db.close();
                if (db.open()) {
                    qDebug() << "Reconnection successful after restarting MySQL.";
                    query.prepare("SELECT id, deviceId, frequency, mode, ip, timeInterval, pathDirectory, companyName "
                                  "FROM devices WHERE deviceId = :deviceId");
                    query.bindValue(":deviceId", deviceId);
                    if (!query.exec()) {
                        qWarning() << "Retry check device failed:" << query.lastError().text();
                        return;
                    }
                } else {
                    qWarning() << "Reconnection failed after restarting MySQL!";
                    return;
                }
            } else {
                qWarning() << "Failed to restart MySQL service.";
                return;
            }
        } else {
            return;
        }
    }

    bool deviceFound = false;

    while (query.next()) {
        int dbId = query.value(0).toInt();
        double dbDeviceId = query.value(1).toDouble();
        double dbFrequency = query.value(2).toDouble();
        QString dbMode = query.value(3).toString();
        QString dbIp = query.value(4).toString();
        int dbTimeInterval = query.value(5).toInt();
        QString dbPathDirectory = query.value(6).toString();
        QString dbCompanyName = query.value(7).toString();

        qDebug() << "Database values fetched:";
        qDebug() << "Device ID:" << dbDeviceId;
        qDebug() << "Frequency:" << dbFrequency;
        qDebug() << "Mode:" << dbMode;
        qDebug() << "IP:" << dbIp;
        qDebug() << "Time Interval:" << dbTimeInterval;
        qDebug() << "Path Directory:" << dbPathDirectory;
        qDebug() << "Company Name:" << dbCompanyName;

        if (deviceId == QString::number(dbDeviceId)) {
            deviceFound = true;

            if (inputFrequency != dbFrequency || inputMode != dbMode || inputIp != dbIp ||
                inputTimeInterval != dbTimeInterval || inputPathDirectory != dbPathDirectory ||
                inputCompanyName != dbCompanyName) {
                qDebug() << "Data mismatch detected. Updating device.";
                UpdateDeviceInDatabase(jsonString,wClient);
            } else {
                qDebug() << "Device already exists with matching data. No update needed.";
            }
            break;
        }
    }

    db.close();

    if (!deviceFound) {
        qDebug() << "Device not found. Proceeding to register new device.";
        RegisterDeviceToDatabase(jsonString,wClient);
    }

    db.close();
}



void Database::UpdateDeviceInDatabase(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "UpdateDeviceInDatabase:" << jsonString;

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format!";
        return;
    }

    QJsonObject obj = doc.object();
    QString deviceId = obj.value("deviceId").toString();
    QString frequency = obj.value("frequency").toString();
    QString mode = obj.value("mode").toString();
    QString ip = obj.value("ip").toString();
    int timeInterval = obj.value("timeInterval").toInt();
    QString pathDirectory = obj.value("pathDirectory").toString();
    QString companyName = obj.value("companyName").toString();

    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE devices "
                        "SET frequency = :frequency, "
                        "mode = :mode, "
                        "ip = :ip, "
                        "timeInterval = :timeInterval, "
                        "pathDirectory = :pathDirectory, "
                        "companyName = :companyName "
                        "WHERE deviceId = :deviceId");

    updateQuery.bindValue(":frequency", frequency);
    updateQuery.bindValue(":mode", mode);
    updateQuery.bindValue(":ip", ip);
    updateQuery.bindValue(":timeInterval", timeInterval);
    updateQuery.bindValue(":pathDirectory", pathDirectory);
    updateQuery.bindValue(":companyName", companyName);
    updateQuery.bindValue(":deviceId", deviceId);

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
    getRegisterDevicePage(jsonString,wClient);
}

void Database::RegisterDeviceToDatabase(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "RegisterDeviceToDatabase";

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format!";
        return;
    }
    QJsonObject obj = doc.object();
    QString deviceId = obj.value("deviceId").toString();
    QString frequency = obj.value("frequency").toString();
    QString mode = obj.value("mode").toString();
    QString ip = obj.value("ip").toString();
    int timeInterval = obj.value("timeInterval").toInt();
    QString pathDirectory = obj.value("pathDirectory").toString();
    QString companyName = obj.value("companyName").toString();

    QSqlQuery query;
    query.prepare("INSERT INTO devices (deviceId, frequency, mode, ip, timeInterval, pathDirectory, companyName) "
                  "VALUES (:deviceId, :frequency, :mode, :ip, :timeInterval, :pathDirectory, :companyName)");

    query.bindValue(":deviceId", deviceId);
    query.bindValue(":frequency", frequency);
    query.bindValue(":mode", mode);
    query.bindValue(":ip", ip);
    query.bindValue(":timeInterval", timeInterval);
    query.bindValue(":pathDirectory", pathDirectory);
    query.bindValue(":companyName", companyName);

    if (!query.exec()) {
        qWarning() << "Insert failed:" << query.lastError().text();
    } else {
        qDebug() << "Device registered successfully!";
    }
    db.close();
    getRegisterDevicePage(jsonString,wClient);
}

void Database::getRegisterDevicePage(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "Fetching all registered devices...";

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query("SELECT * FROM devices");
    QJsonArray deviceArray;

    while (query.next()) {
        QJsonObject deviceObj;
        deviceObj["id"] = query.value("id").toInt();
        deviceObj["deviceId"] = query.value("deviceId").toString();
        deviceObj["frequency"] = query.value("frequency").toString();
        deviceObj["mode"] = query.value("mode").toString();
        deviceObj["ip"] = query.value("ip").toString();
        deviceObj["timeInterval"] = query.value("timeInterval").toInt();
        deviceObj["pathDirectory"] = query.value("pathDirectory").toString();
        deviceObj["companyName"] = query.value("companyName").toString();

        deviceArray.append(deviceObj);
    }

    QJsonObject responseObj;
    responseObj["menuID"] = "deviceList";  // สำหรับ frontend แยกแยะประเภทข้อมูล
    responseObj["devices"] = deviceArray;

    QJsonDocument doc(responseObj);
//    wClient->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    emit sendRecordFiles(doc.toJson(QJsonDocument::Compact), wClient);

    db.close();
}

void Database::removeRegisterDevice(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "Removing registered device..." << jsonString;

    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    QJsonObject obj = doc.object();
//    int deviceIdToDelete = obj["deviceId"].toString().toInt();
    QString deviceIdToDelete = obj["deviceId"].toString();

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    // Step 1: Delete the device
    QSqlQuery deleteQuery;
    deleteQuery.prepare("DELETE FROM devices WHERE deviceId = :deviceId");
    deleteQuery.bindValue(":deviceId", deviceIdToDelete);

    if (!deleteQuery.exec()) {
        qWarning() << "Failed to delete device:" << deleteQuery.lastError().text();
    } else {
        qDebug() << "Deleted device with deviceId:" << deviceIdToDelete;
    }

    // Step 2: Re-fetch the updated list
    QSqlQuery query("SELECT * FROM devices");
    QJsonArray deviceArray;

    while (query.next()) {
        QJsonObject deviceObj;
        deviceObj["id"] = query.value("id").toInt();
        deviceObj["deviceId"] = query.value("deviceId").toString();
        deviceObj["frequency"] = query.value("frequency").toString();
        deviceObj["mode"] = query.value("mode").toString();
        deviceObj["ip"] = query.value("ip").toString();
        deviceObj["timeInterval"] = query.value("timeInterval").toInt();
        deviceObj["pathDirectory"] = query.value("pathDirectory").toString();
        deviceObj["companyName"] = query.value("companyName").toString();

        deviceArray.append(deviceObj);
    }

    QJsonObject responseObj;
    responseObj["menuID"] = "deviceList";
    responseObj["devices"] = deviceArray;

    QJsonDocument responseDoc(responseObj);
    emit sendRecordFiles(responseDoc.toJson(QJsonDocument::Compact), wClient);

    db.close();
}



void Database::fetchAllRecordFiles(QString msgs, QWebSocket* wClient) {
    QByteArray br = msgs.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();

    if (obj["menuID"].toString() == "getRecordFiles") {
        if (!db.isOpen() && !db.open()) {
            qWarning() << "❌ Failed to open database:" << db.lastError().text();
            return;
        }

        QSqlQuery query;
        if (!query.exec("SELECT * FROM record_files")) {
            qWarning() << "❌ Query failed:" << query.lastError().text();
            return;
        }

        QJsonArray recordsArray;
        int count = 0;

        while (query.next()) {
            QJsonObject record;
            record["id"] = QString(query.value("id").toByteArray().toHex());
            record["device"] = query.value("device").isNull() ? "" : query.value("device").toString();
            record["filename"] = query.value("filename").isNull() ? "" : query.value("filename").toString();
            record["created_at"] = query.value("created_at").isNull() ? "" : query.value("created_at").toString();
            record["continuous_count"] = query.value("continuous_count").isNull() ? 0 : query.value("continuous_count").toInt();
            record["file_path"] = query.value("file_path").isNull() ? "" : query.value("file_path").toString();
            QString fullPath = record["file_path"].toString() + "/" + record["filename"].toString();
            record["full_path"] = fullPath;
            recordsArray.append(record);
            count++;
            qDebug() << "#" << count << "File:" << record["filename"].toString();
        }

        QJsonObject mainObj;
        mainObj["objectName"] = "recordFilesList";
        mainObj["records"] = recordsArray;

        QJsonDocument docOut(mainObj);
        QByteArray outBytes = docOut.toJson(QJsonDocument::Compact);

        qDebug() << "Total records sent:" << count;
        emit sendRecordFiles(outBytes, wClient);

        db.close();
    }
}


void Database::recordVolume(int currentVolume, int level) {
    qDebug() << "recordVolume:" << currentVolume << level;

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query;
    query.prepare("UPDATE volume_log SET currentVolume = :currentVolume, level = :level WHERE id = 1");
    query.bindValue(":currentVolume", currentVolume);
    query.bindValue(":level", level);

    if (!query.exec()) {
        qWarning() << "Update failed:" << query.lastError().text();
    } else {
        qDebug() << "Volume record updated.";
    }

    db.close();
    updateRecordVolume();
}

void Database::updateRecordVolume() {
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
    query.bindValue(":file_path", filePath);
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
    mainObject.insert("filePath", filePath);
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
