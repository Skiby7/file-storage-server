#!/bin/bash

CWD=$(realpath $(dirname $0))


bin/client -f /tmp/socket.sk -w ${CWD}/small_files,1 -x -p -t 200 

bin/client -f /tmp/socket.sk -w ${CWD}/small_files,1 -x -p -t 200 

