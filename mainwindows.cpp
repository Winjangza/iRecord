#include "mainwindows.h"
#include <QDebug>
// CREATE USER 'orinnx'@'localhost' IDENTIFIED BY 'OrinX!2024';

mainwindows::mainwindows(QObject *parent)
    : QObject(parent)
{
    qDebug() << "hello world";
    SocketServer = new ChatServer(1234);
    mysql = new Database("iRecord", "orinnx", "OrinX!2024", "localhost", this);
    mydatabase = new Database("recorder", "root", "OTL324$", "localhost", this);
    networking = new NetworkMng(this);
    displayInfo = new infoDisplay(this);
    storageTimer = new QTimer(this);
    max31760 = new MAX31760(this);
    QTimer *timer = new QTimer(this);
    dashboardTimer = new QTimer(this);
    wClient = new QWebSocket();
    Timer = new QTimer();
    client = new SocketClient();
    QTimer *diskTimer = new QTimer(this);
    RotaryEncoderButtonL = new GetInputEvent("/dev/input/by-path/platform-gpio-keys-event",1,28,this);
    RotaryEncoder01 = new GetInputEvent("/dev/input/by-path/platform-rotary@1-event",2,1,this);
    gpioAmp = new GPIOClass(CODEC_RESET_PIN);
    gpioAmp->export_gpio();
    gpioAmp->setdir_gpio("out");
    gpioAmp->setval_gpio(false);
    QThread::msleep(75);
    gpioAmp->setval_gpio(true);
    QThread::msleep(75);
    max9850 = new Max9850("7", 0x10);
    uint32_t sysclkHz = 12000000;
    uint32_t sampleRate = 8000;
//     Connect WebSocketClient to localhost:1235
    client->createConnection("127.0.0.1", 1236);

    connect(client, &SocketClient::Connected, this, []() {
        qDebug() << "mainwindows: SocketClient connected!";
    });
    connect(client, &SocketClient::newCommandProcess, this, [=](QString msg) {
        qDebug() << "mainwindows: Message received via SocketClient:" << msg;

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj["menuID"] == "rec_event") {
                QJsonObject request;
                request["menuID"] = "requestIpDevice";
                QJsonDocument requestDoc(request);
                QString requestMsg = QString::fromUtf8(requestDoc.toJson(QJsonDocument::Compact));
                client->sendMessage(requestMsg);
                qDebug() << "Sent requestIpDevice after successful connection.";
            }
        }
    });;

    connect(wClient, &QWebSocket::connected, this, []() {
    qDebug() << "WebSocket connected!";});
    wClient->open(QUrl("ws://localhost:1234"));
//--------------------------------------mainwindows------------------------------------------------//
    connect(this, &mainwindows::requestUpdateDateTime, this, &mainwindows::getDateTime);
    connect(this, SIGNAL(mainwindowsMesg(QString, QWebSocket*)), SocketServer, SLOT(sendMessage(QString, QWebSocket*)));
//-----------------------------------RotaryEncoderButton----------------------------------------------//
    connect(RotaryEncoderButtonL,SIGNAL(eventCode(int,int,int)),this,SLOT(rotary(int,int,int)));
    connect(RotaryEncoder01,SIGNAL(eventCode(int,int,int)),this,SLOT(rotary(int,int,int)));
//--------------------------------------networking------------------------------------------------//
    connect(networking, SIGNAL(networkmsg( QString)), this, SLOT(mesgManagement( QString)));
    connect(this, &mainwindows::updateNetwork, networking, &NetworkMng::setStaticIpAddr3);
//    connect(this, SIGNAL(updateNetwork(QString,QString,QString,QString,QString,QString,QString)), networking, SLOT(setStaticIpAddr3(QString,QString,QString,QString,QString,QString)));
//-------------------------------------mydatabase------------------------------------------------//
    connect(mysql, SIGNAL(previousRecordVolume( QString)), this, SLOT(updateNewVolumeRecord( QString)));
    connect(mydatabase, SIGNAL(sendRecordFiles(QString, QWebSocket*)), SocketServer, SLOT(sendMessage(QString, QWebSocket*)));
//--------------------------------------WebServer------------------------------------------------//
    connect(SocketServer, SIGNAL(newCommandProcess(QString, QWebSocket*)), this, SLOT(manageData(QString, QWebSocket*)));
    connect(SocketServer, SIGNAL(newCommandProcess(QString, QWebSocket*)), this, SLOT(dashboardManage(QString, QWebSocket*)));

    connect(dashboardTimer, &QTimer::timeout, this, [=]() {dashboardManage(dashboardMsg, clientSocket);});

    int ret;
    ret=pthread_create(&idThread, NULL, ThreadFunc, this);
    if(ret==0){
        qDebug() <<("Thread created successfully.\n");
    }
    else{
        qDebug() <<("Thread not created.\n");
    }

    ret=pthread_create(&idThread2, NULL, ThreadFunc2, this);
    if(ret==0){
        qDebug() <<("Thread2 created successfully.\n");
    }
    else{
        qDebug() <<("Thread2 not created.\n");
    }

    ret=pthread_create(&idThread3, NULL, ThreadFunc3, this);
    if(ret==0){
        qDebug() <<("Thread3 created successfully.\n");
    }
    else{
        qDebug() <<("Thread3 not created.\n");
    }

    if (max9850->initialize(sampleRate)) {
        qDebug() << "MAX9850 initialized successfully";
    } else {
        qWarning() << "Failed to initialize MAX9850";
    }
    networking->getAddress("bond0");
//    displayThread();
    displayVolumes();
    verifySSDandBackupFile();
}

void mainwindows::verifySSDandBackupFile() {
    struct statvfs buf;

    QStringList devices = {"/dev/nvme1n1p1", "/dev/nvme2n1p1"};
    QStringList mountPoints = {"/media/SSD1", "/media/SSD2"};

    for (int i = 0; i < devices.size(); ++i) {
        const QString &device = devices[i];
        const QString &mountPoint = mountPoints[i];

        // 1. Check if device exists
        if (!QFile::exists(device)) {
            qDebug() << "Device not found:" << device;
            continue;
        }

        // 2. Check if already mounted
        bool alreadyMounted = false;
        QFile mounts("/proc/mounts");
        if (mounts.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&mounts);
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.startsWith(device)) {
                    alreadyMounted = true;
                    break;
                }
            }
            mounts.close();
        }

        // 3. Mount if not already mounted
        if (!alreadyMounted) {
            QDir().mkpath(mountPoint); // Ensure mount point exists
            QString cmd = QString("mount %1 %2").arg(device, mountPoint);
            int result = system(cmd.toUtf8().data());
            if (result == 0) {
                qDebug() << "Mounted" << device << "to" << mountPoint;
            } else {
                qDebug() << "Failed to mount" << device;
            }
        } else {
            qDebug() << "Already mounted:" << device;
        }
    }
}




void mainwindows::dashboardManage(QString msgs, QWebSocket* wClient) {
    QByteArray br = msgs.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();

    if (obj["menuID"].toString() == "getMonitorPage") {
        dashboardMsg = msgs;
        clientSocket = wClient;
        // DateTime
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString formattedDateTime = currentDateTime.toString("yyyy/MM/dd HH:mm:ss");

        // Storage
        QStorageInfo storage = QStorageInfo::root();
        double usedGB = (storage.bytesTotal() - storage.bytesAvailable()) / (1024.0 * 1024.0 * 1024.0);
        double totalGB = storage.bytesTotal() / (1024.0 * 1024.0 * 1024.0);
        QString storageText = QString("SSD: %1 / %2 GB")
                                .arg(QString::number(usedGB, 'f', 0))
                                .arg(QString::number(totalGB, 'f', 0));

        // RAM
        QProcess ramProcess;
        ramProcess.start("bash", QStringList() << "-c" << "free -m | awk '/Mem:/ {print $3 \" / \" $2 \" MB\"}'");
        ramProcess.waitForFinished();
        QString ramOutput = ramProcess.readAllStandardOutput().trimmed();
        QStringList ramParts = ramOutput.split(" / ");
        QString ramUsed = ramParts.value(0);
        QString ramTotal = ramParts.value(1);
//        qDebug() << "ramUsed:" << ramUsed << "ramTotal:" << ramTotal;

        // SWAP
        QProcess swapProcess;
        swapProcess.start("bash", QStringList() << "-c" << "free -m | awk '/Swap:/ {print $3 \" / \" $2 \" MB\"}'");
        swapProcess.waitForFinished();
        QString swapOutput = swapProcess.readAllStandardOutput().trimmed();
        QStringList swapParts = swapOutput.split(" / ");
        QString swapUsed = swapParts.value(0);
        QString swapTotal = swapParts.value(1);
//        qDebug() << "swapUsed:" << swapUsed << "swapTotal:" << swapTotal;

        // CPU Usage per core (using %usr)
        QProcess cpuUsageProcess;
        cpuUsageProcess.start("bash", QStringList() << "-c" << "mpstat -P ALL 1 1 | grep -E '^[0-9]' | awk '{print $3 \"%\"}'");
        cpuUsageProcess.waitForFinished();
        QString cpuUsageOutput = cpuUsageProcess.readAllStandardOutput().trimmed();
        QStringList cpuUsageList;
        QStringList cpuLines = cpuUsageOutput.split("\n", Qt::SkipEmptyParts);
        for (int i = 1; i < cpuLines.size(); ++i) {
            cpuUsageList.append(cpuLines[i].trimmed());
        }
//        qDebug() << "cpuUsageList:" << cpuUsageList;

        // CPU & GPU Temperature
        QProcess tempProcess;
        tempProcess.start("bash", QStringList() << "-c" << "cat /sys/class/thermal/thermal_zone0/temp /sys/class/thermal/thermal_zone1/temp");
        tempProcess.waitForFinished();
        QString tempOutput = tempProcess.readAllStandardOutput().trimmed();
        QStringList tempParts = tempOutput.split("\n");
        double cpuTemp = tempParts.value(0).toDouble() / 1000.0; // Convert to Celsius
        double gpuTemp = tempParts.value(1).toDouble() / 1000.0; // Convert to Celsius
//        qDebug() << "cpuTemp:" << cpuTemp << "gpuTemp:" << gpuTemp;

        // Uptime
        QProcess uptimeProcess;
        uptimeProcess.start("bash", QStringList() << "-c" << "uptime -p");
        uptimeProcess.waitForFinished();
        QString uptimeOutput = uptimeProcess.readAllStandardOutput().trimmed();

        // Disk 1: Root SSD (nvme0n1p1)
        if (statvfs("/", &stat) == 0) {
            total = stat.f_blocks * stat.f_frsize;
            free = stat.f_bfree * stat.f_frsize;
            used = total - free;
            rootTotalGB = total / (1024.0 * 1024.0 * 1024.0);
            rootUsedGB = used / (1024.0 * 1024.0 * 1024.0);
        }
        QString rootSSD = QString("Root: %1 / %2 GB")
                            .arg(QString::number(rootUsedGB, 'f', 0))
                            .arg(QString::number(rootTotalGB, 'f', 0));

        // Disk 2: External SSD (nvme1n1p1 mounted at /media/SSD1)
        if (statvfs("/media/SSD1", &stat) == 0) {
            total = stat.f_blocks * stat.f_frsize;
            free = stat.f_bfree * stat.f_frsize;
            used = total - free;
            extTotalGB = total / (1024.0 * 1024.0 * 1024.0);
            extUsedGB = used / (1024.0 * 1024.0 * 1024.0);
        }
        QString extSSD1 = QString("SSD1: %1 / %2 GB")
                            .arg(QString::number(extUsedGB, 'f', 0))
                            .arg(QString::number(extTotalGB, 'f', 0));

        // Disk 2: External SSD (nvme2n1p1 mounted at /media/SSD2)
        if (statvfs("/media/SSD2", &stat) == 0) {
            total = stat.f_blocks * stat.f_frsize;
            free = stat.f_bfree * stat.f_frsize;
            used = total - free;
            extTotalGB = total / (1024.0 * 1024.0 * 1024.0);
            extUsedGB = used / (1024.0 * 1024.0 * 1024.0);
        }
        QString extSSD2 = QString("SSD2: %1 / %2 GB")
                            .arg(QString::number(extUsedGB, 'f', 0))
                            .arg(QString::number(extTotalGB, 'f', 0));



        // Prepare JSON object for sending
        QJsonObject mainObject;
        mainObject.insert("menuID", "monitorData");
        mainObject.insert("formattedDateTime", formattedDateTime);
        mainObject.insert("ram", QString("%1 / %2 MB").arg(ramUsed, ramTotal));
        mainObject.insert("swap", QString("%1 / %2 MB").arg(swapUsed, swapTotal));
        mainObject.insert("cpuUsage", QJsonArray::fromStringList(cpuUsageList));
        mainObject.insert("uptime", uptimeOutput);
        mainObject.insert("cpuTemp", cpuTemp);
        mainObject.insert("gpuTemp", gpuTemp);
        mainObject.insert("ssdRoot", rootSSD);
        mainObject.insert("ssdExternal1", extSSD1);
        mainObject.insert("ssdExternal2", extSSD2);

        QJsonDocument jsonDoc(mainObject);
        QString jsonString = jsonDoc.toJson(QJsonDocument::Compact);
//        qDebug() << "debug_Monitor:" << jsonString;

        // Send data via WebSocket (optional)
        if (wClient) {
            wClient->sendTextMessage(jsonString);
            dashboardTimer->start(100);
        }

    }else if(obj["menuID"].toString() == "getRecordFiles"){
        mydatabase->fetchAllRecordFiles(msgs, wClient);

    }else if(obj["menuID"].toString() == "getRegisterDevicePage"){

        mydatabase->getRegisterDevicePage(msgs, wClient);
        setdefaultPath(msgs, wClient);

        QFile file(FILESETTING);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "Failed to open config file";
            return;
        }

        QTextStream in(&file);
        QString currentRecordDirectory;

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith("record_directory=")) {
                currentRecordDirectory = line.section('=', 1);
                break;
            }
        }

        file.close();

        if (!currentRecordDirectory.isEmpty()) {
            qDebug() << "Record directory:" << currentRecordDirectory;
        } else {
            qDebug() << "record_directory not found in config";
        }
        QJsonObject mainObject;
        mainObject.insert("menuID", "currentDirectoryPath");
        mainObject.insert("currentRecordDirectory", currentRecordDirectory);
        QJsonDocument jsonDoc(mainObject);
        QString jsonString = jsonDoc.toJson(QJsonDocument::Compact);
        qDebug() << "Current Record directory:" << jsonString;

        if (wClient) {
            wClient->sendTextMessage(jsonString);
        }

    }
}

void mainwindows::setdefaultPath(QString megs, QWebSocket* wClient) {
    QString ssd1Path = "/media/SSD1";
    QString ssd2Path = "/media/SSD2";
    QString fallbackPath = "/media/INTERNAL";

    QString ssd1Dev = "/dev/nvme1n1p1";
    QString ssd2Dev = "/dev/nvme2n1p1";

    QDir().mkpath(ssd1Path);
    QDir().mkpath(ssd2Path);
    QDir().mkpath(fallbackPath);

    bool ssd1Ok = mountIfNotMounted(ssd1Dev, ssd1Path);
    bool ssd2Ok = mountIfNotMounted(ssd2Dev, ssd2Path);

    double usage1 = ssd1Ok ? getDiskUsage(ssd1Path) : -1.0;
    double usage2 = ssd2Ok ? getDiskUsage(ssd2Path) : -1.0;

    qDebug() << "SSD1 usage:" << usage1 << "%";
    qDebug() << "SSD2 usage:" << usage2 << "%";

    QString selectedPath;
    QString formattedPath;

    if (usage1 >= 80.0 && usage2 >= 0.0 && usage2 < 80.0) {
        qDebug() << "Condition: SSD1 full, using SSD2";
        selectedPath = ssd2Path;
        formattedPath = ssd1Path;
    } else if (usage2 >= 80.0 && usage1 >= 0.0 && usage1 < 80.0) {
        qDebug() << "Condition: SSD2 full, using SSD1";
        selectedPath = ssd1Path;
        formattedPath = ssd2Path;
    } else if (usage1 >= 0.0 && usage1 < 80.0) {
        qDebug() << "Condition: SSD1 selected (not full)";
        selectedPath = ssd1Path;
    } else if (usage2 >= 0.0 && usage2 < 80.0) {
        qDebug() << "Condition: SSD2 selected (not full)";
        selectedPath = ssd2Path;
    } else {
        qDebug() << "Condition: fallback to internal";
        selectedPath = fallbackPath;
    }

    // Format and remount if needed
    if (!formattedPath.isEmpty()) {
        QString device = getDeviceForMount(formattedPath);
        if (!device.isEmpty()) {
            QProcess umount;
            umount.start("umount", {formattedPath});
            umount.waitForFinished();
            if (umount.exitCode() == 0) {
                qDebug() << "Unmounted:" << formattedPath;
                QProcess mkfs;
                mkfs.start("mkfs.ext4", {"-F", device});
                mkfs.waitForFinished();
                if (mkfs.exitCode() == 0) {
                    qDebug() << "Formatted:" << device;
                    QProcess remount;
                    remount.start("mount", {device, formattedPath});
                    remount.waitForFinished();
                    if (remount.exitCode() == 0)
                        qDebug() << "Remounted:" << device << "to" << formattedPath;
                    else
                        qWarning() << "Failed to remount:" << device;
                } else {
                    qWarning() << "Failed to format:" << device;
                }
            } else {
                qWarning() << "Failed to unmount:" << formattedPath;
            }
        } else {
            qWarning() << "Could not resolve device for:" << formattedPath;
        }
    }

    QJsonObject obj;
    obj["currentBackupPath"] = selectedPath;
    obj["menuID"] = "defaultPath";

    QJsonDocument doc(obj);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    if (wClient && wClient->isValid()) {
        wClient->sendTextMessage(jsonString);
        qDebug() << "Sent default path JSON:" << jsonString;
    } else {
        qWarning() << "WebSocket not connected!";
    }
}

void mainwindows::manageData(QString megs, QWebSocket* wClient){
    QByteArray br = megs.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();
    QJsonObject command = doc.object();
    QString getCommand =  QJsonValue(obj["objectName"]).toString();
    qDebug() << "megs" << megs << wClient;
    if(obj["menuID"].toString() == "getNetworkPage"){
        bond0.dhcpmethod = "on";
        bond0.ipaddress = networking->netWorkCardAddr;
        bond0.gateway = networking->netWorkCardGW;
        bond0.subnet = networking->netWorkCardMask;
        bond0.macAddress = networking->netWorkCardMac;
        bond0.serviceName = networking->netWorkCardDNS;
        bond0.pridns = "8.8.8.8";
        bond0.secdns = "8.8.4.4";
        QJsonObject mainObject;
        mainObject.insert("menuID", "network");
        mainObject.insert("dhcpmethod", bond0.dhcpmethod);
        mainObject.insert("ipaddress", bond0.ipaddress);
        mainObject.insert("subnet", bond0.subnet);
        mainObject.insert("gateway", bond0.gateway);
        mainObject.insert("pridns", bond0.pridns);
        mainObject.insert("secdns", bond0.secdns);
        mainObject.insert("phyNetworkName", "bond0");
        QJsonDocument jsonDoc(mainObject);
        QString networkInfo = jsonDoc.toJson(QJsonDocument::Compact);
        qDebug() << "networkInfo:" << networkInfo;
        emit mainwindowsMesg(networkInfo,wClient);
    }else if(obj["menuID"].toString() == "updateLocalNetwork"){
        qDebug() << "updateLocalNetwork:" << megs;
        bond0.dhcpmethod = obj["dhcpmethod"].toString();
        bond0.ipaddress = obj["ipaddress"].toString();
        bond0.gateway = obj["gateway"].toString();
        bond0.subnet = obj["subnet"].toString();
        bond0.pridns = obj["pridns"].toString();
        bond0.secdns = obj["secdns"].toString();
        bond0.phyNetworkName = obj["phyNetworkName"].toString();
        qDebug() << "dhcpmethod:" << bond0.dhcpmethod
                << "ipaddress:" << bond0.ipaddress
                << "gateway:" << bond0.gateway
                << "subnet:" << bond0.subnet
                << "pridns:" << bond0.pridns
                << "secdns:" << bond0.secdns
                << "phyNetworkName:" << bond0.phyNetworkName ;
        emit updateNetwork(bond0.ipaddress,bond0.subnet,bond0.gateway,bond0.subnet,bond0.pridns,bond0.phyNetworkName);
        networking->getAddress("bond0");
        configDestinationIP(bond0.ipaddress,wClient);
    }else if (obj["menuID"].toString() == "RegisterDevice") {
        qDebug() << "RegisterDevice:" << megs;
        mydatabase->CheckAndHandleDevice(megs,wClient);
        editfileConfig(megs);
        if (obj.contains("pathDirectory")) {
            QString newPathDirectory = obj["pathDirectory"].toString();
            if (!newPathDirectory.isEmpty()) {
                DirectoryManagement(newPathDirectory);
            } else {
                qWarning() << "pathDirectory is empty!";
            }
        } else {
            qWarning() << "No pathDirectory field found!";
        }
    }else if (obj["menuID"].toString() == "removeFile") {
        qDebug() << "removeFile:" << megs;
        fileName = command["fileName"].toString();
        filePath = command["filePath"].toString();
        qDebug() << "fileName:" << fileName << "filePath:" << filePath;
        mydatabase->RemoveFile(megs,wClient);
    }else if (obj["menuID"].toString() == "updateDevice") {
        mydatabase->CheckAndHandleDevice(megs,wClient);
    }else if (obj["menuID"].toString() == "updatePathDirectory") {
        mydatabase->UpdateDeviceInDatabase(megs,wClient);

        updateCurrentPathDirectory(megs,wClient);

    }else if (obj["menuID"].toString() == "deleteDevice") {
        mydatabase->removeRegisterDevice(megs,wClient);
    }else if (obj["menuID"].toString() == "selectPath") {
        handleSelectPath(obj, wClient);
    }

}

void mainwindows::updateCurrentPathDirectory(QString mesg, QWebSocket* wClient) {
    QByteArray br = mesg.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();
    QJsonObject command = doc.object();
    QString getCommand =  QJsonValue(obj["objectName"]).toString();
    QString newPath = obj["pathDirectory"].toString().trimmed();
    int newInterval = obj["timeInterval"].toInt();
    QString currentRecordDirectory;
    int currentRecordInterval = -1;
    QStringList lines;

    QFile file(FILESETTING);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open config file";
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QString trimmedLine = line.trimmed();
        if (trimmedLine.startsWith("record_directory=")) {
            currentRecordDirectory = trimmedLine.section('=', 1).trimmed();
        }
        if (trimmedLine.startsWith("record_interval=")) {
            currentRecordInterval = trimmedLine.section('=', 1).toInt();
        }
        lines.append(line);
    }
    file.close();

    // Update if different
    bool updated = false;

    if (newPath != currentRecordDirectory && !newPath.isEmpty()) {
        qDebug() << "Updating record_directory from" << currentRecordDirectory << "to" << newPath;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].trimmed().startsWith("record_directory=")) {
                lines[i] = QString("record_directory=%1").arg(newPath);
                updated = true;
                break;
            }
        }
    }

    if (newInterval != currentRecordInterval && newInterval > 0) {
        qDebug() << "Updating record_interval from" << currentRecordInterval << "to" << newInterval;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].trimmed().startsWith("record_interval=")) {
                lines[i] = QString("record_interval=%1").arg(newInterval);
                updated = true;
                break;
            }
        }
    }

    if (updated) {
        QFile outFile(FILESETTING);
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&outFile);
            for (const QString &line : lines) {
                out << line << "\n";
            }
            outFile.close();
        } else {
            qDebug() << "Failed to write updated config";
        }
    } else {
        qDebug() << "No changes to update.";
    }

    // Respond
    QJsonObject mainObject;
    mainObject.insert("menuID", "currentDirectoryPath");
    mainObject.insert("currentRecordDirectory", newPath.isEmpty() ? currentRecordDirectory : newPath);
    mainObject.insert("recordInterval", newInterval > 0 ? newInterval : currentRecordInterval);
    QJsonDocument jsonDoc(mainObject);
    QString jsonString = jsonDoc.toJson(QJsonDocument::Compact);

    if (wClient) {
        wClient->sendTextMessage(jsonString);
    }
}



void mainwindows::configDestinationIP(QString mesg, QWebSocket* wClient) {
    QStringList filePaths = {
        FILESETTINGIP,
        FILESETTING
    };

    for (const QString &filePath : filePaths) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Cannot open file for reading:" << filePath;
            continue;
        }

        QStringList lines;
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.trimmed().startsWith("bind_ip=")) {
                line = "bind_ip=" + mesg;
            }
            lines << line;
        }
        file.close();

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qWarning() << "Cannot open file for writing:" << filePath;
            continue;
        }

        QTextStream out(&file);
        for (const QString &line : lines) {
            out << line << "\n";
        }
        file.close();

        qDebug() << "Updated bind_ip in:" << filePath;
    }

//    QJsonObject result;
//    result["objectName"] = "configStatus";
//    result["status"] = "ok";
//    result["new_ip"] = mesg;
//    emit sendRecordFiles(QJsonDocument(result).toJson(QJsonDocument::Compact), wClient);
}


//void mainwindows::handleSelectPath(const QJsonObject &obj, QWebSocket* wClient) {
//    QString sourcePath = obj["path"].toString().trimmed();
//    system("sudo ln -s /media/SSD1/20250507/121100/20250508/ /var/www/html/audiofiles");
//}

void mainwindows::handleSelectPath(const QJsonObject &obj, QWebSocket* wClient) {
    qDebug() << "handleSelectPath() called";
    system("sudo rm -r /var/www/html/audiofiles");
    QString sourcePath = obj["path"].toString().trimmed();
    qDebug() << "Received sourcePath from JSON:" << sourcePath;

    if (!sourcePath.endsWith('/')) {
        sourcePath += '/';
        qDebug() << "Appended '/' to sourcePath:" << sourcePath;
    }

    if (!QDir(sourcePath).exists()) {
        qWarning() << "Source path does not exist:" << sourcePath;
        return;
    }

    QString linkPath = "/var/www/html/audiofiles";
    qDebug() << "Link path to create:" << linkPath;

    if (QFile::exists(linkPath)) {
        bool removed = QFile::remove(linkPath);
        qDebug() << (removed ? "Removed old symlink:" : "Failed to remove old symlink:") << linkPath;
    }

    QString cmd = QString("sudo ln -s \"%1\" \"%2\"").arg(sourcePath, linkPath);
    qDebug() << "Running system command:" << cmd;

    int ret = system(cmd.toUtf8().constData());

    if (ret == 0) {
        qDebug() << "Symlink created:" << linkPath << "→" << sourcePath;
    } else {
        qWarning() << "system() failed to create symlink, return code:" << ret;
    }
}




void mainwindows::getDateTime()
{
    currentDateTime = QDateTime::currentDateTime();

    const QDate d = currentDateTime.date();
    date = QString::number(d.year()) + '/'
         + QString::number(d.month()).rightJustified(2, '0') + '/'
         + QString::number(d.day()).rightJustified(2, '0');

    const QTime t = currentDateTime.time();
    time = QString::number(t.hour()).rightJustified(2, '0') + ':'
         + QString::number(t.minute()).rightJustified(2, '0') + ':'
         + QString::number(t.second()).rightJustified(2, '0');

    displayInfo->setTextLine1_1(date);
    displayInfo->setTextLine1_2(time);
    displayInfo->writeOLEDRun();
}

void mainwindows::DirectoryManagement(QString pathDirectory)
{
    qDebug() << "DirectoryManagement:" << pathDirectory;

    if (pathDirectory.isEmpty()) {
        qWarning() << "Empty pathDirectory!";
        return;
    }
    // แยก folder name ออกมาจาก pathDirectory เช่น 20250429
    QString folderName = QFileInfo(pathDirectory).fileName();
    if (folderName.isEmpty()) {
        qWarning() << "Failed to extract folder name from pathDirectory!";
        return;
    }
    // เช็คว่า parent path "/home/orinnx/audioRec/" มีอยู่แล้วไหม (ตามที่บอกว่ามีอยู่แล้ว)
    QString parentPath = QFileInfo(pathDirectory).absolutePath();
    QDir parentDir(parentPath);
    if (!parentDir.exists()) {
        qWarning() << "Parent directory does not exist:" << parentPath;
        return;
    }

    QDir targetDir(pathDirectory);
    if (!targetDir.exists()) {
        qDebug() << "Creating directory:" << pathDirectory;
        if (targetDir.mkpath(".")) {
            qDebug() << "Directory created successfully.";
        } else {
            qWarning() << "Failed to create directory:" << pathDirectory;
            return;
        }
    } else {
        qDebug() << "Directory already exists:" << pathDirectory;
    }

    // สร้าง symbolic link
    QString linkPath = "/var/www/html/audiofiles/" + folderName;

    if (QFile::exists(linkPath)) {
        qDebug() << "Link already exists. Deleting existing link:" << linkPath;
        QFile::remove(linkPath);
    }

    qDebug() << "Creating symlink:" << linkPath << "->" << pathDirectory;
    if (QFile::link(pathDirectory, linkPath)) {
        qDebug() << "Symlink created successfully.";
    } else {
        qWarning() << "Failed to create symlink!";
    }
}


void mainwindows::editfileConfig(QString mesg)
{
    qDebug() << "editfileConfig received:" << mesg;

    // Step 1: Parse JSON input
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON Parse Error:" << parseError.errorString();
        return;
    }

    QJsonObject obj = doc.object();
    int timeInterval = obj["timeInterval"].toInt(-1);
    QString pathDirectory = obj["pathDirectory"].toString();

    if (timeInterval <= 0 || pathDirectory.isEmpty()) {
        qWarning() << "Invalid timeInterval or pathDirectory in JSON!";
        return;
    }

    const QString cfgfile = FILESETTING;

    QFile file(cfgfile);
    if (!file.exists()) {
        qWarning() << "Configuration file does not exist:" << cfgfile;
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open config file for reading.";
        return;
    }

    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) {
        QString line = in.readLine();

        if (line.trimmed().startsWith("record_interval=")) {
            line = QString("record_interval=%1").arg(timeInterval);
        } else if (line.trimmed().startsWith("record_directory=")) {
            line = QString("record_directory=%1").arg(pathDirectory);
        }

        lines << line;
    }
    file.close();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Failed to open config file for writing.";
        return;
    }

    QTextStream out(&file);
    for (const QString &line : lines) {
        out << line << '\n';
    }

    file.close();
    qDebug() << "Configuration file updated successfully.";
}


void mainwindows::mesgManagement(QString mesg){
    qDebug() << "mesgManagement:" << mesg;
    QJsonDocument d = QJsonDocument::fromJson(mesg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand =  QJsonValue(command["objectName"]).toString().trimmed();
    if(getCommand == "getAddress"){
        qDebug() << "getAddress:" << mesg;
        QString IPAddress = command["IPAddress"].toString();
        QString Gateway = command["Gateway"].toString();
        QString MacAddress = command["MacAddress"].toString();
        QString Netmask = command["Netmask"].toString();
        QString Server = command["Server"].toString();
        QString line2String1 = "IP:"+IPAddress;
        QString line2String2 = "GW:"+Gateway;
        QString line3String1 = "Mac:"+MacAddress;
        QString line3String2 = "Sub:"+Netmask;
        qDebug() << "line2String1:" << line2String1 << line2String2;
        displayInfo->setTextLine2_1(line2String1);
        displayInfo->setTextLine2_2(line2String2);
        displayInfo->writeOLEDRun();
    }
}


void mainwindows::displayThread()
{
    qDebug() << "*********************************************************** displayThread start ********************************************************" ;
    phyNetwork _eth0;
    phyNetwork _eth1;
    int blinkCount = 0;
    QString _Reference;
    QString line2String1 = "Used/All";
    QString line2String2 = "Used/All";
}

void mainwindows::updateNewVolumeRecord(QString mesg) {
    qDebug() << "Received update:" << mesg;
    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON data!";
        return;
    }

    QJsonObject obj = doc.object();
    int volume = obj["currentVolume"].toInt();
    int level = obj["level"].toInt();

    QString volString;

    switch (level) {
        case 0: volString = "Vol: ||||||||||||||"; break;
        case 1: volString = "Vol: ||||||||||||"; break;
        case 2: volString = "Vol: ||||||||||"; break;
        case 3: volString = "Vol: ||||||||"; break;
        case 4: volString = "Vol: ||||||"; break;
        case 5: volString = "Vol: ||||"; break;
        case 6: volString = "Vol: ||"; break;
        case 7: volString = "Vol: "; break;
        default: volString = "Vol: ???"; break;
    }

    displayInfo->setTextLine4_1(volString);
    displayInfo->writeOLEDRun();
}


void mainwindows::rotary(int encoderNum, int val, int InputEventID)
{
    qDebug() << "rotary:" << encoderNum << val << InputEventID;

    if (val == -1) {
        if (currentVolume > 0) currentVolume--;
    } else if (val == 1) {
        if (currentVolume < 0x3F) currentVolume++;
    }

    if (!max9850->setVolume(currentVolume)) {
        qWarning() << "Failed to set volume:" << currentVolume;
        return;
    }

    qDebug() << "Volume set to:" << currentVolume;

    int level = 0;
    if (currentVolume >= 0x00 && currentVolume <= 0x05) {
        level = 0;
        qDebug() << "Volume set level to:" << currentVolume << level;
        QString Volume = "Vol: ||||||||||||||";
        QString maxVol = Volume+"max";
        displayInfo->setTextLine4_1(maxVol);
        displayInfo->writeOLEDRun();
        mysql->recordVolume(currentVolume,level);
    }
    else if (currentVolume > 0x05 &&currentVolume <= 0x0D){
        level = 1;
        qDebug() << "Volume set level to:" << currentVolume << level;
        displayInfo->setTextLine4_1("Vol: ||||||||||||");
        displayInfo->writeOLEDRun();
        mysql->recordVolume(currentVolume,level);
    }
    else if (currentVolume > 0x0D &&currentVolume <= 0x15){
        level = 2;
        qDebug() << "Volume set level to:" << currentVolume << level;
        displayInfo->setTextLine4_1("Vol: ||||||||||");
        displayInfo->writeOLEDRun();
        mysql->recordVolume(currentVolume,level);
    }
    else if (currentVolume > 0x15 &&currentVolume <= 0x1E){
        level = 3;
        qDebug() << "Volume set level to:" << currentVolume << level;
        displayInfo->setTextLine4_1("Vol: ||||||||");
        displayInfo->writeOLEDRun();
        mysql->recordVolume(currentVolume,level);
    }
    else if (currentVolume > 0x1E &&currentVolume <= 0x26){
        level = 4;
        qDebug() << "Volume set level to:" << currentVolume << level;
        displayInfo->setTextLine4_1("Vol: ||||||");
        displayInfo->writeOLEDRun();
        mysql->recordVolume(currentVolume,level);
    }
    else if (currentVolume > 0x26 &&currentVolume <= 0x2E){
        level = 5;
        qDebug() << "Volume set level to:" << currentVolume << level;
        displayInfo->setTextLine4_1("Vol: ||||");
        displayInfo->writeOLEDRun();
        mysql->recordVolume(currentVolume,level);
    }
    else if (currentVolume > 0x2E &&currentVolume <= 0x3E){
        level = 6;
        qDebug() << "Volume set level to:" << currentVolume << level;
        displayInfo->setTextLine4_1("Vol: ||");
        displayInfo->writeOLEDRun();
        mysql->recordVolume(currentVolume,level);
    }
    else {
        level = 7;
        qDebug() << "Volume set level to:" << currentVolume << level;
        displayInfo->setTextLine4_1("Vol: ");
        displayInfo->writeOLEDRun();
        mysql->recordVolume(currentVolume,level);
    }

}


void mainwindows::displayVolumes()
{
    QString volText = "Vol:";
    displayInfo->setTextLine4_1(volText);
    displayInfo->writeOLEDRun();
}


void mainwindows::readSSDStorage()
{
    const QStorageInfo storage = QStorageInfo::root();
    const qint64 totalBytes = storage.bytesTotal();
    const qint64 freeBytes = storage.bytesAvailable();
    const qint64 usedBytes = totalBytes - freeBytes;

    const double usedGB = static_cast<double>(usedBytes) / (1024.0 * 1024.0 * 1024.0);
    const double totalGB = static_cast<double>(totalBytes) / (1024.0 * 1024.0 * 1024.0);

    QString line3String1;
    line3String1.reserve(32); // ประหยัด allocation ล่วงหน้า
    line3String1 = QStringLiteral("SSD:%1/%2 GB")
        .arg(QString::number(usedGB, 'f', 0),
             QString::number(totalGB, 'f', 0));

    displayInfo->setTextLine3_1(line3String1);
    displayInfo->writeOLEDRun();
}

void mainwindows::handleRecordFiles(const QString &jsonData)
{
    qDebug() << "handleRecordFiles:" << jsonData;

    const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format";
        return;
    }

    const QJsonObject &obj = doc.object();
    const QJsonArray records = obj.value(QStringLiteral("records")).toArray();

    for (const QJsonValue &val : records) {
        const QJsonObject rec = val.toObject();
        qDebug().nospace()
            << "ID:" << rec.value(QStringLiteral("id")).toInt()
            << " Device:" << rec.value(QStringLiteral("device")).toString()
            << " Filename:" << rec.value(QStringLiteral("filename")).toString()
            << " Created_at:" << rec.value(QStringLiteral("created_at")).toString()
            << " Count:" << rec.value(QStringLiteral("continuous_count")).toInt();
    }
}


void* mainwindows::ThreadFunc(void* pTr)
{
    mainwindows* pThis = static_cast<mainwindows*>(pTr);
    while (true) {
        emit pThis->requestUpdateDateTime();
        QThread::sleep(1);
    }
    return nullptr;
}

void* mainwindows::ThreadFunc2(void* pTr )
{
    mainwindows* pThis = static_cast<mainwindows*>(pTr);
    while (true) {
        pThis->readSSDStorage();
    }
    return NULL;
}

void* mainwindows::ThreadFunc3(void* pTr )
{
    mainwindows* pThis = static_cast<mainwindows*>(pTr);
    while (true) {
        pThis->max31760->tempDetect();
    }
    return NULL;
}

//    system("speaker-test -D hw:3,1 -r 8000 -c 2 -t sine -f 1000");
//    system("echo PE.06 > /sys/class/gpio/export");
//    QThread::msleep(100);
//    system("echo out > /sys/class/gpio/PE.06/direction");
//    QThread::msleep(100);
//    system("echo 1 > /sys/class/gpio/PE.06/value");
//    system("speaker-test -D hw:3,3 -r 48000 -c 2 -f 1000 -t sine -S 1");
