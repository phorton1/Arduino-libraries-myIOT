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
// This gives me two choices, I think :
//
//		normalize all subsquent column data to the 0th column.
//			In other words, if col0 is temp1, and temp1 goes from
//			 -32 to -20, then all the other columns will be scaled
// 			from -32 to -20 the temperature is from, even though
//			their min&max scales on the yaxisN are arbitrary
//
//  or more sanely:
//
//		try to use a hidden main yaxis with min0 and max0,
//			and then scale all the values within their own
//			min and max values.
// rather than send the data in a raw jqplot ready format
// which means sending a bunch of text, redundant at that,
// we send the raw binary data for each record.
//
// This object will eventually have a more complicated API,
// splitting the responsibility for graph scales between
// the app specific C++ and generic HTML/javascript.
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
// - allowing the app and/or user to specify if the zooming
//   javascript should be included.
// - allowing the user to specify, in the HTML, which cols
//   they want to display
// - allowing the user to specify the period of time they
//   wish to look at, based on ..
// - storing the data on the SD card with or without an
//   additional date-index file of some sort.
// - generally addressing memory usage in myIOT apps
// - changing to an aynchronous myIOTWebServer, which would
//   require a lot of reworking.
//
// I am checking this in as-is because it is a pretty big
// advancement.

#include "myIOTDataLog.h"

#if WITH_SD
// the rest of this file!

#include "myIOTDevice.h"
#include "myIOTLog.h"
#include "myIOTWebServer.h"
// #include <FS.h>


#define DEBUG_QUEUE			0
#define DEBUG_SEND_DATA		1


#define ILLEGAL_DT		1726764664		// 2024-09-19  11:51:04a Panama Local Time

myIOTDataLog::myIOTDataLog(
		const char *name,
		int num_cols,
		logColumn_t *cols ) :
	m_name(name),
	m_num_cols(num_cols),
	m_col(cols)
{
	m_rec_size = (1 + m_num_cols) * sizeof(uint32_t);

	m_num_alloc = 0;
	m_rec_buffer = NULL;
	m_head = 0;
	m_tail = 0;
}


bool myIOTDataLog::init(int num_mem_recs)
{
	if (!my_iot_device->hasSD())
	{
		LOGE("No SD Card in myIOTDataLog::init()");
		return false;
	}
	if (!num_mem_recs)
	{
		LOGW("myIOTDataLog::init(0) called. Using default 10 in-memory records");
		num_mem_recs = 10;
	}
	if (!num_mem_recs > 100)
	{
		LOGW("myIOTDataLog::init(%d) called. Using maximum of 100 in-memory records",num_mem_recs);
		num_mem_recs = 100;
	}
	
	m_num_alloc = num_mem_recs;
	int bytes = m_num_alloc * m_rec_size;
	LOGI("myIOTDataLog(%s) init(%d) will require %d bytes",
		 m_name,
		 m_num_alloc,
		 bytes);
	m_rec_buffer = new uint8_t[bytes];
	memset(m_rec_buffer,0,bytes);

	return true;
}


//---------------------------------------------------
// addRecord() and flushToSD()
//---------------------------------------------------

String myIOTDataLog::dataFilename()
{
	String filename = "/";
	filename += m_name;
	filename += ".datalog";
	return filename;
}


bool myIOTDataLog::writeSDRecs(File &file, const char *what, int at, int num_recs)
{
	int num_bytes = num_recs * m_rec_size;

	#if DEBUG_QUEUE
		LOGD("   writing %d records in %d bytes from %s of queue",num_recs,num_bytes,what);
	#endif

	logRecord_t rec = mem_rec(at);
	int bytes = file.write((const uint8_t*)rec,num_bytes);
	if (bytes != num_bytes)
	{
		LOGE("Error writing %d records in %d bytes from %s of queue to %s",
			 num_recs,num_bytes,what,dataFilename().c_str());
		return false;
	}
	return true;
}



bool myIOTDataLog::flushToSD()
{
	int use_head = m_head;
	if (m_tail == use_head)
		return true;		// nothing to be written

	String filename = dataFilename();
#if DEBUG_QUEUE
	LOGD("myIOTDataLog::flushToSD(%s) alloc(%d) use_head(%d) tail(%d)",filename.c_str(),m_num_alloc,use_head,m_tail);
#endif

	File file = SD.open(filename, FILE_APPEND);
	if (!file)
	{
		LOGE("Could not open %s for writing",filename.c_str());
		return false;
	}

	bool ok = true;
	if (m_tail > use_head)	// write end of buffer
	{
		int num_write = m_num_alloc - m_tail;
		ok = writeSDRecs(file,"end",m_tail,num_write);
		if (ok)
			m_tail = 0;
	}
	if (ok && use_head > m_tail)
	{
		int num_write = use_head - m_tail;
		ok = writeSDRecs(file,"start",m_tail,num_write);
		if (ok)
			m_tail = use_head;
	}

	file.close();
	return ok;
}



bool myIOTDataLog::addRecord(const logRecord_t in_rec)
{
	uint32_t now_gmt = time(NULL);
	if (now_gmt < ILLEGAL_DT)
	{
		String local_s = timeToString(now_gmt);
		LOGE("attempt to call myIOTDataLog::addRecord(%d) with bad time(%s)",now_gmt,local_s.c_str());
		return false;
	}

	int at = m_head;
	int new_head = m_head + 1;
	if (new_head >= m_num_alloc)
		new_head = 0;
	if (new_head == m_tail)
	{
		LOGE("myIOTDataLog::addRecord() buffer overflow alloc=%d head=%d tail=%d",
			m_num_alloc,
			m_head,
			m_tail);
		return false;
	}

#if DEBUG_QUEUE
	LOGD("myIOTDataLog::addRecord(%d) alloc(%d) new_head(%d) tail(%d)",at,m_num_alloc,new_head,m_tail);
#endif

	m_head = new_head;
	logRecord_t new_rec = mem_rec(at);
	*new_rec = now_gmt;
	memcpy(&new_rec[1],in_rec,m_num_cols * sizeof(uint32_t));

	// debugging

#if DEBUG_QUEUE > 1
	logRecord_t dbg_rec = new_rec;
	String tm = timeToString(*dbg_rec);
	LOGD("   dt(%u) = %s",*dbg_rec++,tm.c_str());
	for (int i=0; i<m_num_cols; i++)
	{
		uint32_t col_type = m_col[i].type;
		if (col_type == LOG_COL_TYPE_FLOAT)
		{
			float float_val = *((float*)(dbg_rec++));
			LOGD("   %-15s = %0.3f",m_col[i].name,float_val);
		}
		else if (col_type == LOG_COL_TYPE_INT32)
		{
			int32_t int32_val = *((int32_t*)(dbg_rec++));
			LOGD("   %-15s = %d",m_col[i].name,int32_val);
		}
		else
		{
			LOGD("   %-15s = %d",m_col[i].name,*dbg_rec++);
		}
	}
#endif
}


//-----------------------------------------
// getChartHeader()
//-----------------------------------------

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


String myIOTDataLog::getChartHeader()
{
	String rslt = "{\n";

	addJsonVal(rslt,"name",m_name,true,true,true);
	addJsonVal(rslt,"num_cols",String(m_num_cols),false,true,true);

	rslt += "\"col\":[\n";

	for (int i=0; i<m_num_cols; i++)
	{
		if (i) rslt += ",";
		rslt += "{";

		const logColumn_t *col = &m_col[i];
		const char *str =
			col->type == LOG_COL_TYPE_FLOAT ? "float" :
			col->type == LOG_COL_TYPE_INT32 ? "int32_t" :
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


//-----------------------------------------
// getChartData()
//-----------------------------------------


String myIOTDataLog::sendChartData(uint32_t num_recs)
{
	String filename = dataFilename();
	#if DEBUG_SEND_DATA > 1
		LOGI("sendChartData(%d) from %s",num_recs,filename.c_str());
	#endif

	File file;
	uint32_t num_file_recs = 0;
	if (!SD.exists(filename))
	{
		LOGW("missing %s",filename.c_str());
	}
	else
	{
		file = SD.open(filename, FILE_READ);
		if (!file)
		{
			LOGE("Could not open %s for reading",filename.c_str());
			return "";
		}
		uint32_t size = file.size();
		if (!size)
		{
			LOGW("empty %s",filename.c_str());
		}
		else if (size % m_rec_size)
		{
			LOGE("%s size(%d) is not a multiple of rec_size(%d)",filename.c_str(),size,m_rec_size);
			file.close();
			return "";
		}
		num_file_recs = size / m_rec_size;
		#if DEBUG_SEND_DATA>1
			LOGD("   file size=%d  num_file_recs=%d",size,num_filerecs);
		#endif
	}

	// seek if necessary

	uint32_t at = 0;
	if (num_recs == 0)
		num_recs = num_file_recs;
	if (num_recs > num_file_recs)
		num_recs = num_file_recs;
	if (num_recs < num_file_recs)
	{
		uint32_t rec_at = num_file_recs - num_recs;
		at = rec_at * m_rec_size;
		#if DEBUG_SEND_DATA>0
			LOGD("   Seeking rec %d of %d at byte %d",rec_at,num_file_recs,at);
		#endif
		if (!file.seek(at))
		{
			LOGE("Could not seek to rec %d of %d at byte %d in %s",rec_at,num_file_recs,at,filename.c_str());
			file.close();
			return "";
		}
	}

	// record buffer on stack
	// content_len 4 byte num_recs followed by records

	#define WRITE_BUF_BYTES		512
	uint8_t buf[WRITE_BUF_BYTES];
	uint32_t content_len = 4 + num_recs * m_rec_size;

	#if DEBUG_SEND_DATA
		LOGD("   sending %d records with content_len(%d)",num_recs,content_len);
	#endif

	if (myiot_web_server->startBinaryResponse("application/octet-stream", content_len))
	{
		if (myiot_web_server->writeBinaryData((const char *)&num_recs,4))
		{

			uint32_t left = num_recs * m_rec_size;
			while (left)
			{
				uint32_t amt = left;
				if (amt > WRITE_BUF_BYTES)
					amt = WRITE_BUF_BYTES;
				#if DEBUG_SEND_DATA > 1
					LOGD("      reading and sending %d bytes at %d",amt,at);
				#endif
				uint32_t bytes = file.read(buf,amt);
				if (amt != bytes)
				{
					LOGE("Error reading (%d/%d) bytes at %d from %s",bytes,amt,at,filename.c_str());
					left = 0;
				}
				else if (!myiot_web_server->writeBinaryData((const char *)buf,amt))
				{
					left = 0;
				}
				else
				{
					at += amt;
					left -= amt;
				}
			}
		}
	}

	if (file)
		file.close();
	// 	myiot_web_server->finishBinaryResponse();
	return RESPONSE_HANDLED;
}



//-----------------------------
// getChartHTML()
//-----------------------------

// data_log.m_name = fridgeData
	// produces
	//		<div id='fridgeData'>
	//			<div id='fridgetData_chart'></div>
	//			<button id=fridgeData_plot_button'>PLOT</button>
	//			<select id=fridgeData_period_select'>
	//			<input type='number' id='fridgeData_refresh_interval'>
	// uses urls that must be handled by client
	// by calling getChartHeader() and sendChartData(), below
	//		/custom/chart_header/frigeData and
	//		/custom/chart_data/fridgeData?secs=NNN
	//			client is only one who knows how to change
	//			seconds into #records at this time
	// because the base myIOTDevice does not keep track of
	// dataLoggers.  They are instantiated purely by derived devices.

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
	rslt += "_chart' style='height:";
	rslt += String(height);
	rslt += "px;width:";
	rslt += String(width);
	rslt += "px;'></div>\n";

	rslt += "&nbsp;&nbsp;&nbsp;\n";

	rslt += "<button id='";
	rslt += m_name;
	rslt += "_update_button";
	rslt += "' onclick=\"doPlot('";
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




#endif	// !WITH_SD

