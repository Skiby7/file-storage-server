#!/bin/bash

bin/client -f /tmp/socket.sk -w test/test_2,0 -x -p

bin/client -f /tmp/socket.sk -r /home/leonardo/Documents/SO/Project/file-storage-server/test/large_files/initial_file_0.txt -p 

bin/client -f /tmp/socket.sk -w test/small_files, 10 -x -p




exit 0