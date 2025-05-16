#include "infoDisplay.h"
#include <QDebug>
#include <QByteArray>
infoDisplay::infoDisplay(QObject *parent) : QObject(parent)
{
    OLEDSPI = new SPIClass(SPIDEV);

}
void infoDisplay::setTextLine1_1(QString text)
{
    setTextLine1(text.toStdString().c_str(),LINE1INDEX1);
}
void infoDisplay::setTextLine1_2(QString text)
{
    setTextLine1(text.toStdString().c_str(),LINE1INDEX2);
}
void infoDisplay::setTextLine2_1(QString text)
{
    setTextLine2(text.toStdString().c_str(),LINE2INDEX1);
}
void infoDisplay::setTextLine2_2(QString text)
{
    setTextLine2(text.toStdString().c_str(),LINE2INDEX2);
}
void infoDisplay::setTextLine3_1(QString text)
{
    setTextLine3(text.toStdString().c_str(),LINE3INDEX1);
}
void infoDisplay::setTextLine3_2(QString text)
{
    setTextLine3(text.toStdString().c_str(),LINE3INDEX2);
}
void infoDisplay::setTextLine4_1(QString text)
{
    setTextLine4(text.toStdString().c_str(), LINE4INDEX1);
}
void infoDisplay::setTextLine4_2(QString text)
{
    setTextLine4(text.toStdString().c_str(), LINE4INDEX2);
}

void infoDisplay::setTextLine1(const char *text, int index)
{
    bool textEnd = false;
    for(int i=0; i<LINESIZE/2; i++)
    {
        if (textEnd == false){
            if (text[i] == '\0') textEnd = true;
            tx[index+i] = text[i];
        }
        else
            tx[index+i] = 0x00;
    }
}
void infoDisplay::setTextLine2(const char *text, int index)
{
    bool textEnd = false;
    for(int i=0; i<LINESIZE/2; i++)
    {
        if (textEnd == false){
            if (text[i] == '\0') textEnd = true;
            tx[index+i] = text[i];
        }
        else
            tx[index+i] = 0x00;
    }
}
void infoDisplay::setTextLine3(const char *text, int index)
{
    bool textEnd = false;
    for(int i=0; i<LINESIZE/2; i++)
    {
        if (textEnd == false){
            if (text[i] == '\0') textEnd = true;
            tx[index+i] = text[i];
        }
        else
            tx[index+i] = 0x00;
    }
}
void infoDisplay::setTextLine4(const char *text, int index)
{
    bool textEnd = false;
    for(int i=0; i<LINESIZE/2; i++)
    {
        if (textEnd == false){
            if (text[i] == '\0') textEnd = true;
            tx[index+i] = text[i];
        }
        else
            tx[index+i] = 0x00;
    }
}
//void infoDisplay::setTextLine3(const QString &text)
//{
//    QByteArray byteText = text.toUtf8();
//    int length = qMin(byteText.size(), LINESIZE);

//    for (int i = 0; i < LINESIZE; ++i)
//    {
//        if (i < length)
//            tx[LINE3INDEX + i] = static_cast<quint8>(byteText[i]);
//        else
//            tx[LINE3INDEX + i] = 0x00;
//    }
//}


//void infoDisplay::setTextLine3(const char *text)
//{
//    bool textEnd = false;
//    for(int i=0; i<LINESIZE; i++)
//    {
//        if (textEnd == false){
//            if (text[i] == '\0') textEnd = true;
//            tx[LINE3INDEX+i] = text[i];
//        }
//        else
//            tx[LINE3INDEX+i] = 0x00;
//    }
//}

void infoDisplay::setGpsStatus(bool gps1, bool gps2)
{
    tx[GPS1STAGE] = gps1;
    tx[GPS2STAGE] = gps2;
}
void infoDisplay::updateIPAddress(QString networkID, QString ipaddress, bool connected)
{
    if (ipaddress == "") ipaddress = "0.0.0.0";
    QStringList listIP =  ipaddress.split(".");
    if (listIP.size() == 4)
    {
        if (networkID == "eth0")
        {
            tx[IPADDRESS1_0] = static_cast<uint8_t>(listIP.at(0).toInt());
            tx[IPADDRESS1_1] = static_cast<uint8_t>(listIP.at(1).toInt());
            tx[IPADDRESS1_2] = static_cast<uint8_t>(listIP.at(2).toInt());
            tx[IPADDRESS1_3] = static_cast<uint8_t>(listIP.at(3).toInt());
            tx[ETH1_CONN] = connected;
        }
        else if (networkID == "eth1")
        {
            tx[IPADDRESS2_0] = static_cast<uint8_t>(listIP.at(0).toInt());
            tx[IPADDRESS2_1] = static_cast<uint8_t>(listIP.at(1).toInt());
            tx[IPADDRESS2_2] = static_cast<uint8_t>(listIP.at(2).toInt());
            tx[IPADDRESS2_3] = static_cast<uint8_t>(listIP.at(3).toInt());
            tx[ETH2_CONN] = connected;
        }
        else if (networkID == "bond0")
        {
            tx[IPADDRESS3_0] = static_cast<uint8_t>(listIP.at(0).toInt());
            tx[IPADDRESS3_1] = static_cast<uint8_t>(listIP.at(1).toInt());
            tx[IPADDRESS3_2] = static_cast<uint8_t>(listIP.at(2).toInt());
            tx[IPADDRESS3_3] = static_cast<uint8_t>(listIP.at(3).toInt());
            tx[BOND_ACTIVE] = connected;
        }
    }
    //writeOLED();
}
void infoDisplay::checkSum()
{
    uint8_t chk = 0;
    for (int i=0;i<BUFFERSIZE-1;i++)
    {
        chk ^= tx[i];
    }
    tx[CHECKSUM] = chk;
}
bool infoDisplay::checkNewData()
{
    countSend++;
    if (countSend >= 10){
        countSend = 0;
//        qDebug() << "resend oled data";
        return true;
    }
    for (int i=0;i<BUFFERSIZE;i++)
    {
        if (tx[i] != tx_tmp[i]){
//            qDebug() << "send new oled data";
            return true;
        }
    }
    return false;
}
void infoDisplay::writeOLEDRun()
{
    writeOLED();
}

void infoDisplay::writeOLED()
{
    if (checkNewData() == false) return;
    messageID++;
    tx[MESSAGEID] = messageID;
    tx[0] = 0xAA;
    tx[1] = 0xBB;
    checkSum();
//    QByteArray qByteArray;

    OLEDSPI->send_byte_data(tx,rx,BUFFERSIZE);
    for (int i=0;i<BUFFERSIZE;i++)
    {
        tx_tmp[i] = tx[i];
//        qByteArray.append(tx[i]);
    }
//    qDebug() << qByteArray;
    countSend = 0;
}
