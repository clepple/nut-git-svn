\nut
##################################################
##################################################
####                                          #### 
####            NUT Configuration             ####
####                                          ####
##################################################
##################################################
#
# This is the main configuration file of Nut
#
# NUT configuration use the following formalism :
#
# ---------
# To specify the value of the driver name of the ups "myups" for instance :
#
# ups.myups.driver.name = "newhidups" r
#
# ---------
# For the moment, only string and enumeration of string can be
# given as value for variable. 
# Even if you want to give a number, give it as a string
#
# String are delimited by quotes :
#      "foo"
# Enumeration of string are delimited by brace :
#      { "foo" "foo2" ... }
#
# ---------
# After a value, you can specify the access right to this value
# Valid rights are :
#
# r       : all users can read the variable. No one write set it
# rw      : all users can read and write the variable
# r*      : only NUT's administrators can read the variable. No one can 
#           write it
# rw*     : all users can read the variable. Only NUT's administrators
#           can write it
# r*w*    : only NUT's administrators can read and write the variable. A
#           good idea for variable like password ;-)
# nothing : don't specify the right. It is the same as putting r
#
# ---------
# As NUT use a tree to expose its variables, you may something need to
# set, for instance :
#
# upsmon.runasuser = "nut"
# upsmon.deadtime = "15"
#
# For better look, you can regroup it like :
#
# upsmon (
#     runasuser = "nut"
#     deadtime = "15"
# )
#
# You can have more than one level :
#
# ups (
#     myups1.driver (
#         name = "newhidups"
#         parameter.port = "auto"
#     )
# )
#
# --------
# As you may want to put configuration of different parts of NUT
# in different files, we provide you an "include" command
#
# include "my_other_conf_file"
#
# has the same effect as copying the content of "my_other_conf_file" in
# place the include command.
# Then if you use an include command in a bloc, the content of the included
# file will inherit the context. 
#
# ups (
#     include "ups.conf"
# )
#
# is the same as
#
# ups (
#     The
#     content
#     of
#     ups.conf
# )
#
# For an include command, you need to put either the complete path to the file
# or just the name of the file.
# If the filename is not a complete path (don't begin with a '/'), the parser
# will try to access the file from your default configuration directory.
# --------------------------------------------------------------------------

\nut.ups_header_only
#################
## UPS SECTION ##
#################

\nut.users_header_only
###################
## USERS SECTION ##
###################

\nut.upsd
##################
## UPSD SECTION ##
##################

\nut.upsmon
####################
## UPSMON SECTION ##
####################

\nut.mode
# --------------------------------------------------------------------------
# mode = "<mode>" - In which mode to run NUT
#
# Possible values are :
#
# - standalone  : The standard case : one UPS (or more) protect one computer 
#                 on which NUT is running without any monitoring by network.
# - net-server  : The current computer monitor one or more UPS that are
#                 physically connected to this computer via USB or serial
#                 cable. Other computers can access to UPS status via network
# - net_client  : The current computer monitor one or more UPS that is NOT
#                 physically connected to this computer via USB or serial
#                 cable. Another computers provide UPS status via network
# - pm          : for unified power management use, or custom use. Note that
#                 NUT will not start any service by itself if this value is
#                 given
# - none        : NUT is not configured yet
#
# Note that this value determine which service NUT will start (for instance
# in net_client mode, upsd is not started)
# --------------------------------------------------------------------------

\nut.ups_desc
# Network UPS Tools: example ups.conf
#
# --- SECURITY NOTE ---
#
# If you use snmp-ups and set a community string in here, you
# will have to secure this file to keep other users from obtaining
# that string.  It needs to be readable by upsdrvctl and any drivers,
# and by upsd.
#
# ---
#
# This is where you configure all the UPSes that this system will be
# monitoring directly.  These are usually attached to serial ports, but
# USB devices and SNMP devices are also supported.
#
# This file is used by upsdrvctl to start and stop your driver(s), and
# is also used by upsd to determine which drivers to monitor.  The
# drivers themselves also read this file for configuration directives.
#
# The general form is:
#
# <upsname> (
# 	driver (
#		name = "<drivername>"
# 		port = "<portname>"
#	)
#	[desc = "UPS description"]
#	.
#	.
# )
#
# The name od the ups (<upsname]> can be just about anything as long as
# it is a single word inside containing only letters, number, '-' and '_'.
#  upsd uses this to uniquely identify a UPS on this system.
#
# If you have a UPS called snoopy on a system called "doghouse", 
# the section in your upsmon section to monitor it would look something
# like this:
#
# monitor.snoopy@doghouse (
# 	powervalue = "1" 
# 	user = "upsmonuser"
# )
#

# Configuration directives
# ------------------------
#
# These directives are common to all drivers that support ups section:
#
#  driver.name: REQUIRED.  Specify the program to run to talk to this UPS.
#          apcsmart, fentonups, bestups, and sec are some examples.
#
#  driver.port: REQUIRED.  The serial port where your UPS is connected.
#          /dev/ttyS0 is usually the first port on Linux boxes, for example.
#
#  sdorder: optional.  When you have multiple UPSes on your system, you
#          usually need to turn them off in a certain order.  upsdrvctl
#          shuts down all the 0s, then the 1s, 2s, and so on.  To exclude
#          a UPS from the shutdown sequence, set this to -1.
#
#          The default value for this parameter is 0.
#
#  nolock: optional, and not recommended for use in this file.
#
#          If you put nolock in here, the driver will not lock the
#          serial port every time it starts.  This may allow other
#          processes to seize the port if you start more than one by
#          mistake.
#
#          This is only intended to be used on systems where locking
#          absolutely must be disabled for the software to work.
#
# maxstartdelay: optional.  This can be set as a global variable
#                above your first UPS definition and it can also be
#                set in a UPS section.  This value controls how long
#                upsdrvctl will wait for the driver to finish starting.
#                This keeps your system from getting stuck due to a
#                broken driver or UPS.
#
#                The default is 45 seconds.
#
#
# Anything else is passed through to the hardware-specific part of
# the driver.
#
# Examples
# --------
#
# A simple example for a UPS called "powerpal" that uses the fentonups
# driver on /dev/ttyS0 is:
#
# powerpal (
# 	driver (
# 		name = "fentonups"
#       parameter.port = "/dev/ttyS0"
#	)
#	desc = "Web server"
# )
#
# If your UPS driver requires additional settings, you can specify them
# in the driver.parameter section. For example, if it supports a setting
# of "1234" for the variable "cable", it would look like this:
#
# myups (
# 	driver (
# 		name = "mydriver"
# 		parameter (
#				port = "/dev/ttyS1"
#				cable = "1234"
#		)
#	)
#	desc = "Something descriptive"
# )
#
# To find out if your driver supports any extra settings, start it with
# the -h option and/or read the driver's documentation.
# --------------------------------------------------------------------------

\nut.users_desc
# Network UPS Tools: users section
#
# This section sets the permissions for upsd - the UPS network daemon.
# Users are defined here, are given passwords, and their privileges are
# controlled here too. 

# --------------------------------------------------------------------------

# Each user gets a section in the users section.
# The username is case-sensitive, so admin and AdMiN are two different users.
#
# Example for a user nammed "myuser"
#
# <myuser> (
#	type = "admin" 
#	password = "mypass"
#	allowfrom = { "localhost" "adminbox" }
# )

# Possible settings:
#
# password: The user's password.  This is case-sensitive. You should give
#           it r*w* or r* as right
#
# --------------------------------------------------------------------------
#
# allowfrom: ACL names that this user may connect from.  ACLs are
#            defined in upsd.conf.
#
# It is a list, so put the values between brace
#
# --------------------------------------------------------------------------
#
# actions: Let the user do certain things with upsd.
#
# Valid actions are:
#
# SET   - change the value of certain variables in the UPS
# FSD   - set the "forced shutdown" flag in the UPS
#
# It is a list, so put the values between brace
#
# --------------------------------------------------------------------------
#
# instcmds: Let the user initiate specific instant commands.  Use "ALL"
# to grant all commands automatically.  There are many possible
# commands, so use 'upscmd -l' to see what your hardware supports.  Here
# are a few examples:
#
# test.panel.start      - Start a front panel test
# test.battery.start    - Start battery test
# test.battery.stop     - Stop battery test
# calibrate.start       - Start calibration
# calibrate.stop        - Stop calibration
#
# It is a list, so put the values between brace
#
# --------------------------------------------------------------------------

#
# --- Configuring for upsmon
#
# To add a user for your upsmon, use this example:
#
# <monuser> (
#	type = "upsmon_master" (or "upsmon_slave")
# 	password  = "pass"
#   allowfrom = "bigserver"
#

# The matching monitor section in your upsmon section would look like this:
#
# monitor.myups@myhost (
#	powervalue = "1"
#	user = "monuser"
# )
# --------------------------------------------------------------------------

\nut.upsd.acl
# --------------------------------------------------------------------------
# Access Control Lists (ACLs)
#
# acl.<name> = "<ipblock>"
#
# acl (
# 	localhost = "10.0.0.1/32"
#	all = "0.0.0.0/0"
#)
# --------------------------------------------------------------------------

\nut.upsd.accept
# --------------------------------------------------------------------------
# accept = { "<aclname>" ["<aclname>"] ... }
#
# Define lists of hosts or networks with ACL definitions.
#
# accept use ACL definitions to control whether a host is
# allowed to connect to upsd.
#
# refer to acl section for defined acl name
# --------------------------------------------------------------------------

\nut.upsd.reject
# --------------------------------------------------------------------------
# reject = { "<aclname>" ["<aclname>"] ... }
#
# Define lists of hosts or networks with ACL definitions.
#
# reject use ACL definitions to control whether a host is
# not allowed to connect to upsd.
#
# refer to acl section for defined acl name
# --------------------------------------------------------------------------

\nut.upsd.maxage
# --------------------------------------------------------------------------
# maxage = "<seconds>"
# maxage = "15"
#
# This defaults to 15 seconds.  After a UPS driver has stopped updating
# the data for this many seconds, upsd marks it stale and stops making
# that information available to clients.  After all, the only thing worse
# than no data is bad data.
#
# You should only use this if your driver has difficulties keeping
# the data fresh within the normal 15 second interval.  Watch the syslog
# for notifications from upsd about staleness.
# --------------------------------------------------------------------------

\nut.upsmon.runasuser
# --------------------------------------------------------------------------
# runasuser <username>
#
# By default, upsmon splits into two processes.  One stays as root and
# waits to run the SHUTDOWNCMD.  The other one switches to another userid
# and does everything else.
#
# The default nonprivileged user is set at compile-time with
#       'configure --with-user=...'.
#
# You can override it with '-u <user>' when starting upsmon, or just
# define it here for convenience.
#
# Note: if you plan to use the reload feature, this file (upsmon.conf)
# must be readable by this user!  Since it contains passwords, DO NOT
# make it world-readable.  Also, do not make it writable by the upsmon
# user, since it creates an opportunity for an attack by changing the
# SHUTDOWNCMD to something malicious.
#
# For best results, you should create a new normal user like "nutmon",
# and make it a member of a "nut" group or similar.  Then specify it
# here and grant read access to the upsmon.conf for that group.
#
# This user should not have write access to upsmon.conf.
#
# runasuser "monuser"
# --------------------------------------------------------------------------

\nut.upsmon.monitor
# --------------------------------------------------------------------------
# monitor.<system> (
# 		powervalue = "<powervalue>"
#		user = "<username>"
# )
#
# List systems you want to monitor.  Not all of these may supply power
# to the system running upsmon, but if you want to watch it, it has to
# be in this section.
#
# You must have at least one of these declared.
#
# <system> is a UPS identifier in the form <upsname>@<hostname>[:<port>]
# like ups@localhost, su700@mybox, etc.
#
# Examples:
#
#  - "su700@mybox" means a UPS called "su700" on a system called "mybox"
#
#  - "fenton@bigbox:5678" is a UPS called "fenton" on a system called
#    "bigbox" which runs upsd on port "5678".
#
# The UPS names like "su700" and "fenton" are set in the ups section
# ( in ups trunk )
#
# If the ups.conf on host "doghouse" has a section called "snoopy", the
# identifier for it would be "snoopy@doghouse".
#
# <powervalue> is an integer - the number of power supplies that this UPS
# feeds on this system.  Most computers only have one power supply, so this
# is normally set to 1.  You need a pretty big or special box to have any
# other value here.
#
# You can also set this to 0 for a system that doesn't supply any power,
# but you still want to monitor.  Use this when you want to hear about
# changes for a given UPS without shutting down when it goes critical,
# unless <powervalue> is 0.
#
# <username> must match an entry in that system's users section ( in users
# trunk) that is of type upsmon_maser or upsmon_slave.
# If your upsmon user name is monmaster and he is of type upsmon_master
# you should have in your users section something like
# monmaster (
#		type = upsmon_master
#		password = "your_user_password"
# )
# --------------------------------------------------------------------------

\nut.upsmon.minsupplies
# --------------------------------------------------------------------------
# minsupplies "<num>"
#
# Give the number of power supplies that must be receiving power to keep
# this system running.  Most systems have one power supply, so you would
# put "1 " in this field.
#
# Large/expensive server type systems usually have more, and can run with
# a few missing.  The HP NetServer LH4 can run with 2 out of 4, for example,
# so you'd set that to 2.  The idea is to keep the box running as long
# as possible, right?
#
# Obviously you have to put the redundant supplies on different UPS circuits
# for this to make sense!  See big-servers.txt in the docs subdirectory
# for more information and ideas on how to use this feature.
# --------------------------------------------------------------------------

\nut.upsmon.shutdowncmd
# --------------------------------------------------------------------------
# shutdowncmd "<command>"
#
# upsmon runs this command when the system needs to be brought down.
#
# This should work just about everywhere ... if it doesn't, well, change it.
# --------------------------------------------------------------------------

\nut.upsmon.notifycmd
# --------------------------------------------------------------------------
# notifycmd "<command>"
#
# upsmon calls this to send messages when things happen
#
# This command is called with the full text of the message as one argument.
# The environment string NOTIFYTYPE will contain the type string of
# whatever caused this event to happen.
#
# Note that this is only called for NOTIFY events that have EXEC set with
# notifyflag.  See notifyflag below for more details.
#
# Making this some sort of shell script might not be a bad idea.  For more
# information and ideas, see pager.txt in the docs directory.
#
# Example:
# notifycmd "/usr/local/ups/bin/notifyme"
# --------------------------------------------------------------------------

\nut.upsmon.pollfreq
# --------------------------------------------------------------------------
# pollfreq "<n>"
#
# Polling frequency for normal activities, measured in seconds.
#
# Adjust this to keep upsmon from flooding your network, but don't make
# it too high or it may miss certain short-lived power events.
# --------------------------------------------------------------------------

\nut.upsmon.pollfreqalert
# --------------------------------------------------------------------------
# pollfreqalert "<n>"
#
# Polling frequency in seconds while UPS on battery.
#
# You can make this number lower than pollfreq, which will make updates
# faster when any UPS is running on battery.  This is a good way to tune
# network load if you have a lot of these things running.
#
# The default is 5 seconds for both this and pollfreq.
# --------------------------------------------------------------------------

\nut.upsmon.hostsync
# --------------------------------------------------------------------------
# hostsync "<n>"
#
# How long upsmon will wait before giving up on another upsmon
#
# The master upsmon process uses this number when waiting for slaves to
# disconnect once it has set the forced shutdown (FSD) flag.  If they
# don't disconnect after this many seconds, it goes on without them.
#
# Similarly, upsmon slave processes wait up to this interval for the
# master upsmon to set FSD when a UPS they are monitoring goes critical -
# that is, on battery and low battery.  If the master doesn't do its job,
# the slaves will shut down anyway to avoid damage to the file systems.
#
# This "wait for FSD" is done to avoid races where the status changes
# to critical and back between polls by the master.
# --------------------------------------------------------------------------

\nut.upsmon.deadtime
# --------------------------------------------------------------------------
# deadtime "<n>"
#
# Interval to wait before declaring a stale ups "dead"
#
# upsmon requires a UPS to provide status information every few seconds
# (see pollfreq and pollfreqalert) to keep things updated.  If the status
# fetch fails, the UPS is marked stale.  If it stays stale for more than
# deadtime seconds, the UPS is marked dead.
#
# A dead UPS that was last known to be on battery is assumed to have gone
# to a low battery condition.  This may force a shutdown if it is providing
# a critical amount of power to your system.
#
# Note: deadtime should be a multiple of pollfreq and pollfreqalert.
# Otherwise you'll have "dead" UPSes simply because upsmon isn't polling
# them quickly enough.  Rule of thumb: take the larger of the two
# pollfreq values, and multiply by 3.
# --------------------------------------------------------------------------

\nut.upsmon.powerdownflag
# --------------------------------------------------------------------------
# powerdownflag "<n>"
#
# Flag file for forcing UPS shutdown on the master system
#
# upsmon will create a file with this name in master mode when it's time
# to shut down the load.  You should check for this file's existence in
# your shutdown scripts and run 'upsdrvctl shutdown' if it exists.
#
# See the shutdown.txt file in the docs subdirectory for more information.
# --------------------------------------------------------------------------

\nut.upsmon.notifymsg
# --------------------------------------------------------------------------
# notifymsg - change messages sent by upsmon when certain events occur
#
# You can change the stock messages to something else if you like.
#
# notifymsg.<notify type> = "message"
#
# notifymsg (
# 	online = "UPS %s is getting line power"
# 	onbatt = "Someone pulled the plug on %s"
# )
#
# Note that %s is replaced with the identifier of the UPS in question.
#
# Possible values for <notify type>:
#
# online   : UPS is back online
# onbatt   : UPS is on battery
# lowbatt  : UPS has a low battery (if also on battery, it's "critical")
# fsd      : UPS is being shutdown by the master (FSD = "Forced Shutdown")
# commok   : Communications established with the UPS
# commbad  : Communications lost to the UPS
# shutdown : The system is being shutdown
# replbatt : The UPS battery is bad and needs to be replaced
# nocomm   : A UPS is unavailable (can't be contacted for monitoring)
# --------------------------------------------------------------------------

\nut.upsmon.notifyflag
# --------------------------------------------------------------------------
# notifyflag - change behavior of upsmon when NOTIFY events occur
#
# By default, upsmon sends walls (global messages to all logged in users)
# and writes to the syslog when things happen.  You can change this.
#
# notifyflag.<notify type> = "<flag>[+<flag>][+<flag>]"
#
# notifyflag ( 
# 	online = "SYSLOG"
# 	onbatt = "SYSLOG+WALL+EXEC"
# )
#
# Possible values for the flags:
#
# SYSLOG - Write the message in the syslog
# WALL   - Write the message to all users on the system
# EXEC   - Execute NOTIFYCMD (see above) with the message
# IGNORE - Don't do anything
#
# If you use IGNORE, don't use any other flags on the same line.
# --------------------------------------------------------------------------

\nut.upsmon.rbwarntime
# --------------------------------------------------------------------------
# rbwarntime "<n>" - replace battery warning time in seconds
#
# upsmon will normally warn you about a battery that needs to be replaced
# every 43200 seconds, which is 12 hours.  It does this by triggering a
# NOTIFY_REPLBATT which is then handled by the usual notify structure
# you've defined in the notifyflag section.
#
# If this number is not to your liking, override it here.
# --------------------------------------------------------------------------

\nut.upsmon.nocommwarntime
# --------------------------------------------------------------------------
# nocommwarntime "<n>" - no communications warning time in seconds
#
# upsmon will let you know through the usual notify system if it can't
# talk to any of the UPS entries that are defined in this file.  It will
# trigger a NOTIFY_NOCOMM by default every 300 seconds unless you
# change the interval with this directive.
# --------------------------------------------------------------------------

\nut.upsmon.finaldelay
# --------------------------------------------------------------------------
# finaldealy "<n>" - last sleep interval before shutting down the system
#
# On a master, upsmon will wait this long after sending the NOTIFY_SHUTDOWN
# before executing your SHUTDOWNCMD.  If you need to do something in between
# those events, increase this number.  Remember, at this point your UPS is
# almost depleted, so don't make this too high.
#
# Alternatively, you can set this very low so you don't wait around when
# it's time to shut down.  Some UPSes don't give much warning for low
# battery and will require a value of 0 here for a safe shutdown.
#
# Note: If FINALDELAY on the slave is greater than HOSTSYNC on the master,
# the master will give up waiting for the slave to disconnect.
# --------------------------------------------------------------------------

