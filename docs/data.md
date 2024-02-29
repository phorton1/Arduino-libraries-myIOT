# myIOT - Data

This page describes the scheme for denormalizing
the data directory into git submodules and has not
yet been incorporated into the pretty documentation.


## myIOT/data

Will be made into it's own repository, with my standard
.git_ignore.  /examples/testDevice/data will then be
a submodule reference to it, as will be the /data directories
in bilgeAlarm, theClock, theClock3, and the Perl myIOTServer
site directory.

This will require changes to the way it is served, and referenced,
particularly in myIOTServer.

Cloning a repository that contains submodules is done automatically
if you pass --recursive on the command line:

	git clone --recursive https://github.com/phorton1/junk-test_repo2 some_other_name

Before I do that, though, I wonder what it is like to clone a
repo that has submodules.  Is there a command to automatically
clone the submodules?
