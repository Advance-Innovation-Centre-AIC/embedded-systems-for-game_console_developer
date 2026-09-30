# shooter_step4.py — Shooter #3 (คาบ 12): ศัตรู + ชนแบบกล่อง + คะแนน + ชีวิต = เกมที่เล่นจบรอบได้
# ------------------------------------------------------------------------------
# step นี้คือตอนที่เกมเล่นจบได้จริง: ศัตรูตกจากด้านบน ยิงโดน = ได้คะแนน, หลุดถึงพื้น = เสียชีวิต,
# ชีวิตหมด = GAME OVER. กระสุนยังเป็น "ชุดหมุนเวียน" (Object Pool) 6 นัดจากคาบ 11 เหมือนเดิม
# ศัตรู 6 ตัวใน step นี้ "วนกลับขึ้นบน" (respawn) ทุกครั้งที่หลุดพื้นหรือโดนยิง — ใช้อยู่ตลอด ไม่มีช่วงว่าง
#   (แบบเดียวกับของตกใน Catch คาบ 4) / ชุดศัตรูหมุนเวียนตัวจริงที่มีสถานะว่าง-ใช้อยู่ อยู่ใน step5
# สิ่งที่ engine ให้มาและได้ใช้แบบหลายคู่ครั้งแรก: game.hit(a, b) ชนแบบกล่อง (bentogame.py:466)
#   แล้วอัปเดตป้าย HUD ด้วย hud.set(...) (Text.set — bentogame.py:286) ทุกครั้งที่ตัวเลขเปลี่ยน
# เกมเต็มสำหรับเทียบ: full_games/shooter_full.py (ปล่อยศัตรู :123-135, ลูปศัตรู + ชน :216-259)
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

# ----- เติมส่วนนี้เอง (1): ศัตรู 6 ตัว สร้างครั้งเดียว เริ่มเหนือจอ ความเร็วสุ่ม -----
enemies = [game.Box(random.randint(0, game.WIDTH - 30), -random.randint(60, 700),
                    30, 24, random.choice(ENEMY_COLORS)) for _ in range(MAX_ENEMIES)]
enemy_speed = [random.uniform(1.5, 2.5) for _ in range(MAX_ENEMIES)]   # ช้าพอให้ยิงทัน

def find_free_bullet():
    for bullet in bullets:
        if bullet.y < -20:
            return bullet
    return None

# ----- เติมส่วนนี้เอง (2): ศัตรูวนกลับขึ้นบน (x สุ่ม, y เหนือจอ, สีสุ่ม) -----
def respawn_enemy(index):
    enemies[index].move_to(random.randint(0, game.WIDTH - 30), -random.randint(100, 500))
    enemies[index].set_color(random.choice(ENEMY_COLORS))

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

    # ----- เติมส่วนนี้เอง (3+4): ก ตก / ข หลุดพื้น / ค ชีวิตหมด / ง วนกระสุน / จ โดน -----
    for index, enemy in enumerate(enemies):
        enemy.move_to(enemy.x, enemy.y + enemy_speed[index])   # ก ตกลงหนึ่งก้าว
        if enemy.y > game.HEIGHT:              # ข หลุดพื้น = เสีย 1 ชีวิต
            lives -= 1
            hud.set("Score: %d   Lives: %d" % (score, lives))
            respawn_enemy(index)
            if lives <= 0:                   # ค ชีวิตหมด = จบเกม
                game.sfx("gameover")
                game.Text("GAME OVER", 320, 180, game.RED)
                return False
            continue
        for bullet in bullets:                 # ง วนกระสุนทุกนัด
            if bullet.y >= -20 and game.hit(bullet, enemy):   # จ ใช้อยู่ และโดน?
                score += 1
                game.sfx("hit")
                hud.set("Score: %d   Lives: %d" % (score, lives))
                bullet.move_to(0, -50); bullet.hide()    # คืนกระสุน (จอดเหนือจอ = ว่าง)
                respawn_enemy(index)            # ศัตรูวนกลับขึ้นบน
                break                                   # นัดนี้จบศัตรูตัวนี้แล้ว
    # -----------------------------------------------------------------------

game.run(on_frame, fps=30)
