set term png size 1024,768
set style fill solid 1

set title "Performance 600 million gets"
set xlabel "Values stored (millions)"
set xrange [20: 100]
set ylabel "Time taken (microsecs)"
set yrange [0: 20000000]
set output "store.png"
plot "store.data" using 1:2 title "LevelDB" with linespoints, \
     "store.data" using 1:3 title "None" with linespoints, \
     "store.data" using 1:4 title "ht (a=1.0)" with linespoints

