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
	mkdir $freezermountpoint/1 > /dev/null 2>&1
}

verify_paths()
{
	which ckpt > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		echo "BROK: ckpt not in path"
		exit 1
	fi
	which rstr > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		echo "BROK: rstr not in path"
		exit 1
	fi
	which mktree > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		echo "BROK: mktree not in path"
		exit 1
	fi
}

freeze()
{
	echo FROZEN > ${freezermountpoint}/1/freezer.state
}

thaw()
{
	echo THAWED > ${freezermountpoint}/1/freezer.state
}

get_ltp_user()
{
	awk -F: '{ print $1 '} /etc/passwd | grep "\<ltp\>"
	if [ $? -ne 0 ]; then
		echo "I refuse to mess with your password file"
		echo "please create a user named ltp"
		uid=-1
	fi
	uid=`grep "\<ltp\>" /etc/passwd | awk -F: '{ print $3 '}`
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
