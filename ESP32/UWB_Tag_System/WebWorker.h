#ifndef WEB_WORKER_H
#define WEB_WORKER_H

#include <IPAddress.h>
#include <WebSocketsServer.h> // [เพิ่มบรรทัดนี้]

void setupWeb();
void loopWeb();
IPAddress activeIP(); // Exposed so UWB can print the IP+

#endif