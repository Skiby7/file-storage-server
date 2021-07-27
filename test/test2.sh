#!/bin/bash
CWD=$(realpath $(dirname $0))

bin/server bin/config2.txt &

export SERVER=$!


bash -c "sleep 5 && kill -1 ${SERVER}" &

STOP_SERVER=$!
sleep 1
# Write some files
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/initial_file_0.dat -u ${CWD}/test_2/initial_file_0.dat  -p  

# bin/client -f /tmp/socket.sk -r /home/leonardo/Documents/SO/Project/file-storage-server/test/test_2/initial_file_0.dat -p 
sleep 2
# bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_files_0.dat,${CWD}/test_2/eviction_files_1.dat -D ${CWD}/test_output -u ${CWD}/test_2/evictions_file_0.dat,${CWD}/test_2/evictions_file_1.dat   -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.dat,${CWD}/test_2/eviction_file_1.dat,${CWD}/test_2/eviction_file_2.dat -D ${CWD}/test_output -u ${CWD}/test_2/eviction_file_0.dat  -p  






wait $SERVER
wait $STOP_SERVER

exit 0