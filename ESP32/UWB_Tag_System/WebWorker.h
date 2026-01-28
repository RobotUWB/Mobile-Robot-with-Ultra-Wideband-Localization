#ifndef WEB_WORKER_H
#define WEB_WORKER_H

#include <IPAddress.h>

void setupWeb();
void loopWeb();
IPAddress activeIP(); // Exposed so UWB can print the IP

#endif