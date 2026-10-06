# s13_06_deadzone.py — คาบ 13 การ์ด 5: ตัวควบคุมสามระดับ + ช่องว่าง (เห็นไม้สั่นกับตา)
#
# หลักการ   : การ์ด 5 ตัวควบคุมสามระดับ   ค่าคลาด e = เป้า - ตำแหน่งไม้                (คาบ 7)
#             e > BAND -> +1 (ขยับลง) · e < -BAND -> -1 (ขยับขึ้น) · นอกนั้น 0 (อยู่นิ่ง)
#             กฎเดียวกับไม้ AI ใน pong_step5.py ที่ใช้ช่องว่าง ±6 px
# ลองเล่น   : กด Start · จอยขึ้น/ลง = ย้ายลูกบอล (เป้า) · ไม้ AI ไล่ตามเอง
#             A = สลับ BAND ระหว่าง 6 กับ 0 · B = ล้างตัวนับ FLIPS
#             BAND 6: ไม้หยุดนิ่งในแถบเขียว ไฟ HOLD ติดค้าง ตัวนับ FLIPS นิ่ง
#             BAND 0: ไม้สั่นกึก ๆ ไฟสลับเขียว-ส้มแทบทุกเฟรม FLIPS พุ่ง และได้ยินเสียงติ๊กรัว
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : ไม้ AI ของ Pong ขยับขึ้น หยุด หรือลง ตามลูก
# ในงานจริง : วาล์วมอเตอร์ เปิดเพิ่ม / หยุด / ปิดลง ตามค่าคลาดของระดับน้ำ · ติ๊กหนึ่งครั้ง = กลับทิศหนึ่งครั้ง
#             ยิ่งถี่ยิ่งสึก ช่องว่างจึงสำคัญ · ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : pong_step5.py (ไม้ AI + ช่องว่าง) · s13_tank_hmi.py (วาล์วถังน้ำ)
# งบวิดเจ็ต : 21 ชิ้น (สนาม 1 + เส้นเป้า 1 + แถบช่องว่าง 1 + ลูก 1 + ไม้ 1 + ขีดกลางไม้ 1
#             + ไฟ 3 + ข้อความ 11 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   4) หน้าจอ: สร้างครั้งเดียว   5) วงวนหลัก
import bentogame as game
import random

# ---- 1) ตั้งค่า (แก้ได้) ----
BAND_ON = 6         # ช่องว่าง ±6 px แบบ pong_step5.py (กด A สลับกับ 0)
SPEED = 5           # ไม้ AI ขยับเฟรมละ 5 px คงที่ — มีแค่ ขึ้น / หยุด / ลง ไม่มีครึ่ง ๆ กลาง ๆ
BALL_STEP = 4       # กดจอยค้าง ลูก (เป้า) ขยับเฟรมละ 4 px
NOISE = 1           # ค่าที่วัดได้สั่น ±1 px เหมือนเซนเซอร์จริง (0 = วัดเป๊ะ)
TOP, BOTTOM = 44, 356           # ขอบบน/ล่างของสนาม
BALL_X, AI_X = 90, 330          # คอลัมน์ของลูก และของไม้ AI
BALL, PADDLE_H = 14, 90         # ขนาดสไปรต์ ball 14x14 · paddle 14x90
HUD_X = 460
LAMP_OFF = 0x3A4450
LAMP_ON = {1: game.GREEN, 0: game.CYAN, -1: game.ORANGE}
MEANING = {1: "OPEN", 0: "HOLD", -1: "CLOSE"}

# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ ----
def controller(error, band):
    # การ์ด 5: ตัวควบคุมสามระดับ คืน +1 = ลง/เปิดเพิ่ม · 0 = นิ่ง/หยุด · -1 = ขึ้น/ปิดลง
    if error > band:
        return 1
    if error < -band:
        return -1
    return 0                           # ใกล้พอแล้ว -> อยู่นิ่ง

def signed(value):
    # ใส่เครื่องหมาย + ให้เลขบวก จะได้เห็นทิศของค่าคลาด
    return ("+%d" % value) if value > 0 else ("%d" % value)

# ---- 4) หน้าจอ: สร้างครั้งเดียว (ของที่สร้างก่อนอยู่ล่าง) ----
game.title("DEADZONE")
game.Text("TARGET", BALL_X - 26, 14, game.YELLOW)
game.Text("AI", AI_X - 4, 14, game.WHITE)
game.Box(30, TOP, 400, BOTTOM - TOP, 0x18202A, border=0x3A4A5A, border_w=1)
line = game.Box(BALL_X, 0, AI_X + 40 - BALL_X, 2, game.YELLOW)       # เส้นเป้าที่กลางลูก
band_box = game.Box(AI_X - 14, 0, 42, 2 * BAND_ON, 0x2E8B57)          # แถบช่องว่าง ±BAND
ball = game.Sprite("ball", BALL_X, 0)
paddle = game.Sprite("paddle", AI_X, 0)
mark = game.Box(AI_X - 8, 0, 30, 3, game.WHITE)                       # ขีดกลางไม้

hud_band = game.Text("", HUD_X, 24, game.WHITE)
hud_error = game.Text("", HUD_X, 60, game.WHITE)
hud_out = game.Text("", HUD_X, 96, game.WHITE)
lamps = {}
for i, out in enumerate((1, 0, -1)):
    lamps[out] = game.Box(HUD_X + 10 + i * 105, 138, 30, 30, LAMP_OFF, radius=15)
    game.Text("%s %s" % (signed(out), MEANING[out]), HUD_X + i * 105, 176, game.WHITE)
game.Text("วาล์ว: เปิดเพิ่ม / หยุด / ปิดลง", HUD_X, 214, 0x9AA8B8)
hud_flips = game.Text("", HUD_X, 258, game.WHITE)
game.Text("UP/DOWN = เป้า  A = BAND", HUD_X, 300, game.GB_LIGHT)

ball_y = 180.0                  # มุมบนของลูก
ai_y = 60.0                     # มุมบนของไม้
band = BAND_ON
flips, last_dir = 0, 0
shown = {"ball": None, "ai": None, "band": None, "err": "", "out": None, "flips": None}

# ---- 5) วงวนหลัก ----
def on_frame():
    global ball_y, ai_y, band, flips, last_dir
    keys = game.keys()

    # รับปุ่ม (วัด)
    if keys.up:   ball_y = max(TOP + PADDLE_H / 2 - BALL / 2, ball_y - BALL_STEP)
    if keys.down: ball_y = min(BOTTOM - PADDLE_H / 2 - BALL / 2, ball_y + BALL_STEP)
    if game.pressed_once("a", keys):
        band = 0 if band else BAND_ON
        game.sfx("select")
    if game.pressed_once("b", keys):
        flips = 0

    # คิด (ตัดสิน): ค่าคลาดที่วัดได้ -> สามระดับ -> ไม้ขยับเท่า SPEED หรือไม่ขยับเลย
    target = ball_y + BALL / 2 - PADDLE_H / 2          # อยากให้กลางไม้ตรงกลางลูก
    error = target - ai_y + random.randint(-NOISE, NOISE)
    out = controller(error, band)
    ai_y += out * SPEED
    if out != 0:
        if last_dir != 0 and out != last_dir:        # กลับทิศ = วาล์วสลับหนึ่งครั้ง
            flips += 1
            game.tone(96, ms=12)
        last_dir = out

    # ออก (โชว์): อัปเดตเฉพาะที่เปลี่ยน
    cy = int(ball_y + BALL / 2)
    if cy != shown["ball"]:
        ball.move_to(BALL_X, int(ball_y))
        line.move_to(BALL_X, cy - 1)
        band_box.move_to(AI_X - 14, cy - BAND_ON)
        shown["ball"] = cy
    if band != shown["band"]:
        band_box.show() if band else band_box.hide()
        hud_band.set("BAND = %d px   (A)" % band)
        shown["band"] = band
    if int(ai_y) != shown["ai"]:
        paddle.move_to(AI_X, int(ai_y))
        mark.move_to(AI_X - 8, int(ai_y) + PADDLE_H // 2 - 1)
        shown["ai"] = int(ai_y)
    text = "ERROR = %s px" % signed(int(error))
    if text != shown["err"]:
        hud_error.set(text)
        shown["err"] = text
    if out != shown["out"]:
        if shown["out"] is not None:
            lamps[shown["out"]].set_color(LAMP_OFF)
        lamps[out].set_color(LAMP_ON[out])
        hud_out.set("OUT = %s   %s" % (signed(out), MEANING[out]))
        shown["out"] = out
    if flips != shown["flips"]:
        hud_flips.set("FLIPS = %d   (B = 0)" % flips)
        shown["flips"] = flips
    return True


game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) กด A ให้ BAND = 0 แล้วปล่อยจอย : ไม้สั่นไม่หยุด FLIPS ขึ้นหลายสิบครั้งในไม่กี่วินาที
#    วาล์วจริงที่ถูกสั่งกลับทิศถี่ขนาดนี้ สึกพังในไม่กี่วัน
# 2) SPEED = 5 -> 2 : ไม้ไล่ช้าลง แต่ตอน BAND 0 สั่นเบาลง (ก้าวสั้น) — เร็วกับนิ่งต้องแลกกัน
# 3) NOISE = 1 -> 8 : ค่าวัดสั่นแรงกว่าช่องว่าง FLIPS ขึ้นเองทั้งที่ BAND 6 — เพิ่ม BAND_ON จนนิ่ง
