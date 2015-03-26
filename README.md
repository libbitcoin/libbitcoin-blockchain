[![Build Status](https://travis-ci.org/libbitcoin/libbitcoin-blockchain.svg?branch=master)](https://travis-ci.org/libbitcoin/libbitcoin-blockchain)

# Libbitcoin Blockchain

*Fast Bitcoin blockchain database based on libbitcoin*

Note that you need g++ 4.8 or higher. For this reason Ubuntu 12.04 and older are not supported.

Make sure you have installed [libbitcoin](https://github.com/libbitcoin/libbitcoin) beforehand according to its build instructions.

To use Satoshi consensus code, install [libbitcoin-consensus](https://github.com/libbitcoin/libbitcoin-consensus). To use native consensus code, add `--without-consensus` to `configure`.

```sh
$ ./autogen.sh
$ ./configure
$ make
$ sudo make install
$ sudo ldconfig
```

libbitcoin-blockchain is now installed in `/usr/local/`.
