#
# Regular cron jobs for the mpdev package
#
0 4	* * *	root	[ -x /usr/bin/mpdev_maintenance ] && /usr/bin/mpdev_maintenance
