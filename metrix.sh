set -e

ROWS=${1:-2000}
COLS=${2:-2000}
KSIZE=${3:-5}
K=${4:-5}
RUNS=3
THREAD_LIST="1 2 3 4 6 8 12 16"

echo "# params: ${ROWS}x${COLS}, ksize=$KSIZE, K=$K, runs=$RUNS"
echo "# nproc: $(nproc)"
echo "p,T,S,E"

T1=999999
for r in $(seq $RUNS); do
    T=$(echo "${ROWS} ${COLS} ${KSIZE} ${K} 1" | ./main | grep 'Time:' | awk '{print $2}')
    T1=$(awk -v a=$T1 -v b=$T 'BEGIN{print (b<a)?b:a}')
done
S1=1.000
E1=1.000
echo "1,$T1,$S1,$E1"

for P in $THREAD_LIST; do
    [ "$P" = "1" ] && continue
    BEST=999999
    for r in $(seq $RUNS); do
        T=$(echo "${ROWS} ${COLS} ${KSIZE} ${K} ${P}" | ./main | grep 'Time:' | awk '{print $2}')
        BEST=$(awk -v a=$BEST -v b=$T 'BEGIN{print (b<a)?b:a}')
    done
    S=$(awk -v t1=$T1 -v tp=$BEST 'BEGIN{printf "%.3f", t1/tp}')
    E=$(awk -v s=$S -v p=$P 'BEGIN{printf "%.3f", s/p}')
    echo "$P,$BEST,$S,$E"
done