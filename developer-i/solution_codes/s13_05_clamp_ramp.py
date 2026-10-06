# s13_05_clamp_ramp.py — คาบ 13 (คาบพิเศษ) การ์ด 4: จำกัดขอบ และเร่งแบบมีเพดาน
#
# หลักการ   : การ์ด 4 จำกัดขอบ        x = max(ซ้ายสุด, min(ขวาสุด, x))  ทำทุกเฟรม ทุกแกน    (คาบ 3)
#             เร่งแบบมีเพดาน          speed = min(BASE + hold, MAX)  กดค้างยิ่งนานยิ่งเร็ว
#             สูตรเดียวกับ s03_box_joystick.py ที่ game.Box.move() จำกัดขอบจอให้ ไฟล์นี้เขียนเองให้เห็น
# ลองเล่น   : กด Start · จอยค้าง = วิ่ง (ยิ่งค้างนานยิ่งเร็ว) · ปล่อย = hold กลับเป็น 0
#             A = สลับเพดาน MAX ระหว่าง 30 กับ 10 แล้วดูขีดแดงบนแถบความเร็วเลื่อน
#             วิ่งชนขอบ: ขอบด้านนั้นกับตัวกล่องเปลี่ยนเป็นสีส้มพร้อมเสียง แต่กล่องไม่มีวันหลุดกรอบ
#             ดูบรรทัดสีขาว: เลขในสูตร min(...) เปลี่ยนตามเฟรมที่กดค้างจริง
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : ตัวละครไม่หลุดจอ · กดค้างแล้ววิ่งเร็วขึ้น · ศัตรูเร็วขึ้นตามคะแนนแต่มีเพดาน
# ในงานจริง : ค่าตั้งอุณหภูมิไม่เกินช่วงปลอดภัย · มอเตอร์มีความเร็วสูงสุด
#             เคอร์เซอร์บนหน้าจอควบคุมเร่งตามเวลาที่ดันก้าน · ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : s03_box_joystick.py (BASE_SPEED · MAX_SPEED · hold_frames)
# งบวิดเจ็ต : 22 ชิ้น (ข้อความ 4 + รางแถบ 1 + แถบ 10 + ขีดเพดาน 1 + ขอบ 4 + กล่อง 1 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game

# ---- 1) ตั้งค่า (แก้ได้) ----
BASE_SPEED = 6                  # ความเร็วฐาน (ค่าเดียวกับ s03_box_joystick.py) เฟรมแรกที่แตะจอยได้ 6 + 1 = 7
MAX_CHOICES = (30, 10)          # A สลับเพดานความเร็วระหว่างสองค่านี้
PER_BOX = 3                     # แถบความเร็ว 1 ก้อน = 3 พิกเซลต่อเฟรม (10 ก้อน = 30)
BAR_STEP = 3                    # แถบเปลี่ยนได้เฟรมละไม่เกิน 3 ก้อน (ไม่ส่งข้อความรวดเดียว)
HERO = 40                       # กล่องกว้าง/สูง 40 px แบบ s03
AX, AY, AW, AH = 8, 94, 776, 272                        # กรอบสนาม (ขอบหนา 6 px)
X_MIN, X_MAX = AX + 6, AX + AW - 6 - HERO
Y_MIN, Y_MAX = AY + 6, AY + AH - 6 - HERO
DIM = 0x334455


# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ (ทดสอบได้โดยไม่ต้องมีบอร์ด) ----
def clamp(value, low, high):
    # การ์ด 4 ครึ่งแรก: ค่าที่เปลี่ยนได้ต้องมีขอบเสมอ
    return max(low, min(high, value))


def ramp_speed(hold, max_speed):
    # การ์ด 4 ครึ่งหลัง: กดค้างยิ่งนานยิ่งเร็ว แต่ไม่เกินเพดาน
    return min(BASE_SPEED + hold, max_speed)


def touching(x, y):
    # แตะขอบไหนอยู่บ้าง: (ซ้าย, ขวา, บน, ล่าง)
    return (x == X_MIN, x == X_MAX, y == Y_MIN, y == Y_MAX)


# ---- 3) ตรวจสมองก่อนเปิดจอ ----
def self_test():
    ok = (ramp_speed(0, 30) == 6 and ramp_speed(14, 30) == 20 and ramp_speed(100, 10) == 10
          and clamp(-5, 0, 10) == 0 and clamp(15, 0, 10) == 10)
    print("ผ่าน! ramp_speed กับ clamp ถูกทุกกรณี" if ok else "ยังไม่ผ่าน: ตรวจ ramp_speed กับ clamp")
    return ok


if not self_test():
    raise SystemExit

# ---- 4) หน้าจอ: สร้างครั้งเดียว ----
game.title("CLAMP")
game.Text("speed = min(BASE + hold, MAX)       x = max(ซ้าย, min(ขวา, x))", 16, 4, game.YELLOW)
live = game.Text("กดจอยค้าง ดูความเร็วไต่ขึ้น", 16, 32, game.WHITE)
game.Box(12, 60, 10 * 34 + 4, 26, 0x16222E, border=DIM)
bar = [game.Box(16 + i * 34, 64, 30, 18, game.GREEN) for i in range(10)]   # แถบความเร็ว 10 ก้อน
for seg in bar:
    seg.hide()
ceiling = game.Box(0, 56, 4, 34, game.RED)                  # ขีดแดง = เพดาน MAX
max_text = game.Text("", 372, 62, game.CYAN)
pos_text = game.Text("", 590, 62, game.WHITE)
edges = [game.Box(AX, AY, 6, AH, DIM), game.Box(AX + AW - 6, AY, 6, AH, DIM),    # ซ้าย ขวา
         game.Box(AX, AY, AW, 6, DIM), game.Box(AX, AY + AH - 6, AW, 6, DIM)]    # บน ล่าง
hero = game.Box((X_MIN + X_MAX) // 2, (Y_MIN + Y_MAX) // 2, HERO, HERO, game.CYAN, radius=8)

hold, max_i, bar_on = 0, 0, 0
shown = {"live": "", "pos": "", "touch": (False, False, False, False)}


def set_max():
    top = MAX_CHOICES[max_i]
    max_text.set("MAX %d  (A = สลับ)" % top)
    ceiling.move_to(16 + top * 34 // PER_BOX - 4, ceiling.y)


def show(widget, key, text):
    if text != shown[key]:                                  # ป้ายเปลี่ยนเมื่อข้อความเปลี่ยนเท่านั้น
        widget.set(text)
        shown[key] = text


set_max()


# ---- 5) วงวนหลัก ----
def on_frame():
    global hold, max_i, bar_on
    k = game.keys()
    if game.pressed_once("a", k):                           # เข้า: สลับเพดาน
        max_i = 1 - max_i
        set_max()
        game.sfx("select")

    # คิด: แบบเดียวกับ s03 แล้วจำกัดขอบเองทั้งสองแกน
    moving = k.left or k.right or k.up or k.down
    hold = hold + 1 if moving else 0                        # กดค้าง -> สะสม · ปล่อย -> 0
    top = MAX_CHOICES[max_i]
    speed = ramp_speed(hold, top) if moving else 0
    x = clamp(hero.x + (speed if k.right else 0) - (speed if k.left else 0), X_MIN, X_MAX)
    y = clamp(hero.y + (speed if k.down else 0) - (speed if k.up else 0), Y_MIN, Y_MAX)
    touch = touching(x, y)

    # ออก: กล่อง ขอบ เสียง ป้าย และแถบ เฉพาะที่เปลี่ยน
    if x != hero.x or y != hero.y:
        hero.move_to(x, y)
    if touch != shown["touch"]:
        for i in range(4):
            if touch[i] != shown["touch"][i]:
                edges[i].set_color(game.ORANGE if touch[i] else DIM)
                if touch[i]:
                    game.sfx("wall")                        # เสียงเฉพาะตอนเพิ่งแตะ ไม่ดังรัว
        if any(touch) != any(shown["touch"]):
            hero.set_color(game.ORANGE if any(touch) else game.CYAN)
        shown["touch"] = touch
    if moving:
        show(live, "live", "hold %d  ->  speed = min(%d + %d, %d) = %d" % (hold, BASE_SPEED, hold, top, speed))
    else:
        show(live, "live", "ปล่อยจอย: hold = 0  speed = 0")
    show(pos_text, "pos", "x %d  y %d" % (x, y))
    want = min(speed // PER_BOX, len(bar))
    for _ in range(BAR_STEP):                               # ไล่แถบเข้าหาค่าจริงทีละก้อน
        if bar_on < want:
            bar[bar_on].show()
            bar_on += 1
        elif bar_on > want:
            bar_on -= 1
            bar[bar_on].hide()
    return True


game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) MAX_CHOICES = (30, 10) -> (30, 20) แล้วกด A : ทายก่อนว่ากดค้างกี่เฟรมความเร็วถึงหยุดเพิ่ม (20 - 6 = 14 เฟรม)
# 2) ลบ clamp(...) ออกจากบรรทัด x = ... (เหลือ hero.x + ...) : วิ่งชนขวาแล้วกล่องหายออกนอกจอ ไม่กลับมา
# 3) BASE_SPEED = 6 -> 0 : แตะจอยสั้น ๆ แทบไม่ขยับ ต้องค้างไว้ก่อนถึงจะเริ่มวิ่ง — เหมาะกับเคอร์เซอร์ที่ต้องเล็งละเอียด
