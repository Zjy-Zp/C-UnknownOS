---
title: 下载
---

<script setup>
import { computed } from 'vue'
import { data } from '../.vitepress/data/releases.data.mts'

const releases = computed(() => data.releases || [])
const latest = computed(() => releases.value[0] || null)
const history = computed(() => releases.value.slice(1))

// 从文件名解析构建编号：cunos_v75.iso -> 75（无编号返回 -1）
function buildNo(name) {
  const m = /^cunos_v(\d+)\.iso$/i.exec(name || '')
  return m ? parseInt(m[1], 10) : -1
}

// 当前发布批次下的所有 ISO，按构建编号从新到旧排序
const isos = computed(() => {
  const list = ((latest.value && latest.value.assets) || []).filter((a) => /\.iso$/i.test(a.name))
  return list.slice().sort((a, b) => buildNo(b.name) - buildNo(a.name))
})

// 最新构建＝编号最大的 cunos_vN.iso；没有编号时才退回 cunos.iso
function pickMain() {
  const list = isos.value
  const numbered = list.filter((a) => buildNo(a.name) >= 0)
  if (numbered.length) return numbered[0]
  return list.find((a) => /^cunos\.iso$/i.test(a.name)) || list[0] || null
}

// 旧版本＝除最新构建以外的全部镜像
function others(release) {
  if (!release) return []
  const main = pickMain()
  return isos.value.filter((a) => !main || a.name !== main.name)
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

const latestMain = computed(() => pickMain())
const latestLabel = computed(() => {
  const n = buildNo(latestMain.value && latestMain.value.name)
  return n >= 0 ? `cunos v${n}` : latestMain.value ? latestMain.value.name : ''
})
</script>

# 下载 cunos

页面数据在每次站点构建时从 GitHub Releases 自动抓取，发布新版本后站点会自动重建，无需手动操作。

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
      {{ latestLabel }}
      <span v-if="latest.prerelease" class="dl-badge">预发布</span>
    </div>
    <div class="dl-meta">发布于 {{ fmtDate(latest.publishedAt) }}</div>
  </div>
  <a v-if="latestMain" class="dl-btn" :href="latestMain.downloadUrl" download>
    下载 {{ latestMain.name }}（{{ fmtSize(latestMain.size) }}）
  </a>
</div>

<p v-if="latest.prerelease" class="dl-hint">
该版本标记为预发布，功能可能不完整，仅在测试环境使用。
</p>

<div v-if="others(latest).length">

<details class="dl-older">
<summary>旧版本构建镜像（{{ others(latest).length }} 个，仅用于回溯）</summary>

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

</details>

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
.dl-older {
  margin-top: 16px;
}
.dl-older > summary {
  cursor: pointer;
  color: var(--vp-c-text-2);
  font-size: 14px;
  padding: 6px 0;
  user-select: none;
}
.dl-older > summary:hover {
  color: var(--vp-c-brand-1);
}
</style>
