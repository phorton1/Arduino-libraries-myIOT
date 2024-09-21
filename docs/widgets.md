# myIOT - Widgets, Charts & Data Logging, Custom Pages & Pass Through HTTP

This document crosses the boundary between the base-apps-myIOTServer
and Anrdoid-libraries-myIOT repositories.


We want to implement a dt (or sha checksum) scheme for
CACHING js and other files in the webserver for increased
performance.  But first we have to get it working!



## Widgets

For aggregation by the myIOTServer into a single page representing the
state of the entire LAN, each myIOTDevice may specify a "widget" that
will be shown on the (new) "Dashboard" tab of the myIOTServer, after the
current "Dashboard" tab, with Reboot, Update, etc, is renamed "System".

Via the myIOTServer's preferences file, and/or a UI to modify it, the
user will be able to setup a grid (order and position, not content)
of the Widgets to display.  Better would be that there can be multiple
different layouts, depending on what the user wants to see.

I'm thinking about the boat, and being able to have one page that,
for instance, shows the bilgePump chart (not yet implemented) in
one widget and the fridgeController chart in another widget.
Widgets should have the ability to show a specific set of values,
as well, like the 'status' fields for the clocks, and/or each
might have a "reboot" button.

This goes to the long-standing desire of having more than just a
single column view of specified values on the 'dashboard' and
'config' pages of the ESP32 myIOT device.

Remember that the myIOTServer is leveraging off of the **iotCommon.js**
and **index.html** to provide a 'look-alike' view to a given
selected ESP32 myIOT device in the myIOTServers.


## Charts & Data Logger

The ability to produce a chart shall henceforth be built into the
myIOTDevice.  That first means that the **data submodule** will now
contain the jqplot JS on all devices.

That also implies that the prototype fridgeController::dataLog object
will be moved into the myIOT library as myIOTDataLogger.

However, the Charts should not be limited to devices that have SD cards.
One thing that would be super-nice would be able to handle quick
plots, like the sine-waves for the clock, or the voltage sensing
for the fridgeController reasonably in a webpage via the existing
open WebSocket in, say, 1 second chunks, where the Javascript and
browser could maintain the bigger data array.

Also, for those devices without SD Cards, like the clocks, the
myIOTServer, running on an rPi with more storage, could serve as
the dataLogger location, without sacrificing the ability for
individual devices to have their own data-logs.  And/or the
device's JS could, as long as a window and WS remains open to the
device, could aggregate slower data in memory.

All of this is very exciting, yet very complicated.


## Custom Pages & Pass Thru HTTP

Not currently documented elsewhere is the way in which the myIOTServer
look-alike view works with pass-thru HTTP calls to implement the SD card
and SPIFFS tabs and the OTA button, and how the 'custom' links, and
associated pass-thru HTTP calls to get 'custom' HTML, JS, JSON, and
binary data from the underlying myIOT device works.

It is currently implemented by a thin set of conventions about how
certain pathnames will be interpreted by the myIOTServer and the
myIOT library HTTP Server.

The current problem is that there is no simple way to append the
required **?uuid=XXX** parameter in static files served from
the the ESP32 so that it works with the myIOTServer.  The current
example is the fridgeController's "temp_chart.html" file,
and the problem *may* extend to this new idea of **widgets**
more generally.


### SPIFFS list and File Upload

To start with, for example, we look at how the SPIFFS tab shows
a list of files, and how the "Upload" files within it works.

The common **index.html** contains the following HTML snippet
for the spiffs tab:

```
    <!-- SPIFFS tab -->

    <div id="spiffs" class="tab-pane fade">
        <table id='spiffs_list' class='table' >
            <thead>
                <tr>
                    <th>
                        Name
                        <label htmlFor="spiffs_files">
                            <button class='btn btn-secondary myiot' onclick="onUploadClick('spiffs_files')">upload</button>
                        </label>
                        <input type="file" multiple id="spiffs_files" class='myiot' onchange="uploadFiles(event)" style="display:none"/>

                        <label htmlFor="ota_files">
                            <button class='btn btn-secondary myiot' onclick="onUploadClick('ota_files')">OTA</button>
                        </label>
                        <input type="file" id="ota_files" class='myiot' onchange="uploadFiles(event)" style="display:none"/>
                    </th>
                    <th id='spiffs_used'>Size</th>
                    <th id='spiffs_size'>&nbsp;</th>
                </tr>
            </thead>
            <tbody>
                <tr><td>test</td><td>2</td><td>&nbsp;</td></tr>
            </tbody>
        </table>
    </div>
```


The **iotCommon.js** populates the table whenever it receives
a WS json that includes a *spiffs_list* member:

```
    if (obj.spiffs_list)
        updateFileList(obj.spiffs_list);
	...

	for (var i=0; i<obj.files.length; i++)
    {
        var file = obj.files[i];
        var link = '<a class="myiot" ' +
            'target=”_blank” ' +
            'href="/' + prefix + '/' + file.name;
        if (is_server)
            link += '?uuid=' + device_uuid;
        link += '">' + file.name;
        link += '</a>';
        var button = "<button " +
            "class='btn btn-secondary myiot' " +
            // "class='my_trash_can' " +
            "onclick='confirmDelete(\"/" + prefix + "/" +  file.name + "\")'>" +
            "delete" +
            // "<span class='my_trash_can'>delete</span>" +
            "</button>";
        $('table#' + prefix + '_list tbody').append(
            $('<tr />').append(
              $('<td />').append(link),
              $('<td />').text(file.size),
              $('<td />').append(button) ));
    }
```

In either myIOT or myIOTServer, the JS prepends
'/spiffs/' to the file request, so that it knows where the
file is.

The javascript adds a **?uuid=device_uuid** http parameter to
the link for each file so that later, if it receives that
request for /blah.file_type?uuid=XXX it can do a pass-thru
call to device XXX for the file.


### Path Prefix and Data Submodule conventions

- **/** - *should be* used only by the myIOTServer to reference files
  in the DOCUMENT_ROOT that make up the 'admin.html' page.
  Does not make sense in cross-server stuff that applies to
  both servers.
- **/myIOT/** - a path on either server that refers to files in
  the common **data submodule**.
- **/spiffs/** - a path that means the file exists on the ESP32
  SPIFFS filesystem, and which must have a ?uuid=XXXX http parameter
  to work with pass-thrus from the myIOTServer.
- **/sdczrd/** - a path that means the file exists on the ESP32
  SD filesystem, and which must have a ?uuid=XXXX http parameter
  to work with pass-thrus from the myIOTServer.
- **/custom/** - a path that means the request will be handled
  by the specific ESP32 device implementation via the onCustomLink
  mechanism, and which must have a ?uuid=XXXX http parameter
  to work with pass-thrus from the myIOTServer.
- **/spiffs_files/** **/sdcard_files/** and **/ota_files/** -
  are file upload prefixes which are handled specifically
  by the myIOT server as POST requests containing one
  or more files to upload to the given location on the ESP32,
  and which must have a ?uuid=XXXX http parameter
  to work with pass-thrus from the myIOTServer.

In most cases, the **iotCommon.js**, when existing as **is_server**
on the myIOTServer, automatically appends the *?uuid=XXX* parameter,
as needed, and since the look-alike Javascript has the context of the current
device, knows where to send the /spiffs_files/, /sdcard_files/,
and /ota_files/ requests.


### **/myIOT/** as an exmple.

Generally speaking, any HTTP request on an ESP32 is assumed to be
a request to get something from the common *data submodule* at **/**.
However, because the myIOTServer also (perhaps primarily) serves its
own "admin" webpage, and it's DOCUMENT_ROOT maps to /base/apps/myIOTServer
/site, it nests the common *data submodule* in /site/myIOT.

Therfore when **index.html** (*or any other webpage*) includes the
common *data submodule* JS libraries, it includes them like this:

```
    <link href="/myIOT/iotCommon.css" rel="stylesheet">
    <script src="/myIOT/jquery-3.6.0.min.js"></script>
```

The myIOTServer then naturally checks for the existence of,
and serves the file from it's /site/myIOT *data submodule*.
This convention is known by the myIOT server, and it **removes**
any leading **/myIOT/** it finds, and just serves the file,
if it exists, from the SPIFFS file system, which is where it
stores it's copy of the **data submodule**, which is also,
by convention in the /data directory of the Arduino-specific_device
repository/directory so that it can be uploaded with the mkspiffs
tool (on IDE 1.18.3) and/or my /base/bat/upload_spiffs.pm script.





