# โครงเว้นบางส่วน — ช่องที่น้องต้องเติมเอง / เฉลย: solution_codes/s13_conveyor_sort.py / ใบ้: สไลด์คาบ 13 ส่วนที่ 7
# วิธีเล่น  : เติม 2 ช่อง "เติมส่วนนี้เอง" ในส่วน 2) สมอง แล้วรัน — ไฟล์ตรวจคำตอบเองก่อนเปิดจอ
#             ผ่านครบ = Console ขึ้น "ผ่าน!" แล้วเล่นได้เหมือนไฟล์เฉลย
#             ยังไม่ถูก = Console บอกว่ากรณีไหนผิด แล้วหยุด (ยังไม่เปิดจอ)
# s13_conveyor_sort.py — คาบ 13 (คาบพิเศษ): สายพานคัดแยกพัสดุ = Shooter คาบ 12 ที่เปลี่ยนความหมาย
#
# หลักการ   : โค้ดเดิมจาก shooter_step4.py ใช้ต่อได้เกือบทั้งหมด สิ่งที่เปลี่ยนคือ "ความหมาย"
#             ศัตรูที่ตก -> กล่องพัสดุบนสายพาน (ชุดหมุนเวียน คาบ 11 วนกลับขึ้นบน)
#             ยานมีน้ำหนัก -> สายพานเร่งแล้วค่อย ๆ หยุด (เร่ง + แรงเสียดทาน คาบ 6, 10)
#             ยิงโดน -> กล่องถึงประตูคัดแยก (ชนแบบกล่อง คาบ 4, 12) · ระเบิด -> กล่องลงช่องผิด
#             ชีวิต + GAME OVER -> โควตาของเสีย + หยุดสายพาน (เครื่องสถานะ คาบ 2, 7)
# ลองเล่น   : กด Start · กด A ค้าง = สายพานเร่ง (แถบบนสายพานวิ่งตาม) · ปล่อย = ค่อย ๆ หยุด
#             จอยซ้าย = ประตูชี้ช่อง A (แดง) · จอยขวา = ประตูชี้ช่อง B (น้ำเงิน) ช่องที่ชี้อยู่จะสว่าง
#             กล่องป้ายแดงต้องลงช่อง A กล่องป้ายน้ำเงินต้องลงช่อง B · คัดผิด = ระเบิดที่ช่องนั้น
#             คัดผิดครบ 3 ครั้ง = สายพานหยุด (STOPPED) · ผิดได้อีกครั้งเดียว = มีบรรทัดเตือนสีส้ม
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : Shooter คาบ 12 (ศัตรูตก · ชน · ระเบิด · คะแนน · ชีวิต · GAME OVER)
# ในงานจริง : หน้าจอสายพานคัดแยกในศูนย์กระจายสินค้า · แถบสถานะบอก RUNNING / IDLE / STOPPED
#             บรรทัดเตือนขึ้นก่อนหยุดสายพาน · ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และเครื่องจำลอง
# ต่อยอดจาก : shooter_step4.py (ชุดหมุนเวียน · ชน · ชีวิต) · pong_step2.py (เร่ง + แรงเสียดทาน)
# งบวิดเจ็ต : 26 ชิ้น (แถบสถานะ 4 + สายพาน 3 + กล่อง 4 + ประตู 1 + ช่อง 4 + ระเบิด 1
#             + การ์ดค่า 8 + game.run 1) · ต่อเฟรมส่งคำสั่งราว 6-8 ครั้ง (เฟรมที่กล่องถึงประตูราว 10-14)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game
import random

# ---- 1) ตั้งค่า (แก้ได้) ----
BOXES = 4            # กล่องบนสายพาน (ชุดหมุนเวียน สร้างครั้งเดียว)
GAP = 78             # ระยะห่างระหว่างกล่อง (px)
ACCEL = 0.20         # กด A ค้าง 1 เฟรม สายพานเร็วขึ้นเท่านี้
FRICTION = 0.96      # ปล่อย A แล้วความเร็วคูณเท่านี้ทุกเฟรม (ค่อย ๆ หยุด)
MAX_SPEED = 6.0      # เพดานความเร็วสายพาน (px ต่อเฟรม)
REJECTS = 3          # คัดผิดได้กี่ครั้งก่อนหยุดสายพาน
COLORS = [game.RED, game.BLUE]      # ป้ายสีที่ 0 = ช่อง A · ป้ายสีที่ 1 = ช่อง B

BELT_X, BELT_W, BELT_END = 336, 120, 300    # สายพานแนวตั้งกลางจอ (บนสุด y = 40)
BOX_W, BOX_H = 54, 42
LANE_X = BELT_X + (BELT_W - BOX_W) // 2
GATE_Y = 304
GATE_W = 160
GATE_X = {0: BELT_X - 100, 1: BELT_X + 60}  # ประตูเลื่อนไปทางช่อง A (ซ้าย) หรือช่อง B (ขวา)
CHUTE_X, CHUTE_W = {0: 146, 1: 416}, 230
DIM = {0: 0x2A1416, 1: 0x14202E}            # ช่องที่ประตูไม่ได้ชี้ (มืด)
LIT = {0: 0x6A2226, 1: 0x1E4A78}            # ช่องที่ประตูชี้อยู่ (สว่าง)
PANEL, EDGE = 0x1B2633, 0x3D5266
BOOM = ["boom1", "boom2", "boom3", "boom4", "boom5", "boom6"]



# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ (ทดสอบได้โดยไม่ต้องมีบอร์ด) ----
def belt_step(speed, pressed, accel, friction, vmax):
    # ----- เติมส่วนนี้เอง (งานของน้อง) (1): การ์ด 3 เร่ง หน่วง และเพดาน -----
    # TODO: กดอยู่ -> เร็วขึ้นทีละ accel แต่ไม่เกิน vmax
    #       ปล่อย  -> คูณ friction ทุกเฟรม และถ้าช้ากว่า 0.05 ให้หยุดสนิท (คืน 0.0)
    #   ใบ้: player_speed += ACCEL / player_speed *= FRICTION ใน pong_step2.py
    return speed                 # <- แก้บรรทัดนี้ (ตอนนี้สายพานไม่เร่งไม่หยุด)


def judge(color_index, gate_side):
    # ----- เติมส่วนนี้เอง (งานของน้อง) (2): คัดถูกหรือผิด -----
    # TODO: คืน True เมื่อสีของกล่องตรงกับช่องที่ประตูชี้อยู่ (สี 0 = ช่อง A · สี 1 = ช่อง B)
    return True                  # <- แก้บรรทัดนี้ (ตอนนี้ทุกกล่องนับว่าถูกหมด)


def next_y_above(ys, gap):
    # วนกล่องกลับขึ้นไปต่อท้ายแถว: เหนือกล่องที่อยู่สูงสุดอีกหนึ่งช่วง gap
    return min(ys) - gap


# ---- 3) ตรวจสมองก่อนเปิดจอ ----
def self_test():
    cases = [
        ("belt_step", belt_step(0.0, True, 0.5, 0.9, 6.0), 0.5),
        ("belt_step", belt_step(5.8, True, 0.5, 0.9, 6.0), 6.0),   # ชนเพดาน
        ("belt_step", belt_step(4.0, False, 0.5, 0.9, 6.0), 3.6),  # ปล่อย = คูณแรงเสียดทาน
        ("belt_step", belt_step(0.04, False, 0.5, 0.9, 6.0), 0.0), # ช้ามาก = หยุดสนิท
        ("judge", judge(0, 0), True),
        ("judge", judge(1, 1), True),
        ("judge", judge(1, 0), False),
        ("next_y_above", next_y_above([100, -20, 300], 90), -110),
    ]
    for name, got, want in cases:
        if abs(got - want) > 1e-6:
            print("ยังไม่ผ่าน:", name, "ได้", got, "ควรได้", want)
            return False
    print("ผ่าน! สมองของสายพานถูกทั้ง", len(cases), "กรณี")
    return True


if not self_test():
    raise SystemExit

# ---- 4) หน้าจอ: สร้างครั้งเดียว (สร้างก่อน = อยู่ล่าง สร้างทีหลัง = ทับข้างบน) ----
game.title("CONVEYOR SORT")
game.Box(BELT_X, 40, BELT_W, BELT_END - 40, 0x202833, border=0x8899AA, border_w=4)   # ผืนสายพาน + ราง
slats = [game.Box(BELT_X + 4, 60 + i * 130, BELT_W - 8, 8, 0x46546A) for i in range(2)]
box_color = [random.randint(0, 1) for _ in range(BOXES)]  # กล่องไม่จำสีตัวเอง จึงจำไว้ในลิสต์
boxes = [game.Box(LANE_X, BELT_END - 70 - i * GAP, BOX_W, BOX_H, COLORS[box_color[i]],
                  border=0xC08A4E, radius=3, border_w=8) for i in range(BOXES)]   # ขอบน้ำตาล = กล่อง · กลาง = ป้ายสี

bar = game.Box(-4, -4, 800, 44, PANEL, border=EDGE, radius=4)    # แถบสถานะชิดขอบจอ (บังกล่องที่โผล่จากบนจอ)
game.Text("สายพานคัดแยกพัสดุ   SORTER", 14, 9, game.WHITE)
hud_state = game.Text("IDLE  หยุดรอ", 600, 9, game.WHITE)
warn = game.Text(" ", 18, 240, game.ORANGE)

gate = game.Box(GATE_X[0], GATE_Y, GATE_W, 12, COLORS[0], border=game.WHITE, radius=4, border_w=2)
chutes, chute_txt = [], []
for side in (0, 1):
    chutes.append(game.Box(CHUTE_X[side], 322, CHUTE_W, 46, LIT[side] if side == 0 else DIM[side],
                           border=COLORS[side], radius=6, border_w=3))
    chute_txt.append(game.Text("ช่อง %s   0" % "AB"[side], CHUTE_X[side] + 24, 334, game.WHITE))
boom = game.Sprite("boom1", 0, 0)
boom.hide()


def card(x, y, accent, title):
    # การ์ดค่า: แผงมืดขอบสี + ป้ายสองบรรทัด (บรรทัดบน = ชื่อ · บรรทัดล่าง = ค่า)
    game.Box(x, y, 230, 78, PANEL, border=accent, radius=8, border_w=2)
    return game.Text(title, x + 16, y + 15, game.WHITE)


cards = {"ok": card(14, 52, game.GREEN, "คัดถูก  OK"), "wrong": card(14, 142, game.RED, "คัดผิด  WRONG"),
         "left": card(548, 52, game.ORANGE, "ผิดได้อีก  LEFT"), "belt": card(548, 142, game.CYAN, "สายพาน  BELT")}

speed, gate_side = 0.0, 0
sorted_ok, wrong, rejects_left = 0, 0, REJECTS
count = [0, 0]                     # นับกล่องที่ลงแต่ละช่อง
boom_t, flash_t = 0, 0             # ตัวนับเฟรมของภาพระเบิด / ช่องกะพริบ (ไม่ใช้ sleep)
shown = {}


def show(key, widget, text):
    # อัปเดตป้ายเฉพาะตอนข้อความเปลี่ยน
    if shown.get(key) != text:
        widget.set(text)
        shown[key] = text


# ---- 5) วงวนหลัก ----
def on_frame():
    global speed, gate_side, sorted_ok, wrong, rejects_left, boom_t, flash_t
    keys = game.keys()
    stopped = rejects_left <= 0

    # รับปุ่ม (วัด)
    speed = 0.0 if stopped else belt_step(speed, keys.a, ACCEL, FRICTION, MAX_SPEED)
    side = 0 if keys.left else (1 if keys.right else gate_side)
    if side != gate_side and not stopped:
        gate_side = side
        gate.move_to(GATE_X[side], GATE_Y); gate.set_color(COLORS[side])
        chutes[side].set_color(LIT[side]); chutes[1 - side].set_color(DIM[1 - side])
        game.sfx("move")

    # คิด (ตัดสิน): กล่องทุกใบและแถบสายพานไหลด้วยความเร็วสายพานตัวเดียว แล้วเช็คว่าถึงประตูหรือยัง
    if speed > 0:
        for s in slats:
            y = s.y + speed
            s.move_to(s.x, y - 260 if y > BELT_END - 8 else y)
        for i, box in enumerate(boxes):
            box.y += speed                     # ขยับในตัวแปรก่อน แล้วส่งไปจอครั้งเดียวท้ายรอบ
            if game.hit(box, gate):
                count[gate_side] += 1
                if judge(box_color[i], gate_side):
                    sorted_ok += 1
                    chutes[gate_side].set_color(COLORS[gate_side])     # ช่องกะพริบสีตัวเอง = รับกล่องถูก
                    game.sfx("point")
                else:
                    wrong += 1
                    rejects_left -= 1
                    chutes[gate_side].set_color(game.ORANGE)           # ช่องกะพริบส้ม + ระเบิด = ลงผิด
                    boom.move_to(CHUTE_X[gate_side] + CHUTE_W - 50, 333)
                    boom.show()
                    boom_t = 3 * len(BOOM)
                    game.sfx("explode")
                flash_t = 6
                box_color[i] = random.randint(0, 1)
                box.set_color(COLORS[box_color[i]])
                box.y = next_y_above([b.y for b in boxes], GAP)
            box.move_to(box.x, box.y)

    # ภาพเคลื่อนไหวนับเฟรม: ระเบิดเปลี่ยนภาพทุก 3 เฟรม · ช่องกะพริบ 6 เฟรมแล้วกลับสีเดิม
    if boom_t > 0:
        boom_t -= 1
        if boom_t == 0:
            boom.hide()
            boom.frame(BOOM[0])                 # เตรียมภาพแรกไว้สำหรับรอบหน้า
        elif boom_t % 3 == 0:
            boom.frame(BOOM[len(BOOM) - boom_t // 3])
    if flash_t > 0:
        flash_t -= 1
        if flash_t == 0:
            chutes[gate_side].set_color(LIT[gate_side])

    # ออก (ทำ + โชว์)
    show("ok", cards["ok"], "คัดถูก  OK\n%d กล่อง" % sorted_ok)
    show("wrong", cards["wrong"], "คัดผิด  WRONG\n%d กล่อง" % wrong)
    show("left", cards["left"], "ผิดได้อีก  LEFT\n%d ครั้ง" % max(rejects_left, 0))
    show("belt", cards["belt"], "สายพาน  BELT\n%d.%d px ต่อเฟรม" % (int(speed), int(speed * 10) % 10))
    for side in (0, 1):
        show("c%d" % side, chute_txt[side], "ช่อง %s   %d" % ("AB"[side], count[side]))
    state = "STOPPED" if rejects_left <= 0 else ("RUNNING" if speed > 0 else "IDLE")
    if state != shown.get("state"):                     # เครื่องสถานะ: สีแถบบนบอกสถานะเครื่อง
        hud_state.set({"STOPPED": "STOPPED  หยุดเครื่อง", "RUNNING": "RUNNING  กำลังเดิน",
                       "IDLE": "IDLE  หยุดรอ"}[state])
        bar.set_color({"STOPPED": 0xB02020, "RUNNING": 0x1C5A34, "IDLE": PANEL}[state])
        shown["state"] = state
        if state == "STOPPED":
            game.sfx("gameover")
    if rejects_left <= 0:
        show("warn", warn, "หยุดสายพาน!\nคัดผิดครบ %d ครั้ง" % REJECTS)
    else:
        show("warn", warn, "ระวัง! ผิดอีก 1 ครั้ง\nสายพานจะหยุด" if rejects_left == 1 else " ")
    if stopped and boom_t == 0:                        # รอระเบิดเล่นจบก่อน แล้วจบโปรแกรม
        return False
    return True


game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) FRICTION = 0.96 -> 0.80 : ปล่อย A แล้วสายพานหยุดเร็วขึ้น — แบบไหนคุมง่ายกว่า
# 2) GAP = 78 -> 50 : กล่องมาถี่ขึ้น คัดทันไหม (ดูแถบสายพานวิ่งเท่าเดิม แต่กล่องแน่นขึ้น)
# 3) เพิ่มป้ายสีที่สาม (game.GREEN) และช่อง C — ต้องแก้ judge() กับ GATE_X / CHUTE_X ตรงไหนบ้าง
