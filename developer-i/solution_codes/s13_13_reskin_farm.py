# s13_13_reskin_farm.py — คาบ 13 (คาบพิเศษ): เอาภาพเดิมไปเล่าเรื่องใหม่ = โดรนพ่นยากันแมลงในแปลงผัก
#
# หลักการ   : ภาพเดิม ความหมายใหม่ (สไลด์ "เอาภาพเดิมไปเล่าเรื่องใหม่")
#             ship = โดรนพ่นยา · enemy / enemy2 / enemy3 = แมลงศัตรูพืช 3 แบบ · snake_food = ผลผลิต
#             boom1-boom6 = แมลงโดนยา · เสียง flap ของ Flappy = เสียงพ่นละออง
#             ตรรกะคือ Shooter เดิมทั้งหมด: ชุดหมุนเวียน (คาบ 11) · ชนแบบกล่อง (คาบ 4, 12)
#             คูลดาวน์ c = max(0, c - 1) (คาบ 11) · จำกัดขอบ x = max(lo, min(hi, x)) (คาบ 3)
# ลองเล่น   : กด Start · จอย 4 ทิศ = บินโดรน · A = พ่นยา (ละอองตกลงตรง ๆ)
#             แมลงคลานไปตามแถวผัก เจอผลไหนจะหยุดกัดราว 3.5 วินาที แล้วผลนั้นหายไป
#             พ่นโดนแมลง = ภาพระเบิดแวบหนึ่งแล้วหาย ตัวใหม่คลานเข้ามาแทน · กำจัดครบ = FARM SAFE!
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : Shooter (ยานยิงศัตรู · ระเบิด · ชุดหมุนเวียนของกระสุนและศัตรู)
# ในงานจริง : หน้าจอติดตามโดรนพ่นยาในแปลงเกษตร ตำแหน่งแมลงมาจากกล้องหรือกับดัก
#             ผลผลิตที่รอดคือตัวชี้วัดของรอบ · ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : shooter_step3.py (คูลดาวน์การยิง) · shooter_full.py (pool + frame ของระเบิด)
# งบวิดเจ็ต : 23 ชิ้น (ทุ่ง 1 + ดิน 1 + ผัก 7 + แมลง 3 + ละออง 3 + โดรน 1 + ข้อความ HUD 3
#                    + ป้ายกลางจอ 1 + ข้อความในป้าย 2 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game
import random

# ---- 1) ตั้งค่า (แก้ได้) ----
CROPS = 7            # ผลผลิตในแถว
PESTS_TOTAL = 15     # แมลงทั้งรอบ
ON_SCREEN = 3        # แมลงบนจอพร้อมกันกี่ตัว (ขนาดชุดหมุนเวียน)
BOOM_FRAMES = 10     # ภาพระเบิดค้างกี่เฟรม
BITE = 105           # แมลงกัดผลหนึ่งผลนานกี่เฟรม (105 / 30 = 3.5 วินาที)
SPRAYS = 3           # ละอองในชุดหมุนเวียน
SPRAY_COOLDOWN = 6   # พ่นได้ทุก 6 เฟรม
SPRAY_SPEED = 7      # ละอองตกเฟรมละกี่ px
DRONE_SPEED = 6
ENEMY = ("enemy", "enemy2", "enemy3")
SIZE = ((16, 14), (21, 10), (18, 14))                       # ขนาดภาพแมลงแต่ละแบบ (px)
BOOM = (("boom1", "boom2"), ("boom3", "boom4"), ("boom5", "boom6"))
LANE_Y, CROP_Y, SOIL_Y = 290, 304, 300                      # แนวแมลงคลาน · ผัก · ดิน
CROP_X = [96 + i * 100 for i in range(CROPS)]               # มุมซ้ายของผักแต่ละต้น

# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ (ทดสอบได้โดยไม่ต้องมีบอร์ด) ----
def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def crop_under(cx, alive):
    # ผักต้นไหนอยู่ใต้กลางตัวแมลงพอดี (ห่างไม่เกิน 1 px) และยังไม่ถูกกิน · ไม่มี = -1
    for i in range(len(alive)):
        if alive[i] and abs(cx - (CROP_X[i] + 8)) <= 1:
            return i
    return -1

def verdict(saved, pests_left):
    if saved == 0:      return "lose"
    if pests_left == 0: return "win"
    return ""

# ---- 3) ตรวจสมองก่อนเปิดจอ ----
def self_test():
    for got, want in ((clamp(800, 0, 730), 730), (crop_under(104, [True] * CROPS), 0),
                      (crop_under(104, [False] * CROPS), -1), (crop_under(150, [True] * CROPS), -1),
                      (verdict(0, 3), "lose"), (verdict(4, 0), "win"), (verdict(4, 2), "")):
        if got != want:
            print("ยังไม่ผ่าน: ได้", got, "ควรได้", want)
            return False
    print("ผ่าน! สมองของแปลงผักถูกทุกกรณี")
    return True

if not self_test():
    raise SystemExit

# ---- 4) หน้าจอ: สร้างครั้งเดียว (ทุ่งก่อน โดรนทีหลัง โดรนจึงบินทับทุกอย่าง) ----
game.title("FARM DRONE")
game.Box(0, 0, game.WIDTH, game.HEIGHT, 0x16301C)                   # ทุ่ง
game.Box(0, SOIL_Y, game.WIDTH, 34, 0x5A3A22)                       # แถวดิน
crops = [game.Sprite("snake_food", x, CROP_Y) for x in CROP_X]      # snake_food = ผลผลิต
pests = game.pool("enemy", ON_SCREEN)                                       # enemy = แมลง
sprays = [game.Box(-20, -20, 6, 12, game.CYAN, radius=3) for _ in range(SPRAYS)]
drone = game.Sprite("ship", 365, 120)                               # ship = โดรน
hud_saved = game.Text("", 12, 8, game.WHITE)
hud_pests = game.Text("", 640, 8, game.WHITE)
game.Text("ship = โดรน   enemy = แมลง   snake_food = ผลผลิต", 200, 40, 0x9FC9A0)
banner = game.Box(246, 150, 300, 84, 0x10122C, border=game.YELLOW, radius=8, border_w=3)
big = game.Text("", game.WIDTH // 2 - 50, 168, game.YELLOW)        # สองข้อความยาวเท่ากัน ใช้ที่เดียวได้
small = game.Text("", game.WIDTH // 2 - 70, 200, game.WHITE)

st, alive, shown = {}, [], {"saved": None, "pests": None}
n = ON_SCREEN
kind, state, timer, target, pdir, pspd = [0] * n, [""] * n, [0] * n, [0] * n, [1] * n, [1.0] * n

def spawn(i):
    # แมลงตัวใหม่คลานเข้ามาจากขอบซ้ายหรือขวา หน้าตาสุ่ม 1 ใน 3 แบบ (เปลี่ยนภาพด้วย .frame)
    kind[i], pdir[i] = random.randint(0, 2), random.choice((1, -1))
    pspd[i], state[i] = random.randint(6, 12) / 10, "walk"
    pests[i].frame(ENEMY[kind[i]])
    pests[i].w, pests[i].h = SIZE[kind[i]]
    pests[i].move_to(-20 if pdir[i] > 0 else game.WIDTH, LANE_Y)
    pests[i].show()

def show_banner(msg, sub):
    # ข่าวใหญ่กลางจอ: มีข้อความ = โชว์กรอบ · ข้อความว่าง = ซ่อน
    if msg: banner.show()
    else:   banner.hide()
    big.set(msg)
    small.set(sub)

def new_round():
    st.update({"saved": CROPS, "left": PESTS_TOTAL, "queue": PESTS_TOTAL - ON_SCREEN, "cool": 0, "tick": 0, "over": False})
    alive[:] = [True] * CROPS
    for c in crops: c.show()
    for i in range(ON_SCREEN): spawn(i)
    show_banner("", "")

def spray_step(s):
    # ละอองหนึ่งก้อน: ตกลง · โดนแมลง = แมลงเปลี่ยนภาพเป็นระเบิด · ใช้แล้วเก็บคืนชุด
    s.move_to(s.x, s.y + SPRAY_SPEED)
    used = s.y > SOIL_Y + 10
    for i in range(ON_SCREEN):
        if not used and state[i] in ("walk", "chew") and game.hit(s, pests[i]):
            state[i], timer[i] = "boom", BOOM_FRAMES
            pests[i].frame(BOOM[kind[i]][0])
            pests[i].move_to(pests[i].x - 6, LANE_Y - 6)            # ภาพระเบิด 29x24 ใหญ่กว่าแมลง จัดกลางใหม่
            st["left"] -= 1
            game.sfx("hit")
            used = True
    if used:
        s.move_to(-20, -20)
        s.hide()

def pest_step(i):
    p = pests[i]
    if state[i] == "walk":
        if (p.x < 0 and pdir[i] < 0) or (p.x > game.WIDTH - 20 and pdir[i] > 0):
            pdir[i] = -pdir[i]                                      # ถึงขอบแปลง กลับหลังหัน
        p.move_to(p.x + pdir[i] * pspd[i], LANE_Y + (st["tick"] // 6) % 2 * 2)   # คลานกระดึ๊บ
        c = crop_under(p.x + p.w / 2, alive)
        if c >= 0:
            state[i], timer[i], target[i] = "chew", BITE, c
    elif state[i] == "chew":
        timer[i] -= 1
        if timer[i] == 0:
            state[i] = "walk"
            if alive[target[i]]:
                alive[target[i]] = False
                crops[target[i]].hide()
                st["saved"] -= 1
                game.sfx("lose_life")
            p.move_to(p.x + pdir[i] * 3, LANE_Y)                    # ก้าวพ้นต้นที่กินแล้ว
    elif state[i] == "boom":
        timer[i] -= 1
        if timer[i] == BOOM_FRAMES // 2: p.frame(BOOM[kind[i]][1])                 # ครึ่งหลังเปลี่ยนเป็นเฟรมระเบิดวงใหญ่
        if timer[i] == 0:
            if st["queue"] > 0:                                     # ยังมีแมลงรอคิว = ตัวใหม่คลานเข้ามา
                st["queue"] -= 1
                spawn(i)
            else:                                                   # หมดคิว = ช่องนี้ว่างถาวร
                state[i] = "gone"
                p.hide()

new_round()

# ---- 5) วงวนหลัก ----
def on_frame():
    k = game.keys()
    again = game.pressed_once("a", k)        # อ่านทุกเฟรม: กด A ค้างตอนจบรอบจะไม่นับเป็นการกดใหม่
    if st["over"]:
        if again: new_round()
        return True
    st["tick"] += 1
    dx, dy = (k.right - k.left) * DRONE_SPEED, (k.down - k.up) * DRONE_SPEED
    if dx or dy:                                                    # ขยับเฉพาะตอนกดจอย
        drone.move_to(clamp(drone.x + dx, 0, game.WIDTH - 62), clamp(drone.y + dy, 64, 220))
    st["cool"] = max(0, st["cool"] - 1)
    if k.a and st["cool"] == 0:
        for s in sprays:
            if s.y < 0:                                             # ก้อนที่ว่างอยู่ในชุด
                s.move_to(drone.x + 28, drone.y + 28)
                s.show()
                game.sfx("flap")
                st["cool"] = SPRAY_COOLDOWN
                break
    for s in sprays:
        if s.y >= 0: spray_step(s)
    for i in range(ON_SCREEN):
        pest_step(i)
    if st["saved"] != shown["saved"]:                               # HUD เขียนเฉพาะตอนค่าเปลี่ยน
        hud_saved.set("SAVED %d/%d" % (st["saved"], CROPS))
        shown["saved"] = st["saved"]
    if st["left"] != shown["pests"]:
        hud_pests.set("PESTS %d" % st["left"])
        shown["pests"] = st["left"]
    v = verdict(st["saved"], st["left"])
    if v:
        st["over"] = True
        show_banner("FARM SAFE!" if v == "win" else "CROPS LOST", "A = play again")
        game.sfx("win" if v == "win" else "gameover")
    return True

game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) ENEMY = (...) -> ("bird", "bird", "bird") และ SIZE เป็น (44, 29) ทั้งสามช่อง :
#    แมลงกลายเป็นนกมาจิกผลไม้ เรื่องใหม่ทันทีโดยไม่แตะตรรกะเลยสักบรรทัด
# 2) BITE = 105 -> 30 : แมลงกินไวจนพ่นไม่ทัน ผลผลิตรอดน้อยลง (ในงานจริงค่าแบบนี้มาจากการวัดในแปลง)
# 3) SPRAY_COOLDOWN = 6 -> 1 : กดค้างแล้วพ่นรัว แต่ละอองมีแค่ 3 ก้อน ครบ 3 ก้อนต้องรอก้อนแรกตกถึงดินก่อน
