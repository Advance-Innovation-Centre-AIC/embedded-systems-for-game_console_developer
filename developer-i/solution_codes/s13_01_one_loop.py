# s13_01_one_loop.py — คาบ 13 (คาบพิเศษ) ส่วนที่ 1: หัวใจดวงเดียว = เข้า -> คิด -> ออก ทุกเฟรม
#
# หลักการ   : ทุกระบบเรียลไทม์คือวงวนเดียว game.run() เรียก on_frame() 30 ครั้งต่อวินาที (คาบ 3-4)
#             เข้า (วัด)          อ่านปุ่ม k = game.keys() และดูว่าปุ่มไหน "เพิ่งกด"    (คาบ 2)
#             คิด (ตัดสิน)        ตำแหน่งใหม่ = เดิม + ก้าว แล้วจำกัดขอบ              (คาบ 3)
#             ออก (ทำ + โชว์)     ship.move_to() · hud.set() · เสียงสั้นตอนเพิ่งกด    (คาบ 8)
# ลองเล่น   : กด Start · จอย 4 ทิศ = ขับยาน · A B X Y = ปุ่มโน้ต (ทุกปุ่มรวมกัน = โด ถึง โด สูง)
#             ดูแถบบน 3 ช่อง: เข้า = ปุ่มที่กดอยู่ · คิด = ตำแหน่งที่คำนวณได้ · ออก = นับเฟรม
#             กดค้างไว้ เสียงดังแค่ครั้งแรก ไฟ "เสียง" ติดแวบเดียว — นี่คือการจับจังหวะกด (edge)
#             ดันยานชนขอบ แล้วดูช่อง "คิด" ขึ้นคำว่า ชนขอบ
# ของบนบอร์ด: จอ · จอย · ปุ่ม · ลำโพง
# ในเกม     : ทุกเกมของคอร์ส (s04_catch · pong · shooter) คือฟังก์ชัน on_frame() หน้าตาแบบนี้
# ในงานจริง : เครื่องควบคุมในโรงงาน (PLC) ก็วนแบบนี้ทุกรอบ: อ่านเซนเซอร์ -> ตัดสิน -> สั่งเครื่อง + อัปเดตจอ
#             ตัวเลขในไฟล์นี้เป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : s03_box_joystick.py (ขยับตามจอย + ขอบ) · s02_button_fsm.py (จับจังหวะกด)
#             s08_sound.py (แต่งโน้ตเอง)
# งบวิดเจ็ต : 14 ชิ้น (กรอบหัว 3 + หัวข้อ 3 + ค่า 3 + สนาม 1 + ยาน 1 + ไฟเสียง 1 + ป้ายเสียง 1 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ (ไฟล์นี้ข้าม)
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game

# ---- 1) ตั้งค่า (แก้ได้) ----
FPS = 30            # เฟรมต่อวินาที (game.run เรียก on_frame กี่ครั้งต่อวินาที)
STEP = 6            # ยานขยับกี่พิกเซลต่อเฟรมเมื่อกดจอย
LAMP_FRAMES = 8     # ไฟเสียงติดค้างกี่เฟรมหลังโน้ตดัง (นับเฟรม ไม่ใช้ sleep)
NOTE = {"left": 60, "down": 62, "up": 64, "right": 65,     # โด เร มี ฟา
        "a": 67, "b": 69, "x": 71, "y": 72}                # ซอล ลา ที โด(สูง)
ORDER = ("left", "right", "up", "down", "a", "b", "x", "y")
AREA_X, AREA_Y, AREA_W, AREA_H = 8, 74, 776, 290           # สนามที่ยานวิ่งได้
SHIP_W, SHIP_H = 62, 28                                    # ขนาดภาพ ship
X_MIN, X_MAX = AREA_X + 4, AREA_X + AREA_W - SHIP_W - 4
Y_MIN, Y_MAX = AREA_Y + 4, AREA_Y + AREA_H - SHIP_H - 4
PANEL, GRAY = 0x16222E, 0x556070


# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ ----
def clamp(value, low, high):
    return max(low, min(high, value))


def held_names(k):
    # เข้า: แปลงปุ่มที่กดอยู่ให้เป็นข้อความ เช่น "LEFT UP A" (ไม่กดเลย = "-")
    names = [name.upper() for name in ORDER if getattr(k, name)]
    return " ".join(names) if names else "-"


def next_pos(x, y, k):
    # คิด: ก้าวตามจอย แล้วจำกัดขอบ คืน (x ใหม่, y ใหม่, ชนขอบไหม)
    want_x = x + (STEP if k.right else 0) - (STEP if k.left else 0)
    want_y = y + (STEP if k.down else 0) - (STEP if k.up else 0)
    new_x, new_y = clamp(want_x, X_MIN, X_MAX), clamp(want_y, Y_MIN, Y_MAX)
    return new_x, new_y, (new_x != want_x or new_y != want_y)


# ---- 4) หน้าจอ: สร้างครั้งเดียว ----
game.title("ONE LOOP")
for col, head, color in ((8, "เข้า = วัด", game.CYAN), (272, "คิด = ตัดสิน", game.YELLOW),
                         (536, "ออก = ทำ + โชว์", game.GREEN)):
    game.Box(col, 2, 248, 64, PANEL, border=color, border_w=2, radius=6)
    game.Text(head, col + 12, 8, color)
in_text = game.Text("-", 20, 36, game.WHITE)
think_text = game.Text("", 284, 36, game.WHITE)
out_text = game.Text("", 548, 36, game.WHITE)
lamp = game.Box(712, 12, 16, 16, GRAY, radius=8)
game.Text("เสียง", 734, 8, game.WHITE)
game.Box(AREA_X, AREA_Y, AREA_W, AREA_H, 0x0E1620, border=0x334455, border_w=1)
x, y = (X_MIN + X_MAX) // 2, (Y_MIN + Y_MAX) // 2
ship = game.Sprite("ship", x, y)

frame, lamp_left = 0, 0
shown = {"in": "-", "think": ""}


def show(widget, key, text):
    # อัปเดตป้ายเฉพาะตอนข้อความเปลี่ยน (ทุก set() = หนึ่งข้อความข้ามแกนไปวาดจอ)
    if text != shown[key]:
        widget.set(text)
        shown[key] = text


# ---- 5) วงวนหลัก ----
def on_frame():
    global x, y, frame, lamp_left
    # ===== เข้า (วัด): อ่านปุ่มเฟรมนี้ + หาปุ่มที่ "เพิ่งกด" =====
    k = game.keys()
    note = None
    for name in ORDER:                              # ต้องเรียกทุกปุ่มทุกเฟรม ให้มันจำเฟรมก่อนได้
        if game.pressed_once(name, k):
            note = NOTE[name]                       # กดค้าง = True แค่เฟรมแรก

    # ===== คิด (ตัดสิน): คำนวณสถานะใหม่ ยังไม่แตะจอเลย =====
    frame += 1
    new_x, new_y, at_edge = next_pos(x, y, k)
    moved = new_x != x or new_y != y
    x, y = new_x, new_y

    # ===== ออก (ทำ + โชว์): วาด เสียง และป้าย เฉพาะที่เปลี่ยน =====
    if moved:
        ship.move_to(x, y)
    show(in_text, "in", held_names(k))
    show(think_text, "think", "x %d  y %d%s" % (x, y, "  ชนขอบ" if at_edge else ""))
    out_text.set("เฟรม %d  (%d วิ)" % (frame, frame // FPS))     # เปลี่ยนทุกเฟรม = 1 ข้อความ
    if note is not None:
        game.tone(note, ms=120)                     # เสียงสั้น ไม่ต่อคิว ไม่ต้อง sleep
        if lamp_left == 0:
            lamp.set_color(game.ORANGE)
        lamp_left = LAMP_FRAMES
    elif lamp_left > 0:                             # นับถอยหลังแล้วดับไฟ
        lamp_left -= 1
        if lamp_left == 0:
            lamp.set_color(GRAY)
    return True                                     # คืน False เมื่ออยากจบเกม


game.run(on_frame, fps=FPS)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) FPS = 30 -> 10 : ยานช้าลง 3 เท่า ทั้งที่ STEP เท่าเดิม — ความเร็วจริง = STEP x FPS พิกเซลต่อวินาที
# 2) เปลี่ยน game.pressed_once(name, k) เป็น getattr(k, name) : กดค้างแล้วโน้ตดังรัวทุกเฟรม ไฟเสียงติดค้าง
#    นี่คือเหตุผลที่ปุ่มยิงกับปุ่มรับทราบสัญญาณเตือนต้องจับจังหวะกด
# 3) ในฟังก์ชัน show() เปลี่ยน if text != shown[key]: เป็น if True: : จอยังถูก แต่ส่งข้อความข้ามแกนเพิ่มเฟรมละ 2 ครั้ง
#    บนบอร์ดจริงจะเริ่มหน่วงเมื่อของบนจอเยอะ — วาดเฉพาะที่เปลี่ยนคือกฎของหน้าจอควบคุมด้วย
