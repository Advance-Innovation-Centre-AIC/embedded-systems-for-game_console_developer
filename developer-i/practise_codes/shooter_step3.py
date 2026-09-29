# โครงเว้น 30% — ส่วนที่คุณต้องเติมเอง / เฉลย: solution_codes/shooter_step3.py
# ใบ้: สไลด์คาบ 11 หน้า "งานของเรา" และใบงานคาบ 11
# shooter_step3.py — Shooter #2 (คาบ 11): เลเซอร์จาก "ชุดกระสุนหมุนเวียน" 6 นัด
# ------------------------------------------------------------------------------
# เทคนิคของคาบ: Object Pool (เอกสารไทยบางที่เขียน "พูล") — คาบนี้เรียกว่า "ชุดหมุนเวียน"
# สร้างกระสุน 6 นัดครั้งเดียวก่อนเกมเริ่ม แล้วยืม-คืนใช้ซ้ำ ไม่สร้างใหม่ใน on_frame
# (จอมีได้ไม่เกิน 32 ชิ้นรวมทุกอย่าง bentogame.py:179 — ชิ้นที่ 33 = RuntimeError: ui: max 32 widgets)
# ว่าง ⇔ bullet.y < -20 (จอดรอเหนือจอ)  ·  ใช้อยู่ ⇔ bullet.y >= -20 (กำลังบิน)
# hide() แค่ซ่อนให้มองไม่เห็น — สิ่งที่ทำให้ "ว่าง" คือค่า y ที่ต่ำกว่า -20
# 5 ขั้น: ① สร้างชุดครั้งเดียว ② หาตัวว่าง ③ ยืม ④ อัปเดตทุกเฟรม ⑤ คืน
# ไฟล์นี้ให้ ① ② มาแล้ว · ที่ต้องเติม: "งานของคุณ 1" (คูลดาวน์ + ③) และ "งานของคุณ 2" (④ ⑤)
# คำสั่งที่ใช้ (bentogame.py): Box :211 · move_to :234 · hide :262 · show :265 · sfx :73
# ------------------------------------------------------------------------------
import bentogame as game

ACCEL, MAX_SPEED, FRICTION = 1.4, 13.0, 0.80
MAX_BULLETS = 6                                 # ขนาดชุดหมุนเวียน (เกมเต็มใช้ 4: shooter_full.py:30)

game.title("SHOOTER")                          # หน้าชื่อเกม: Start = เล่น · Back = ออก (เรียก start() ให้ในตัว)

ship = game.Box(365, 352, 62, 24, game.GREEN)
ship_x, ship_speed, fire_cooldown = 365.0, 0.0, 0
score, lives = 0, 3                             # ยังไม่ใช้ในคาบนี้ (คาบ 12 ใช้นับคะแนน/ชีวิต)
hud = game.Text("Score: 0   Lives: 3", 10, 8, game.WHITE)

# ① สร้างชุดครั้งเดียว (ให้มาแล้ว ห้ามย้ายเข้าไปใน on_frame): 6 นัด จอดรอที่ y = -50 เหนือจอ
bullets = [game.Box(0, -50, 6, 14, game.CYAN) for _ in range(MAX_BULLETS)]
for bullet in bullets:
    bullet.hide()                               # ซ่อนไว้ก่อน (ที่ทำให้ว่างคือ y = -50 จาก Box ข้างบน)

# ② หาตัวว่าง (ให้มาแล้ว): ไล่ดูทีละนัด นัดแรกที่ y < -20 คือตัวว่าง
def find_free_bullet():
    for bullet in bullets:
        if bullet.y < -20:
            return bullet                       # เจอแล้ว ส่งนัดนี้ออกไปใช้
    return None                                 # ใช้อยู่ครบ 6 → ไม่มีตัวว่าง เฟรมนี้ยิงไม่ออก

def on_frame():
    global ship_x, ship_speed, fire_cooldown
    keys = game.keys()
    # (Back = ออก · Start = พักเกม — game.run() จัดการให้ ดู bentogame.py:934-935)

    # ยาน (จาก step2 คาบ 10)
    if keys.left:    ship_speed -= ACCEL
    elif keys.right: ship_speed += ACCEL
    else:            ship_speed *= FRICTION
    ship_speed = max(-MAX_SPEED, min(MAX_SPEED, ship_speed))
    ship_x = max(0, min(game.WIDTH - ship.w, ship_x + ship_speed))
    ship.move_to(ship_x, 352)

    # ----- งานของคุณ 1: คูลดาวน์ + ③ ยืม -----
    # 1) ทุกเฟรมลด fire_cooldown ลงทีละ 1 แต่อย่าให้ต่ำกว่า 0 (ใช้ max(0, ...))
    # 2) เช็กว่ากดยิงไหม: กด A (keys.a) หรือดันขึ้น (keys.up) "และ" fire_cooldown == 0
    # 3) ถ้าใช่ -> ② หาตัวว่างด้วย find_free_bullet() ; ถ้าได้ (ไม่ใช่ None) -> ③ ยืม:
    #    3.1 โผล่กระสุนด้วย .show()
    #    3.2 ย้ายไปที่หัวยานด้วย .move_to(x, y) — x = กลางยานถอยครึ่งความกว้างกระสุน, y = เหนือยานเล็กน้อย
    #    3.3 เล่นเสียงยิงด้วย game.sfx("fire")
    #    3.4 ตั้ง fire_cooldown เป็นค่าหน่วง (มากไป = ยิงช้า, น้อยไป = ชุดหมุนเวียนหมด / ติดดู solution_codes/)
    pass   # <- ลบ pass ออกเมื่อเริ่มเขียน

    # ----- งานของคุณ 2: ④ อัปเดตทุกเฟรม + ⑤ คืน -----
    # 1) วนทุกกระสุน: for bullet in bullets:
    # 2) ④ ถ้ากระสุนยัง "ใช้อยู่" (bullet.y >= -20) ให้ขยับขึ้นด้วย .move_to(x, y)
    #    โดย x คงเดิม ส่วน y ลดลงทีละน้อยทุกเฟรม = พุ่งขึ้น (เลือกระยะเอง — มาก=เร็ว)
    # 3) ⑤ หลังขยับ ถ้าพ้นเส้นแล้ว (bullet.y < -20) ให้ .hide() = คืน (y ทำให้ว่าง ส่วน hide() แค่ซ่อน)
    pass   # <- ลบ pass ออกเมื่อเริ่มเขียน
    # -----------------------------------------------------------------------

game.run(on_frame, fps=30)
