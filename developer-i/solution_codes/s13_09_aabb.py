# s13_09_aabb.py — คาบ 13: ชนแบบกล่อง เห็นสี่เงื่อนไขเป็น True/False สด ๆ บนจอ
#
# หลักการ   : ชนแบบกล่อง (คาบ 4, 12) กล่อง A ชนกล่อง B เมื่อสี่ข้อนี้จริง "พร้อมกัน" (และ / and)
#             ax < bx+bw   และ   ax+aw > bx   และ   ay < by+bh   และ   ay+ah > by
#             ข้อ 1-2 ดูแกน x (ซ้าย-ขวาซ้อนกันไหม) · ข้อ 3-4 ดูแกน y (บน-ล่างซ้อนกันไหม)
# ลองเล่น   : กด Start · จอย = ขับกล่องเหลือง (A) ไปรอบ ๆ สามโซน
#             แผงขวาแสดงตัวเลขจริงของ A กับโซนที่ใกล้ที่สุด (B) และสี่ข้อเป็น True / False พร้อมไฟ
#             ขับเข้าใกล้ทีละแกน ดูไฟเขียวทีละดวง ครบสี่ดวงเมื่อไร โซนกลายเป็นแดงและมีเสียงเตือน
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : กระสุนโดนศัตรู · ลูกโดนไม้ · ของตกลงตะกร้า (game.hit ใน bentogame.py)
# ในงานจริง : หุ่นยนต์ส่งของ (AGV) เข้าโซนห้ามเข้า · กล่องผ่านเซนเซอร์ลำแสง · นิ้วแตะโดนปุ่มบนจอสัมผัส
#             ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : s04_catch.py (ของตกลงตะกร้า) · shooter_step4.py (ชนหลายคู่)
# งบวิดเจ็ต : 24 ชิ้น (สนาม 1 + โซน 3 + ป้ายโซน 3 + กล่อง A 1 + ไฟ 5 + ข้อความ 10 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   4) หน้าจอ: สร้างครั้งเดียว   5) วงวนหลัก
import bentogame as game

# ---- 1) ตั้งค่า (แก้ได้) ----
STEP = 4                                  # กดจอยค้าง กล่อง A ขยับเฟรมละ 4 px
A_W, A_H = 46, 30                         # ขนาดกล่อง A (หุ่นยนต์ส่งของ AGV)
FIELD_X, FIELD_Y, FIELD_W, FIELD_H = 12, 10, 450, 350   # ขอบสนามลงตัวกับก้าว 4 px จอดชิดขอบโซนได้พอดี
ZONES = [                                 # (ชื่อ, x, y, w, h, สีปกติ)
    ("ZONE 1", 40, 40, 140, 90, 0x2D5F8A),
    ("ZONE 2", 290, 70, 150, 100, 0x2E7D4F),
    ("ZONE 3", 130, 220, 170, 90, 0x8A3D6E),
]
HUD_X = 478
LAMP_OFF = 0x3A4450
NAMES = ("ax < bx+bw", "ax+aw > bx", "ay < by+bh", "ay+ah > by")
OPS = ("<", ">", "<", ">")


# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ ----
def aabb_parts(a, b):
    # สี่ข้อของการชนแบบกล่อง เรียงเหมือน game.hit() ใน bentogame.py ทุกตัวอักษร
    return (a.x < b.x + b.w, a.x + a.w > b.x, a.y < b.y + b.h, a.y + a.h > b.y)


def part_numbers(a, b):
    # ตัวเลขซ้าย-ขวาของแต่ละข้อ เอาไว้โชว์ว่าเทียบอะไรกับอะไร
    return ((a.x, b.x + b.w), (a.x + a.w, b.x), (a.y, b.y + b.h), (a.y + a.h, b.y))


def nearest(a, boxes):
    # โซนที่จุดกลางอยู่ใกล้จุดกลางของ A ที่สุด (เทียบระยะยกกำลังสอง ไม่ต้องถอดราก)
    ax, ay = a.center()
    best, best_d = 0, None
    for i, b in enumerate(boxes):
        bx, by = b.center()
        d = (ax - bx) * (ax - bx) + (ay - by) * (ay - by)
        if best_d is None or d < best_d:
            best, best_d = i, d
    return best


# ---- 4) หน้าจอ: สร้างครั้งเดียว ----
game.title("AABB")
game.Box(FIELD_X, FIELD_Y, FIELD_W, FIELD_H, 0x18202A, border=0x3A4A5A)
zones = []
for name, x, y, w, h, color in ZONES:
    zones.append(game.Box(x, y, w, h, color, border=game.WHITE, radius=4))
    game.Text(name, x + 8, y + 6, game.WHITE)
agv = game.Box(332, 270, A_W, A_H, game.YELLOW, border=game.BLACK, border_w=2, radius=3)

hud_a = game.Text("", HUD_X, 12, game.YELLOW)
hud_name = game.Text("", HUD_X, 40, game.WHITE)
hud_b = game.Text("", HUD_X, 66, game.WHITE)
lamps, lines = [], []
for i in range(4):
    lamps.append(game.Box(HUD_X, 110 + i * 32, 14, 14, LAMP_OFF, radius=7))
    lines.append(game.Text("", HUD_X + 22, 104 + i * 32, game.WHITE))
hit_lamp = game.Box(HUD_X, 242, 30, 30, LAMP_OFF, radius=15)
hud_hit = game.Text("", HUD_X + 40, 246, game.WHITE)
game.Text("ผิดข้อเดียว = ไม่ชน", HUD_X, 290, 0x9AA8B8)
game.Text("จอย = ขับกล่อง A", HUD_X, 322, game.GB_LIGHT)

shown = {"lines": [""] * 4, "parts": [None] * 4, "near": None, "a": "", "hits": None}

# ---- 5) วงวนหลัก ----
def on_frame():
    keys = game.keys()

    # รับปุ่ม (วัด): ขับกล่อง A อยู่ในสนาม
    dx = (STEP if keys.right else 0) - (STEP if keys.left else 0)
    dy = (STEP if keys.down else 0) - (STEP if keys.up else 0)
    if dx or dy:
        x = max(FIELD_X, min(FIELD_X + FIELD_W - A_W, agv.x + dx))
        y = max(FIELD_Y, min(FIELD_Y + FIELD_H - A_H, agv.y + dy))
        agv.move_to(x, y)

    # คิด (ตัดสิน): โซนไหนใกล้สุด แล้วสี่ข้อของคู่นั้นเป็นอย่างไร · โซนไหนชนบ้าง
    near = nearest(agv, zones)
    b = zones[near]
    parts = aabb_parts(agv, b)
    nums = part_numbers(agv, b)
    hits = tuple(all(aabb_parts(agv, z)) for z in zones)    # ผลเดียวกับ game.hit(agv, z)

    # ออก (โชว์): อัปเดตเฉพาะที่เปลี่ยน
    text = "A x=%d y=%d w=%d h=%d" % (agv.x, agv.y, A_W, A_H)
    if text != shown["a"]:
        hud_a.set(text)
        shown["a"] = text
    if near != shown["near"]:
        hud_name.set("B = %s (ใกล้สุด)" % ZONES[near][0])
        hud_b.set("B x=%d y=%d w=%d h=%d" % (b.x, b.y, b.w, b.h))
        shown["near"] = near
    for i in range(4):
        text = "%s  %d%s%d  %s" % (NAMES[i], nums[i][0], OPS[i], nums[i][1],
                                   "True" if parts[i] else "False")
        if text != shown["lines"][i]:
            lines[i].set(text)
            shown["lines"][i] = text
        if parts[i] != shown["parts"][i]:
            lamps[i].set_color(game.GREEN if parts[i] else LAMP_OFF)
            shown["parts"][i] = parts[i]
    if hits != shown["hits"]:
        for i in range(len(zones)):
            if shown["hits"] is None or hits[i] != shown["hits"][i]:
                zones[i].set_color(game.RED if hits[i] else ZONES[i][5])
        if True in hits:
            hud_hit.set("HIT = True  " + ZONES[hits.index(True)][0])
            hit_lamp.set_color(game.RED)
            game.sfx("deny")
        else:
            hud_hit.set("HIT = False")
            hit_lamp.set_color(LAMP_OFF)
        shown["hits"] = hits
    return True


game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) ขับ A ไปอยู่ใต้ ZONE 2 (แนว x ซ้อนกัน แต่ y ไม่ซ้อน) : ไฟข้อ 1-2 เขียว ข้อ 3-4 ไม่ครบ = ไม่ชน
# 2) จอด A ชิดขอบซ้ายของ ZONE 2 พอดี (ข้อ 2 อ่าน 290>290 False = แตะขอบพอดียังไม่ชน)
#    แล้วเปลี่ยน a.x + a.w > b.x ใน aabb_parts เป็น >= : จอดที่เดิม ข้อ 2 กลายเป็น True ทันที
# 3) A_W, A_H = 46, 30 -> 120, 80 : รถใหญ่ขึ้น ชนง่ายขึ้นมาก ไม่ต้องขยับเข้าไปลึกก็ครบสี่ข้อ
