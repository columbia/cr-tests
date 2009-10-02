#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

selinuxload() {
	if [ ! -d /usr/share/selinux/devel ]; then
		echo install selinux-policy-devel
		exit 1
	fi
	rm -rf cr-test-module
	cp -r /usr/share/selinux/devel cr-test-module
	rm -f cr-test-module/example.??
	rm -rf cr-test-module/tmp
	cp cr-tests-policy.* cr-test-module/
	# plug our dirname into the file contexts file
	dn=`pwd`
	echo "$dn/ckpt -- gen_context(system_u:object_r:ckpt_test_exec_t,s0)" \
		>> cr-test-module/cr-tests-policy.fc
	# allow our context to transition to the test dirs
	myrole=`cat /proc/self/attr/current |awk -F: '{ print $2 '}`
	myctx=`cat /proc/self/attr/current |awk -F: '{ print $3 '}`
	dirctx=`attr -qS -g selinux . | awk -F: '{ print $3 '}`
cat >> cr-test-module/cr-tests-policy.te << EOF
gen_require(\`
	role $myrole;
	type $myctx;
	type $dirctx;
')
ckpt_test_domtrans($myctx,$myrole)
allow $myctx ckpt_test_file_t:file rw_file_perms;
EOF
	dir=`pwd`
	stop=0
	while [ $stop -ne 1 ]; do
		dirctx=`attr -qS -g selinux $dir | awk -F: '{ print $3 '}`
cat >> cr-test-module/cr-tests-policy.te << EOF
gen_require(\`
	type $dirctx;
')
list_dirs_pattern(ckpt_test_domain,$dirctx,$dirctx)
EOF
		if [ $dir == "/" ]; then
			stop=1
		fi
		dir=`dirname $dir`
	done
	(cd cr-test-module; make; semodule -i cr-tests-policy.pp)
	ret=$?
	if [ $ret -ne 0 ]; then
		echo failed to load policy
	fi
	echo "policy loaded"
}

selinuxunload() {
	semodule -r cr-tests-policy
}

source ../common.sh
verify_freezer
verify_paths

cp `which restart` restart
selinuxload

rm -f ./cr-test.out out context

dirctx=`attr -qS -g selinux . | awk -F: '{ print $3 '}`
filctx=`attr -qS -g selinux ckpt.c | awk -F: '{ print $3 '}`
chcon -t ckpt_test_file_t .
chcon -t ckpt_test_exec_t ./restart
chcon -t ckpt_test_file_t ./ckpt
chcon -t ckpt_test_exec_t ./wrap

trap '\
setenforce 0; \
semodule -B; \
selinuxunload; \
echo "Unloaded selinux policy, exiting"; \
chcon -t $dirctx . ; \
chcon -t $filctx ./ckpt ; \
chcon -t $filctx ./context ; \
chcon -t $filctx ./cr-test.out; \
chcon -t $filctx ./wrap ; \
chcon -t $filctx ./out ; \
chcon -t $filctx ./restart ' EXIT

semodule -BD
setenforce 1

# create a checkpoint image with task as type ckpt_test_1_t
echo "Creating checkpoint image as ckpt_test_1_t"
runcon -t ckpt_test_1_t ./wrap ./ckpt

echo ab > context
chcon -t ckpt_test_file_t context

# restart from image starting as ckpt_test_2_t
# make sure it was restarted as ckpt_test_2_t
echo "Test 1: restart without KEEP_LSM and verify original task context"
runcon -t ckpt_test_2_t -- ./restart < out
ret=$?
if [ $ret -ne 0 ]; then
	echo "Restart failed, returned $ret"
	exit 1
fi
context=`cat context | awk -F: '{ print $3 '}`
if [ -z "$context" -o "$context" != "ckpt_test_2_t" ]; then
	echo "Fail, context was $context instead of ckpt_test_2_t"
	exit 1
fi
echo Pass

echo ab > context
chcon -t ckpt_test_file_t context
# restart with KEEP_LSM from image as ckpt_test_3_t
# make sure it fails
echo "Test 2: restart with KEEP_LSM from unauthorized context"
runcon -t ckpt_test_3_t -- ./restart -k < out
ret=$?
if [ $ret -eq 0 ]; then
	echo "Fail (return code $ret)"
	exit 1
fi
echo Pass

echo ab > context
chcon -t ckpt_test_file_t context
# restart with KEEP_LSM from image as ckpt_test2_t
# make sure it was restarted as ckpt_test_t
echo "Test 3: restart with KEEP_LSM and verify restored task context"
runcon -t ckpt_test_2_t -- ./restart -k < out
ret=$?
if [ $ret -ne 0 ]; then
	echo "Restart failed, returned $ret"
	exit 1
fi
context=`cat context | awk -F: '{ print $3 '}`
if [ -z "$context" -o "$context" != "ckpt_test_1_t" ]; then
	echo "Fail"
	exit 1
fi
echo Pass

# END that is it for tests define so far
echo "REST of tests are not yet implemented in policy, exiting."
echo "All tests passed."
exit 0

cg=${freezermountpoint}/1
mkdir -p $cg

# restart from type ckpt_test3_t which creates files of type ckpt_testf2_t
# make sure open file is ckpt_testf2_t
echo "Test 4: restart without KEEP_LSM and verify open file context"
runcon -t ckpt_test3_t -- ./restart -F $cg < out
sleep 1
pid=`pidof ckpt`
context=`ls -lZ /proc/$pid/fd | grep cr-test.out | awk '{ print $3 '}`
thaw
if [ -z "$context" -o "$context" != "ckpt_testf2_t" ]; then
	echo "Fail"
	exit 1
fi
echo Pass
