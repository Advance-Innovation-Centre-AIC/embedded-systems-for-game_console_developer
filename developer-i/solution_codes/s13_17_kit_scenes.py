# s13_17_kit_scenes.py — คาบ 13 (คาบพิเศษ): ชุดฉากสำเร็จรูป ยกมาจากเกมเต็มของจริง พร้อมราคาวิดเจ็ตของแต่ละฉาก
#
# หลักการ   : ฉาก = กล่องที่สร้างครั้งเดียวตอนจัดฉาก ไม่มีงานต่อเฟรม แต่ "กินงบวิดเจ็ต" ทุกชิ้น  (คาบ 4, 12)
#             ของที่สร้างก่อนอยู่ชั้นล่าง · พื้นหลังต้องสร้างเป็นอย่างแรกเสมอ               (คาบ 4)
#             ท่อ Flappy: ขนาดคงที่ เลื่อนอย่างเดียว x = x - ความเร็ว*dt ห้าม resize ทุกเฟรม  (คาบ 5, 11)
#             งูกล่อง: ย้ายกล่องหางไปเป็นคอ = 2 ข้อความต่อก้าว ไม่ว่างูยาวแค่ไหน            (คาบ 11)
# ลองเล่น   : กด Start · ซ้าย/ขวา = เปลี่ยนฉาก (โชว์ทีละฉาก ลบฉากเก่าทิ้งก่อน) · A = ลูกเล่นของฉาก
#             อวกาศ: A สลับดาว 5 / 12 ดวง ดูตัวเลข "ใช้ N ชิ้น" เปลี่ยน · โต๊ะ: A เปิด/ปิดเน็ต (14 ชิ้น)
#             ทุ่งหญ้า: A เปลี่ยน EASY/NORMAL/HARD ท่อต้นถัดไปช่องแคบลง เร็วขึ้น
#             Game Boy: A เปลี่ยนสีพื้นไล่ 4 โทน ดูว่าอะไรกลืนหายไปกับพื้น
# ของบนบอร์ด: จอ · จอย
# ในเกม     : shooter_full (อวกาศ) · pong_full (โต๊ะ+เน็ต) · flappy_full (ฟ้า+หญ้า+ท่อ) · snake_full (สี GB)
# ในงานจริง : ฉากหลังของหน้าจอควบคุม = ผังโรงงาน/ผังถัง วาดครั้งเดียว · ท่อเลื่อน = สายพานที่ของไหลผ่าน
#             พื้นเรียบสีเดียวอ่านง่ายกว่าพื้นลาย (แนว high-performance HMI) · ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : full_games/shooter_full.py · pong_full.py · flappy_full.py · snake_full.py · bentogame.py
# งบวิดเจ็ต : สูงสุด 24 ชิ้น อยู่บนจอทีละฉาก (ทุ่งหญ้า: ฟ้า 8 + หญ้า 1 + ท่อ 3x4 = 21 + ป้าย 2 + game.run 1)
#             อวกาศ 6 หรือ 13 (ดาว 5/12) · โต๊ะ 19 หรือ 5 (เน็ต 14) · Game Boy 18 — ตัวเลขนี้นับจริงบนจอทุกครั้ง
#
# วิธีหยิบไปใช้: คัดลอกกล่อง "หยิบไปใช้ ... จบชิ้น" ไปวางใต้ import ของเกมน้อง แล้วเรียก make_... ครั้งเดียว
#   ก่อน game.run() เป็นอย่างแรก (ฉากต้องอยู่ชั้นล่างสุด) — ทุก make_ คืนลิสต์วิดเจ็ต len() ของมันคือราคาฉาก
import bentogame as game
import random

# ---- 1) ตั้งค่า (แก้ได้) ----
FPS = 30
DT = 1.0 / FPS          # flappy_full วัด dt จริงด้วย time.ticks_ms · ชุดนี้ใช้ dt คงที่ จะได้ import แค่ bentogame

# ---- 2) สมอง + ชิ้นสำเร็จรูป (หยิบไปใช้ได้ทีละกล่อง) ----
# ===== หยิบไปใช้: ฉากอวกาศ (จาก full_games/shooter_full.py:23-30, 67-68 · bentogame.py:846-861) =====
SPACE_BG, STAR_DIM = 0x10122C, 0xB8C0E0

def make_space(stars=5):
    # shooter_full ใช้ดาว 5 ดวง (หักจากค่าปกติ 12 ไปให้ศัตรูกับกระสุนครบ) · เกมเต็มไม่เลื่อนดาว ฟ้านิ่ง
    return [game.background(SPACE_BG)] + game.starfield(stars, STAR_DIM)
# ===== จบชิ้น =====

# ===== หยิบไปใช้: โต๊ะปิงปอง + เน็ต (จาก full_games/pong_full.py:28-29, 169-175) =====
FELT, CHALK = 0x1C7A34, 0xF0F8F0             # สักหลาดเขียว + ชอล์กขาว สีเดียวกับเกม C

def make_court(with_net=True):
    w = [game.background(FELT),
         game.Box(0, 0, game.WIDTH, 3, CHALK), game.Box(0, game.HEIGHT - 3, game.WIDTH, 3, CHALK),
         game.Box(0, 0, 3, game.HEIGHT, CHALK), game.Box(game.WIDTH - 3, 0, 3, game.HEIGHT, CHALK)]
    if with_net:
        w += game.net(CHALK)                 # เน็ต 14 ขีด = 14 ชิ้น ชิ้นแพงที่สุดของฉากนี้
    return w
# ===== จบชิ้น =====

# ===== หยิบไปใช้: ทุ่งหญ้าฟ้าไล่สี (จาก full_games/flappy_full.py:23, 35-36, 55-60) =====
GROUND_H = 20
PLAY_H = game.HEIGHT - GROUND_H              # ความสูงสนามเล่นจริง (เหนือพื้นหญ้า)
SKY_BANDS = (0x4898F0, 0x58A2F1, 0x68ADF2, 0x78B7F3, 0x88C1F5, 0x98CBF6, 0xA8D6F7, 0xB8E0F8)

def make_meadow():
    band_h, w = PLAY_H // len(SKY_BANDS), []
    for i, sky in enumerate(SKY_BANDS):      # ฟ้า 8 แถบ เข้มบน จางล่าง แทน gradient ของเกม C
        h = band_h if i < len(SKY_BANDS) - 1 else PLAY_H - band_h * i
        w.append(game.Box(0, i * band_h, game.WIDTH, h, sky))
    return w + [game.grass(GROUND_H)]
# ===== จบชิ้น =====

# ===== หยิบไปใช้: ท่อ Flappy ขนาดคงที่ เลื่อนอย่างเดียว ไม่ resize (จาก full_games/flappy_full.py:20-22, 27-33, 62-78, 97-128, 204-219) =====
# (เกมของน้องต้องมี import random ด้วย)
PIPE_SPACING, PIPE_W, CAP_W, CAP_H = 290, 64, 82, 18
PIPE_PLAY_H = game.HEIGHT - 20                # สนามเหนือพื้นหญ้า 20 px (เท่า PLAY_H ของฉากทุ่งหญ้า)
PIPE_TINTS, CAP_COLOR = (0x6CD84C, 0x4CB838, 0x84E060), 0x7CE85C   # เขียว 3 เฉด + ปากท่อสว่างกว่า
MODES = (("EASY", 175, 110.0), ("NORMAL", 130, 150.0), ("HARD", 105, 200.0))   # (ชื่อ, ช่องว่าง px, ท่อวิ่ง px/s)
CAP_SPRITE = False   # True = ปากท่อเป็นสไปรต์ pipe_cap ของเกม C (82x18 เท่ากล่องพอดี) ใช้ได้ในเกมที่สร้างท่อครั้งเดียว
                     # ไฟล์นี้ลบฉากทิ้งตอนเปลี่ยนฉาก และ BENTO Emulator ยังลบสไปรต์ไม่ได้ จึงตั้งไว้ False

def make_pipes(n=3, gap=130):
    pipes = []
    for _ in range(n):                       # ต้นละ 4 ชิ้น: ท่อบน/ล่าง "สูงเต็มสนาม" + ปากท่อบน/ล่าง
        p = {"top": game.Box(-200, 0, PIPE_W, PIPE_PLAY_H, game.GB_DARK),
             "bot": game.Box(-200, 0, PIPE_W, PIPE_PLAY_H, game.GB_DARK), "vis": True}
        for k in ("cap_t", "cap_b"):
            p[k] = game.Sprite("pipe_cap", -200, 0) if CAP_SPRITE else game.Box(-200, 0, CAP_W, CAP_H, CAP_COLOR)
        pipes.append(p)
    for i, p in enumerate(pipes):            # วางต้นแรกพ้นขอบขวา ต้นถัดไปห่างกันทีละ PIPE_SPACING
        reset_pipe(pipes, p, gap, game.WIDTH + i * PIPE_SPACING)
        render_pipe(p)
    return pipes

def reset_pipe(pipes, p, gap, first_x=None):
    # เกิดใหม่ทางขวา: สุ่มช่องว่าง ตำแหน่งช่อง เฉดสี — ไม่ resize (ท่อสูงเต็มสนามคงที่)
    p["x"] = float(first_x) if first_x is not None else max(q["x"] for q in pipes) + PIPE_SPACING
    p["gap"] = gap + random.randint(-12, 12)
    half = p["gap"] // 2
    p["gy"] = random.randint(half + 24, PIPE_PLAY_H - half - 24)
    p["gap_top"], p["gap_bot"] = max(p["gy"] - half, 20), min(p["gy"] + half, PIPE_PLAY_H - 20)
    tint = PIPE_TINTS[random.randint(0, 2)]
    p["top"].set_color(tint)
    p["bot"].set_color(tint)

def render_pipe(p):
    x = int(p["x"])
    on = -PIPE_W < x < game.WIDTH
    if on != p["vis"]:                       # โชว์/ซ่อนเฉพาะตอนเปลี่ยน
        for k in ("top", "bot", "cap_t", "cap_b"):
            if on:
                p[k].show()
            else:
                p[k].hide()
        p["vis"] = on
    if on:                                   # เลื่อนอย่างเดียว ส่วนที่พ้นขอบจอ ฮาร์ดแวร์ตัดทิ้งให้เอง
        p["top"].move_to(x, p["gap_top"] - PIPE_PLAY_H)   # ขอบล่างของท่อบน = ขอบบนของช่อง
        p["bot"].move_to(x, p["gap_bot"])            # ขอบบนของท่อล่าง = ขอบล่างของช่อง
        cap_x = x + PIPE_W // 2 - CAP_W // 2
        p["cap_t"].move_to(cap_x, p["gap_top"] - CAP_H)
        p["cap_b"].move_to(cap_x, p["gap_bot"])

def pipes_step(pipes, step_px, gap):         # เรียกทุกเฟรม step_px = ระยะเต็มพิกเซลของเฟรมนี้
    for p in pipes:
        p["x"] -= step_px
        if p["x"] + PIPE_W < 0:              # หลุดจอซ้าย: วนกลับไปเกิดใหม่ทางขวา
            reset_pipe(pipes, p, gap)
        render_pipe(p)
# ===== จบชิ้น =====

# ===== หยิบไปใช้: พาเลต Game Boy 4 สี + งูกล่องเดินด้วยเคล็ดย้ายหาง (จาก bentogame.py:37-41 · full_games/snake_full.py:12, 61-70, 137-147) =====
GB = (("GB_DARKEST", game.GB_DARKEST), ("GB_DARK", game.GB_DARK),
      ("GB_LIGHT", game.GB_LIGHT), ("GB_LIGHTEST", game.GB_LIGHTEST))   # เข้มสุด -> สว่างสุด (ค่าเดียวกับเกม C)
CELL = 16

def make_box_snake(col, row, length=5):     # หัวเข้มสุด ตัวเข้ม แบบ snake_full
    body = [[col - i, row] for i in range(length)]
    head = game.Box(body[0][0] * CELL, body[0][1] * CELL, CELL - 2, CELL - 2, game.GB_DARKEST)
    boxes = [game.Box(c[0] * CELL, c[1] * CELL, CELL - 2, CELL - 2, game.GB_DARK) for c in body[1:]]
    return {"body": body, "head": head, "boxes": boxes}

def box_snake_step(sn, dx, dy):
    # ทั้งตัวดูเหมือนขยับ แต่ที่เปลี่ยนจริงมีแค่หัวกับหาง: ย้าย "กล่องหาง" ไปสวมตำแหน่งหัวเก่า
    old = sn["body"][0]
    new = [old[0] + dx, old[1] + dy]
    sn["body"].insert(0, new)
    sn["body"].pop()
    sn["head"].move_to(new[0] * CELL, new[1] * CELL)
    box = sn["boxes"].pop()
    box.move_to(old[0] * CELL, old[1] * CELL)
    sn["boxes"].insert(0, box)                # กลายเป็นคอ (ต่อจากหัว)
# ===== จบชิ้น =====


# ---- 4) หน้าจอ: โชว์ทีละฉาก ----
# เปลี่ยนฉาก = ลบวิดเจ็ตของฉากเก่าทิ้งทุกชิ้น แล้วสร้างฉากใหม่ เกิดเฉพาะเฟรมที่เพิ่งกด (ไม่ใช่ทุกเฟรม งบจึงไม่บาน)
# ไม่ใช้ game.clear() เพราะล้างทั้งจอแล้ว BENTO Emulator จะซ่อนจอราว 2 วินาทีกันภาพกระพริบ
# ฉากใหม่สร้างทีหลังป้ายปุ่มของ game.run() เลยทับป้ายนั้น — ป้ายบนสุดของไฟล์นี้จึงบอกปุ่มแทน
def build_space(opt):
    return make_space(12 if opt % 2 else 5), {}

def build_court(opt):
    return make_court(opt % 2 == 0), {}

def build_meadow(opt):
    w = make_meadow()
    pipes = make_pipes(3, MODES[opt % 3][1])
    return w + [p[k] for p in pipes for k in ("top", "bot", "cap_t", "cap_b")], {"pipes": pipes, "acc": 0.0}

def play_meadow(d, opt):
    name, gap, speed = MODES[opt % 3]
    d["acc"] += speed * DT                    # สะสมเศษพิกเซล ครบ 1 px ค่อยขยับ (flappy_full.py:204-206)
    step = int(d["acc"])
    d["acc"] -= step
    pipes_step(d["pipes"], step, gap)
    say("A = %s  ช่อง %d  ท่อ %d px/s   ซ้าย/ขวา = ฉากอื่น" % (name, gap, int(speed)))

def build_gb(opt):
    w = [game.background(GB[3 - opt % 4][1])]
    for i, (name, color) in enumerate(GB):   # แถบตัวอย่างสี 4 โทน + ชื่อ + ค่าสี
        w.append(game.Box(60 + i * 180, 96, 140, 56, color, border=game.GB_DARKEST, border_w=2, radius=6))
        w.append(game.Text(name, 60 + i * 180, 160, game.GB_DARKEST))
        w.append(game.Text("0x%06X" % color, 60 + i * 180, 184, game.GB_DARKEST))
    sn = make_box_snake(12, 16)
    return w + [sn["head"]] + sn["boxes"], {"sn": sn, "dir": (1, 0), "t": 0}

def play_gb(d, opt):
    say("A = สีพื้น: %s   ซ้าย/ขวา = ฉากอื่น" % GB[3 - opt % 4][0])
    d["t"] += 1
    if d["t"] % 3:                            # ก้าวทุก 3 เฟรม
        return
    c, r = d["sn"]["body"][0]                 # เดินวนสี่เหลี่ยมใต้แถบสี: ขวา ลง ซ้าย ขึ้น
    turn = {(1, 0): (c >= 42, (0, 1)), (0, 1): (r >= 21, (-1, 0)),
            (-1, 0): (c <= 6, (0, -1)), (0, -1): (r <= 16, (1, 0))}[d["dir"]]
    if turn[0]:
        d["dir"] = turn[1]
    box_snake_step(d["sn"], d["dir"][0], d["dir"][1])

SCENES = (("SPACE  shooter_full", build_space, None, game.YELLOW, "A = ดาว 5/12 ดวง   ซ้าย/ขวา = ฉากอื่น"),
          ("COURT  pong_full", build_court, None, game.WHITE, "A = เปิด/ปิดเน็ต   ซ้าย/ขวา = ฉากอื่น"),
          ("MEADOW + PIPES  flappy_full", build_meadow, play_meadow, game.WHITE, ""),
          ("GAME BOY  snake_full", build_gb, play_gb, game.GB_DARKEST, ""))
view = {"idx": 0, "made": [], "opt": [0, 0, 1, 0], "cur": None, "help": None, "last": None}

def open_scene(i):                            # ลบฉากเก่าทุกชิ้น สร้างฉากใหม่ แล้วสร้างป้ายทับไว้ชั้นบนสุด
    for w in view["made"]:
        w.delete()
    name, build, play, ink, hlp = SCENES[i]
    made, view["cur"] = build(view["opt"][i])
    cap = game.Text("%d/%d  %s   ใช้ %d ชิ้น" % (i + 1, len(SCENES), name, len(made)), 12, 8, ink)
    view["help"], view["last"], view["idx"] = game.Text(hlp, 12, 32, ink), hlp, i
    view["made"] = made + [cap, view["help"]]

def say(msg):                                 # เขียนป้ายเฉพาะตอนข้อความเปลี่ยน
    if msg != view["last"]:
        view["help"].set(msg)
        view["last"] = msg

# ---- 5) วงวนหลัก ----
def on_frame():
    k = game.keys()
    go = int(game.pressed_once("right", k)) - int(game.pressed_once("left", k))
    i = view["idx"]
    if game.pressed_once("a", k):
        view["opt"][i] += 1
        game.sfx("select")
        if SCENES[i][2] is None:              # ฉากนิ่ง: ตัวเลือกเปลี่ยนจำนวนชิ้น ต้องสร้างฉากใหม่
            open_scene(i)
        elif i == 3:                          # Game Boy: เปลี่ยนแค่สีพื้น ไม่ต้องสร้างใหม่
            view["made"][0].set_color(GB[3 - view["opt"][i] % 4][1])
    if go:                                    # เปลี่ยนฉากเฉพาะเฟรมที่เพิ่งกด
        open_scene((i + go) % len(SCENES))
        game.sfx("move")
    elif SCENES[i][2] is not None:
        SCENES[i][2](view["cur"], view["opt"][i])
    return True

game.title("KIT SCENES")
open_scene(0)
game.run(on_frame, fps=FPS)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) ใน make_meadow ลด SKY_BANDS เหลือ 4 สี : ฉากถูกลง 4 ชิ้น แต่ขอบแถบฟ้าเห็นชัดขึ้น (ยิ่งซอยถี่ยิ่งเนียน)
# 2) PIPE_SPACING = 290 -> 400 : ท่อห่างขึ้น บินผ่านง่ายขึ้น (ถ้าต่ำกว่า 290 ท่อ 3 ต้นจะเรียงไม่พอความกว้างจอ แล้วท่อใหม่โผล่กลางจอ)
# 3) make_box_snake(12, 16) -> make_box_snake(12, 16, 9) : งูยาวขึ้นเกือบเท่าตัว แต่แต่ละก้าวยังส่งแค่ 2 ข้อความเท่าเดิม
