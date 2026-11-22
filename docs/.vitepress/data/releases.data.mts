import { defineLoader } from 'vitepress'

export interface ReleaseAsset {
  name: string
  size: number
  downloadUrl: string
  downloads: number
}

export interface ReleaseInfo {
  tag: string
  name: string
  publishedAt: string
  prerelease: boolean
  body: string
  htmlUrl: string
  assets: ReleaseAsset[]
}

export interface ReleasesData {
  releases: ReleaseInfo[]
  fetchedAt: string
  error: string
}

declare const data: ReleasesData
export { data }

const REPO = 'Zjy-Zp/C-UnknownOS'
const API = `https://api.github.com/repos/${REPO}/releases?per_page=100`

interface RawAsset {
  name: string
  size: number
  browser_download_url: string
  download_count: number
}

interface RawRelease {
  tag_name: string
  name: string
  published_at: string
  prerelease: boolean
  body: string
  html_url: string
  draft: boolean
  assets: RawAsset[]
}

export default defineLoader({
  async load(): Promise<ReleasesData> {
    const headers: Record<string, string> = {
      Accept: 'application/vnd.github+json',
      'User-Agent': 'cunos-docs-build'
    }
    // GitHub Actions 环境下注入 token，避免匿名 60 次/小时的限流
    const token = process.env.GITHUB_TOKEN || process.env.GH_TOKEN
    if (token) headers.Authorization = `Bearer ${token}`

    // 构建机到 api.github.com 偶发抖动，做 3 次带退避的重试
    let raw: RawRelease[] = []
    let lastErr: unknown = null
    for (let attempt = 1; attempt <= 3; attempt++) {
      try {
        const res = await fetch(API, { headers, signal: AbortSignal.timeout(20000) })
        if (!res.ok) throw new Error(`GitHub API ${res.status}`)
        raw = (await res.json()) as RawRelease[]
        lastErr = null
        break
      } catch (e) {
        lastErr = e
        if (attempt < 3) await new Promise((r) => setTimeout(r, attempt * 1500))
      }
    }

    try {
      if (lastErr) throw lastErr
      const releases = raw
        .filter((r) => !r.draft)
        .map((r) => ({
          tag: r.tag_name,
          name: r.name || r.tag_name,
          publishedAt: r.published_at,
          prerelease: r.prerelease,
          body: (r.body || '').trim(),
          htmlUrl: r.html_url,
          assets: r.assets.map((a) => ({
            name: a.name,
            size: a.size,
            downloadUrl: a.browser_download_url,
            downloads: a.download_count
          }))
        }))
        .sort((a, b) => (a.publishedAt < b.publishedAt ? 1 : -1))
      return { releases, fetchedAt: new Date().toISOString(), error: '' }
    } catch (e) {
      return {
        releases: [],
        fetchedAt: new Date().toISOString(),
        error: e instanceof Error ? e.message : String(e)
      }
    }
  }
})
