MD5 () {
	if which md5 2> /dev/null; then
		$(which md5) -q $*
	else
		$(which md5sum) $* | cut -d' ' -f1
	fi
}
