# CLAUDE.md — terPiece

พอร์ต **Patty's Realms** (`../patty-project`, three.js) มาเป็นเกม **C++20 + OpenGL 4.1** ให้เจ้าของลองเล่นเอง แล้วค่อยต่อยอดเป็นเกมของตัวเอง
**คุยกับเจ้าของเป็นภาษาไทย** · งานที่ต้องทำต่อ: [`docs/HANDOFF.md`](docs/HANDOFF.md)

## Build / รัน

```sh
cmake -S . -B build && cmake --build build -j
./build/terpiece [--char patty|ter|momonga] [--time day|dusk|night] [--realm winter|medieval] [--pos x z] [--cam yaw pitch dist]
                 [--server host:port]
./build/terpiece --shot out.ppm [--shot-delay s] [--auto wrjgue]   # บันทึกภาพแล้วออก (w เดิน r วิ่ง j กระโดด g ร่อน u jetpack e กด E)
sips -s format png out.ppm --out out.png                                    # ดูภาพ
python3 tools/gen_sprites.py                                                # ดึง sprite จาก pixelart.js ฝั่งเว็บ → src/art/sprite_data.inc
```
จอย: ก้านซ้ายเดิน · ก้านขวากล้อง · A กระโดด · X ใช้งาน · B ปาเชอร์รี่ · Y jetpack · Start settings
ปุ่ม: WASD เดิน · Shift วิ่ง · Space กระโดด / กลางอากาศ = ร่อน · U jetpack · E ใช้งาน (เรือ/ขี่/นอน/ทำอาหาร) · F ปาเชอร์รี่ · Y รหัสลับ · T ช่วงเวลา · Esc ปิด settings · ลากเมาส์หมุนกล้อง · scroll ซูม · R โหลด shader ใหม่

- **Multiplayer**: รัน `python3 server.py 8000` ใน `../patty-project/xmas-app` แล้ว `--server 127.0.0.1:8000` (หรือ env `TERPIECE_SERVER`) — โปรโตคอลเดียวกับเว็บ เห็นกันข้ามเว็บ/เดสก์ท็อปได้ · รองรับแค่ `ws://` (ngrok `https` ยังไม่ได้)
- save อยู่ที่ `~/Library/Application Support/terPiece/` (key เดียวกับ localStorage ฝั่งเว็บ) · รัน `--shot` จะไม่เขียน save
- **ห้ามใช้ `ls` ผ่าน Bash tool** (alias eza ค้าง) → ใช้ `/bin/ls`

## โครงสร้าง (ไฟล์ C++ ตั้งชื่อตามไฟล์ JS ที่พอร์ตมา)

| ที่ | หน้าที่ |
|---|---|
| `src/scene/` | three.js ย่อส่วน: `geometry` (generator r151 ตรงๆ + `worldUV` + extrude), `material`, `object` (Object3D/Mesh/InstancedMesh/Sprite/Points/PointLight + tag `cw`), `renderer` (เงา PCF, point light ≤16, fog, เรียงโปร่งแสง, MSAA → pixel 2× → tilt-shift/grade/sRGB), `gltf` (cgltf + meshopt + WebP, AnimationMixer) |
| `src/gfx/` | `gl.h`, `shader` (+ `sharedShader()` สำหรับ ShaderMaterial), `texture` + `image.mm` (ImageIO อ่าน/เขียนรูป) |
| `src/art/` | `textures` (= TEX), `sprites` (= buildSheet + SpriteActor), `glow` (= glowSprite), `canvas.h` |
| `src/world/` | `world.h` (API ร่วม), `winter` (= world.js), `medieval` (= medieval.js), `cubeworld` (= cubeworld.js), `overworld` + `layout.h` (โลกเปิด: สองเมือง + The Wilds ถนน/แม่น้ำ — ของใหม่) |
| `src/game/` | `game` (= main.js), `player`, `pets` (= entities.js), `berries`, `parkour`, `vehicles`, `effects`, `home` (ห้องคริสต์มาส), `save`, `config.h` (= config.js + ZONES/TIME_INFO) |
| `src/ui/` | `canvas2d.mm` (canvas 2D บน CoreText — ไทย/emoji), `hud` (= index.html + style.css), `minimap` |
| `src/audio/`, `src/net/` | = audio.js (miniaudio) / net.js (WebSocket client เขียนเอง) |
| `shaders/` | `mesh` (Basic/Lambert/Standard), `depth`, `sprite`, `post`, `tilt`, `overlay` + ShaderMaterial: `sky`, `stars`, `aurora`, `snow`, `fireworks`, `neon`, `fireparticles` |
| `assets/` | ของเจ้าของ: `models/` (.glb, baked.jpg, fire.png), `art/keyart.jpg` (ไม่ได้ใช้แล้ว), `audio/xmas.mp3`, `fonts/` (Mali, Cormorant Garamond, Inter — OFL) |

## ข้อตกลง

- **พิกัดในแต่ละเมืองเหมือนฝั่งเว็บ** (Y ขึ้น, −Z = ทิศเหนือ, เมตร) → ก๊อปตัวเลขจาก world.js/medieval.js ได้ตรงๆ · หมู่บ้านหิมะ = global, เมืองยุคกลางวางที่ (0,250) หมุน π (`layout.h`) — ของที่อยู่ในเมืองใช้ local, ผู้เล่นใช้ global
- พอร์ตบรรทัดต่อบรรทัด: ค่าคงที่/seed/ลำดับการเรียก rng ให้ตรงเว็บ (แผนที่ออกมาเหมือนเป๊ะ) · สี hex = linear ตรงๆ แบบ three.js r151 legacy
- **comment ในโค้ดเป็นภาษาอังกฤษ และอยู่บรรทัดของตัวเองเหนือโค้ด** (ไม่ต่อท้ายตัวแปร) — เจ้าของจะพัฒนาต่อ
- sprite ตัวละครแก้ที่ `pixelart.js` ฝั่งเว็บ แล้วรัน `tools/gen_sprites.py` (อย่าแก้ `sprite_data.inc` ตรงๆ)
- ตรวจผลด้วย `--shot` + `--auto` แทนการจับหน้าจอ
- **asset ของ Cube World ห้ามคัดลอกเข้าโปรเจค** (เป็นชุด crack) — โหลดจาก `$CW_ASSETS` หรือ `../patty-project/xmas-app/web/assets/cubeworld` ตอนรัน, ห้ามแจกจ่าย build คู่กับไฟล์พวกนั้น (ไม่มีก็เล่นได้ ใช้โมเดลของเราเอง)
- โมเดล .glb ของโปรเจคปีก่อนเป็นของเจ้าของ ใช้ได้ (คัดลอกไว้ใน `assets/models/` แล้ว)
- รหัสลับเปลี่ยนตัวละครเก็บเป็น SHA-256 เท่านั้น — ห้ามเขียนคำจริงในโค้ด/เอกสาร
- ยังไม่มี git — commit เฉพาะตอนเจ้าของสั่ง
