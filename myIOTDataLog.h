//-----------------------------------------------
// myIOTDataLog.h - data logging object
//-----------------------------------------------
// requires WITH_SD=1;  evaulates to nothing if not

#pragma once

#include <myIOTTypes.h>

#if WITH_SD

// The rest of this file

#include <SD.h>

#define DATA_COLS_MAX			20
	// an arbitrary upper limit
#define LOG_COL_TYPE_UINT32			0x00000001
#define LOG_COL_TYPE_INT32			0x00000002
#define LOG_COL_TYPE_FLOAT			0x00000004
#define LOG_COL_TYPE_TEMPERATURE	0x00000008
	// float in Centigrade. Clients can use the
	// the DEGREE_TYPE to show according to use
	// preferences.

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
	// an array of uint32_t's where the 0th the dt
	// in unix format followed by num_cols 32bit values


class myIOTDataLog
{
public:

	myIOTDataLog(
		const char *name,
		int num_cols,
		logColumn_t *cols,
		int debug_send_data = 1);

	String dataFilename();
		// returns "name.datalog"
	bool addRecord(const logRecord_t rec);
		// Will assign the dt field to the record
		// Writes the record to the SD card.


	//----------------------------------------
	// chart support
	//----------------------------------------
	
	String getChartHTML(int height, int width, int period, int refresh);
		// Returns <div> that contains the chart_div, update button,
		// 		period selection, and refresh interval inputs.
		// Currently height and width are fixed; might go away.
		// Period and refresh are used as defaults for a new browsr load,
		// 		but browsers tend to keep values through reloads
		// Uses urls that must be handled by client by calling getChartHeader()
		// 		and sendChartData() from their onCustomLink() method:
		//		/custom/chart_header/m_name and
		//		/custom/chart_data/m_name?secs=NNNAssumes
		// Assumes the page already includes iotChart.js and dependencies.
	String getChartHeader();
		// returns a String containing the json used to create a chart
	String sendChartData(uint32_t secs);
		// sends the chart data to the myiot_web_server and returns
		// RESPONSE_HANDLED. 0 means all.

private:

	const char *m_name;
	int m_num_cols;
	int m_rec_size;
	logColumn_t *m_col;

	int m_debug_send_data;
		// client is allowed to set m_debug_send_data=0
		// to turn off debugging, or higher numbers to
		// see more
	void dbg_rec(logRecord_t rec);

};


//----------------------------------------------------
// backwards fixed size record SD file iterator
//----------------------------------------------------
// static methods available to other clients, like baHistory
// supports single record or "chunked" responses from getSDBackwards()

	typedef bool (*SDBackardsCB)(uint32_t client_data, uint8_t *rec);
		// return true if iteration should proceed, false otherwise

    typedef struct {

		// members setup by client before calling startSDBackwards()

        bool chunked;				// return buffers and *num_recs possibly > 1
        uint32_t client_data;		// i.e. "this" or the cutoff_date used by CB method
        const char *filename;		// SD card file to open and iterate
        uint32_t rec_size;			// size of a single record in the file
        SDBackardsCB record_fxn;	// client callback funtion
        uint8_t *buffer;            // the buffer
        uint32_t buf_size;			// MUST be an even multiple of rec_size
        int dbg_level;           	// 0..4

		// membets maintained through an iteration

        File file;
        bool done;					// iteration has finished
        int num_buf_recs;			// nuumber of records in the current buffer
        int read_pos;         		// starts as file.size()
        int buf_idx;        		// which record in the buffer are we at

    } SDBackwards_t;


    extern bool startSDBackwards(SDBackwards_t *iter);
		// reports returns false on error
		// returns true if no errors
		// iter->done set to true if missing or empty file
		// file will possibly be open if returns true
    extern uint8_t *getSDBackwards(SDBackwards_t *iter, int *num_recs);
        // returns record(s), but only as many as the client
        // callback has verified that it wanted
		// file will be closed if returns NULL


#endif	// WITH_SD



