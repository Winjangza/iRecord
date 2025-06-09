#include "mainwindows.h"
#include <QDebug>
// CREATE USER 'orinnx'@'localhost' IDENTIFIED BY 'OrinX!2024';
//CREATE USER 'recorder'@'localhost' IDENTIFIED BY 'OTL324$a';
mainwindows::mainwindows(QObject *parent)
    : QObject(parent)
{
    qDebug() << "hello world";
    SocketServer = new ChatServer(1234);
//    mysql = new Database("iRecord", "orinnx", "OrinX!2024", "localhost", this);
//    mydatabase = new Database("recorder", "root", "OTL324$", "localhost", this);
    mydatabase = new Database("recorder", "root", "OTL324$", "localhost", this);
    networking = new NetworkMng(this);
    displayInfo = new infoDisplay(this);
    storageTimer = new QTimer(this);
    max31760 = new MAX31760(this);
    QTimer *timer = new QTimer(this);
    dashboardTimer = new QTimer(this);
    downloader = new FileDownloader;
    UnixSocketListener* unixReceiver = new UnixSocketListener(this);
    liveStreaming = new QTimer(this);
    wClient = new QWebSocket();
    Timer = new QTimer();
    client = new SocketClient();
    QTimer *diskTimer = new QTimer(this);
    RotaryEncoderButtonL = new GetInputEvent("/dev/input/by-path/platform-gpio-keys-event",1,28,this);
    RotaryEncoder01 = new GetInputEvent("/dev/input/by-path/platform-rotary@2-event",2,1,this);
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
//    client->createConnection("127.0.0.1", 1237);

//    connect(client, &SocketClient::Connected, this, []() {
//        qDebug() << "mainwindows: SocketClient connected!";
//    });
//    connect(client, &SocketClient::newCommandProcess, this, [=](QString msg) {
//        qDebug() << "mainwindows: Message received via SocketClient:" << msg;

//        QJsonParseError parseError;
//        QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &parseError);

//        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
//            QJsonObject obj = doc.object();
//            QString menuID = obj.value("menuID").toString();
//            QString messageText = obj.value("message").toString();

//            if (menuID == "rec_event") {
//                manageData(msg, wClient);
//            }
//        } else {
//            qDebug() << "Parse error:" << parseError.errorString();
//        }
//    });



    connect(unixReceiver, &UnixSocketListener::messageReceived, this, [=](const QString& msg) {
        qDebug() << "mainwindows: Received message from socket:" << msg;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &err);
//        if (err.error == QJsonParseError::NoError && doc.isObject()) {
//            QJsonObject obj = doc.object();
            recordDeviceLiveStream(msg, wClient);
//        }
//        QStringList msgSplit = msg.split(",", Qt::SkipEmptyParts);
//        if(msgSplit.size() >= 4){
//            QString ip     = msgSplit[0].trimmed();
//            QString freq   = msgSplit[1].trimmed();
//            QString uri    = msgSplit[2].trimmed();
//            QString action = msgSplit[3].trimmed();

//            qDebug() << "IP:" << ip << "Freq:" << freq << "URI:" << uri << "Action:" << action;
//            if(ip != ""){
//                autoLiveStream(ip);
//            }
//        }

    });

    system("sudo hwclock -s");
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
//-------------------------------------mydatabase------------------------------------------------//
    connect(mydatabase, SIGNAL(previousRecordVolume( QString)), this, SLOT(updateNewVolumeRecord( QString)));
    connect(mydatabase, SIGNAL(sendRecordFiles(QString, QWebSocket*)), SocketServer, SLOT(sendMessage(QString, QWebSocket*)));
//--------------------------------------WebServer------------------------------------------------//
    connect(SocketServer, SIGNAL(newCommandProcess(QString, QWebSocket*)), this, SLOT(manageData(QString, QWebSocket*)));
    connect(SocketServer, SIGNAL(newCommandProcess(QString, QWebSocket*)), this, SLOT(dashboardManage(QString, QWebSocket*)));
    connect(dashboardTimer, &QTimer::timeout, this, [=]() {dashboardManage(dashboardMsg, clientSocket);});
    connect(downloader,SIGNAL(downloadCompleted(const QString)), this, SLOT(downloadCompleted(const QString)));
    connect(SocketServer, SIGNAL(newCommandProcess(QString, QWebSocket*)), this, SLOT(startNTPServer(QString, QWebSocket*)));

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
    mydatabase->updateRecordVolume();
    mydatabase->CheckandVerifyDatabases();
    mydatabase->maybeRunCleanup();
    mydatabase->linkRecordChannelWithDeviceStation();
    mydatabase->linkRecordFilesWithDeviceStationOnce();

}

void mainwindows::verifySSDandBackupFile() {
    struct statvfs buf;

    QStringList devices = {"/dev/nvme1n1p1", "/dev/nvme2n1p1"}; //CheckandVerifyDatabases
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
        QString currentRecordDirectory;

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "Failed to open config file:" << FILESETTING;
            return;
        }

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith("record_directory=")) {
                currentRecordDirectory = line.section('=', 1);
                break;
            }
        }
        file.close();

        if (!currentRecordDirectory.isEmpty()) {
            qDebug() << "Record directory (FILESETTING):" << currentRecordDirectory;
        } else {
            qDebug() << "record_directory not found in FILESETTING";
        }

        QFile fileInit(FILESETTINGPATH);
        QString currentRecordDirectoryInit;

        if (!fileInit.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "Failed to open config file:" << FILESETTINGPATH;
            return;
        }

        QTextStream inInit(&fileInit);
        while (!inInit.atEnd()) {
            QString line = inInit.readLine().trimmed();
            if (line.startsWith("record_directory=")) {
                currentRecordDirectoryInit = line.section('=', 1);
                break;
            }
        }
        fileInit.close();

        if (!currentRecordDirectoryInit.isEmpty()) {
            qDebug() << "Record directory (FILESETTINGPATH):" << currentRecordDirectoryInit;
        } else {
            qDebug() << "record_directory not found in FILESETTINGPATH";
        }

        QJsonObject mainObject;
        mainObject.insert("menuID", "currentDirectoryPath");
        mainObject.insert("currentRecordDirectory", currentRecordDirectoryInit);
        QJsonDocument jsonDoc(mainObject);
        QString jsonString = jsonDoc.toJson(QJsonDocument::Compact);
        qDebug() << "Current Record directory:" << jsonString;

        if (wClient) {
            wClient->sendTextMessage(jsonString);
//            runBuildAndRestartServices();
        }
    }


}

void mainwindows::setdefaultPath(QString megs, QWebSocket* wClient) {
    qDebug() << "setdefaultPath:" << megs;

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



// mainwindows.cpp
void mainwindows::recordDeviceLiveStream(QString megs, QWebSocket* wClient) {
    mydatabase->lookupDeviceStationByIp(megs, wClient);
}

//void mainwindows::recordDeviceLiveStream(QString megs, QWebSocket* wClient) {
//    QtConcurrent::run([=]() {
//           qDebug() << "recordDeviceLiveStream:" << megs;

//           QStringList parts = megs.split(",");
//           if (parts.size() != 4) {
//               qWarning() << "Invalid message format";
//               return;
//           }

//           QString ip = parts[0].trimmed();
//           QString freq = parts[1].trimmed();
//           QString url = parts[2].trimmed();
//           QString action = parts[3].trimmed();

//           QJsonObject obj;
//           obj["ip"] = ip;
//           obj["freq"] = freq;
//           obj["url"] = url;
//           obj["action"] = action;

//           mydatabase->mysqlRecordDevice(obj);

//           if (!ip.isEmpty() && !freq.isEmpty() && !url.isEmpty() && !action.isEmpty()) {
//               QJsonObject response;
//               response["menuID"] = "realTimerecord";
//               response["data"] = obj;
//               QJsonDocument doc(response);
//               QString resultJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

//               QMetaObject::invokeMethod(qApp, [=]() {
//                   if (wClient && wClient->isValid()) {
//                       wClient->sendTextMessage(resultJson);
//                       qDebug() << "record_Device_LiveStream:" << resultJson;
//                   }
//               }, Qt::QueuedConnection);
//           }
//       });
////    qDebug() << "recordDeviceLiveStream:" << megs;

////    QStringList parts = megs.split(",");
////    if (parts.size() != 4) {
////        qWarning() << "Invalid message format";
////        return;
////    }

////    QString ip = parts[0].trimmed();
////    QString freq = parts[1].trimmed();
////    QString url = parts[2].trimmed();
////    QString action = parts[3].trimmed();

////    QJsonObject obj;
////    obj["ip"] = ip;
////    obj["freq"] = freq;
////    obj["url"] = url;
////    obj["action"] = action;

////    mydatabase->mysqlRecordDevice(obj);

////    QJsonObject response;
////    response["menuID"] = "realTimerecord";
////    response["data"] = obj;
////    QJsonDocument doc(response);
////    QString resultJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
////    if(obj["ip"].toString() != "" && obj["freq"].toString() != "" && obj["url"].toString() != "" && obj["action"].toString() != ""){
////        if (wClient) {
////            wClient->sendTextMessage(resultJson);
////            qDebug() << "record_Device_LiveStream:" << resultJson;
////        }
////    }

//}


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

    }else if (obj["menuID"].toString() == "changePathDirectory") {
        updateCurrentPathDirectory(megs,wClient);
        mydatabase->updatePath(megs,wClient);

    }else if (obj["menuID"].toString() == "playFileWav") {
        qDebug() << "Received request to play file:" << megs;

        QString fileName = obj["fileName"].toString().trimmed(); // เช่น 121950_20250605_2_105508_95382635.wav
        QString basePath = obj["filePath"].toString().trimmed(); // เช่น /home/orinnx/audioRec

        QStringList parts = fileName.split('_');
        if (parts.size() < 4) {
            qWarning() << "❌ Invalid filename format:" << fileName;
            return;
        }

        QString freq = parts[0];
        QString date = parts[1];
        QString ch = parts[2];

        QString fullPath = QString("%1/%2/%3/%4/%5").arg(basePath, freq, date, ch, fileName);
        qDebug() << "✅ Playing file from full path:" << fullPath;

        playWavFile(fullPath);
    }else if (obj["menuID"].toString() == "deleteDevice") {
        mydatabase->removeRegisterDevice(megs,wClient);
    }else if (obj["menuID"].toString() == "selectPath") {
        handleSelectPath(obj, wClient);
    }else if (obj["menuID"].toString() == "rec_event") { /// receive data from C record
        qDebug() << "rec_event:" << megs;
        mydatabase->recordChannel(megs,wClient);
    }else if (obj["menuID"].toString() == "getRecordChannel") {
        qDebug() << "getReccordChannel:" << megs;
        mydatabase->selectRecordChannel(megs,wClient);
//        startUnixSocketReader(wClient);
    }else if (obj["menuID"].toString() == "getRecordChannel2") {
        qDebug() << "getReccordChannel:" << megs;
        wClient->sendTextMessage("autoPlay");
        system("sudo systemctl restart AudioStreamServer.service");

    }else if (obj["menuID"].toString() == "startLiveStream") {
        qDebug() << "startLiveStream:" << megs;
        liveSteamRecordRaido(megs,wClient);
    }else if (obj["menuID"].toString() == "stopLiveStream") {
        qDebug() << "stopLiveStream:" << megs;
        stopLiveStream(megs,wClient);
    }else if (obj["menuID"].toString() == "updateFirmware") {
        qDebug() << "updateFirmware:" << megs;
        updateSystem(megs);
    }else if (obj["menuID"].toString() == "formatExternal") {
        qDebug() << "formatExternal:" << megs;
        formatExternal(megs);
        mydatabase->formatDatabases(megs);
    }else if (obj["menuID"].toString() == "getSystemPage") {
        qDebug() << "getSystemPage:" << megs;

        QString iRecord = "unknown";
        QString swversion = "unknown";
        QString hwversion = "unknown";

        QFile file("/home/orinnx/README.txt");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();

                if (line.contains("iRecord")) {
                    QRegularExpression re("iRecord\\s*=\\s*\"([^\"]+)\"");
                    QRegularExpressionMatch match = re.match(line);
                    if (match.hasMatch()) iRecord = match.captured(1);
                }

                if (line.contains("swversion")) {
                    QRegularExpression re("swversion\\s*=\\s*\"([^\"]+)\"");
                    QRegularExpressionMatch match = re.match(line);
                    if (match.hasMatch()) swversion = match.captured(1);
                }

                if (line.contains("hwversion")) {
                    QRegularExpression re("hwversion\\s*=\\s*\"([^\"]+)\"");
                    QRegularExpressionMatch match = re.match(line);
                    if (match.hasMatch()) hwversion = match.captured(1);
                }
            }
            file.close();
        }

        // สร้าง JSON ส่งกลับ
        QJsonObject reply;
        reply["menuID"] = "system";
        reply["firmwareVersion"] = iRecord;
        reply["swversion"] = swversion;
        reply["hwversion"] = hwversion;

        QJsonDocument doc(reply);
        QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        qDebug() << "Sending systemPageInfo:" << jsonString;

        if (wClient) {
            wClient->sendTextMessage(jsonString);
        }
    }else if (obj["menuID"].toString() == "setLocation") {
        QString location = obj["location"].toString().trimmed();

        if (!location.isEmpty()) {
            currentLocationTimezone = location;
            qDebug() << "Location set to:" << currentLocationTimezone;
        }
    }else if (obj["menuID"].toString() == "updateNTPServer") {
        qDebug() << "updateNTPServer:" << megs;
        syncNTPServer(megs,wClient);

    }
}

void mainwindows::syncNTPServer(QString megs, QWebSocket* wClient) {
    qDebug() << "syncNTPServer:" << megs;

    // Parse input JSON
    QJsonDocument doc = QJsonDocument::fromJson(megs.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format.";
        return;
    }

    QJsonObject obj = doc.object();
    QString ntpIP = obj["ntpServer"].toString();  // ตัวอย่าง "192.168.10.66"
    if (ntpIP.isEmpty()) {
        qWarning() << "Empty NTP IP address.";
        return;
    }

    QString configPath = "/etc/systemd/timesyncd.conf";

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open:" << configPath;
        return;
    }

    QString content = file.readAll();
    file.close();

    QStringList lines = content.split('\n');
    bool ntpLineFound = false;

    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed().startsWith("NTP=")) {
            lines[i] = QString("NTP=%1 %2").arg(ntpIP, "pool.ntp.org 0.ubuntu.pool.ntp.org 1.ubuntu.pool.ntp.org ntp.ubuntu.com");
            ntpLineFound = true;
            break;
        }
    }

    if (!ntpLineFound) {
        // Append [Time] section and NTP config if not found
        lines << "[Time]";
        lines << QString("NTP=%1 pool.ntp.org 0.ubuntu.pool.ntp.org 1.ubuntu.pool.ntp.org ntp.ubuntu.com").arg(ntpIP);
        lines << "FallbackNTP=0.pool.ntp.org 1.pool.ntp.org 0.fr.pool.ntp.org";
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Failed to write:" << configPath;
        return;
    }

    QTextStream out(&file);
    out << lines.join('\n');
    file.close();

    // Restart the timesyncd service and manually sync time
    QProcess::execute("sudo systemctl restart systemd-timesyncd.service");
    QProcess::execute(QString("sudo ntpdate %1").arg(ntpIP));

    // Read the current time from system
    QProcess dateProc;
    dateProc.start("date");
    dateProc.waitForFinished();
    QString systemTime = dateProc.readAllStandardOutput().trimmed();

    qDebug() << "System time after sync:" << systemTime;

    QJsonObject response;
    response["menuID"] = "syncNTPServerResult";
    response["ip"] = ntpIP;
    response["systemTime"] = systemTime;

    QJsonDocument jsonDoc(response);  // ✅ ใส่หลังจาก response มีข้อมูล
    QString jsonString = jsonDoc.toJson(QJsonDocument::Compact);

    wClient->sendTextMessage(jsonString);
    startNTPServer(jsonString, wClient);

}

void mainwindows::startNTPServer(QString mesg, QWebSocket* wClient) {
    QByteArray br = mesg.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();

    qDebug() << "startNTPServer:" << mesg;
    if(obj["menuID"].toString() == "getSystemPage"  && obj["menuID"].toString() == "updateNTPServer"){
        if(ntpBroadcastTimer){
            ntpBroadcastTimer->stop();
            ntpBroadcastTimer->deleteLater();
            ntpBroadcastTimer = nullptr;
        }

        ntpBroadcastTimer = new QTimer(this);
        connect(ntpBroadcastTimer, &QTimer::timeout, this, [wClient]() {
            if (!wClient || !wClient->isValid())
                return;

            QDateTime dt = QDateTime::currentDateTime();
            QString timeStr = dt.time().toString("HH:mm:ss");
            QString dateStr = dt.date().toString("yyyy-MM-dd");

            qDebug() << "Broadcasting time:" << timeStr << ", date:" << dateStr;

            QJsonObject obj;
            obj["menuID"] = "broadcastLocalTime";
            obj["currentTime"] = timeStr;
            obj["currentDate"] = dateStr;

            QJsonDocument doc(obj);
            QString jsonMsg = doc.toJson(QJsonDocument::Compact);
            qDebug() << "Sending to WebSocket:" << jsonMsg;

            wClient->sendTextMessage(jsonMsg);
        });

        ntpBroadcastTimer->start(1000);
    }

}



void mainwindows::updateCurrentPathDirectory(QString mesg, QWebSocket* wClient) {
    QByteArray br = mesg.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();

    // Default values
    QString newPath = obj["pathDirectory"].toString().trimmed();
    int newInterval = obj["timeInterval"].toInt();

    // Alternate keys support
    if (obj.contains("ChangePathDirectory")) {
        newPath = obj["ChangePathDirectory"].toString().trimmed();
    }
    if (obj.contains("timeIntervalpath")) {
        newInterval = obj["timeIntervalpath"].toInt();
    }

    QStringList configFiles = { FILESETTING};
    QString latestPath = "";
    int latestInterval = -1;

    for (const QString &configFilePath : configFiles) {
        QFile file(configFilePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to open config file:" << configFilePath;
            continue;
        }

        QString currentRecordDirectory;
        int currentRecordInterval = -1;
        QStringList lines;
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

        // Update if necessary
        bool updated = false;
        if (newPath != currentRecordDirectory && !newPath.isEmpty()) {
            qDebug() << "Updating record_directory in" << configFilePath << "to" << newPath;
            for (int i = 0; i < lines.size(); ++i) {
                if (lines[i].trimmed().startsWith("record_directory=")) {
                    lines[i] = QString("record_directory=%1").arg(newPath);
                    updated = true;
                }
            }
        }

        if (newInterval != currentRecordInterval && newInterval > 0) {
            qDebug() << "Updating record_interval in" << configFilePath << "to" << newInterval;
            for (int i = 0; i < lines.size(); ++i) {
                if (lines[i].trimmed().startsWith("record_interval=")) {
                    lines[i] = QString("record_interval=%1").arg(newInterval);
                    updated = true;
                }
            }
        }

        if (updated) {
            QFile outFile(configFilePath);
            if (outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                QTextStream out(&outFile);
                for (const QString &line : lines)
                    out << line << "\n";
                outFile.close();
            } else {
                qWarning() << "Failed to write to config file:" << configFilePath;
            }
        }
        // === Add symlink update here ===
        QString symlinkPath = "/var/www/html/audiofiles";
        QFile::remove(symlinkPath);  // remove existing link or file

        if (!QFile::link(newPath, symlinkPath)) {
            qWarning() << "❌ Failed to create symlink from" << newPath << "to" << symlinkPath;
        } else {
            qDebug() << "✅ Created symlink:" << symlinkPath << " -> " << newPath;
        }

        // Keep track of last updated for WebSocket reply
        if (!newPath.isEmpty()) latestPath = newPath;
        else if (!currentRecordDirectory.isEmpty()) latestPath = currentRecordDirectory;

        if (newInterval > 0) latestInterval = newInterval;
        else if (currentRecordInterval >= 0) latestInterval = currentRecordInterval;
    }

    // Send WebSocket response
    QJsonObject mainObject;
    mainObject.insert("menuID", "currentDirectoryPath");
    mainObject.insert("currentRecordDirectory", latestPath);
    mainObject.insert("recordInterval", latestInterval);
    QJsonDocument jsonDoc(mainObject);
    QString jsonString = jsonDoc.toJson(QJsonDocument::Compact);

    if (wClient) {
        wClient->sendTextMessage(jsonString);
    }
}


//void mainwindows::updateCurrentPathDirectory(QString mesg, QWebSocket* wClient) {
//    QByteArray br = mesg.toUtf8();
//    QJsonDocument doc = QJsonDocument::fromJson(br);
//    QJsonObject obj = doc.object();
//    QJsonObject command = doc.object();
//    QString getCommand =  QJsonValue(obj["objectName"]).toString();
//    QString newPath = obj["pathDirectory"].toString().trimmed();
//    int newInterval = obj["timeInterval"].toInt();
//    QString currentRecordDirectory;
//    int currentRecordInterval = -1;
//    QStringList lines;

//    QFile file(FILESETTING);
//    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
//        qDebug() << "Failed to open config file";
//        return;
//    }

//    QTextStream in(&file);
//    while (!in.atEnd()) {
//        QString line = in.readLine();
//        QString trimmedLine = line.trimmed();
//        if (trimmedLine.startsWith("record_directory=")) {
//            currentRecordDirectory = trimmedLine.section('=', 1).trimmed();
//        }
//        if (trimmedLine.startsWith("record_interval=")) {
//            currentRecordInterval = trimmedLine.section('=', 1).toInt();
//        }
//        lines.append(line);
//    }
//    file.close();

//    // Update if different
//    bool updated = false;

//    if (newPath != currentRecordDirectory && !newPath.isEmpty()) {
//        qDebug() << "Updating record_directory from" << currentRecordDirectory << "to" << newPath;
//        for (int i = 0; i < lines.size(); ++i) {
//            if (lines[i].trimmed().startsWith("record_directory=")) {
//                lines[i] = QString("record_directory=%1").arg(newPath);
//                updated = true;
//                break;            }
//        }
//    }

//    if (newInterval != currentRecordInterval && newInterval > 0) {
//        qDebug() << "Updating record_interval from" << currentRecordInterval << "to" << newInterval;
//        for (int i = 0; i < lines.size(); ++i) {
//            if (lines[i].trimmed().startsWith("record_interval=")) {
//                lines[i] = QString("record_interval=%1").arg(newInterval);
//                updated = true;
//                break;
//            }
//        }
//    }

//    if (updated) {
//        QFile outFile(FILESETTING);
//        if (outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
//            QTextStream out(&outFile);
//            for (const QString &line : lines) {
//                out << line << "\n";
//            }
//            outFile.close();
//        } else {
//            qDebug() << "Failed to write updated config";
//        }
//    } else {
//        qDebug() << "No changes to update.";
//    }

//    // Respond
//    QJsonObject mainObject;
//    mainObject.insert("menuID", "currentDirectoryPath");
//    mainObject.insert("currentRecordDirectory", newPath.isEmpty() ? currentRecordDirectory : newPath);
//    mainObject.insert("recordInterval", newInterval > 0 ? newInterval : currentRecordInterval);
//    QJsonDocument jsonDoc(mainObject);
//    QString jsonString = jsonDoc.toJson(QJsonDocument::Compact);

//    if (wClient) {
//        wClient->sendTextMessage(jsonString);
//    }
//}


void mainwindows::configDestinationIP(QString mesg, QWebSocket* wClient) {
    QStringList filePaths = {
        FILESETTINGIP,
        FILESETTING,
        FILESETTINGPATH // ✅ เพิ่มไฟล์นี้ตามที่ระบุ
    };

    for (const QString &filePath : filePaths) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Cannot open file for reading:" << filePath;
            continue;
        }

        QStringList lines;
        QTextStream in(&file);
//        bool foundBindIp = false;

//        while (!in.atEnd()) {
//            QString line = in.readLine();
//            if (line.trimmed().startsWith("bind_ip=")) {
//                line = "bind_ip=" + mesg;
//                foundBindIp = true;
//            }
//            lines << line;
//        }
//        file.close();

//        if (!foundBindIp) {
//            lines << "bind_ip=" + mesg;
//        }

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

    // ✅ (ไม่จำเป็นแต่แนะนำ) ส่งผลลัพธ์กลับไปที่ client
    if (wClient && wClient->isValid()) {
        QJsonObject result;
        result["menuID"] = "configStatus";
        result["status"] = "ok";
        result["updated_ip"] = mesg;
        wClient->sendTextMessage(QJsonDocument(result).toJson(QJsonDocument::Compact));
//        runBuildAndRestartServices();
    }
}



//void mainwindows::configDestinationIP(QString mesg, QWebSocket* wClient) {
//    QStringList filePaths = {
//        FILESETTINGIP,
//        FILESETTING
//    };

//    for (const QString &filePath : filePaths) {
//        QFile file(filePath);
//        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
//            qWarning() << "Cannot open file for reading:" << filePath;
//            continue;
//        }

//        QStringList lines;
//        QTextStream in(&file);
//        while (!in.atEnd()) {
//            QString line = in.readLine();
//            if (line.trimmed().startsWith("bind_ip=")) {
//                line = "bind_ip=" + mesg;
//            }
//            lines << line;
//        }
//        file.close();

//        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
//            qWarning() << "Cannot open file for writing:" << filePath;
//            continue;
//        }

//        QTextStream out(&file);
//        for (const QString &line : lines) {
//            out << line << "\n";
//        }
//        file.close();

//        qDebug() << "Updated bind_ip in:" << filePath;
//    }

////    QJsonObject result;
////    result["objectName"] = "configStatus";
////    result["status"] = "ok";
////    result["new_ip"] = mesg;
////    emit sendRecordFiles(QJsonDocument(result).toJson(QJsonDocument::Compact), wClient);
//}


//void mainwindows::handleSelectPath(const QJsonObject &obj, QWebSocket* wClient) {
//    QString sourcePath = obj["path"].toString().trimmed();
//    system("sudo ln -s /media/SSD1/20250507/121100/20250508/ /var/www/html/audiofiles");
//}

void mainwindows::handleSelectPath(const QJsonObject &obj, QWebSocket* wClient) {
    qDebug() << "handleSelectPath() called";
    system("sudo rm -r /var/www/html/audiofiles");
    QString sourcePath = obj["path"].toString().trimmed();
//    QString sourcePath = "/home/orinnx/audioRec";
    qDebug() << "Received sourcePath from JSON:" << sourcePath;

//    if (!sourcePath.endsWith('/')) {
//        sourcePath += '/';
//        qDebug() << "Appended '/' to sourcePath:" << sourcePath;
//    }

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
    QProcess ntpCheck;
    ntpCheck.start("timedatectl show -p NTPSynchronized --value");
    ntpCheck.waitForFinished(1000);
    QString ntpStatus = ntpCheck.readAllStandardOutput().trimmed();
//    qDebug() << "NTP synchronized:" << ntpStatus;

    QDateTime systemTime = QDateTime::currentDateTimeUtc();

    QProcess hwclockProc;
    hwclockProc.start("hwclock -r");  // Output: "2025-05-28 11:19:49.123456+00:00"
    hwclockProc.waitForFinished(1000);
    QString rtcOutput = hwclockProc.readAllStandardOutput().trimmed();
//    qDebug() << "Raw RTC output:" << rtcOutput;

    QDateTime rtcTime;

    rtcTime = QDateTime::fromString(rtcOutput, Qt::ISODate);
    if (!rtcTime.isValid() && rtcOutput.length() >= 19) {
        // Fallback: strip fractional seconds if parsing fails
        rtcTime = QDateTime::fromString(rtcOutput.left(19), "yyyy-MM-dd HH:mm:ss");
        rtcTime.setTimeSpec(Qt::UTC);
    }

    if (rtcTime.isValid()) {
        qint64 drift = qAbs(systemTime.secsTo(rtcTime));
//        qDebug() << "Drift between system and RTC:" << drift << "seconds";

        if (ntpStatus == "yes") {
            system("hwclock -w");  // Write system time to RTC
//            qDebug() << "NTP active → hwclock updated from system time.";
        } else if (drift > 5) {
            system("hwclock -s");  // Set system time from RTC
//            qDebug() << "System clock synced from RTC (no NTP).";
        } else {
//            qDebug() << "Drift small, no sync needed.";
        }
    } else {
//        qWarning() << "Failed to parse RTC time!";
    }

    QDateTime currentDateTime = QDateTime::currentDateTime();

    const QDate d = currentDateTime.date();
    date = QString::number(d.year()) + '/'
         + QString::number(d.month()).rightJustified(2, '0') + '/'
         + QString::number(d.day()).rightJustified(2, '0');

    const QTime t = currentDateTime.time();
    time = QString::number(t.hour()).rightJustified(2, '0') + ':'
         + QString::number(t.minute()).rightJustified(2, '0') + ':'
         + QString::number(t.second()).rightJustified(2, '0');
//    qDebug() << "date&&time:" << date << time;

    displayInfo->setTextLine1_1(date);
    displayInfo->setTextLine1_2(time);
    displayInfo->writeOLEDRun();

}



//void mainwindows::getDateTime()
//{

//    QDateTime currentDateTime = QDateTime::currentDateTime();
////    qDebug() << "getDateTime:" << currentDateTime;

//    const QDate d = currentDateTime.date();
//    date = QString::number(d.year()) + '/'
//         + QString::number(d.month()).rightJustified(2, '0') + '/'
//         + QString::number(d.day()).rightJustified(2, '0');

//    const QTime t = currentDateTime.time();
//    time = QString::number(t.hour()).rightJustified(2, '0') + ':'
//         + QString::number(t.minute()).rightJustified(2, '0') + ':'
//         + QString::number(t.second()).rightJustified(2, '0');

//    displayInfo->setTextLine1_1(date);
//    displayInfo->setTextLine1_2(time);
//    displayInfo->writeOLEDRun();
//}

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

    QStringList configFiles = {
        FILESETTING,
        FILESETTINGPATH
    };

    for (const QString &cfgfile : configFiles) {
        QFile file(cfgfile);
        if (!file.exists()) {
            qWarning() << "Configuration file does not exist:" << cfgfile;
            continue;
        }

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to open for reading:" << cfgfile;
            continue;
        }

        QTextStream in(&file);
        QStringList lines;
        bool foundDir = false, foundInterval = false;

        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.trimmed().startsWith("record_interval=")) {
                line = QString("record_interval=%1").arg(timeInterval);
                foundInterval = true;
            } else if (line.trimmed().startsWith("record_directory=")) {
                line = QString("record_directory=%1").arg(pathDirectory);
                foundDir = true;
            }
            lines << line;
        }
        file.close();

        if (!foundInterval) {
            lines << QString("record_interval=%1").arg(timeInterval);
        }
        if (!foundDir) {
            lines << QString("record_directory=%1").arg(pathDirectory);
        }

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qWarning() << "Failed to open for writing:" << cfgfile;
            continue;
        }

        QTextStream out(&file);
        for (const QString &line : lines) {
            out << line << '\n';
        }
        file.close();

        qDebug() << "Updated config file:" << cfgfile;
    }
//    runBuildAndRestartServices();
}


//void mainwindows::editfileConfig(QString mesg)
//{
//    qDebug() << "editfileConfig received:" << mesg;

//    // Step 1: Parse JSON input
//    QJsonParseError parseError;
//    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8(), &parseError);
//    if (parseError.error != QJsonParseError::NoError) {
//        qWarning() << "JSON Parse Error:" << parseError.errorString();
//        return;
//    }

//    QJsonObject obj = doc.object();
//    int timeInterval = obj["timeInterval"].toInt(-1);
//    QString pathDirectory = obj["pathDirectory"].toString();

//    if (timeInterval <= 0 || pathDirectory.isEmpty()) {
//        qWarning() << "Invalid timeInterval or pathDirectory in JSON!";
//        return;
//    }

//    const QString cfgfile = FILESETTING;
//    const QString cfgfileINIT = FILESETTINGPATH;


//    QFile file(cfgfile);
//    if (!file.exists()) {
//        qWarning() << "Configuration file does not exist:" << cfgfile;
//        return;
//    }
//    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
//        qWarning() << "Failed to open config file for reading.";
//        return;
//    }
//    QTextStream in(&file);
//    QStringList lines;
//    while (!in.atEnd()) {
//        QString line = in.readLine();

//        if (line.trimmed().startsWith("record_interval=")) {
//            line = QString("record_interval=%1").arg(timeInterval);
//        } else if (line.trimmed().startsWith("record_directory=")) {
//            line = QString("record_directory=%1").arg(pathDirectory);
//        }

//        lines << line;
//    }
//    file.close();

//    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
//        qWarning() << "Failed to open config file for writing.";
//        return;
//    }

//    QTextStream out(&file);
//    for (const QString &line : lines) {
//        out << line << '\n';
//    }

//    file.close();
//    qDebug() << "Configuration file updated successfully.";



//    QFile file2(cfgfileINIT);
//    if (!file2.exists()) {
//        qWarning() << "Configuration file does not exist:" << cfgfileINIT;
//        return;
//    }
//    if (!file2.open(QIODevice::ReadOnly | QIODevice::Text)) {
//        qWarning() << "Failed to open config file for reading.";
//        return;
//    }

//    QTextStream in2(&file2);
//    QStringList lines2;
//    while (!in2.atEnd()) {
//        QString line2 = in2.readLine();

//        if (line2.trimmed().startsWith("record_interval=")) {
//            line2 = QString("record_interval=%1").arg(timeInterval);
//        } else if (line2.trimmed().startsWith("record_directory=")) {
//            line2 = QString("record_directory=%1").arg(pathDirectory);
//        }

//        lines << line2;
//    }
//    file.close();

//    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
//        qWarning() << "Failed to open config file for writing.";
//        return;
//    }

//    QTextStream out2(&file2);
//    for (const QString &line2 : lines2) {
//        out2 << line2 << '\n';
//    }
//    file2.close();
//    qDebug() << "Configuration file updated successfully.";
//}


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

void mainwindows::updateNewVolumeRecord(QString mesg) {
    qDebug() << "Received update:" << mesg;
    QJsonDocument d = QJsonDocument::fromJson(mesg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand =  QJsonValue(command["objectName"]).toString().trimmed();
    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8());

    if (!doc.isObject()) {
        qWarning() << "Invalid JSON data!";
        return;
    }

    QJsonObject obj = doc.object();
    int volume = obj["currentVolume"].toInt();
    int level = obj["level"].toInt();
    currentVolume = volume;
    updatelevel = level;
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
        currentVolume--;
    } else if (val == 1) {
        currentVolume++;
    }

    // จำกัดค่าระหว่าง 0–63
    currentVolume = qBound(0, currentVolume, 63);

    qDebug() << "Adjusted currentVolume:" << currentVolume;

    if (!max9850->setVolume(currentVolume)) {
        qWarning() << "Failed to set volume:" << currentVolume;
        return;
    }

    qDebug() << "Volume set to:" << currentVolume;

    int level = 0;
    QString volBar;

    if (currentVolume <= 0x05) {
        level = 0; volBar = "Vol: |||||||||||||| max";
    }
    else if (currentVolume <= 0x0D) {
        level = 1; volBar = "Vol: ||||||||||||";
    }
    else if (currentVolume <= 0x15) {
        level = 2; volBar = "Vol: ||||||||||";
    }
    else if (currentVolume <= 0x1E) {
        level = 3; volBar = "Vol: ||||||||";
    }
    else if (currentVolume <= 0x26) {
        level = 4; volBar = "Vol: ||||||";
    }
    else if (currentVolume <= 0x2E) {
        level = 5; volBar = "Vol: ||||";
    }
    else if (currentVolume <= 0x3E) {
        level = 6; volBar = "Vol: ||";
    }
    else {
        level = 7; volBar = "Vol: ";
    }

    displayInfo->setTextLine4_1(volBar);
    displayInfo->writeOLEDRun();
    mydatabase->recordVolume(currentVolume, level);
}

//void mainwindows::rotary(int encoderNum, int val, int InputEventID)
//{
//    qDebug() << "rotary:" << encoderNum << val << InputEventID;

//    if (val == -1) {
//        if (currentVolume > 0) currentVolume--;
//        qWarning() << "currentVolume--:" << currentVolume;

//    } else if (val == 1) {
//        if (currentVolume < 0x3F) currentVolume++;
//        qWarning() << "currentVolume++:" << currentVolume;
//    }

//    if (!max9850->setVolume(currentVolume)) {
//        qWarning() << "Failed to set volume:" << currentVolume;
//        return;
//    }

//    qDebug() << "Volume set to:" << currentVolume;

//    int level = 0;
//    if (currentVolume >= 0x00 && currentVolume <= 0x05) {
//        level = 0;
//        qDebug() << "Volume set level to:" << currentVolume << level;
//        QString Volume = "Vol: ||||||||||||||";
//        QString maxVol = Volume+"max";
//        displayInfo->setTextLine4_1(maxVol);
//        displayInfo->writeOLEDRun();
//        mydatabase->recordVolume(currentVolume,level);
//    }
//    else if (currentVolume > 0x05 &&currentVolume <= 0x0D){
//        level = 1;
//        qDebug() << "Volume set level to:" << currentVolume << level;
//        displayInfo->setTextLine4_1("Vol: ||||||||||||");
//        displayInfo->writeOLEDRun();
//        mydatabase->recordVolume(currentVolume,level);
//    }
//    else if (currentVolume > 0x0D &&currentVolume <= 0x15){
//        level = 2;
//        qDebug() << "Volume set level to:" << currentVolume << level;
//        displayInfo->setTextLine4_1("Vol: ||||||||||");
//        displayInfo->writeOLEDRun();
//        mydatabase->recordVolume(currentVolume,level);
//    }
//    else if (currentVolume > 0x15 &&currentVolume <= 0x1E){
//        level = 3;
//        qDebug() << "Volume set level to:" << currentVolume << level;
//        displayInfo->setTextLine4_1("Vol: ||||||||");
//        displayInfo->writeOLEDRun();
//        mydatabase->recordVolume(currentVolume,level);
//    }
//    else if (currentVolume > 0x1E &&currentVolume <= 0x26){
//        level = 4;
//        qDebug() << "Volume set level to:" << currentVolume << level;
//        displayInfo->setTextLine4_1("Vol: ||||||");
//        displayInfo->writeOLEDRun();
//        mydatabase->recordVolume(currentVolume,level);
//    }
//    else if (currentVolume > 0x26 &&currentVolume <= 0x2E){
//        level = 5;
//        qDebug() << "Volume set level to:" << currentVolume << level;
//        displayInfo->setTextLine4_1("Vol: ||||");
//        displayInfo->writeOLEDRun();
//        mydatabase->recordVolume(currentVolume,level);
//    }
//    else if (currentVolume > 0x2E &&currentVolume <= 0x3E){
//        level = 6;
//        qDebug() << "Volume set level to:" << currentVolume << level;
//        displayInfo->setTextLine4_1("Vol: ||");
//        displayInfo->writeOLEDRun();
//        mydatabase->recordVolume(currentVolume,level);
//    }
//    else {
//        level = 7;
//        qDebug() << "Volume set level to:" << currentVolume << level;
//        displayInfo->setTextLine4_1("Vol: ");
//        displayInfo->writeOLEDRun();
//        mydatabase->recordVolume(currentVolume,level);
//    }
//}


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


void mainwindows::liveSteamRecordRaido(QString mesg, QWebSocket* wClient) {
    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8());
    QJsonObject obj = doc.object();

    if (obj["menuID"].toString() != "startLiveStream")
        return;

    int sid = obj["sid"].toInt();
    QString channel = QString("ch%1").arg(sid); // เช่น ch3
    QString rtspUrl = QString("rtsp://127.0.0.1:654/live/%1/").arg(channel);

    // ❌ ไม่ต้องเช็คหรือลบ process เก่า

    QString outputDevice = "outLoop1"; // 🔧 หรือ generate ตาม sid เช่น "outLoop%1"
    QProcess* process = new QProcess(this);

    QStringList args = {
        "-fflags", "+genpts",
        "-use_wallclock_as_timestamps", "1",
        "-i", rtspUrl,
        "-ar", "8000",
        "-ac", "1",
        "-f", "alsa",
        outputDevice
    };

    process->setProgram("ffmpeg");
    process->setArguments(args);
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        QByteArray output = process->readAllStandardOutput();
        qDebug().noquote() << "[ffmpeg sid:" << sid << "]" << QString::fromUtf8(output).trimmed();
    });

    connect(process, &QProcess::started, this, [=]() {
        qDebug() << "ffmpeg started for sid:" << sid;
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int exitCode, QProcess::ExitStatus status) {
        qDebug() << "⚠ffmpeg exited for sid:" << sid << " code:" << exitCode;
        process->deleteLater(); // cleanup
    });

    qDebug() << "Starting ffmpeg sid:" << sid << ":" << process->program() << args;
    process->start();
    bool started = process->waitForStarted();
    if (started) {
        ffmpegProcessMap[sid] = process;
        qDebug() << "Stored process in ffmpegProcessMap for sid:" << sid;
    } else {
        qWarning() << "Failed to start ffmpeg for sid:" << sid;
        process->deleteLater();
    }
}



void mainwindows::stopLiveStream(QString mesg, QWebSocket* wClient) {
    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8());
    QJsonObject obj = doc.object();
    qDebug() << "stopLiveStream" << mesg;
    qDebug() << "📦 Available ffmpegProcessMap keys:" << ffmpegProcessMap.keys();
    if (obj["menuID"].toString() == "stopLiveStream") {
        int sid = obj["sid"].toInt();
        qDebug() << "sid:" << sid;

        if (ffmpegProcessMap.contains(sid)) {
            qDebug() << "ffmpegProcessMap_sid:" << sid;

            QProcess *proc = ffmpegProcessMap[sid];
            qDebug() << "process ffmpeg:" << sid;
            proc->terminate();
            proc->waitForFinished(3000);
            delete proc;
            ffmpegProcessMap.remove(sid);

            qDebug() << "🛑 Stopped FFmpeg for sid:" << sid;
        } else {
            qWarning() << "⚠️ No FFmpeg process found for sid:" << sid;
        }

        // ✅ Notify client
        QJsonObject reply;
        reply["menuID"] = "streamStopped";
        reply["sid"] = sid;
        wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }
}





//void mainwindows::liveSteamRecordRaido(QString mesg, QWebSocket* wClient) {
//    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8());
//    QJsonObject obj = doc.object();

//    if (obj["menuID"].toString() != "startLiveStream")
//        return;

//    int sid = obj["sid"].toInt();
//    QString channel = QString("ch%1").arg(sid); // เช่น ch3
//    QString rtspUrl = QString("rtsp://127.0.0.1:654/live/%1/").arg(channel);

//    if (ffmpegProcess) {
//        qDebug() << "🛑 Killing existing ffmpeg process";
//        ffmpegProcess->kill();
//        ffmpegProcess->deleteLater();
//        ffmpegProcess = nullptr;
//    }

//    ffmpegProcess = new QProcess(this);
//    QString outputDevice = "outLoop1";
//    QStringList args = {
//        "-fflags", "+genpts",
//        "-use_wallclock_as_timestamps", "1",
//        "-i", rtspUrl,
//        "-ar", "8000",
//        "-ac", "1",
//        "-f", "alsa",
//        outputDevice
//    };

//    ffmpegProcess->setProgram("ffmpeg");
//    ffmpegProcess->setArguments(args);
//    ffmpegProcess->setProcessChannelMode(QProcess::MergedChannels);

//    connect(ffmpegProcess, &QProcess::readyReadStandardOutput, this, [=]() {
//        QByteArray output = ffmpegProcess->readAllStandardOutput();
//        qDebug().noquote() << "[ffmpeg output]" << QString::fromUtf8(output).trimmed();
//    });

//    connect(ffmpegProcess, &QProcess::started, this, [=]() {
//        qDebug() << "ffmpeg started for sid:" << sid;
//    });

//    connect(ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
//            this, [=](int exitCode, QProcess::ExitStatus status) {
//        qDebug() << "⚠️ ffmpeg exited with code:" << exitCode;
//        ffmpegProcess->deleteLater();
//        ffmpegProcess = nullptr;
//    });

//    qDebug() << "Running ffmpeg:" << ffmpegProcess->program() << args;
//    ffmpegProcess->start();
//}



//void mainwindows::liveSteamRecordRaido(QString mesg, QWebSocket* wClient) {
//    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8());
//      QJsonObject obj = doc.object();

//      if (obj["menuID"].toString() != "startLiveStream")
//          return;

//      int sid = obj["sid"].toInt();
//      QString channel = QString("ch%1").arg(sid); // เช่น ch3
//      QString rtspUrl = QString("rtsp://127.0.0.1:654/live/%1/").arg(channel);

//      // หยุด process เดิมถ้ามี
//      if (ffmpegProcess) {
//          qDebug() << "🛑 Killing existing ffmpeg process";
//          ffmpegProcess->kill();
//          ffmpegProcess->deleteLater();
//          ffmpegProcess = nullptr;
//      }

//      // สร้างคำสั่ง ffmpeg ใหม่
//      ffmpegProcess = new QProcess(this);
//      QStringList args = {
//          "-fflags", "+genpts",
//          "-use_wallclock_as_timestamps", "1",
//          "-i", rtspUrl,
//          "-ar", "8000",
//          "-ac", "1",
//          "-f", "alsa",
//          "outLoop1"
//      };

//      ffmpegProcess->setProgram("ffmpeg");
//      ffmpegProcess->setArguments(args);
//      ffmpegProcess->setProcessChannelMode(QProcess::MergedChannels);

//      // log ข้อมูล stdout/stderr ถ้าต้องการ debug
//      connect(ffmpegProcess, &QProcess::readyReadStandardOutput, this, [=]() {
//          QByteArray output = ffmpegProcess->readAllStandardOutput();
//          qDebug() << "[ffmpeg]" << output;
//      });

//      // เมื่อ ffmpeg เริ่ม
//      connect(ffmpegProcess, &QProcess::started, this, [=]() {
//          qDebug() << "ffmpeg started for sid:" << sid;
//      });

//      connect(ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
//              this, [=](int exitCode, QProcess::ExitStatus status) {
//          qDebug() << "ffmpeg exited with code:" << exitCode;
//          ffmpegProcess->deleteLater();
//          ffmpegProcess = nullptr;
//      });

//      // เริ่ม process
//      qDebug() << "Running ffmpeg:" << ffmpegProcess->program() << args;
//      ffmpegProcess->start();
//}


//void mainwindows::liveSteamRecordRaido(QString mesg, QWebSocket* wClient) {
//    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8());
//    QJsonObject obj = doc.object();

//    if (obj["menuID"].toString() != "startLiveStream")
//        return;

//    int sid = obj["sid"].toInt();
//    QString ip = obj["ip"].toString();
//    QString rtspUrl = QString("rtsp://127.0.0.1:654/live/ch%1/").arg(sid);
//    QString outputBase = QString("/var/www/html/audiofiles/ch%1_part").arg(sid);

//    // Start ffmpeg to create HLS files with segment rotation
//    QStringList arguments;
//    arguments << "-fflags" << "+genpts"
//              << "-use_wallclock_as_timestamps" << "1"
//              << "-i" << rtspUrl
//              << "-codec" << "copy"
//              << "-f" << "hls"
//              << "-hls_time" << "5"                // 5-second segments
//              << "-hls_list_size" << "1"           // only 1 segment in list
//              << "-hls_segment_filename" << QString("%1%d.ts").arg(outputBase)
//              << QString("%1%d.m3u8").arg(outputBase);  // chX_part1.m3u8, chX_part2.m3u8...

//    QProcess *ffmpegProcess = new QProcess(this);
//    ffmpegProcess->start("ffmpeg", arguments);
//    ffmpegProcess->setProcessChannelMode(QProcess::MergedChannels);

//    // Start timer to check for .m3u8 file existence and notify client
//    QTimer *notifyTimer = new QTimer(this);
//    notifyTimer->setInterval(1000); // check every 1 second
//    int *part = new int(1);         // current part being monitored

//    connect(notifyTimer, &QTimer::timeout, this, [=]() mutable {
//        QString m3u8Filename = QString("ch%1_part%2.m3u8").arg(sid).arg(*part);
//        QString fullPath = QString("/var/www/html/audiofiles/%1").arg(m3u8Filename);
//        QFile checkFile(fullPath);

//        if (checkFile.exists()) {
//            QJsonObject reply;
//            reply["menuID"] = "streamStarted";
//            reply["sid"] = sid;
//            reply["part"] = *part;
//            reply["m3u8Path"] = QString("/audiofiles/%1").arg(m3u8Filename);

//            if (wClient && wClient->isValid()) {
//                wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
//            }

//            *part = (*part % 3) + 1; // rotate part 1→2→3→1
//        }
//    });

//    notifyTimer->start();

//    // Optional: Clean up process and timer later
//    connect(ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
//            this, [=](int, QProcess::ExitStatus) {
//                notifyTimer->stop();
//                delete notifyTimer;
//                delete part;
//                ffmpegProcess->deleteLater();
//            });
//}


//void mainwindows::liveSteamRecordRaido(QString mesg, QWebSocket* wClient) {
//    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8());
//    QJsonObject obj = doc.object();

//    if (obj["menuID"].toString() != "startLiveStream")
//        return;

//    int sid = obj["sid"].toInt();
//    QString targetPath = "/home/orinnx/liveStream";
//    QString symlinkPath = "/var/www/html/audiofiles";
//    QString rtspUrl = QString("rtsp://127.0.0.1:654/live/ch%1/").arg(sid);

//    // สร้างโฟลเดอร์ถ้ายังไม่มี
//    QDir().mkpath(targetPath);

//    // ตั้ง symlink
//    QFileInfo symlinkInfo(symlinkPath);
//    if (symlinkInfo.exists() || symlinkInfo.isSymLink())
//        QFile::remove(symlinkPath);
//    QProcess::execute("ln", QStringList() << "-s" << targetPath << symlinkPath);

//    // เริ่ม thread สำหรับวนลูปสตรีม
//    QtConcurrent::run([=]() {
//        int part = 1;
//        while (true) {
//            QString outputPath = QString("%1/ch%2_part%3.m3u8").arg(targetPath).arg(sid).arg(part);
//            QStringList args;
//            args << "-y" << "-i" << rtspUrl
//                 << "-vn"
//                 << "-acodec" << "aac"
//                 << "-f" << "hls"
//                 << "-hls_time" << "2"
//                 << "-hls_list_size" << "3"
//                 << "-hls_flags" << "delete_segments"
//                 << outputPath;

//            QProcess *ffmpegProc = new QProcess();
//            ffmpegProc->setProcessChannelMode(QProcess::MergedChannels);

//            QObject::connect(ffmpegProc, &QProcess::readyReadStandardOutput, [=]() {
//                qDebug() << "[ffmpeg stdout]" << ffmpegProc->readAllStandardOutput().trimmed();
//            });
//            QObject::connect(ffmpegProc, &QProcess::readyReadStandardError, [=]() {
//                qDebug() << "[ffmpeg stderr]" << ffmpegProc->readAllStandardError().trimmed();
//            });

//            ffmpegProc->start("ffmpeg", args);
//            if (!ffmpegProc->waitForStarted(3000)) {
//                qWarning() << "❌ FFmpeg failed to start for part" << part;
//                delete ffmpegProc;
//                continue;
//            }

//            // รอจนกว่าจะมี .m3u8
//            QFileInfo checkFile(outputPath);
//            int waitMs = 0;
//            while (!checkFile.exists() && waitMs < 5000) {
//                QThread::msleep(200);
//                waitMs += 200;
//                checkFile.refresh();
//            }

//            if (checkFile.exists()) {
//                QJsonObject reply;
//                reply["menuID"] = "streamStarted";
//                reply["sid"] = sid;
//                reply["part"] = part;
//                reply["m3u8Path"] = QString("/audiofiles/ch%1_part%2.m3u8").arg(sid).arg(part);
//                QMetaObject::invokeMethod(this, [=]() {
//                    if (wClient && wClient->isValid()) {
//                        wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
//                    }
//                }, Qt::QueuedConnection);
//            }

//            // ปล่อย ffmpeg รันสักระยะ แล้ว kill แล้วหมุน part
//            QThread::sleep(9);
//            ffmpegProc->terminate();
//            ffmpegProc->waitForFinished(2000);
//            delete ffmpegProc;

//            part = (part % 3) + 1; // 1 → 2 → 3 → 1 → ...
//        }
//    });
//}



//void mainwindows::stopLiveStream(QString mesg, QWebSocket* wClient) {
//    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8());
//    QJsonObject obj = doc.object();

//    if (obj["menuID"].toString() == "stopLiveStream") {
//        int sid = obj["sid"].toInt();

//        // 🔴 หยุด process ก่อน
//        if (ffmpegProcessMap.contains(sid)) {
//            QProcess *proc = ffmpegProcessMap[sid];
//            proc->terminate();
//            proc->waitForFinished(3000);
//            delete proc;
//            ffmpegProcessMap.remove(sid);

//            qDebug() << "🛑 Stopped FFmpeg for sid:" << sid;
//        } else {
//            qWarning() << "⚠️ No FFmpeg process found for sid:" << sid;
//        }

////        // 🧹 ลบไฟล์ที่เกี่ยวข้อง
////        QString dirPath = "/home/orinnx/liveStream";
////        QDir dir(dirPath);
////        QStringList filters;
////        filters << QString("ch%1_part*.m3u8").arg(sid)
////                << QString("ch%1_part*.ts").arg(sid);  // ลบ segment ด้วย

////        QFileInfoList filesToRemove = dir.entryInfoList(filters, QDir::Files);
////        for (const QFileInfo &fileInfo : filesToRemove) {
////            QFile::remove(fileInfo.absoluteFilePath());
////            qDebug() << "🗑️ Deleted file:" << fileInfo.fileName();
////        }

//        // ✅ แจ้ง client
//        QJsonObject reply;
//        reply["menuID"] = "streamStopped";
//        reply["sid"] = sid;
//        wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
//    }
//}


//void mainwindows::liveSteamRecordRaido(QString mesg, QWebSocket* wClient) {
//    QJsonDocument doc = QJsonDocument::fromJson(mesg.toUtf8());
//    QJsonObject obj = doc.object();

//    if (obj["menuID"].toString() == "startLiveStream") {
//        int sid = obj["sid"].toInt();
//        QString rtspUrl = QString("rtsp://127.0.0.1:654/live/ch%1/").arg(sid);
//        QString outputPath = QString("/home/orinnx/liveStream/ch%1.m3u8").arg(sid);

//        QString symlinkPath = "/var/www/html/audiofiles";
//        QString targetPath = "/home/orinnx/liveStream";
//        QFileInfo symlinkInfo(symlinkPath);
//        if (symlinkInfo.exists() || symlinkInfo.isSymLink()) {
//            QFile::remove(symlinkPath);
//            qDebug() << "Removed old symlink:" << symlinkPath;
//        }

//        QProcess lnProc;
//        lnProc.start("ln", QStringList() << "-s" << targetPath << symlinkPath);
//        lnProc.waitForFinished();
//        qDebug() << "Created:" << symlinkPath << "→" << targetPath;

//        QStringList args;
//        args << "-i" << rtspUrl
//             << "-vn"
//             << "-acodec" << "aac"
//             << "-f" << "hls"
//             << "-hls_time" << "2"
//             << "-hls_list_size" << "3"
//             << "-hls_flags" << "delete_segments"
//             << outputPath;

//        QProcess *ffmpegProc = new QProcess(this);
//        ffmpegProc->setProcessChannelMode(QProcess::MergedChannels);
//        connect(ffmpegProc, &QProcess::readyReadStandardOutput, [=]() {
//            qDebug() << "[ffmpeg output]" << ffmpegProc->readAllStandardOutput();
//        });

//        ffmpegProc->start("ffmpeg", args);
//        qDebug() << "treaming sid:" << sid << "from" << rtspUrl << "to:" << outputPath;

//        QFileInfo checkFile(outputPath);
//        int waitMs = 0;
//        while (!checkFile.exists() && waitMs < 3000) {
//            QThread::msleep(200);
//            waitMs += 200;
//            checkFile.refresh();
//        }

//        if (checkFile.exists()) {
//            QJsonObject reply;
//            reply["menuID"] = "streamStarted";
//            reply["sid"] = sid;
//            wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
//        } else {
//            qWarning() << "Failed to generate .m3u8 file in time.";
//        }
//    }
//}

//void mainwindows::liveSteamRecordRaido(QString mesg, QWebSocket* wClient) {
//    QByteArray br = mesg.toUtf8();
//    QJsonDocument doc = QJsonDocument::fromJson(br);
//    QJsonObject obj = doc.object();
//    QJsonObject command = doc.object();
//    QString getCommand =  QJsonValue(obj["objectName"]).toString();
//    if (obj["menuID"].toString() == "startLiveStream") {
//        int sid = obj["sid"].toInt();
//        // QString ip = obj["ip"].toString(); // ❌ ไม่ต้องใช้ ip ของอุปกรณ์

//        // ✅ เปลี่ยนมาใช้ localhost หรือ IP ของเครื่องที่มี RTSP stream อยู่
//        QString rtspUrl = QString("rtsp://127.0.0.1:654/live/ch%1/").arg(sid);
//        QString outputPath = QString("/var/www/html/live/ch%1.m3u8").arg(sid);

//        QStringList args;
//        args << "-i" << rtspUrl
//             << "-vn"
//             << "-acodec" << "aac"
//             << "-f" << "hls"
//             << "-hls_time" << "2"
//             << "-hls_list_size" << "3"
//             << "-hls_flags" << "delete_segments"
//             << outputPath;

//        QProcess *ffmpegProcess = new QProcess(this);
//        ffmpegProcess->start("ffmpeg", args);

//        qDebug() << "[FFmpeg] Start stream for ch" << sid << "from:" << rtspUrl;
//    }


//}

void mainwindows::updateSystem(QString message) {
    qDebug() << "message:" << message;
    QJsonDocument d = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject command = d.object();
    QString getCommand =  QJsonValue(command["objectName"]).toString();
//    QString lastFirmWare = command["text"].toString();
//    QString lastFirmWareupdate = command["text"].toString();
    QString lastFirmWare = command["swversion"].toString();
    QString lastFirmWareupdate = command["swversion"].toString();

    QString link = command["link"].toString();
    qDebug() << "lastFirmWare for download" << lastFirmWare << "currentFirmWare running" << SwVersion << link;
    if (((SwVersion.split(" ").size() >= 2) & (lastFirmWare.split(" ").size() >= 2)) == false)
    {
        if (SwVersion != lastFirmWare)
        {
            qDebug() << "(lastFirmWare != currentFirmWare)" << "lastFirmWare for download" << lastFirmWare << "currentFirmWare running" << SwVersion;
            SwVersion=lastFirmWareupdate;

            downloader->downloadFile(link, "/var/www/html/uploads/update.tar");
        }
    }
    else if (SwVersion != lastFirmWare)
    {
        double currentVersion =  QString(SwVersion.split(" ").at(1)).toDouble();
        double lastVersion = QString(lastFirmWare.split(" ").at(1)).toDouble();
        QString currentFirmWare = SwVersion.split(" ").at(0);
        lastFirmWare = lastFirmWare.split(" ").at(0);
        qDebug() << "update lastFirmWare for download" << lastVersion << lastFirmWare << "currentFirmWare running" << currentVersion << currentFirmWare;
        if (lastVersion > currentVersion)
        {
            qDebug() << "(lastFirmWare != currentFirmWare)" << "lastFirmWare for download" << lastVersion << lastFirmWare << "currentFirmWare running" << currentVersion << currentFirmWare;
            SwVersion=lastFirmWareupdate;
            qDebug() << "SwVersion:" << SwVersion;
            downloader->downloadFile(link, "/var/www/html/uploads/update.tar");

        }
    }
}

void mainwindows::downloadCompleted(const QString &outputPath)
{
    qDebug() << "downloadCompleted:" << outputPath;

    if (outputPath.contains("update.tar"))
    {
        updateFirmware();
    }
}

void mainwindows::updateFirmware()
{
    system("sync");
    qDebug() << "Start updateFirmware";
    foundfileupdate = true;
    QStringList fileupdate;
    fileupdate = findFile();
    system("sudo mkdir -p /tmp/update");

    if (fileupdate.size() > 0) {
        qDebug() << "Start update";
        QString commandCopyFile = "cp " + QString(fileupdate.at(0)) + " /tmp/update/update.tar";
        qDebug() << "commandCopyFile:" << commandCopyFile;
        system(commandCopyFile.toStdString().c_str());
        system("tar -xf /tmp/update/update.tar -C /tmp/update/");
        system("cp $(find /tmp/update -name 'README.txt' | head -n1) /home/orinnx/README.txt");
        qDebug() << "Before update complete";
        system("sh /tmp/update/update.sh");
        qDebug() << "Update script done";
        system("rm -rf /tmp/update");
        qDebug() << "Update complete";
        system("sudo systemctl restart iRecord.service");
        system("sync");
        qDebug() << "Exiting now";
        exit(0);
    }

    foundfileupdate = false;
}

//void mainwindows::updateFirmware()
//{
//    system("sync");
//    qDebug() << "Start updateFirmware";
//    foundfileupdate = true;
//    QStringList fileupdate;
//    fileupdate = findFile();
//    system("sudo mkdir -p /tmp/update");
//    if (fileupdate.size() > 0){
////        updateScreenOnOff(true);
//        qDebug() << "Start update";
//        QString commandCopyFile = "cp " + QString(fileupdate.at(0)) + " /tmp/update/update.tar";
//        qDebug() << "commandCopyFile:" << commandCopyFile;
//        system(commandCopyFile.toStdString().c_str());
//        system("tar -xf /tmp/update/update.tar -C /tmp/update/");
////        system("sudo cp /home/pi/update.sh /tmp/update/");
//        system("sh /tmp/update/update.sh");
//        system("rm -rf /tmp/update");
////        system("rm -rf /usr/share/apache2/default-site/htdocs/uploads/*");
//        qDebug() << "Update complete";
//        system("sync");
//        exit(0);
//    }
//    foundfileupdate = false;
//}


QStringList mainwindows::findFile()
{
    QStringList listfilename;
    QString ss="/var/www/html/uploads/";
    const char *sss ;
    sss = ss.toStdString().c_str();
    QDir dir1("/var/www/html/uploads/");
    QString filepath;
    QString filename;
    QFileInfoList fi1List( dir1.entryInfoList( QDir::Files, QDir::Name) );
    foreach( const QFileInfo & fi1, fi1List ) {
        filepath = QString::fromUtf8(fi1.absoluteFilePath().toLocal8Bit());
        filename = QString::fromUtf8(fi1.fileName().toLocal8Bit());
        listfilename << filepath;
        qDebug() << filepath;// << filepath.toUtf8().toHex();
    }
    return listfilename;
}




void mainwindows::playWavFile(const QString& filePath) {
    qDebug() << "Playing WAV file:" << filePath;

    QString safePath = QDir::cleanPath(filePath);
    QString cmd = QString("ffplay -nodisp -autoexit -loglevel quiet \"%1\" &").arg(safePath);
    qDebug() << "Command:" << cmd;

    int result = system(cmd.toUtf8().constData());
    if (result != 0) {
        qWarning() << "❌ Failed to execute ffplay command!";
    }
}


void mainwindows::formatExternal(const QString &filePath) {
    qDebug() << "formatExternal called with:" << filePath;
    QByteArray br = filePath.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();
    QJsonObject command = doc.object();
    QString getfilePath =  QJsonValue(obj["storage"]).toString();

    QString getfilePath1 =  QJsonValue(obj["external1"]).toString();
    QString getfilePath2 =  QJsonValue(obj["external2"]).toString();

    QString targetDevice;
    QString partition;
    QString mountPath;

    if (getfilePath == "external1") {
        targetDevice = "/dev/nvme1n1";
        partition = "/dev/nvme1n1p1";
        mountPath = "/media/SSD1";
        qDebug() << "external1:" << filePath ;

    } else if (getfilePath == "external2") {
        targetDevice = "/dev/nvme2n1";
        partition = "/dev/nvme2n1p1";
        mountPath = "/media/SSD2";
        qDebug() << "external2:" << filePath ;

    } else {
        qWarning() << "Invalid filePath argument:" << filePath;
        return;
    }

    QDir().mkpath(mountPath);
    QProcess::execute("umount", QStringList() << mountPath);
    QProcess::execute("wipefs", QStringList() << "-a" << targetDevice);
    QProcess::execute("parted", QStringList() << "-s" << targetDevice << "mklabel" << "gpt");
    QProcess::execute("parted", QStringList() << "-s" << targetDevice << "mkpart" << "primary" << "ext4" << "0%" << "100%");
    QProcess::execute("mkfs.ext4", QStringList() << "-F" << partition);
    QProcess::execute("mount", QStringList() << partition << mountPath);

    qDebug() << "Successfully formatted and mounted" << partition << "to" << mountPath;
}


void* mainwindows::ThreadFunc(void* pTr)
{
    mainwindows* pThis = static_cast<mainwindows*>(pTr);
    while (true) {
        emit pThis->getDateTime();
        QThread::msleep(1000);
    }
    return nullptr;
}

void* mainwindows::ThreadFunc2(void* pTr )
{
    mainwindows* pThis = static_cast<mainwindows*>(pTr);
    while (true) {
        pThis->readSSDStorage();
        QThread::msleep(1000);
    }
    return NULL;
}

void* mainwindows::ThreadFunc3(void* pTr)
{
    mainwindows* pThis = static_cast<mainwindows*>(pTr);
    while (true) {
        pThis->max31760->tempDetect();
        QThread::msleep(1000);
    }
    return NULL;
}



//void mainwindows::autoLiveStream(QString) {

//}



void mainwindows::runBuildAndRestartServices() {
    QString workDir = "/home/orinnx/alphatest/irecd1.0.4/src/";

    // Step 1: make build
    QProcess build;
    build.setWorkingDirectory(workDir);
    build.start("make", QStringList() << "build");
    if (!build.waitForFinished() || build.exitCode() != 0) {
        qWarning() << "make build failed:" << build.readAllStandardError();
        return;
    }
    qDebug() << "make build completed.";

    // Step 2: make install
    QProcess install;
    install.setWorkingDirectory(workDir);
    install.start("make", QStringList() << "install");
    if (!install.waitForFinished() || install.exitCode() != 0) {
        qWarning() << "make install failed:" << install.readAllStandardError();
        return;
    }
    qDebug() << "make install completed.";

    // Step 3: restart services
    QProcess restart;
    QString command = "sudo systemctl restart irecd.service iplayd.service";
    restart.start("bash", QStringList() << "-c" << command);
    if (!restart.waitForFinished() || restart.exitCode() != 0) {
        qWarning() << "Failed to restart services:" << restart.readAllStandardError();
        return;
    }

    qDebug() << "Services restarted successfully.";
}



//    system("speaker-test -D hw:3,1 -r 8000  -c 2 -t sine -f 1000");
//    system("echo PE.06 > /sys/class/gpio/export");
//    QThread::msleep(100);
//    system("echo out > /sys/class/gpio/PE.06/direction");
//    QThread::msleep(100);
//    system("echo 1 > /sys/class/gpio/PE.06/value");
//    system("speaker-test -D hw:3,3 -r 48000 -c 2 -f 1000 -t sine -S 1");
