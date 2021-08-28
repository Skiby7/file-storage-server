#!/bin/bash
CWD=$(realpath $(dirname $0))
MAGENTA="\033[35m"
RESET="\033[0m"

echo -e "${MAGENTA}>> Test con politica FIFO <<${RESET}"

bin/server bin/config2_FIFO.txt &

SERVER=$!

sleep 1
# Write some files
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt  -p  

# bin/client -f /tmp/socket.sk -r /home/leonardo/Documents/SO/Project/file-storage-server/test/test_2/initial_file_0.txt -p 
sleep 1
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -u ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  
for i in {0..6}
do
	bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p   
done 
echo -e "\n\n${MAGENTA}Espulso initial_file_0.txt (output ls -l):${RESET}"
ls -l ${CWD}/test_output/${CWD}/test_2
echo ""
echo ""
sleep 2
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -u ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  
for i in {0..8}
do
	bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p   
done

echo -e "\n\n${MAGENTA}Espulso eviction_file_0.txt (output ls -l):${RESET}"
ls -l ${CWD}/test_output/${CWD}/test_2
echo ""
echo ""
sleep 2

kill -1 $SERVER

wait $SERVER

echo -e "${MAGENTA}Pulisco test_output${RESET}"

rm -rf ${CWD}/test_output/*


sleep 2



echo -e "\n\n${MAGENTA}>> Test con politica LRU <<${RESET}"

bin/server bin/config2_LRU.txt &
sleep 1
echo -e "\n${MAGENTA}Carico initial_file_0.txt${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt  -p  
sleep 1

echo -e "\n${MAGENTA}Carico eviction_file_0.txt${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -u ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  
sleep 1

echo -e "\n${MAGENTA}Utilizzo initial_file_0.txt e provoco un'espulsione${RESET}\n"
bin/client -f /tmp/socket.sk -r ${CWD}/test_2/initial_file_0.txt -p 2> /dev/null


bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -u ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  
for i in {0..5}
do
	bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p   
done
 
echo -e "\n\n${MAGENTA}Espulso eviction_file_0.txt (output ls -l):${RESET}"
ls -l ${CWD}/test_output/${CWD}/test_2
echo ""
echo ""
SERVER=$!
kill -1 $SERVER
wait $SERVER

echo -e "${MAGENTA}Pulisco test_output${RESET}"

rm -rf ${CWD}/test_output/*
sleep 2

echo -e "\n\n${MAGENTA}>> Test con politica LFU <<${RESET}"

bin/server bin/config2_LFU.txt &
sleep 1
echo -e "\n${MAGENTA}Carico initial_file_0.txt (40kb)${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt  -p  
sleep 1

echo -e "\n${MAGENTA}Carico eviction_file_0.txt (20kb)${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -u ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  

sleep 1

echo -e "\n${MAGENTA}Carico eviction_file_1.txt (20kb)${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -u ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  

sleep 1

echo -e "\n${MAGENTA}Carico eviction_file_2.txt (20kb)${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -u ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output  -p  

sleep 1

echo -e "\n${MAGENTA}Leggo initial_file_0.txt 15 volte e eviction_file_0.txt 7 volte${RESET}\n"
sleep 2
for i in {0..14}
do
	bin/client -f /tmp/socket.sk -r ${CWD}/test_2/initial_file_0.txt -p 2> /dev/null 
done

for i in {0..6}
do
	bin/client -f /tmp/socket.sk -r ${CWD}/test_2/eviction_file_0.txt -p 2> /dev/null  
done


echo -e "\n${MAGENTA}Provoco la prima espulsione scrivendo in evction_file_1.txt${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
sleep 1


echo -e "\n\n${MAGENTA}Espulso eviction_file_2.txt (output ls -l):${RESET}"
ls -l ${CWD}/test_output/${CWD}/test_2
echo ""
echo ""
sleep 2

echo -e "\n${MAGENTA}Leggo di nuovo da initial_file_0.txt e provoco la seconda espulsione scrivendo in evction_file_1.txt${RESET}\n"
for i in {0..14}
do
	bin/client -f /tmp/socket.sk -r ${CWD}/test_2/initial_file_0.txt -p 2> /dev/null 
done

bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
sleep 1


echo -e "\n\n${MAGENTA}Espulso eviction_file_0.txt (output ls -l):${RESET}"
ls -l ${CWD}/test_output/${CWD}/test_2
echo ""
echo ""
sleep 2

SERVER=$!
kill -1 $SERVER
wait $SERVER
echo -e "\n\n${MAGENTA}Pulisco test_output${RESET}"

rm -rf ${CWD}/test_output/*
sleep 2

echo -e "\n\n${MAGENTA}>> Test con politica LRFU <<${RESET}"

bin/server bin/config2_LRFU.txt &
sleep 1
echo -e "\n${MAGENTA}Carico initial_file_0.txt (40kb)${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt  -p  
sleep 1

echo -e "\n${MAGENTA}Carico eviction_file_0.txt (10kb)${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -u ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  

sleep 1

echo -e "\n${MAGENTA}Carico eviction_file_1.txt (10kb)${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -u ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  

sleep 1

echo -e "\n${MAGENTA}Carico eviction_file_2.txt (10kb)${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -u ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output  -p  

sleep 1

echo -e "\n${MAGENTA}Leggo initial_file_0.txt 15 aspetto 5 secondi e leggo eviction_file_0.txt${RESET}\n"
sleep 2
for i in {0..14}
do
	bin/client -f /tmp/socket.sk -r ${CWD}/test_2/initial_file_0.txt -p 2> /dev/null 
done

for i in {0..6}
do
	bin/client -f /tmp/socket.sk -r ${CWD}/test_2/eviction_file_0.txt -p 2> /dev/null  

done


echo -e "\n${MAGENTA}Provoco la prima espulsione scrivendo in eviction_file_1.txt${RESET}\n"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
sleep 1

exit 0