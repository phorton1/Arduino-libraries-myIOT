//-----------------------------------------------
// myIOTDataLog.h - data logging object
//-----------------------------------------------
// requires WITH_SD=1;  evaulates to nothing if not


#pragma once

#include <myIOTTypes.h>

#if !WITH_SD
	#message("dataLog requires myIOT define WITH_SD=1")
#else

// The rest of this file

#include <SD.h>

#define LOG_COL_TYPE_UINT32		0x00000001
#define LOG_COL_TYPE_INT32		0x00000002
#define LOG_COL_TYPE_FLOAT		0x00000004

// the tick_intervals are used to determine the
// min/max and num_ticks in the javascript.

typedef struct {
	const char *name;			// provided by caller
	uint32_t type;				// provided by caller
	float tick_interval;		// provided by caller
	// float min;
	// float max;
} logColumn_t;


typedef uint32_t *logRecord_t;
	// an array of uin32's where the 0th the dt in unix format
	// followed by num_cols 32bit values


class myIOTDataLog
{
public:

	myIOTDataLog(
		const char *name,
		int num_cols,
		logColumn_t *cols );
	bool init(int num_mem_recs = 10);
		// Will report an error and return false
		// if there is no SD Card.  The size of the
		// memory queue is determined by the application
		// based on it's likely addRecord:flushToSD() ratio,
		// where 10 is a good default.

	bool addRecord(const logRecord_t rec);
		// Quickly add a record to a in-memory queue to be
		// written to the SD Card later.  The queue can
		// hold a limited number of records before
		// flushToSD() must be called.  Will report an error
		// and return false if there is a buffer overflow.
		// Will report an error an return false if the timestamp
		// at the front of the record is not a valid current time,
		// to prevent writing 1970 log records.
		// Typically called from myIOTDevice's stateMachine task.
	bool flushToSD();
		// Flush the in-memory queue (Slow) to the SD card.
		// Will NOT block calls to addRecord().
		// Will be added to myIOTDevice::loop() at some point.
		// Currently called from myIOTDevice's loop() method
		// BEFORE it calls myIOTDevice::loop() so that sendChartData()
		// works entire from the SD card.

	// myIOTWebServer interface
	// Called from the myIOTDevice's onCustomLink()

	String getChartHeader();
		// returns a String containing the json used to create a chart
	String sendChartData(uint32_t num_recs);
		// sends the chart data to the myiot_web_server and returns
		// RESPONSE_HANDLED. num_recent_recs is determined by the
		// the client and their logging rate, where 0 means "all"


private:

	const char *m_name;
	int m_num_cols;
	int m_rec_size;
	logColumn_t *m_col;

	// in-memory queue

	int m_num_alloc;
	uint8_t *m_rec_buffer;
	volatile int m_head;
	volatile int m_tail;

	logRecord_t mem_rec(int i)
	{
		return (logRecord_t) &m_rec_buffer[i * m_rec_size];
	}
	
	String dataFilename();
	bool writeSDRecs(File &file, const char *what, int at, int num_recs);


};


#endif	// !WITH_SD

