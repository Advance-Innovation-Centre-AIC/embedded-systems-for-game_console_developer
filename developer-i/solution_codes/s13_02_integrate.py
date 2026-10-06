# s13_02_integrate.py — คาบ 13 (คาบพิเศษ) การ์ด 1: สะสมค่าทีละเฟรม ลูกบอลกับถังน้ำใช้สมการเดียวกัน
#
# หลักการ   : การ์ด 1 สะสมค่าทีละเฟรม   ค่าใหม่ = ค่าเดิม + ที่เปลี่ยนต่อเฟรม               (คาบ 5)
#             ลูกบอล   x = x + v                        แบบเดียวกับ ball_x += ball_vx ใน pong_step1.py
#             ถังน้ำ    ระดับ = ระดับ + น้ำเข้า - น้ำออก   แบบเดียวกับ s13_tank_hmi.py
#             ถังมีขอบ 0 ถึง 100 (การ์ด 4 คาบ 3) · ลูกวิ่งทะลุขวาแล้วโผล่ซ้าย (วนรอบ)
# ลองเล่น   : กด Start · จอยซ้าย/ขวา = ลด/เพิ่ม v ทีละ 1 · จอยขึ้น/ลง = เพิ่ม/ลดน้ำเข้าทีละ 0.10
#             A = เริ่มใหม่ · ดูบรรทัดสีขาวสองบรรทัด: สมการเดียวกันกำลังคิดเลขจริงทุกเฟรม
#             ขีดเหลืองใต้ลูก = ที่อยู่ของลูกทุก 6 เฟรม ยิ่ง v มากขีดยิ่งห่าง (ห่าง = 6 x v)
#             ตั้งน้ำเข้า 0.30 ให้เท่าน้ำออก แล้วดูระดับน้ำนิ่ง — เหมือนลูกตอน v = 0
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : ลูกบอลวิ่ง · ยานลอย · ศัตรูตก (pong_step1.py · shooter_step2.py)
# ในงานจริง : ระดับน้ำในถังโรงเรือน · ระยะทางที่รถ AGV วิ่งได้ · ยอดชิ้นงานสะสมบนสายพาน
#             ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : pong_step1.py (สะสมค่า) · s13_tank_hmi.py (ถังน้ำ 10 ก้อน)
# งบวิดเจ็ต : 25 ชิ้น (หัวเรื่อง 1 + สมการ 4 + ราง 1 + ลูก 1 + ขีด 4 + ป้ายขีด 1 + ถัง 1 + แถบน้ำ 10
#             + ป้ายปุ่ม 1 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ (ไฟล์นี้ข้าม)
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game

# ---- 1) ตั้งค่า (แก้ได้) ----
V_START, V_MAX = 4, 12          # ความเร็วลูกตอนเริ่ม และสูงสุด (พิกเซลต่อเฟรม)
IN_START, IN_MAX = 0.40, 1.00   # น้ำเข้าตอนเริ่ม และสูงสุด (% ต่อเฟรม)
OUT = 0.30                      # น้ำออกคงที่ (% ต่อเฟรม)
LEVEL_START = 30.0              # ระดับน้ำตอนเริ่ม (%)
TRAIL_EVERY = 6                 # วางขีดเหลืองทุกกี่เฟรม
LANE_X, LANE_Y, LANE_W, LANE_H = 16, 100, 370, 100
BALL = 14                       # ภาพ ball กว้าง/สูง 14 px
X_LO, X_HI = LANE_X + 4, LANE_X + LANE_W - BALL - 4
TANK_X, TANK_Y, TANK_W, TANK_H = 470, 100, 120, 262
SEG_H = 24                      # แถบน้ำหนึ่งก้อน (10 ก้อน = 240 px = 100%)


# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ (ทดสอบได้โดยไม่ต้องมีบอร์ด) ----
def integrate(value, change):
    # การ์ด 1: หัวใจของไฟล์นี้ — ลูกบอลและถังน้ำเรียกฟังก์ชันนี้ตัวเดียวกัน
    return value + change


def wrap(x):
    # ลูกทะลุขวา -> โผล่ซ้าย และกลับกัน (ไม่ใช่สะท้อน สะท้อนอยู่การ์ด 2)
    span = X_HI - X_LO
    return x - span if x > X_HI else (x + span if x < X_LO else x)


def signed(value, fmt):
    # (4, "%d") -> "+ 4" · (-0.3, "%.2f") -> "- 0.30" ให้สมการบนจอเหมือนที่เขียนบนกระดาน
    return ("- " if value < 0 else "+ ") + (fmt % abs(value))


# ---- 4) หน้าจอ: สร้างครั้งเดียว ----
game.title("INTEGRATE")
game.Text("สมการเดียว: ใหม่ = เดิม + ที่เปลี่ยน", 16, 4, game.YELLOW)
game.Text("ลูกบอล    x = x + v", 16, 36, game.CYAN)
ball_eq = game.Text("", 16, 64, game.WHITE)
game.Text("ถังน้ำ    น้ำ = น้ำ + เข้า - ออก", 420, 36, game.CYAN)
tank_eq = game.Text("", 420, 64, game.WHITE)
game.Box(LANE_X, LANE_Y, LANE_W, LANE_H, 0x16222E, border=0x334455, border_w=1)
ticks = [game.Box(-20, LANE_Y + 50, 3, 26, game.YELLOW) for _ in range(4)]
ball = game.Sprite("ball", X_LO, LANE_Y + 24)
game.Text("ขีดเหลือง = ลูกอยู่ตรงนี้ทุก %d เฟรม" % TRAIL_EVERY, 16, 210, game.YELLOW)
game.Box(TANK_X, TANK_Y, TANK_W, TANK_H, 0x1E2A36, border=0x8899AA, border_w=2)
segments = []
for i in range(10):                                   # ก้อนที่ 0 อยู่ก้นถัง
    seg = game.Box(TANK_X + 10, TANK_Y + TANK_H - 10 - (i + 1) * SEG_H + 2, TANK_W - 20, SEG_H - 4, game.BLUE)
    seg.hide()
    segments.append(seg)
game.Text("ซ้ายขวา = v  ขึ้นลง = น้ำเข้า  A = ใหม่", 16, 300, game.WHITE)

x, v, level, inflow = X_LO, V_START, LEVEL_START, IN_START
frame, next_tick = 0, 0
shown = {"bars": 0, "ball": "", "tank": "", "limit": False}


def show(widget, key, text):
    if text != shown[key]:                            # ป้ายเปลี่ยนเมื่อข้อความเปลี่ยนเท่านั้น
        widget.set(text)
        shown[key] = text


# ---- 5) วงวนหลัก ----
def on_frame():
    global x, v, level, inflow, frame, next_tick
    keys = game.keys()

    # เข้า: ปุ่มเปลี่ยน "ที่เปลี่ยนต่อเฟรม" ไม่ได้ย้ายลูกหรือเติมน้ำตรง ๆ
    if game.pressed_once("right", keys): v = min(v + 1, V_MAX)
    if game.pressed_once("left", keys):  v = max(v - 1, -V_MAX)
    if game.pressed_once("up", keys):    inflow = round(min(inflow + 0.10, IN_MAX), 2)
    if game.pressed_once("down", keys):  inflow = round(max(inflow - 0.10, 0.0), 2)
    if game.pressed_once("a", keys):
        x, v, level, inflow = X_LO, V_START, LEVEL_START, IN_START
        game.sfx("select")

    # คิด: สมการเดียวกันสองที่
    old_x, old_level = x, level
    x = wrap(integrate(x, v))                              # ลูกบอล
    level = integrate(level, inflow - OUT)                 # ถังน้ำ
    level = max(0.0, min(100.0, level))                    # การ์ด 4: ถังมีขอบเสมอ
    at_limit = level >= 100.0 or level <= 0.0
    frame += 1

    # ออก: วาด + ป้าย เฉพาะที่เปลี่ยน
    ball.move_to(x, ball.y)
    if frame % TRAIL_EVERY == 0:                           # ยืมขีดเก่าสุดมาวางใหม่ (วนรอบ 4 ขีด)
        ticks[next_tick].move_to(int(x) + 5, ticks[next_tick].y)
        next_tick = (next_tick + 1) % len(ticks)
    show(ball_eq, "ball", "%d %s = %d" % (old_x, signed(v, "%d"), old_x + v))
    note = "  เต็ม" if level >= 100.0 else ("  แห้ง" if level <= 0.0 else "")
    show(tank_eq, "tank", "%.1f %s %s = %.1f%s" % (old_level, signed(inflow, "%.2f"),
                                                    signed(-OUT, "%.2f"), level, note))
    bars = max(0, min(10, int(level) // 10))               # ถังมี 10 ก้อน · แตะเฉพาะก้อนที่เปลี่ยน
    while shown["bars"] < bars:
        segments[shown["bars"]].show()
        shown["bars"] += 1
    while shown["bars"] > bars:
        shown["bars"] -= 1
        segments[shown["bars"]].hide()
    if at_limit and not shown["limit"]:
        game.sfx("deny")                                   # ถังเต็มหรือแห้ง เตือนครั้งเดียว
    shown["limit"] = at_limit
    return True


game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) TRAIL_EVERY = 6 -> 1 : ขีดเหลืองห่างกันเท่า v พอดี — นี่คือ "ก้าว" หนึ่งเฟรมของลูก
# 2) OUT = 0.30 -> 0.0 แล้วกดน้ำเข้าลงจนเป็น 0.00 : ระดับน้ำนิ่ง เหมือนลูกตอน v = 0 ทุกอย่าง
# 3) ลบบรรทัด level = max(0.0, min(100.0, level)) : สมการบอกน้ำ 130% หรือ -20% ซึ่งไม่มีจริง
#    การ์ด 4 (จำกัดขอบ) จึงต้องมาคู่กับการ์ด 1 เสมอ
