/*
 * wifi.h
 *
 */

#ifndef FIND_PUBLIC_IP_USER_WIFI_H_
#define FIND_PUBLIC_IP_USER_WIFI_H_
#include "os_type.h"
typedef void (*WifiCallback)(uint8_t);
void WIFI_Connect(WifiCallback cb);


#endif /* FIND_PUBLIC_IP_USER_WIFI_H_ */
