# s13_16_kit_characters.py — คาบ 13 (คาบพิเศษ): ชุดตัวละครสำเร็จรูป ยกมาจากเกมเต็มของจริง หยิบไปใส่เกมของน้องได้ทันที
#
# หลักการ   : ตัวละคร 1 ตัว = ภาพ (Sprite) + สถานะ (dict) + ฟังก์ชันก้าวทีละเฟรม          (คาบ 4, 11)
#             ยานมีน้ำหนัก  กดค้าง v = v + a · ปล่อย v = v * f · มีเพดานความเร็ว          (คาบ 6, 10)
#             นกเงือก      v = v + g*dt · y = y + v*dt · กระพือ = ตั้ง v ใหม่ทันที         (คาบ 5, 6)
#             ไม้ปิงปอง    vy ลูก += 2*จุดที่โดน + vy ไม้ * 0.28 (สปิน)                     (คาบ 7)
#             หัวงูเปลี่ยนภาพตามทิศด้วย .frame() · ระเบิด 2 เฟรมด้วยตัวนับถอยหลัง          (คาบ 4, 12)
# ลองเล่น   : กด Start · ซ้าย/ขวา = เปลี่ยนตัวละคร (โชว์ทีละตัว) · A = ท่าประจำตัว
#             ยาน: บินซ้าย-ขวาเอง (จำลองกดค้าง/ปล่อย) UP/DOWN ขับเอง A ยิง ดูกระสุนพุ่งเฉียงตามแรงยาน
#             ศัตรู: A = ระเบิดตัวล่างสุด ดูสีระเบิดตามชนิด · นก: A/UP กระพือ ดูช่วง 4 วินาทีที่ตกพื้นไม่ตาย
#             งู: A เลี้ยวตามเข็มนาฬิกา UP/DOWN เลี้ยวแบบเกมจริง ดูหัวหันตามทิศ · ไม้: UP/DOWN ขยับ A เสิร์ฟ
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : shooter_full · flappy_full · snake_sprite_full · pong_full ใช้ตัวละครชุดนี้ทั้งตัว
# ในงานจริง : ยาน = โดรนพ่นยา/รถ AGV ที่เร่งแล้วไหลต่อ · ระเบิด = ชิ้นงานถูกคัดทิ้ง · ไม้ = แขนกลคัดแยก
#             ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : full_games/shooter_full.py · flappy_full.py · snake_sprite_full.py · pong_full.py
# งบวิดเจ็ต : 25 ชิ้น สร้างครบทุกตัวครั้งเดียว โชว์ทีละตัว ตัวอื่นซ่อนไว้ (ไม่สร้างไม่ลบระหว่างเล่น)
#             พื้นหลัง 1 + ยาน 1 กระสุน 3 + ศัตรู 3 ระเบิด 3 + นก 1 พื้นหญ้า 1 + งู 4+หัว 1+อาหาร 1
#             + ไม้ 1 ลูก 1 + ป้าย 3 + game.run 1 (โชว์ใช้กระสุน 3 นัด พูลงู 4 ท่อน ให้พองบ 26)
#
# วิธีหยิบไปใช้: คัดลอกทั้งกล่อง "หยิบไปใช้ ... จบชิ้น" ไปวางในเกมของน้อง (บนสุด ใต้ import)
#   สร้างตัวละครครั้งเดียวก่อน game.run() แล้วเรียกฟังก์ชันก้าวใน on_frame ทุกเฟรม — ตัวอย่างอยู่ในส่วนที่ 4
import bentogame as game
import random

# ---- 1) ตั้งค่า (แก้ได้) ----
FPS = 30
DT = 1.0 / FPS          # flappy_full วัด dt จริงด้วย time.ticks_ms · ชุดนี้ใช้ dt คงที่ จะได้ import แค่ bentogame
SPACE = 0x10122C        # สีอวกาศ (shooter_full.py:67)

# ---- 2) สมอง + ชิ้นสำเร็จรูป (หยิบไปใช้ได้ทีละกล่อง) ----
# ===== หยิบไปใช้: ยานมีน้ำหนัก + กระสุนติดแรงยาน (จาก full_games/shooter_full.py:15-20, 71-74, 98-113, 137-148, 178-214) =====
SHIP_W, SHIP_H = 62, 28
ACCEL, MAXV, FRICTION = 1.4, 13.0, 0.80      # ฟิสิกส์ยานชุดเดียวกับเกม C เป๊ะ
BULLET_VY, BULLET_INHERIT = 8.0, 0.55        # กระสุนพุ่งขึ้น + พกแรงแนวนอนของยานไป 55%
LASER = 0x80F4FF

def make_ship(x, y, bullets=4):
    s = {"spr": game.Sprite("ship", x, y), "x": float(x), "y": float(y), "vx": 0.0, "vy": 0.0,
         "b": [], "on": [False] * bullets, "bx": [0.0] * bullets, "by": [0.0] * bullets, "bvx": [0.0] * bullets}
    for _ in range(bullets):                 # กระสุนแท่งฟ้าขอบขาวมุมมน สร้างครั้งเดียวแล้วซ่อน
        b = game.Box(-20, -50, 5, 12, LASER, border=game.WHITE, radius=2)
        b.hide()
        s["b"].append(b)
    return s

def axis(v, minus, plus):
    # กดค้าง = เร่งทีละ ACCEL · ปล่อย = ไถลด้วยแรงเสียดทาน · แล้วตัดที่เพดานความเร็ว
    if minus and not plus:
        v -= ACCEL
    elif plus and not minus:
        v += ACCEL
    else:
        v *= FRICTION
    return max(-MAXV, min(MAXV, v))

def ship_step(s, left, right, up, down, y_min=24, y_max=game.HEIGHT - SHIP_H - 32):
    s["vx"], s["vy"] = axis(s["vx"], left, right), axis(s["vy"], up, down)
    x, y = s["x"] + s["vx"], s["y"] + s["vy"]
    if x < 0 or x > game.WIDTH - SHIP_W:     # ชนขอบ = หยุดความเร็วแกนนั้น ยานจะได้ไม่อัดขอบค้าง
        x, s["vx"] = max(0.0, min(float(game.WIDTH - SHIP_W), x)), 0.0
    if y < y_min or y > y_max:
        y, s["vy"] = max(float(y_min), min(float(y_max), y)), 0.0
    s["x"], s["y"] = x, y
    s["spr"].move_to(x, y)
    for j in range(len(s["b"])):             # กระสุน: ขึ้น + เฉียงตามแรงที่ติดมา หลุดจอเก็บคืนพูล
        if s["on"][j]:
            s["by"][j] -= BULLET_VY
            s["bx"][j] += s["bvx"][j]
            if s["by"][j] < -12 or s["bx"][j] < -12 or s["bx"][j] > game.WIDTH:
                s["on"][j] = False
                s["b"][j].hide()
            else:
                s["b"][j].move_to(s["bx"][j], s["by"][j])

def ship_fire(s):
    for j in range(len(s["b"])):             # หานัดที่ว่างในพูล ไม่มีก็ยิงไม่ออก (ไม่สร้างนัดใหม่)
        if not s["on"][j]:
            s["on"][j] = True
            game.sfx("fire")
            s["bx"][j], s["by"][j] = s["x"] + SHIP_W / 2.0 - 2.0, s["y"] - 8.0
            s["bvx"][j] = s["vx"] * BULLET_INHERIT
            s["b"][j].move_to(s["bx"][j], s["by"][j])
            s["b"][j].show()
            return
# ===== จบชิ้น =====

# ===== หยิบไปใช้: ศัตรู 3 แบบ + ระเบิด 2 เฟรมสีตามชนิด (จาก full_games/shooter_full.py:21, 42-45, 75-76, 123-135, 150-170) =====
ENEMY_SPR = ("enemy", "enemy2", "enemy3")                  # invader ม่วง / UFO เงิน / demon แดง
BOOM_FRAMES = (("boom1", "boom2"), ("boom3", "boom4"), ("boom5", "boom6"))
BOOM_TICKS = 6                                             # ค้างกี่เฟรม ครึ่งหลังสลับเป็นวงใหญ่

def make_enemy(kind, x, y):
    e = game.Sprite("enemy", x, y)
    e.frame(ENEMY_SPR[kind])          # เกมเต็มสร้างพูลเป็น "enemy" แล้วเปลี่ยนหน้าตาตอนเกิด
    e.w, e.h, e.kind = 26, 16, kind   # กล่องชนที่เกมเต็มใช้ (สไปรต์ไม่รู้ขนาดตัวเอง)
    return e

def make_booms(n=3):
    return {"spr": game.pool("boom1", n), "t": [0] * n, "k": [0] * n}

def boom(bm, cx, cy, kind):
    game.sfx("explode")
    for b in range(len(bm["t"])):
        if bm["t"][b] == 0:
            bm["t"][b], bm["k"][b] = BOOM_TICKS, kind
            bm["spr"][b].frame(BOOM_FRAMES[kind][0])
            bm["spr"][b].move_to(cx - 15, cy - 12)    # จัดกึ่งกลางภาพระเบิด 29x24
            bm["spr"][b].show()
            return

def booms_step(bm):                                    # เรียกทุกเฟรม
    for b in range(len(bm["t"])):
        if bm["t"][b] == 0:
            continue
        bm["t"][b] -= 1
        if bm["t"][b] == 0:
            bm["spr"][b].hide()
        elif bm["t"][b] == BOOM_TICKS // 2:            # ครึ่งหลังสลับเป็นเฟรมบึ้มวงใหญ่
            bm["spr"][b].frame(BOOM_FRAMES[bm["k"][b]][1])
# ===== จบชิ้น =====

# ===== หยิบไปใช้: นกเงือก แรงโน้มถ่วง + กระพือ + ออกตัว 4 วินาทีตกไม่ตาย (จาก full_games/flappy_full.py:17-19, 29, 145-200) =====
BIRD_W, BIRD_H, BIRD_BOX = 44, 29, 36        # ภาพนก 44x29 · กล่องชน 36 (ใจดีกว่าภาพนิดหน่อย)
GRAVITY, FLAP_VEL = 1050.0, -340.0           # โหมด NORMAL ของเกม C: px/s^2 และ px/s
GRACE = 120                                  # 4 วินาทีแรก ที่ 30 fps (เกมเต็มจับเวลาเป็น ms ที่นี่นับเฟรม)

def make_bird(x, play_h):
    b = {"spr": game.Sprite("bird", 0, 0), "x": x, "play_h": play_h}
    bird_reset(b)
    return b

def bird_reset(b):                            # นกลอยรอกลางจอ จนกว่าจะกระพือครั้งแรก
    b["y"], b["vy"], b["started"], b["grace"] = float(b["play_h"] // 2), 0.0, False, 0
    b["spr"].move_to(b["x"] - BIRD_W // 2, b["play_h"] // 2 - BIRD_H // 2)

def bird_step(b, flap, dt):                   # คืน False เมื่อนกตกพื้นหลังหมดช่วง grace
    if flap:
        b["vy"] = FLAP_VEL
        game.sfx("flap")
    if not b["started"]:                      # ยังไม่ออกตัว: ไม่มีแรงโน้มถ่วง ไม่ตก
        if not flap:
            return True
        b["started"], b["grace"] = True, GRACE
    b["grace"] = max(0, b["grace"] - 1)
    b["vy"] += GRAVITY * dt
    b["y"] += b["vy"] * dt
    half = BIRD_BOX / 2
    if b["y"] < half:                         # ชนเพดาน: ค้างไว้ ไม่ตาย
        b["y"], b["vy"] = half, 0.0
    if b["y"] > b["play_h"] - half:           # ถึงพื้น
        if b["grace"] == 0:
            game.sfx("fall")
            return False
        b["y"], b["vy"] = b["play_h"] - half, 0.0   # ช่วง grace: หยุดที่พื้นเฉย ๆ
    b["spr"].move_to(b["x"] - BIRD_W // 2, int(b["y"]) - BIRD_H // 2)
    return True
# ===== จบชิ้น =====

# ===== หยิบไปใช้: งูสไปรต์ หัวหันตามทิศ (จาก full_games/snake_sprite_full.py:14-15, 38-39, 61-64, 78-95, 109-138) =====
# (เกมของน้องต้องมี import random ด้วย)
CELL = 16
COLS, ROWS = game.WIDTH // CELL, game.HEIGHT // CELL          # 49 x 24 ช่อง
HEAD_BY_DIR = {(1, 0): "snake_head_r", (-1, 0): "snake_head_l",
               (0, -1): "snake_head_u", (0, 1): "snake_head_d"}

def make_snake(col, row, length=5, pool=8):
    sn = {"body": [[col - i, row] for i in range(length)], "dx": 1, "dy": 0, "shown": 0,
          "seg": game.pool("snake_body", pool), "head": game.Sprite("snake_head_r", 0, 0),
          "food": game.Sprite("snake_food", 0, 0), "fc": [0, 0]}
    place_food(sn)
    snake_draw(sn)
    return sn

def snake_turn(sn, up, down, left, right):    # เลี้ยวได้ทีละ 90 องศา ห้ามกลับหลังหัน
    if up and sn["dy"] == 0:      sn["dx"], sn["dy"] = 0, -1
    elif down and sn["dy"] == 0:  sn["dx"], sn["dy"] = 0, 1
    elif left and sn["dx"] == 0:  sn["dx"], sn["dy"] = -1, 0
    elif right and sn["dx"] == 0: sn["dx"], sn["dy"] = 1, 0

def place_food(sn):                           # สุ่มจนได้ช่องที่ไม่ทับตัวงู
    while True:
        c = [random.randint(0, COLS - 1), random.randint(0, ROWS - 1)]
        if c not in sn["body"]:
            sn["fc"] = c
            sn["food"].move_to(c[0] * CELL, c[1] * CELL)
            return

def snake_draw(sn):                           # ต่างจากเกมเต็มนิดเดียว: โชว์/ซ่อนเฉพาะท่อนที่เปลี่ยน ประหยัดข้อความวาดจอ
    name = HEAD_BY_DIR[(sn["dx"], sn["dy"])]
    if name != sn["head"].name:               # เปลี่ยนภาพหัวเฉพาะตอนทิศเปลี่ยน
        sn["head"].frame(name)
    sn["head"].move_to(sn["body"][0][0] * CELL, sn["body"][0][1] * CELL)
    n = min(len(sn["body"]) - 1, len(sn["seg"]))
    for i in range(len(sn["seg"])):           # body[1:] -> สไปรต์ในพูล ที่เหลือซ่อน
        if i < n:
            sn["seg"][i].move_to(sn["body"][i + 1][0] * CELL, sn["body"][i + 1][1] * CELL)
            if i >= sn["shown"]:
                sn["seg"][i].show()
        elif i < sn["shown"]:
            sn["seg"][i].hide()
    sn["shown"] = n

def snake_step(sn):                           # ก้าว 1 ช่อง คืน "wall" / "self" / "eat" / ""
    nh = [sn["body"][0][0] + sn["dx"], sn["body"][0][1] + sn["dy"]]
    if not (0 <= nh[0] < COLS and 0 <= nh[1] < ROWS):
        game.sfx("die")                       # ชนกำแพง
        return "wall"
    if nh in sn["body"]:
        game.sfx("lose")                      # กัดตัวเอง คนละเสียงกับชนกำแพง
        return "self"
    sn["body"].insert(0, nh)
    ate = nh == sn["fc"]
    if ate:
        game.sfx("eat")
        place_food(sn)
    if not ate or len(sn["body"]) > len(sn["seg"]) + 1:     # ไม่ได้กิน หรือพูลเต็ม = ไม่โต
        sn["body"].pop()
    snake_draw(sn)
    return "eat" if ate else ""
# ===== จบชิ้น =====

# ===== หยิบไปใช้: ไม้ปิงปองมีน้ำหนัก + ตีแล้วใส่สปิน (จาก full_games/pong_full.py:18-27, 36-53, 82-125, 179-185) =====
# (เกมของน้องต้องมี import random ด้วย)
PADDLE_W, PADDLE_H, PADDLE_X = 14, 90, 14
BALL_SIZE, BALL_SPEED0, BALL_MAXV, BALL_SPEEDUP = 14, 6.2, 14.0, 0.35
PADDLE_ACCEL, PADDLE_MAXV, PADDLE_FRICTION, PADDLE_SPIN = 1.5, 13.0, 0.78, 0.28

def make_pong():
    y0 = game.HEIGHT // 2 - PADDLE_H // 2
    p = {"pad": game.Sprite("paddle", PADDLE_X, y0), "ball": game.Sprite("ball", 0, 0),
         "py": float(y0), "pvy": 0.0, "spin": 0.0}
    p["pad"].w, p["pad"].h = PADDLE_W, PADDLE_H          # บอกขนาดให้ game.hit() ใช้
    p["ball"].w = p["ball"].h = BALL_SIZE
    pong_serve(p, False)
    return p

def pong_serve(p, to_right):                  # วางลูกกลางโต๊ะ เสิร์ฟมุมสุ่มเล็กน้อย
    p["bx"], p["by"] = game.WIDTH / 2 - BALL_SIZE / 2, game.HEIGHT / 2 - BALL_SIZE / 2
    p["bvx"] = BALL_SPEED0 if to_right else -BALL_SPEED0
    p["bvy"] = random.randint(-22, 22) / 10.0

def paddle_step(p, up, down):
    if up and not down:
        p["pvy"] -= PADDLE_ACCEL
    elif down and not up:
        p["pvy"] += PADDLE_ACCEL
    else:
        p["pvy"] *= PADDLE_FRICTION
    p["pvy"] = max(-PADDLE_MAXV, min(PADDLE_MAXV, p["pvy"]))
    p["py"] += p["pvy"]
    if p["py"] <= 0 or p["py"] >= game.HEIGHT - PADDLE_H:   # ชนขอบแล้วหยุดนิ่ง ไม่เด้ง
        p["py"], p["pvy"] = max(0.0, min(float(game.HEIGHT - PADDLE_H), p["py"])), 0.0
    p["pad"].move_to(PADDLE_X, p["py"])

def ball_step(p):                             # คืน True เมื่อตีโดนไม้เฟรมนี้
    p["bx"] += p["bvx"]
    p["by"] += p["bvy"]
    if p["by"] <= 0 or p["by"] >= game.HEIGHT - BALL_SIZE:  # เด้งขอบบน/ล่าง
        p["by"] = max(0.0, min(float(game.HEIGHT - BALL_SIZE), p["by"]))
        p["bvy"] = -p["bvy"]
        game.sfx("wall")
    p["ball"].x, p["ball"].y = p["bx"], p["by"]      # ให้กล่องชนรู้ตำแหน่งลูกของเฟรมนี้ก่อนเช็ค
    hit = p["bvx"] < 0 and game.hit(p["ball"], p["pad"])
    if hit:                                   # สะท้อน + เร่ง + มุมตามจุดที่โดน + สปินจากไม้ที่กำลังวิ่ง
        norm = ((p["by"] + BALL_SIZE / 2) - (p["py"] + PADDLE_H / 2)) / (PADDLE_H / 2)
        p["bx"] = float(PADDLE_X + PADDLE_W)
        p["bvx"] = abs(p["bvx"]) + BALL_SPEEDUP
        p["spin"] = p["pvy"] * PADDLE_SPIN
        p["bvy"] += norm * 2.0 + p["spin"]
        speed = (p["bvx"] ** 2 + p["bvy"] ** 2) ** 0.5
        if speed > BALL_MAXV:                 # จำกัดความเร็ว "รวม" ไม่ให้ทะลุเพดาน
            p["bvx"], p["bvy"] = p["bvx"] * BALL_MAXV / speed, p["bvy"] * BALL_MAXV / speed
        game.sfx("paddle")
    p["ball"].move_to(p["bx"], p["by"])       # วาดหลังเคลียร์เรื่องชนแล้ว ลูกจะไม่จมในไม้
    return hit
# ===== จบชิ้น =====

# ---- 4) หน้าจอ: สร้างครบทุกตัวครั้งเดียว แล้วโชว์ทีละตัว (ตัวอื่นซ่อนไว้) ----
# เปลี่ยนตัว = ซ่อนชุดเก่า + โชว์ชุดใหม่ ไม่สร้างไม่ลบระหว่างเล่นเลย (สไปรต์ใช้ซ้ำได้ตลอด)
def enter_ship(s):
    bg.set_color(SPACE)
    s["x"], s["y"], s["vx"], s["vy"], s["t"] = 365.0, float(game.HEIGHT - SHIP_H - 32), 0.0, 0.0, 0
    s["on"] = [False] * len(s["b"])
    s["spr"].move_to(s["x"], s["y"])

def play_ship(s, k, a):
    s["t"] += 1
    phase = (s["t"] // 24) % 4                # ขวา 24 เฟรม · ปล่อย · ซ้าย 24 เฟรม · ปล่อย
    ship_step(s, phase == 2, phase == 0, k.up, k.down)
    if a:
        ship_fire(s)
    say("vx %d  vy %d  (เพดาน 13)   กระสุนพก vx ไป 55%%" % (int(s["vx"]), int(s["vy"])))

def make_enemies():
    d = {"e": [make_enemy(i, 0, 0) for i in range(3)], "bm": make_booms(3)}   # ระเบิดสร้างทีหลัง อยู่ชั้นบน
    d["all"], d["base"] = d["e"] + d["bm"]["spr"], d["e"]
    return d

def enter_enemy(d):
    bg.set_color(SPACE)
    for i, e in enumerate(d["e"]):
        e.move_to(220 + i * 160, 100 + i * 60)
    d["bm"]["t"] = [0] * len(d["bm"]["t"])
    d["vy"], d["n"] = [random.randint(18, 34) / 20.0 for _ in range(3)], 0   # ช้ากว่าเกมเต็มครึ่งหนึ่ง

def play_enemy(d, k, a):
    for i, e in enumerate(d["e"]):
        e.move_to(e.x, (e.y + d["vy"][i]) if e.y < game.HEIGHT else -20)   # หลุดล่าง = เกิดใหม่บนสุด
    if a:                                     # ยิงโดนตัวล่างสุด: บึ้ม + เสียงโดน + เกิดใหม่บนสุด
        e = max(d["e"], key=lambda en: en.y)
        boom(d["bm"], e.x + 13, e.y + 8, e.kind)
        game.sfx("hit")
        e.move_to(random.randint(20, game.WIDTH - 46), -20)
        d["n"] += 1
    booms_step(d["bm"])
    say("invader / UFO / demon   ระเบิดไป %d ตัว" % d["n"])

def enter_bird(b):
    bg.set_color(0x88C1F5)                    # ฟ้าแถบกลางของ flappy_full.py:35
    bird_reset(b)

def play_bird(b, k, a):
    up = game.pressed_once("up", k)           # อ่านทุกปุ่มก่อนค่อย or (pressed_once ต้องเห็นทุกเฟรม)
    if not bird_step(b, a or up, DT):
        bird_reset(b)
    if not b["started"]:
        say("ลอยรอ  กด A หรือ UP เพื่อออกตัว")
    elif b["grace"] > 0:
        say("grace อีก %d วินาที: ตกพื้นยังไม่ตาย" % ((b["grace"] + FPS - 1) // FPS))
    else:
        say("บินจริงแล้ว: ตกถึงพื้น = ตาย")

def enter_snake(sn):
    bg.set_color(0x4CA838)                    # สนามหญ้า (snake_sprite_full.py:25)
    sn["body"], sn["dx"], sn["dy"], sn["shown"] = [[10 - i, 12] for i in range(5)], 1, 0, 0
    sn["t"], sn["want"], sn["score"] = 0, None, 0
    snake_draw(sn)

def play_snake(sn, k, a):
    up, down = game.pressed_once("up", k), game.pressed_once("down", k)
    if a or up or down:                       # จำปุ่มไว้ เลี้ยวตอนก้าว (ก้าวละครั้ง เหมือนเกมเต็ม)
        sn["want"] = "a" if a else ("up" if up else "down")
    sn["t"] += 1
    if sn["t"] % 4:                           # ก้าวทุก 4 เฟรม = 133 ms (เกมเต็ม CLASSIC 115 ms)
        return
    if sn["want"] == "a":                     # ทางลัดของโชว์: เลี้ยวตามเข็มนาฬิกา ขวา-ลง-ซ้าย-ขึ้น
        sn["dx"], sn["dy"] = -sn["dy"], sn["dx"]
    else:
        snake_turn(sn, sn["want"] == "up", sn["want"] == "down", False, False)
    sn["want"] = None
    got = snake_step(sn)
    if got == "eat":
        sn["score"] += 1
    if got in ("wall", "self"):               # ตาย: เริ่มตัวใหม่ (เกมเต็มขึ้นป้าย GAME OVER)
        score = sn["score"]
        enter_snake(sn)
        sn["score"] = score
    say("หัว: %s   กินไป %d   ยาว %d" % (sn["head"].name, sn["score"], len(sn["body"])))

def enter_pong(p):
    bg.set_color(0x1C7A34)                    # สักหลาดเขียว FELT (pong_full.py:28)
    p["py"], p["pvy"] = float(game.HEIGHT // 2 - PADDLE_H // 2), 0.0
    p["pad"].move_to(PADDLE_X, p["py"])
    pong_serve(p, False)
    say("ตีตอนไม้กำลังวิ่ง = ใส่สปิน")

def play_pong(p, k, a):
    if a:
        pong_serve(p, False)
    paddle_step(p, k.up, k.down)
    if ball_step(p):
        say("ตีโดน! vy ไม้ %d -> สปิน %.1f -> vy ลูก %.1f" % (int(p["pvy"]), p["spin"], p["bvy"]))
    if p["bx"] > game.WIDTH - BALL_SIZE:      # โชว์นี้ใช้ผนังขวาแทนไม้ AI ของเกมเต็ม
        p["bx"], p["bvx"] = float(game.WIDTH - BALL_SIZE), -abs(p["bvx"])
        p["ball"].move_to(p["bx"], p["by"])
        game.sfx("wall")
    elif p["bx"] < -25:                       # พลาด: ลูกเลยขอบไป 25 px แล้วเสิร์ฟใหม่
        game.sfx("pong_score")
        pong_serve(p, False)

def say(msg):                                 # เขียนป้ายเฉพาะตอนข้อความเปลี่ยน
    if msg != view["last"]:
        info.set(msg)
        view["last"] = msg

def open_piece(i):                            # ซ่อนตัวเก่าทั้งชุด โชว์ตัวใหม่ แล้วเริ่มสถานะใหม่
    for w in pieces[view["idx"]]["all"]:
        w.hide()
    view["idx"] = i
    say("")                                   # ล้างป้ายข้อมูลของตัวก่อน
    for w in pieces[i]["base"]:
        w.show()
    PIECES[i][1](pieces[i])
    caption.set("%d/%d  %s   (ซ้าย/ขวา = ตัวอื่น)" % (i + 1, len(PIECES), PIECES[i][0]))
    helper.set(PIECES[i][3])

PIECES = (("SHIP  shooter_full", enter_ship, play_ship, "UP/DOWN = ขับ   A = ยิง   (ซ้าย-ขวาบินเอง)"),
          ("ENEMY + BOOM  shooter_full", enter_enemy, play_enemy, "A = ยิงตัวล่างสุดให้ระเบิด"),
          ("BIRD  flappy_full", enter_bird, play_bird, "A หรือ UP = กระพือ"),
          ("SNAKE HEAD  snake_sprite_full", enter_snake, play_snake, "A = เลี้ยวตามเข็ม   UP/DOWN = เลี้ยว"),
          ("PADDLE + SPIN  pong_full", enter_pong, play_pong, "UP/DOWN = ขยับไม้   A = เสิร์ฟ"))

game.title("KIT CHARACTERS")
bg = game.background(SPACE)                   # พื้นหลังชิ้นเดียวใช้ทุกตัว เปลี่ยนแค่สี
ship = make_ship(0, 0, bullets=3)             # โชว์ใช้ 3 นัดให้พองบ (เกมเต็มใช้ 4)
grass = game.grass(20)                        # พื้นหญ้าของนก = เส้นตาย (flappy_full.py:60)
bird = make_bird(game.WIDTH // 2, game.HEIGHT - 20)
snake = make_snake(10, 12, pool=4)            # พูล 4 = ยาว 5 ท่อนพอดี กินแล้วไม่โต (แบบพูลเต็มของเกมเต็ม)
pong = make_pong()
# "all" = ทุกชิ้นของตัวนั้น (ไว้ซ่อนตอนออก) · "base" = ชิ้นที่ต้องโชว์ตอนเข้า (กระสุน/ระเบิด/ลำตัว โชว์เองตามเกม)
ship["all"], ship["base"] = [ship["spr"]] + ship["b"], [ship["spr"]]
bird["all"] = bird["base"] = [grass, bird["spr"]]
snake["all"], snake["base"] = snake["seg"] + [snake["head"], snake["food"]], [snake["head"], snake["food"]]
pong["all"] = pong["base"] = [pong["pad"], pong["ball"]]
pieces = [ship, make_enemies(), bird, snake, pong]
caption = game.Text("", 12, 8, game.YELLOW)   # ป้ายสร้างหลังสุด จะได้อยู่ชั้นบน
helper = game.Text("", 12, 32, game.WHITE)
info = game.Text("", 12, 56, game.CYAN)
view = {"idx": 0, "last": None}
for d in pieces:                              # เริ่มต้น: ซ่อนทุกตัวก่อน
    for w in d["all"]:
        w.hide()
open_piece(0)

# ---- 5) วงวนหลัก ----
def on_frame():
    k = game.keys()
    go = int(game.pressed_once("right", k)) - int(game.pressed_once("left", k))
    if go:                                    # เปลี่ยนตัวเฉพาะเฟรมที่เพิ่งกด
        open_piece((view["idx"] + go) % len(PIECES))
        game.sfx("move")
        return True
    PIECES[view["idx"]][2](pieces[view["idx"]], k, game.pressed_once("a", k))
    return True

game.run(on_frame, fps=FPS)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) FRICTION = 0.80 -> 0.97 : ยานไถลยาวเหมือนบนน้ำแข็ง กว่าจะหยุดต้องรอนาน (0.5 = หยุดแทบทันที ไม่มีน้ำหนัก)
# 2) BULLET_INHERIT = 0.55 -> 0 : กระสุนพุ่งตรงขึ้นเสมอแม้ยานกำลังไถล · ลอง 1.5 แล้วกระสุนจะเฉียงแรงจนหลุดข้างจอ
# 3) GRACE = 120 -> 0 : นกตกพื้นตายทันทีตั้งแต่กระพือครั้งแรก — เห็นเลยว่าช่วงออกตัวช่วยผู้เล่นใหม่แค่ไหน
