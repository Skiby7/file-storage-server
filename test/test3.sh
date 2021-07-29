

bin/server bin/config3.txt &

export SERVER=$!
sleep 2
stress_test_pids=()
for i in {1..10}; do
    bash -c './test/stress_test.sh' &
    stress_test_pids+=($!)
    sleep 0.1
done
 
sleep 10
kill -2 ${SERVER}
wait $SERVER
for i in "${stress_test_pids[@]}"; do
    kill -9 ${i} &> /dev/null
    wait ${i} &> /dev/null
done


# $(pidof client) | xargs kill -9 2> /dev/null

exit 0