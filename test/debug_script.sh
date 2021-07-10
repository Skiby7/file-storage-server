#!/bin/bash



bin/client -f /tmp/socket.sk -w test/medium_files,0 -x -p &

bin/client -f /tmp/socket.sk -w test/small_files,0 -x -p &

sleep 5

/home/leonardo/Documents/SO/Project/file-storage-server/statistiche.sh /home/leonardo/Documents/SO/Project/file-storage-server/bin/server.log




exit 0