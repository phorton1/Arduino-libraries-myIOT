# myIOT - data_master

This page describes the scheme for using the **myIOT/data_master**
repo into as a git submodule in a **myIOT project**.
This information has not yet been incorporated into the
[main myIOT documentation](/docs/readme.md).


## Shared JS, CSS, and HTML files

The myIOT/data_master repo contains a version of jquery, bootstrap,
and some common JS, CSS, and HTML that are served by myIOT projects.
The data_master repo is currently included as a submodule in the
following repos:

- /src/Arduino/libraries/myIOT/examples/testDevice/data
- /src/Arduino/bilgeAlarm/data
- /src/Arduino/theClock/data
- /src/Arduino/theClock3/data
- /base/apps/myIOTServer/site/myIOT


The file captive.html is self contained and does not load any
additional JS, CSS, or HTML, and is only used in Esp32 (Arduino)
repos.

In Esp32 projects, the entire contents of the /data directory
are uploaded to the **flat** SPIFFS file system and served from
there statically.  All paths are prefixed with **/myIOT/**, so, for example,
a request to '/' gets turned into **/myIOT/index.html**,
which is loaded, and which then causes the browser to request the
JS and CSS from the HTTP Server.

In the ESP32 the /myIOT/ prefix is merely stripped off and the
files are served from the flat SPIFFS.  In the myIOTServer Perl,
on the other hand, the HTTP server notices in leading /myIOT/ and
serves the fils from /base/apps/myIOTServer/site/myIOT, instead
of its normal location (/site).

The directory is called 'data' becuase that is the standard name
of an Arduino project directory that you upload to a SPIFFS
file system, and works with the standard ESP32 Data Uploader tool.


## USING SUBMODULES

Adding a submodule to an existing project. For example, to add
the /src/Arduino/libraries/myIOT/data_master submodule to the myIOT/examples
folder (after removing and commiting the removal of the old data folder,
if one existed):

	cd /src/Arduino/libraries/myIOT
	git submodule add https://github.com/phorton1/Arduino-libraries-myIOT-data_master examples/testDevice/data

Cloning a repository that contains submodules is done automatically
if you pass --recursive on the command line.  Here's how you would
clone the src-Arduino-theClock3 repo into your Arduino projects folder:

	cd /src/Arduino
	git clone --recursive https://github.com/phorton1/src-Arduino-theClock3 theClock3

After cloning a new submodules you *NEED TO* checkout the 'master' branch before proceeding,
possibly from the directory:

	cd /src/Arduino/theClock3/data
	git checkout master

Updating a repository that has submodules that are out of date, i.e.
after modifying, committing and pushing the **myIOT/data_master** repo,
the copy in theClock3 can be updated either by doing a "git pull" from
the submodule directory:

	cd /src/Arduino/theClock3/data
	git pull

Or for all sumodules in theClock3 by:

	cd /src/Arduino/theClock3
	git submodule update --recursive --remote


## Parent Repo Submodule Change

Note that updating a submodule creates an UNSTAGED CHANGE
on the parent repo which then needs to, itself, be committed
and or pushed.


## Notes vis-a-vis Pub::ServiceUpdate

On the rPi, which alreaady had a myIOTServer project, after my built-in
ServiceUpdate, it had the submodule, but an empty directy.
I had to redo the submodule update with --init

	cd /base/apps/myIOTServer
	git submodule update --init --recursive --remote

to get it to download the submodule. I may only need to do
that the first time. In any case I'm pretty sure as long as I
don't change the myIOT folder, the ServiceUpdate mechanism will
still get the rest of the myIOTServer source code changes ok.

My ServiceUpdate mechanism, and updating submodules in general
is "Work in Progress" as part of the GIT STATUS  project, currently
under the umbrella of this apps/gitUI project.
