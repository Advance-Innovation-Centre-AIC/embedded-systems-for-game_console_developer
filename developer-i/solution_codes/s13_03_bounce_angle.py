# s13_03_bounce_angle.py — คาบ 13 (คาบพิเศษ) การ์ด 2: สะท้อน และมุมกระทบ
#
# หลักการ   : การ์ด 2 สะท้อน    ชนผนัง: vy = -vy แล้วดึงลูกกลับเข้าขอบ (ต้องครบสองบรรทัด)   (คาบ 5)
#             มุมกระทบ          offset = (กลางลูก - กลางไม้) / ครึ่งไม้  อยู่ใน -1 ถึง 1
#                               vy += 2 * offset  แบบเดียวกับ pong_step3.py                  (คาบ 7)
#             vy มีเพดาน VY_MAX (การ์ด 4 คาบ 3) ลูกจะได้ไม่ชันจนเด้งขึ้นลงอยู่กับที่
# ลองเล่น   : กด Start · จอยขึ้น/ลง = ขยับไม้ (ซ้ายมือ) · ปล่อยให้ลูกเด้งผนังบน ล่าง ขวา
#             ลองรับลูกด้วย "ปลายบน" "กลาง" "ปลายล่าง" ของไม้ แล้วดูบรรทัด offset กับ vy
#             จุดสีบนขอบไม้ = ตำแหน่งที่ลูกเพิ่งโดน (เขียว = กลาง · เหลือง · ส้ม = ปลายไม้)
#             ผนังกะพริบขาวตอนลูกชน · รับพลาด = ลูกเสิร์ฟใหม่จากกลางจอทันที
# ของบนบอร์ด: จอ · จอย · ลำโพง
# ในเกม     : Pong · ทุบอิฐ · ลูกเด้งผนัง (pong_step1.py ถึง pong_step3.py)
# ในงานจริง : ชิ้นงานกระทบแผ่นเบี่ยงทางบนสายพานแล้วเปลี่ยนทิศ · หุ่นยนต์ดูดฝุ่นกลับทิศเมื่อชนผนัง
#             ตัวเลขทั้งหมดเป็นค่าจำลองเพื่อการเรียน
# บอร์ด     : Eva Kit (เฟิร์มแวร์ Game Console) และ BENTO Emulator
# ต่อยอดจาก : pong_step1.py (สะท้อนผนัง) · pong_step3.py (มุมกระทบ hit_offset)
# งบวิดเจ็ต : 10 ชิ้น (ผนัง 3 + ไม้ 1 + จุดโดน 1 + ลูก 1 + ข้อความ 3 + game.run 1)
#
# โครงไฟล์ (เหมือนกันทุกไฟล์ของคาบ 13):
#   1) ตั้งค่า (แก้ได้)   2) สมอง: คิดล้วน ๆ ไม่แตะจอ   3) ตรวจสมองก่อนเปิดจอ (ไฟล์นี้ข้าม)
#   4) หน้าจอ: สร้างครั้งเดียว อัปเดตเฉพาะที่เปลี่ยน   5) วงวนหลัก
import bentogame as game

# ---- 1) ตั้งค่า (แก้ได้) ----
BALL_VX = 8.0                   # ความเร็วแนวนอนของลูก (คงที่ ไฟล์นี้ดูเฉพาะ vy)
SERVE_VY, VY_MAX = 2.0, 9.0     # vy ตอนเสิร์ฟ และเพดานความเร็วแนวตั้ง
PADDLE_SPEED = 9                # ไม้ขยับกี่พิกเซลต่อเฟรม
FLASH = 4                       # ผนังกะพริบกี่เฟรม (นับเฟรม ไม่ใช้ sleep)
TOP, BOTTOM, RIGHT = 70, 356, 776                     # ขอบสนามด้านใน (ผนังอยู่นอกเส้นเหล่านี้)
BALL, PADDLE_X, PADDLE_W, PADDLE_H = 14, 20, 14, 90   # ขนาดภาพ ball / paddle
WALL = 0x8899AA


# ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ (ทดสอบได้โดยไม่ต้องมีบอร์ด) ----
def clamp(value, low, high):
    return max(low, min(high, value))


def hit_offset(ball_y, paddle_y):
    # โดนกลางไม้ = 0 · ปลายบน = -1 · ปลายล่าง = +1 (สูตรเดียวกับ pong_step3.py แล้วจำกัดขอบ)
    return clamp(((ball_y + BALL / 2) - (paddle_y + PADDLE_H / 2)) / (PADDLE_H / 2), -1.0, 1.0)


def paddle_vy(vy, offset):
    # การ์ด 2: มุมกระทบ แล้วจำกัดเพดาน (การ์ด 4)
    return clamp(vy + 2 * offset, -VY_MAX, VY_MAX)


# ---- 4) หน้าจอ: สร้างครั้งเดียว ----
game.title("BOUNCE")
game.Text("ชนผนัง:  vy = -vy        ชนไม้:  vy += 2 * offset", 16, 4, game.YELLOW)
hud_hit = game.Text("รับลูกด้วยไม้ แล้วดูตัวเลขตรงนี้", 16, 34, game.WHITE)
hud_score = game.Text("รับได้ 0   พลาด 0", 600, 34, game.WHITE)
walls = {"top": game.Box(0, TOP - 6, 792, 6, WALL), "bottom": game.Box(0, BOTTOM, 792, 6, WALL),
         "right": game.Box(RIGHT, TOP - 6, 16, BOTTOM - TOP + 12, WALL)}
paddle = game.Sprite("paddle", PADDLE_X, (TOP + BOTTOM - PADDLE_H) // 2)
paddle.w, paddle.h = PADDLE_W, PADDLE_H              # ภาพไม่รู้ขนาดตัวเอง ตั้งเองก่อนใช้ game.hit
mark = game.Box(-20, 0, 8, 14, game.GREEN)           # จุดที่ลูกเพิ่งโดนไม้ (จอดนอกจอไว้ก่อน)
ball = game.Sprite("ball", 396, 200)
ball.w, ball.h = BALL, BALL

bx, by, vx, vy = 396.0, 200.0, -BALL_VX, SERVE_VY
paddle_y, hits, misses, flash_wall, flash_left = paddle.y, 0, 0, None, 0


def flash(name):
    # ผนังที่ลูกชนกะพริบขาว FLASH เฟรม พร้อมเสียง wall
    global flash_wall, flash_left
    if flash_wall is not None:
        walls[flash_wall].set_color(WALL)
    flash_wall, flash_left = name, FLASH
    walls[name].set_color(game.WHITE)
    game.sfx("wall")


# ---- 5) วงวนหลัก ----
def on_frame():
    global bx, by, vx, vy, paddle_y, hits, misses, flash_wall, flash_left
    keys = game.keys()

    # เข้า: ขยับไม้แล้วจำกัดขอบ (จุดโดนเลื่อนตามไม้)
    step = (PADDLE_SPEED if keys.down else 0) - (PADDLE_SPEED if keys.up else 0)
    new_y = clamp(paddle_y + step, TOP, BOTTOM - PADDLE_H)
    if new_y != paddle_y:
        if hits:
            mark.move_to(mark.x, mark.y + new_y - paddle_y)
        paddle_y = new_y
        paddle.move_to(PADDLE_X, paddle_y)
    if flash_left > 0:                               # นับถอยหลังแล้วคืนสีผนัง
        flash_left -= 1
        if flash_left == 0:
            walls[flash_wall].set_color(WALL)
            flash_wall = None

    # คิด: สะสมค่า แล้วสะท้อน = ดึงกลับเข้าขอบ + กลับเครื่องหมาย (ต้องครบสองบรรทัด)
    bx += vx
    by += vy
    if by <= TOP:                                    # ชนผนังบน
        by = TOP
        vy = -vy
        flash("top")
    elif by >= BOTTOM - BALL:                        # ชนผนังล่าง
        by = BOTTOM - BALL
        vy = -vy
        flash("bottom")
    if bx >= RIGHT - BALL:                           # ชนผนังขวา (กลับทิศแนวนอนแทน)
        bx = RIGHT - BALL
        vx = -vx
        flash("right")
    ball.move_to(bx, by)

    # ออก: ชนไม้ = มุมกระทบ · หลุดซ้าย = พลาด เสิร์ฟใหม่
    if vx < 0 and game.hit(ball, paddle):
        offset = hit_offset(by, paddle_y)
        bx, vx = PADDLE_X + PADDLE_W, abs(vx)
        vy = paddle_vy(vy, offset)
        hits += 1
        far = abs(offset)                            # เขียว = กลางไม้ · เหลือง · ส้ม = ปลายไม้
        mark.set_color(game.GREEN if far < 0.34 else (game.YELLOW if far < 0.67 else game.ORANGE))
        mark.move_to(PADDLE_X + PADDLE_W, int(paddle_y + PADDLE_H / 2 + offset * PADDLE_H / 2) - 7)
        hud_hit.set("offset %+.2f  ->  vy += %+.2f  ->  vy %+.1f" % (offset, 2 * offset, vy))
        game.sfx("paddle")
    elif bx < -BALL:
        misses += 1
        bx, by, vx = 396.0, 200.0, -BALL_VX
        vy = SERVE_VY if misses % 2 else -SERVE_VY
        game.sfx("lose")
    else:
        return True
    hud_score.set("รับได้ %d   พลาด %d" % (hits, misses))
    return True


game.run(on_frame, fps=30)

# ---- ลองแก้ แล้วรันใหม่ ----
# 1) ลบบรรทัด vy = -vy ใต้ if by <= TOP : ลูกไถลติดผนังบน ผนังค้างสีขาวพร้อมเสียงรัว
#    สะท้อนต้องมีครบสองอย่าง: ดึงกลับเข้าขอบ และกลับเครื่องหมาย
# 2) เปลี่ยน 2 * offset เป็น 6 * offset ในฟังก์ชัน paddle_vy : ตีปลายไม้นิดเดียว ลูกชันขึ้นทันที
# 3) VY_MAX = 9.0 -> 30.0 : ตีปลายไม้ซ้ำหลายครั้ง ลูกชันจนเกือบเด้งขึ้นลงอยู่กับที่ — เพดานจึงสำคัญ
