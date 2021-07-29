stress_test_pids=()
for i in {1..15}; do
    bash -c './test/stress_test.sh' &
    stress_test_pids+=($!)
    sleep 0.1
done

sleep 30
SERVER=$(pidof server)

for i in "${stress_test_pids[@]}"; do
    kill -9 ${i} &> /dev/null
    wait ${i} 
done
kill -2 $SERVER
echo "SIGNAL SENT TO SERVER"
wait $SERVER

$(pidof client) | xargs kill -9 {} &> /dev/null

exit 0