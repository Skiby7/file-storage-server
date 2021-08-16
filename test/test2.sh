#!/bin/bash
CWD=$(realpath $(dirname $0))

bin/server $1 &

export SERVER=$!


bash -c "sleep 5 && kill -1 ${SERVER}" &

STOP_SERVER=$!
sleep 1
# Write some files
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt  -p  

# bin/client -f /tmp/socket.sk -r /home/leonardo/Documents/SO/Project/file-storage-server/test/test_2/initial_file_0.txt -p 
sleep 2
# bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_files_0.txt,${CWD}/test_2/eviction_files_1.txt -D ${CWD}/test_output -u ${CWD}/test_2/evictions_file_0.txt,${CWD}/test_2/evictions_file_1.txt   -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt,${CWD}/test_2/eviction_file_1.txt,${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output -u ${CWD}/test_2/eviction_file_0.txt  -p  






wait $SERVER
wait $STOP_SERVER

exit 0