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

