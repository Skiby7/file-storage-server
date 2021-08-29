CWD=$(realpath $(dirname $0))

while true
do
	if [[ $1 = "DELETE" ]]
	then
	for i in {0..50}
	do
		bin/client -f /tmp/socket.sk -l ${CWD}/medium_files/small_${i}.txt -c ${CWD}/medium_files/medium_${i}.txt -p &> /dev/null
		bin/client -f /tmp/socket.sk -l ${CWD}/small_files/small_${i}.txt -c ${CWD}/small_files/small_${i}.txt -p &> /dev/null
	done
    	
	fi

	if [[ $1 = "WRITE" ]]
	then
	for i in {0..50}
	do
		bin/client -f /tmp/socket.sk -W ${CWD}/medium_files/small_${i}.txt -u ${CWD}/medium_files/medium_${i}.txt -p &> /dev/null
		bin/client -f /tmp/socket.sk -W ${CWD}/small_files/small_${i}.txt -u ${CWD}/small_files/small_${i}.txt -p &> /dev/null
	done
	fi

	if [[ $1 = "READ" ]]
	then
	for i in {0..9}
	do
		bin/client -f /tmp/socket.sk -r ${CWD}/small_files/small_${i}.txt -p &> /dev/null
		bin/client -f /tmp/socket.sk -r ${CWD}/medium_files/medium_${i}.txt -p &> /dev/null
		bin/client -f /tmp/socket.sk -R 0
	done
	fi
	sleep 1
done