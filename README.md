# RA Hub

**One RetroAchievements login for your whole jailbroken PS5.**

A background payload that signs in to [retroachievements.org](https://retroachievements.org) once and shares the session with other homebrew apps.

<p align="center"><img src="assets/icon0.png" width="128" alt="RA Hub icon"></p>

<p align="center"><a href="https://ko-fi.com/T1K329H7HC"><img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support me on Ko-fi"></a></p>

## Highlights

- **Sign in once.** Apps get your session from `http://127.0.0.1:8045/session`.
- **Profile overview for the TV.** Points, rank, now playing, awards, recent games and achievements.
- **In the Media tab** as **RA Hub**, or at `http://<ps5-ip>:8045` from any device.
- **Few requests.** Data is cached and fetched only while the page is open.
- **No stored password.** Only the session token is kept.

## Install

Needs [Payload Manager](https://github.com/itsPLK/ps5-payload-manager) and an FTP server on the PS5.

1. Download `rahub.elf` from the [latest release](../../releases/latest).
2. Copy it over FTP to `/data/pldmgr/payloads/local/rahub/rahub.elf`.
3. Launch **rahub** from Payload Manager, open **RA Hub** in the Media tab and sign in. For the overview, add your Web API key from [retroachievements.org/settings](https://retroachievements.org/settings).

**Security:** your home network is trusted. The page uses plain HTTP, so your password crosses it once when you sign in; on networks you don't trust, sign in from the console.

## For app developers

```sh
curl http://127.0.0.1:8045/session
# {"user":"YourName","token":"…","key":"…"}
```

`token` is the Connect token, `key` the Web API key. Only answers on the console itself. **RA Hub Test** (`rahub-test.elf` in the release) is a reference client.

## Build

Needs [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk), LLVM 18+ and Node (tests). Works on macOS and Linux (Windows via WSL); the system root certificates are embedded automatically, or pass `CACERT=/path/to/cacert.pem`.

```sh
make                                # build/rahub.elf, build/rahub-test.elf
make test                           # page check (Node)
make install PS5_HOST=<ps5-ip>      # upload to Payload Manager over FTP (install-test for the test app)
```

`src/` C (`hub.c`, `test_app.c` + shared modules) · `web/` pages · `assets/` icons · `tests/` page check

**Translations:** copy `web/lang/en.json` to `web/lang/<code>.json` (e.g. `fr.json`, `pt-BR.json`) and rebuild. The console language picks it; missing texts fall back to English.

## AI disclosure

AI was used creating this piece of software, use at own discretion.

## License

GPL-3.0-or-later. See [THIRD_PARTY.md](THIRD_PARTY.md) for bundled software.
