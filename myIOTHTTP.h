//--------------------------
// myIOTHTTP.h
//--------------------------

#pragma once

#include "myIOTTypes.h"
#include <FS.h>


class myIOTHTTP
{
    public:

        myIOTHTTP();
        ~myIOTHTTP();

        void setup();
        void loop();

        void onConnectAP();
        void onDisconnectAP();
        void onConnectStation();
            // The HTTP Server is responsible for starting and stopping
            // the underlying SSDP Server (in station mode) and DNS server
            // (in AP mode) via calls to onConnect and onDisconnect AP/Station


    protected:

        static String getContentType(String filename);

        static void handle_SSDP();
        static void handle_captive();
        static void handle_request();
        static void handle_SPIFFS();
        #if WITH_SD
            static void handle_SDCARD();
        #endif
        static void handle_fileUpload(bool sdcard, fs::FS the_fs);
        static void handle_OTA();

        static void debugRequest(const char *what);
        static String doReplacements(const char *page);

        // Upload handlers

        static int m_upload_pct;
        static int m_num_upload_files;
        static File m_upload_file;
        static bool m_in_upload;
        static uint32_t m_upload_total;
        static uint32_t m_upload_bytes;
        static String m_upload_filename;
        static String m_upload_request_id;

        static void sendUploadProgress(int every, bool finished=false, bool error=false);

};
