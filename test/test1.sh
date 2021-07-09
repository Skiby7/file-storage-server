#!/bin/bash

valgrind --leak-check=full bin/server bin/config1.txt &

export SERVER=$!

echo $SERVER

bash -c "sleep 7 && kill -1 ${SERVER}" &

STOP_SERVER=$!
sleep 1
# Write some files
bin/client -f /tmp/socket.sk -w test_set,0 -x -p -t 200 

# bin/client -f /tmp/socket.sk -R 0 -d /home/leonardo/Documents/SO/Project/file-storage-server/test_out -p -t 200 &

bin/client -f /tmp/socket.sk -W /home/leonardo/Documents/SO/Project/file-storage-server/test_set/README.md -u /home/leonardo/Documents/SO/Project/file-storage-server/test_set/README.md  -p -t 200 

bin/client -f /tmp/socket.sk -r /home/leonardo/Documents/SO/Project/file-storage-server/test_set/README.md -d /home/leonardo/Documents/SO/Project/file-storage-server/test_out -p -t 200 

bin/client -f /tmp/socket.sk -l /home/leonardo/Documents/SO/Project/file-storage-server/test_set/README.md -u /home/leonardo/Documents/SO/Project/file-storage-server/test_set/README.md -p -t 2000 &

bin/client -f /tmp/socket.sk -l /home/leonardo/Documents/SO/Project/file-storage-server/test_set/README.md -c /home/leonardo/Documents/SO/Project/file-storage-server/test_set/README.md -p -t 200 



wait $SERVER
wait $STOP_SERVER

exit 0