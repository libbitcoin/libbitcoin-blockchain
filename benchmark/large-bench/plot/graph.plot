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

set title "Performance 100 million values stored"
set xlabel "Gets (millions)"
set xrange [10: 600]
set ylabel "Time taken (microsecs)"
set yrange [0: 20000000]
set output "gets.png"
plot "gets.data" using 1:2 title "LevelDB" with linespoints, \
     "gets.data" using 1:3 title "None" with linespoints, \
     "gets.data" using 1:4 title "ht (a=1.0)" with linespoints

set title "Different load factors with 100 million values stored"
set xlabel "Load factor (a)"
set xrange [0.4: 4.0]
set ylabel "Time taken (microsecs)"
set yrange [0: 1000000]
set output "buckets.png"
plot "buckets.data" using 1:2 title "ht (100 million gets)" with linespoints, \
     "buckets.data" using 1:3 title "ht (200 million gets)" with linespoints, \
     "buckets.data" using 1:4 title "ht (400 million gets)" with linespoints, \
     "buckets.data" using 1:5 title "ht (600 million gets)" with linespoints

