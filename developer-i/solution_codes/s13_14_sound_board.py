# s13_14_sound_board.py — คาบ 13 (คาบพิเศษ): แผงเสียง 21 เสียง · ในเกมคือรางวัล ในโรงงานคือคำเตือน
#
# หลักการ   : หนึ่งเสียง หนึ่งความหมาย จัดเสียงเป็น 4 กลุ่ม ตอบรับ / ได้ / เสีย / กระทำ (สไลด์เรื่องเสียง)
#             ความถี่ของโน้ต MIDI  f = 440 * 2^((n - 69) / 12)   n = 69 คือ ลา 440 Hz   (คาบ 8)
#             ทำนองไม่ใช้ sleep: นับเฟรมแล้วเล่นโน้ตถัดไปเมื่อถึงจังหวะ  (เวลาคือการนับเฟรม คาบ 4, 11)
#             สัญญาณเตือนสองระดับ = เครื่องสถานะ ปกติ / ด่วน / แจ้งทราบ  (คาบ 2, 7)
# ลองเล่น   : กด Start · จอย ขึ้น/ลง = เลือกกลุ่ม · ซ้าย/ขวา = เลือกเสียง (ชื่อในวงเล็บ) · A = เล่นเสียง
#             B = ทำนองสองโน้ต ดูเลข n กับความถี่ Hz ที่คำนวณจากสูตรขึ้นทีละโน้ต
#             X = จำลองสัญญาณเตือน สลับระดับทุกครั้งที่กด: ด่วน (ไฟแดงกะพริบ ดังถี่จนกด Y)
#                 หรือ แจ้งทราบ (ไฟเหลือง ดังครั้งเดียว) · Y = รับทราบ
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : eat / point / win = รางวัล · die / lose_life / gameover = เสีย · fire / flap = การกระทำ
# ในงานจริง : เสียงเตือนหน้าเครื่องจักรแยกระดับ ด่วนมาก = ถี่ ซ้ำ จนมีคนกดรับทราบ · แจ้งให้ทราบ = ครั้งเดียว
#             ตัวเลขและสัญญาณเตือนทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : s08_sound.py (sfx + tone) · s02_button_fsm.py (กดหนึ่งครั้ง = นับหนึ่งครั้ง)
# งบวิดเจ็ต : 16 ชิ้น (แถบเลือก 1 + ชื่อกลุ่ม 4 + แถวเสียง 4 + ข้อความ 5 + ไฟเตือน 1 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game
import math

# ---- 1) ตั้งค่า (แก้ได้) ----
GROUPS = [("ตอบรับ", ["select", "move", "back", "deny", "start"]),
          ("ได้",    ["eat", "point", "hit", "win", "pong_score"]),
          ("เสีย",   ["die", "fall", "lose", "lose_life", "gameover"]),
          ("กระทำ",  ["fire", "turn", "flap", "wall", "paddle", "explode"])]   # รวม 21 เสียง
JINGLE = (72, 79)     # โด สูง -> ซอล สูง (เลข MIDI)
NOTE_GAP = 8          # โน้ตถัดไปห่าง 8 เฟรม (ราว 0.27 วินาที)
HIGH_EVERY = 6        # เตือนด่วน: ดังทุก 6 เฟรม = 5 ครั้งต่อวินาที
HIGH_NOTE, LOW_NOTE = 88, 76
ROW_Y = [62, 100, 138, 176]
GRAY, DARK_RED = 0x556070, 0x5C1F1F

# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ (ทดสอบได้โดยไม่ต้องมีบอร์ด) ----
def midi_hz(n):
    return 440 * math.pow(2, (n - 69) / 12)     # ขึ้น 12 = ความถี่คูณ 2 (สูงขึ้นหนึ่งช่วงคู่แปด)

def row_text(names, sel):
    # ชื่อที่เลือกอยู่ในวงเล็บ ชื่ออื่นเว้นช่องเท่ากัน ตัวหนังสือจึงไม่ขยับไปมา
    return " ".join(("[%s]" if i == sel else " %s ") % n for i, n in enumerate(names))

def alarm_beep(level, t, every):
    # ด่วน = ดังทุก every เฟรม ไม่หยุดเอง · แจ้งทราบ = ดังเฉพาะเฟรมแรก · ปกติ = เงียบ
    if level == "HIGH": return t % every == 0
    if level == "LOW":  return t == 0
    return False

# ---- 3) ตรวจสมองก่อนเปิดจอ ----
def self_test():
    for got, want in ((round(midi_hz(69), 2), 440.0), (round(midi_hz(72), 2), 523.25), (round(midi_hz(81), 2), 880.0),
                      (row_text(["a", "b"], 1), " a  [b]"), (alarm_beep("HIGH", 12, 6), True),
                      (alarm_beep("HIGH", 13, 6), False), (alarm_beep("LOW", 0, 6), True), (alarm_beep("LOW", 6, 6), False)):
        if got != want:
            print("ยังไม่ผ่าน: ได้", got, "ควรได้", want)
            return False
    print("ผ่าน! สมองของแผงเสียงถูกทุกกรณี")
    return True

if not self_test():
    raise SystemExit

# ---- 4) หน้าจอ: สร้างครั้งเดียว (แถบเลือกก่อน ตัวหนังสือทีหลัง จะได้อยู่บนแถบ) ----
game.title("SOUND BOARD")
# ข้อความหนึ่งป้ายยาวได้ไม่เกิน 95 ไบต์ตอนสร้าง (ไทยตัวละ 3 ไบต์) ยาวกว่านั้นบอร์ดตัดท้ายทิ้ง
game.Text("A = เล่น   B = ทำนอง   X = เตือน   Y = รับทราบ", 12, 12, game.CYAN)
bar = game.Box(8, ROW_Y[0] - 6, 776, 34, 0x2A3340, radius=6)
for r in range(4):
    game.Text(GROUPS[r][0], 20, ROW_Y[r], 0x8899AA)
rows = [game.Text("", 110, ROW_Y[r], game.WHITE) for r in range(4)]
game.Text("f = 440 * 2^((n - 69) / 12)", 20, 230, game.YELLOW)
hud_note = game.Text("B = ทำนอง 2 โน้ต", 20, 262, game.WHITE)
lamp = game.Box(470, 226, 44, 44, GRAY, border=0x8899AA, radius=22, border_w=2)
hud_alarm = game.Text("NORMAL", 530, 230, game.WHITE)
hud_next = game.Text("", 530, 262, 0x8899AA)

st = {"r": 0, "c": 0, "flash": 0, "jingle": -1, "level": "", "t": 0, "next": "HIGH", "lit": False}

def paint_rows():
    for r in range(4):
        rows[r].set(row_text(GROUPS[r][1], st["c"] if r == st["r"] else -1))
    bar.move_to(8, ROW_Y[st["r"]] - 6)

def set_alarm(level):
    st["level"], st["t"] = level, 0
    lamp.set_color({"HIGH": game.RED, "LOW": game.YELLOW}.get(level, GRAY))
    hud_alarm.set({"HIGH": "HIGH ALARM - press Y", "LOW": "LOW: notify once"}.get(level, "NORMAL"))
    hud_next.set("X = raise %s" % st["next"])

paint_rows()
set_alarm("")

# ---- 5) วงวนหลัก ----
def on_frame():
    k = game.keys()
    dr = game.pressed_once("down", k) - game.pressed_once("up", k)      # +1 / 0 / -1
    dc = game.pressed_once("right", k) - game.pressed_once("left", k)
    if dr or dc:                                              # วาดแถวใหม่เฉพาะตอนเลื่อน
        st["r"] = max(0, min(3, st["r"] + dr))                # ขึ้นลงมีขอบ
        n = len(GROUPS[st["r"]][1])
        st["c"] = (min(st["c"], n - 1) + dc) % n              # ซ้ายขวาวนรอบในแถว
        paint_rows()
    if game.pressed_once("a", k):
        game.sfx(GROUPS[st["r"]][1][st["c"]])
        bar.set_color(0x3E6A9A)                               # แถบสว่างแวบ 6 เฟรม = รู้ว่ากดติด
        st["flash"] = 6
    if st["flash"] > 0:
        st["flash"] -= 1
        if st["flash"] == 0: bar.set_color(0x2A3340)

    if game.pressed_once("b", k):
        st["jingle"] = 0                                      # เริ่มนับเฟรมของทำนอง
    if st["jingle"] >= 0:
        if st["jingle"] % NOTE_GAP == 0:                      # ถึงจังหวะโน้ตถัดไป
            i = st["jingle"] // NOTE_GAP
            game.tone(JINGLE[i], ms=220)
            hud_note.set("note %d/%d   n = %d  ->  f = %.1f Hz" % (i + 1, len(JINGLE), JINGLE[i], midi_hz(JINGLE[i])))
        st["jingle"] += 1
        if st["jingle"] >= len(JINGLE) * NOTE_GAP:
            st["jingle"] = -1                                 # ครบทุกโน้ต หยุดนับ

    if game.pressed_once("x", k):                             # ยกสัญญาณเตือน สลับระดับทุกครั้ง
        level = st["next"]
        st["next"] = "LOW" if level == "HIGH" else "HIGH"
        set_alarm(level)
    if game.pressed_once("y", k) and st["level"]:             # รับทราบ = เงียบ ไฟกลับเทา
        game.sfx("select")
        set_alarm("")
    if alarm_beep(st["level"], st["t"], HIGH_EVERY):
        game.tone(HIGH_NOTE if st["level"] == "HIGH" else LOW_NOTE, ms=90)
        if st["level"] == "HIGH":                             # ไฟแดงกะพริบตามจังหวะเสียง
            st["lit"] = not st["lit"]
            lamp.set_color(game.RED if st["lit"] else DARK_RED)
    st["t"] += 1
    return True

game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) JINGLE = (72, 79) -> (60, 64, 67, 72) : ทำนองยาวขึ้นเป็นสี่โน้ตเองโดยไม่ต้องแก้วงวน · ดูว่า n = 72 ได้ 523.3 Hz
#    ซึ่งเป็นสองเท่าของ n = 60 (261.6 Hz) พอดี เพราะห่างกัน 12
# 2) HIGH_EVERY = 6 -> 30 : เตือนด่วนดังแค่วินาทีละครั้ง ฟังแล้วยังรู้สึก "ด่วน" อยู่ไหม
# 3) NOTE_GAP = 8 -> 2 : โน้ตถัดไปมาก่อนโน้ตแรกจบ เสียงแรกถูกตัด เพราะ game.tone() ไม่ต่อคิว
