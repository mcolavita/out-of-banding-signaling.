#!/bin/bash

SUPER_OUT=$1
CLIENT_OUT=$2
DIFF=0

echo "****************************************"
echo "  	 
        	<   Misure    >
         	 -------------"
echo "****************************************"

tail -n 21 $SUPER_OUT | grep -v EXITING | awk '{ print $3,$5}' | while read row; do
	ESTIMATE_SECRET=$(echo $row | awk '{print $1}')
	CLIENT_ID=$(echo $row | awk '{print $2}')
	T_CLIENT_SECRET=$(grep $CLIENT_ID $CLIENT_OUT | grep -v DONE |awk '{print $NF}')
	DIFF=$(expr "$T_CLIENT_SECRET" - "$ESTIMATE_SECRET")
	ABS=$(echo ${DIFF#-})
	if [[ "$ABS" -le "25" ]]; then
		echo "VALID SECRET: Il secret $ESTIMATE_SECRET, stimato dal Supervisor, differisce di $ABS unità rispetto al secret generato dal Client $CLIENT_ID ($T_CLIENT_SECRET)"
	else
		echo "INVALID SECRET: Il secret $ESTIMATE_SECRET, stimato dal Supervisor, differisce di $ABS unità rispetto al secret generato dal Client $CLIENT_ID ($T_CLIENT_SECRET)"
	fi
done

rm -f OOB*
