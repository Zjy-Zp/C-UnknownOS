import { defineConfig } from 'vitepress'

// 部署路径由环境变量 DOCS_BASE 控制：
//   GitHub Pages 子路径部署（https://user.github.io/<repo>/）→ 由 Action 自动注入 '/<repo>/'
//   本地预览、独立域名、<user>.github.io 仓库 → 默认 '/'
const base = process.env.DOCS_BASE || '/'

export default defineConfig({
  lang: 'zh-CN',
  title: 'cunos',
  description: 'cunos —— 32 位 i386 保护模式自研操作系统内核 · 设计、实现与验证文档',
  base,
  cleanUrls: true,
  lastUpdated: true,
  head: [
    ['link', { rel: 'icon', type: 'image/svg+xml', href: `${base}favicon.svg` }]
  ],

  markdown: {
    languageAlias: {
      // shiki 未内置 NASM / 链接脚本语法，映射到最接近的高亮器
      nasm: 'asm',
      ld: 'ini'
    }
  },

  themeConfig: {
    logo: '/cunos-mark.svg',

    nav: [
      { text: '指南', link: '/guide/', activeMatch: '/guide/' },
      { text: '手册', link: '/manual/', activeMatch: '/manual/' },
      { text: '下载', link: '/download/', activeMatch: '/download/' },
      { text: '架构', link: '/arch/', activeMatch: '/arch/' },
      { text: '开发', link: '/dev/', activeMatch: '/dev/' },
      { text: 'VMM', link: '/vmm/', activeMatch: '/vmm/' },
      { text: '参考', link: '/reference/', activeMatch: '/reference/' }
    ],

    sidebar: {
      '/manual/': [
        {
          text: '手册总览',
          items: [
            { text: '四份手册与阅读顺序', link: '/manual/' }
          ]
        },
        {
          text: '操作指南',
          collapsed: true,
          items: [
            { text: '本篇概览', link: '/manual/user-guide' },
            { text: '1. 版本与术语约定', link: '/manual/user-guide#_1-版本与术语约定' },
            { text: '2. 启动与进入 shell', link: '/manual/user-guide#_2-启动与进入-shell' },
            { text: '3. 终端提示符与路径语义', link: '/manual/user-guide#_3-终端提示符与路径语义' },
            { text: '4. 命令行输入规则', link: '/manual/user-guide#_4-命令行输入规则' },
            { text: '5. shell 命令全表', link: '/manual/user-guide#_5-shell-命令全表' },
            { text: '6. `/bin` 程序清单', link: '/manual/user-guide#_6-bin-程序清单' },
            { text: '7. 编辑器使用', link: '/manual/user-guide#_7-编辑器使用' },
            { text: '8. 输入法（IME）', link: '/manual/user-guide#_8-输入法-ime' },
            { text: '9. 文件系统与磁盘操作', link: '/manual/user-guide#_9-文件系统与磁盘操作' },
            { text: '10. 图形与桌面', link: '/manual/user-guide#_10-图形与桌面' },
            { text: '11. 网络配置与测试', link: '/manual/user-guide#_11-网络配置与测试' },
            { text: '12. 常见操作流程', link: '/manual/user-guide#_12-常见操作流程' },
            { text: '13. 已知限制速查', link: '/manual/user-guide#_13-已知限制速查' },
            { text: '14. 一页速查', link: '/manual/user-guide#_14-一页速查' }
          ]
        },
        {
          text: '架构指南',
          collapsed: true,
          items: [
            { text: '本篇概览', link: '/manual/architecture' },
            { text: '1. 代码地图', link: '/manual/architecture#_1-代码地图' },
            { text: '2. 启动链路', link: '/manual/architecture#_2-启动链路' },
            { text: '3. 特权级与中断底座', link: '/manual/architecture#_3-特权级与中断底座' },
            { text: '4. 分页与内存管理', link: '/manual/architecture#_4-分页与内存管理' },
            { text: '5. 进程与调度', link: '/manual/architecture#_5-进程与调度' },
            { text: '6. ELF 装载', link: '/manual/architecture#_6-elf-装载' },
            { text: '7. 系统调用分发', link: '/manual/architecture#_7-系统调用分发' },
            { text: '8. VFS 与文件系统', link: '/manual/architecture#_8-vfs-与文件系统' },
            { text: '9. 块层', link: '/manual/architecture#_9-块层' },
            { text: '10. 网络栈', link: '/manual/architecture#_10-网络栈' },
            { text: '11. 图形栈', link: '/manual/architecture#_11-图形栈' },
            { text: '12. 输入栈', link: '/manual/architecture#_12-输入栈' },
            { text: '13. ACPI / PCI / USB', link: '/manual/architecture#_13-acpi-pci-usb' },
            { text: '14. 安装器与内嵌载荷', link: '/manual/architecture#_14-安装器与内嵌载荷' },
            { text: '15. 未实现能力汇总（架构层面）', link: '/manual/architecture#_15-未实现能力汇总-架构层面' },
            { text: '16. 源码索引表', link: '/manual/architecture#_16-源码索引表' }
          ]
        },
        {
          text: '开发指南',
          collapsed: true,
          items: [
            { text: '本篇概览', link: '/manual/developer' },
            { text: '1. 环境与工具链', link: '/manual/developer#_1-环境与工具链' },
            { text: '2. 构建系统', link: '/manual/developer#_2-构建系统' },
            { text: '3. 仓库地图与阅读顺序', link: '/manual/developer#_3-仓库地图与阅读顺序' },
            { text: '4. 加一条内核内建命令（完整流程）', link: '/manual/developer#_4-加一条内核内建命令-完整流程' },
            { text: '5. 加一个 `/bin` 用户态程序', link: '/manual/developer#_5-加一个-bin-用户态程序' },
            { text: '6. 加一个网卡驱动', link: '/manual/developer#_6-加一个网卡驱动' },
            { text: '7. 加系统调用', link: '/manual/developer#_7-加系统调用' },
            { text: '8. 改文件系统 / 路径层', link: '/manual/developer#_8-改文件系统-路径层' },
            { text: '9. 改图形 / 桌面', link: '/manual/developer#_9-改图形-桌面' },
            { text: '10. 验证方法论（本项目硬要求）', link: '/manual/developer#_10-验证方法论-本项目硬要求' },
            { text: '11. 代码规范与约定（从现有代码反推）', link: '/manual/developer#_11-代码规范与约定-从现有代码反推' },
            { text: '12. 常见编译/运行故障', link: '/manual/developer#_12-常见编译-运行故障' },
            { text: '13. 提交前自检清单', link: '/manual/developer#_13-提交前自检清单' }
          ]
        },
        {
          text: '实体机配置参考',
          collapsed: true,
          items: [
            { text: '本篇概览', link: '/manual/hardware' },
            { text: '1. 最低可运行配置', link: '/manual/hardware#_1-最低可运行配置' },
            { text: '2. 引导方式与固件设置', link: '/manual/hardware#_2-引导方式与固件设置' },
            { text: '3. 安装到硬盘的两种方式', link: '/manual/hardware#_3-安装到硬盘的两种方式' },
            { text: '4. 输入设备：实机的最大不确定性', link: '/manual/hardware#_4-输入设备-实机的最大不确定性' },
            { text: '5. 网卡支持清单', link: '/manual/hardware#_5-网卡支持清单' },
            { text: '6. 存储控制器', link: '/manual/hardware#_6-存储控制器' },
            { text: '7. 显示与图形', link: '/manual/hardware#_7-显示与图形' },
            { text: '8. 实机排障表', link: '/manual/hardware#_8-实机排障表' },
            { text: '9. 实机与 QEMU 的差异清单', link: '/manual/hardware#_9-实机与-qemu-的差异清单' },
            { text: '10. 上机检查单（照着走）', link: '/manual/hardware#_10-上机检查单-照着走' },
            { text: '11. 明确不支持（别浪费时间试）', link: '/manual/hardware#_11-明确不支持-别浪费时间试' }
          ]
        }
      ],
      '/download/': [
        {
          text: '下载',
          items: [
            { text: '最新版本', link: '/download/#最新版本' },
            { text: '历史版本', link: '/download/#历史版本' },
            { text: '拿到 ISO 之后', link: '/download/#拿到-iso-之后' }
          ]
        }
      ],
      '/guide/': [
        {
          text: '入门',
          items: [
            { text: '总览', link: '/guide/' },
            { text: '五分钟跑起来', link: '/guide/getting-started' },
            { text: '工具链与交叉编译', link: '/guide/toolchain' },
            { text: 'QEMU 无头验证', link: '/guide/qemu-verify' }
          ]
        }
      ],
      '/arch/': [
        {
          text: '内核架构',
          items: [
            { text: '分层与子项目', link: '/arch/' },
            { text: '引导与进入保护模式', link: '/arch/boot' },
            { text: '分页与内存管理', link: '/arch/memory' },
            { text: '中断、异常与系统调用', link: '/arch/interrupt' }
          ]
        },
        {
          text: '子系统',
          items: [
            { text: '驱动模型（PCI/ATA/AHCI/USB）', link: '/arch/drivers' },
            { text: '文件系统与 VFS', link: '/arch/vfs' },
            { text: '网络栈', link: '/arch/net' },
            { text: '帧缓冲与图形桌面', link: '/arch/graphics' }
          ]
        }
      ],
      '/dev/': [
        {
          text: '开发手册',
          items: [
            { text: '编码规范', link: '/dev/' },
            { text: '调试手段', link: '/dev/debug' },
            { text: '提交与分支约定', link: '/dev/git' }
          ]
        }
      ],
      '/reference/': [
        {
          text: '参考',
          items: [
            { text: '构建命令速查', link: '/reference/' },
            { text: '术语表', link: '/reference/glossary' }
          ]
        }
      ],
      '/vmm/': [
        {
          text: '第 1 章 文档定位、读者与术语',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch01' },
            { text: '1.1 本文要解决的问题', link: '/vmm/ch01#_1-1-本文要解决的问题' },
            { text: '1.2 读者与前置知识', link: '/vmm/ch01#_1-2-读者与前置知识' },
            { text: '1.3 术语表', link: '/vmm/ch01#_1-3-术语表' },
            { text: '1.4 两个写作约定', link: '/vmm/ch01#_1-4-两个写作约定' }
          ]
        },
        {
          text: '第 2 章 Type-1 与 Type-2：cunos 的 VMM 到底算哪一类',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch02' },
            { text: '2.1 三条判据', link: '/vmm/ch02#_2-1-三条判据' },
            { text: '2.2 逐个打分', link: '/vmm/ch02#_2-2-逐个打分' },
            { text: '2.3 与 KVM / WSL2 / virtio 的关系', link: '/vmm/ch02#_2-3-与-kvm-wsl2-virtio-的关系' },
            { text: '2.4 栈位置对照', link: '/vmm/ch02#_2-4-栈位置对照' }
          ]
        },
        {
          text: '第 3 章 为什么 cunos 要做 VMM：三条路线的量化对比',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch03' },
            { text: '3.1 三条路线的定义', link: '/vmm/ch03#_3-1-三条路线的定义' },
            { text: '3.2 对比表', link: '/vmm/ch03#_3-2-对比表' },
            { text: '3.3 路线 A 的代价必须提前承认', link: '/vmm/ch03#_3-3-路线-a-的代价必须提前承认' }
          ]
        },
        {
          text: '第 4 章 AMD-V（SVM）硬件机制详解',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch04' },
            { text: '4.1 为什么 32 位保护模式宿主可以用 SVM', link: '/vmm/ch04#_4-1-为什么-32-位保护模式宿主可以用-svm' },
            { text: '4.2 探测：CPUID', link: '/vmm/ch04#_4-2-探测-cpuid' },
            { text: '4.3 启用：EFER.SVME 与 VM_CR', link: '/vmm/ch04#_4-3-启用-efer-svme-与-vm-cr' },
            { text: '4.4 HSAVE 页', link: '/vmm/ch04#_4-4-hsave-页' },
            { text: '4.5 VMCB 内存布局', link: '/vmm/ch04#_4-5-vmcb-内存布局' },
            { text: '4.6 八条 SVM 指令', link: '/vmm/ch04#_4-6-八条-svm-指令' },
            { text: '4.7 GIF：一个比想象中更关键的开关', link: '/vmm/ch04#_4-7-gif-一个比想象中更关键的开关' },
            { text: '4.8 ASID 与 clean/dirty 位', link: '/vmm/ch04#_4-8-asid-与-clean-dirty-位' },
            { text: '4.9 一张图总结 SVM 的执行流', link: '/vmm/ch04#_4-9-一张图总结-svm-的执行流' }
          ]
        },
        {
          text: '第 5 章 #VMEXIT 机制与退出码表',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch05' },
            { text: '5.1 一次退出的完整过程', link: '/vmm/ch05#_5-1-一次退出的完整过程' },
            { text: '5.2 exit_code 的编码', link: '/vmm/ch05#_5-2-exit-code-的编码' },
            { text: '5.3 EXITINFO1 / EXITINFO2 的语义', link: '/vmm/ch05#_5-3-exitinfo1-exitinfo2-的语义' },
            { text: '5.4 退出码表', link: '/vmm/ch05#_5-4-退出码表' },
            { text: '5.5 退出次数预算：为什么这决定了架构', link: '/vmm/ch05#_5-5-退出次数预算-为什么这决定了架构' },
            { text: '5.6 与 cunos 现有异常框架的衔接', link: '/vmm/ch05#_5-6-与-cunos-现有异常框架的衔接' }
          ]
        },
        {
          text: '第 6 章 NPT 嵌套分页',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch06' },
            { text: '6.1 为什么必须用 NPT', link: '/vmm/ch06#_6-1-为什么必须用-npt' },
            { text: '6.2 NPT 表项格式与层级', link: '/vmm/ch06#_6-2-npt-表项格式与层级' },
            { text: '6.3 NPT 表在 cunos 里的存放位置', link: '/vmm/ch06#_6-3-npt-表在-cunos-里的存放位置' },
            { text: '6.4 NPF 退出与 EXITINFO1 位域', link: '/vmm/ch06#_6-4-npf-退出与-exitinfo1-位域' },
            { text: '6.5 大页', link: '/vmm/ch06#_6-5-大页' },
            { text: '6.6 TLB 与 INVLPGA', link: '/vmm/ch06#_6-6-tlb-与-invlpga' },
            { text: '6.7 NPT 的建立与 cunos 内存布局的衔接', link: '/vmm/ch06#_6-7-npt-的建立与-cunos-内存布局的衔接' },
            { text: '6.8 宿主如何访问 guest 内存', link: '/vmm/ch06#_6-8-宿主如何访问-guest-内存' },
            { text: '6.9 一个容易忽略的细节：AMO 与跨页访问', link: '/vmm/ch06#_6-9-一个容易忽略的细节-amo-与跨页访问' }
          ]
        },
        {
          text: '第 7 章 cunos 现有地基盘点',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch07' },
            { text: '7.1 逐模块体检', link: '/vmm/ch07#_7-1-逐模块体检' },
            { text: '7.2 三个必须提前解决的"地基裂缝"', link: '/vmm/ch07#_7-2-三个必须提前解决的-地基裂缝' }
          ]
        },
        {
          text: '第 8 章 总体架构与模块划分',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch08' },
            { text: '8.1 目录结构', link: '/vmm/ch08#_8-1-目录结构' },
            { text: '8.2 模块职责与依赖方向', link: '/vmm/ch08#_8-2-模块职责与依赖方向' },
            { text: '8.3 调用关系', link: '/vmm/ch08#_8-3-调用关系' },
            { text: '8.4 宿主执行体模型：没有内核线程时怎么办', link: '/vmm/ch08#_8-4-宿主执行体模型-没有内核线程时怎么办' },
            { text: '8.5 两个窗口、两个池：选址的完整推导', link: '/vmm/ch08#_8-5-两个窗口、两个池-选址的完整推导' },
            { text: '8.6 代码量估算（用于第 21 章的里程碑拆分）', link: '/vmm/ch08#_8-6-代码量估算-用于第-21-章的里程碑拆分' }
          ]
        },
        {
          text: '第 9 章 核心数据结构',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch09' },
            { text: '9.1 `vdev_t`：设备模型统一接口', link: '/vmm/ch09#_9-1-vdev-t-设备模型统一接口' },
            { text: '9.2 `npt_t`', link: '/vmm/ch09#_9-2-npt-t' },
            { text: '9.3 `vcpu_t`', link: '/vmm/ch09#_9-3-vcpu-t' },
            { text: '9.4 `vm_t`', link: '/vmm/ch09#_9-4-vm-t' },
            { text: '9.5 VMCB 访问器：把偏移表变成 API', link: '/vmm/ch09#_9-5-vmcb-访问器-把偏移表变成-api' }
          ]
        },
        {
          text: '第 10 章 初始化流水线',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch10' },
            { text: '10.1 流水线总览', link: '/vmm/ch10#_10-1-流水线总览' },
            { text: '10.2 每一步的失败分支', link: '/vmm/ch10#_10-2-每一步的失败分支' },
            { text: '10.3 `vm_create()` 实现', link: '/vmm/ch10#_10-3-vm-create-实现' },
            { text: '10.4 guest 初始上下文：把 16 位实模式世界"造"出来', link: '/vmm/ch10#_10-4-guest-初始上下文-把-16-位实模式世界-造-出来' },
            { text: '10.5 实模式 guest 起点的三个坑', link: '/vmm/ch10#_10-5-实模式-guest-起点的三个坑' }
          ]
        },
        {
          text: '第 11 章 阶段 1：最小闭环的完整代码骨架',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch11' },
            { text: '11.1 guest：16 位引导扇区（汇编源）', link: '/vmm/ch11#_11-1-guest-16-位引导扇区-汇编源' },
            { text: '11.2 宿主：把 guest 装进 RAM 并跑起来（约 40 行）', link: '/vmm/ch11#_11-2-宿主-把-guest-装进-ram-并跑起来-约-40-行' },
            { text: '11.3 五分钟实验：确认 NPT 的层级假设', link: '/vmm/ch11#_11-3-五分钟实验-确认-npt-的层级假设' },
            { text: '11.4 VMRUN 失败检测', link: '/vmm/ch11#_11-4-vmrun-失败检测' },
            { text: '11.5 主循环完整骨架', link: '/vmm/ch11#_11-5-主循环完整骨架' },
            { text: '11.6 阶段 1 验收标准', link: '/vmm/ch11#_11-6-阶段-1-验收标准' },
            { text: '11.7 失败现象 → 病因对照表', link: '/vmm/ch11#_11-7-失败现象-→-病因对照表' }
          ]
        },
        {
          text: '第 12 章 #VMEXIT 分发器与五大退出码',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch12' },
            { text: '12.1 分发表', link: '/vmm/ch12#_12-1-分发表' },
            { text: '12.2 CPUID（`0x06A`）', link: '/vmm/ch12#_12-2-cpuid-0x06a' },
            { text: '12.3 HLT（`0x070`）', link: '/vmm/ch12#_12-3-hlt-0x070' },
            { text: '12.4 IOIO（`0x073`）', link: '/vmm/ch12#_12-4-ioio-0x073' },
            { text: '12.5 MSR（`0x074`）', link: '/vmm/ch12#_12-5-msr-0x074' },
            { text: '12.6 SHUTDOWN（`0x077`）', link: '/vmm/ch12#_12-6-shutdown-0x077' },
            { text: '12.7 两个额外退出码', link: '/vmm/ch12#_12-7-两个额外退出码' }
          ]
        },
        {
          text: '第 13 章 设备模型',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch13' },
            { text: '13.1 统一约定与资源分配表', link: '/vmm/ch13#_13-1-统一约定与资源分配表' },
            { text: '13.2 8259A 可编程中断控制器', link: '/vmm/ch13#_13-2-8259a-可编程中断控制器' },
            { text: '13.3 8254 可编程间隔定时器', link: '/vmm/ch13#_13-3-8254-可编程间隔定时器' },
            { text: '13.4 8250 UART', link: '/vmm/ch13#_13-4-8250-uart' },
            { text: '13.5 PCI 配置空间（`0xCF8` / `0xCFC`）', link: '/vmm/ch13#_13-5-pci-配置空间-0xcf8-0xcfc' },
            { text: '13.6 块设备：ATA 模拟 vs virtio-blk', link: '/vmm/ch13#_13-6-块设备-ata-模拟-vs-virtio-blk' },
            { text: '13.7 VGA / VBE 帧缓冲', link: '/vmm/ch13#_13-7-vga-vbe-帧缓冲' },
            { text: '13.8 8042 键盘控制器', link: '/vmm/ch13#_13-8-8042-键盘控制器' },
            { text: '13.9 两条实现路径的取舍（完整 PC 模拟 vs virtio 半虚拟化）', link: '/vmm/ch13#_13-9-两条实现路径的取舍-完整-pc-模拟-vs-virtio-半虚拟化' },
            { text: '13.10 设备注册与冲突检测', link: '/vmm/ch13#_13-10-设备注册与冲突检测' }
          ]
        },
        {
          text: '第 14 章 客户机物理地址空间布局',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch14' },
            { text: '14.1 布局表', link: '/vmm/ch14#_14-1-布局表' },
            { text: '14.2 布局的三条设计约束', link: '/vmm/ch14#_14-2-布局的三条设计约束' },
            { text: '14.3 给 guest 的 E820 内存图', link: '/vmm/ch14#_14-3-给-guest-的-e820-内存图' }
          ]
        },
        {
          text: '第 15 章 客户机引导方案：SeaBIOS 还是自写直引导 loader',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch15' },
            { text: '15.1 为什么"引导"是最容易低估的一步', link: '/vmm/ch15#_15-1-为什么-引导-是最容易低估的一步' },
            { text: '15.2 路线 A：移植 SeaBIOS', link: '/vmm/ch15#_15-2-路线-a-移植-seabios' },
            { text: '15.3 路线 B：自写 32 位直引导 loader（跳过实模式）', link: '/vmm/ch15#_15-3-路线-b-自写-32-位直引导-loader-跳过实模式' },
            { text: '15.4 直引导的真实难点：内核解压', link: '/vmm/ch15#_15-4-直引导的真实难点-内核解压' },
            { text: '15.5 不硬编码偏移：用 magic 扫描解析 setup header', link: '/vmm/ch15#_15-5-不硬编码偏移-用-magic-扫描解析-setup-header' },
            { text: '15.6 两条路线的最终对比', link: '/vmm/ch15#_15-6-两条路线的最终对比' }
          ]
        },
        {
          text: '第 16 章 Debian i386 的现实约束',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch16' },
            { text: '16.1 内核 flavour 与 PAE', link: '/vmm/ch16#_16-1-内核-flavour-与-pae' },
            { text: '16.2 initrd 是绕不过去的', link: '/vmm/ch16#_16-2-initrd-是绕不过去的' },
            { text: '16.3 rootfs 复用 ext2 镜像', link: '/vmm/ch16#_16-3-rootfs-复用-ext2-镜像' },
            { text: '16.4 内存预算：128MB 够不够', link: '/vmm/ch16#_16-4-内存预算-128mb-够不够' },
            { text: '16.5 内核命令行：最小可用集', link: '/vmm/ch16#_16-5-内核命令行-最小可用集' },
            { text: '16.6 三个"必然遇到"的失败点', link: '/vmm/ch16#_16-6-三个-必然遇到-的失败点' }
          ]
        },
        {
          text: '第 17 章 与 cunos 图形栈的集成',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch17' },
            { text: '17.1 先明确现状：cunos 目前没有图形栈', link: '/vmm/ch17#_17-1-先明确现状-cunos-目前没有图形栈' },
            { text: '17.2 guest 的画面怎么出来：不要用 VBE BIOS', link: '/vmm/ch17#_17-2-guest-的画面怎么出来-不要用-vbe-bios' },
            { text: '17.3 双缓冲与抖动规避', link: '/vmm/ch17#_17-3-双缓冲与抖动规避' },
            { text: '17.4 输入注入', link: '/vmm/ch17#_17-4-输入注入' },
            { text: '17.5 三种显示形态的取舍', link: '/vmm/ch17#_17-5-三种显示形态的取舍' }
          ]
        },
        {
          text: '第 18 章 调试与验证体系',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch18' },
            { text: '18.1 日志分级与格式', link: '/vmm/ch18#_18-1-日志分级与格式' },
            { text: '18.2 #VMEXIT 环形追踪', link: '/vmm/ch18#_18-2-vmexit-环形追踪' },
            { text: '18.3 退出码统计', link: '/vmm/ch18#_18-3-退出码统计' },
            { text: '18.4 VMCB dump', link: '/vmm/ch18#_18-4-vmcb-dump' },
            { text: '18.5 guest 内存与寄存器检查工具', link: '/vmm/ch18#_18-5-guest-内存与寄存器检查工具' },
            { text: '18.6 两条验证路径：QEMU 无头 vs 真机 U 盘', link: '/vmm/ch18#_18-6-两条验证路径-qemu-无头-vs-真机-u-盘' },
            { text: '18.7 三落（Triple Fault）的定位方法', link: '/vmm/ch18#_18-7-三落-triple-fault-的定位方法' },
            { text: '18.8 回归测试：用"退出次数"当断言', link: '/vmm/ch18#_18-8-回归测试-用-退出次数-当断言' },
            { text: '18.9 建议的验证节奏', link: '/vmm/ch18#_18-9-建议的验证节奏' }
          ]
        },
        {
          text: '第 19 章 性能预期与优化',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch19' },
            { text: '19.1 先测，再优化：一次进入/退出的成本', link: '/vmm/ch19#_19-1-先测-再优化-一次进入-退出的成本' },
            { text: '19.2 各阶段的退出预算', link: '/vmm/ch19#_19-2-各阶段的退出预算' },
            { text: '19.3 减少退出的五个手段', link: '/vmm/ch19#_19-3-减少退出的五个手段' },
            { text: '19.4 ASID 与 TLB', link: '/vmm/ch19#_19-4-asid-与-tlb' },
            { text: '19.5 抢占：cunos 协作式调度的硬约束', link: '/vmm/ch19#_19-5-抢占-cunos-协作式调度的硬约束' },
            { text: '19.6 AVIC / VAPIC：为什么阶段 5 也不做', link: '/vmm/ch19#_19-6-avic-vapic-为什么阶段-5-也不做' },
            { text: '19.7 串口才是阶段 3 的真瓶颈', link: '/vmm/ch19#_19-7-串口才是阶段-3-的真瓶颈' },
            { text: '19.8 性能优化的优先级排序', link: '/vmm/ch19#_19-8-性能优化的优先级排序' }
          ]
        },
        {
          text: '第 20 章 风险清单与缓解措施',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch20' }
          ]
        },
        {
          text: '第 21 章 分阶段里程碑',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch21' },
            { text: '21.1 阶段依赖图', link: '/vmm/ch21#_21-1-阶段依赖图' },
            { text: '21.2 里程碑表', link: '/vmm/ch21#_21-2-里程碑表' },
            { text: '21.3 每个阶段的"不许跳过"原则', link: '/vmm/ch21#_21-3-每个阶段的-不许跳过-原则' }
          ]
        },
        {
          text: '第 22 章 附录',
          collapsed: true,
          items: [
            { text: '本章概览', link: '/vmm/ch22' },
            { text: '22.1 附录 A：真机前置检查清单', link: '/vmm/ch22#_22-1-附录-a-真机前置检查清单' },
            { text: '22.2 附录 B：常用 MSR 速查表', link: '/vmm/ch22#_22-2-附录-b-常用-msr-速查表' },
            { text: '22.3 附录 C：VMCB 偏移速查（汇总，全部需核对）', link: '/vmm/ch22#_22-3-附录-c-vmcb-偏移速查-汇总-全部需核对' },
            { text: '22.4 附录 D：参考资料', link: '/vmm/ch22#_22-4-附录-d-参考资料' },
            { text: '22.5 附录 E：缩写与术语索引', link: '/vmm/ch22#_22-5-附录-e-缩写与术语索引' },
            { text: '22.6 附录 F：与 Intel VT-x 的对照（仅供理解，不实现）', link: '/vmm/ch22#_22-6-附录-f-与-intel-vt-x-的对照-仅供理解-不实现' },
            { text: '22.7 附录 G：全部"需核对"项汇总（交付前必须清零）', link: '/vmm/ch22#_22-7-附录-g-全部-需核对-项汇总-交付前必须清零' }
          ]
        }
      ]
    },

    search: {
      provider: 'local'
    },

    outline: { level: [2, 3], label: '本页导航' },
    docFooter: { prev: '上一篇', next: '下一篇' },
    lastUpdated: { text: '最后更新于' },
    returnToTopLabel: '回到顶部',
    sidebarMenuLabel: '目录',
    darkModeSwitchLabel: '主题',
    externalLinkIcon: true,

    footer: {
      message: 'cunos · 32 位 i386 保护模式内核',
      copyright: '教学 / 实验性质项目'
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/' }
    ]
  }
})
