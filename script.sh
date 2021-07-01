#!/bin/bash

for((i = 0; i < 10; i++));
do
	bin/client -f /tmp/socket.sk -w test_set,0 -x -R 0 -d /home/leonardo/Documents/SO/Project/file-storage-server/test_out -p -t 1000
bin/client -f /tmp/socket.sk -l a.out -c a.out -p -t 1000	
# sleep 1
done
echo "Requests done"
