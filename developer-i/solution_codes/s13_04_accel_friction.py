# s13_04_accel_friction.py — คาบ 13 (คาบพิเศษ) การ์ด 3: เร่ง หน่วง และเพดาน (ลื่นหรือหนืด ลองรู้สึกเอง)
#
# หลักการ   : การ์ด 3 เร่ง หน่วง เพดาน                                              (คาบ 6, 10)
#             กดค้าง: v += a   ·   ปล่อย: v *= f   ·   v อยู่ใน -VMAX ถึง +VMAX
#             ปล่อยแล้ว v ลดแบบ v0, v0*f, v0*f*f, ... ระยะไถลรวม ~ v0 * f / (1 - f)
#             f = 0.95 -> ไถล ~247 px · f = 0.78 -> ~46 px · f = 0.30 -> ~6 px (เมื่อ v0 = 13)
# ลองเล่น   : กด Start · จอยซ้าย/ขวา ค้าง = เร่ง · ปล่อย = ไถลแล้วหยุดเอง
#             A = เปลี่ยน f: 0.95 ลื่นเหมือนน้ำแข็ง -> 0.78 พอดีแบบ Pong -> 0.30 หนืดเหมือนโคลน
#             ดูแถบความเร็ว (ก้อนแดงปลายแถบ = ชนเพดาน) และบรรทัด "ไถล" ที่วัดให้ทุกครั้งที่ปล่อย
#             ธงเหลืองปักตรงจุดที่ปล่อยจอย ระยะจากธงถึงยาน = ระยะไถลจริงบนจอ
#             ลองเร่งเต็มที่แล้วปล่อยในแต่ละ f — ยานเดิม ปุ่มเดิม แต่ความรู้สึกต่างกันมาก
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : ไม้ตีมีน้ำหนัก (pong_step2.py) · ยานลื่นมีโมเมนตัม (shooter_step2.py)
# ในงานจริง : สายพานหรือลิฟต์ขนของ เร่งจนถึงเพดาน แล้วค่อย ๆ หยุด ไม่กระชาก ของบนสายพานไม่ล้ม
#             ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : pong_step2.py (ACCEL / FRICTION / MAX_SPEED) · shooter_step2.py (ยานซ้าย-ขวา)
# งบวิดเจ็ต : 25 ชิ้น (ข้อความ 3 + แถบ 14 + ขีดศูนย์ 1 + ป้ายแถบ 3 + พื้น 1 + ธง 1 + ยาน 1 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ (ไฟล์นี้ข้าม)
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game

# ---- 1) ตั้งค่า (แก้ได้) ----
ACCEL, VMAX = 1.4, 13.0                         # ค่าเดียวกับ shooter_step2.py
FRICTIONS = (0.95, 0.78, 0.30)                  # A วนเปลี่ยนสามค่านี้
FEELS = ("ลื่นเหมือนน้ำแข็ง", "พอดีแบบ Pong", "หนืดเหมือนโคลน")
FLOORS = (0xA8E6FF, 0x8899AA, 0x8B5A2B)         # สีพื้น: น้ำแข็ง · ปกติ · โคลน
STOP = 0.2                                      # ช้ากว่านี้ถือว่าหยุดแล้ว (v = 0)
SEGS = 7                                        # ก้อนแถบความเร็วต่อข้าง
SHIP_W, SHIP_Y, FLOOR_Y, BAR_Y = 62, 150, 180, 250
CENTER = game.WIDTH // 2


# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ ----
def next_speed(v, push, friction):
    # การ์ด 3 ทั้งใบ: push = -1 ซ้าย · +1 ขวา · 0 ปล่อย
    if push != 0:
        v += push * ACCEL                       # กดค้าง -> เร่ง
    else:
        v *= friction                           # ปล่อย -> หน่วง
    return max(-VMAX, min(VMAX, v))             # เพดาน


def bar_level(v):
    # v -> จำนวนก้อนที่ติด มีเครื่องหมาย (-7 ถึง +7)
    n = int(abs(v) / VMAX * SEGS + 0.5)
    return -n if v < 0 else n


def box_on(side, i, level):
    # ก้อนที่ i (นับจากกลาง) ของข้าง side (-1 ซ้าย / +1 ขวา) ควรติดไหม
    return level * side > 0 and i < abs(level)


# ---- 4) หน้าจอ: สร้างครั้งเดียว ----
game.title("SLIDE")
game.Text("กดค้าง: v += a     ปล่อย: v *= f     เพดาน: -%d ถึง +%d" % (int(VMAX), int(VMAX)), 16, 4, game.YELLOW)
feel_text = game.Text("", 16, 34, game.CYAN)
slide_text = game.Text("เร่งให้เต็มแถบ แล้วปล่อยจอย", 16, 62, game.WHITE)
floor = game.Box(0, FLOOR_Y, game.WIDTH, 10, FLOORS[0])
flag = game.Box(-20, FLOOR_Y - 40, 4, 40, game.YELLOW)     # ธงเหลือง = จุดที่เพิ่งปล่อยจอย
ship = game.Sprite("ship", CENTER - SHIP_W // 2, SHIP_Y)
bar = {-1: [], 1: []}
for side in (-1, 1):
    for i in range(SEGS):                       # ก้อนนอกสุดสีแดง = เพดาน
        left = CENTER + 6 + i * 46 if side > 0 else CENTER - 46 - i * 46
        seg = game.Box(left, BAR_Y, 40, 24, game.RED if i == SEGS - 1 else game.GREEN)
        seg.hide()
        bar[side].append(seg)
game.Box(CENTER - 2, BAR_Y - 6, 4, 36, game.WHITE)
game.Text("-%d" % int(VMAX), CENTER - 46 * SEGS, BAR_Y + 34, game.WHITE)
game.Text("+%d" % int(VMAX), CENTER + 46 * SEGS - 30, BAR_Y + 34, game.WHITE)
v_text = game.Text("", CENTER - 30, BAR_Y + 34, game.WHITE)

x, v, mode, prev_push = float(CENTER - SHIP_W // 2), 0.0, 0, 0
slide = None                                    # [เฟรม, พิกเซล, f] ระหว่างวัดระยะไถล
shown = {"level": 0, "v": ""}


def set_mode(m):
    feel_text.set("f = %.2f   %s   (A = เปลี่ยน)" % (FRICTIONS[m], FEELS[m]))
    floor.set_color(FLOORS[m])


set_mode(0)


# ---- 5) วงวนหลัก ----
def on_frame():
    global x, v, mode, prev_push, slide
    keys = game.keys()
    if game.pressed_once("a", keys):            # เข้า: เปลี่ยน f
        mode = (mode + 1) % len(FRICTIONS)
        set_mode(mode)
        game.sfx("select")
    push = (1 if keys.right else 0) - (1 if keys.left else 0)

    # คิด: ปุ่มเปลี่ยน "ความเร็ว" แล้วความเร็วค่อยเปลี่ยนตำแหน่ง
    v = next_speed(v, push, FRICTIONS[mode])
    if push == 0 and abs(v) < STOP:
        v = 0.0
    if push == 0 and prev_push != 0 and abs(v) > 1:
        slide = [0, 0.0, FRICTIONS[mode]]       # เพิ่งปล่อย -> เริ่มวัดระยะไถล ปักธงตรงนี้
        flag.move_to(int(x) + SHIP_W // 2, flag.y)
    elif push != 0:
        slide = None
    prev_push = push
    want = x + v
    x = max(0.0, min(game.WIDTH - SHIP_W, want))
    hit_wall = x != want                        # ชนขอบจอ = หยุด (เร็วพอถึงจะมีเสียง)
    if hit_wall and abs(v) > 2: game.sfx("wall")
    if hit_wall:                v = 0.0

    # ออก: ยาน + แถบ + ป้าย เฉพาะที่เปลี่ยน
    ship.move_to(x, SHIP_Y)
    if slide is not None:
        slide[0] += 1
        slide[1] += abs(v)
        if v == 0.0:
            slide_text.set("f %.2f  ปล่อยแล้วไถล %d เฟรม  %d px%s"
                           % (slide[2], slide[0], int(slide[1]), "  (ชนขอบ)" if hit_wall else ""))
            slide = None
    level = bar_level(v)
    if level != shown["level"]:
        for side in (-1, 1):
            for i in range(SEGS):
                now, before = box_on(side, i, level), box_on(side, i, shown["level"])
                if now and not before: bar[side][i].show()
                if before and not now: bar[side][i].hide()
        shown["level"] = level
    text = "v %+.1f" % v
    if text != shown["v"]:
        v_text.set(text)
        shown["v"] = text
    return True


game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) FRICTIONS = (0.95, 0.78, 0.30) -> (0.99, 0.78, 0.30) : ทายก่อนว่าไถลกี่ px (สูตร v0 * f / (1 - f))
#    แล้วเร่งเต็มที่ ปล่อย และอ่านบรรทัด "ไถล" — ไถลไกลจนชนขอบไหม
# 2) VMAX = 13.0 -> 6.0 : แถบชนก้อนแดงเร็วขึ้น ยานไม่มีทางเร็วกว่าเพดาน แม้กดค้างนานแค่ไหน
# 3) ACCEL = 1.4 -> 0.3 : ยานหนักเหมือนรถบรรทุก ต้องกดค้างนานกว่าจะถึงเพดาน
