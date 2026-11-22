# cunos 操作指南

本文面向在 QEMU 或实体机上操作 cunos 的使用者，逐条说明 shell 命令、内置程序、编辑器、输入法、文件系统与磁盘操作、图形桌面、网络配置以及常见操作流程。

本文所有命令名、参数形式、快捷键与提示文案，均取自 `C:\cunos` 仓库源码；凡是源码中不存在的能力，本文标注为「未实现」或直接不写，不做推测性描述。主要依据文件：`src/kernel.c`（shell、help_table、命令分发）、`src/editor.c`、`src/ime.c`、`src/net.c`、`src/install.c`、`user/bin/*.c`。

---

## 1. 版本与术语约定

内核版本字符串定义于 `src/kernel.c:26`：

```c
#define CUNOS_VERSION "1.1"
```

`help` 标题与启动横幅都使用该值（`src/kernel.c:948`、`src/kernel.c:1517`）。注意：图形桌面状态栏右上角显示的版本号是另一处硬编码字符串 `cunos 0.3.0`（`src/kernel.c` 的 `desktop_frame()`），与内核版本字符串不是同一个量。本文统一称内核版本为「cunos 1.1」。

本文与其余三份文档（架构指南、开发指南、实体机配置参考）共用以下术语：

| 术语 | 含义 | 依据 |
| --- | --- | --- |
| 卷（volume） | 一个已挂载的文件系统，登记在全局卷表中，最多同时登记 2 个 | `src/kernel.c` 的 `g_fs` / `g_fs_count` |
| 当前卷 | shell 当前操作的卷，即 `curfs()` 的返回值 | `src/kernel.c` |
| rootfs | 第二块盘（`-hdb`）上的 ext2 卷，挂载后成为 VFS 的 `/` | `cmd_root()`，`src/kernel.c` |
| 安装介质 | 作为第二块盘接入的 cunos 盘，卷内是整盘 FAT32 或分区 FAT32 | `src/install.c` 第 5 步 |
| 内建命令 | 由 `src/kernel.c` 中 `cmd_*` 函数实现、在 shell 进程内执行的命令 | `kernel_exec_line()` |
| 外部命令 | 由 `/bin/<名字>.elf` 装载、在 ring3 运行的 ELF32 程序 | `run_external()` |
| `/bin` 程序 | `user/bin/*.c` 用 `tools/build_bin.sh` 编译出的静态 ELF32，源码目录见 `user/bin/` | `tools/build_bin.sh` |

---

## 2. 启动与进入 shell

### 2.1 启动链

BIOS/UEFI（CSM）→ GRUB → `multiboot` 装载内核 → `kernel_main()`。ISO 版引导菜单定义在 `src/grub.cfg`：

```
set timeout=0
set default=0
insmod all_video
set gfxmode=1024x768x32,800x600x32,640x480x32,auto
set gfxpayload=1024x768x32
menuentry "cunos" {
    multiboot /boot/cunos.bin
}
```

即：默认无菜单等待，优先申请 1024x768x32 帧缓冲，失败时依次退到 800x600x32、640x480x32、`auto`。

内核启动完成后打印横幅（`banner()`，`src/kernel.c`），内容包含版本、显示模式、输入设备、磁盘扇区数、已挂载卷列表、关机方式，随后进入 shell。**cunos 没有登录流程，也没有用户账号概念**：开机即得到 root 权限的 shell，源码中不存在用户认证代码。

### 2.2 启动参数

引导时可在 `multiboot` 行追加 `nobg`，放弃帧缓冲、使用 VGA 文本模式 80x25。这一入口写在 `src/install.c` 内嵌的 `grub_cfg` 中：

```
menuentry "cunos (no framebuffer)" {
    multiboot /cunos.bin nobg
    boot
}
```

当显卡 VBE 模式枚举异常、或需要纯文本串口日志时使用它。文本模式下 `vga_fb_active()` 返回 0，`desktop` 命令会被拒绝。

### 2.3 QEMU 启动方式

仓库提供若干批处理脚本，其中 `boot.bat` 是最常用入口。`Makefile` 的 `run` 目标固定使用：

```
/mnt/c/EXES/qemu/qemu-system-i386.exe -cdrom $(OUTFILE) -m 128
```

即单核、128 MB 内存、光盘引导。需要挂第二块盘（rootfs 或安装介质）时，用 `-hdb` 指定镜像；`cuninstall` 与 `root` 都依赖第二块盘存在（`src/install.c`、`cmd_root()`）。

---

## 3. 终端提示符与路径语义

### 3.1 提示符格式

`shell()`（`src/kernel.c:1693` 起）每轮输出：

```
cunos<卷标记> <当前路径> > 
```

- `cunos`：固定前缀，颜色 `C_PROMPT`。
- 卷标记：按顺序判定——ext2 rootfs 已挂载输出 `:ext2`；否则若有当前卷输出 `:fat16` 或 `:fat32`（由 `f->type` 决定）；否则输出 `:nodisk`。
- 当前路径：由 `path_disp()` 生成，宽度上限 `CU_PATH_DISP_MAX`，过长时截断显示，但内部实际路径不受影响。

因此 `cunos:fat32 /bin > ` 表示当前卷是 FAT32、当前目录是 `/bin`；`cunos:nodisk / > ` 表示没有识别到任何卷，此时 `/bin` 外部命令全部不可用。

### 3.2 路径语义

- 所有相对路径从**当前工作目录**解析，`abspath()` / `path_abs()` 完成拼接（`user/bin/cu.h`、`src/kernel.c`）。
- `cd` 无参数回 `/`；`cd -` 回到上一个目录（`path_prev()`）。
- `cd` 的错误分支文案固定（`cmd_cd()`）：`no such directory`、`not a directory`、`subdirectories need the ext2 rootfs`、`path too long (limit %d bytes)`。
- 第三条文案说明一个重要约束：**只有 rootfs（ext2）挂载后，FAT 卷上才支持子目录路径解析**。未执行 `root` 时，`cd /bin`、`cat /etc/lang` 这类多级路径会报 `subdirectories need the ext2 rootfs`。
- 路径长度上限 `CU_PATH_MAX`，超限报 `path too long`。

---

## 4. 命令行输入规则

### 4.1 行编辑能力

`readline()`（`src/kernel.c`）实现行输入，能力边界如下：

| 按键 | 行为 |
| --- | --- |
| 可打印 ASCII（0x20–0x7F） | 插入到行尾光标处 |
| Enter / Return | 提交该行，并 `ime_reset()` 清空输入法缓冲 |
| Backspace | 删除一个字符；汉字等双宽字符占两格，按一次删一个完整字符 |
| Ctrl+Space | 切换输入法（英文 ↔ 上一次使用的输入法），见 `ime_toggle()` |
| 左/右/上/下方向键 | **被忽略**，不做光标移动，也没有历史命令 |

行缓冲长度 256 字节（`static char line[256]`）。**源码中不存在 Tab 补全、历史记录（上下键回溯）、管道、重定向和引号语义**：`split_args()` 只按空格与制表符切分，最多取 8 个参数（`argv[8]`），第 9 个及以后的参数被丢弃。因此含空格的参数无法用引号保护，`say hello world` 会被拆成两个参数并由 `say` 重新用空格拼回。

### 4.2 两个行首通用开关

`kernel_exec_line()` 会先扫描整行参数，把 `-d`、`-q` 摘出（剩余参数前移），因此它们可以出现在任意位置：

| 开关 | 作用 | 依据 |
| --- | --- | --- |
| `-d` | 打开内核调试输出（`g_dbg=1`）。外部命令会额外打印 `exec: <path> (<bytes> bytes)` | `kernel_exec_line()`、`run_external()` |
| `-q` | 输出分页，按空格翻下一页（`vga_pager_set(1)`） | `kernel_exec_line()`、`vga_pager_set()` |

示例：`-d ls /bin`、`fs -q`。

### 4.3 命令解析顺序

`kernel_exec_line()` 的判定顺序（`src/kernel.c:1597` 起）：

1. 若当前卷存在且 `argv[0]` 非空，先尝试 `run_external()`：按 `/bin/<名字>.elf` 查表（名字不以 `.elf` 结尾则自动补 `.elf`），命中且不是目录属性（`at & 0x10`）就用 `elf_exec()` 装载执行；`argv[0]` 以 `/` 开头时按该绝对路径在当前 FAT 卷上查找。
2. 外部命令未命中，再逐条匹配内建命令名（`help`、`cd`、`lang`、`ime`、`say`、`root`、`cuninstall`、`mkfat32`、`ino`、`enter`、`edit`/`nano`、`fs`、`vfs`、`run`/`exec`、`pci`、`usb`、`net`、`ifconfig`、`ping`、`tcp`、`chat`、`drm`、`color`/`colours`、`clear`/`cls`、`shutdown`/`poweroff`/`halt`、`reboot`）。
3. 都不匹配，打印未知命令提示（`MSG_NOCMD`，文案随 `lang` 切换）。

**含义**：`ls`、`cat`、`copy`、`tetris` 等命令实际是 `/bin` 下的 ELF 程序，不是内核内建函数；`cd`、`root`、`color`、`clear`、`reboot` 必须留在内核内，因为它们要改 shell 自身状态或直接操作硬件（源码 `src/kernel.c:176` 注释明确说明了这一点）。

---

## 5. shell 命令全表

下表按 `help_table`（`src/kernel.c:584`–`965`）的 `group` 字段分组，逐条给出 `names`、`usage`、`desc`、`note` 的中文化说明，并补充源码中可核实的细节。语法列与源码 `usage` 逐字一致。

### 5.1 shell 分组

| 命令 / 别名 | 语法 | 作用 | 说明与前置条件 |
| --- | --- | --- | --- |
| `help` / `?` | `help [command\|group]` | 帮助列表；给一条命令或一个分组名 | 分组名同样可用，例如 `help network`；无参数打印全表并统计命令数与别名数 |
| `-d` | `-d <command>` | 带内核调试输出执行一条命令 | 等价于在行内任意位置写 `-d` |
| `-q` | `-q <command>` | 输出分页，空格翻页 | 同上 |
| `lang` | `lang [zh\|en\|ru\|de]` | 切换全系统界面语言 | 保存在 `/etc/lang`；无参数显示当前语言与可用列表；非法值提示 `MSG_LANG_BAD` |
| `ime` | `ime [en\|zh\|ru\|de]` | 切换命令行输入法 | 保存在 `/etc/ime`；`zh` 说明为拼音 + 3000 常用汉字，空格或 `1`–`9` 上屏；非法值提示 `unknown input method '<code>'` |
| `say` | `say <text...>` | 按 UTF-8 原样回显，用于验证输入法输出 | 帮助里给的示例是 `say 你好 привет ä` |
| `color` / `colours` | `color` | 打印 16 色 VGA 调色板色条 | `vga_color_bar()` |
| `clear` / `cls` | `clear` | 清屏 | `vga_clear()` |
| `reboot` | `reboot` | 复位机器，重新进入 GRUB | 先等 0x64 状态寄存器空闲，再向 0x64 写 `0xFE` |
| `shutdown` / `poweroff` / `halt` | `shutdown` | ACPI S5 软关机 | 需要 ACPI；无 FADT 时提示 `no FADT found, trying legacy PM1a_CNT`，失败后打印 `shutdown failed, system halted (close the window manually)` 并 `hlt` 死循环 |

示例：

```
help
help network
lang zh
ime zh
say 你好 привет ä
-d ls /
```

### 5.2 filesystem 分组

| 命令 / 别名 | 语法 | 作用 | 说明与前置条件 |
| --- | --- | --- | --- |
| `root` | `root` | 把 `-hdb` 上的 ext2 卷挂为 VFS `/` | 已挂载时报 `rootfs already mounted as VFS /`；找不到卷报 `no ext2 volume on the second disk (-hdb)`；成功后打印块数与块大小 |
| `cd` / `chdir` | `cd [dir]` | 切换工作目录 | 无参数回 `/`，`cd -` 回上一个目录 |
| `mkfat32` | `mkfat32 [start_lba] [sectors]` | 把 ATA 盘格式化为一个 FAT32 卷（安装器配套工具） | 默认 `start_lba`=2048，省略 `sectors` 时取 `ata_sector_count()-start` 并允许追加 32768 扇区上限；标签固定 `CUNOS`；格式化后自动挂载登记为卷并切换为当前卷 |
| `cuninstall` | `cuninstall` | 把 GRUB、FAT32、内核与 `/bin` 程序写入 ATA 盘 | 需逐字输入 `YES` 再输入 `GRUB` 才写盘，随后打印 `[1/6]`…`[6/6]`，详见第 9 节 |
| `ino` | `ino <path>` | 打印 inode 号、mode、大小并做一次 VFS 打开测试 | 必须先 `root` 挂载 rootfs |
| `enter` | `enter <file>` | 在 FAT 卷上创建文本文件，逐行输入 | 单独一行 `.` 结束输入；缓冲 4096 字节，超出截断并告警 |
| `edit` / `nano` | `edit <file>` | 全屏编辑器，作用于当前卷 | `^O` 保存、`^X` 退出；缺参数报 `usage: edit <filename>` |
| `fs` | `fs [n]` | 无参数显示磁盘与卷信息；带 `n` 切换当前卷 | 无参数输出 ATA 容量、卷摘要、当前卷空闲/总簇数 |
| `vfs` | `vfs` | 打印打开的文件描述符表 | 输出 fd、flags、pos/size、名字与写缓冲上限 `VFS_WRITE_MAX` |

示例：

```
fs
fs 1
root
ino /bin/ver.elf
enter note.txt
edit note.txt
```

### 5.3 bin 分组（对应 `/bin` 下的 ELF 程序）

| 命令 / 别名 | 语法 | 对应程序 | 行为 |
| --- | --- | --- | --- |
| `echo` | `echo [text...]` | `/bin/echo.elf` | 把参数用单空格拼成一行输出，无换行控制 |
| `ver` / `version` | `ver` | `/bin/ver.elf` | 通过系统调用接口读取内核版本与子系统摘要 |
| `ls` / `dir` | `ls [-a] [path]` | `/bin/ls.elf` | 列目录，按名字排序，目录加 `[DIR]` 前缀；`-a`/`-la`/`-al` 连 `.`/`..` 一起列；无参数列当前目录；多个路径时每个路径单独一段 `path:`，末尾打印 `N entry/entries` |
| `tree` | `tree [path]` | `/bin/tree.elf` | 递归画目录树，**深度 4 层**；无参数从当前目录开始 |
| `enterf` / `mkdir` | `enterf <dir>` | `/bin/enterf.elf`、`/bin/mkdir.elf` | 建目录，路径从当前目录解析，父目录自动创建（`fat_mkpath`） |
| `pwd` | `pwd` | `/bin/pwd.elf` | 打印当前工作目录 |
| `cat` | `cat <path>` | `/bin/cat.elf` | 打印文件内容，**只读前 4096 字节**，不可打印字节显示为 `.` |
| `curl` | `curl [-i] [-o <file>] <url>` | `/bin/curl.elf` | 走 socketcall TCP 做 HTTP 下载；`-i` 连同响应头显示，`-o` 把正文写入文件；URL 只接受**点分 IPv4**，不支持域名解析 |
| `tetris` | `tetris` | `/bin/tetris.elf` | 文本模式俄罗斯方块，读写 `/dev/vga` 与 `/dev/kbd`；`a`/`d` 左右移动，`w` 旋转，空格下落，`p` 暂停 |
| `copy` | `copy <file> <dir>` | `/bin/copy.elf` | 把文件复制进目标目录，保留源文件名 |
| `del` / `rm` | `del <file>` | `/bin/rm.elf` | 删除 FAT 卷上的文件；**ext2 上尚无删除实现** |

示例：

```
ls -a /
tree /bin
pwd
cat /etc/lang
copy note.txt /bin
del note.txt
curl -i -o index.html 10.0.2.2/index.html
```

### 5.4 userland 分组

| 命令 / 别名 | 语法 | 作用 | 说明与前置条件 |
| --- | --- | --- | --- |
| `run` / `exec` | `run <file> [args...]` | 在 ring3 装载并运行 i386 ELF32 | 绝对路径（以 `/` 开头）走 ext2 树形 VFS 并支持 `PT_INTERP` 动态链接与 PIE；未挂载 rootfs 时报 `run: ext2 rootfs not mounted (try 'root')`。相对名走当前 FAT 卷；无卷时报 `run: no FAT volume mounted` |
| `desktop` / `x11` | `desktop` | 图形桌面：可拖动窗口，内含终端 | 需要帧缓冲，否则报 `framebuffer mode required (VESA VBE not active)`；启动后打印 `cunos X11 server`、`transport COM1 @ 115200 8N1`、`protocol X11 core subset depth 24 TrueColor`，`Ctrl+Q` 返回 shell |
| `kde` | `kde` | 只读探测：读 `-hdb` 的 ext2 卷并检查 KDE 相关 ELF | 只打印探测结果，**不启动任何桌面**（`help_table` 的 note 明确写 "read only smoke test, no desktop is started"） |

示例：

```
run /bin/ver.elf
desktop
```

### 5.5 hardware / network / memory / graphics 分组

| 命令 | 语法 | 作用 | 说明 |
| --- | --- | --- | --- |
| `pci` | `pci` | 列出 PCI 设备的 vendor/class/位置，并附 GPU、USB 摘要 | `cmd_pci()` |
| `usb` | `usb` | 列出 USB 主机控制器、端口与设备 | `cmd_usb()` |
| `net` | `net` | 打印活动网卡驱动、链路状态、IPv4、MAC、ARP 缓存、计数 | `net_dump()` |
| `ifconfig` | `ifconfig [ip] [mask] [gw]` | 查看或设置活动网卡 IPv4 | 无参数等同 `net`；地址非法报 `ifconfig: bad address`；无网卡报 `ifconfig: no NIC, address stored but link is down`；成功报 `ifconfig: applied` |
| `ping` | `ping [ip]` | 4 次 ICMP 回显 | 无参数 ping 默认网关；无网卡报 `ping: no NIC` |
| `tcp` | `tcp [ip] [port] [msg]` | TCP 连接、发送一条消息并读回 | 默认目标网关、端口 80、空消息 |
| `chat` | `chat <ip> <port>` | 全双工 TCP 聊天，键盘与对端同时工作 | 缺参报 `chat: usage: chat <ip> <port>`；无链路报 `chat: no link, no supported NIC`；连接失败报 `chat: connect failed <code>`；连接成功提示 `chat: connected, type a line and press enter, /quit or ESC to leave`；`/quit`、`/exit` 或 `ESC` 退出；对端关闭报 `chat: peer closed the connection` |
| `mem` | `mem` | 打印 VMA 列表与缺页/COW/页缓存计数 | `cmd_mem()` |
| `drm` | `drm` | `fb0` + `card0` 的 ioctl 自检 | `drm_selftest()` |

### 5.6 help 明确列为「仅预留接口」的能力

`help_planned[]`（`src/kernel.c`）原样列出尚未实现的区域，`help` 会一并展示，用于避免误导：

```
process: fork / clone / execve / wait4, preemptive scheduler
signal:  sigaction / kill / sigreturn
ipc:     pipe, futex, shm
fs:      ext4 write, /proc, /sys, uid-gid permission checks
system:  SMP, loadable modules, udev, cgroup, POSIX timers
```

操作层面据此可得：**没有管道、没有信号、没有 /proc、没有权限检查、没有多核支持**；调度是协作式的（见架构指南）。

---

## 6. `/bin` 程序清单

`user/bin/` 下每个 `.c` 文件都是一个独立程序，源码清单（`C:\cunos\user\bin\`）：

| 源文件 | 可执行文件 | 功能 |
| --- | --- | --- |
| `crt0.c` | （链接用启动桩，不单独产出） | 提供 `_start`，从栈上取 `argc/argv` 调 `cu_main`，再调 exit |
| `cu.h` / `mkpath.h` | （头文件） | 极简用户态运行时：`sys3()` 内联系统调用、`puts_`、`pnum`、`abspath()`；无 libc |
| `echo.c` | `echo.elf` | 回显参数 |
| `ver.c` | `ver.elf` | 版本与子系统摘要 |
| `ls.c` | `ls.elf` | 列目录（支持 `-a`、多路径，见 5.3） |
| `tree.c` | `tree.elf` | 目录树，4 层 |
| `pwd.c` | `pwd.elf` | 打印工作目录 |
| `cat.c` | `cat.elf` | 打印文件前 4096 字节 |
| `copy.c` | `copy.elf` | 复制文件到目录 |
| `rm.c` | `rm.elf` | 删除 FAT 文件 |
| `mkdir.c`、`enterf.c` | `mkdir.elf`、`enterf.elf` | 建目录（含父目录） |
| `curl.c` | `curl.elf` | HTTP 客户端 |
| `tetris.c` | `tetris.elf` | 文本俄罗斯方块 |
| `forktest.c` | `forktest.elf` | fork/wait 验证程序 |
| `exectest.c` | `exectest.elf` | exec 验证程序 |
| `pietest.c` | `pietest.elf` | PIE（ET_DYN）装载验证 |
| `pie/pie.c` | `pie.elf` | ET_DYN + `-pie` 编译的验证程序 |

另外 `user/hello.c`、`user/vfs_test.c` 由 `tools/build_user.sh` 编译进**根目录**（写入镜像 `::/`，不进 `/bin`），用于早期冒烟测试。

如何运行这些程序：

```
ls /bin
run /bin/forktest.elf
run /bin/pietest.elf
```

也可以直接敲 `forktest`、`pietest`，`run_external()` 会自动补 `.elf`。注意：`forktest`、`exectest`、`pietest`、`pie` **没有**登记在 `help_table` 里，因此 `help` 不会列出它们，但它们确实在 `/bin` 中。

---

## 7. 编辑器使用

`edit <file>` / `nano <file>` 进入编辑器（`src/editor.c`），界面自下而上分四区：标题栏、正文区、状态栏、快捷键栏。

```
 cunos edit   note.txt    [modified]
...
line 3 / col 12    245 bytes    saved to disk
^O save   ^X exit   arrows move   Enter newline   Backspace delete 
```

### 7.1 快捷键

| 按键 | 行为 |
| --- | --- |
| `^O` / `^o` | 保存到当前卷（`save()`） |
| `^X` / `^x` | 退出；若有未保存修改，第一次按只提示 `unsaved changes, press ^X again to quit` |
| `ESC` | 有未保存修改时提示 `unsaved changes, use ^X to quit`，不会退出 |
| 方向键 | 移动光标（左/右/上/下） |
| `Home` / `End` / `PgUp` / `PgDn` / `Delete` | 行首/行尾/翻页/删除 |
| `Enter` | 换行 |
| `Backspace` | 删除前一个字符 |
| 可打印 ASCII（32–126） | 插入字符 |

### 7.2 边界与限制

- 单文件缓冲上限 `TEXT_MAX`=8192 字节，写满后再输入提示 `buffer full`，不覆盖已有内容。
- 保存失败路径：无 FAT 卷时提示 `no FAT volume mounted`；写盘失败提示 `save failed: disk write error`；成功提示 `saved to disk` 并把 `[modified]` 标记清掉。
- 打开文件时：能查到且长度大于 0 则读入并提示 `file loaded`，否则按新文件处理并提示 `new file`。
- 正文区行数随显示模式变化：帧缓冲模式下为 `disp_rows()-4`，文本模式下固定 21 行。
- **编辑器只接受 32–126 的可打印 ASCII**，没有输入法接入口，因此不能在编辑器里直接输入中文；中文内容请用 `say` 验证输入法输出后用 `enter` 写入，或自行在宿主侧准备文件。

---

## 8. 输入法（IME）

### 8.1 切换方式

- `Ctrl+Space`：在「英文」与「上一次使用的输入法」之间切换（`ime_toggle()`，`src/ime.h`），用于命令行即时输入。
- `ime [en|zh|ru|de]`：直接用命令设定，写入 `/etc/ime` 持久化，下次启动由 `load_ime_setting()` 读取生效。
- `ime` 不带参数：显示当前输入法与可用列表：

```
en English | zh 中文拼音 | ru Русский | de Deutsch
```

### 8.2 四种输入法

| 代码 | 输入方式 | 源码说明 |
| --- | --- | --- |
| `en` | 直接英文 | 无缓冲 |
| `zh` | 拼音输入，内置 3000 常用汉字；候选上屏用**空格**或**数字 `1`–`9`**，`ESC` 取消 | `ime.c`；词库由 `tools/gen_cjk3000.py` 生成 |
| `ru` | 拉丁转写 → 西里尔字母，`sh ch zh shch yu ya kh ts yo` 等规则 | `ime.c` |
| `de` | 死键：先按 `"` 再按 `a`/`o`/`u`/`s` 得 `ä`/`ö`/`ü`/`ß` | `ime.c` |

### 8.3 使用要点

- 输入法处理由 `ime_key()` 完成，返回三种结果：大于 0 表示有 UTF-8 文本待上屏；返回 `0` 表示该键被输入法消费（缓冲或候选变化）；返回 `IME_PASS` / `IME_PASS_BUF` 表示按键按原义继续处理。命令行遇 `IME_PASS_BUF` 会先上屏缓冲再处理该键，避免空格、标点被吞。
- 拼音缓冲与候选提示绘制在屏幕底部状态行（`ime_status_refresh()`）。
- 回车提交命令行时会 `ime_reset()`，不会把拼音残缓冲带进下一条命令。
- 中文在命令行里按**双宽字符**处理，退格一次删一个完整汉字。
- 验证方法：`say 你好` 应当原样回显 `你好`；若显示乱码，说明当前字体/字库或 `lang` 设置不对。

---

## 9. 文件系统与磁盘操作

### 9.1 卷的概念

内核维护一个卷表，最多登记 2 个卷（`g_fs` / `g_fs_count`，`src/kernel.c`）。开机时 `probe_filesystems()` 探测 ATA 盘上的 FAT16/FAT32 并挂载为卷。`fs` 查看、`fs n` 切换当前卷（切换时会 `vfs_set_current()` → `vfs_reset()` → `path_init()`，即工作目录回到 `/`）。

### 9.2 挂载 ext2 rootfs

```
root
```

- 成功：打印 ext2 的 `blocks_count × block_size`，并把 `/` 切换到 ext2 树形 VFS。此后 `ls` 走 `vfs_readdir()`（`getdents64`），能列子目录，`run /bin/...` 可走动态链接路径。
- 失败：`no ext2 volume on the second disk (-hdb)`，说明 QEMU 未挂第二块盘，或第二块盘不是 ext2。
- 重复执行：`rootfs already mounted as VFS /`。

### 9.3 FAT 卷上的文件操作

| 操作 | 命令 | 备注 |
| --- | --- | --- |
| 列目录 | `ls [-a] [path]` | 需多级路径时要求 rootfs 已挂载 |
| 建目录 | `mkdir <dir>` / `enterf <dir>` | 父目录自动创建 |
| 写新文本 | `enter <file>` | 单独一行 `.` 结束 |
| 编辑已有文件 | `edit <file>` | `^O`/`^X` |
| 查看 | `cat <path>` | 仅前 4096 字节 |
| 复制 | `copy <file> <dir>` | 保留源名 |
| 删除 | `del <file>` | FAT only，ext2 无删除 |

**FAT 命名限制**：`fat.h` 中 `#define FAT_NAME_LEN 13`（8.3 + 点 + `\0`），内核 FAT 后端使用 8.3 短名，不实现长文件名（LFN）。写文件时名字由 `fat_format_name()` 规范化，因此请用 `xxxxxxx.yyy` 形式的短名；中文件名、超长名会被截断或规范化。

### 9.4 磁盘格式化与安装

```
mkfat32            # 从 LBA 2048 起，用剩余空间做一个 FAT32 卷，标签 CUNOS
cuninstall         # 安装 GRUB + FAT32 + 内核 + /bin 到 ATA 盘
```

`cuninstall` 的完整流程（`src/install.c` 的 `install_grub_run()`）：

1. 前置检查：`ata_present()` 失败报 `cuninstall: no ATA disk`；内嵌 GRUB 载荷缺失（`boot.img`/`core.img` 找不到或 `boot_size < 440`）报 `cuninstall: embedded grub payload missing (-2)`，并提示执行 `python3 tools/mk_grub_blob.py build/grub_blob.bin`；磁盘小于 `PART_START + 65536` 扇区报 `cuninstall: disk too small (%u sectors)`（`PART_START`=2048）。
2. 打印载荷大小、磁盘扇区数、分区起点/大小与链路类型（`AHCI` 或 `ATA PIO`）。
3. 第一次确认：逐字输入大写 `YES`，否则 `cuninstall: aborted, nothing was written`。
4. 第二次确认：逐字输入大写 `GRUB`，否则同样 abort。
5. 写盘并输出六步进度：
   - `[1/6] MBR written (boot.img 440B + partition table)`
   - `[2/6] core.img written at LBA 1..%u, sector1 sig %02x%02x`
   - `[3/6] FAT32 on partition: %u clusters x %u bytes`
   - `[4/6] /grub/grub.cfg written, %u bytes, cluster %u`（失败为 `verify FAILED`）
   - `[5/6]` 从第二块盘复制内核到 `/cunos.bin`
   - `[6/6]` 把内嵌的 `/bin` 程序写入 `/bin`

安装完成后，目标盘上一定有：MBR 引导代码、`core.img`（LBA 1 起）、一个 FAT32 主分区、`/grub/grub.cfg`、`/cunos.bin`、`/bin/*.elf`。内嵌的 `grub.cfg` 内容是固定的四个菜单项：`cunos (installed on disk)`、`cunos (no framebuffer)`、`GRUB shell`、`Reboot`，默认 5 秒超时，并开启 COM1 115200 的双向串口终端。

**警告**：`cuninstall` 与 `mkfat32` 都会**破坏目标盘原有数据**，属于不可逆写入，请确认目标盘不是宿主系统的数据盘。

### 9.5 查看 VFS 状态

```
vfs
```

输出 fd 表：`fd 0x%03x`、flags、`pos/size`、名字（标准流标 `std`），末尾一行 `%d fd open, write buffer %u bytes`，其中写缓冲上限来自 `VFS_WRITE_MAX`（32768）。VFS 同时提供四个设备后端：`/dev/fb0`（帧缓冲）、`/dev/dri/card0`（图形设备）、`/dev/kbd`（非阻塞单键）、`/dev/vga`（ANSI 子集虚拟终端），定义见 `src/vfs.h`。

---

## 10. 图形与桌面

### 10.1 进入条件

`desktop` 要求 `vga_fb_active()` 为真，即 GRUB 已成功设置 VBE 帧缓冲（`bpp` 为 16/24/32）。文本模式下直接报：

```
framebuffer mode required (VESA VBE not active)
```

### 10.2 启动信息与协议

成功后依次打印：

```
cunos X11 server
transport COM1 @ 115200 8N1
protocol X11 core subset depth 24 TrueColor
Ctrl+Q returns to shell
```

含义：GUI 通过 **COM1（0x3F8）串口**承载 X11 core 协议子集，客户程序跑在宿主侧，画到内核帧缓冲。`src/x11.c` 实现的请求集合包括 `create_window`、`map_window`、`configure_window`、`get_geometry`、`query_tree`、`intern_atom`、`change_property`、`create_gc`、`draw_rects`、`draw_lines`、`draw_arcs`、`draw_points`、`fill_poly`、`clear_area`、`alloc_color`、`image_text`、`poly_text`、`get_image`、`copy_area`、`create_pixmap`、`free_pixmap`、`query_pointer`、`translate_coords`、`query_keymap`、`get_modifier_mapping`、`query_extension` 等。

### 10.3 窗口操作

| 操作 | 方式 |
| --- | --- |
| 拖动窗口 | 按住标题栏拖动 |
| 最小化 | 标题栏 `-` |
| 最大化 | 标题栏方框按钮 |
| 关闭 | 标题栏 `x` |
| 返回 shell | `Ctrl+Q` |

桌面上可直接在窗口内终端输入 shell 命令，走的是同一个 `kernel_exec_line()` 解释器（源码注释：「桌面终端等界面复用的命令解释入口」）。桌面状态栏会显示任务项与版本串 `cunos 0.3.0`。

### 10.4 图像自检与截图

- `drm`：对 `/dev/fb0` 与 `/dev/dri/card0` 做 ioctl 自检，验证帧缓冲几何参数可用。
- 宿主侧截图：`tools/qshot.py`、`tools/qsd*.py` 通过 QEMU QMP 抓取帧缓冲并转 PNG（转换脚本 `tools/convert_ppm.py`）。

### 10.5 KDE 探测

`kde` 不是桌面入口，它只做只读检查：挂载 `-hdb` 的 ext2 卷，列 `/usr/bin` 前 10 个条目，并对 `/usr/bin/plasmashell`、`/usr/bin/kwin_x11`、`/lib/i386-linux-gnu/libc.so.6`、`/lib/i386-linux-gnu/ld-linux.so.2`、`/lib/ld-linux.so.2`、`/usr/bin/yes` 逐个打印 ELF 头信息，最后打印 `ext2 rootfs readable (KDE stage 1)`。**真正的 KDE 桌面在 cunos 上未实现**。

---

## 11. 网络配置与测试

### 11.1 默认参数

`src/net.c` 中默认值：

| 项 | 值 |
| --- | --- |
| IP | `0x0A00020F` = 10.0.2.15 |
| 掩码 | `0xFFFFFF00` = 255.255.255.0 |
| 网关 | `0x0A000202` = 10.0.2.2 |

这正是 QEMU 用户态网络（slirp）的默认网段，因此 QEMU 下开箱可用。

### 11.2 查看

```
net
ifconfig
```

`net_dump()` 输出：驱动、探测信息、IP、网关、掩码、MAC（6 字节十六进制）、ARP 缓存（最多 8 项）、计数（`arp=`、`ip=`、`icmp=`、`udp=`、`tcp=`、`drop=`）。

### 11.3 设置

```
ifconfig 10.0.2.20 255.255.255.0 10.0.2.2
ifconfig 10.0.2.20            # 掩码默认 255.255.255.0
ifconfig 10.0.2.20 255.255.255.0   # 网关默认 (ip & mask) | 1
```

设置成功会清空 ARP 缓存、重新解析网关并打印 `ifconfig: applied`。**没有 DHCP 客户端**（源码中不存在 DHCP 实现）。

### 11.4 连通性测试

```
ping
ping 10.0.2.2
tcp 10.0.2.2 80
curl http://10.0.2.2/
chat 10.0.2.2 5555
```

`chat` 需要宿主侧先监听，例如在宿主执行 `python tools/tcp_echo.py`（端口见脚本内容）或 `nc -l 5555`。聊天时逐行输入，`/quit`、`/exit` 或 `ESC` 退出。

### 11.5 支持的网卡

`net_init()` 按内核内置驱动探测：`e1000`、`rtl8139`、`rtl8169`、`ne2k`（对应 `src/e1000.c`、`src/rtl8139.c`、`src/rtl8169.c`、`src/ne2k.c`）。**未列出的网卡一律不支持**：没有 virtio-net、没有 USB 网卡、没有无线网卡驱动。`pci` 命令可以确认设备是否被枚举到。

---

## 12. 常见操作流程

### 12.1 建目录、复制、运行程序

```
fs                      # 确认当前卷
mkdir demo              # 建目录
cd demo                 # 进入
pwd                     # 校验工作目录
copy /bin/ver.elf .     # 把程序复制进当前目录
run ver.elf             # 相对路径运行
run /bin/ver.elf        # 绝对路径运行（需 rootfs）
```

注意 `run ver.elf` 走当前 FAT 卷；`run /bin/ver.elf` 走 ext2 rootfs 并启用 `PT_INTERP`/PIE 支持。

### 12.2 写一个文本文件并编辑

```
enter note.txt
第一行内容
第二行内容
.
cat note.txt
edit note.txt
```

`enter` 结束符是单独一行 `.`，缓冲上限 4096 字节。

### 12.3 挂载 rootfs 后浏览 ext2 目录

```
root
ls /
ls /bin
ino /usr/bin/plasmashell
kde
```

### 12.4 安装到磁盘

1. QEMU/真机以**安装介质**启动（CD 或已装好的盘），并确保目标 ATA 盘作为第一块盘、安装介质作为第二块盘（`-hda` 目标、`-hdb` 介质）。
2. 执行 `cuninstall`，按提示输入 `YES`、`GRUB`。
3. 等待 `[6/6]` 完成，执行 `fs` 确认新卷，`ls /bin` 应有全部程序。
4. 重启（`reboot`），从目标盘引导，GRUB 菜单选 `cunos (installed on disk)`。

### 12.5 联网自检

```
pci            # 看网卡 vendor:device 是否被识别
net            # 看 driver 与 link
ping 10.0.2.2
tcp 10.0.2.2 80
```

若 `net` 显示驱动为空、链路 down：先用 `pci` 确认设备存在，再对照实体机配置参考的「支持与不支持硬件矩阵」。

### 12.6 抓一张图形界面截图

宿主执行（WSL 内）：

```
python3 tools/qshot.py
```

脚本通过 QMP 在 cunos 运行时抓帧缓冲，产物为 PPM，再用 `tools/convert_ppm.py` 转 PNG。

---

## 13. 已知限制速查

| 能力 | 现状 | 依据 |
| --- | --- | --- |
| 多任务 | 有进程与 fork/execve/wait4（`/bin` 程序可用），但**调度为协作式**，无抢占、无 SMP | `help_planned[]`、`src/proc.c` |
| 管道 / 重定向 / 通配符 | 未实现 | `split_args()` 仅空格切分 |
| 命令历史 / Tab 补全 | 未实现 | `readline()` 忽略方向键 |
| 信号 / IPC | 未实现（`pipe`、`futex`、`shm`、`sigaction` 全无） | `help_planned[]` |
| 权限 / 用户 | 未实现（无 uid/gid 检查，无登录） | `help_planned[]` |
| `/proc`、`/sys`、`udev` | 未实现 | `help_planned[]` |
| ext2 写入 / 删除 | 未实现（ext2 仅读；写走 FAT） | `cmd_ino()`、`del` 说明 |
| 长文件名（LFN） | 未实现（8.3 短名） | `src/fat.h` `FAT_NAME_LEN 13` |
| 文件大于 4096 字节的读取 | `cat` 只显示前 4096 字节 | `cat` 帮助 note |
| 域名解析 / DHCP | 未实现（`curl` 与 `ping` 只接受点分 IPv4） | `src/net.c` |
| USB 存储 / HID | 未实现（只枚举控制器与设备，不提供驱动） | `cmd_usb()`、`src/usb.c` |
| 声音 | 未实现（源码中无音频子系统） | `src/` 目录 |
| KDE 桌面 | 未实现（`kde` 只是只读探测） | `help_table` note |
| 编辑器内中文输入 | 未实现（只接受 ASCII 32–126） | `src/editor.c` |

---

## 14. 一页速查

```
# 帮助
help                    帮助全表
help network            单个分组
help ls                 单条命令

# 语言与输入法
lang zh                 界面中文
ime zh                  拼音输入法
Ctrl+Space              英中切换

# 目录与文件
ls -a /                 列根目录（含 . / ..）
cd /bin                 切换目录
pwd                     当前目录
mkdir demo              建目录
enter a.txt             写文本，单独一行 . 结束
cat a.txt               查看（前 4096 字节）
edit a.txt              ^O 保存，^X 退出
copy a.txt /bin         复制
del a.txt               删除

# 卷与磁盘
fs                      卷与磁盘信息
fs 1                    切换到卷 1
root                    挂载 -hdb 的 ext2 为 /
mkfat32                 格式化 ATA 盘为 FAT32
cuninstall              安装到 ATA 盘（YES / GRUB 双确认）
vfs                     看 fd 表

# 运行程序
run /bin/ver.elf        绝对路径（走 ext2）
tetris                  直接敲名字即可

# 图形
desktop                 进入桌面，Ctrl+Q 返回
drm                     帧缓冲自检

# 网络
net / ifconfig          查看
ifconfig 10.0.2.20 255.255.255.0 10.0.2.2
ping 10.0.2.2
tcp 10.0.2.2 80
chat 10.0.2.2 5555

# 关机与重启
shutdown                ACPI S5
reboot                  复位
```
