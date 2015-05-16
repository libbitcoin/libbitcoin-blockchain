[![Build Status](https://travis-ci.org/libbitcoin/libbitcoin-blockchain.svg?branch=master)](https://travis-ci.org/libbitcoin/libbitcoin-blockchain)

[![Coverage Status](https://coveralls.io/repos/libbitcoin/libbitcoin-blockchain/badge.svg)](https://coveralls.io/r/libbitcoin/libbitcoin-blockchain)

# Libbitcoin Blockchain

*Fast Bitcoin blockchain database based on libbitcoin*

Note that you need g++ 4.8 or higher. For this reason Ubuntu 12.04 and older are not supported.

Make sure you have installed [libbitcoin](https://github.com/libbitcoin/libbitcoin) and [libbitcoin-consensus](https://github.com/libbitcoin/libbitcoin-consensus) beforehand according to their respective build instructions.

```sh
$ ./autogen.sh
$ ./configure
$ make
$ sudo make install
$ sudo ldconfig
```

libbitcoin-blockchain is now installed in `/usr/local/`.

## Configure Options

The default configuration requires `libbitcoin-consensus`. This ensures consensus parity with the Satoshi client. To eliminate the `libbitcoin-consensus` dependency use the `--without-consensus` option. This results in use of `libbitcoin` consensus checks.
