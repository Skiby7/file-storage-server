#!/bin/bash
CWD=$(realpath $(dirname $0))

bin/server bin/config2.txt &

export SERVER=$!

echo $SERVER

bash -c "sleep 5 && kill -1 ${SERVER}" &

STOP_SERVER=$!
sleep 1
# Write some files
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt  -p  

bin/client -f /tmp/socket.sk -r /home/leonardo/Documents/SO/Project/file-storage-server/test/test_2/initial_file_0.txt -p 
sleep 2
bin/client -f /tmp/socket.sk -w ${CWD}/small_files,12 -x -p




wait $SERVER
wait $STOP_SERVER

exit 0