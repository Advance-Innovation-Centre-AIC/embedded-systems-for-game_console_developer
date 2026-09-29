# shooter_step3.py — Shooter #2 (คาบ 11): เลเซอร์จาก "ชุดกระสุนหมุนเวียน" 6 นัด
# ------------------------------------------------------------------------------
# เทคนิคของคาบ: Object Pool (เอกสารไทยบางที่เขียน "พูล") — คาบนี้เรียกว่า "ชุดหมุนเวียน"
# สร้างกระสุน 6 นัดครั้งเดียวก่อนเกมเริ่ม แล้วยืม-คืนใช้ซ้ำ ไม่สร้างใหม่ใน on_frame
# (จอมีได้ไม่เกิน 32 ชิ้นรวมทุกอย่าง bentogame.py:179 — ชิ้นที่ 33 = RuntimeError: ui: max 32 widgets)
# ว่าง ⇔ bullet.y < -20 (จอดรอเหนือจอ)  ·  ใช้อยู่ ⇔ bullet.y >= -20 (กำลังบิน)
# hide() แค่ซ่อนให้มองไม่เห็น — สิ่งที่ทำให้ "ว่าง" คือค่า y ที่ต่ำกว่า -20
# 5 ขั้น: ① สร้างชุดครั้งเดียว ② หาตัวว่าง ③ ยืม ④ อัปเดตทุกเฟรม ⑤ คืน
# คำสั่งที่ใช้ (bentogame.py): Box :211 · move_to :234 · hide :262 · show :265 · sfx :73
# เกมเต็มให้ดูเทียบ: full_games/shooter_full.py (สร้าง :71-74 · ยืม fire() :137 · คืน reset_bullet() :115)
# ------------------------------------------------------------------------------
import bentogame as game

ACCEL, MAX_SPEED, FRICTION = 1.4, 13.0, 0.80
MAX_BULLETS = 6                                 # ขนาดชุดหมุนเวียน (เกมเต็มใช้ 4: shooter_full.py:30)

game.title("SHOOTER")                          # หน้าชื่อเกม: Start = เล่น · Back = ออก (เรียก start() ให้ในตัว)

ship = game.Box(365, 352, 62, 24, game.GREEN)
ship_x, ship_speed, fire_cooldown = 365.0, 0.0, 0
score, lives = 0, 3                             # ยังไม่ใช้ในคาบนี้ (คาบ 12 ใช้นับคะแนน/ชีวิต)
hud = game.Text("Score: 0   Lives: 3", 10, 8, game.WHITE)

# ----- ① สร้างชุดครั้งเดียว (ไฟล์ practise ให้มาแล้ว): 6 นัด จอดรอที่ y = -50 เหนือจอ -----
bullets = [game.Box(0, -50, 6, 14, game.CYAN) for _ in range(MAX_BULLETS)]
for bullet in bullets:
    bullet.hide()                               # ซ่อนไว้ก่อน (ที่ทำให้ว่างคือ y = -50 จาก Box ข้างบน)

# ----- ② หาตัวว่าง (ให้มาแล้ว): ไล่ดูทีละนัด นัดแรกที่ y < -20 คือตัวว่าง -----
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
    fire_cooldown = max(0, fire_cooldown - 1)
    if (keys.a or keys.up) and fire_cooldown == 0:
        bullet = find_free_bullet()                # ② หาตัวว่าง
        if bullet:                                 # ③ ยืม (ได้ตัวว่าง ไม่ใช่ None)
            bullet.show()
            bullet.move_to(ship_x + ship.w // 2 - 3, 340)   # ออกจากหัวยาน
            game.sfx("fire")                       # เสียงเลเซอร์ (bentogame.py:73)
            fire_cooldown = 8                      # คูลดาวน์ 8 เฟรม → ยิงได้ 30 ÷ 8 ≈ 3.75 นัด/วินาที

    # ----- งานของคุณ 2: ④ อัปเดตทุกเฟรม + ⑤ คืน -----
    for bullet in bullets:
        if bullet.y >= -20:                        # เฉพาะนัดที่ใช้อยู่
            bullet.move_to(bullet.x, bullet.y - 9) # ④ ขึ้น 9 px ต่อเฟรม
            if bullet.y < -20:                     # ⑤ พ้นเส้น -20 แล้ว = ว่าง (คืน)
                bullet.hide()                      #    ซ่อนให้ผู้เล่นมองไม่เห็นด้วย
    # -----------------------------------------------------------------------

game.run(on_frame, fps=30)
