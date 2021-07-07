#!/bin/bash

bin/server bin/config.txt &

SERVER=$!

echo $SERVER

bash -c "sleep 3 && kill -2 $SERVER" &

STOP_SERVER=$!

# Write some files
bin/client -f /tmp/socket.sk -w test_set,0 -x -p -t 100 &

echo 'FILES WRITTEN'
bin/client -f /tmp/socket.sk -R 0 -d /home/leonardo/Documents/SO/Project/file-storage-server/test_out -p -t 100 &

echo 'FILES READ'
bin/client -f /tmp/socket.sk -W /home/leonardo/Documents/SO/Project/file-storage-server/test_set/README.md -p -t 100 &
echo 'FILE APPEND'

bin/client -f /tmp/socket.sk -r /home/leonardo/Documents/SO/Project/file-storage-server/test_set/README.md -d /home/leonardo/Documents/SO/Project/file-storage-server/test_out -p -t 100 &
echo 'FILE READ'

wait $S_PID
wait $STOP_SERVER
