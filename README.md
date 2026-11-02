# Embedded Systems for Developer

> เรียนรู้ **ระบบสมองกลฝังตัว (Embedded Systems)** จากของจริงบนบอร์ด **BENTO PSoC Edge** ด้วยภาษา **MicroPython** — แตะฮาร์ดแวร์จริง แล้วสร้างเกมและงาน IoT ได้ด้วยมือตัวเอง

### ดูสไลด์ออนไลน์ (ไม่ต้องติดตั้งอะไร)

**https://advance-innovation-centre-aic.github.io/embedded-systems-for-developer/**

---

## สำหรับใคร

หลักสูตรนี้ออกแบบสำหรับ **นิสิตภาควิชาวิศวกรรมระบบสมองกลฝังตัว และภาควิชาวิศวกรรมไฟฟ้า คณะวิศวกรรมศาสตร์ มหาวิทยาลัยบูรพา**

แนวคิดของเราคือ — embedded ไม่จำเป็นต้องเริ่มจากทฤษฎียาก ๆ หรือสู้กับ toolchain ของภาษา C ตั้งแต่วันแรก เราเริ่มจาก **MicroPython** ที่เห็นผลทันทีบน REPL สั่งฮาร์ดแวร์จริง (LED, ปุ่ม, เซนเซอร์, WiFi) แล้วค่อย ๆ ประกอบขึ้นเป็นเกมและระบบ IoT ที่เล่นและใช้งานได้จริง

หัวใจของทุกคาบคือสูตร **core 70% / เขียนเอง 30%** — เราเตรียม engine (70%) ไว้ให้ น้อง ๆ โฟกัสเขียนตรรกะสำคัญ (30%) เอง จึงได้ของที่เล่นได้ตั้งแต่คาบแรก ๆ และเข้าใจลึกขึ้นเรื่อย ๆ ตามเส้นทาง:

**เล่นได้ → แก้ได้ → สร้างได้ → ส่งจริง**

---

## หลักสูตร 3 ส่วน

| ส่วน | เนื้อหา | ทักษะหลัก | ลิงก์ |
|------|---------|-----------|-------|
| **Developer I** (14 คาบ) | รากฐาน embedded → LED · ปุ่ม · timer · เซนเซอร์ · เสียง · WiFi แล้วประกอบเป็น 2 เกม **Pong + Space Shooter** | GPIO, FSM, game loop, เวกเตอร์, การชน, AI พื้นฐาน, object pool, WiFi+MQTT | [เปิดหลักสูตร](https://advance-innovation-centre-aic.github.io/embedded-systems-for-developer/developer-i/roadmap.html) |
| **Developer II** (13 คาบ) | ต่อยอด → **Snake** (list/deque) + **Flappy Bird** (ฟิสิกส์แรงโน้มถ่วง) + interrupts | โครงสร้างข้อมูล, physics integration, delta-time, `Pin.irq`, IoT | [เปิดหลักสูตร](https://advance-innovation-centre-aic.github.io/embedded-systems-for-developer/developer-ii/roadmap.html) |
| **Extra — WiFi + MQTT** (10 โปรเจกต์) | IoT Extra Projects → leaderboard, เล่นข้ามบอร์ด, telemetry, แจ้งเตือน, รีโมตคุมข้ามบอร์ด | pub/sub, MQTT, IoT/Industry 4.0, multiplayer | [เปิด Extra Pack](https://advance-innovation-centre-aic.github.io/embedded-systems-for-developer/extra/index.html) |

---

## สิ่งที่น้อง ๆ จะได้

- เขียน **MicroPython** สั่งฮาร์ดแวร์จริงบน PSoC Edge — GPIO (LED/ปุ่ม), timer/interrupt, ADC, เซนเซอร์ I2C (accelerometer), เสียง, จอ
- เข้าใจ **game loop** (read → update → draw), ฟิสิกส์เวกเตอร์/แรงโน้มถ่วง, การชน (AABB), โครงสร้างข้อมูล (list/deque), object pool, และ AI คู่ต่อสู้ตัวแรก
- ต่อ **WiFi + MQTT** ทำงาน IoT จริง (ส่งเซนเซอร์ขึ้นคลาวด์, leaderboard, เล่นข้ามบอร์ด) — pattern เดียวกับ Industrial IoT
- ฝึก **DevOps เบื้องต้น** — branch, commit, release ส่งงานให้เพื่อนโหลดไปเล่นได้จริง

---

## เริ่มต้นอย่างไร

1. **อุปกรณ์**: บอร์ด **BENTO PSoC Edge** (AI Kit หรือ Eval Kit)
2. **เครื่องมือ**: ติดตั้ง **BENTO IDE** (ส่วนขยายบน VS Code) — ดูที่ https://ide.tesaiot.com/
3. **รันโค้ด**: เปิดไฟล์ `.py` ใน BENTO IDE แล้วกดปุ่ม **Program to Device**
4. **ต่อ WiFi** (สำหรับคาบ IoT): ต่อผ่านเมนู **Wi-Fi บนจอบอร์ด** ครั้งเดียว (เซฟรหัสไว้ บอร์ดจะ auto-connect ทุกครั้งที่เปิด) — คลื่น **2.4GHz** เท่านั้น

> สไลด์ทั้งหมดเปิดดูบนเว็บได้เลยจากลิงก์ด้านบน (ในห้องเรียนกดดูทีละหน้า หรือกด overview ดูภาพรวมแล้วคลิกข้ามหน้าได้)

---

## โครงสร้าง repo

```
embedded-systems-for-developer/        (เว็บไซต์ static — GitHub Pages)
├── index.html              หน้าแรก เลือก 3 ส่วน
├── developer-i/            สไลด์ Developer I (roadmap + คาบ 1–14)
│   └── practise_codes/     ไฟล์ฝึกเว้นช่องให้นิสิตเติม
├── developer-ii/           สไลด์ Developer II (roadmap + คาบ 1–13)
│   └── practise_codes/     ไฟล์ฝึกเว้นช่องให้นิสิตเติม
└── extra/                  สไลด์ Extra WiFi+MQTT (index + 10 โปรเจกต์)
```

ไฟล์ฝึก (`practise_codes/` ของแต่ละหลักสูตร) เป็นโค้ดเว้นช่อง — โหลดจากรีโปนี้ได้ตรง ๆ แล้วเติมเอง 30% · เฉลยอาจารย์เฉลยพร้อมกันในห้อง

---

## โค้ดภาษา C คู่กับทุกไฟล์ MicroPython

ทุกไฟล์ `.py` ในรีโปนี้มีไฟล์ `.c` ชื่อเดียวกันวางอยู่ข้างกัน เป็นโค้ด C ที่เขียนด้วยมือ ทำงานแบบเดียวกับไฟล์ `.py` ทั้งหน้าจอ ข้อความ ไฟ และจังหวะเวลา เขียนตามกฎ **MISRA C:2012** และ **Clean Code** ของไลบรารี BENTO (`CLEAN_C.md`) ส่วนบนของแต่ละไฟล์บอกว่าเป็นคู่ของไฟล์ `.py` ไหน และจุดไหนที่ภาษา C ต้องทำต่างไป

### วิธีรัน

1. เปิด BENTO IDE ที่ [ide.bentoedge.com](https://ide.bentoedge.com) แล้วไปที่แท็บ **C**
2. กดนำเข้าไฟล์ (Import files) แล้วเลือกไฟล์ `.c` ถ้ามีไฟล์ `.h` ชื่อเดียวกันให้เลือกมาด้วย
3. กด **Run on Emulator** เพื่อลองบน BENTO Emulator หรือกด **Program to Device** เพื่อส่งขึ้นบอร์ด

เฟิร์มแวร์ที่รันโปรแกรม C ได้: Dev Kit 2.7.0 ขึ้นไป, Eva Kit (MicroPython) 1.10.0 ขึ้นไป, Eva Kit (Game) 1.6.0 ขึ้นไป

### ข้อจำกัดใน BENTO Emulator ตอนนี้

Edge AI, MQTT และแพลตฟอร์ม TESAIoT จากภาษา C ยังไม่มีใน Emulator โปรแกรมจะขึ้นข้อความบอกบนจอ แล้วทำงานต่อตามเส้นทางเดียวกับไฟล์ `.py` เช่น การ์ด "ไม่มีโมเดลบนบอร์ดนี้" หรือโหมดออฟไลน์ บนบอร์ดจริงทุกส่วนทำงานครบ

### การตรวจที่ทุกไฟล์ผ่าน

- คอมไพล์สำหรับ PSoC Edge ด้วย arm-none-eabi-gcc โดยถือ warning เป็น error
- คอมไพเลอร์ในเบราว์เซอร์ของ BENTO IDE ทั้งสำหรับบอร์ดและสำหรับ Emulator
- cppcheck 2.17.1 กฎ MISRA C:2012 ไม่เหลือข้อที่ผิด นอกจากข้อยกเว้นใน `c_twins_deviations.txt` และข้อยกเว้นของไลบรารี
- clang-tidy 20 และ clang-format 20 ตามค่าตั้งของไลบรารี

### ไฟล์ที่ไม่มีคู่ภาษา C

- `bentogame.py` (ใน full_games, practise_codes และ solution_codes) คือตัวไลบรารีเกมเอง ฝั่งภาษา C ใช้ `bento_game.h` ของเฟิร์มแวร์แทน ทุกไฟล์ `.c` ในรีโปนี้ include ไฟล์นี้

---

## สัญญาอนุญาต (License)

โปรเจกต์นี้ใช้สัญญาอนุญาตแยกตามประเภท:

- **เนื้อหา / สไลด์ / สื่อการสอน** — [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (ดู [CONTENT-LICENSE.md](CONTENT-LICENSE.md)) — ใช้/ดัดแปลง/เผยแพร่ได้ (รวมเชิงพาณิชย์) **โดยต้องอ้างอิงที่มา**
- **โค้ดตัวอย่าง / สคริปต์** — [MIT License](LICENSE) — ใช้และต่อยอดได้อิสระ

การอ้างอิงที่แนะนำ:
> "Embedded Systems for Developer" โดย รศ. วิรุฬห์ ศรีบริรักษ์ · Advance Innovation Centre (AIC) · มหาวิทยาลัยบูรพา — CC BY 4.0

---

## ผู้จัดทำ

**รองศาสตราจารย์ วิรุฬห์ ศรีบริรักษ์**
Advance Innovation Centre (AIC) · BENTO & TESAIoT
จัดทำสำหรับนิสิตคณะวิศวกรรมศาสตร์ มหาวิทยาลัยบูรพา

© 2026 รองศาสตราจารย์ วิรุฬห์ ศรีบริรักษ์ · *BENTO : : Make Anything.*
