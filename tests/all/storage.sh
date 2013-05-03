. tests/func.inc.sh

tmpStoragePath="$TMP_DIR"/test.storage.btree

$CMD -t "$tmpStoragePath" < $TEST_ROOT/storage.data

md5=$(MD5 "$tmpStoragePath")
if [ "$md5" != '799aadfda752d649231e48b99643ee66' ]; then
	exit 1
fi
