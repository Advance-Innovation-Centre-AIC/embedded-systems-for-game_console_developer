# s13_18_kit_props_hud.py — คาบ 13 (คาบพิเศษ): ชุดของประกอบ + ป้าย HUD สำเร็จรูป ยกมาจากเกมเต็มของจริง
#
# หลักการ   : ชุดหมุนเวียน  สร้างกระสุนครบตั้งแต่แรกแล้วซ่อน ยิง = หยิบนัดว่าง หลุดจอ = เก็บคืน   (คาบ 11)
#             กล่องแต่งขอบ  game.Box(..., border=สีขอบ, radius=ความมน, border_w=ความหนา)        (คาบ 4)
#             สุ่มจนกว่าจะได้  วางอาหารซ้ำจนได้ช่องที่ไม่ทับตัวงู                               (คาบ 4)
#             เครื่องสถานะ   เมนูเลื่อนวนหัวท้าย sel = (sel + 1) % n · เล่นอยู่ / จบตา             (คาบ 2, 7)
#             HUD อัปเดตเมื่อเปลี่ยน  จำค่าชุดล่าสุดไว้ ต่างจากเดิมค่อย set()                 (คาบ 10, 12)
# ลองเล่น   : กด Start · ซ้าย/ขวา = เปลี่ยนชิ้น (โชว์ทีละชิ้น) · A = ท่าของชิ้นนั้น
#             กระสุน: A ยิง กดรัวจนพูลหมดแล้วดูว่ายิงไม่ออก · อาหาร: A วางใหม่ ไม่เคยทับตัวงู
#             เมนู: UP/DOWN เลื่อน A ยืนยัน · จบเกม: คะแนนวิ่งเอง A = จบตา ดู Best · HUD: A ได้แต้ม B เสียชีวิต
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : shooter_full (กระสุน HUD) · snake_full/snake_sprite_full (อาหาร) · ทุกเกมเต็ม (เมนู ป้ายจบเกม Best)
# ในงานจริง : กระสุนชุดหมุนเวียน = กล่องพัสดุบนสายพาน · เมนู = หน้าเลือกสูตรผลิต · ป้ายจบเกม = ป้ายหยุดเครื่อง
#             HUD = แถบยอดผลิต วาดเฉพาะตอนเปลี่ยน จอนิ่ง อ่านง่าย · ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : full_games/shooter_full.py · snake_full.py · snake_sprite_full.py · bentogame.py (menu, game_over, best)
# งบวิดเจ็ต : 25 ชิ้น สร้างครบครั้งเดียว โชว์ทีละชิ้น (พื้นหลัง 1 + กระสุน 5 + อาหาร 5 + เมนู 5 + จบเกม 5
#             + HUD 1 + ป้ายล่าง 2 + game.run 1) — ไม่สร้างไม่ลบระหว่างเล่น
#
# วิธีหยิบไปใช้: คัดลอกกล่อง "หยิบไปใช้ ... จบชิ้น" ไปวางใต้ import ของเกมน้อง สร้างด้วย make_... ครั้งเดียวก่อน
#   game.run() แล้วเรียกฟังก์ชันที่เหลือใน on_frame — ตัวอย่างการต่อสายอยู่ในส่วนที่ 4
import bentogame as game
import random

# ---- 1) ตั้งค่า (แก้ได้) ----
FPS = 30
SPACE = 0x10122C        # สีอวกาศ (shooter_full.py:67)
NAME = "kit"            # ชื่อช่อง Best ของไฟล์นี้ (เกมเต็มใช้ "shooter" "flappy" "snake" "pong")

# ---- 2) สมอง + ชิ้นสำเร็จรูป (หยิบไปใช้ได้ทีละกล่อง) ----
# ===== หยิบไปใช้: กระสุนเลเซอร์ขอบขาวมุมมน + ชุดหมุนเวียน (จาก full_games/shooter_full.py:19, 32, 70-74, 115-117, 137-148, 205-214) =====
LASER, BULLET_VY = 0x80F4FF, 8.0             # ฟ้าลำแสงสีเดียวกับเกม C · พุ่งขึ้นเฟรมละ 8 px

def make_bullets(n=4):
    bl = {"box": [], "on": [False] * n, "x": [0.0] * n, "y": [0.0] * n}
    for _ in range(n):                        # แท่ง 5x12 ขอบขาว มุมมน 2 — สร้างครั้งเดียวแล้วซ่อน
        b = game.Box(-20, -50, 5, 12, LASER, border=game.WHITE, radius=2)
        b.hide()
        bl["box"].append(b)
    return bl

def bullet_fire(bl, x, y):                    # คืน False เมื่อไม่มีนัดว่าง (ยิงไม่ออก ไม่สร้างนัดใหม่)
    for j in range(len(bl["on"])):
        if not bl["on"][j]:
            bl["on"][j], bl["x"][j], bl["y"][j] = True, x, y
            bl["box"][j].move_to(x, y)
            bl["box"][j].show()
            game.sfx("fire")
            return True
    return False

def bullets_step(bl):                         # เรียกทุกเฟรม: พุ่งขึ้น หลุดขอบบนแล้วเก็บคืนพูล
    for j in range(len(bl["on"])):
        if bl["on"][j]:
            bl["y"][j] -= BULLET_VY
            if bl["y"][j] < -12:
                bl["on"][j] = False
                bl["box"][j].hide()
            else:
                bl["box"][j].move_to(bl["x"][j], bl["y"][j])
# ===== จบชิ้น =====

# ===== หยิบไปใช้: อาหาร/ผลไม้ วางสุ่มบนตาราง ไม่ทับตัวงู (จาก full_games/snake_sprite_full.py:14-15, 64, 78-84 · snake_full.py:29, 71, 85-92) =====
# (เกมของน้องต้องมี import random ด้วย)
CELL = 16
COLS, ROWS = game.WIDTH // CELL, game.HEIGHT // CELL   # 49 x 24 ช่อง
APPLE = 0xD82818                                     # แอปเปิลกล่องแดงของ snake_full

def make_food(sprite=True):                   # True = แอปเปิลพิกเซลของเกม C · False = กล่องแดง 14x14
    w = game.Sprite("snake_food", 0, 0) if sprite else game.Box(0, 0, CELL - 2, CELL - 2, APPLE)
    return {"w": w, "cell": [0, 0]}

def place_food(food, body, rows=(0, ROWS)):   # rows = ช่วงแถวที่ยอมให้วาง (เว้นที่ให้ HUD ได้)
    while True:                               # สุ่มไปเรื่อย ๆ จนเจอช่องที่ไม่ทับตัวงู
        c = [random.randint(0, COLS - 1), random.randint(rows[0], rows[1] - 1)]
        if c not in body:
            food["cell"] = c
            food["w"].move_to(c[0] * CELL, c[1] * CELL)
            return
# ===== จบชิ้น =====

# ===== หยิบไปใช้: กล่องเมนูแบบเกม C ที่ไม่หยุดเกม (จาก bentogame.py:1099-1133, 1145-1155 ของ game.menu) =====
MENU_BG, MENU_EDGE, MENU_DIM = 0x101A38, 0x80F4FF, 0x6C7CA8   # น้ำเงินเข้ม ขอบฟ้า มุมมน · แถวที่ไม่เลือกสีเทา

def make_menu(options, heading="SELECT MODE", sel=1):
    n = len(options)
    panel_h = 70 + 34 * n
    top = game.HEIGHT // 2 - panel_h // 2
    m = {"opt": options, "sel": sel, "heading": heading,
         "box": game.Box(game.WIDTH // 2 - 160, top, 320, panel_h, MENU_BG, border=MENU_EDGE, border_w=2, radius=10),
         "head": game.Text(heading, game.WIDTH // 2 - len(heading) * 4, top + 14, game.WHITE),
         "rows": [game.Text("", game.WIDTH // 2 - (len(options[i]) + 6) * 4, top + 50 + i * 34, game.WHITE)
                  for i in range(n)]}
    menu_paint(m)
    return m

def menu_paint(m):                            # ตัวที่เลือก = ">  ชื่อ  <" สีฟ้า ตัวอื่นสีเทา
    for i, row in enumerate(m["rows"]):
        row.set(">  %s  <" % m["opt"][i] if i == m["sel"] else "   %s" % m["opt"][i])
        try:
            row._label.color(MENU_EDGE if i == m["sel"] else MENU_DIM)   # ท่าเดียวกับ game.menu
        except Exception:
            pass

def menu_step(m, up, down):                   # ส่ง pressed_once ของ up/down มา · เลื่อนวนหัวท้าย
    n = len(m["opt"])
    if up:
        m["sel"] = (m["sel"] + n - 1) % n
    elif down:
        m["sel"] = (m["sel"] + 1) % n
    else:
        return
    game.sfx("move")
    menu_paint(m)
# ===== จบชิ้น =====

# ===== หยิบไปใช้: ป้ายจบเกม + Best แบบไม่หยุดเกม (จาก bentogame.py:1008-1019, 1043-1060 · shooter_full.py:270-277) =====
PANEL_BG, PANEL_EDGE = 0x101A38, 0x80F4FF    # สีกล่องเดียวกับ game.game_over (bentogame.py:1046-1047)

def make_over_panel():                        # กล่อง 360x140 น้ำเงินเข้ม ขอบฟ้า มุมมน
    x, y = game.WIDTH // 2 - 180, game.HEIGHT // 2 - 70
    p = {"box": game.Box(x, y, 360, 140, PANEL_BG, border=PANEL_EDGE, border_w=2, radius=10),
         "t": [game.Text("", x + 28, y + 22 + i * 36, c) for i, c in enumerate((game.CYAN, game.WHITE, game.GB_LIGHT))]}
    p["box"].hide()
    return p

def over_show(p, score, name, msg="GAME OVER", hint="A = เล่นใหม่"):
    best = game.save_best(score, name)        # บันทึกเข้าช่อง Best ก่อนแล้วค่อยโชว์ (game_over ทำแบบนี้)
    for t, text in zip(p["t"], (msg, "Score %d      Best %d" % (score, best), hint)):
        t.set(text)
    p["box"].show()

def over_hide(p):
    p["box"].hide()
    for t in p["t"]:                          # ป้ายข้อความซ่อนไม่ได้ จึงเขียนเป็นว่างแทน
        t.set("")
# ===== จบชิ้น =====

# ===== หยิบไปใช้: HUD คะแนน/Best/ชีวิต วาดใหม่เฉพาะตอนเลขเปลี่ยน (จาก full_games/shooter_full.py:77, 89-96) =====
def make_hud(x=10, y=8):
    return {"t": game.Text("", x, y, game.WHITE), "last": None, "draws": 0}

def hud_update(h, score, lives, name="game"):
    row = (score, max(game.best(name), score), lives)    # Best ขยับตามทันทีเมื่อแซง แบบเกม C
    if row != h["last"]:
        h["last"] = row
        h["t"].set("Score: %d    Best: %d    Lives: %d" % row)
        h["draws"] += 1
# ===== จบชิ้น =====


# ---- 4) หน้าจอ: สร้างครบทุกชิ้นครั้งเดียว แล้วโชว์ทีละชิ้น ----
def enter_bullets(d):
    bg.set_color(SPACE)
    d["on"] = [False] * len(d["on"])

def play_bullets(d, k, a):
    if a and not bullet_fire(d, gun.x + 29, gun.y - 8):  # ยิงจากหัวยาน (ยานกว้าง 62)
        game.sfx("deny")
    bullets_step(d)
    say("ในอากาศ %d/%d นัด   หมดพูล = ยิงไม่ออก" % (d["on"].count(True), len(d["on"])))

def enter_food(d):
    bg.set_color(0x4CA838)                    # สนามหญ้า (snake_sprite_full.py:25)
    d["n"] = 0
    play_food(d, None, True)

def play_food(d, k, a):
    if a:
        for f in d["foods"]:
            place_food(f, d["body"], (4, 19))  # เว้นแถวบนกับล่างให้ป้าย
        game.sfx("eat")
        d["n"] += 1
    say("วางใหม่ %d ครั้ง  ไม่เคยทับตัวงู" % d["n"])

def enter_menu(m):
    bg.set_color(SPACE)
    m["head"].set(m["heading"])
    menu_paint(m)

def play_menu(m, k, a):
    menu_step(m, game.pressed_once("up", k), game.pressed_once("down", k))
    if a:
        game.sfx("select")
        say("เลือก %s แล้ว   ในเกมเต็มคือ mode = picked" % m["opt"][m["sel"]])

def enter_over(d):
    bg.set_color(SPACE)
    d["score"], d["t"], d["over"], d["hud"]["last"] = 0, 0, False, None

def play_over(d, k, a):
    if a and d["over"]:                       # เล่นใหม่: ซ่อนป้าย เริ่มนับคะแนนใหม่
        over_hide(d["panel"])
        d["score"], d["over"] = 0, False
        game.sfx("start")
    elif a:                                   # จบตา: บันทึก Best + ขึ้นป้าย (เกมยังเดินต่อได้)
        over_show(d["panel"], d["score"], NAME)
        d["over"] = True
        game.sfx("gameover")
    d["t"] += 1
    if not d["over"] and d["t"] % 20 == 0:    # จำลองว่ากำลังเล่น: ได้แต้มทุก 20 เฟรม
        d["score"] += 1
    hud_update(d["hud"], d["score"], 3, NAME)
    say("ป้ายจบเกม" if d["over"] else "คะแนนวิ่งเอง  A = จบตา แล้วดู Best")

def enter_hud(h):
    bg.set_color(SPACE)
    h["score"], h["lives"], h["frames"], h["last"], h["draws"] = 0, 3, 0, None, 0

def play_hud(h, k, a):
    if a:
        h["score"] += 1
        game.sfx("hit")
    if game.pressed_once("b", k):
        h["lives"] -= 1
        game.sfx("lose_life")
        if h["lives"] == 0:                   # ชีวิตหมด: บันทึก Best แล้วเริ่มใหม่
            game.save_best(h["score"], NAME)
            game.sfx("gameover")
            h["score"], h["lives"] = 0, 3
    hud_update(h, h["score"], h["lives"], NAME)
    h["frames"] += 1
    if h["frames"] % FPS == 0:                # ป้ายนี้เองก็เขียนแค่วินาทีละครั้ง
        say("%d เฟรม  แต่วาด HUD ใหม่ %d ครั้ง" % (h["frames"], h["draws"]))

def say(msg):                                 # เขียนป้ายล่างเฉพาะตอนข้อความเปลี่ยน
    if msg != view["last"]:
        info.set(msg)
        view["last"] = msg

def hide(w):
    if hasattr(w, "hide"):
        w.hide()
    else:
        w.set("")                             # ป้ายข้อความ (Text) ไม่มี hide ใช้ข้อความว่างแทน

def open_piece(i):                            # ซ่อนชิ้นเก่าทั้งชุด โชว์ชิ้นใหม่ แล้วเริ่มสถานะใหม่
    for w in pieces[view["idx"]]["all"]:
        hide(w)
    view["idx"] = i
    say("")
    for w in pieces[i]["base"]:
        w.show()
    PIECES[i][1](pieces[i])
    caption.set("%d/%d  %s   %s" % (i + 1, len(PIECES), PIECES[i][0], PIECES[i][3]))

PIECES = (("BULLETS", enter_bullets, play_bullets, "A = ยิง"),
          ("FOOD", enter_food, play_food, "A = วางใหม่"),
          ("MENU", enter_menu, play_menu, "UP/DOWN + A"),
          ("GAME OVER + BEST", enter_over, play_over, "A = จบตา / เล่นใหม่"),
          ("HUD", enter_hud, play_hud, "A = ได้แต้ม  B = เสียชีวิต"))

game.title("KIT PROPS + HUD")
bg = game.background(SPACE)                   # พื้นหลังชิ้นเดียวใช้ทุกชิ้น เปลี่ยนแค่สี
gun = game.Sprite("ship", game.WIDTH // 2 - 31, 250)
bullets = make_bullets(4)
bullets["all"], bullets["base"] = [gun] + bullets["box"], [gun]
body = [[c, 10] for c in range(22, 25)]       # ตัวงูสมมุติ 3 ช่อง ไว้ให้ place_food หลบ
food = {"foods": [make_food(True), make_food(False)], "body": body,
        "segs": [game.Sprite("snake_body", c * CELL, r * CELL) for c, r in body]}
food["all"] = food["base"] = [f["w"] for f in food["foods"]] + food["segs"]
menu = make_menu(("EASY", "NORMAL", "HARD"))
menu["all"], menu["base"] = [menu["box"], menu["head"]] + menu["rows"], [menu["box"]]
over = {"hud": make_hud(), "panel": make_over_panel()}
over["all"], over["base"] = [over["hud"]["t"], over["panel"]["box"]] + over["panel"]["t"], []
hud = make_hud()
hud["all"], hud["base"] = [hud["t"]], []
pieces = [bullets, food, menu, over, hud]
caption = game.Text("", 12, game.HEIGHT - 74, game.YELLOW)   # ป้ายล่าง สร้างหลังสุดจะได้อยู่ชั้นบน
info = game.Text("", 12, game.HEIGHT - 50, game.CYAN)
view = {"idx": 0, "last": None}
for d in pieces[1:]:                          # เริ่มต้น: ซ่อนทุกชิ้นยกเว้นชิ้นแรก
    for w in d["all"]:
        hide(w)
open_piece(0)

# ---- 5) วงวนหลัก ----
def on_frame():
    k = game.keys()
    go = int(game.pressed_once("right", k)) - int(game.pressed_once("left", k))
    if go:                                    # เปลี่ยนชิ้นเฉพาะเฟรมที่เพิ่งกด
        open_piece((view["idx"] + go) % len(PIECES))
        game.sfx("move")
        return True
    PIECES[view["idx"]][2](pieces[view["idx"]], k, game.pressed_once("a", k))
    return True

game.run(on_frame, fps=FPS)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) make_bullets(4) -> make_bullets(2) : กด A รัวแล้วได้เสียง deny บ่อยขึ้น เพราะนัดว่างในพูลหมดไวขึ้น
# 2) ใน make_bullets เปลี่ยน radius=2 -> radius=6 และ border=game.WHITE -> game.PINK : กระสุนกลมมนขอบชมพู
# 3) ใน hud_update ย้ายทั้ง h["t"].set(...) และ h["draws"] += 1 ออกมานอก if : ตัวเลขเหมือนเดิม แต่ป้ายล่างจะบอกว่าวาดใหม่ทุกเฟรม
