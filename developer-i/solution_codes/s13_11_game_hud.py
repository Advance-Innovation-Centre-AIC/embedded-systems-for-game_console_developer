# s13_11_game_hud.py — คาบ 13 (คาบพิเศษ): HUD ในเกม = ตอบ "ตอนนี้ฉันเป็นยังไง" ภายในครึ่งวินาที
#
# หลักการ   : กฎ HUD 3 ข้อ (สไลด์ส่วนที่ 5)
#             1) ตัวเลขสำคัญอยู่มุมเดิมเสมอ   คะแนนซ้ายบน · เวลากลางบน · ชีวิต (ยาน) ขวาบน
#             2) อัปเดตเฉพาะตอนค่าเปลี่ยน      จำข้อความล่าสุดไว้ ต่างจากเดิมค่อย set()   (คาบ 4, 12)
#             3) ข่าวใหญ่อยู่กลางจอ ตัวใหญ่     READY ตอนเริ่ม · GAME OVER ตอนจบ   (เครื่องสถานะ คาบ 2, 7)
#             เวลาคือการนับเฟรม  วินาทีที่เหลือ = (เฟรมทั้งหมด - เฟรมที่เล่นไป) / fps ปัดขึ้น  (คาบ 4, 11)
# ลองเล่น   : กด Start · รอป้าย READY หายไป · จอยซ้าย/ขวา = เลื่อนตะกร้ารับผลไม้
#             รับได้ = คะแนนซ้ายบนขึ้น · พลาด = ยานขวาบนหายไปหนึ่งลำ · 5 วินาทีสุดท้ายมีเสียงนับ
#             ชีวิตหมดหรือเวลาหมด = GAME OVER กลางจอ · กด A เล่นใหม่
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : แถบคะแนน ชีวิต เวลา ของทุกเกม (shooter_step4.py ใช้ป้าย Score / Lives มุมซ้ายบน)
# ในงานจริง : แถบสถานะของหน้าจอควบคุม ยอดผลิตอยู่มุมเดิม · เวลากะที่เหลืออยู่กลาง
#             เหตุใหญ่ขึ้นกลางจอ · ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : s04_catch.py (ตะกร้ารับของ) · shooter_step4.py (HUD คะแนน + ชีวิต)
# งบวิดเจ็ต : 17 ชิ้น (ผลไม้ 3 + แถบ HUD 1 + พื้น 1 + ตะกร้า 1 + ยานบอกชีวิต 3 + ข้อความ HUD 2
#                    + ป้ายกลางจอ 1 + ข้อความในป้าย 4 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game
import random

# ---- 1) ตั้งค่า (แก้ได้) ----
FPS = 30
GAME_SECONDS = 30   # เล่นรอบละกี่วินาที
LIVES = 3           # ชีวิต = จำนวนยานมุมขวาบน
READY_FRAMES = 90   # ป้าย READY ค้าง 90 เฟรม = 3 วินาที
FRUITS = 3          # ผลไม้ที่ตกพร้อมกัน (ชุดหมุนเวียน สร้างครั้งเดียว)
FALL_MIN, FALL_MAX = 18, 30        # ความเร็วตก หน่วย 0.1 px ต่อเฟรม (1.8 ถึง 3.0)
BASKET_W, BASKET_SPEED = 110, 9
GROUND_Y = 356      # พื้น (ผลไม้ตกถึงตรงนี้ = พลาด)
SHIP_W, FRUIT = 62, 16             # ขนาดภาพ ship กับ snake_food (ภาพไม่รู้ขนาดตัวเอง)
TOTAL = GAME_SECONDS * FPS

# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ (ทดสอบได้โดยไม่ต้องมีบอร์ด) ----
def seconds_left(frame, total, fps):
    # ปัดขึ้น: เฟรมแรกยังเห็น 30 ไม่ใช่ 29 และเห็น 0 ตอนหมดเวลาจริงเท่านั้น
    return max(0, (total - frame + fps - 1) // fps)

def next_phase(phase, ready_count, lives, secs):
    # เครื่องสถานะของรอบเกม  ready -> play -> over
    if phase == "ready" and ready_count <= 0:
        return "play"
    if phase == "play" and (lives <= 0 or secs <= 0):
        return "over"
    return phase

def centre_x(msg):
    # ตัวอักษรกว้างราว 10 px  กลางจอ = กึ่งกลางจอ - ครึ่งความยาวข้อความ
    return game.WIDTH // 2 - len(msg) * 5

# ---- 3) ตรวจสมองก่อนเปิดจอ ----
def self_test():
    cases = [("seconds_left", seconds_left(0, 900, 30), 30), ("seconds_left", seconds_left(29, 900, 30), 30),
             ("seconds_left", seconds_left(899, 900, 30), 1), ("seconds_left", seconds_left(900, 900, 30), 0),
             ("next_phase", next_phase("ready", 0, 3, 30), "play"), ("next_phase", next_phase("play", 0, 0, 12), "over"),
             ("next_phase", next_phase("play", 0, 2, 0), "over"), ("next_phase", next_phase("play", 0, 2, 5), "play")]
    for name, got, want in cases:
        if got != want:
            print("ยังไม่ผ่าน:", name, "ได้", got, "ควรได้", want)
            return False
    print("ผ่าน! สมองของ HUD ถูกทั้ง", len(cases), "กรณี")
    return True

if not self_test():
    raise SystemExit

# ---- 4) หน้าจอ: สร้างครั้งเดียว (ลำดับการวาด = ลำดับการสร้าง) ----
game.title("GAME HUD")
fruits = game.pool("snake_food", FRUITS)                      # สร้างก่อนแถบ HUD = ผลไม้โผล่จากใต้แถบ
for f in fruits:
    f.w, f.h = FRUIT, FRUIT
game.Box(0, 0, game.WIDTH, 40, 0x1E2A36)                      # แถบ HUD: บ้านประจำของตัวเลข
game.Box(0, GROUND_Y, game.WIDTH, 8, game.GB_DARK)            # พื้น
basket = game.Box(340, GROUND_Y - 18, BASKET_W, 14, game.ORANGE, border=game.WHITE, radius=4)
ships = [game.Sprite("ship", game.WIDTH - 12 - (LIVES - i) * (SHIP_W + 6), 6) for i in range(LIVES)]
hud_score = game.Text("", 12, 11, game.WHITE)                 # กฎ 1: ซ้ายบนเสมอ
hud_time = game.Text("", centre_x("TIME 30"), 11, game.YELLOW)  # กฎ 1: กลางบนเสมอ
banner = game.Box(236, 150, 320, 90, 0x10122C, border=game.YELLOW, radius=8, border_w=3)

def line(msg, y, color):
    return (game.Text("", centre_x(msg), y, color), msg)      # สร้างว่างไว้ จะโชว์ค่อย set

ready_lines = [line("READY", 172, game.YELLOW), line("catch the fruit", 204, game.WHITE)]
over_lines = [line("GAME OVER", 172, game.RED), line("A = play again", 204, game.WHITE)]

def show_banner(lines, on):
    # กฎ 3: ข่าวใหญ่ขึ้นกลางจอในกรอบเด่น แล้วหายไปเมื่อหมดข่าว
    if on: banner.show()
    else:  banner.hide()
    for text, msg in lines:
        text.set(msg if on else "")

st = {"phase": "ready", "count": 0, "frame": 0, "score": 0, "lives": 0}
shown = {"score": None, "time": None}                         # กฎ 2: จำค่าที่โชว์ล่าสุด
fall = [0.0] * FRUITS

def drop(i):
    fruits[i].move_to(random.randint(20, game.WIDTH - 36), -random.randint(20, 400))
    fall[i] = random.randint(FALL_MIN, FALL_MAX) / 10

def new_round():
    st.update({"phase": "ready", "count": READY_FRAMES, "frame": 0, "score": 0, "lives": LIVES})
    for s in ships: s.show()
    for i in range(FRUITS):
        drop(i)
        fruits[i].show()
    show_banner(over_lines, False)
    show_banner(ready_lines, True)

new_round()

# ---- 5) วงวนหลัก ----
def on_frame():
    k = game.keys()
    if st["phase"] == "over":                                 # จบแล้ว รอ A อย่างเดียว
        if game.pressed_once("a", k): new_round()
        return True
    if k.left:  basket.move(-BASKET_SPEED, 0)
    if k.right: basket.move(BASKET_SPEED, 0)
    if st["phase"] == "ready":
        st["count"] -= 1
    else:
        st["frame"] += 1
        for i in range(FRUITS):
            f = fruits[i]
            f.move_to(f.x, f.y + fall[i])
            if game.hit(f, basket):
                st["score"] += 1
                game.sfx("eat")
                drop(i)
            elif f.y > GROUND_Y - FRUIT:
                if st["lives"] > 0:
                    st["lives"] -= 1
                    ships[st["lives"]].hide()                 # ชีวิตหาย = ยานหายหนึ่งลำ ที่มุมเดิม
                    game.sfx("lose_life")
                drop(i)

    secs = seconds_left(st["frame"], TOTAL, FPS)
    if st["score"] != shown["score"]:                         # กฎ 2: เปลี่ยนเมื่อไรค่อยเขียน
        hud_score.set("SCORE %d" % st["score"])
        shown["score"] = st["score"]
    if secs != shown["time"]:                                 # วินาทีละครั้ง ไม่ใช่ทุกเฟรม
        hud_time.set("TIME %d" % secs)
        shown["time"] = secs
        if st["phase"] == "play" and 0 < secs <= 5:
            game.sfx("move")                                  # 5 วินาทีสุดท้าย นับด้วยเสียง

    phase = next_phase(st["phase"], st["count"], st["lives"], secs)
    if phase != st["phase"]:
        st["phase"] = phase
        if phase == "play":
            show_banner(ready_lines, False)
            game.sfx("select")
        else:
            show_banner(over_lines, True)
            game.sfx("gameover")
    return True

game.run(on_frame, fps=FPS)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) ย้าย hud_score.set(...) ออกมานอก if ให้เขียนทุกเฟรม : ตัวเลขเหมือนเดิม แต่บอร์ดต้องส่งข้อความวาดจอ
#    เพิ่ม 30 ครั้งต่อวินาทีโดยไม่จำเป็น (งบจริงราว 10-15 ข้อความต่อเฟรม) เกมเริ่มหน่วงบนบอร์ด
# 2) LIVES = 3 -> 5 : ยานมุมขวาบนเพิ่มเป็น 5 ลำเอง เพราะตำแหน่งคิดจากสูตร ไม่ได้พิมพ์ทีละลำ
# 3) READY_FRAMES = 90 -> 20 : ป้าย READY แวบเดียว ผู้เล่นตั้งตัวไม่ทัน ข่าวใหญ่ต้องค้างนานพอให้อ่านทัน
