CWD=$(realpath $(dirname $0))


bin/client -f /tmp/socket.sk -w ${CWD}/small_files -x -p  &
bin/client -f /tmp/socket.sk -w ${CWD}/medium_files -x -p  &
# bin/client -f /tmp/socket.sk -R 0 -p  &
# bin/client -f /tmp/socket.sk -w ${CWD}/medium_files -x -p  &
# bin/client -f /tmp/socket.sk -R 10 -p  &
bin/client -f /tmp/socket.sk -w ${CWD}/small_files -x -p  &
bin/client -f /tmp/socket.sk -w ${CWD}/small_files -x -p  &
# bin/client -f /tmp/socket.sk -w ${CWD}/large_files -x -p  &
bin/client -f /tmp/socket.sk -w ${CWD}/small_files -x -p  &
# bin/client -f /tmp/socket.sk -R 0 -p  &
bin/client -f /tmp/socket.sk -w ${CWD}/medium_files -x -p  &
bin/client -f /tmp/socket.sk -w ${CWD}/small_files -x -p  &
# bin/client -f /tmp/socket.sk -R 50 -p  &
# bin/client -f /tmp/socket.sk -w ${CWD}/large_files -x -p  &
bin/client -f /tmp/socket.sk -w ${CWD}/small_files -x -p  &
# bin/client -f /tmp/socket.sk -w ${CWD}/medium_files -x -p  &
# bin/client -f /tmp/socket.sk -R 0 -p  &
bin/client -f /tmp/socket.sk -w ${CWD}/small_files -x -p  


# bin/client -f /tmp/socket.sk -r /home/leonardo/Documents/SO/Project/file-storage-server/test/test_2/initial_file_0.txt -p
# sleep 2
# bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_files_0.txt,${CWD}/test_2/eviction_files_1.txt -D ${CWD}/test_output -u ${CWD}/test_2/evictions_file_0.txt,${CWD}/test_2/evictions_file_1.txt   -p  
# bin/client -f /tmp/socket.sk -W ${CWD}/test_2/eviction_file_0.txt,${CWD}/test_2/eviction_file_1.txt,${CWD}/test_2/eviction_file_2.txt -D ${CWD}/test_output -u ${CWD}/test_2/eviction_file_0.txt,${CWD}/test_2/eviction_file_1.txt,${CWD}/test_2/eviction_file_2.txt  -p  


