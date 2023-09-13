//--------------------------
// myIOTLog.cpp
//--------------------------

#include "myIOTLog.h"
#include "myIOTDevice.h" // for WITH_TELNET type and hasSD() method
#include <stdio.h>

#ifndef LOG_TIMESTAMP_MS
    #define LOG_TIMESTAMP_MS    1
#endif


#if WITH_SD
	#include <SD.h>
	#include <FS.h>
#endif

#if WITH_TELNET
    #include "myIOTSerial.h"
    #include <ESPTelnet.h>
#endif

#define MSG_COLOR_WHITE         "\033[97m"       // bright white
#define MSG_COLOR_GREEN         "\033[92m"       // bright green
#define MSG_COLOR_YELLOW        "\033[93m"       // yellow
#define MSG_COLOR_RED           "\033[91m"       // red
#define MSG_COLOR_CYAN          "\033[96m"       // bright cyan
#define MSG_COLOR_MAGENTA       "\033[95m"       // magenta
#define MSG_COLOR_LIGHT_GREY    "\033[37m"       // light_grey


uint32_t iot_log_level = LOG_LEVEL_INFO;
uint32_t iot_debug_level = LOG_LEVEL_DEBUG;
volatile int iot_proc_level = 0;
#if WITH_SD
	static bool logfile_error = 0;
#endif


#if PROC_ENTRY_AS_METHOD
    void proc_entry()
    {
        iot_proc_level++;
    }

    void proc_leave()
    {
        iot_proc_level--;
        if (iot_proc_level < 0)
        {
            Serial.print("=================> iot_proc_level<0");
			Serial.print("\r\n");
            iot_proc_level = 0;
        }
    }
#endif



static int mycat(char *buf, const char *str, char **end)
{
	int len = strlen(str);
	strcpy(buf,str);
	*end = &buf[len];
	return len;
}


static void log_output(bool with_indent, int level, const char *format, va_list *var)
{
    if (iot_debug_level < level &&
		(!myIOTDevice::hasSD() || iot_log_level < level))
		return;

	bool log_colors = 0;
	bool log_date = 0;
	bool log_time = 0;
	bool log_mem = 0;

	my_iot_device->getLogBooleans(&log_colors, &log_date, &log_time, &log_mem);

	#define MAX_BUFFER  255

	int len = 0;
	char display_buf[MAX_BUFFER+1];		// buffer including color sequence
	char *log_buf = display_buf;		// after color sequence
    char *end = display_buf;			// next position to write at


	if (log_colors)
	{
		const char *color = MSG_COLOR_LIGHT_GREY;    // default == LOG_LEVEL_USER
		switch (level)
		{
			case LOG_LEVEL_ERROR   : color = MSG_COLOR_RED; break;
			case LOG_LEVEL_WARNING : color = MSG_COLOR_YELLOW; break;
			case LOG_LEVEL_INFO    : color = MSG_COLOR_CYAN; break;
			case LOG_LEVEL_DEBUG   : color = MSG_COLOR_GREEN; break;
			case LOG_LEVEL_VERBOSE : color = MSG_COLOR_MAGENTA; break;
		}
		mycat(display_buf,color,&log_buf);
	}

	if (log_date || log_time)
	{
		struct timeval tv_now;
		gettimeofday(&tv_now, NULL);
		String tm = timeToString(tv_now.tv_sec);	// time(NULL));

		#if LOG_TIMESTAMP_MS
			if (log_time)
			{
				static char ibuf[12];
				int ms = tv_now.tv_usec / 1000L;
				sprintf(ibuf,".%03d",ms);
				tm += ibuf;
			}
		#endif

		tm += " ";

		if (!log_date)
			tm = tm.substring(12);
		else if (!log_time)
			tm = tm.substring(0,12);
		mycat(log_buf,tm.c_str(),&end);
	}

	if (log_mem)
	{
		uint32_t mem_free = xPortGetFreeHeapSize() / 1024;
		uint32_t mem_min = xPortGetMinimumEverFreeHeapSize() / 1024;
		len = mycat(end,String(mem_free).c_str(),&end);
		len += mycat(end,":",&end);
		len += mycat(end,String(mem_min).c_str(),&end);
		while (len < 10)
		{
			strcpy(end++," ");
			len++;
		}
	}

	// Serial.println(display_buf);
	// return;

	// indent will never be more than 32 bytes
	// only DEBUG messages are done by proc level
	// but INFO messages are statically indented 1
	// for clarity in logfile

	if (with_indent || level == LOG_LEVEL_INFO)
	{
		int use_level = level == LOG_LEVEL_INFO ? 1 : iot_proc_level;
		while (use_level > 8)
			use_level -= 8;
        for (int i=0; i<use_level; i++)
			mycat(end,"    ",&end);
	}

	// add the final formatted string

	len = strlen(display_buf);
	int avail = MAX_BUFFER - len - 2 - 5 - 1;
		// 2 for \r\n,  5 for trailing color string, 1 for 0 terminator
	vsnprintf(end,avail,format,*var);
	len = strlen(display_buf);
	end = &display_buf[len];
	strcpy(end,"\r\n");
	end++;
	end++;

	if (iot_debug_level >= level)
	{
		if (log_colors)
			strcpy(end,MSG_COLOR_LIGHT_GREY);
		Serial.print(display_buf);
		#if WITH_TELNET
			if (myIOTSerial::telnetConnected())
				myIOTSerial::telnet.print(display_buf);
		#endif

	}

	// output non verbose messages to logfile

    #if WITH_SD
		if (!logfile_error &&
			level < LOG_LEVEL_VERBOSE &&
			iot_log_level >= level &&
			myIOTDevice::hasSD())
		{
			String logfile_name = "/";
			logfile_name += my_iot_device->getDeviceType();
			logfile_name += "_log.txt";
			File file = SD.open(logfile_name, FILE_APPEND);
			if (!file)
			{
				Serial.print("WARNING - COULD NOT OPEN LOGFILE ");
				Serial.print(logfile_name);
				Serial.print("\r\n");
				logfile_error = 1;
			}
			else
			{
				if (log_colors)
					*end = 0;	// remove the terminating color string
				file.print(log_buf);
				file.close();
			}
		}
	#endif

}


void LOGU(const char *format, ...)
{
	va_list var;
	va_start(var, format);
    log_output(false,LOG_LEVEL_USER,format,&var);
	va_end(var);
}
void LOGE(const char *format, ...)
{
	va_list var;
	va_start(var, format);
	log_output(false,LOG_LEVEL_ERROR,format,&var);
	va_end(var);
}
void LOGW(const char *format, ...)
{
	va_list var;
	va_start(var, format);
	log_output(false,LOG_LEVEL_WARNING,format,&var);
	va_end(var);
}
void LOGI(const char *format, ...)
{
	va_list var;
	va_start(var, format);
	log_output(false,LOG_LEVEL_INFO,format,&var);
	va_end(var);
}
void LOGD(const char *format, ...)
{
	va_list var;
	va_start(var, format);
	log_output(true,LOG_LEVEL_DEBUG,format,&var);
	va_end(var);
}
void LOGV(const char *format, ...)
{
	va_list var;
	va_start(var, format);
	log_output(true,LOG_LEVEL_VERBOSE,format,&var);
	va_end(var);
}

//------------------------------------
// time
//-------------------------------------


String timeToString(time_t t)
{
    char buf[40];
    struct tm *ts = localtime(&t);
	// prefer space between date and time, so I put in two
    sprintf(buf,"%04d-%02d-%02d  %02d:%02d:%02d",
        ts->tm_year + 1900,
        ts->tm_mon + 1,
        ts->tm_mday,
        ts->tm_hour,
        ts->tm_min,
        ts->tm_sec);
    return String(buf);
}
