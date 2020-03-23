#!/bin/bash
echo "Inizio test"


echo "RUN SUPERVISOR"

./supervisor 8 >>supervisor_out 2>>supervisor_err &

x=$!

sleep 2

echo "Eseguo i 20 client"

for((i = 0; i < 10 ; i++)); do
		
	./client 5 8 20 >>clientout.log &
	./client 5 8 20 >>clientout.log &
	sleep 1
done

echo "Sono stati eseguiti tutti i client"

echo "Passeranno adesso 60 secondi prima che il SUPERVISOR venga chiuso"

 
for (( i = 0 ; i < 6 ; i++)); do
	sleep 10
	echo "WAIT"
	kill -SIGINT $x
done

kill -SIGINT $x
kill -SIGINT $x

echo "Il SUPERVISOR è stato chiuso correttamente"


bash ./misura.sh supervisor_out clientout.log
