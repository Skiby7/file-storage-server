stress_test_pids=()
for i in {1..10}; do
    bash -c './test/stress_test.sh' &
    stress_test_pids+=($!)
    sleep 0.1
done

sleep 20

for i in "${stress_test_pids[@]}"; do
    kill -9 ${i} &> /dev/null
    wait ${i} &> /dev/null
done


# $(pidof client) | xargs kill -9 2> /dev/null

# CWD=$(realpath $(dirname $0))

# for i in {1..50}
# do
# 	SMALL_FILE_NUM=$((RANDOM % 100))
# 	MEDIUM_FILE_NUM=$((RANDOM % 10))
# 	bin/client -f /tmp/socket.sk -w ${CWD}/small_files/small_${SMALL_FILE_NUM}.txt -u ${CWD}/small_files/small_${SMALL_FILE_NUM}.txt -R 5 -d ${CWD}/output_stress_test -W ${CWD}/medium_files/medium_${MEDIUM_FILE_NUM}.txt -u ${CWD}/medium_files/medium_${MEDIUM_FILE_NUM}.txt -W ${CWD}/test_2/initial_file_0.txt -u ${CWD}/test_2/initial_file_0.txt &> /dev/null
# 	bin/client -f /tmp/socket.sk -l ${CWD}/test_2/initial_file_0.txt -c ${CWD}/test_2/initial_file_0.txt &> /dev/null
# 	if [[ $(($RANDOM % 2)) -eq 0 ]]
# 	then
#     	bin/client -f /tmp/socket.sk -l ${CWD}/small_files/small_${SMALL_FILE_NUM}.txt -c ${CWD}/small_files/small_${SMALL_FILE_NUM}.txt &> /dev/null
# 	else
#     	bin/client -f /tmp/socket.sk -l ${CWD}/medium_files/medium_${SMALL_FILE_NUM}.txt -c ${CWD}/medium_files/medium_${SMALL_FILE_NUM}.txt &> /dev/null

# 	fi
#     sleep 1
# done

exit 0