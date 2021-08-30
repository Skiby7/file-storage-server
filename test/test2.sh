#!/bin/bash
CWD=$(realpath $(dirname $0))
MAGENTA="\033[35m"
RESET="\033[0m"
SEP="\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n"




### TEST CON PILITICA FIFO ###

echo -e "${MAGENTA}Pulisco test_output${RESET}"
rm -rf ${CWD}/test_output/*
sleep 2
echo -e "${MAGENTA}>> Test con politica FIFO <<${RESET}"

bin/server test/config/config2_FIFO.txt &

SERVER=$!

sleep 1
# Carico il primo file
echo -e "\n${MAGENTA}Carico initial_file_0.txt (40kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt  -p  
sleep 1

# Carico il secondo file
echo -e "\n${MAGENTA}Carico eviction_file_0.txt (60kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -u ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  
for i in {0..4}
do
	bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p   
done 
sleep 1

# Carico un file che provochi un'espulsione
echo -e "\n${MAGENTA}Carico eviction_file_1.txt (20kb) provocando un'espulsione${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -u ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -p  
echo -e "\n\n${MAGENTA}Espulso initial_file_0.txt (output ls -l):${RESET}"
ls -l ${CWD}/test_output/${CWD}/test_2
echo ""
echo ""
sleep 2

# Carico un altro file che provochi un'altra espulsione
echo -e "\n${MAGENTA}Carico eviction_file_2.txt (40kb) provocando un'altra espulsione${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -u ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output  -p  
for i in {0..2}
do
	bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output  -p   
done

echo -e "\n\n${MAGENTA}Espulso eviction_file_0.txt (output ls -l):${RESET}"
ls -l ${CWD}/test_output/${CWD}/test_2
echo ""
echo ""
sleep 2

kill -1 $SERVER
wait $SERVER




### TEST CON PILITICA LRU ###

echo -e "${SEP}${MAGENTA}Pulisco test_output${RESET}"
rm -rf ${CWD}/test_output/*

sleep 2



echo -e "${MAGENTA}>> Test con politica LRU <<${RESET}"

bin/server test/config/config2_LRU.txt &
SERVER=$!
sleep 1

# Carico il file iniziale
echo -e "\n${MAGENTA}Carico initial_file_0.txt (40kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt  -p  
sleep 1

# Carico un altro file
echo -e "\n${MAGENTA}Carico eviction_file_0.txt (20kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -u ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  
sleep 1

# Leggo il file iniziale così da aggiornare il timestamp dell'ultimo accesso e carico il file che provocherà un'espulsione
echo -e "\n${MAGENTA}Utilizzo initial_file_0.txt e provoco un'espulsione caricando eviction_file_1.txt (60kb)${RESET}"
bin/client -f /tmp/socket.sk -r ${CWD}/test_2/initial_file_0.txt -p 2> /dev/null
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -u ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  
for i in {0..4}
do
	bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p   
done
 
echo -e "\n\n${MAGENTA}Espulso eviction_file_0.txt (output ls -l):${RESET}"
ls -l ${CWD}/test_output/${CWD}/test_2
echo ""
echo ""

kill -1 $SERVER
wait $SERVER

### TEST CON POLITIVA LFU ###
echo -e "${SEP}${MAGENTA}Pulisco test_output${RESET}"

rm -rf ${CWD}/test_output/*
sleep 2

echo -e "${MAGENTA}>> Test con politica LFU <<${RESET}"

bin/server test/config/config2_LFU.txt &
SERVER=$!
sleep 1


# Carico il file iniziale
echo -e "\n${MAGENTA}Carico initial_file_0.txt (40kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt  -p  
sleep 1

# Carico un altro file
echo -e "\n${MAGENTA}Carico eviction_file_0.txt (20kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -u ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  
sleep 1

# Carico un altro file
echo -e "\n${MAGENTA}Carico eviction_file_1.txt (20kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -u ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  
sleep 1

# Carico un altro file
echo -e "\n${MAGENTA}Carico eviction_file_2.txt (20kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -u ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output  -p  

sleep 1

# Aggiorno use_stat di initial_file_0.txt in modo che sia maggiore di quello di eviction_file_0.txt
echo -e "\n${MAGENTA}Leggo initial_file_0.txt 15 volte e eviction_file_0.txt 7 volte${RESET}"
sleep 2
for i in {0..14}
do
	bin/client -f /tmp/socket.sk -r ${CWD}/test_2/initial_file_0.txt -p 2> /dev/null 
done

for i in {0..6}
do
	bin/client -f /tmp/socket.sk -r ${CWD}/test_2/eviction_file_0.txt -p 2> /dev/null  
done

# Aumento la dimensione di eviction_file_1.txt per provocare un'espulsione
echo -e "\n${MAGENTA}Provoco la prima espulsione scrivendo in eviction_file_1.txt (+10kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
sleep 1


echo -e "\n\n${MAGENTA}Espulso eviction_file_2.txt poiché use_stat(eviction_file_2.txt) < use_stat(eviction_file_0.txt) (output ls -l):${RESET}"
ls -l ${CWD}/test_output/${CWD}/test_2
echo ""
echo ""
sleep 2

echo -e "\n${MAGENTA}Provoco una seconda espulsione scrivendo di nuovo in eviction_file_1.txt${RESET}"
# for i in {0..14}
# do
# 	bin/client -f /tmp/socket.sk -r ${CWD}/test_2/initial_file_0.txt -p 2> /dev/null 
# done

bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output -p  
sleep 1


echo -e "\n\n${MAGENTA}Espulso eviction_file_0.txt (output ls -l):${RESET}"
ls -l ${CWD}/test_output/${CWD}/test_2
echo ""
echo ""
sleep 2


kill -1 $SERVER
wait $SERVER
echo -e "${SEP}${MAGENTA}Pulisco test_output${RESET}"

rm -rf ${CWD}/test_output/*
sleep 2

echo -e "${MAGENTA}>> Test con politica LRFU <<${RESET}"

bin/server test/config/config2_LRFU.txt &
SERVER=$!
sleep 1
echo -e "\n${MAGENTA}Carico initial_file_0.txt (40kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt  -p  
sleep 1

echo -e "\n${MAGENTA}Carico eviction_file_0.txt (20kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -u ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt -D ${CWD}/test_output  -p  

sleep 1

echo -e "\n${MAGENTA}Carico eviction_file_1.txt (20kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -u ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_1.txt -D ${CWD}/test_output  -p  

sleep 1

echo -e "\n${MAGENTA}Carico eviction_file_2.txt (20kb)${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -u ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output  -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output  -p  

sleep 1

echo -e "\n${MAGENTA}Leggo initial_file_0.txt 15 volte, aspetto 5 secondi e leggo 3 volte eviction_file_0.txt e una volta eviction_file_1.txt${RESET}"
sleep 2
for i in {0..14}
do
	bin/client -f /tmp/socket.sk -r ${CWD}/test_2/initial_file_0.txt -p 2> /dev/null 
done
for i in {0..4}
do
	echo -n "."
	sleep 1
done
echo ""
echo ""

for i in {0..2}
do
	bin/client -f /tmp/socket.sk -r ${CWD}/test_2/eviction_file_0.txt -p 2> /dev/null  
done

bin/client -f /tmp/socket.sk -r ${CWD}/test_2/eviction_file_1.txt -p 2> /dev/null  

echo -e "\n${MAGENTA}Provoco la prima espulsione scrivendo in eviction_file_2.txt${RESET}"
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output -p  
bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output -p  
sleep 1

echo -e "\n\n${MAGENTA}Espulso eviction_file_0.txt pur avendo lo stesso valore di 'use_stat' di eviction_file_1.txt\npoiché quest'ultimo è stato letto più recentemente.\ninitial_file_0.txt, invece, pur non essendo stato letto da un po', ha un valore di 'use_stat' maggiore rispetto a entrambi (output ls -l):${RESET}"
ls -l ${CWD}/test_output/${CWD}/test_2
echo ""
echo ""
sleep 2

kill -1 $SERVER
wait $SERVER

exit 0