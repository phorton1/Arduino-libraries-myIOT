//--------------------------
// myIOTLog.h
//--------------------------

#pragma once

#include <Arduino.h>
#include <time.h>


#define LOG_LEVEL_NONE          0
#define LOG_LEVEL_USER          1
#define LOG_LEVEL_ERROR         2
#define LOG_LEVEL_WARNING       3
#define LOG_LEVEL_INFO          4
#define LOG_LEVEL_DEBUG         5
#define LOG_LEVEL_VERBOSE       6

// wow, there's some kind of a reference to iot_log_level in the ESP32 libraries.
// I don't get any compile or link warnings or errors, but the value mysteriously
// changes upon a call to WiFi.mode(WIFI_AP_STA) in myIOTServer.cpp!!!

extern uint32_t iot_log_level;       // what is written to log file
extern uint32_t iot_debug_level;     // what is written to screen

#define PROC_ENTRY_AS_METHOD  0

#if PROC_ENTRY_AS_METHOD
    extern void proc_entry();
    extern void proc_leave();
#else
    extern volatile int iot_proc_level;
    #define proc_entry()    iot_proc_level++
    #define proc_leave()    iot_proc_level--
#endif


extern void LOGU(const char *format, ...);
extern void LOGE(const char *format, ...);
extern void LOGW(const char *format, ...);
extern void LOGI(const char *format, ...);
extern void LOGD(const char *format, ...);
extern void LOGV(const char *format, ...);

// time utility

String timeToString(time_t t);
