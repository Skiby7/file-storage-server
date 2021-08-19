#!/bin/bash
CWD=$(realpath $(dirname $0))

while true
do
	SMALL_FILE_NUM=$((RANDOM % 100))
	MEDIUM_FILE_NUM=$((RANDOM % 10))
	LARGE_FILE_NUM=$((RANDOM % 5))
	bin/client -f /tmp/socket.sk -W ${CWD}/small_files/small_${SMALL_FILE_NUM}.txt -u ${CWD}/small_files/small_${SMALL_FILE_NUM}.txt -R 5 -W ${CWD}/medium_files/medium_${MEDIUM_FILE_NUM}.txt -u ${CWD}/medium_files/medium_${MEDIUM_FILE_NUM}.txt -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt -l ${CWD}/test_2/initial_file_0.txt -c ${CWD}/test_2/initial_file_0.txt -t 0 &> /dev/null
	
	bin/client -f /tmp/socket.sk -W ${CWD}/large_files/large_${LARGE_FILE_NUM}.txt -u ${CWD}/large_files/large_${LARGE_FILE_NUM}.txt &> /dev/null
	bin/client -f /tmp/socket.sk -w ${CWD}/medium_files,3 -x &> /dev/null
	bin/client -f /tmp/socket.sk -w ${CWD}/small_files,30 -x &> /dev/null

	
done