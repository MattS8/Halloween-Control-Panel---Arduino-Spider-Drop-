#ifndef SPIDER_DROP_H
#define SPIDER_DROP_H

#include "firebaseFunctions.h"
#include <FS.h>

#ifndef LOCAL_FILE
#define LOCAL_FILE
const char* SpiderDropFilePath = "SpiderDrop.dat";
#endif

#define MAX_HANG_TIME 15000

#define CONST_DELAY 1
#define MOTOR_DELAY_INTERVAL 250

#endif