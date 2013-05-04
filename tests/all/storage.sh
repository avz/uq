. tests/func.inc.sh

tmpStoragePath="$TMP_DIR"/test.storage.btree

rm "$tmpStoragePath"
$CMD -ct "$tmpStoragePath" < $TEST_ROOT/storage.data > /dev/null

md5=$(MD5 "$tmpStoragePath")
if [ "$md5" != '0f07ab5edb8196a5e02ff91c71449419' ]; then
	echo "Invalid checksum $md5"
	exit 1
fi
