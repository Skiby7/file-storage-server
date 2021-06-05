#!/bin/bash

for((i = 0; i < 10; i++));
do
	bin/client  -f /tmp/sockfile.sk < input > client_output &
	# sleep 1
done
echo "Requests done"
