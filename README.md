[![Build Status](https://travis-ci.org/libbitcoin/libbitcoin-blockchain.svg?branch=master)](https://travis-ci.org/libbitcoin/libbitcoin-blockchain)

[![Coverage Status](https://coveralls.io/repos/libbitcoin/libbitcoin-blockchain/badge.svg)](https://coveralls.io/r/libbitcoin/libbitcoin-blockchain)

# Libbitcoin Blockchain

*Bitcoin blockchain library*

Make sure you have installed [libbitcoin-database](https://github.com/libbitcoin/libbitcoin-database) and [libbitcoin-consensus](https://github.com/libbitcoin/libbitcoin-consensus) (optional) beforehand according to their respective build instructions.

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

## Installation

### Macintosh

#### Using Homebrew

##### Installing from Formula

Instead of building, libbitcoin can be installed from a formula:
```sh
$ brew install libbitcoin-blockchain
```
