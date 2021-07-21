if [ $# -eq 0 ]
  then
    echo "Usare -> ./statistiche.sh path/to/log"
    exit 1
fi


echo -e -n "Open effettuate -> "
grep -w "Opened" $1 | wc -l

echo -e -n "Open-lock effettuate -> "
grep -w "Open-locked" $1 | wc -l

echo -e -n "Close effettuate -> "
grep -w "Closed" $1 | wc -l

echo -e -n "Read effettuate -> "
grep -w "Read" $1 | wc -l

echo -e -n "Write effettuate -> "
grep -w "Wrote" $1 | wc -l

echo -e -n "Lock effettuate -> "
grep -w "Locked" $1 | wc -l

echo -e -n "Unlock effettuate -> "
grep -w "Unlocked" $1 | wc -l

echo -e -n "Evictions effettuate -> "
grep -w "evictions" $1 | awk '{print $NF}'

echo -e -n "Numero massimo di clients connessi contemporaneamente -> "
grep -w "active" $1 | awk '{print $NF}'

echo -e -n "Dimensione massima raggiunta dallo storage (bytes) -> "
grep -w "Max size reached" $1 | awk '{print $NF}'

echo -e -n "Numero massimo di files presenti nello storage -> "
grep -w "Max file num reached" $1 | awk '{print $NF}'

echo -n "Dimensione media scritture -> "
grep -w "Wrote" $1 | awk '{SUM += $(NF-1); COUNT += 1} END {print (COUNT==0) ? "0 bytes" : int(SUM/COUNT) " bytes"}'

echo -n "Dimensione media letture -> "
grep -w "Read" $1 | awk '{SUM += $(NF-1); COUNT += 1} END {print (COUNT==0) ? "0 bytes" : int(SUM/COUNT) " bytes"}'

grep -w "Thread" $1 | awk '{count[$6]++} END {for (i in count) print ">> Il thread " i " ha servito " count[i] " richieste"}'