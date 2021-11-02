## yggkeygen - vanity address generator for yggdrasil

This tool generates vanity [yggdrasil][] addresses.

### Requirements

* C99 compatible compiler (gcc and clang should work)
* libsodium (including headers)
* GNU make
* GNU autoconf (to generate configure script, needed only if not using release tarball)
* UNIX-like platform (currently tested in Linux and OpenBSD, but should
  also build under cygwin and msys2).

For debian-like linux distros, this should be enough to prepare for building:

```bash
apt install gcc libsodium-dev make autoconf
```

### Building

`./autogen.sh` to generate configure script, if it's not there already.

`./configure` to generate makefile; in \*BSD platforms you probably want to use
`./configure CPPFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib"`.

On AMD64 platforms, you probably also want to pass something like
`--enable-amd64-51-30k`  to configure script for faster key generation;
run `./configure --help` to see all available options.

Finally, `make` to start building (`gmake` in \*BSD platforms).

### Usage

**TODO**

### Contact

For bug reports/questions/whatever else, email cathugger at cock dot li.\

### Acknowledgements & Legal

To the extent possible under law, the author(s) have dedicated all
copyright and related and neighboring rights to this software to the
public domain worldwide. This software is distributed without any
warranty.
You should have received a copy of the CC0 Public Domain Dedication
along with this software. If not, see [CC0][].

* `ed25519/{ref10,amd64-51-30k,amd64-64-24k}` are adopted from
  [SUPERCOP][]
* `ed25519/ed25519-donna` adopted from [ed25519-donna][]
* Idea used in `worker_fast()` is stolen from [horse25519][]
* base64 routines and initial YAML processing work contributed by
  Alexander Khristoforov (heios at protonmail dot com)
* Passphrase-based generation code and idea used in `worker_batch()`
  contributed by [foobar2019][]

[yggdrasil]: https://github.com/yggdrasil-network/yggdrasil-go
[CC0]: https://creativecommons.org/publicdomain/zero/1.0/
[SUPERCOP]: https://bench.cr.yp.to/supercop.html
[ed25519-donna]: https://github.com/floodyberry/ed25519-donna
[horse25519]: https://github.com/Yawning/horse25519
[foobar2019]: https://github.com/foobar2019
