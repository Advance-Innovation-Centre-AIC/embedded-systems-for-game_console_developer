# s13_12_sprite_gallery.py — คาบ 13 (คาบพิเศษ): ห้องแสดงภาพจากเกมตัวเต็มครบ 20 แบบ
#
# หลักการ   : ภาพ 20 แบบ แต่สร้างภาพจริงแค่ 5 ชิ้น แล้วเปลี่ยนหน้าตาด้วย .frame(ชื่อ)  (ชุดหมุนเวียน คาบ 11)
#             ช่องที่ j โชว์ภาพลำดับ (index + j - 2) % 20  วนครบรอบไม่มีหลุดขอบ   (คาบ 2)
#             ภาพไม่รู้ขนาดตัวเอง เราจดขนาดไว้ในตาราง แล้วจัดกลาง  x = cx - w // 2   (คาบ 3)
#             ภาพเคลื่อนไหว = สลับเฟรมทุก 8 เฟรม  frames[(tick // 8) % จำนวนเฟรม]   (คาบ 4, 11)
# ลองเล่น   : กด Start · จอยซ้าย/ขวา = เลื่อนดูภาพทีละแบบ (ภาพกลางกรอบเหลือง คือภาพที่เลือก)
#             boom1-boom6 จะระเบิดสลับ 2 เฟรม · snake_head_* จะหันหัวครบ 4 ทิศ
#             กด A = ฟังเสียงที่คู่กับภาพนั้นในเกมของมัน
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : Shooter (ยาน ศัตรู ระเบิด) · Pong (ลูก ไม้) · Flappy (นก ปากท่อ) · Snake (หัว ตัว อาหาร)
# ในงานจริง : หน้าจอควบคุมใช้ไอคอนชุดเดียวกันทั้งระบบ แล้วเปลี่ยนรูปตามสถานะ (มอเตอร์หมุน / หยุด / เสีย)
#             ด้วยวิธีเดียวกับ .frame() · ข้อมูลทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : shooter_full.py (pool + frame ของระเบิด) · snake_sprite_full.py (หัวงูสี่ทิศ)
#             pong_full.py (ตั้ง .w .h ให้ภาพ)
# งบวิดเจ็ต : 19 ชิ้น (กรอบ 5 + ภาพ 5 + ป้ายสีเกม 1 + ข้อความ 7 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game

# ---- 1) ตั้งค่า (แก้ได้) ----
SPRITES = [   # (ชื่อภาพ, เกม, กว้าง, สูง, เสียงที่คู่กัน) — ขนาดจริงของภาพในเฟิร์มแวร์ หน่วย px
    ("ship", "Shooter", 62, 28, "fire"),      ("enemy", "Shooter", 16, 14, "hit"),
    ("enemy2", "Shooter", 21, 10, "hit"),     ("enemy3", "Shooter", 18, 14, "hit"),
    ("boom1", "Shooter", 29, 24, "explode"),  ("boom2", "Shooter", 29, 24, "explode"),
    ("boom3", "Shooter", 29, 24, "explode"),  ("boom4", "Shooter", 29, 24, "explode"),
    ("boom5", "Shooter", 29, 24, "explode"),  ("boom6", "Shooter", 29, 24, "explode"),
    ("ball", "Pong", 14, 14, "wall"),         ("paddle", "Pong", 14, 90, "paddle"),
    ("bird", "Flappy", 44, 29, "flap"),       ("pipe_cap", "Flappy", 82, 18, "point"),
    ("snake_head_r", "Snake", 16, 16, "turn"), ("snake_head_d", "Snake", 16, 16, "turn"),
    ("snake_head_l", "Snake", 16, 16, "turn"), ("snake_head_u", "Snake", 16, 16, "turn"),
    ("snake_body", "Snake", 16, 16, "die"),   ("snake_food", "Snake", 16, 16, "eat"),
]
GAME_COLOR = {"Shooter": 0x5A3FB0, "Pong": 0x2F6F9F, "Flappy": 0x2E7D32, "Snake": game.GB_DARK}
HEADS = ("snake_head_r", "snake_head_d", "snake_head_l", "snake_head_u")
ANIM_EVERY = 8                              # สลับเฟรมทุก 8 เฟรม (30 fps = ราว 4 ครั้งต่อวินาที)
SLOT_X = [96, 246, 396, 546, 696]           # จุดกลางของช่อง 5 ช่อง (ช่องกลาง = ช่องที่ 2)
SLOT_Y = 160
N = len(SPRITES)

# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ (ทดสอบได้โดยไม่ต้องมีบอร์ด) ----
def slot_index(index, j):
    return (index + j - 2) % N              # วนรอบ: ซ้ายของภาพแรกคือภาพสุดท้าย

def centred(cx, cy, w, h):
    return (cx - w // 2, cy - h // 2)       # ภาพวางจากมุมซ้ายบน จึงต้องถอยครึ่งตัว

def anim_frames(name):
    # ภาพที่มีหลายเฟรม: ระเบิดเป็นคู่ (1,2) (3,4) (5,6) · หัวงูมี 4 ทิศ · ภาพอื่นมีเฟรมเดียว
    if name.startswith("boom"):
        k = int(name[4])
        first = k if k % 2 == 1 else k - 1
        return ("boom%d" % first, "boom%d" % (first + 1))
    if name.startswith("snake_head"):
        return HEADS
    return (name,)

def frame_at(frames, tick, every):
    return frames[(tick // every) % len(frames)]

# ---- 3) ตรวจสมองก่อนเปิดจอ ----
def self_test():
    cases = [("slot_index", slot_index(0, 0), 18), ("slot_index", slot_index(19, 4), 1),
             ("centred", centred(396, 160, 62, 28), (365, 146)),
             ("anim_frames", anim_frames("boom4"), ("boom3", "boom4")),
             ("anim_frames", anim_frames("ball"), ("ball",)),
             ("frame_at", frame_at(HEADS, 17, 8), "snake_head_l")]
    for name, got, want in cases:
        if got != want:
            print("ยังไม่ผ่าน:", name, "ได้", got, "ควรได้", want)
            return False
    print("ผ่าน! สมองของห้องภาพถูกทั้ง", len(cases), "กรณี")
    return True

if not self_test():
    raise SystemExit

# ---- 4) หน้าจอ: สร้างครั้งเดียว (กรอบก่อน ภาพทีหลัง ภาพจึงอยู่ชั้นบน) ----
game.title("SPRITES 20")
game.Text("ภาพจากเกมตัวเต็ม 20 แบบ", 12, 8, game.CYAN)
hud_index = game.Text("", 690, 8, game.WHITE)
for j in range(5):
    if j == 2: game.Box(SLOT_X[j] - 70, SLOT_Y - 60, 140, 120, 0x1E2A36, border=game.YELLOW, radius=10, border_w=3)
    else:      game.Box(SLOT_X[j] - 52, SLOT_Y - 52, 104, 104, 0x181C24, border=0x445566, radius=8)
slots = [game.Sprite(SPRITES[slot_index(0, j)][0], 0, 0) for j in range(5)]
tag = game.Box(240, 266, 130, 26, GAME_COLOR["Shooter"], radius=6)   # ป้ายสีของเกม เปลี่ยนสีตามเกม
hud_name = game.Text("", 250, 238, game.YELLOW)
hud_game = game.Text("", 250, 269, game.WHITE)
hud_size = game.Text("", 250, 300, game.WHITE)
hud_frame = game.Text("", 250, 330, game.GB_LIGHT)
game.Text("ซ้าย/ขวา = เลือกภาพ   A = ฟังเสียง", 470, 269, 0x8899AA)

st = {"index": 0, "tick": 0, "frame": ""}

def show_gallery():
    # เรียกเฉพาะตอนเลือกภาพใหม่: 5 ช่อง x (frame + move_to) = 10 ข้อความ แล้วนิ่งจนกดอีกครั้ง
    for j in range(5):
        name, g, w, h, snd = SPRITES[slot_index(st["index"], j)]
        x, y = centred(SLOT_X[j], SLOT_Y, w, h)
        slots[j].frame(name)
        slots[j].move_to(x, y)
    name, g, w, h, snd = SPRITES[st["index"]]
    hud_index.set("%d / %d" % (st["index"] + 1, N))
    hud_name.set(name)
    hud_game.set("เกม " + g)
    tag.set_color(GAME_COLOR[g])
    hud_size.set("%d x %d px   เสียง %s" % (w, h, snd))
    st["tick"] = anim_frames(name).index(name) * ANIM_EVERY   # เริ่มนับจากเฟรมของภาพที่เลือกพอดี
    st["frame"] = name
    hud_frame.set(".frame(\"%s\")" % name)

show_gallery()

# ---- 5) วงวนหลัก ----
def on_frame():
    k = game.keys()
    step = 0
    if game.pressed_once("left", k):  step = -1
    if game.pressed_once("right", k): step = 1
    if step:
        st["index"] = (st["index"] + step) % N
        game.sfx("move")
        show_gallery()
    if game.pressed_once("a", k):
        game.sfx(SPRITES[st["index"]][4])

    # ภาพกลางที่มีหลายเฟรม: สลับเฟรมเมื่อถึงจังหวะ และเขียนจอเฉพาะตอนเฟรมเปลี่ยน
    st["tick"] += 1
    frames = anim_frames(SPRITES[st["index"]][0])
    now = frame_at(frames, st["tick"], ANIM_EVERY)
    if now != st["frame"]:
        slots[2].frame(now)
        hud_frame.set(".frame(\"%s\")" % now)
        st["frame"] = now
    return True

game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) ANIM_EVERY = 8 -> 2 : ระเบิดกับหัวงูสลับเฟรมรัวจนดูสั่น · -> 20 : ช้าจนไม่เหมือนระเบิด ลองหาค่าที่ "ใช่"
# 2) แก้ ("ship", "Shooter", 62, 28, ...) เป็นกว้าง 20 : ยานไม่อยู่กลางกรอบอีกต่อไป
#    เพราะการจัดกลางเชื่อตัวเลขในตาราง ไม่ได้วัดจากภาพจริง
# 3) เปลี่ยนเสียงของ "snake_body" จาก "die" เป็น "eat" : ฟังแล้วความหมายเปลี่ยนไหม (หนึ่งเสียง หนึ่งความหมาย)
