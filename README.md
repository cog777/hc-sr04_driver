hc-sr04_driver
==============

HC SR04 Ultrasonic Ranging Module Driver

ABOUT THIS DRIVER
=================
The driver enables to trigger a measurement on the HC SR04 Ultrasonic Ranging Module. The measuring can be triggerd by writing an arbitrary 32 bit integer number 
(take care of unbuffering writing!) to /dev/us_service character device file. The lasted time of measurement can be read (32 bit unsigned integer) 
from /dev/us_service after 60 ms. It can be coverted to distance in cm using devide by 58000 or in inch using devide by 148000. 
Please read the documentation https://docs.google.com/document/d/1Y-yZnNhMYy7rwhAgyL_pfa39RsB-x2qR4vP8saG73rE/edit?pli=1 or http://www.micropik.com/PDF/HCSR04.pdf.

REQUIREMENTS
============
Originally the driver was designed for Raspberry Pi but probably it should work on other Single Board Computers because Raspberry Pi specific instructions aren't used. 
In addition a simple circuit is needed to attach the HC SR04. Please have a look at the ultrasonic_scheme.png and ultrasonic_breadboard.png.

BUILD INSTRUCTIONS
==================
First of all a kernel source should be downloaded from github (https://github.com/raspberrypi/linux) and compiled. The /lib/modules/(current kernel version)/build should 
point to the current kernel source directory. Please edit the driver source and change the echo and trigger GPIO pins:

#define GPIO_ECHO 3 // GPIO 3 the echo pin
#define GPIO_TRIGGER 2 // GPIO 2 trigger pin

The echo pin is GPIO2 and trigger is GPIO3 by default.

The "make" should produce us_service.ko kernel module.

USAGE INSTRUCTIONS
==================

Load the kernel driver:
  sudo insmod ./us_service.ko

Create a device file in /dev
  sudo mknod -m 666 /dev/us_service c 240 0

Example in C:
```C
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int timer_file, result;
    unsigned int distance = 0;

    timer_file = open("/dev/us_service", O_RDWR);
    if (timer_file < 0)
    {
        fputs("open() failed, aborting...\n", stderr);
        return 1;
    }

    write(timer_file, &distance, sizeof(distance));
    usleep(100000);
    result = read(timer_file, &distance, sizeof(distance));
        
    if (result != 4)
    {
        fputs("reading error, aborting...\n", stderr);
        close(timer_file);
        return 1;
    }
    printf("Distance is in cm: %d\n", distance / 58000);
        
    close(timer_file);

    return 0;
}
```

Example in Qt:
```C++
  #include <QDebug>
  #include <QFile>
  #include <QDataStream>
 
  int main(int argc, char *argv[])
  {
    QFile deviceFile("/dev/us_service");
    quint32 distance = 0;
    deviceFile.open(QIODevice::ReadWrite | QIODevice::Unbuffered);
    QDataStream deviceStream(&deviceFile);
    deviceStream.setByteOrder(QDataStream::LittleEndian);
    deviceStream << data;
    usleep(100000);
    deviceStream >> data;
    qDebug() << "Distance is: " << data / 58000;
    deviceFile.close();
    return 0;
  } 
```

KNOWN ISSUES AND LIMITATIONS
============================
Sometime the driver retuns not valid, big value. The reason is this driver and the kernel are not realtime so a higher priority task or interrupt could cause delay 
so the interrupt handler will be triggered later and measures longer time. To avoid this problem you can measure more times and calculate avarage. 
To resolve this issue I am working on a realtime kernel driver using Xenomai kernel.

CONTACT
=======
I am happy to read your feedback. My email address is: zoltan.hanisch.dev@gmail.com
