# shooter_step2.py — Shooter #1 (คาบ 10) step 2: บังคับยานซ้าย/ขวา (เร่ง + ไถล)
# ------------------------------------------------------------------------------
# step นี้ทำให้ยาน "ขับได้และมีน้ำหนัก" เหมือนเข็นรถเข็นในห้าง กดค้างไว้แล้วยิ่งเร็วขึ้น
# (เร่ง/ACCEL) พอปล่อยมือก็ไถลต่อแล้วช้าลงเอง (FRICTION) ชนขอบจอแล้วหยุด (clamp)
# ปุ่ม -> ความเร็ว (ship_speed) -> ตำแหน่ง (ship_x) -> จอ (ship.move_to)
# คำสั่งที่เอนจินมีให้ (70%) ที่เพิ่งได้ใช้: game.keys() (อ่านปุ่ม/จอย) · ship.move_to()
# ท่าเดียวกับไม้ตี Pong คาบ 6 (pong_step2.py) แค่เปลี่ยนจากแนวตั้งเป็นแนวนอน
# move_to() ตัดเศษเป็นพิกเซลเต็มตอนวาด (bentogame.py:238) ส่วน ship_x เก็บทศนิยมไว้ครบ
# เกมเต็ม: full_games/shooter_full.py:18 (ค่าฟิสิกส์) และ :178-193 (ขับยาน 2 แกน)
# ------------------------------------------------------------------------------
import bentogame as game

ACCEL, MAX_SPEED, FRICTION = 1.4, 13.0, 0.80  # ค่าเดียวกับเกมเต็ม (shooter_full.py:18)

game.title("SHOOTER")                          # หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว)

ship = game.Box(365, 352, 62, 24, game.GREEN)
ship_x, ship_speed = 365.0, 0.0               # ตำแหน่ง x + ความเร็วของยาน
score, lives = 0, 3
hud = game.Text("Score: 0   Lives: 3", 10, 8, game.WHITE)

def on_frame():
    global ship_x, ship_speed
    keys = game.keys()
    # ปุ่มระบบ เอนจินจัดการเอง: Back = ออก · Start = พักเกม (Pause)

    # ----- เติมส่วนนี้เอง: 5 ขั้นของยานที่มีน้ำหนัก -----
    if keys.left:    ship_speed -= ACCEL      # ① เร่งไปทางซ้าย
    elif keys.right: ship_speed += ACCEL      # ① เร่งไปทางขวา
    else:            ship_speed *= FRICTION   # ② ไถล: เหลือ 80% ทุกเฟรม
    ship_speed = max(-MAX_SPEED, min(MAX_SPEED, ship_speed))   # ③ เพดานความเร็ว
    ship_x = max(0, min(game.WIDTH - ship.w, ship_x + ship_speed))  # ④ ขยับ + ขอบจอ
    ship.move_to(ship_x, 352)                                  # ⑤ วาด
    # -----------------------------------------------------------------------

game.run(on_frame, fps=30)
