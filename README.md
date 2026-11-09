# Mashup Share (Geode mod for Geometry Dash)

Upload your own song mashup for any level and replace its music. **Everyone can upload one mashup per level**,
and anyone can browse them and pick which one to play.

```
mashup-share/
├── mod.json  CMakeLists.txt  logo.png  about.md  changelog.md
├── resources/mashup-icon.png
├── src/            <- the Geode mod (C++)
├── server/         <- the upload/download backend (Node.js)
└── .github/workflows/build.yml   <- builds the .geode for every platform
```

## How it works
1. Open any online level's info page → tap the green **Mashup** button (left side menu).
2. **Upload mine** → choose an `.mp3 / .ogg / .wav` (max 20 MB). Uploading again replaces your own.
3. The list shows everyone's mashups for that level. Tap **Use** to download and select one.
4. Start the level – the mashup plays instead of the original song. **Original** switches back.

## 1) Run the server
```bash
cd server
npm install
npm start                    # http://localhost:3000
# optional: ADMIN_KEY=secret PORT=3000 DATA_DIR=/var/mashups npm start
```
Or with Docker: `docker build -t mashup-server server && docker run -p 3000:3000 -v mashups:/data mashup-server`

Host it somewhere public (VPS, Railway, Render, Fly.io…). **Put it behind HTTPS** for real use and
set the address in the mod's settings (`Server URL`). Remove a bad upload with:
`curl -X DELETE -H "X-Admin-Key: secret" https://your-server/api/admin/mashups/<levelID>/<accountID>`

## 2) Build the mod
Prerequisites (needs a **C++23** compiler): [Geode SDK + CLI](https://docs.geode-sdk.org/getting-started/), CMake, a C++ toolchain.
```bash
geode sdk install        # first time only
geode build              # produces yourname.mashup-share.geode
```
Drop the `.geode` file into your GD `geode/mods` folder (or install via the Geode menu).
No toolchain? Push this folder to GitHub – the included workflow builds Windows / macOS / Android for you.

Before publishing, change `"id"` and `"developer"` in `mod.json`.

## Notes & limitations
- Written for **Geode v5** and **GD 2.2081** (v5.11.2 was the latest when this was made). It will **not** load on
  2.2074 / Geode 4.x. Geode 5.11.2 also has a forward-compat mode for 2.209, but that is untested here.
- Run `geode sdk update` then `geode project update` so `"geode"` in `mod.json` matches your installed SDK.
- The code has **not been compiled** by the author. If the build reports an error, it is most likely in the
  `FMODAudioEngine::playMusic` hook signature (`PlayHooks.cpp`) or a string-type conversion from the v5 API changes.
- The mashup replaces the level music via a hook on `FMODAudioEngine::playMusic`. The level's song *offset*
  isn't adjusted, so line your mashup up to the level's start.
- Account identity: the server binds a random per-install token to your account ID on first upload, so others
  can't overwrite your files. It does **not** verify that you really own that GD account – for stronger
  guarantees add Argon verification (Geode's `argon` mod) to the server.
- Only people with the mod and the same server see mashups. Uploading copyrighted music is your responsibility –
  use the admin delete endpoint to moderate.
