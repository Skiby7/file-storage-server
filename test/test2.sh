#!/bin/bash

bin/server bin/config1.txt &

export SERVER=$!

echo $SERVER

bash -c "sleep 5 && kill -1 ${SERVER}" &

STOP_SERVER=$!
sleep 1
# Write some files
bin/client -f /tmp/socket.sk -W test/large_files/large_0.txt -u test/large_files/large_0.txt -p

bin/client -f /tmp/socket.sk -r /home/leonardo/Documents/SO/Project/file-storage-server/test/large_files/large_0.txt -p 






wait $SERVER
wait $STOP_SERVER

exit 0