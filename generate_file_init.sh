#!/bin/bash

LINE_UP='\033[A'
CWD=$(realpath $(dirname $0))

echo -e "\n"
mkdir ${CWD}/test &> /dev/null
mkdir ${CWD}/test/small_files &> /dev/null
for i in {0..99}
do
	head -c 1024 < /dev/urandom > ${CWD}/test/small_files/small_${i}.txt
	echo -e "${LINE_UP}Generati ${i+1} files"
done
chmod 777 -R ${CWD}/test/small_files/
echo ""


mkdir ${CWD}/test/medium_files
for i in {0..9}
do
	head -c 1000000 < /dev/urandom > ${CWD}/test/medium_files/medium_${i}.txt
	echo -e "${LINE_UP}Generati ${i+1} files"
done
chmod 777 -R ${CWD}/test/medium_files
echo ""

mkdir ${CWD}/test/large_files
for i in {0..4}
do
	head -c 50000000 < /dev/urandom > ${CWD}/test/large_files/large_${i}.txt
	echo -e "${LINE_UP}Generati ${i+1} files"
done
chmod 777 -R ${CWD}/test/large_files/
echo -e "Fatto!\n"