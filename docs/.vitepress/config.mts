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
      { text: '架构', link: '/arch/', activeMatch: '/arch/' },
      { text: '开发', link: '/dev/', activeMatch: '/dev/' },
      { text: '参考', link: '/reference/', activeMatch: '/reference/' }
    ],

    sidebar: {
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
