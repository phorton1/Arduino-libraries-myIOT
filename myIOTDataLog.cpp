//-----------------------------------------------
// myIOTDataLog.cpp - data logging object
//-----------------------------------------------
// The data file contains LOCAL timestamps becase
// - time(NULL) returns local time
// - logFile time2Str and javascript convert them correctly
// - I can't figure how to easily get gmt from ESP21 and
//   would have to fix the above!
//
// As far as I can tell, there is actually only one ABSOLUTE_SCALE for a plot,
// determined by the 1st y-axis definition.  Then, weirdly, the other axis
// scale LABELS can be changed, but their values are always charted against
// the ABSOLUTE_SCALE, so the data must be normalized.
//
// This results in the javascript normalize all subsquent column data
// to the 0th column. In other words, if col0 is temp1, and temp1 goes from
// -32 to -20, then all the other columns will be scaled from -32 to -20.
//
// Normalization and magic tricks to get jqPlot to display
// decent scales are currently all performed in the javascript.
// Currently the JS picks scales based on the intervals so
// that all ticks line up on the chart, but this wastes visual
// space. Other alternatives include:
//
// - automatically creating tick marks that are not lined up,
//   (but grid lines only on the 0th y axis) that maximize the
//   displayed accuracy
// - having the app explicitly determine the scales with
//   min and max members per column and API on this object.
//
// Other options might include
//
// - allowing the app and/or user to specify if the zooming
//   javascript should be included.
// - allowing the user to specify, in the HTML, which cols
//   they want to initially display (and remembering them
//	 on refreshes).
//
// Current bug is that zoom and displayed cols
// is lost during refresh.


#include "myIOTDataLog.h"
#include "myIOTDevice.h"
#include "myIOTLog.h"
#include "myIOTWebServer.h"
#include "myIotTempSensor.h"


#define DEBUG_ADD			0
#define DEBUG_SEND_DATA		1
#define DEBUG_ITER			0


#define ILLEGAL_DT		1726764664		// 2024-09-19  11:51:04a Panama Local Time


//------------------------------------
// implementation
//------------------------------------

static int colSize(uint32_t typ)
{
	if (typ == LOG_COL_TYPE_UINT8 ||
		typ == LOG_COL_TYPE_UINT8x10 ||
		typ == LOG_COL_TYPE_INT8 ||
		typ == LOG_COL_TYPE_CENTIGRADE8)
		return 1;
	if (typ == LOG_COL_TYPE_UINT16 ||
		typ == LOG_COL_TYPE_INT16 ||
		typ == LOG_COL_TYPE_CENTIGRADE_RAW ||
		typ == LOG_COL_TYPE_INT16_10)
		return 2;
	// type == LOG_COL_TYPE_UINT32
	// type == LOG_COL_TYPE_INT32
	// type == LOG_COL_TYPE_FLOAT32
	// type == LOG_COL_TYPE_CENTIGRADE32
	return 4;
}


myIOTDataLog::myIOTDataLog(
		const char *name,
		int num_cols,
		logColumn_t *cols) :
	m_name(name),
	m_num_cols(num_cols),
	m_col(cols)
{
	m_rec_size = 4;		// 4 for the dt
	for (int i=0; i<num_cols; i++)
	{
		m_rec_size += colSize(cols[i].type);
	}
}




void myIOTDataLog::dbg_rec(const logRecord_t rec)
{
	String tm = timeToString(*((uint32_t *)rec));
	LOGD("REC(%s)",tm.c_str());

	int offset = 4;	// skip the dt
	for (int i=0; i<m_num_cols; i++)
	{
		uint32_t col_type = m_col[i].type;

		if (col_type == LOG_COL_TYPE_UINT16)
		{
			uint16_t val = *((uint16_t*)&rec[offset]);
			offset += 2;
			LOGD("   %-15s = %u",m_col[i].name,val);
		}
		else if (col_type == LOG_COL_TYPE_UINT8)
		{
			uint8_t val = *((uint8_t*)&rec[offset]);
			offset += 1;
			LOGD("   %-15s = %u",m_col[i].name,val);
		}
		else if (col_type == LOG_COL_TYPE_UINT8x10)
		{
			uint8_t val = *((uint8_t*)&rec[offset]);
			offset += 1;
			LOGD("   %-15s = %u",m_col[i].name,val*10);
		}
		else if (col_type == LOG_COL_TYPE_INT32)
		{
			int32_t val = *((int32_t*)&rec[offset]);
			offset += 4;
			LOGD("   %-15s = %d",m_col[i].name,val);
		}
		else if (col_type == LOG_COL_TYPE_INT16)
		{
			int16_t val = *((int16_t*)&rec[offset]);
			offset += 2;
			LOGD("   %-15s = %d",m_col[i].name,val);
		}
		else if (col_type == LOG_COL_TYPE_INT8)
		{
			int8_t val = *((int8_t*)&rec[offset]);
			offset += 2;
			LOGD("   %-15s = %d",m_col[i].name,val);
		}
		else if (col_type == LOG_COL_TYPE_FLOAT32 ||
				 col_type == LOG_COL_TYPE_CENTIGRADE32)
		{
			float val = *((float*)&rec[offset]);
			offset += 4;
			LOGD("   %-15s = %0.3f",m_col[i].name,val);
		}
		else if (col_type == LOG_COL_TYPE_CENTIGRADE_RAW)
		{
			int16_t raw = *((uint16_t*)(&rec[offset]));
			offset += 2;
			float val = myIOTTempSensor::rawToDegreesC(raw);
			LOGD("   %-15s = %0.1f", m_col[i].name,val);
 		}
		else if (col_type == LOG_COL_TYPE_CENTIGRADE8)
		{
			uint8_t val = *((uint8_t*)&rec[offset]);
			offset += 1;
			LOGD("   %-15s = %d",m_col[i].name,val-40);
		}
		else if (col_type == LOG_COL_TYPE_INT16_10)
		{
			float val = *((int16_t*)&rec[offset]);
			val /= 10;
			offset += 2;
			LOGD("   %-15s = %0.1f",m_col[i].name,val-40);
		}
		else	// UINT32_t
		{
			uint32_t val = *((uint32_t*)&rec[offset]);
			offset += 4;
			LOGD("   %-15s = %u",m_col[i].name,val);
		}
	}
}


//---------------------------------------------------
// addRecord()
//---------------------------------------------------

#if WITH_SD

	String myIOTDataLog::dataFilename()
	{
		String filename = "/";
		filename += m_name;
		filename += ".datalog";
		return filename;
	}

	bool myIOTDataLog::addRecord(const logRecord_t rec)
	{
		uint32_t tm = time(NULL);
		if (tm < ILLEGAL_DT)
		{
			String stime = timeToString(tm);
			LOGE("attempt to call myIOTDataLog::addRecord(%d) at bad time(%s)",tm,stime.c_str());
			return false;
		}
		*((uint32_t *)rec) = tm;

		if (myIOTDevice::m_suppress_log)
		{
			LOGD("myIOTDataLog::addRecord() suppressed during maintenance");
			return true;
		}

		String filename = dataFilename();
		File file = SD.open(filename, FILE_APPEND);
		if (!file)
		{
			LOGE("myIOTDataLog::addRecord() could not open %s for appending",filename.c_str());
			return false;
		}

		uint32_t size = file.size();
	#if DEBUG_ADD
		int num_recs = size / m_rec_size;
		LOGD("myIOTDataLog::addRecord() rec_size(%d) rec_num(%d)=file_size(%d) dt=%s",
			 m_rec_size,
			 num_recs + 1,
			 size,
			 timeToString(tm).c_str());
		#if DEBUG_ADD > 1
			dbg_rec(rec);
		#endif
	#endif

		bool retval = true;
		int bytes = file.write((uint8_t *)rec,m_rec_size);
		if (bytes != m_rec_size)
		{
			LOGE("myIOTDataLog::addRecord() Error appending(%d/%d) bytes at %d in %s",bytes,m_rec_size,size,filename.c_str());
			retval = false;
		}

		file.close();
		return retval;
	}
#endif 	// WITH_SD



//-----------------------------------------
// getChartHeader()
//-----------------------------------------
// returns json chart_header string as used by iotChart.js

void addJsonVal(String &rslt, const char *field, String val, bool quoted, bool comma, bool cr)
{
	rslt += "\"";
	rslt += field;
	rslt += "\":";
	if (quoted) rslt += "\"";
	rslt += val;
	if (quoted) rslt += "\"";
	if (comma) rslt += ",";
	if (cr) rslt += "\n";
}


String myIOTDataLog::getChartHeader(int period, int with_degrees, const String *series_colors /*=NULL*/)
{
	String rslt = "{\n";

	addJsonVal(rslt,"name",m_name,true,true,true);
	addJsonVal(rslt,"rec_size",String(m_rec_size),false,true,true);
	addJsonVal(rslt,"num_cols",String(m_num_cols),false,true,true);
	addJsonVal(rslt,"default_period",String(period),false,true,true);
	#if WITH_SD
	addJsonVal(rslt,"has_file","true",false,true,true);
	#else
	addJsonVal(rslt,"has_file","false",false,true,true);
	#endif
	addJsonVal(rslt,"with_degrees",String(with_degrees),false,true,true);
	if (with_degrees)
	{
		uint32_t degree_type = my_iot_device->getEnum(ID_DEGREE_TYPE);
		addJsonVal(rslt,"degree_type",String(degree_type),false,true,true);
	}

	if (series_colors)
		addJsonVal(rslt,"series_colors",*series_colors,false,true,true);
	
	rslt += "\"col\":[\n";

	for (int i=0; i<m_num_cols; i++)
	{
		if (i) rslt += ",";
		rslt += "{";

		const logColumn_t *col = &m_col[i];
		const char *str =
			col->type == LOG_COL_TYPE_UINT16			? "uint16_t" :
			col->type == LOG_COL_TYPE_UINT8				? "uint8_t" :
			col->type == LOG_COL_TYPE_UINT8x10			? "uint8x10_t" :
			col->type == LOG_COL_TYPE_INT32				? "int32_t" :
			col->type == LOG_COL_TYPE_INT16				? "int16_t" :
			col->type == LOG_COL_TYPE_INT8				? "int8_t" :
			col->type == LOG_COL_TYPE_FLOAT32			? "float32_t" :
			col->type == LOG_COL_TYPE_CENTIGRADE32		? "centigrade32_t" :
			col->type == LOG_COL_TYPE_CENTIGRADE_RAW	? "centigradeRaw_t" :
			col->type == LOG_COL_TYPE_CENTIGRADE8		? "centigrade8_t" :
			col->type == LOG_COL_TYPE_INT16_10			? "int16div10_t" :
			"uint32_t";
		addJsonVal(rslt,"name",col->name,							true,true,false);
		addJsonVal(rslt,"type",str,									true,true,false);
		addJsonVal(rslt,"tick_interval",String(col->tick_interval),	false,false,true);
		rslt += "}\n";
	}
	rslt += "]\n";
	rslt += "}";

	#if 0
		Serial.print("myIOTDataLog::getChartHeader()=");
		Serial.println(rslt.c_str());
	#endif

	return rslt;
}




//================================================
// backwards SD file iterator
//================================================

#if WITH_SD

	bool startSDBackwards(SDBackwards_t *iter)
	{
		iter->done = true;
		iter->read_pos = 0;
		iter->buf_idx = -1;

		#if DEBUG_ITER
			LOGD("startSDBackwards(%s) rec_size(%d) buf_size(%d)",
				 iter->filename, iter->rec_size, iter->buf_size);
		#endif
		
		int num_file_recs = 0;
		if (!SD.exists(iter->filename))
		{
			LOGW("missing %s",iter->filename);
		}
		else
		{
			iter->file = SD.open(iter->filename, FILE_READ);
			if (!iter->file)
			{
				LOGE("Could not open %s for reading",iter->filename);
				return false;
			}
			uint32_t size = iter->file.size();
			if (!size)
			{
				LOGW("empty %s",iter->filename);
			}
			else if (size % iter->rec_size)
			{
				LOGE("%s size(%d) is not a multiple of rec_size(%d)",iter->filename,size,iter->rec_size);
				iter->file.close();
				return false;
			}
			num_file_recs = size / iter->rec_size;
			#if DEBUG_ITER
				LOGD("file size=%d  num_file_recs=%d",size,num_file_recs);
			#endif
		}

		if (num_file_recs > 0)
		{
			iter->done = false;
			iter->read_pos = iter->file.size();	// Initial read position
			#if DEBUG_ITER
				LOGD("initial read_pos=%d",iter->read_pos,num_file_recs);
			#endif
		}

		// we are setup for the fist iteration

		#if DEBUG_ITER
			LOGD("startSDBackwards(%s) returning done(%d)",iter->filename,iter->done);
		#endif
		
		return true;

	}   // startSDBackwards()



	uint8_t *getSDBackwards(SDBackwards_t *iter, int *num_recs)
		// returns record(s), but only as many as the client
		// callback has verified that it wanted
	{
		*num_recs = 0;

		if (iter->done)
			return NULL;

		#if DEBUG_ITER > 1
			LOGD("getSDBackwards() idx(%d) read_pos(%d)",iter->buf_idx,iter->read_pos);
		#endif

		if (iter->buf_idx < 0)  // buffer exhausted
		{
			if (iter->read_pos == 0)
			{
				#if DEBUG_ITER
					LOGD("           END OF FILE");
				#endif
				iter->done = 1;
				return NULL;
			}

			int read_bytes = iter->buf_size;
			if (iter->read_pos < read_bytes)
			{
				read_bytes = iter->read_pos;
				iter->read_pos = 0;
			}
			else
				iter->read_pos -= read_bytes;

			#if DEBUG_ITER > 1
				LOGD("seeking to file_offset(%d)",iter->read_pos);
			#endif

			if (!iter->file.seek(iter->read_pos))
			{
				LOGE("Could not seek to byte %d", iter->read_pos);
				iter->file.close();
				return NULL;
			}

			#if DEBUG_ITER > 1
				LOGD("    reading %d bytes at file_offset(%d)",read_bytes,iter->read_pos);
			#endif

			uint32_t bytes = iter->file.read(iter->buffer, read_bytes);
			if (bytes != read_bytes)
			{
				LOGE("Error reading (%d/%d) at read_pos=%d", bytes,read_bytes,iter->read_pos);
				iter->file.close();
				return NULL;
			}

			iter->num_buf_recs = read_bytes / iter->rec_size;
			iter->buf_idx = iter->num_buf_recs - 1;

			#if DEBUG_ITER > 1
				LOGD("    buf_recs=%d idx=%d",iter->num_buf_recs,iter->buf_idx);
			#endif
			
		}   // new buffer succesfully read

		if (iter->chunked)
		{
			uint8_t *retval = NULL;
			bool running = true;
			while (running && iter->buf_idx >= 0)
			{
				uint8_t *rec = iter->buffer + (iter->rec_size * iter->buf_idx--);
				SDIterState_t state = iter->record_fxn(iter->client_data, rec);
				if (state == ITER_INCLUDE)
				{
					(*num_recs)++;
					retval = rec;
				}
				else if (state == ITER_STOP)
				{
					running = false;
					#if DEBUG_ITER
						LOGD("iteration ended by STOP at buf_idx(%d)",iter->buf_idx);
					#endif
					iter->done = 1;
					iter->file.close();
				}
				else	// ITER_SKIP (tombstone)
				{
					if (*num_recs > 0)
						running = false;	// end current batch at tombstone to preserve contiguity
					// else: no batch started yet, skip and continue
				}
			}

			#if DEBUG_ITER
				LOGD("getSDBackwards(chunked) returning %d records at index %d",*num_recs,iter->buf_idx + 1);
			#endif

			return retval;
		}
		else	// non-chunked: return one record per call, skip tombstones
		{
			while (iter->buf_idx >= 0)
			{
				uint8_t *rec = iter->buffer + (iter->rec_size * iter->buf_idx--);
				SDIterState_t state = iter->record_fxn(iter->client_data, rec);
				if (state == ITER_INCLUDE)
				{
					*num_recs = 1;
					return rec;
				}
				if (state == ITER_STOP)
				{
					iter->done = 1;
					iter->file.close();
					return NULL;
				}
				// ITER_SKIP: continue loop
			}
			// Buffer exhausted without INCLUDE or STOP - caller will load next buffer
			return NULL;
		}
	}


	//-----------------------------------------
	// sendChartData()
	//-----------------------------------------
	// Usually single instance, debugging of static method
	// uses global for multiple instances

	SDIterState_t chartDataCondition(uint32_t cutoff, uint8_t *rec)
	{
		uint32_t ts = *((uint32_t *) rec);
		if (ts == 0)
			return ITER_SKIP;		// tombstoned record
		if (ts >= cutoff)
			return ITER_INCLUDE;

		#if DEBUG_SEND_DATA>2
		{
			String dt1 = timeToString(ts);
			String dt2 = timeToString(cutoff);
			LOGD("    chartDataCondition(STOP) at %s < %s",dt1.c_str(),dt2.c_str());
		}
		#endif
		return ITER_STOP;
	}


	String myIOTDataLog::sendChartData(uint32_t secs_or_dt,bool since/*=false*/)
	{
		#define BASE_BUF_SIZE	1024

		String filename = dataFilename();
		uint32_t cutoff = secs_or_dt ?
			since ? secs_or_dt : time(NULL) - secs_or_dt : 0;

		// pick bufsize > 512 that will hold even number of records

		int buf_size = ((BASE_BUF_SIZE + m_rec_size-1) / m_rec_size) * m_rec_size;
		uint8_t stack_buffer[buf_size];

		#if DEBUG_SEND_DATA
		{
			String dbg_tm = timeToString(cutoff);
			LOGI("sendChartData rec_size(%d) secs/dt(%d) since_bool(%d) since(%s) from %s",
				 m_rec_size,
				 secs_or_dt,
				 since,
				 secs_or_dt?dbg_tm.c_str():"forever",
				 filename.c_str());
			LOGD("    buf_size(%d) cutoff=(%d)",buf_size,cutoff);
		}
		#endif

		// initialize iterator struct

		SDBackwards_t iter;
		iter.chunked        = 1;
		iter.client_data    = cutoff;
		iter.filename       = filename.c_str();
		iter.rec_size       = m_rec_size;
		iter.record_fxn     = chartDataCondition;
		iter.buffer         = stack_buffer;                 // an even multiple of rec_size
		iter.buf_size       = buf_size;

		if (!startSDBackwards(&iter))
			return "";

		if (!myiot_web_server->startBinaryResponse("application/octet-stream", CONTENT_LENGTH_UNKNOWN))
			return "";

		int sent = 0;
		int num_file_recs = iter.file ? iter.file.size() / m_rec_size : 0;

		int num_recs;
		uint8_t *rec_buf = getSDBackwards(&iter,&num_recs);
		while (num_recs)
		{
			sent += num_recs;

			#if DEBUG_SEND_DATA > 1
				int offset = 0;
				for (int i=0; i<num_recs; i++)
				{
					dbg_rec(&rec_buf[i*m_rec_size]);
				}
			#endif

			if (!myiot_web_server->writeBinaryData((const char*)rec_buf, num_recs * m_rec_size))
			{
				if (iter.file)
					iter.file.close();
				return "";
			}

			rec_buf = getSDBackwards(&iter,&num_recs);
		}

		#if DEBUG_SEND_DATA
			LOGD("    sendChartData() sent %d/%d records",sent,num_file_recs);
		#endif

		return RESPONSE_HANDLED;
	}


	//-----------------------------------------
	// scanFile()
	//-----------------------------------------

	String myIOTDataLog::scanFile()
	{
		String filename = dataFilename();
		File file = SD.open(filename.c_str(), FILE_READ);
		if (!file)
		{
			LOGE("scanFile() could not open %s", filename.c_str());
			return "";
		}

		uint32_t size = file.size();
		if (!size)
		{
			file.close();
			return "{\"num_recs\":0,\"first_dt\":0,\"last_dt\":0,\"tombstones\":0,\"spikes\":[],\"needs_compact\":false}";
		}

		// Chunked forward read — one SD read per chunk instead of one seek
		// per record.  Only the backward spike-walk uses individual seeks.
		#define SCAN_BASE_BUF  1024
		int      buf_size = ((SCAN_BASE_BUF + m_rec_size - 1) / m_rec_size) * m_rec_size;
		uint8_t  stack_buffer[buf_size];

		uint32_t num_recs    = size / m_rec_size;
		uint32_t tombstones  = 0;
		uint32_t first_dt    = 0;
		uint32_t last_dt     = 0;
		uint32_t prev_dt     = 0;
		String   spikes_json = "[";
		bool     first_spike = true;

		// How far back to walk when characterising a spike (individual seeks).
		#define MAX_SPIKE_LOOK 100

		uint32_t file_offset = 0;
		uint32_t rec_idx     = 0;

		while (file_offset < size)
		{
			int to_read = (int)min((uint32_t)buf_size, size - file_offset);
			to_read = (to_read / m_rec_size) * m_rec_size;
			if (to_read == 0) break;

			if (!file.seek(file_offset)) break;
			int got = file.read(stack_buffer, to_read);
			if (got <= 0) break;

			int recs_in_chunk = got / m_rec_size;
			for (int r = 0; r < recs_in_chunk; r++, rec_idx++)
			{
				uint32_t dt;
				memcpy(&dt, stack_buffer + r * m_rec_size, 4);

				if (dt == 0) { tombstones++; continue; }
				if (!first_dt) first_dt = dt;
				last_dt = dt;

				if (prev_dt && dt < prev_dt)
				{
					// Drop-back: walk backward (individual seeks) to find spike extent.
					uint32_t resume_dt       = dt;
					uint32_t spike_end_idx   = rec_idx - 1;
					uint32_t spike_start_idx = rec_idx - 1;
					uint32_t spike_first_dt  = prev_dt;
					uint32_t spike_last_dt   = prev_dt;
					uint32_t before_spike_dt = 0;

					int32_t j    = (int32_t)rec_idx - 1;
					int     look = 0;
					while (j >= 0 && look < MAX_SPIKE_LOOK)
					{
						file.seek((uint32_t)j * m_rec_size);
						uint32_t jdt;
						if (file.read((uint8_t *)&jdt, 4) != 4) break;
						if (jdt == 0) { j--; look++; continue; }
						if (jdt <= resume_dt)
						{
							before_spike_dt = jdt;
							spike_start_idx = (uint32_t)(j + 1);
							break;
						}
						spike_first_dt  = jdt;
						spike_start_idx = (uint32_t)j;
						j--;
						look++;
					}

					uint32_t spike_count = spike_end_idx - spike_start_idx + 1;
					if (!first_spike) spikes_json += ",";
					spikes_json += "{";
					spikes_json += "\"start_idx\":"  + String(spike_start_idx) + ",";
					spikes_json += "\"end_idx\":"    + String(spike_end_idx)   + ",";
					spikes_json += "\"count\":"      + String(spike_count)     + ",";
					spikes_json += "\"first_dt\":"   + String(spike_first_dt)  + ",";
					spikes_json += "\"last_dt\":"    + String(spike_last_dt)   + ",";
					spikes_json += "\"prev_dt\":"    + String(before_spike_dt) + ",";
					spikes_json += "\"next_dt\":"    + String(resume_dt);
					spikes_json += "}";
					first_spike = false;
				}

				prev_dt = dt;
			}

			file_offset += got;
		}

		file.close();
		spikes_json += "]";

		String result = "{";
		result += "\"num_recs\":"      + String(num_recs)   + ",";
		result += "\"first_dt\":"      + String(first_dt)   + ",";
		result += "\"last_dt\":"       + String(last_dt)    + ",";
		result += "\"tombstones\":"    + String(tombstones) + ",";
		result += "\"spikes\":"        + spikes_json        + ",";
		result += "\"needs_compact\":" + String(tombstones > 0 ? "true" : "false");
		result += "}";
		return result;
	}


	//-----------------------------------------
	// tombstoneByDt()
	//-----------------------------------------

	bool myIOTDataLog::tombstoneByDt(uint32_t dt)
	{
		String filename = dataFilename();
		File file = SD.open(filename.c_str(), "r+");
		if (!file)
		{
			LOGE("tombstoneByDt() could not open %s", filename.c_str());
			return false;
		}

		// Chunked forward read to find matching records, then seek back only
		// to write the tombstone (dt=0).  ~1075 reads instead of 100K seeks.
		#define TOMB_BASE_BUF  1024
		int     buf_size = ((TOMB_BASE_BUF + m_rec_size - 1) / m_rec_size) * m_rec_size;
		uint8_t stack_buffer[buf_size];

		uint32_t size       = file.size();
		uint32_t file_offset = 0;
		uint32_t rec_idx    = 0;
		bool     found      = false;
		const uint32_t zero = 0;

		while (file_offset < size)
		{
			int to_read = (int)min((uint32_t)buf_size, size - file_offset);
			to_read = (to_read / m_rec_size) * m_rec_size;
			if (to_read == 0) break;

			if (!file.seek(file_offset)) break;
			int got = file.read(stack_buffer, to_read);
			if (got <= 0) break;

			int recs_in_chunk = got / m_rec_size;
			for (int r = 0; r < recs_in_chunk; r++, rec_idx++)
			{
				uint32_t rec_dt;
				memcpy(&rec_dt, stack_buffer + r * m_rec_size, 4);
				if (rec_dt == dt)
				{
					uint32_t write_pos = file_offset + r * m_rec_size;
					file.seek(write_pos);
					file.write((uint8_t *)&zero, 4);
					found = true;
				}
			}

			file_offset += got;
		}

		file.close();
		LOGI("tombstoneByDt(%u) found=%d", dt, found);
		return found;
	}


	//-----------------------------------------
	// tombstoneByIndex()
	//-----------------------------------------

	bool myIOTDataLog::tombstoneByIndex(uint32_t idx)
	{
		String filename = dataFilename();
		File file = SD.open(filename.c_str(), "r+");
		if (!file)
		{
			LOGE("tombstoneByIndex() could not open %s", filename.c_str());
			return false;
		}

		uint32_t num_recs = file.size() / m_rec_size;
		if (idx >= num_recs)
		{
			LOGE("tombstoneByIndex(%u) out of range (%u recs)", idx, num_recs);
			file.close();
			return false;
		}

		const uint32_t zero = 0;
		bool ok = file.seek(idx * m_rec_size) &&
				  (file.write((uint8_t *)&zero, 4) == 4);
		file.close();
		LOGI("tombstoneByIndex(%u) ok=%d", idx, ok);
		return ok;
	}


	//-----------------------------------------
	// compactFile() / trimBefore()
	//-----------------------------------------

	static bool rewriteFile(myIOTDataLog *log, uint32_t cutoff_dt)
		// Rewrite the datalog keeping records where dt!=0 && dt>=cutoff_dt.
		// cutoff_dt==0 means compact only (keep all non-tombstone records).
		//
		// Uses two chunked buffers: read a chunk from src, filter into a write
		// buffer, flush the write buffer when full or at end.  ~1075 SD reads
		// and proportionally fewer writes instead of one seek per record.
	{
		String filename = log->dataFilename();
		String tmpname = "/" + String(log->getName()) + ".tmp";

		File src = SD.open(filename.c_str(), FILE_READ);
		if (!src)
		{
			LOGE("rewriteFile() could not open %s", filename.c_str());
			return false;
		}

		if (SD.exists(tmpname.c_str()))
			SD.remove(tmpname.c_str());

		File dst = SD.open(tmpname.c_str(), FILE_WRITE);
		if (!dst)
		{
			LOGE("rewriteFile() could not create %s", tmpname.c_str());
			src.close();
			return false;
		}

		int rec_size = log->getRecSize();
		#define RW_BASE_BUF 1024
		int buf_size = ((RW_BASE_BUF + rec_size - 1) / rec_size) * rec_size;

		uint8_t read_buf[buf_size];
		uint8_t write_buf[buf_size];
		int write_fill = 0;   // bytes accumulated in write_buf
		int written    = 0;   // records written

		int got;
		while ((got = src.read(read_buf, buf_size)) > 0)
		{
			int recs = (got / rec_size) * rec_size;  // ignore partial trailing bytes
			for (int off = 0; off < recs; off += rec_size)
			{
				uint32_t dt;
				memcpy(&dt, read_buf + off, 4);
				if (dt == 0 || (cutoff_dt != 0 && dt < cutoff_dt))
					continue;

				memcpy(write_buf + write_fill, read_buf + off, rec_size);
				write_fill += rec_size;
				written++;

				if (write_fill == buf_size)
				{
					dst.write(write_buf, write_fill);
					write_fill = 0;
				}
			}
		}

		if (write_fill > 0)
			dst.write(write_buf, write_fill);

		src.close();
		dst.close();

		SD.remove(filename.c_str());
		if (!SD.rename(tmpname.c_str(), filename.c_str()))
		{
			LOGE("rewriteFile() rename failed");
			return false;
		}

		LOGI("rewriteFile(%s cutoff=%u) wrote %d records", log->getName(), cutoff_dt, written);
		return true;
	}


	bool myIOTDataLog::compactFile()
	{
		return rewriteFile(this, 0);
	}

	bool myIOTDataLog::trimBefore(uint32_t cutoff_dt)
	{
		return rewriteFile(this, cutoff_dt);
	}


#endif	// WITH_SD

