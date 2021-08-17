#!/bin/bash

CWD=$(realpath $(dirname $0))

valgrind --fair-sched=yes --leak-check=full bin/server $1 &
# bin/server $1 &

SERVER=$!

echo -e -n "\nAvvio il server \033[5m*\033[0m\n\n"

sleep 3

bin/client -f /tmp/socket.sk -w ${CWD}/small_files,10 -x -p -t 200 

bin/client -f /tmp/socket.sk -w ${CWD}/small_files,10 -x -p -t 200 

bin/client -f /tmp/socket.sk -w ${CWD}/small_files,10 -x -p -t 200 

bin/client -f /tmp/socket.sk -R 0 -d ${CWD}/test_output -p -t 200

bin/client -f /tmp/socket.sk -W ${CWD}/medium_files/medium_0.txt -u ${CWD}/medium_files/medium_0.txt -p -t 200 

bin/client -f /tmp/socket.sk -r ${CWD}/medium_files/medium_0.txt -d ${CWD}/test_output -p -t 200 

bin/client -f /tmp/socket.sk -l ${CWD}/medium_files/medium_0.txt -u ${CWD}/medium_files/medium_0.txt -p -t 2000 &

bin/client -f /tmp/socket.sk -l ${CWD}/medium_files/medium_0.txt -c ${CWD}/medium_files/medium_0.txt -p -t 200 

echo -e "\n\x1b[33mEseguo un file binario prima di inviarlo al server\x1b[0m"

${CWD}/binary/binary_test Originale

echo ""

bin/client -f /tmp/socket.sk -W ${CWD}/binary/binary_test -u ${CWD}/binary/binary_test -p -t 200

bin/client -f /tmp/socket.sk -r ${CWD}/binary/binary_test -d ${CWD}/test_output -p -t 200 

echo -e "\n\x1b[33mEseguo il file binario dopo averlo letto dal server\x1b[0m"

${CWD}/test_output${CWD}/binary/binary_test Scaricato

echo ""

kill -1 $SERVER

echo -e -n "\nSIGHUP inviato al server, attendo \033[5m*\033[0m\n\n"

wait $SERVER

exit 0