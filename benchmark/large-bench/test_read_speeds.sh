#!/bin/bash

# We can use seq but I want the values visible here.
PREPARE_TOTALS="\
    20
    30
    40
    50
    60
    70
    80
    90
    100"

BUCKETS_LOAD="\
    0.4
    0.6
    0.8
    1.0
    1.2
    1.4
    1.6
    1.8
    2.0
    2.2
    2.4
    2.6
    2.8
    3.0
    4.0"

READ_ITERATIONS="\
    10
    20
    40
    60
    80
    100
    120
    140
    160
    180
    200
    250
    300
    400
    600"

million() {
    multiply $1 1000000
}

multiply() {
    python -c "print int($1 * $2)"
}

show_date() {
    date +%F_%R
}

prepare() {
    for TOTAL in $PREPARE_TOTALS; do
        echo "Preparing $TOTAL million txs..."
        TOTAL=$(million $TOTAL)
        ./prepare $TOTAL
        write $TOTAL
    done
    echo "Finished $(show_date)."
}

write() {
    TOTAL=$1
    rm -fr leveldb.db
    # Write LevelDB database once.
    echo "Writing LevelDB database..."
    ./leveldb_write
    echo "Performing reads for LevelDB and None..."
    bench_reads leveldb
    bench_reads null
    for LOAD in $BUCKETS_LOAD; do
        BUCKETS=$(multiply $LOAD $TOTAL)
        echo "Writing htdb_slab database (a = $LOAD)..."
        ./htdb_slab_write $BUCKETS
        bench_reads htdb_slab
    done
}

bench_reads() {
    COMMAND="./$1_read"
    for ITERATIONS in $READ_ITERATIONS; do
        ITERATIONS=$(million $ITERATIONS)
        ../drop_caches
        $COMMAND $ITERATIONS
    done
}

FILENAME="output.$(show_date).log"
prepare | tee $FILENAME
echo "Test output written to $FILENAME"

