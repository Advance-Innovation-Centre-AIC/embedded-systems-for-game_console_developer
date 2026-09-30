# โครงเว้นบางส่วน — ช่องที่คุณต้องเติมเอง / เฉลย: solution_codes/shooter_step4.py / ใบ้: สไลด์คาบ 12 + ใบงาน
# ------------------------------------------------------------------------------
# shooter_step4.py — Shooter #3 (คาบ 12): เติมศัตรู + ชนแบบกล่อง + คะแนน + ชีวิต จนเกมเล่นจบรอบได้
# กระสุนใช้ "ชุดหมุนเวียน" 6 นัดจากคาบ 11 (ให้มาแล้วด้านล่าง) / ศัตรู 6 ตัว "วนกลับขึ้นบน"
# ทุกครั้งที่หลุดพื้นหรือโดนยิง (แบบของตกใน Catch คาบ 4) — ศัตรูใช้อยู่ตลอด ไม่มีช่วงว่าง
# กติกา: ยิงโดน = Score +1 / หลุดพื้น = Lives -1 / Lives เหลือ 0 = GAME OVER
# สิ่งที่เพิ่งได้ใช้แบบหลายคู่: game.hit(a, b) ชนแบบกล่อง (bentogame.py:466) และ hud.set(...)
# ------------------------------------------------------------------------------
import bentogame as game
import random

ACCEL, MAX_SPEED, FRICTION = 1.4, 13.0, 0.80
MAX_BULLETS, MAX_ENEMIES = 6, 6                 # ชุดกระสุน 6 นัด, ศัตรู 6 ตัว
ENEMY_COLORS = [game.RED, game.ORANGE, game.PINK]

game.title("SHOOTER")                          # หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว)

ship = game.Box(365, 352, 62, 24, game.GREEN)
ship_x, ship_speed = 365.0, 0.0
score, lives, fire_cooldown = 0, 3, 0
hud = game.Text("Score: 0   Lives: 3", 10, 8, game.WHITE)

bullets = [game.Box(0, -50, 6, 14, game.CYAN) for _ in range(MAX_BULLETS)]
for bullet in bullets:
    bullet.hide()

# ----- เติมส่วนนี้เอง (งานของคุณ) (1): สร้างศัตรู 6 ตัวครั้งเดียว ให้ตกจากเหนือจอด้วยความเร็วสุ่ม -----
# TODO: สร้าง list ศัตรูแบบ list comprehension วน range(MAX_ENEMIES) ใช้ game.Box (bentogame.py:211)
#   1) ตำแหน่ง x สุ่มให้อยู่ในจอ (ใช้ random.randint, ระวังอย่าให้เลยขอบขวา game.WIDTH)
#   2) ตำแหน่ง y ให้เป็นค่าติดลบ (เริ่มเหนือจอ) เพื่อให้ศัตรูทยอยร่วงลงมาไม่พร้อมกัน
#   3) สีของแต่ละตัวสุ่มด้วย random.choice(ENEMY_COLORS)
#   4) แล้วสร้าง enemy_speed เป็น list ความเร็วสุ่มต่อตัวด้วย random.uniform(...) — ช้าพอให้ยิงทัน
#   (ลองปรับช่วงค่าเอง / เปิด solution_codes/shooter_step4.py ถ้าติดจริง ๆ)
enemies = [game.Box(0, -50, 30, 24, game.RED) for _ in range(MAX_ENEMIES)]   # <- placeholder: แก้ให้สุ่มตำแหน่ง/สี
enemy_speed = [0.0 for _ in range(MAX_ENEMIES)]                              # <- placeholder: แก้ให้สุ่มความเร็ว

def find_free_bullet():
    for bullet in bullets:
        if bullet.y < -20:
            return bullet
    return None

# ----- เติมส่วนนี้เอง (งานของคุณ) (2): ศัตรูวนกลับขึ้นบน (เริ่มรอบใหม่เหนือจอ) -----
# TODO: เติมตัวฟังก์ชันให้ศัตรูตัวที่ index "วนกลับขึ้นบน" (ย้ายกล่องเดิม ไม่สร้างใหม่)
#   1) ใช้ enemies[index].move_to(...) (Box.move_to — bentogame.py:234) ย้ายไป x สุ่มในจอ,
#      y ติดลบ (เหนือจอ) ด้วย random.randint (ช่วงค่าดูตาราง "ชิ้นส่วนสุ่ม" ในสไลด์คาบ 12)
#   2) เปลี่ยนสีใหม่ด้วย enemies[index].set_color(random.choice(ENEMY_COLORS))  (set_color — bentogame.py:259)
def respawn_enemy(index):
    pass   # <- ลบ pass ออกเมื่อเริ่มเขียน

def on_frame():
    global ship_x, ship_speed, score, lives, fire_cooldown
    keys = game.keys()
    # (Back = ออก / Start = พักเกม — game.run() จัดการให้ bentogame.py:970-974)

    # ยาน (step2)
    if keys.left:    ship_speed -= ACCEL
    elif keys.right: ship_speed += ACCEL
    else:            ship_speed *= FRICTION
    ship_speed = max(-MAX_SPEED, min(MAX_SPEED, ship_speed))
    ship_x = max(0, min(game.WIDTH - ship.w, ship_x + ship_speed))
    ship.move_to(ship_x, 352)

    # ยิง (step3)
    fire_cooldown = max(0, fire_cooldown - 1)
    if (keys.a or keys.up) and fire_cooldown == 0:
        bullet = find_free_bullet()
        if bullet:
            bullet.show(); bullet.move_to(ship_x + ship.w // 2 - 3, 340); game.sfx("fire"); fire_cooldown = 8

    for bullet in bullets:
        if bullet.y >= -20:
            bullet.move_to(bullet.x, bullet.y - 9)
            if bullet.y < -20: bullet.hide()

    # ----- เติมส่วนนี้เอง (งานของคุณ) (3+4): ก ตก / ข หลุดพื้น / ค ชีวิตหมด / ง วนกระสุน / จ โดน -----
    # TODO: วนศัตรูทุกตัวด้วย enumerate(enemies) เพื่อได้ทั้ง index และ enemy
    #   ก) ให้ศัตรูตกลงทีละเฟรม: เลื่อน enemy ด้วย move_to ลงตามค่า enemy_speed[index] ของมัน
    #   ข) หลุดพื้น? เทียบ enemy.y กับ game.HEIGHT — ถ้าหลุด: ลด lives, อัปเดต HUD ด้วย
    #      hud.set(...) (Text.set — bentogame.py:286), เรียก respawn_enemy(index) แล้ว continue ไปตัวถัดไป
    #   ค) ถ้า lives หมด (<= 0): เล่น game.sfx("gameover") (bentogame.py:73), วาด game.Text("GAME OVER", ...)
    #      (Text — bentogame.py:280) แล้ว return False เพื่อจบเกม
    #   ง) ถ้ายังไม่หลุด: วนกระสุนทุกนัด เฉพาะนัดที่ใช้อยู่ (bullet.y >= -20) แล้วเช็ค game.hit(bullet, enemy) (bentogame.py:466)
    #   จ) ถ้าโดน: score += 1, game.sfx("hit"), อัปเดต HUD ด้วย hud.set(...) (อย่าลืมตรงนี้!),
    #      คืนกระสุน (move_to ไปเหนือจอ เช่น (0, -50) + hide()), respawn_enemy(index), แล้ว break ออกจาก loop กระสุน
    pass   # <- ลบ pass ออกเมื่อเริ่มเขียน
    # -----------------------------------------------------------------------

game.run(on_frame, fps=30)
