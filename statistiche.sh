if [ $# -eq 0 ]
  then
    echo "Usare -> ./statistiche.sh path/to/log"
    exit 1
fi

echo -e -n "Read effettuate -> "
grep -w "read" $1 | wc -l

echo -e -n "Write effettuate -> "
grep -w "wrote" $1 | wc -l

echo -e -n "Lock effettuate -> "
grep -w "locked" $1 | wc -l

echo -e -n "Unlock effettuate -> "
grep -w "unlocked" $1 | wc -l

echo -e -n "Evictions effettuate -> "
grep -w "evictions" $1 | awk '{print $NF}'

echo -n "Dimensione media scritture -> "
grep -w 'wrote' $1 | awk '{SUM += $(NF-1); COUNT += 1} END {print int(SUM/COUNT) " bytes"}'

echo -n "Dimensione media letture -> "
grep -w 'read' $1 | awk '{SUM += $(NF-1); COUNT += 1} END {print int(SUM/COUNT) " bytes"}'