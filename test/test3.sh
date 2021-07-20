

bin/server bin/config3.txt &
SERVER=$!
export SERVER
bash -c 'sleep 30 && kill -2 ${SERVER}' &

stress_test_pids=()
for i in {1..10}; do
    bash -c './test/stress_test.sh' &
    stress_test_pids+=($!)
    sleep 0.1
done

sleep 30

for i in "${stress_test_pids[@]}"; do
    kill -9 ${i}
    wait ${i}
done

wait ${SERVER}
kill -9 $(pidof client)
exit 0