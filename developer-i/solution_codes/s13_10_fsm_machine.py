# s13_10_fsm_machine.py — คาบ 13 ส่วนที่ 4: เครื่องสถานะของเครื่องซักผ้า (อยู่ในสถานะเดียวเสมอ)
#
# หลักการ   : เครื่องสถานะ (คาบ 2, 7, 12)   IDLE -> FILL -> WASH -> SPIN -> DONE -> IDLE
#             ทุกสถานะมีตัวจับเวลาของตัวเอง N = t x fps นับถอยหลังทีละเฟรม ถึง 0 แล้วย้ายไปสถานะถัดไป
#             A = เริ่ม (จับจังหวะกด pressed_once ครั้งเดียว) · B = หยุด กลับ IDLE ทันทีจากทุกสถานะ
# ลองเล่น   : กด Start · A = เริ่มซัก · B ตอนไหนก็ได้ = หยุดทันที · ทุกการย้ายสถานะมีเสียงไม่ซ้ำกัน
#             FILL น้ำขึ้นทีละแถบ · WASH ผ้าโยกไปมา · SPIN น้ำระบายออกแล้วผ้าหมุนเร็วขึ้นเรื่อย ๆ
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : หน้าเริ่ม -> เล่น -> GAME OVER -> เล่นใหม่ (สถานะแมตช์ของ Pong และ Shooter)
# ในงานจริง : หน้าจอเครื่องซักผ้า เครื่องบรรจุขวด เตาอบอุตสาหกรรม ทำงานเป็นขั้น ๆ ตามเวลา
#             ปุ่มหยุดฉุกเฉินทำทันทีไม่ต้องยืนยัน เพราะการหยุดปลอดภัยเสมอ · เวลาทุกขั้นเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : s02_button_fsm.py (จับจังหวะกด + ไฟจราจร) · s02_traffic_v2.py · pong_step5.py (สถานะแมตช์)
# งบวิดเจ็ต : 26 ชิ้น รวม game.run 1 (ตัวเครื่อง 1 + จอเครื่อง 1 + ประตู 1 + น้ำ 4 + ผ้า 3 + ป้ายใหญ่ 2
#             + ไฟ 5 + ป้ายไฟ 5 + ข้อความ 3 + game.run 1) · ไฟล์นี้ไม่ใช้ game_over() จึงไม่ต้องเผื่ออีก 4
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   4) หน้าจอ: สร้างครั้งเดียว   5) วงวนหลัก
import bentogame as game
import math

# ---- 1) ตั้งค่า (แก้ได้) ----
FPS = 30
IDLE, FILL, WASH, SPIN, DONE = 0, 1, 2, 3, 4
NAMES = ("IDLE", "FILL", "WASH", "SPIN", "DONE")
THAI = ("รอเริ่ม", "เติมน้ำ", "ซัก", "ปั่นหมาด", "เสร็จแล้ว")
SECONDS = (0, 3, 5, 4, 3)                     # อยู่ในแต่ละสถานะกี่วินาที (IDLE รอปุ่ม ไม่มีเวลา)
NEXT = (IDLE, WASH, SPIN, DONE, IDLE)         # หมดเวลาแล้วไปสถานะไหน
SOUNDS = ("back", "start", "select", "fire", "win")    # เสียงตอนเข้าแต่ละสถานะ (หยุดด้วย B = "deny")
BANNER = (0x3A4450, 0x1F5FAF, 0x0F7F8F, 0xB0601A, 0x2E8B57)   # สีป้ายใหญ่ (เข้มพอให้ตัวขาวอ่านง่าย)
LAMP = (0x8899AA, game.BLUE, game.CYAN, game.ORANGE, game.GREEN)
LAMP_OFF = 0x2A3440
CX, CY, R = 175, 200, 80                      # จุดกลางและรัศมีของประตูกลม
LR = 46                                       # รัศมีวงที่ผ้าวิ่ง (อยู่ในถังเสมอ)

# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ ----
def frames_for(state):
    return SECONDS[state] * FPS                # N = t x fps

def fsm_step(state, left, start, stop):
    # กฎทั้งหมดของเครื่องอยู่ที่นี่ที่เดียว คืน (สถานะใหม่, เฟรมที่เหลือ)
    if stop and state != IDLE:
        return IDLE, 0                         # หยุดไม่ถาม ไม่สนว่าอยู่สถานะไหน
    if state == IDLE:
        return (FILL, frames_for(FILL)) if start and not stop else (IDLE, 0)   # กดหยุดค้าง = เริ่มไม่ได้
    left -= 1                                  # นับเฟรมถอยหลัง (ไม่มี sleep)
    if left > 0:
        return state, left
    return NEXT[state], frames_for(NEXT[state])

def water_level(state, left):
    # จำนวนแถบน้ำ 0..4 : FILL ขึ้นทีละแถบ · WASH เต็ม · SPIN ระบายหมดในครึ่งแรก
    total = frames_for(state)
    if state == FILL: return min(4, (total - left) * 4 // total + 1)
    if state == WASH: return 4
    if state == SPIN: return max(0, 4 - (total - left) * 8 // total)
    return 0

# ---- 4) หน้าจอ: สร้างครั้งเดียว ----
game.title("WASHER FSM")
game.Box(50, 20, 250, 330, 0xD8DEE6, border=0x8899AA, border_w=2, radius=14)
game.Box(66, 34, 218, 40, 0x2A3440, radius=8)                  # จอเล็กของเครื่อง: เฟรมที่เหลือ
hud_time = game.Text("", 84, 44, game.GREEN)
game.Box(CX - R, CY - R, 2 * R, 2 * R, 0x22303C, border=0x8899AA, border_w=6, radius=R)
water = []
for i in range(4):                             # แถบล่างสุดคือแถบที่ 0 กว้างตามโค้งของถัง
    d = R - 14 - i * 18 - 9                    # ระยะจากจุดกลางถึงกลางแถบ
    w = int(2 * math.sqrt((R - 8) ** 2 - d * d))
    water.append(game.Box(CX - w // 2, CY + d - 9, w, 18, 0x3D8BD9, radius=6))
    water[i].hide()
clothes = [game.Box(0, 0, 16, 16, c, border=game.WHITE, radius=5)
           for c in (game.RED, game.YELLOW, game.PINK)]
banner = game.Box(330, 20, 440, 64, BANNER[IDLE], radius=10)
hud_state = game.Text("", 352, 40, game.WHITE)
lamps = []
for i in range(5):
    lamps.append(game.Box(344 + i * 88, 112, 30, 30, LAMP_OFF, border=0x556070, radius=15))
    game.Text(NAMES[i], 338 + i * 88, 150, game.WHITE)
hud_rule = game.Text("", 340, 200, game.WHITE)
game.Text("A = START (กดครั้งเดียว)    B = STOP ทันที", 340, 250, game.GB_LIGHT)

def place(cloth, angle, radius):               # วางผ้าบนวงกลมรอบจุดกลางถัง ที่มุม angle (เรเดียน)
    cloth.move_to(int(CX + radius * math.cos(angle)) - 8, int(CY + radius * math.sin(angle)) - 8)

def enter(new, old, by_stop):
    # ทำครั้งเดียวตอนย้ายสถานะ: ป้าย ไฟ เสียง (อัปเดตเฉพาะที่เปลี่ยน)
    banner.set_color(BANNER[new])
    hud_state.set("%s   %s" % (NAMES[new], THAI[new]))
    lamps[old].set_color(LAMP_OFF)
    lamps[new].set_color(LAMP[new])
    if new != old:                             # ตอนเปิดเครื่องครั้งแรกไม่มีเสียง
        game.sfx("deny" if by_stop else SOUNDS[new])
    if new in (IDLE, FILL, DONE):              # ผ้ากองนิ่งที่ก้นถัง
        for i, cloth in enumerate(clothes): place(cloth, math.pi / 2 + (i - 1) * 0.5, 52)
    if new == IDLE:
        hud_time.set("STOP" if by_stop else "READY")
        hud_rule.set("หยุดแล้ว (B)  กด A เริ่มใหม่" if by_stop else "กด A เพื่อเริ่มซัก")
    else:                                      # ตัวจับเวลาของสถานะนี้: N = t x fps
        hud_rule.set("%s = %d s x %d = %d f" % (NAMES[new], SECONDS[new], FPS, frames_for(new)))


state, left, frame, spin, omega, level = IDLE, 0, 0, 0.0, 0.0, 0
enter(IDLE, IDLE, False)

# ---- 5) วงวนหลัก ----
def on_frame():
    global state, left, frame, spin, omega, level
    keys = game.keys()
    frame += 1
    # รับปุ่ม (วัด) + คิด (ตัดสิน): A จับจังหวะกด · B ดูแค่ว่ากดอยู่ไหม (หยุดต้องไวที่สุด)
    start = game.pressed_once("a", keys)
    new, left = fsm_step(state, left, start, keys.b)
    if new != state:
        enter(new, state, keys.b and state != IDLE)
        state, spin, omega = new, 0.0, 0.0
    # ออก (โชว์): น้ำเปลี่ยนเฉพาะตอนจำนวนแถบเปลี่ยน · ผ้าขยับเฉพาะตอน WASH กับ SPIN
    lv = water_level(state, left)
    if lv != level:
        for i in range(4):
            if (i < lv) != (i < level):
                water[i].show() if i < lv else water[i].hide()
        level = lv
    if state == WASH:                          # โยกไปมาใต้ถัง (ซักแบบตีน้ำ)
        for i, cloth in enumerate(clothes):
            place(cloth, math.pi / 2 + 0.9 * math.sin(frame * 0.18 + i * 0.9), LR)
    elif state == SPIN:                        # หมุนรอบถัง เร่งขึ้นทีละนิดจนถึงเพดาน 0.5 เรเดียนต่อเฟรม
        omega = min(0.5, omega + 0.01)
        spin += omega
        for i, cloth in enumerate(clothes):
            place(cloth, spin + i * 2.094, LR)  # ห่างกัน 120 องศา
    if state != IDLE:
        hud_time.set("เหลือ %d f" % left)
    return True


game.run(on_frame, fps=FPS)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) SECONDS = (0, 3, 5, 4, 3) -> (0, 3, 15, 4, 3) : ซักนานขึ้น 3 เท่า ตัวเลขเฟรมของ WASH เริ่มที่ 450
# 2) NEXT = (IDLE, WASH, SPIN, DONE, IDLE) -> (IDLE, WASH, SPIN, WASH, IDLE) : ปั่นเสร็จวนกลับไปซักไม่จบ
#    เครื่องจริงที่ตารางสถานะผิดหนึ่งช่องก็ทำแบบนี้ — ตาราง NEXT คือแผนที่ของทั้งเครื่อง
# 3) ใน fsm_step ลบสองบรรทัดแรก (if stop ...) : กด B แล้วไม่มีอะไรเกิดขึ้น — ปุ่มหยุดต้องมาก่อนกฎอื่นเสมอ
