// a wifi event handler for debugging


#include <WiFi.h>
#include "myIOTLog.h"

static WiFiEvent_t last_event;

int ap_connection_count = 0;


static const char *wifiEventName(WiFiEvent_t event)
{
	switch (event)
	{
		case SYSTEM_EVENT_WIFI_READY	        : return "WIFI_READY";
		case SYSTEM_EVENT_SCAN_DONE	            : return "SCAN_DONE";
		case SYSTEM_EVENT_STA_START	            : return "STA_START";
		case SYSTEM_EVENT_STA_STOP	            : return "STA_STOP";
		case SYSTEM_EVENT_STA_CONNECTED	        : return "STA_CONNECTED";
		case SYSTEM_EVENT_STA_DISCONNECTED	    : return "STA_DISCONNECTED";
		case SYSTEM_EVENT_STA_AUTHMODE_CHANGE	: return "STA_AUTHMODE_CHANGE";
		case SYSTEM_EVENT_STA_GOT_IP	        : return "STA_GOT_IP";
		case SYSTEM_EVENT_STA_LOST_IP	        : return "STA_LOST_IP";
		case SYSTEM_EVENT_STA_WPS_ER_SUCCESS	: return "STA_WPS_ER_SUCCESS";
		case SYSTEM_EVENT_STA_WPS_ER_FAILED	    : return "STA_WPS_ER_FAILED";
		case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT	: return "STA_WPS_ER_TIMEOUT";
		case SYSTEM_EVENT_STA_WPS_ER_PIN	    : return "STA_WPS_ER_PIN";
		case SYSTEM_EVENT_AP_START	            : return "AP_START";
		case SYSTEM_EVENT_AP_STOP	            : return "AP_STOP";
		case SYSTEM_EVENT_AP_STACONNECTED	    : return "AP_STACONNECTED";
		case SYSTEM_EVENT_AP_STADISCONNECTED	: return "AP_STADISCONNECTED";
		case SYSTEM_EVENT_AP_STAIPASSIGNED	    : return "AP_STAIPASSIGNED";
		case SYSTEM_EVENT_AP_PROBEREQRECVED	    : return "AP_PROBEREQRECVED";
		case SYSTEM_EVENT_GOT_IP6	            : return "GOT_IP6";
		case SYSTEM_EVENT_ETH_START	            : return "ETH_START";
		case SYSTEM_EVENT_ETH_STOP	            : return "ETH_STOP";
		case SYSTEM_EVENT_ETH_CONNECTED         : return "ETH_CONNECTED	ESP32";
		case SYSTEM_EVENT_ETH_DISCONNECTED	    : return "ETH_DISCONNECTED";
		case SYSTEM_EVENT_ETH_GOT_IP	        : return "ETH_GOT_IP";

		// added to avoid compiler warnings

		case SYSTEM_EVENT_STA_WPS_ER_PBC_OVERLAP : return "STA_WPS_ER_PBC_OVERLAP";
		case SYSTEM_EVENT_MAX                    : return "EVENT_MAX";

	}
	return "UNKNOWN_WIFI_EVENT";
}


static void onWiFiEvent(WiFiEvent_t event)
{
	// only show new event types
	// to prevent showing hundreds of STA_DISCONNECTED
	// when WIFI goes down.

	if (last_event != event)
	{
		LOGD("WIFI_EVENT(%d) %s",event,wifiEventName(event));
		last_event = event;
	}

	// keep track of AP_STA connections

	if (event == SYSTEM_EVENT_AP_STACONNECTED)
	{
		ap_connection_count ++;
		LOGI("ap_connection_count=%d",ap_connection_count);
	}
	else if (event == SYSTEM_EVENT_AP_STADISCONNECTED)
	{
		ap_connection_count --;
		LOGI("ap_connection_count=%d",ap_connection_count);
	}
}


void initWifiEventHandler()
{
	WiFi.onEvent(onWiFiEvent);
}
