#!/bin/bash

for((i = 0; i < 5; i++));
do
	bin/client  -f /tmp/sockfile.sk < input &

done
echo "Requests done"
