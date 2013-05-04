#!/bin/sh

testDir=$(pwd)/$(dirname $0)/all
rootDir=$(pwd)/$(dirname $0)/..

cd "$testDir"

fail=""

export TEST_ROOT="$testDir"
export TMP_DIR="/tmp"
export CMD="./uq"

echo "Running tests"

for test in *.sh; do

	echo -n " - $test ... "

	cd "$rootDir"

	output=$(sh "$testDir/$test" 2>&1)
	retCode=$?

	if [ "$retCode" = "0" ]; then
		echo "ok"
	else
		echo "failed. Error code $retCode, output:"
		echo "$output" | sed -e 's/^/   	/'

		fail="$fail $test"
	fi

	cd "$testDir"
done

if [ -z "$fail" ]; then
	echo "done"
	exit 0
else
	echo "FAILED:$fail"
	exit 1
fi
