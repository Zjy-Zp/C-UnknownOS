#ifndef EDITOR_H
#define EDITOR_H

#include "fat.h"

/* nano 风格全屏编辑器：Ctrl+O 保存，Ctrl+X 退出 */
void editor_open(fat_fs *fs, const char *filename);

#endif
