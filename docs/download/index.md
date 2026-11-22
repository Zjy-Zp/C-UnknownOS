---
title: 下载
---

<script setup>
import { computed } from 'vue'
import { data } from '../.vitepress/data/releases.data.mts'

const releases = computed(() => data.releases || [])
const latest = computed(() => releases.value[0] || null)
const history = computed(() => releases.value.slice(1))

function pickMain(release) {
  if (!release) return null
  const iso = release.assets.filter((a) => /\.iso$/i.test(a.name))
  const pool = iso.length ? iso : release.assets
  return pool.find((a) => /^cunos\.iso$/i.test(a.name)) || pool[0] || null
}

function others(release) {
  if (!release) return []
  const main = pickMain(release)
  return release.assets.filter((a) => !main || a.name !== main.name)
}

function fmtSize(bytes) {
  if (!bytes && bytes !== 0) return '—'
  const mb = bytes / 1024 / 1024
  return mb >= 1 ? `${mb.toFixed(1)} MB` : `${(bytes / 1024).toFixed(0)} KB`
}

function fmtDate(iso) {
  if (!iso) return '—'
  const d = new Date(iso)
  const p = (n) => String(n).padStart(2, '0')
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}`
}

const latestMain = computed(() => pickMain(latest.value))
</script>

# 下载 cunos

页面数据在每次站点构建时从 GitHub Releases 自动抓取，发布新版本后无需修改文档，重新部署即可同步。

<div v-if="data.error" class="dl-warn">

构建时未能获取 Releases 数据（{{ data.error }}）。请直接前往 [GitHub Releases](https://github.com/Zjy-Zp/C-UnknownOS/releases) 查看与下载，稍后重新部署即可恢复本页列表。

</div>

<div v-else-if="!latest" class="dl-warn">

仓库当前还没有已发布的版本。可先 [查看 Releases 页面](https://github.com/Zjy-Zp/C-UnknownOS/releases) 获取最新动态。

</div>

<div v-else>

## 最新版本

<div class="dl-hero">
  <div class="dl-hero-main">
    <div class="dl-ver">
      {{ latest.name }}
      <span v-if="latest.prerelease" class="dl-badge">预发布</span>
    </div>
    <div class="dl-meta">标签 <code>{{ latest.tag }}</code> · 发布于 {{ fmtDate(latest.publishedAt) }}</div>
  </div>
  <a v-if="latestMain" class="dl-btn" :href="latestMain.downloadUrl" download>
    下载 {{ latestMain.name }}（{{ fmtSize(latestMain.size) }}）
  </a>
</div>

<p v-if="latest.prerelease" class="dl-hint">
该版本标记为预发布，功能可能不完整，仅在测试环境使用。
</p>

<div v-if="others(latest).length">

同一版本的其他镜像文件：

<table class="dl-table">
  <thead>
    <tr><th>文件</th><th>大小</th><th>下载次数</th><th>下载</th></tr>
  </thead>
  <tbody>
    <tr v-for="a in others(latest)" :key="a.name">
      <td><code>{{ a.name }}</code></td>
      <td>{{ fmtSize(a.size) }}</td>
      <td>{{ a.downloads }}</td>
      <td><a :href="a.downloadUrl" download>下载</a></td>
    </tr>
  </tbody>
</table>

</div>

## 历史版本

<div v-if="!history.length" class="dl-empty">

暂无更早的版本记录。发布新版本后，旧版本会自动出现在这里并保持可下载。

</div>

<table v-else class="dl-table">
  <thead>
    <tr><th>版本</th><th>发布日期</th><th>文件</th><th>大小</th><th>下载</th></tr>
  </thead>
  <tbody>
    <template v-for="rel in history" :key="rel.tag">
      <tr v-for="(a, i) in (rel.assets.length ? rel.assets : [null])" :key="rel.tag + '-' + i">
        <td v-if="i === 0" :rowspan="rel.assets.length || 1">
          <strong>{{ rel.name }}</strong>
          <span v-if="rel.prerelease" class="dl-badge">预发布</span>
          <div class="dl-tag"><code>{{ rel.tag }}</code></div>
        </td>
        <td v-if="i === 0" :rowspan="rel.assets.length || 1">{{ fmtDate(rel.publishedAt) }}</td>
        <td><code v-if="a">{{ a.name }}</code><span v-else>—</span></td>
        <td>{{ a ? fmtSize(a.size) : '—' }}</td>
        <td><a v-if="a" :href="a.downloadUrl" download>下载</a><span v-else>—</span></td>
      </tr>
    </template>
  </tbody>
</table>

</div>

## 拿到 ISO 之后

1. **先看验证方法**：[QEMU 无头验证](/guide/qemu-verify)，一条命令即可确认内核能起来。
2. **工具链与交叉编译**：[工具链](/guide/toolchain)，用于自行从源码构建。
3. **自己构建**：源码在 [Zjy-Zp/C-UnknownOS](https://github.com/Zjy-Zp/C-UnknownOS)，`git clone` 后按开发手册编译。

<details>
<summary>文件校验</summary>

Release 资产未附带独立的校验文件。下载后可比对文件大小与上表是否一致；需要严格校验时，请以仓库源码自行构建的产物为基准。

</details>

<style scoped>
.dl-warn {
  border: 1px solid var(--vp-c-warning-border, #e6c26a);
  background: var(--vp-c-warning-soft, #fdf6e3);
  color: var(--vp-c-text-1);
  border-radius: 8px;
  padding: 12px 16px;
  margin: 16px 0;
}
.dl-hero {
  border: 1px solid var(--vp-c-divider);
  border-radius: 12px;
  padding: 20px;
  margin: 16px 0;
  display: flex;
  flex-wrap: wrap;
  gap: 16px;
  align-items: center;
  justify-content: space-between;
  background: var(--vp-c-bg-soft);
}
.dl-ver {
  font-size: 26px;
  font-weight: 700;
  line-height: 1.2;
}
.dl-badge {
  display: inline-block;
  margin-left: 8px;
  padding: 1px 8px;
  font-size: 12px;
  font-weight: 500;
  border-radius: 10px;
  border: 1px solid var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
  vertical-align: middle;
}
.dl-meta {
  margin-top: 6px;
  color: var(--vp-c-text-2);
  font-size: 14px;
}
.dl-btn {
  display: inline-block;
  padding: 10px 20px;
  border-radius: 8px;
  background: var(--vp-c-brand-1);
  color: #fff !important;
  font-weight: 600;
  text-decoration: none;
}
.dl-btn:hover {
  background: var(--vp-c-brand-2);
}
.dl-hint {
  color: var(--vp-c-text-2);
  font-size: 14px;
}
.dl-empty {
  color: var(--vp-c-text-2);
  margin: 12px 0;
}
.dl-table {
  width: 100%;
  border-collapse: collapse;
  margin: 12px 0;
}
.dl-table th,
.dl-table td {
  border: 1px solid var(--vp-c-divider);
  padding: 8px 12px;
  text-align: left;
  vertical-align: top;
}
.dl-tag {
  margin-top: 4px;
  font-size: 12px;
  color: var(--vp-c-text-3);
}
</style>
