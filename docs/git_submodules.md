# Shared JS, CSS, and HTML files

The myIOT/data directory contains a version of jquery, bootstrap,
and some common JS, CSS, and HTML that is shared between Esp32
myIOT projects, as well as to the Perl apps::myIOTServer project.

The file captive.html is self contained and does not load any
additional JS, CSS, or HTML, and is only used in Esp32 projects.

In Esp32 projects, the entire contents of the /data directory
are uploaded to the **flat** SPIFFS file system and served from
there statically. A request to '/' gets turned into **index.html**,
which theen causes the browser to request the JS and CSS from the
HTTP Server.

The directory is called 'data' becuase that is the standard name
of an Arduino project directory that you upload to a SPIFFS
file system.


## Vestigial Design

Because SPIFFS is a flat file system, in the initial implentation
we simply mapped '/' to 'index.html' and serve any otherwise unhandled
requests as simple absolute file requests, thus serving any files that
are found on the SPIFFS system as 'root' files.

The bilgeAlarm stores the history.dat file in the root of the
SPIFFS as well, but it is never uploaded to the ESP32.  The clocks
don't upload anything special to, or otherwise use, the SPIFFS.

Super duper care was taken when implementing the Perl myIOTServer
to use the same common index.HTML, and the JS and CSS from the
/data directory.  The JS and CSS are *slightly* but **significantly**
modified to work with the myIOTServer to redirect to a given specific
device that is known to the myIOTServer.

The myIOTServer not make use of captive.html. Care would have to
be taken to change the Wifi Host or Credentials in any myIOT thing
via any UI, as the very act of doing so **may** cause the connection
to be lost (the wifi host and credentials should only be picked up
in a reboot).


# Denormalization

In any case, the issue of normalizing this source code has become
a bit onerous.  As I mentioned, it currently exists 6 times in
my github repos:

	/src/Arduino/libraries/myIOT/data - the 'master' copy
	/src/Arduino/libraries/myIOT/examples/testDevice/data
	/src/Arduino/bilgeAlarm/data
	/src/Arduino/theClock/data - unused code, really
	/src/Arduino/bilgeAlarm/data
	/base/apps/myIOTServer/site

It is particularly problematic in myIOTServer, where the /site
directory also contains:

	admin.html
	admin.js
	standard_system.js - also denormalized
	favicon.ico (not currently on ESP32, we may want different in each device)

The issue arises because I also want to normalize 'standard_system.h' somehow,
and, in fact, change it to 'standard_utils.js'.  Also of significance is that
the entire Inventory jquery is essentially a copy of that from Artisan with
an added 'DataTables', and minus 'fancyTree'.


There are a few other 'denormalized' files in my git repos, including
/src/circle/_prh/_apps/Looper/commonDefines.h which is copied to
/src/Arduino/teensyExpression and /src/Arduino/teensyExpression2, but
for now I am not going to worry about that, and just try to think about
the common HTML, JS, and CSS.


## Git Submodules

My idea is to use git submodules in all but one of the projects.
In this case, the /src/Arduino/Libraries/myIOT/data directory would become
a new standalone repo with 'exclusion' in .gitignore like I currently
do for Perl sublibraries.  This exclusion would need to be tested
as myIOT is much more nested than /base/My and /base/Pub, but I think
it can be made to work.

The other projects would then use the git submodules thing. Without
going into too much detail, that would STILL create some issues:

- working on the submodule in one project, what does it mean to
  commit and push it?  Within the project?
- updating the other ones to re-fetch the modified module
- working in my gitUI program


## Testing - Theory

I don't feel confident just making this change to my actual source.
I think I need to create a copy of test projects in /junk for this
and add them to github ... the whole 9 yards ...

Which in turn puts me up agains the 100 file limit for github
repo calls in gitUI::reposGitHub.pm.

Which in turn means that I should clean up junk, and get
rid of any superflous _old source directories, just for sanity.


