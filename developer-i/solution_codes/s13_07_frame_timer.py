# s13_07_frame_timer.py — คาบ 13 การ์ด 6: เวลาคือการนับเฟรม (ตัวจับเวลาสามตัวบนจอเดียว)
#
# หลักการ   : การ์ด 6 เวลาคือการนับเฟรม   N = t x fps   และ   c = max(0, c - 1) ทุกเฟรม   (คาบ 4, 11, 12)
#             30 วินาที = 900 เฟรม · คูลดาวน์ 8 เฟรม = 30 / 8 = 3.75 นัด/วินาที · ศัตรูทุก 45 เฟรม · ไม่มี sleep
# ลองเล่น   : กด Start · จอยซ้าย/ขวา = ขยับยาน · กด A ค้าง = ยิงรัว · ดู c นับ 8 ลงถึง 0 แล้วจึงยิงนัดใหม่
#             shots/s วัดจริงได้ 3 กับ 4 สลับกัน (เฉลี่ย 3.75) · 5 วินาทีสุดท้ายแถบแดงและมีเสียงนับ
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : คูลดาวน์กันยิงรัว · ปล่อยศัตรูทุก N เฟรม · นับถอยหลังหมดเวลา
# ในงานจริง : กันกดสั่งปั๊มซ้ำ (คูลดาวน์) · อ่านเซนเซอร์ทุก 1 วินาที · นับถอยหลังรอบการผลิต · ค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : s04_catch.py (นับถอยหลัง) · shooter_step3.py (คูลดาวน์) · shooter_step5.py (ตัวจับเวลาปล่อยศัตรู)
# งบวิดเจ็ต : 23 ชิ้น (ยาน 1 + กระสุน 6 + ศัตรู 4 + เส้นแบ่ง 1 + แถบเวลา 2 + ข้อความ 8 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   4) หน้าจอ: สร้างครั้งเดียว   5) วงวนหลัก
import bentogame as game
import random

# ---- 1) ตั้งค่า (แก้ได้) ----
FPS = 30
ROUND_SEC = 30            # นาฬิกานับถอยหลัง 30 วินาที
COOLDOWN = 8              # ยิงแล้วพัก 8 เฟรม
SPAWN = 45                # ปล่อยศัตรูทุก 45 เฟรม
BULLETS, ENEMIES = 6, 4   # ขนาดชุดหมุนเวียน (คาบ 11) สร้างครั้งเดียว
BULLET_SPEED, ENEMY_SPEED, SHIP_SPEED = 12, 3, 6
FIELD_W, SHIP_Y = 470, 326          # สนามยิงซ้ายมือ · ขวามือเป็นแผงตัวจับเวลา
PANEL_X, BAR_W = 494, 280
DIM = 0x9AA8B8

# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ ----
def frames_for(seconds):
    return seconds * FPS                    # N = t x fps

def tick(counter):
    return max(0, counter - 1)              # c = max(0, c - 1) ไม่ติดลบ

def seconds_left(frames):
    return (frames + FPS - 1) // FPS        # ปัดขึ้น: เหลือ 1 เฟรมก็ยังขึ้น 1 วินาที

def find_free(items):
    for item in items:                      # จอดรอเหนือจอ (y < -20) = ว่าง
        if item.y < -20:
            return item
    return None

# ---- 4) หน้าจอ: สร้างครั้งเดียว ----
game.title("FRAME TIMER")
enemies = game.pool("enemy", ENEMIES)
for e in enemies: e.w, e.h = 16, 14         # สไปรต์ไม่มีขนาดในตัว ต้องบอกเองก่อนใช้ game.hit
bullets = [game.Box(0, -50, 5, 12, game.CYAN, border=game.WHITE, radius=2) for _ in range(BULLETS)]
for b in bullets: b.hide()
ship = game.Sprite("ship", 200, SHIP_Y)
game.Box(478, 10, 2, 350, 0x3A4A5A)
game.Text("CLOCK  %d x %d = %d f" % (ROUND_SEC, FPS, frames_for(ROUND_SEC)), PANEL_X, 12, DIM)
game.Box(PANEL_X, 42, BAR_W, 16, 0x1E2A36, border=0x8899AA)
bar = game.Box(PANEL_X + 2, 44, BAR_W - 4, 12, game.GREEN)
hud_time = game.Text("", PANEL_X, 66, game.WHITE)
game.Text("FIRE  c = max(0, c-1)", PANEL_X, 112, DIM)
hud_cool = game.Text("", PANEL_X, 140, game.WHITE)
hud_rate = game.Text("", PANEL_X, 168, game.WHITE)
game.Text("SPAWN  every %d f" % SPAWN, PANEL_X, 214, DIM)
hud_spawn = game.Text("", PANEL_X, 242, game.WHITE)
hud_score = game.Text("", PANEL_X, 296, game.YELLOW)

ship_x, frame = 200, 0
time_left, cool, spawn_left = frames_for(ROUND_SEC), 0, SPAWN
score, best, shots = 0, 0, []               # shots = เลขเฟรมของนัดที่ยิงในหนึ่งวินาทีล่าสุด
shown = {"sec": None, "cool": None, "rate": None, "score": None}
IDEAL = FPS * 100 // COOLDOWN               # 375 = 3.75 นัด/วินาที (เก็บเป็นร้อยส่วน)

# ---- 5) วงวนหลัก ----
def on_frame():
    global ship_x, frame, time_left, cool, spawn_left, score, best
    keys = game.keys()
    frame += 1
    # ตัวที่ 1: นาฬิกานับถอยหลัง หมดแล้วเริ่มรอบใหม่
    time_left = tick(time_left)
    if time_left == 0:
        best, score = max(best, score), 0
        time_left = frames_for(ROUND_SEC)
        game.sfx("win")
    # ตัวที่ 2: คูลดาวน์ — ลดก่อน แล้วยิงได้เมื่อถึง 0 และมีกระสุนว่าง
    cool = tick(cool)
    if keys.left or keys.right:
        ship_x = max(0, min(FIELD_W - 62, ship_x + (SHIP_SPEED if keys.right else -SHIP_SPEED)))
        ship.move_to(ship_x, SHIP_Y)
    if keys.a and cool == 0:
        b = find_free(bullets)
        if b:
            b.move_to(ship_x + 29, SHIP_Y - 12)
            b.show()
            game.sfx("fire")
            cool = COOLDOWN
            shots.append(frame)
    while shots and frame - shots[0] >= FPS: shots.pop(0)    # ทิ้งนัดที่เก่ากว่า 1 วินาที
    # ตัวที่ 3: ตัวจับเวลาปล่อยศัตรู
    spawn_left = tick(spawn_left)
    if spawn_left == 0:
        spawn_left = SPAWN
        e = find_free(enemies)
        if e:                                    # ชุดเต็ม = รอบนี้ข้ามไป
            e.move_to(random.randint(10, FIELD_W - 30), -14)
            e.show()
    # ขยับของที่ใช้อยู่ + ชน (คืนของเข้าชุดเมื่อพ้นจอหรือชน)
    for b in bullets:
        if b.y >= -20:
            b.move_to(b.x, b.y - BULLET_SPEED)
            if b.y < -20: b.hide()             # พ้นจอ = คืนเข้าชุด
    for e in enemies:
        if e.y >= -20:
            e.move_to(e.x, e.y + ENEMY_SPEED)
            shot_down = False
            for b in bullets:
                if b.y >= -20 and game.hit(b, e):
                    b.move_to(b.x, -50); b.hide()
                    score += 1; shot_down = True
                    game.sfx("hit")
                    break
            if shot_down or e.y > 360:           # ชนหรือหล่นพ้นจอ -> คืนเข้าชุด
                e.hide(); e.move_to(-100, -100)
    # โชว์: ตัวเลขเฟรมเปลี่ยนทุกเฟรม ส่วนแถบเวลาเปลี่ยนแค่วินาทีละครั้ง
    sec = seconds_left(time_left)
    hud_time.set("%d s   %d f" % (sec, time_left))
    if sec != shown["sec"]:
        bar.resize(max(2, (BAR_W - 4) * time_left // frames_for(ROUND_SEC)), 12)
        if sec <= 5: game.tone(72 if sec > 1 else 84, ms=80)
        if sec == 5 or sec == ROUND_SEC: bar.set_color(game.RED if sec == 5 else game.GREEN)
        shown["sec"] = sec
    if cool != shown["cool"]:
        hud_cool.set("c = %d   (hold A)" % cool)
        shown["cool"] = cool
    if len(shots) != shown["rate"]:
        hud_rate.set("%d shots/s  ideal %d.%02d" % (len(shots), IDEAL // 100, IDEAL % 100))
        shown["rate"] = len(shots)
    hud_spawn.set("next in %d f" % spawn_left)
    if (score, best) != shown["score"]:
        hud_score.set("SCORE %d   BEST %d" % (score, best))
        shown["score"] = (score, best)
    return True

game.run(on_frame, fps=FPS)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) COOLDOWN = 8 -> 2 : ทายก่อนว่าได้ 15 นัดต่อวินาทีไหม แล้วกด A ค้างดู shots/s
#    ทำไมยิง 6 นัดแล้วสะดุด? (ใบ้: กระสุนมีแค่ 6 นัดในชุด ต้องรอนัดเก่าพ้นจอก่อน — การ์ดถัดไป)
# 2) SPAWN = 45 -> 15 : ศัตรูหล่นถี่ขึ้น 3 เท่า แต่บนจอมีไม่เกิน 4 ตัว เพราะชุดศัตรูมี 4 ตัว
# 3) ROUND_SEC = 30 -> 10 : แถบเวลาสั้นลงเร็วขึ้น ตัวเลขเฟรมเริ่มที่ 300 = 10 x 30
