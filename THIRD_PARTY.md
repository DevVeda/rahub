# Third-party software

RA Hub is licensed under **GPL-3.0-or-later** (`LICENSE`). The ELF links or embeds:

| Component | License | Use |
|---|---|---|
| [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk) | GPL-3.0-or-later (BSD for `include/freebsd`) | Runtime and system stubs |
| [libcurl](https://curl.se/) | [curl](https://curl.se/docs/copyright.html) (MIT-style) | HTTP/HTTPS |
| [OpenSSL 3](https://www.openssl.org/) | [Apache-2.0](https://www.openssl.org/source/license.html) | TLS |
| [zlib](https://zlib.net/) | [zlib](https://zlib.net/zlib_license.html) | Compression |
| [zstd](https://github.com/facebook/zstd) | [BSD-3-Clause](https://github.com/facebook/zstd/blob/dev/LICENSE) | Compression |
| [libpsl](https://github.com/rockdaboot/libpsl) | [MIT](https://github.com/rockdaboot/libpsl/blob/master/LICENSE) | Cookie domain checks (curl) |
| Mozilla CA certificates (`build/cacert.pem`, copied from `/etc/ssl/cert.pem`) | [MPL-2.0](https://www.mozilla.org/MPL/2.0/) | HTTPS verification; source: [Mozilla CA program](https://wiki.mozilla.org/CA) |

The libraries come prebuilt with the ps5-payload-sdk; their full license texts ship with their sources at the links above.

RetroAchievements is a trademark of its owners. This project is not affiliated with RetroAchievements; it uses their public API. Icons and pages are original work under this project's license.
