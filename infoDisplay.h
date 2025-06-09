#ifndef INFODISPLAY_H
#define INFODISPLAY_H
/*
 * Line 1: iClock NTP Server            (ASCI) [63:0]
 * Line 2: GPS1: searching GPS2: Locked (BOOL) [65:64]
 * Line 3: Source: PPS1                 (ASCI) [129:66]
 * Line 4: L1: 192.168.10.20 L2: 192.168.10.20 (HEX) [139:130]
 */
#include <QObject>
#include <SPI.h>
#include <QString>

//#define BUFFERSIZE           	  150
//#define LINESIZE           			 64

//#define LINE2INDEX1          		  2
//#define LINE2INDEX2          		 34

#define BUFFERSIZE           	  230
#define LINESIZE           		   64

#define LINE1INDEX1          		  2
#define LINE1INDEX2          		 34
#define LINE2INDEX1          		 66
#define LINE2INDEX2          		 98
#define LINE3INDEX1          		 130
//#define LINE3INDEX2          		 162
#define LINE4INDEX1          		 162
//#define LINE4INDEX2          		 226
#define GPS1STAGE           66
#define GPS2STAGE           67
#define LINE3INDEX          68

#define IPADDRESS1_0 		132
#define IPADDRESS1_1 		133
#define IPADDRESS1_2 		134
#define IPADDRESS1_3 		135
#define IPADDRESS2_0 		136
#define IPADDRESS2_1 		137
#define IPADDRESS2_2 		138
#define IPADDRESS2_3 		139

#define BOND_ACTIVE         142
#define IPADDRESS3_0 		143
#define IPADDRESS3_1 		144
#define IPADDRESS3_2 		145
#define IPADDRESS3_3 		146

#define	ETH1_CONN			140
#define	ETH2_CONN			141

#define CHECKSUM        (BUFFERSIZE-1)

#define MESSAGEID		(BUFFERSIZE-2)
#define SPIDEV          "/dev/spidev2.0"

class infoDisplay : public QObject
{
    Q_OBJECT
public:
    explicit infoDisplay(QObject *parent = nullptr);
    void setTextLine1_1(QString text);
    void setTextLine1_2(QString text);
    void setTextLine2_1(QString text);
    void setTextLine2_2(QString text);
    void setTextLine3_1(QString text);
    void setTextLine3_2(QString text);
    void setTextLine4_1(QString text);
    void setTextLine4_2(QString text);
    void setTextLine1(const char *text, int index);
    void setTextLine2(const char *text, int index);
    void setTextLine3(const char *text, int index);
    void setTextLine4(const char *text, int index);

//    void setTextLine3(const QString &text);
//    void setTextLine3(const char *text);
    void setGpsStatus(bool gps1, bool gps2);
    void updateIPAddress(QString networkID, QString ipaddress, bool connected);
    void writeOLEDRun();
signals:

private:
    int countSend = 0;
    char *spiDev;
    SPIClass *OLEDSPI;
    uint8_t messageID = 0;
    uint8_t tx_tmp[BUFFERSIZE];
    uint8_t tx[BUFFERSIZE];
    uint8_t rx[BUFFERSIZE];
    bool checkNewData();
    void checkSum();
    void writeOLED();


};

#endif // INFODISPLAY_H
