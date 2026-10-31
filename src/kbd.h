#ifndef KBD_H
#define KBD_H

#include "common.h"

/* 特殊键用负值表示，普通字符直接用 ASCII 正值 */
#define K_NONE   0
#define K_UP    (-1)
#define K_DOWN  (-2)
#define K_LEFT  (-3)
#define K_RIGHT (-4)
#define K_DELETE (-5)
#define K_HOME  (-6)
#define K_END   (-7)
#define K_PGUP  (-8)
#define K_PGDN  (-9)
#define K_ESC   (-10)

void kbd_init(void);
int  kbd_present(void);   /* 8042 控制器是否存在 */
int  kbd_poll(void);      /* 非阻塞，无按键返回 K_NONE */
int  kbd_getkey(void);    /* 阻塞等待按键 */
int  kbd_shift_down(void);
int  kbd_ctrl_down(void);

#endif
