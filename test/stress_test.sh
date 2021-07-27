CWD=$(realpath $(dirname $0))

while true
do
	SMALL_FILE_NUM=$((RANDOM % 100))
	MEDIUM_FILE_NUM=$((RANDOM % 10))
	bin/client -f /tmp/socket.sk -w ${CWD}/small_files/small_${SMALL_FILE_NUM}.dat -u ${CWD}/small_files/small_${SMALL_FILE_NUM}.dat -R 5 -d ${CWD}/output_stress_test -W ${CWD}/medium_files/medium_${MEDIUM_FILE_NUM}.dat -u ${CWD}/medium_files/medium_${MEDIUM_FILE_NUM}.dat -W ${CWD}/test_2/initial_file_0.dat -u ${CWD}/test_2/initial_file_0.dat -l ${CWD}/test_2/initial_file_0.dat -c ${CWD}/test_2/initial_file_0.dat -t 500 &> /dev/null
	# bin/client -f /tmp/socket.sk -l ${CWD}/test_2/initial_file_0.dat -c ${CWD}/test_2/initial_file_0.dat &> /dev/null
	if [[ $(($RANDOM % 2)) -eq 0 ]]
	then
    	bin/client -f /tmp/socket.sk -l ${CWD}/small_files/small_${SMALL_FILE_NUM}.dat -c ${CWD}/small_files/small_${SMALL_FILE_NUM}.dat &> /dev/null
	else
    	bin/client -f /tmp/socket.sk -l ${CWD}/medium_files/medium_${MEDIUM_FILE_NUM}.dat -c ${CWD}/medium_files/medium_${MEDIUM_FILE_NUM}.dat &> /dev/null

	fi


done