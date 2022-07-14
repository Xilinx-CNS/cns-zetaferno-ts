# ZETAFERNO-TS - test suite for testing TCPDirect public API

The testing is dedicated to examine TCPDirect network stack behavior and API basing on the public API, i.e. it is a kind of requirements based coverage.
Most tests are designed to check correct using of the API.
However, some points of invalid using, especially which are not obvious invalid, also tried to be covered.

The tests cover:
* Sending and receiving multicast packets
* TCPDirect multiplexer API
* Overlapped API
* Performance
* TCP zocket API
* TCPDirect Timestamps API
* UDP RX zocket API
* UDP TX zocket API
* TCPDirect API for alternative queues
* TCPDirect Delegated Sends API

OKTET Labs TE is used as an Engine that allows to prepare desired environment for every test.
This guarantees reproducible test results.

## License

See the LICENSE file in this repository