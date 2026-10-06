# s13_19_kit_sounds.py — คาบ 13 (คาบพิเศษ): สูตรเสียงประกอบจากเกมเต็มของจริง เหตุการณ์ไหนใช้เสียงไหน + ทำนองแบบไม่ใช้ sleep
#
# หลักการ   : หนึ่งเสียง หนึ่งความหมาย ดังเฉพาะตอนเกิดเหตุ — game.sfx("ชื่อ") ตรงบรรทัดที่เหตุเกิด    (คาบ 8)
#             โน้ตเอง game.tone(โน้ต MIDI, ms=...) · 60 = โดกลาง · f = 440 * 2^((n - 69)/12)          (คาบ 8)
#             tone() ไม่ต่อคิว และห้าม sleep ในวงวน — ทำนองจึงใช้ตัวนับเฟรม: ถึงคิวค่อยเล่นโน้ตถัดไป   (คาบ 4, 11)
# ลองเล่น   : กด Start (ฟังทำนองเปิดเกม melody=True) · ซ้าย/ขวา = เปลี่ยนเกม · UP/DOWN = เลือกแถว · A = เล่น
#             ป้ายล่างบอกว่ากำลังเล่นอะไร มาจากบรรทัดไหนของเกมเต็ม · หน้าสุดท้าย MELODY ดูไฟวิ่งตามระดับโน้ต
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : ตารางทั้งหมดอ่านจากโค้ดจริงของ shooter_full · flappy_full · pong_full · snake_full · bentogame
# ในงานจริง : เสียงเตือนของเครื่องจักรก็ใช้กฎเดียวกัน — ด่วนมาก = ถี่ ซ้ำ จนมีคนรับทราบ · แจ้งให้ทราบ = ครั้งเดียว
#             ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : s08_sound.py · full_games/*.py · bentogame.py (sfx, tone, jingle, title)
# งบวิดเจ็ต : 13 ชิ้น (พื้นหลัง 1 + แถบเลือก 1 + หัวข้อ 1 + แถว 6 + ป้ายกำลังเล่น 1 + ป้ายปุ่ม 1 + ไฟโน้ต 1 + game.run 1)
#
# วิธีหยิบไปใช้: ตารางเสียง = อ่านแล้วใส่ game.sfx(...) ในเกมของน้องตรงเหตุการณ์เดียวกัน
#   เครื่องเล่นทำนอง + สูตรทำนอง = คัดลอกทั้งสองกล่อง สร้าง make_player() ครั้งเดียว เรียก player_step() ทุกเฟรม
import bentogame as game

# ---- 1) ตั้งค่า (แก้ได้) ----
FPS = 30
ROWS_Y, ROW_H = 60, 34                        # แถวแรกอยู่ที่ y = 60 ห่างกันแถวละ 34 px

# ---- 2) สมอง + ชิ้นสำเร็จรูป (หยิบไปใช้ได้ทีละกล่อง) ----
# ===== หยิบไปใช้: ตารางเหตุการณ์ -> เสียง ของเกมเต็มทั้ง 4 เกม (อ่านจากโค้ดจริง เลขบรรทัดอยู่ท้ายแถว) =====
# (ชื่อเสียงสำหรับ game.sfx, เหตุการณ์ในเกม, ไฟล์:บรรทัด ที่เรียกเสียงนั้น)
SOUND_TABLES = (
    ("SHOOTER", (("fire", "ยิงกระสุน", "shooter_full.py:142"),
                 ("explode", "ระเบิด ทุกครั้งที่บึ้ม", "shooter_full.py:152"),
                 ("hit", "ยิงโดนศัตรู (ซ้อนเสียงระเบิด)", "shooter_full.py:242"),
                 ("lose_life", "ศัตรูชนยาน ยังรอด", "shooter_full.py:259"),
                 ("gameover", "ชีวิตหมด", "shooter_full.py:229, 256"))),
    ("FLAPPY", (("flap", "กระพือปีก", "flappy_full.py:177"),
                ("point", "บินผ่านท่อ ได้ 1 คะแนน", "flappy_full.py:213"),
                ("fall", "ตกพื้น หรือชนท่อ", "flappy_full.py:197, 221"))),
    ("PONG", (("wall", "ลูกเด้งขอบบน/ล่าง", "pong_full.py:101, 105"),
              ("paddle", "ตีโดนไม้", "pong_full.py:115, 122"),
              ("pong_score", "ได้แต้ม แมตช์ยังไม่จบ", "pong_full.py:134, 142"),
              ("win", "ผู้เล่นชนะแมตช์", "pong_full.py:140"),
              ("lose", "AI ชนะแมตช์", "pong_full.py:132"))),
    ("SNAKE", (("eat", "กินแอปเปิล", "snake_full.py:127"),
               ("COIN", "ทุกลูกที่ 10 เสียงเหรียญ 2 โน้ต", "snake_full.py:128-129"),
               ("point", "ทุกลูกที่ 10 (ฉบับสไปรต์)", "snake_sprite_full.py:132-133"),
               ("die", "ชนกำแพง", "snake_full.py:119"),
               ("lose", "กัดตัวเอง (คนละเสียงกับชนกำแพง)", "snake_full.py:121"))),
    ("SYSTEM", (("start", "กด Start หน้าเริ่ม / เล่นใหม่", "bentogame.py:915, 1072"),
                ("select", "พักเกม / ยืนยันเมนู", "bentogame.py:991, 1154"),
                ("move", "เลื่อนเมนูขึ้นลง", "bentogame.py:1147, 1151"),
                ("back", "กด Back ในเมนู", "bentogame.py:1142"))),
)
# ===== จบชิ้น =====

# ===== หยิบไปใช้: เครื่องเล่นทำนองแบบนับเฟรม ไม่มี sleep (แทน game.jingle ที่รอด้วย sleep — bentogame.py:869-877) =====
# ทำนอง = ลำดับขั้น (โน้ต MIDI, ยาวกี่ ms, อีกกี่เฟรมค่อยเล่นขั้นถัดไป) · โน้ต 0 = เว้นจังหวะ
def make_player():
    return {"steps": (), "i": 0, "wait": 0}

def play(pl, steps):                          # เริ่มทำนองใหม่ (ทับทำนองเดิมที่ยังเล่นไม่จบ)
    pl["steps"], pl["i"], pl["wait"] = steps, 0, 0

def player_step(pl):                          # เรียกทุกเฟรม คืนโน้ตที่เพิ่งเริ่ม (0 = เว้น) หรือ None ถ้ายังไม่ถึงคิว
    if pl["i"] >= len(pl["steps"]):
        return None
    if pl["wait"] > 0:                        # ยังไม่ถึงคิว: นับถอยหลังแล้วกลับไปทำงานอื่นก่อน
        pl["wait"] -= 1
        return None
    note, ms, frames = pl["steps"][pl["i"]]
    if note:
        game.tone(note, ms=ms)
    pl["i"] += 1
    pl["wait"] = frames - 1
    return note

def jingle_steps(notes, ms=90, frames=3):     # แปลงโน้ตแบบ game.jingle(...) เป็นขั้นของเครื่องเล่นนี้
    return tuple((n, ms, frames) for n in notes)
# ===== จบชิ้น =====

# ===== หยิบไปใช้: สูตรทำนองจากเกมเต็ม (จาก full_games/snake_full.py:32-37 · bentogame.py:869-871, 907-908) =====
# ตัวเลขเฟรมคิดที่ 30 fps (1 เฟรม = 33 ms) ใช้คู่กับเครื่องเล่นทำนองกล่องก่อนหน้า
COIN = ((83, 60, 1), (88, 160, 1))            # เหรียญ cha-ching: B5 สั้น แล้ว E6 ยาว (เกมเต็มเว้น 45 ms ด้วย sleep)
TITLE = ((60, 90, 3), (64, 90, 3), (67, 90, 3), (72, 90, 3))   # โด มี ซอล โดสูง = game.title(..., melody=True)
TEAM = ((64, 110, 4), (0, 110, 4), (64, 110, 4), (67, 110, 4), (72, 110, 4))   # ตัวอย่างในคู่มือ game.jingle
# ===== จบชิ้น =====


# ---- 4) หน้าจอ: สร้างครั้งเดียว เปลี่ยนหน้า = เขียนข้อความใหม่ลงแถวเดิม ----
PAGES = SOUND_TABLES + (("MELODY", (("TITLE", "ทำนองเปิดเกม (ทุกเกมเต็มใช้)", "bentogame.py:869, 907"),
                                    ("COIN", "เหรียญ 2 โน้ตของ snake_full", "snake_full.py:32-37"),
                                    ("TEAM", "ทำนองของทีม แต่งเองได้", "bentogame.py:871"))),)
RECIPES = {"TITLE": TITLE, "COIN": COIN, "TEAM": TEAM}

game.title("KIT SOUNDS", melody=True)         # ทำนองเปิดเกมใช้ sleep ได้ เพราะยังไม่เข้าวงวนเกม
game.background(0x10122C)
bar = game.Box(20, ROWS_Y - 5, 752, ROW_H - 4, 0x24406A, border=0x80F4FF, radius=6)   # แถบบอกแถวที่เลือก
header = game.Text("", 24, 16, game.YELLOW)
rows = [game.Text("", 36, ROWS_Y + i * ROW_H, game.WHITE) for i in range(6)]
now = game.Text("", 24, 290, game.CYAN)
game.Text("ซ้าย/ขวา = เกมอื่น   UP/DOWN = เลือก   A = เล่น", 24, 330, game.WHITE)
lamp = game.Box(700, 286, 24, 24, 0x334455, radius=12)   # ไฟโน้ต: ติดเมื่อมีเสียง วิ่งตามระดับโน้ต
player = make_player()
view = {"page": 0, "sel": 0, "last": None, "lamp": 0, "name": ""}

def show_page():
    name, table = PAGES[view["page"]]
    header.set("%d/%d  %s   เหตุการณ์ -> เสียง" % (view["page"] + 1, len(PAGES), name))
    for i, row in enumerate(rows):
        row.set("%-11s %s" % (table[i][0], table[i][1]) if i < len(table) else "")
    view["sel"] = min(view["sel"], len(table) - 1)
    bar.move_to(20, ROWS_Y - 5 + view["sel"] * ROW_H)

def say(msg):                                 # เขียนป้ายเฉพาะตอนข้อความเปลี่ยน
    if msg != view["last"]:
        now.set(msg)
        view["last"] = msg

def light(x):                                 # ไฟโน้ตติด 6 เฟรม แล้วดับเอง (นับเฟรม ไม่ใช้ sleep)
    if view["lamp"] == 0:
        lamp.set_color(game.YELLOW)
    lamp.move_to(x, 286)
    view["lamp"] = 6

show_page()
say("เลือกแถวแล้วกด A")

# ---- 5) วงวนหลัก ----
def on_frame():
    k = game.keys()
    go = int(game.pressed_once("right", k)) - int(game.pressed_once("left", k))
    step = int(game.pressed_once("down", k)) - int(game.pressed_once("up", k))
    table = PAGES[view["page"]][1]
    if go:
        view["page"] = (view["page"] + go) % len(PAGES)
        show_page()
        game.sfx("move")
    elif step:
        view["sel"] = (view["sel"] + step) % len(table)
        bar.move_to(20, ROWS_Y - 5 + view["sel"] * ROW_H)
    if game.pressed_once("a", k):
        name, _, src = table[view["sel"]]
        view["name"] = name
        if name in RECIPES:                   # ทำนอง: ส่งให้เครื่องเล่น แล้วมันเล่นทีละโน้ตตามเฟรม
            play(player, RECIPES[name])
        else:                                 # เสียงสำเร็จรูป: เรียกครั้งเดียวจบ
            game.sfx(name)
            light(560)
            say("กำลังเล่น: %s   <- %s" % (name, src))
    note = player_step(player)
    if note is not None:                      # ทำนองเพิ่งขึ้นโน้ตใหม่: ไฟวิ่งตามระดับเสียง
        if note:
            light(420 + (note - 60) * 10)
        say("กำลังเล่น: %s  โน้ต %d/%d  (%d)" % (view["name"], player["i"], len(player["steps"]), note))
    if view["lamp"] > 0:
        view["lamp"] -= 1
        if view["lamp"] == 0:
            lamp.set_color(0x334455)
    return True

game.run(on_frame, fps=FPS)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) COIN = ((83, 60, 1), (88, 160, 1)) -> ((83, 60, 6), (88, 160, 1)) : เว้นช่วงยาวขึ้น ฟังเป็นสองเสียงแยกกัน ไม่ใช่เหรียญ
# 2) TEAM เปลี่ยนเป็น ((72, 110, 4), (67, 110, 4), (64, 110, 4), (60, 200, 4)) : ทำนองไล่ลง ฟังเหมือนเสียงแพ้แทนเสียงชนะ
# 3) ใน on_frame ใส่ game.tone(60) ทุกเฟรม (นอก if) : เสียงอื่นโดนทับจนฟังไม่ออก — หนึ่งเสียงต้องดังเฉพาะตอนเกิดเหตุ
