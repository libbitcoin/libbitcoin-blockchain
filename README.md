[![Build Status](https://travis-ci.org/libbitcoin/libbitcoin-blockchain.svg?branch=master)](https://travis-ci.org/libbitcoin/libbitcoin-blockchain)
[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2Fcrypto0sy80%2Flibbitcoin-blockchain.svg?type=shield)](https://app.fossa.io/projects/git%2Bgithub.com%2Fcrypto0sy80%2Flibbitcoin-blockchain?ref=badge_shield)

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


## License
[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2Fcrypto0sy80%2Flibbitcoin-blockchain.svg?type=large)](https://app.fossa.io/projects/git%2Bgithub.com%2Fcrypto0sy80%2Flibbitcoin-blockchain?ref=badge_large)