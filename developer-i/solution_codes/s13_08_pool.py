# s13_08_pool.py — คาบ 13 ส่วนที่ 3: ชุดหมุนเวียน (ยืม-คืน ไม่สร้างใหม่ระหว่างเล่น)
#
# หลักการ   : ชุดหมุนเวียน (คาบ 11) สร้างของ 8 ตัวครั้งเดียวตอนเริ่ม แล้วยืม-คืนใช้ซ้ำ
#             ว่าง ⇔ y < -20 (จอดรอนอกจอ) · ยืม = หาตัวว่างตัวแรก · คืน = บินพ้นขอบบนแล้วนับว่าว่าง
#             ช่องเก็บของ (ล็อกเกอร์) 8 ช่องด้านล่างคือ "ภาพ" ของรายการ items ทั้ง 8 ช่อง
# ลองเล่น   : กด Start · กด A = ยืม 1 ตัว ปล่อยขึ้นจากช่องของมันเอง
#             ช่องเปลี่ยนเขียว -> ส้ม ตอนยืม และกลับเป็นเขียวตอนของบินพ้นจอ · ขีดขาว = ช่องที่จะยืมต่อไป
#             ของแต่ละช่องบินเร็วไม่เท่ากัน ช่องกลาง ๆ จึงว่างก่อนช่องซ้ายได้ ดูขีดขาวกระโดดไปหา
#             กด A รัว ๆ จนครบ 8 แล้วกดอีก = เสียงปฏิเสธ (ชุดเต็ม ต้องรอคืนก่อน)
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : กระสุน · ศัตรู · ระเบิด (shooter_step3.py ใช้กระสุน 6 นัดวนใช้ซ้ำ)
# ในงานจริง : ลานจอดรถ 8 ช่องที่ป้ายบอกช่องว่าง · ถาดพัสดุบนสายพานที่วนกลับมารับของใหม่
#             จองครั้งเดียวไม่ขอหน่วยความจำเพิ่มระหว่างทำงาน · ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : shooter_step3.py (ชุดกระสุนหมุนเวียน) · shooter_step5.py (ชุดศัตรู)
# งบวิดเจ็ต : 24 ชิ้น (ของ 8 + ช่อง 8 + ขีดขาว 1 + ข้อความ 6 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   4) หน้าจอ: สร้างครั้งเดียว   5) วงวนหลัก
import bentogame as game

# ---- 1) ตั้งค่า (แก้ได้) ----
POOL = 8                    # ขนาดชุด (งบวิดเจ็ตจำกัด: ของ 1 ตัวกิน 2 ชิ้น คือตัวมันกับช่องของมัน)
SPEEDS = (2, 3, 4)          # ความเร็วบินขึ้น (px ต่อเฟรม) วนตามเลขช่อง 0, 1, 2, 0, 1, ...
LOCK_X0, LOCK_Y, LOCK_W, LOCK_H, LOCK_GAP = 34, 316, 44, 30, 54
LAUNCH_Y = LOCK_Y - 22      # จุดปล่อยของ เหนือช่องของตัวเอง
FREE_COLOR, USED_COLOR = game.GREEN, game.ORANGE
HUD_X = 500


# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ ----
def is_free(item):
    return item.y < -20                 # จอดรอนอกจอ = ว่าง


def find_free(items):
    for i in range(len(items)):         # ไล่จากช่องซ้ายสุด เจอตัวว่างตัวแรกก็ใช้เลย
        if is_free(items[i]):
            return i
    return None                         # ใช้อยู่ครบทุกตัว


def count_free(items):
    n = 0
    for item in items:
        if is_free(item):
            n += 1
    return n


def locker_x(i):
    return LOCK_X0 + i * LOCK_GAP


# ---- 4) หน้าจอ: สร้างครั้งเดียว ----
game.title("POOL")
lockers = [game.Box(locker_x(i), LOCK_Y, LOCK_W, LOCK_H, FREE_COLOR, border=game.WHITE, radius=4)
           for i in range(POOL)]
cursor = game.Box(locker_x(0), LOCK_Y + LOCK_H + 5, LOCK_W, 4, game.WHITE)
items = []
for i in range(POOL):                   # สร้างครบ 8 ตัวตรงนี้ที่เดียว ในวงวนหลักไม่สร้างเพิ่มเลย
    item = game.Sprite("enemy" if i % 2 == 0 else "enemy2", -100, -100)
    item.w = 16 if i % 2 == 0 else 21
    item.hide()
    items.append(item)

game.Text("POOL %d    free = y < -20" % POOL, HUD_X, 20, game.CYAN)
hud_count = game.Text("", HUD_X, 58, game.WHITE)
hud_msg = game.Text("A = ยืม 1 ตัว", HUD_X, 96, game.YELLOW)
game.Text("เขียว = ว่าง    ส้ม = ใช้อยู่", HUD_X, 150, game.WHITE)
game.Text("ขีดขาว = ช่องที่จะยืมต่อไป", HUD_X, 186, game.WHITE)
game.Text("ลานจอด %d ช่อง / ถาดบนสายพาน" % POOL, HUD_X, 240, 0x9AA8B8)

shown = {"free": None, "next": 0}

# ---- 5) วงวนหลัก ----
def on_frame():
    keys = game.keys()

    # ยืม: กด A หนึ่งครั้ง = ขอหนึ่งตัว
    if game.pressed_once("a", keys):
        i = find_free(items)
        if i is None:
            hud_msg.set("เต็ม! รอของบินพ้นจอก่อน")
            game.sfx("deny")
        else:
            item = items[i]
            item.move_to(locker_x(i) + (LOCK_W - item.w) // 2, LAUNCH_Y)
            item.show()
            lockers[i].set_color(USED_COLOR)
            hud_msg.set("ยืมช่อง %d ไปแล้ว" % (i + 1))
            game.sfx("select")

    # อัปเดตเฉพาะตัวที่ใช้อยู่ + คืนเมื่อพ้นขอบบน
    for i in range(POOL):
        item = items[i]
        if not is_free(item):
            item.move_to(item.x, item.y - SPEEDS[i % len(SPEEDS)])
            if is_free(item):               # เพิ่งพ้น -20 = คืนเข้าชุดแล้ว
                item.hide()
                lockers[i].set_color(FREE_COLOR)
                hud_msg.set("คืนช่อง %d แล้ว" % (i + 1))
                game.tone(76, ms=40)

    # โชว์: เปลี่ยนเฉพาะเมื่อจำนวนว่างหรือช่องถัดไปเปลี่ยน
    free = count_free(items)
    if free != shown["free"]:
        hud_count.set("FREE %d    USED %d" % (free, POOL - free))
        shown["free"] = free
    nxt = find_free(items)
    if nxt != shown["next"]:
        if nxt is None:
            cursor.hide()
        else:
            cursor.move_to(locker_x(nxt), LOCK_Y + LOCK_H + 5)
            cursor.show()
        shown["next"] = nxt
    return True


game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) SPEEDS = (2, 3, 4) -> (3,) : ทุกตัวบินเร็วเท่ากัน ของที่ยืมก่อนคืนก่อนเสมอ (เข้าก่อน ออกก่อน)
# 2) POOL = 8 -> 4 : เต็มเร็วขึ้นมาก ได้ยินเสียงปฏิเสธบ่อย แต่คืนงบจอได้ 8 ชิ้น (ของ 4 + ช่อง 4)
#    กลับกัน POOL = 10 จะเกินงบ 26 ชิ้น เพราะของหนึ่งตัวกินสองชิ้น: ตัวมันกับช่องของมัน
# 3) ลบ item.hide() ในส่วนคืน : เกมยังทำงานเหมือนเดิม เพราะที่ทำให้ "ว่าง" คือค่า y < -20 ไม่ใช่การซ่อน
