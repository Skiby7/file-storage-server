#!/bin/bash

LINE_UP='\033[A'
CWD=$(realpath $(dirname $0))

echo -e "\n"
mkdir ${CWD}/test &> /dev/null
mkdir ${CWD}/test/small_files &> /dev/null
for i in {0..99}
do
	base64 /dev/urandom | head -c 1024 > ${CWD}/test/small_files/small_${i}.txt
	NUM=$((i+1))
	echo -e "${LINE_UP}Generati ${NUM} file piccoli"
done
chmod 777 -R ${CWD}/test/small_files/
echo ""


mkdir ${CWD}/test/medium_files &> /dev/null
for i in {0..9}
do
	base64 /dev/urandom | head -c 1000000 > ${CWD}/test/medium_files/medium_${i}.txt
	NUM=$((i+1))
	echo -e "${LINE_UP}Generati ${NUM} file medi"
done
chmod 777 -R ${CWD}/test/medium_files
echo ""

mkdir ${CWD}/test/large_files &> /dev/null
for i in {0..4}
do
	base64 /dev/urandom | head -c 30000000 > ${CWD}/test/large_files/large_${i}.txt
	NUM=$((i+1))
	echo -e "${LINE_UP}Generati ${NUM} file grandi"
done
chmod 777 -R ${CWD}/test/large_files
echo ""
mkdir ${CWD}/test/test_2 &> /dev/null

base64 /dev/urandom | head -c 400000 > ${CWD}/test/test_2/eviction_file_0.txt
base64 /dev/urandom | head -c 400000 > ${CWD}/test/test_2/eviction_file_1.txt
base64 /dev/urandom | head -c 400000 > ${CWD}/test/test_2/eviction_file_2.txt
base64 /dev/urandom | head -c 500000 > ${CWD}/test/test_2/initial_file_0.txt
echo -e "${LINE_UP}Generati i file per il test 2"

chmod 777 -R ${CWD}/test/test_2/
mkdir ${CWD}/test/output_stress_test &> /dev/null
mkdir ${CWD}/test/test_output &> /dev/null
echo -e "Fatto!\n"