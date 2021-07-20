#!/bin/bash

CWD=$(realpath $(dirname $0))

valgrind --leak-check=full bin/server bin/config1.txt &

export SERVER=$!

bash -c "sleep 10 && kill -1 ${SERVER}" &

STOP_SERVER=$!
sleep 1
# Write some files
# bin/client -f /tmp/socket.sk -w ${CWD}/small_files,10 -x -p -t 200 

# bin/client -f /tmp/socket.sk -R 0 -d ${CWD}/test_output -p -t 200

bin/client -f /tmp/socket.sk -W ${CWD}/medium_files/medium_0.txt -u ${CWD}/medium_files/medium_0.txt  -p -t 200 

bin/client -f /tmp/socket.sk -r ${CWD}/medium_files/medium_0.txt -d ${CWD}/medium_files -p -t 200 

bin/client -f /tmp/socket.sk -l ${CWD}/medium_files/medium_0.txt -u ${CWD}/medium_files/medium_0.txt -p -t 2000 &

bin/client -f /tmp/socket.sk -l ${CWD}/medium_files/medium_0.txt -c ${CWD}/medium_files/medium_0.txt -p -t 200 



wait $SERVER
wait $STOP_SERVER

exit 0