# s13_15_game_juice.py — คาบ 13 (คาบพิเศษ): ความสนุกที่มาจากคณิตศาสตร์ที่เรียนแล้ว (game juice)
#
# หลักการ   : ลูกเล่นทุกอย่างในไฟล์นี้ใช้แค่สมการที่เรียนแล้ว ไม่มีของใหม่เลย
#             ยิ่งเก่งยิ่งยาก   speed = min(BASE + score // 5, MAX)   ทุก 5 แต้มเร็วขึ้น 1 แต่มีเพดาน  (คาบ 3)
#             โดนแล้วกะพริบ    set_color(ขาว) แล้วนับถอยหลัง c = max(0, c - 1) ครบ 6 เฟรมคืนสีเดิม  (คาบ 11)
#             ระเบิดเป็นภาพ    ย้าย boom ไปจุดที่โดน แสดง 10 เฟรม ครึ่งแรก boom1 ครึ่งหลัง boom2  (คาบ 4)
#             บอสมีแถบพลัง     กล่องเล็ก 5 ก้อน โดนหนึ่งครั้งซ่อนหนึ่งก้อน ไม่ย่อขยายกล่อง  (ตัวนับ คาบ 12)
# ลองเล่น   : กด Start · จอยซ้าย/ขวา = ขับยาน · กด A ค้าง = ยิงต่อเนื่อง (มีคูลดาวน์)
#             ยิงศัตรูที่ตกลงมา +1 แต้ม · ยิงบอส (กล่องม่วงด้านบน) ดูบอสกะพริบขาวและแถบพลังขวาบนหายทีละก้อน
#             บอสพลังหมด +5 แต้ม · ดูบรรทัด SPEED กลางบน ตัวเลขขึ้นทุก 5 แต้ม แล้วหยุดที่เพดาน
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : Shooter ตัวเต็ม (ระเบิด 2 เฟรม · คูลดาวน์การยิง · ความยากเพิ่มตามคะแนน)
# ในงานจริง : ผลตอบกลับที่ทันทีและชัดใช้กับหน้าจอควบคุมด้วย กดปุ่มแล้วปุ่มกะพริบตอบ · ค่าที่เพิ่มตามเวลาต้องมีเพดาน
#             แถบความจุถังหรือแบตเตอรี่แบบเป็นก้อน ซ่อนทีละก้อน · ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : shooter_step3.py (คูลดาวน์) · shooter_full.py (ระเบิด 2 เฟรม) · s03_box_joystick.py (min กับเพดาน)
# งบวิดเจ็ต : 21 ชิ้น (อวกาศ 1 + ศัตรู 3 + บอส 2 + กระสุน 3 + ยาน 1 + ระเบิด 2 + แถบพลัง 5
#                    + ข้อความ 3 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game
import random

# ---- 1) ตั้งค่า (แก้ได้) ----
BASE_SPEED, MAX_SPEED = 2, 6   # ความเร็วศัตรูตอนเริ่ม และเพดาน (px ต่อเฟรม)
FLASH_FRAMES = 6               # บอสกะพริบขาวกี่เฟรม
BOOM_FRAMES = 10               # ระเบิดค้างกี่เฟรม
BOSS_HP = 5                    # แถบพลังบอส 5 ก้อน
BOSS_RESPAWN = 60              # บอสตัวใหม่มาหลังตัวเก่าแตก 60 เฟรม (2 วินาที)
ENEMIES, BULLETS, BOOMS = 3, 3, 2   # ขนาดชุดหมุนเวียน: ของที่ขยับพร้อมกันน้อย = ลื่นบนบอร์ด
FIRE_COOLDOWN = 6
BULLET_SPEED = 10
SHIP_SPEED = 8
SHIP_Y, BOSS_Y = 330, 60
BOSS_W, BOSS_H = 110, 34
BOSS_COLOR = 0x7B4FD6
ENEMY = ("enemy", "enemy2", "enemy3")
SIZE = ((16, 14), (21, 10), (18, 14))

# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ (ทดสอบได้โดยไม่ต้องมีบอร์ด) ----
def ramp(score):
    return min(BASE_SPEED + score // 5, MAX_SPEED)    # // = หารปัดเศษทิ้ง 0-4 แต้ม = +0, 5-9 แต้ม = +1

def countdown(c):
    return max(0, c - 1)                              # นับถอยหลังแล้วหยุดที่ 0 ไม่ติดลบ

def boom_frame(t):
    return "boom1" if t > BOOM_FRAMES // 2 else "boom2"   # ครึ่งแรกลูกไฟเล็ก ครึ่งหลังวงใหญ่

# ---- 3) ตรวจสมองก่อนเปิดจอ ----
def self_test():
    for got, want in ((ramp(0), 2), (ramp(4), 2), (ramp(5), 3), (ramp(19), 5), (ramp(999), 6),
                      (countdown(3), 2), (countdown(0), 0), (boom_frame(10), "boom1"), (boom_frame(5), "boom2")):
        if got != want:
            print("ยังไม่ผ่าน: ได้", got, "ควรได้", want)
            return False
    print("ผ่าน! สมองของลูกเล่นถูกทุกกรณี")
    return True

if not self_test():
    raise SystemExit

# ---- 4) หน้าจอ: สร้างครั้งเดียว (พื้นหลังก่อน ระเบิดกับ HUD ทีหลัง จะได้อยู่บนสุด) ----
game.title("GAME JUICE")
game.Box(0, 0, game.WIDTH, game.HEIGHT, 0x10122C)
enemies = game.pool("enemy", ENEMIES)
boss = game.Box(340, BOSS_Y, BOSS_W, BOSS_H, BOSS_COLOR, border=game.WHITE, radius=8, border_w=2)
boss_face = game.Sprite("enemy3", 0, 0)
bullets = [game.Box(-20, -50, 5, 12, 0x80F4FF, border=game.WHITE, radius=2) for _ in range(BULLETS)]
ship = game.Sprite("ship", 365, SHIP_Y)
booms = game.pool("boom1", BOOMS)
hud_score = game.Text("", 12, 8, game.WHITE)
hud_ramp = game.Text("", 230, 8, game.YELLOW)
game.Text("BOSS", 560, 8, game.WHITE)
hp_boxes = [game.Box(620 + i * 32, 10, 26, 16, game.RED, border=game.WHITE, radius=3) for i in range(BOSS_HP)]

st = {"score": 0, "cool": 0, "flash": 0, "hp": BOSS_HP, "away": 0, "bvx": 3, "shown": None}
boom_t = [0] * BOOMS

def drop(i):
    kind = random.randint(0, 2)
    enemies[i].frame(ENEMY[kind])
    enemies[i].w, enemies[i].h = SIZE[kind]
    enemies[i].move_to(random.randint(20, game.WIDTH - 40), -random.randint(20, 220))
    enemies[i].show()

def boom_at(cx, cy):
    b = boom_t.index(min(boom_t))                   # ใช้ลูกที่ว่าง หรือใกล้หมดเวลาที่สุด
    boom_t[b] = BOOM_FRAMES
    booms[b].frame("boom1")
    booms[b].move_to(cx - 14, cy - 12)              # ภาพระเบิด 29x24 จัดกลางที่จุดโดน
    booms[b].show()

def hit_boss(x):
    boom_at(x, BOSS_Y + BOSS_H)                     # ระเบิดเล็กตรงจุดที่กระสุนกระทบใต้ท้องบอส
    st["hp"] -= 1
    hp_boxes[st["hp"]].hide()                       # แถบพลังหายทีละก้อน ไม่ resize
    boss.set_color(game.WHITE)                      # กะพริบ: เปลี่ยนสีทันที แล้วนับถอยหลัง
    st["flash"] = FLASH_FRAMES
    game.sfx("hit")
    if st["hp"] == 0:                               # บอสแตก
        boss.hide()
        boss_face.hide()
        boom_at(boss.x + BOSS_W // 2, BOSS_Y + BOSS_H // 2)
        st["score"] += 5
        st["away"] = BOSS_RESPAWN
        game.sfx("explode")

for i in range(ENEMIES): drop(i)

# ---- 5) วงวนหลัก ----
def on_frame():
    k = game.keys()
    if k.left or k.right:
        ship.move_to(max(0, min(game.WIDTH - 62, ship.x + (k.right - k.left) * SHIP_SPEED)), SHIP_Y)
    st["cool"] = countdown(st["cool"])
    if k.a and st["cool"] == 0:
        for b in bullets:
            if b.y < 0:
                b.move_to(ship.x + 28, SHIP_Y - 12)
                st["cool"] = FIRE_COOLDOWN
                game.sfx("fire")
                break

    if st["away"] == 0:                             # บอสวิ่งซ้ายขวา ชนขอบแล้วสะท้อน
        if boss.x < 20 or boss.x > game.WIDTH - 20 - BOSS_W:
            st["bvx"] = -st["bvx"]
        boss.move_to(boss.x + st["bvx"], BOSS_Y)
        boss_face.move_to(boss.x + BOSS_W // 2 - 9, BOSS_Y + 10)
    else:
        st["away"] = countdown(st["away"])
        if st["away"] == 0:                         # บอสตัวใหม่ พลังเต็ม
            st["hp"] = BOSS_HP
            for h in hp_boxes: h.show()
            boss.show()
            boss_face.show()

    speed = ramp(st["score"])                       # ศัตรูตกเร็วตามคะแนน (มีเพดาน)
    for i in range(ENEMIES):
        enemies[i].move_to(enemies[i].x, enemies[i].y + speed)
        if enemies[i].y > game.HEIGHT: drop(i)       # หลุดล่างจอ = วนกลับขึ้นบน (ชุดหมุนเวียน)

    for b in bullets:
        if b.y < 0: continue
        b.move_to(b.x, b.y - BULLET_SPEED)
        used = b.y < 30
        for i in range(ENEMIES):
            if not used and game.hit(b, enemies[i]):
                boom_at(enemies[i].x + enemies[i].w // 2, enemies[i].y + enemies[i].h // 2)
                st["score"] += 1
                game.sfx("hit")
                drop(i)
                used = True
        if not used and st["away"] == 0 and game.hit(b, boss):
            hit_boss(b.x + 2)
            used = True
        if used: b.move_to(-20, -50)

    if st["flash"] > 0:                             # นับถอยหลังกะพริบ ครบแล้วคืนสีเดิม
        st["flash"] = countdown(st["flash"])
        if st["flash"] == 0: boss.set_color(BOSS_COLOR)
    for j in range(BOOMS):                          # ระเบิด: ครึ่งแรก boom1 ครึ่งหลัง boom2 แล้วซ่อน
        if boom_t[j] > 0:
            boom_t[j] -= 1
            if boom_t[j] == BOOM_FRAMES // 2: booms[j].frame(boom_frame(boom_t[j]))
            if boom_t[j] == 0: booms[j].hide()

    if st["score"] != st["shown"]:                  # HUD เขียนเฉพาะตอนคะแนนเปลี่ยน
        hud_score.set("SCORE %d" % st["score"])
        hud_ramp.set("SPEED = min(%d + %d // 5, %d) = %d" % (BASE_SPEED, st["score"], MAX_SPEED, ramp(st["score"])))
        st["shown"] = st["score"]
    return True

game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) MAX_SPEED = 6 -> 99 : ไม่มีเพดาน ยิ่งเก่งยิ่งเร็วไม่หยุด จนศัตรูก้าวต่อเฟรมยาวกว่าตัวกระสุนกับตัวมันรวมกัน
#    บางนัดจึงทะลุผ่านไปเฉย ๆ ทั้งที่เล็งตรง — เพดานคือสิ่งที่ทำให้เกมยังเล่นได้
# 2) FLASH_FRAMES = 6 -> 1 : กะพริบแวบเดียวแทบไม่เห็น · -> 30 : ขาวค้างนานจนไม่รู้ว่าโดนครั้งไหน
# 3) ลบบรรทัด boss.set_color(game.WHITE) ออก : ยิงโดนบอสแล้วรู้สึก "ไม่โดน" ทั้งที่แถบพลังลด ลองเทียบความรู้สึกดู
