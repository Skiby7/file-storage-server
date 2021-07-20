while true
do
	bin/client -f /tmp/socket.sk -w small_files,10 -x -R 10 -d test/output_stress_test -W test/medium_files/medium_0.txt -u test/medium_files/medium_0.txt -W test/test_2/initial_file_0.txt -u test/medium_files/initial_file_0.txt
	bin/client -f /tmp/socket.sk -l test/test_2/initial_file_0.txt -c test/medium_files/initial_file_0.txt
done