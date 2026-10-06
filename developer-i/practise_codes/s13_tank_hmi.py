# โครงเว้นบางส่วน — ช่องที่น้องต้องเติมเอง / เฉลย: solution_codes/s13_tank_hmi.py / ใบ้: สไลด์คาบ 13 การ์ด 1, 4, 5
# วิธีเล่น  : เติม 2 ช่อง "เติมส่วนนี้เอง" ในส่วน 2) สมอง แล้วรัน — ไฟล์ตรวจคำตอบเองก่อนเปิดจอ
#             ผ่านครบ = Console ขึ้น "ผ่าน!" แล้วเล่นได้เหมือนไฟล์เฉลย
#             ยังไม่ถูก = Console บอกว่ากรณีไหนผิด แล้วหยุด (ยังไม่เปิดจอ)
# s13_tank_hmi.py — คาบ 13 (คาบพิเศษ): หน้าจอควบคุมถังน้ำโรงเรือน = เทคนิคจาก Pong ในงานจริง
#
# หลักการ   : การ์ด 1 สะสมค่าทีละเฟรม   ระดับน้ำ = เดิม + น้ำเข้า - น้ำออก      (คาบ 5)
#             การ์ด 4 จำกัดขอบ          ระดับน้ำอยู่ระหว่าง 0 ถึง 100 เสมอ        (คาบ 3)
#             การ์ด 5 ตัวควบคุมสามระดับ  วาล์วเปิดเพิ่ม / หยุด / ปิดลง มีช่องว่าง  (คาบ 7)
#             แบบเดียวกับไม้ AI ใน pong_step5.py ที่ขยับขึ้น หยุด หรือลง ตามลูก
# ลองเล่น   : กด Start · จอยขวา = ดึงน้ำออกมากขึ้น · จอยซ้าย = ดึงน้ำออกน้อยลง
#             A = เป้าหมายสูงขึ้น 5% · B = เป้าหมายต่ำลง 5%
#             ดูวาล์วไล่ระดับน้ำให้เข้าใกล้เส้นเป้าหมาย (เส้นเหลือง) แล้วหยุดนิ่งในช่องว่าง (HOLD)
#             น้ำในท่อเข้าไหลเฉพาะตอนวาล์วเปิด · จุดน้ำในท่อออกวิ่งเร็วตามค่า OUT
#             ลองดึงน้ำออกจนเกินที่วาล์วเติมทัน แล้วดูแถบบนกลายเป็นสัญญาณเตือน LOW WATER พร้อมเสียง
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : ลูกบอลวิ่ง (สะสมค่า) · ไม้ไม่หลุดจอ (จำกัดขอบ) · ไม้ AI ไล่ลูก (สามระดับ + ช่องว่าง)
# ในงานจริง : หน้าจอถังน้ำในโรงเรือน วาล์วมอเตอร์เติมน้ำเองให้ระดับใกล้เป้าหมาย
#             ช่องว่างกันวาล์วเปิด-ปิดสลับไปมาจนพัง · ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และเครื่องจำลอง
# ต่อยอดจาก : pong_step1.py (สะสมค่า) · pong_step2.py (จำกัดขอบ) · pong_step5.py (ช่องว่าง)
# งบวิดเจ็ต : 26 ชิ้น (แถบสถานะ 3 + ถังกับน้ำ 4 + เส้นเป้า 2 + ท่อเข้า 3 + วาล์ว 2 + ท่อออก 3
#             + การ์ดค่า 8 + game.run 1) · ต่อเฟรมส่งคำสั่งราว 5-9 ครั้ง (ตอนน้ำข้ามขั้น 10% เพิ่มอีก 3)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game

# ---- 1) ตั้งค่า (แก้ได้) ----
IN_MAX = 0.6        # วาล์วเปิดสุด เติมน้ำได้กี่ % ต่อเฟรม
OUT_START = 0.20    # น้ำออกตอนเริ่ม (% ต่อเฟรม)
OUT_MAX = 0.80      # ดึงน้ำออกได้สูงสุด — มากกว่า IN_MAX จึงทำให้น้ำลดได้แม้วาล์วเปิดสุด
OUT_STEP = 0.01     # กดจอยค้าง 1 เฟรม น้ำออกเปลี่ยนเท่านี้
VALVE_RATE = 0.02   # วาล์วมอเตอร์ขยับได้เฟรมละ 2% (มอเตอร์จริงหมุนช้า ไม่ได้เปิดทันที)
BAND = 3            # ช่องว่าง: ห่างเป้าไม่เกิน 3% = วาล์วหยุดนิ่ง
TARGET_START = 60   # เป้าหมายระดับน้ำ (%)
LOW_ALARM = 20      # ต่ำกว่านี้ = เตือน LOW WATER
ALARM_EVERY = 20    # เตือนดังทุก 20 เฟรม (ไม่ใช้ sleep ในวงวน)

TANK_X, TANK_Y, TANK_W, TANK_H = 160, 52, 196, 294
SEG_H = 28          # น้ำหนึ่งขั้น 10% สูง 28 px (10 ขั้น = 280 px = 100%)
BOTTOM = TANK_Y + TANK_H - 8       # ก้นถังด้านใน (y)
PIPE_Y, OUT_Y = 72, 324            # ท่อน้ำเข้า (บนซ้าย) · ท่อน้ำออก (ล่างขวา)
CARD_X, CARD_W = 548, 232          # คอลัมน์การ์ดค่าด้านขวา
PANEL, EDGE, GRAY = 0x1B2633, 0x3D5266, 0x556070   # แผง · ขอบ · วาล์วหยุด
PIPE, TANK_BG = 0x4A5868, 0x101A24
DEEP, LIGHT = 0x1E5FA8, 0x5DB4F0   # น้ำสองเฉด: เนื้อน้ำเข้ม · ผิวน้ำอ่อน


# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ (ทดสอบได้โดยไม่ต้องมีบอร์ด) ----
def clamp(value, low, high):
    return max(low, min(high, value))


def next_level(level, inflow, outflow):
    # ----- เติมส่วนนี้เอง (งานของน้อง) (1): การ์ด 1 + การ์ด 4 -----
    # TODO: ระดับใหม่ = ระดับเดิม + น้ำเข้า - น้ำออก แล้วจำกัดให้อยู่ใน 0..100 ด้วย clamp()
    #   ใบ้: ball_x += ball_vx ใน pong_step1.py และ max(0, min(...)) ใน pong_step2.py
    return level                 # <- แก้บรรทัดนี้ (ตอนนี้ระดับน้ำไม่เปลี่ยนเลย)


def valve_step(level, target, band):
    # ----- เติมส่วนนี้เอง (งานของน้อง) (2): การ์ด 5 ตัวควบคุมสามระดับ -----
    # TODO: น้ำน้อยกว่า target - band -> คืน 1 (เปิดเพิ่ม)
    #       น้ำมากกว่า target + band -> คืน -1 (ปิดลง) · นอกนั้นคืน 0 (หยุดนิ่ง)
    #   ใบ้: ไม้ AI ใน pong_step5.py (if ai_y < target_y - 6 ... elif ai_y > target_y + 6 ...)
    return 0                     # <- แก้บรรทัดนี้ (ตอนนี้วาล์วไม่ขยับเลย)


def bars_on(level):
    # แถบ 10 ก้อน ก้อนละ 10% (น้ำ 0-9.9% = ยังไม่ขึ้นสักก้อน)
    return int(level) // 10


# ---- 3) ตรวจสมองก่อนเปิดจอ ----
def self_test():
    cases = [
        ("next_level", next_level(50, 1.0, 0.5), 50.5),
        ("next_level", next_level(99.8, 1.0, 0.0), 100),   # ล้นไม่ได้
        ("next_level", next_level(0.2, 0.0, 1.0), 0),      # ติดลบไม่ได้
        ("valve_step", valve_step(40, 60, 3), 1),
        ("valve_step", valve_step(58, 60, 3), 0),
        ("valve_step", valve_step(63, 60, 3), 0),          # ขอบช่องว่างพอดี = ยังหยุด
        ("valve_step", valve_step(70, 60, 3), -1),
        ("bars_on", bars_on(9.9), 0),
        ("bars_on", bars_on(55), 5),
        ("bars_on", bars_on(100), 10),
    ]
    for name, got, want in cases:
        if abs(got - want) > 1e-6:
            print("ยังไม่ผ่าน:", name, "ได้", got, "ควรได้", want)
            return False
    print("ผ่าน! สมองของถังน้ำถูกทั้ง", len(cases), "กรณี")
    return True


if not self_test():
    raise SystemExit

# ---- 4) หน้าจอ: สร้างครั้งเดียว (สร้างก่อน = อยู่ล่าง สร้างทีหลัง = ทับข้างบน) ----
game.title("TANK HMI")
bar = game.Box(-4, -4, 800, 44, PANEL, border=EDGE, radius=4)        # แถบสถานะบนสุด (ชิดขอบจอ)
game.Text("ถังน้ำโรงเรือน   TANK HMI", 14, 9, game.WHITE)
hud_state = game.Text("ปกติ  NORMAL", 560, 9, game.WHITE)

game.Box(20, PIPE_Y, TANK_X - 16, 14, PIPE, border=EDGE)                # ท่อน้ำเข้า
flow_in = [game.Box(26 + i * 58, PIPE_Y + 4, 14, 6, LIGHT) for i in range(2)]
valve_sym = game.Box(62, PIPE_Y - 12, 38, 38, GRAY, border=game.WHITE, radius=19, border_w=3)
valve_tag = game.Text("HOLD", 54, PIPE_Y + 34, game.WHITE)
game.Box(TANK_X + TANK_W - 6, OUT_Y, 170, 14, PIPE, border=EDGE)       # ท่อน้ำออก
drops = [game.Box(TANK_X + TANK_W + 10 + i * 71, OUT_Y + 4, 10, 6, LIGHT) for i in range(2)]

tank = game.Box(TANK_X, TANK_Y, TANK_W, TANK_H, TANK_BG, border=0x8899AA, radius=6, border_w=3)
water = game.Box(TANK_X + 6, BOTTOM, TANK_W - 12, SEG_H, DEEP)      # เนื้อน้ำสีเข้ม สูงทีละขั้น 10%
water.hide()
surface = game.Box(TANK_X + 6, BOTTOM, TANK_W - 12, SEG_H, LIGHT)   # ผิวน้ำสีอ่อน ขยับทุก 1%
game.Box(TANK_X - 14, BOTTOM, TANK_W + 28, SEG_H + 2, 0x2A3644, border=EDGE, radius=4)  # ฐานถัง บังผิวน้ำที่เลยก้น


def level_to_y(level):
    return int(BOTTOM - level * (10 * SEG_H) / 100)


target_line = game.Box(TANK_X - 6, level_to_y(TARGET_START), TANK_W + 12, 3, game.YELLOW)
target_tag = game.Text("เป้า %d%%" % TARGET_START, TANK_X + TANK_W + 14, level_to_y(TARGET_START) - 11, game.YELLOW)


def card(row, accent, title):
    # การ์ดค่า: แผงมืดขอบสี + ป้ายสองบรรทัด (บรรทัดบน = ชื่อ · บรรทัดล่าง = ค่า)
    y = 52 + row * 76
    game.Box(CARD_X, y, CARD_W, 66, PANEL, border=accent, radius=8, border_w=2)
    return game.Text(title, CARD_X + 16, y + 9, game.WHITE)


cards = {"level": card(0, LIGHT, "ระดับน้ำ  LEVEL"), "target": card(1, game.YELLOW, "เป้าหมาย  TARGET"),
         "out": card(2, game.CYAN, "น้ำออก  OUT"), "valve": card(3, game.GREEN, "วาล์ว  VALVE")}

level, target, outflow, valve = 50.0, TARGET_START, OUT_START, 0.0
shown = {"bars": 0, "move": None, "low": False, "open": True}
alarm_count = 0
VALVE_LOOK = {1: (game.GREEN, "OPEN+"), 0: (GRAY, "HOLD"), -1: (game.ORANGE, "CLOSE-")}


def show(key, widget, text):
    # อัปเดตป้ายเฉพาะตอนข้อความเปลี่ยน (ไม่เปลี่ยน = ไม่ส่งคำสั่งไปจอ)
    if shown.get(key) != text:
        widget.set(text)
        shown[key] = text


def slide(items, step, start, end):
    # เลื่อนกล่องเล็กไปทางขวา ถึงปลายท่อแล้ววนกลับต้นท่อ · ส่งคำสั่งเฉพาะตอนพิกเซลเปลี่ยน
    for it in items:
        x = it.x + step
        if x > end:
            x -= end - start
        if int(x) != int(it.x):
            it.move_to(x, it.y)
        else:
            it.x = x


# ---- 5) วงวนหลัก ----
def on_frame():
    global level, target, outflow, valve, alarm_count
    keys = game.keys()

    # รับปุ่ม (วัด)
    if keys.right: outflow = clamp(outflow + OUT_STEP, 0, OUT_MAX)
    if keys.left:  outflow = clamp(outflow - OUT_STEP, 0, OUT_MAX)
    if game.pressed_once("a", keys): target = clamp(target + 5, 10, 90)
    if game.pressed_once("b", keys): target = clamp(target - 5, LOW_ALARM + BAND + 2, 90)

    # คิด (ตัดสิน): วาล์วขยับตามตัวควบคุมสามระดับ แล้วระดับน้ำสะสมค่า
    move = valve_step(level, target, BAND)
    valve = clamp(valve + move * VALVE_RATE, 0, 1)
    level = next_level(level, valve * IN_MAX, outflow)
    low = level < LOW_ALARM

    # ออก (ทำ + โชว์) — น้ำในถัง: เนื้อน้ำเปลี่ยนเฉพาะตอนข้ามขั้น 10% (ไม่ resize ทุกเฟรม)
    bars = bars_on(level)
    if bars != shown["bars"]:
        if bars == 0:
            water.hide()
        else:
            water.move_to(water.x, BOTTOM - bars * SEG_H)
            water.resize(water.w, bars * SEG_H)
            water.show()
        shown["bars"] = bars
    y = level_to_y(level)
    if y != surface.y:                                # ผิวน้ำ: ขยับตามระดับจริงทุก 1 px
        surface.move_to(surface.x, y)
    y = level_to_y(target)
    if y != target_line.y:                            # เส้นเป้า + ป้าย ย้ายเฉพาะตอนกด A / B
        target_line.move_to(target_line.x, y)
        target_tag.set("เป้า %d%%" % target)
        target_tag._label.pos(TANK_X + TANK_W + 14, y - 11)   # game.Text ไม่มี move_to จึงย้ายป้ายข้างในตรง ๆ
    # น้ำไหลในท่อ: ท่อเข้าไหลเฉพาะตอนวาล์วเปิด (เร็วตามองศาวาล์ว) · ท่อออกเร็วตาม OUT
    opened = valve > 0
    if opened != shown["open"]:
        for f in flow_in:
            if opened: f.show()
            else:      f.hide()
        shown["open"] = opened
    if opened:
        slide(flow_in, 1 + valve * 5, 26, TANK_X - 18)
    slide(drops, outflow * 12, TANK_X + TANK_W + 10, TANK_X + TANK_W + 152)
    # วาล์ว + การ์ดค่า
    if move != shown["move"]:
        valve_sym.set_color(VALVE_LOOK[move][0])
        valve_tag.set(VALVE_LOOK[move][1])
        shown["move"] = move
    show("level", cards["level"], "ระดับน้ำ  LEVEL\n%d %%" % int(level))
    show("target", cards["target"], "เป้าหมาย  TARGET\n%d %%" % target)
    show("out", cards["out"], "น้ำออก  OUT\n%d.%02d %% ต่อเฟรม" % (int(outflow), int(outflow * 100 + 0.5) % 100))
    show("valve", cards["valve"], "วาล์ว  VALVE\n%d %%   %s" % (int(valve * 100), VALVE_LOOK[move][1]))
    # สถานะ: น้ำต่ำ = แถบบนกลายเป็นป้ายเตือนสีแดง กะพริบพร้อมเสียงทุก ALARM_EVERY เฟรม
    if low != shown["low"]:
        hud_state.set("น้ำต่ำ  LOW WATER" if low else "ปกติ  NORMAL")
        tank.set_color(0x3A1216 if low else TANK_BG)
        if not low:
            bar.set_color(PANEL)
        shown["low"] = low
        alarm_count = 0
    if low:
        if alarm_count == 0:
            bar.set_color(0xC62828)
            game.sfx("deny")
        elif alarm_count == ALARM_EVERY // 2:
            bar.set_color(0x7A1C1C)
        alarm_count = (alarm_count + 1) % ALARM_EVERY
    return True


game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) BAND = 3 -> 0 : วาล์วเปิด-ปิดสลับไม่หยุด (วงกลมวาล์วกะพริบเขียว-ส้ม การ์ด VALVE สลับ OPEN+ / CLOSE-)
# 2) VALVE_RATE = 0.02 -> 0.2 : วาล์วตอบสนองไวขึ้น แต่ระดับน้ำแกว่งเกินเส้นเป้ามากขึ้นไหม
# 3) OUT_MAX = 0.80 -> 0.50 : น้ำยังลดจนเตือนได้ไหม เมื่อน้ำออกสูงสุดน้อยกว่าที่วาล์วเติมทัน
