//-----------------------------------------------
// myIOTDataLog.h - data logging object
//-----------------------------------------------
// can be compiled for in-memory usage if WITH_SD=0,
// in which case client must implement chart data handling

#pragma once

#include <myIOTTypes.h>

#if WITH_SD
	#include <SD.h>
#endif



#define DATA_COLS_MAX			20

	// an arbitrary upper limit
#define LOG_COL_TYPE_UINT32			0x00000001	// full unsigned 32 bit
#define LOG_COL_TYPE_UINT16			0x00000002
#define LOG_COL_TYPE_UINT8			0x00000004
#define LOG_COL_TYPE_UINT8x10		0x00000008	// 0..2550 stored as 0..255

#define LOG_COL_TYPE_INT32			0x00000010	// full signed 32 bit
#define LOG_COL_TYPE_INT16			0x00000020	// full signed 32 bit
#define LOG_COL_TYPE_INT8			0x00000040	// full signed 32 bit

#define LOG_COL_TYPE_FLOAT32		0x00000100	// full 32 bit float (unused)

// For the following the user can use the DEGREE_TYPE to show Centigrade vs Farenheit

#define LOG_COL_TYPE_CENTIGRADE32	0x00001000	// full 32 bit float in Centigrade
#define LOG_COL_TYPE_CENTIGRADE_RAW	0x00002000	// int16_t 16 bit Centigrade/128 (raw DS18B20 reading)
#define LOG_COL_TYPE_CENTIGRADE8	0x00004000	// integer Centigrade bias 40; i.e. 40=0C; min=-40C; max=215C,



// the tick_intervals are used to determine the
// min/max and num_ticks in the javascript.

typedef struct {
	const char *name;			// provided by caller
	uint32_t type;				// provided by caller
	float tick_interval;		// provided by caller
} logColumn_t;


typedef uint8_t *logRecord_t;

class myIOTDataLog
{
public:

	myIOTDataLog(
		const char *name,				// a unique name for this dataLog
		int num_cols,					// number of columns and
		logColumn_t *cols);				// column types determine m_rec_size

	#if WITH_SD
		String dataFilename();
			// returns "name.datalog"
		bool addRecord(const logRecord_t rec);
			// Will assign the dt field to the record
			// Writes the record to the SD card.
	#endif


	//----------------------------------------
	// chart support
	//----------------------------------------
	
	String getChartHeader(int period, int with_degrees, const String *series_colors=NULL);

	#if WITH_SD
		String sendChartData(uint32_t secs_or_dt,bool since=false);
	#endif

private:

	const char *m_name;
	int m_num_cols;
	logColumn_t *m_col;

	int m_rec_size;

	void dbg_rec(const logRecord_t rec);
};


//----------------------------------------------------
// backwards fixed size record SD file iterator
//----------------------------------------------------
// static methods available to other clients, like baHistory
// supports single record or "chunked" responses from getSDBackwards()

#if WITH_SD

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




