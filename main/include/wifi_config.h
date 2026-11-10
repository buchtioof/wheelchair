#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
    #define WIFI_SSID     SECRET_WIFI_SSID
    #define WIFI_PASSWORD SECRET_WIFI_PASSWORD
#else
    #define WIFI_SSID     "VOTRE_SSID_ICI"
    #define WIFI_PASSWORD "VOTRE_MOT_DE_PASSE_ICI"
#endif

#endif