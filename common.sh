verify_freezer()
{
	line=`grep freezer /proc/mounts`
	if [ $? -ne 0 ]; then
		echo "please mount freezer cgroup"
		echo "  mkdir /cgroup"
		echo "  mount -t cgroup -o freezer freezer /cgroup"
		exit 1
	fi
	freezermountpoint=`echo $line | awk '{ print $2 '}`
	mkdir -p $freezermountpoint/1 > /dev/null 2>&1
}

verify_paths()
{
	which checkpoint > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		echo "BROK: checkpoint not in path"
		exit 1
	fi
	which restart > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		echo "BROK: restart not in path"
		exit 1
	fi
}
verify_freezer
verify_paths

freeze()
{
	d=${freezermountpoint}/1
	echo FROZEN > $d/freezer.state
	while [ `cat $d/freezer.state` != "FROZEN" ]; do
		echo FROZEN > $d/freezer.state
	done
}

thaw()
{
	d=${freezermountpoint}/1
	echo THAWED > $d/freezer.state
	cat $d/freezer.state > /dev/null
}

get_ltp_user()
{
	awk -F: '{ print $1 }' /etc/passwd | grep "\<ltp\>"
	if [ $? -ne 0 ]; then
		echo "I refuse to mess with your password file"
		echo "please create a user named ltp"
		uid=-1
	else
		uid=`grep "\<ltp\>" /etc/passwd | awk -F: '{ print $3 }'`
	fi
}

handlesigusr1()
{
	echo "FAIL: timed out"
	exit 1
}

trap handlesigusr1 SIGUSR1 
timerpid=0

canceltimer()
{
	if [ $timerpid -ne 0 ]; then
		kill -9 $timerpid > /dev/null 2>&1
	fi
}

settimer()
{
	(sleep $1; kill -s USR1 $$) &
	timerpid=`jobs -p | tail -1`
}

CHECKPOINT=`which checkpoint`
if [ $? -ne 0 ]; then
	echo "BROK: checkpoint not found in your path"
	exit 1
fi
RESTART=`which restart`
if [ $? -ne 0 ]; then
	echo "BROK: restart not found in your path"
	exit 1
fi
