#include "I2CReadWrite.h"

I2CReadWrite::I2CReadWrite(const char* i2c_dev, int i2c_address)
{
    file_i2c = open(i2c_dev,O_RDWR);
    if(ioctl(file_i2c,I2C_SLAVE,i2c_address) < 0){
        printf("i2c not open");
    }
    sprintf(i2cdevice,"%s",i2c_dev);
    i2cAddress = i2c_address;
    printf("file_i2c. %s %d\n",i2c_dev,file_i2c);
}
bool I2CReadWrite::readi2cData(){
    write(file_i2c, buffer, 1);
    int len = read(file_i2c, buffer, length);
    if (len != length) {
        printf("Failed to read from the i2c bus %s 0x%02x. 0x%02x 0x%02x 0x%02x 0x%02x\n",i2cdevice, i2cAddress, buffer[0],buffer[1], length, len);
        return  false;
    }
    else {
//        printf("Data read: %d %d\n", buffer[0],buffer[1]);
    }
    return true;
}
bool I2CReadWrite::writeBytes(){
    int len = write(file_i2c, buffer, length);
    if (len != length) {
        printf("Failed to write to the i2c bus %s 0x%02x. address 0x%02x data 0x%02x \n",i2cdevice, i2cAddress, buffer[0], buffer[1]);
        return false;
    }
    else {
//        printf("Write to the i2c bus. %d %d %d\n",buffer[0], buffer[1], len);
    }
    return true;
}
