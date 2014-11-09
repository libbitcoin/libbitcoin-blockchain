[![Build Status](https://travis-ci.org/libbitcoin/libbitcoin-blockchain.svg?branch=version2)](https://travis-ci.org/libbitcoin/libbitcoin-blockchain)

# Libbitcoin Blockchain

*Fast Bitcoin blockchain database based on libbitcoin*

Note that you need g++ 4.7 or higher. For this reason Ubuntu 12.04 and older are not supported. Make sure you have installed [libbitcoin](https://github.com/libbitcoin/libbitcoin) beforehand according to its build instructions.

```sh
$ ./autogen.sh
$ ./configure
$ make
$ sudo make install
$ sudo ldconfig
```

libbitcoin-node is now installed in `/usr/local/`.