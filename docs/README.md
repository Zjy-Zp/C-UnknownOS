---
AIGC:
    Label: "1"
    ContentProducer: 001191440300708461136T1XGW3
    ProduceID: 0a49692c9e8a806948f690d4834b2457_10ea6237e56511f184bd52540072b1e8
    ReservedCode1: kj8Jv1Q546dysH3y0HQ5wOJQTPAgTseflZvXXtU2ECMTfWzQsEc6U2vIB8uXqQwNKpuiPS4FONNAermcvSaw5tZnNUZ0q9Odt2L9PYvT6pQAHSsEoZ9xxzcJ4JQWNX3MH9hmNjB6GYvyYtDxTJOWkSJ9owx7k/smg2GqW6CtzyHGscL7kvX4+fc+9bs=
    ContentPropagator: 001191440300708461136T1XGW3
    PropagateID: 0a49692c9e8a806948f690d4834b2457_10ea6237e56511f184bd52540072b1e8
    ReservedCode2: kj8Jv1Q546dysH3y0HQ5wOJQTPAgTseflZvXXtU2ECMTfWzQsEc6U2vIB8uXqQwNKpuiPS4FONNAermcvSaw5tZnNUZ0q9Odt2L9PYvT6pQAHSsEoZ9xxzcJ4JQWNX3MH9hmNjB6GYvyYtDxTJOWkSJ9owx7k/smg2GqW6CtzyHGscL7kvX4+fc+9bs=
---

# cunos 文档站

cunos 操作系统内核的文档站，基于 [VitePress](https://vitepress.dev) 构建。

## 本地启动

```bash
npm install
npm run dev          # 开发预览 http://localhost:5173
npm run build        # 构建静态站点到 .vitepress/dist
npm run preview      # 预览构建产物
```

## 目录结构

```
cunos-docs/
├── .vitepress/
│   ├── config.mts          导航、侧边栏、搜索、站点元信息
│   └── theme/
│       ├── index.ts        继承默认主题
│       └── custom.css      品牌色与中文字体微调
├── guide/                  入门指南（跑起来 → 工具链 → 验证）
├── arch/                   内核架构（引导 / 内存 / 中断 / 驱动 / FS / 网络 / 图形）
├── dev/                    开发手册（规范 / 调试 / 提交约定）
├── reference/              命令速查与术语表
├── public/                 静态资源（favicon 等，构建时原样复制）
└── index.md                首页
```

## 新增页面

1. 在对应目录下新建 `.md` 文件
2. 在 `.vitepress/config.mts` 的 `sidebar` 中登记链接
3. 页面顶部用 `# 一级标题` 作为标题

```markdown
# 页面标题

正文……

## 二级标题
```

## 写作约定

- 每个技术页面结尾附「验证清单」，列出可观测的成功判据
- 命令一律可在 WSL Debian 下直接复制执行
- 标注「规划中」的章节表示设计已定、代码未落地
- 与内核代码同一提交内同步更新文档

## 部署

### GitHub Pages

1. 推送到 GitHub 仓库
2. 若仓库名为 `cunos-docs`（非 `<user>.github.io`），需把 `.vitepress/config.mts` 中的 `base` 改为 `'/cunos-docs/'`
3. 添加 `.github/workflows/deploy.yml`：

```yaml
name: Deploy VitePress

on:
  push:
    branches: [main]

permissions:
  contents: read
  pages: write
  id-token: write

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 22
      - run: npm ci || npm install
      - run: npm run build
      - uses: actions/upload-pages-artifact@v3
        with:
          path: .vitepress/dist

  deploy:
    needs: build
    runs-on: ubuntu-latest
    environment:
      name: github-pages
    steps:
      - uses: actions/deploy-pages@v4
```

### Cloudflare Pages

| 配置项 | 值 |
|---|---|
| 构建命令 | `npm run build` |
| 输出目录 | `.vitepress/dist` |
| Node 版本 | 22 |

绑定自定义域名后自动签发 HTTPS 证书。

## 搜索

当前使用 VitePress 内置本地搜索（`themeConfig.search.provider = 'local'`），构建时生成索引，无需外部服务。站内内容量增大后若中文分词效果不足，可切换 Algolia DocSearch。
*（内容由AI生成，仅供参考）*
