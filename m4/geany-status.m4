dnl GEANY_STATUS_ADD(description, value)
dnl Add a status message to be displayed by GEANY_STATUS_OUTPUT
AC_DEFUN([GEANY_STATUS_ADD],
[
	_GEANY_STATUS="$_GEANY_STATUS
$1:$2"
])

dnl GEANY_STATUS_OUTPUT
dnl Nicely displays all messages registered with GEANY_STATUS_ADD
AC_DEFUN([GEANY_STATUS_OUTPUT],
[
	# Count the max lengths
	dlen=0
	vlen=0
	while read l; do
		d=`echo "$l" | cut -d: -f1`
		v=`echo "$l" | cut -d: -f2`
		dl=${#d}
		vl=${#v}
		test $dlen -lt $dl && dlen=$dl
		test $vlen -lt $vl && vlen=$vl
	done << EOF
$_GEANY_STATUS
EOF

	# Print a nice top bar
	# description + ' : ' + value
	total=`expr $dlen + 3 + $vlen`
	for i in `seq 1 $total`; do echo -n '-'; done
	echo

	# And print the actual content
	# format is:
	#  key1       : value1
	#  second key : second value
	while read l; do
		test -z "$l" && continue
		d=`echo "$l" | cut -d: -f1`
		v=`echo "$l" | cut -d: -f2`
		printf '%-*s : %s\n' $dlen "$d" "$v"
	done << EOF
$_GEANY_STATUS
EOF
])
