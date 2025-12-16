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

#define DEBUG_ADD			0
#define DEBUG_SEND_DATA		1

#define ILLEGAL_DT		1726764664		// 2024-09-19  11:51:04a Panama Local Time


//------------------------------------
// implementation
//------------------------------------

myIOTDataLog::myIOTDataLog(
		const char *name,
		int num_cols,
		logColumn_t *cols,
		int debug_send_data /* = 1*/ ) :
	m_name(name),
	m_num_cols(num_cols),
	m_col(cols),
	m_debug_send_data(debug_send_data)
{
	m_rec_size = (1 + m_num_cols) * sizeof(uint32_t);
}


void myIOTDataLog::dbg_rec(logRecord_t rec)
{
	String tm = timeToString(*rec);
	LOGD("   dt(%u) = %s",*rec++,tm.c_str());
	for (int i=0; i<m_num_cols; i++)
	{
		uint32_t col_type = m_col[i].type;
		if (col_type == LOG_COL_TYPE_FLOAT ||
			col_type == LOG_COL_TYPE_TEMPERATURE)
		{
			float float_val = *((float*)(rec++));
			LOGD("   %-15s = %0.3f",m_col[i].name,float_val);
		}
		else if (col_type == LOG_COL_TYPE_INT32)

		{
			int32_t int32_val = *((int32_t*)(rec++));
			LOGD("   %-15s = %d",m_col[i].name,int32_val);
		}
		else
		{
			LOGD("   %-15s = %d",m_col[i].name,*rec++);
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
		*rec = time(NULL);
		if (*rec < ILLEGAL_DT)
		{
			String stime = timeToString(*rec);
			LOGE("attempt to call myIOTDataLog::addRecord(%d) at bad time(%s)",*rec,stime.c_str());
			return false;
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
		LOGD("myIOTDataLog::addRecord(%d) at ",num_recs + 1,size);
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


//-----------------------------
// getChartHTML()
//-----------------------------
// produces
//		<div id='fridgeData'>
//			<div id='fridgetData_chart'></div>
//			<button id=fridgeData_plot_button'>PLOT</button>
//			<select id=fridgeData_period_select'>
//			<input type='number' id='fridgeData_refresh_interval'>
// and uses urls that must be handled by client.


String addSelectOption(int default_value,int value, const char *name)
{
	String rslt = "<option value='";
	rslt += String(value);
	rslt += "'";
	if (value == default_value)
		rslt += " selected='selected'";
	rslt += ">";
	rslt += name;
	rslt += "</option>\n";
	return rslt;
}
	

String myIOTDataLog::getChartHTML(
	int height,
	int width,
	int period,
	int refresh)
{
	LOGD("myIOTDataLog::getChartHTML(%d,%d,%d,%d)",height,width,period,refresh);

	String rslt = "<div id='";
	rslt += m_name;
	rslt += "'>\n";

	rslt += "<div id='";
	rslt += m_name;
	rslt += "_chart'";
	rslt += " class='iot_chart'";
	
	#if 0
		rslt += " height='";
		rslt += String(height);
		rslt += "px' width'";
		rslt += String(width);
		rslt += "px'";
	#elif 0
		rslt += " height:'100%' width:'100%'";
	#elif 0
		rslt += " style='height:100%;width:100%;'";
	#elif 0
		rslt += " style='height:";
		rslt += String(height);
		rslt += "px;width:";
		rslt += String(width);
		rslt += "px;'";
	#endif
	rslt += ">";
	rslt += "</div>\n";

	rslt += "&nbsp;&nbsp;&nbsp;\n";

	rslt += "<button id='";
	rslt += m_name;
	rslt += "_update_button";
	rslt += "' onclick=\"doChart('";
	rslt += m_name;
	rslt += "')\" disabled>Update</button>\n";

	rslt += "&nbsp;&nbsp;&nbsp;\n";

	rslt += "<label for='";
	rslt += m_name;
	rslt += "_chart_period'>Chart Period:</label>\n";
	rslt += "<select name='period' id='";
	rslt += m_name;
	rslt += "_chart_period' onchange=\"get_chart_data('";
	rslt += m_name;
	rslt += "')\">\n";
	rslt += addSelectOption(period,0,"All");
	rslt += addSelectOption(period,60,"Minute");
	rslt += addSelectOption(period,900,"15 Minutes");
	rslt += addSelectOption(period,3600,"Hour");
	rslt += addSelectOption(period,10800,"3 Hours");
	rslt += addSelectOption(period,43200,"12 Hours");
	rslt += addSelectOption(period,86400,"Day");
	rslt += addSelectOption(period,172800,"2 Days");
	rslt += addSelectOption(period,259200,"3 Days");
	rslt += addSelectOption(period,604800,"Week");
	rslt += addSelectOption(period,2592000,"Month");
	rslt += addSelectOption(period,5184000,"2 Months");
	rslt += addSelectOption(period,7776000,"3 Months");
	rslt += addSelectOption(period,31536000,"Year");
	rslt += "</select>\n";

	rslt += "&nbsp;&nbsp;&nbsp;\n";

	rslt += "<label for='";
	rslt += m_name;
	rslt += "_refresh_interval'>Refresh Interval:</label>\n";
	rslt += "<input id='";
	rslt += m_name;
	rslt += "_refresh_interval' type='number' value='";
	rslt += String(refresh);
	rslt += "' min='0' max='999999'>\n";

	rslt += "</div>\n";

	#if 0
		Serial.print("myIOTDataLog::getChartHTML()=");
		Serial.println(rslt.c_str());
	#endif
	
	return rslt;

}



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


String myIOTDataLog::getChartHeader(const String *series_colors /*=NULL*/,  int supports_incremental_update /*=0*/)
{
	String rslt = "{\n";

	addJsonVal(rslt,"name",m_name,true,true,true);
	addJsonVal(rslt,"num_cols",String(m_num_cols),false,true,true);
	if (series_colors)
		addJsonVal(rslt,"series_colors",*series_colors,false,true,true);
	if (supports_incremental_update)
		addJsonVal(rslt,"incremental_update",String(supports_incremental_update),false,true,true);

	
	rslt += "\"col\":[\n";

	for (int i=0; i<m_num_cols; i++)
	{
		if (i) rslt += ",";
		rslt += "{";

		const logColumn_t *col = &m_col[i];
		const char *str =
			col->type == LOG_COL_TYPE_FLOAT ? "float" :
			col->type == LOG_COL_TYPE_INT32 ? "int32_t" :
			col->type == LOG_COL_TYPE_TEMPERATURE ? "temperature_t" :
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

		if (iter->dbg_level > 1)
			LOGD("startSDBackwards(%s) rec_size(%d) buf_size(%d)",
				 iter->filename, iter->rec_size, iter->buf_size);

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
			if (iter->dbg_level > 1)
				LOGD("file size=%d  num_file_recs=%d",size,num_file_recs);
		}

		if (num_file_recs > 0)
		{
			iter->done = false;
			iter->read_pos = iter->file.size();	// Initial read position
			if (iter->dbg_level > 1)
				LOGD("initial read_pos=%d",iter->read_pos,num_file_recs);
		}

		// we are setup for the fist iteration

		if (iter->dbg_level > 1)
			LOGD("startSDBackwards(%s) returning done(%d)",iter->filename,iter->done);

		return true;

	}   // startSDBackwards()



	uint8_t *getSDBackwards(SDBackwards_t *iter, int *num_recs)
		// returns record(s), but only as many as the client
		// callback has verified that it wanted
	{
		*num_recs = 0;

		if (iter->done)
			return NULL;

		if (iter->dbg_level>2)
			LOGD("getSDBackwards() idx(%d) read_pos(%d)",iter->buf_idx,iter->read_pos);

		if (iter->buf_idx < 0)  // buffer exhausted
		{
			if (iter->read_pos == 0)
			{
				if (iter->dbg_level > 1)
					LOGD("           END OF FILE");
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

			if (iter->dbg_level > 1)
				LOGD("seeking to file_offset(%d)",iter->read_pos);

			if (!iter->file.seek(iter->read_pos))
			{
				LOGE("Could not seek to byte %d", iter->read_pos);
				iter->file.close();
				return NULL;
			}

			if (iter->dbg_level > 1)
				LOGD("    reading %d bytes at file_offset(%d)",read_bytes,iter->read_pos);

			uint32_t bytes = iter->file.read(iter->buffer, read_bytes);
			if (bytes != read_bytes)
			{
				LOGE("Error reading (%d/%d) at read_pos=%d", bytes,read_bytes,iter->read_pos);
				iter->file.close();
				return NULL;
			}

			iter->num_buf_recs = read_bytes / iter->rec_size;
			iter->buf_idx = iter->num_buf_recs - 1;

			if (iter->dbg_level > 1)
				LOGD("    buf_recs=%d idx=%d",iter->num_buf_recs,iter->buf_idx);

		}   // new buffer succesfully read

		if (iter->chunked)
		{
			uint8_t *retval = NULL;
			bool condition = true;
			while (condition && iter->buf_idx >= 0)
			{
				uint8_t *rec = iter->buffer + (iter->rec_size * iter->buf_idx--);
				condition = iter->record_fxn(iter->client_data,rec);
				if (condition)
				{
					(*num_recs)++;
					retval = rec;
				}
			}

			if (!condition)
			{
				if (iter->dbg_level > 1)
					LOGD("iteration ended by condition at buf_idx(%d)",iter->buf_idx);
				iter->done = 1;
				iter->file.close();
			}

			if (iter->dbg_level > 1)
				LOGD("getSDBackwards(chunked) returning %d records at index %d",*num_recs,iter->buf_idx + 1);
			return retval;
		}
		else
		{
			*num_recs = 1;
			uint8_t *rec = iter->buffer + (iter->rec_size * iter->buf_idx--);
			bool condition = iter->record_fxn(iter->client_data,rec);
			if (condition)
				return rec;
		}

		// iteration ended by user condition

		iter->done = 1;
		iter->file.close();
		return NULL;
	}




	//-----------------------------------------
	// sendChartData()
	//-----------------------------------------
	// There is a lot of debugging in this routine,
	// Upto 4, normally DEBUG_SEND_DATA==1 or so
	//
	// Usually single instance, debugging of static method
	// uses global for multiple instances

	int g_debug_send_data = 1;


	bool chartDataCondition(uint32_t cutoff, uint8_t *rec)
	{
		uint32_t ts = *((uint32_t *) rec);
		if (ts >= cutoff)
			return true;

		if (g_debug_send_data)
		{
			String dt1 = timeToString(ts);
			String dt2 = timeToString(cutoff);
			LOGD("    chartDataCondition(FALSE) at %s < %s",dt1.c_str(),dt2.c_str());
		}
		return false;
	}


	String myIOTDataLog::sendChartData(uint32_t secs)
	{
		#define BASE_BUF_SIZE	1024

		String filename = dataFilename();
		uint32_t cutoff = secs ? time(NULL) - secs : 0;

		// pick bufsize > 512 that will hold even number of records

		int buf_size = ((BASE_BUF_SIZE + m_rec_size-1) / m_rec_size) * m_rec_size;
		uint8_t stack_buffer[buf_size];

		g_debug_send_data = m_debug_send_data;

		if (m_debug_send_data)
		{
			String dbg_tm = timeToString(cutoff);
			LOGI("sendChartData(%d) since %s from %s",secs,secs?dbg_tm.c_str():"forever",filename.c_str());
			if (m_debug_send_data > 1)
				LOGD("buf_size(%d) cutoff=(%d)",buf_size,cutoff);
		}

		// initialize iterator struct

		SDBackwards_t iter;
		iter.chunked        = 1;
		iter.client_data    = cutoff;
		iter.filename       = filename.c_str();
		iter.rec_size       = m_rec_size;
		iter.record_fxn     = chartDataCondition;
		iter.buffer         = stack_buffer;                 // an even multiple of rec_size
		iter.buf_size       = buf_size;
		iter.dbg_level      = m_debug_send_data;              // 0..2

		if (!startSDBackwards(&iter))
			return "";

		if (!myiot_web_server->startBinaryResponse("application/octet-stream", CONTENT_LENGTH_UNKNOWN))
			return "";

		int sent = 0;
		int num_file_recs = iter.file ? iter.file.size() / m_rec_size : 0;

		int num_recs;
		uint8_t *base_rec = getSDBackwards(&iter,&num_recs);
		while (num_recs)
		{
			sent += num_recs;
			if (!myiot_web_server->writeBinaryData((const char*)base_rec, num_recs * m_rec_size))
			{
				if (iter.file)
					iter.file.close();
				return "";
			}
			base_rec = getSDBackwards(&iter,&num_recs);
		}

		if (m_debug_send_data)
		{
			LOGD("    sendChartData() sent %d/%d records",sent,num_file_recs);
		}

		return RESPONSE_HANDLED;
	}

#endif	// WITH_SD

