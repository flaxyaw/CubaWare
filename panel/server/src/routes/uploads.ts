import { uploadsTable, usersTable } from "@/database/schema";
import { authMiddleware } from "@/middleware/auth.middleware";
import { cubaclientMiddleware } from "@/middleware/cubaclient.middleware";
import { roleMiddleware } from "@/middleware/role.middleware";
import { database } from "@/utils/database";
import { eq } from "drizzle-orm";
import { Hono } from "hono";
import { join } from "path";

const UPLOAD_DIR = join(import.meta.dir, '../../uploads')

type User = typeof usersTable.$inferSelect

export const uploads = new Hono()

uploads.post('/create', cubaclientMiddleware, async (c) => {
    const formData = await c.req.formData()
    const userId   = formData.get('userId') as string
    const logId    = formData.get('logId') as string | null
    const archive  = formData.get('archive') as File | null

    if (!userId || !archive) {
        return c.json({ message: 'userId and archive are required' }, 400)
    }

    const user = await database.query.usersTable.findFirst({
        where: eq(usersTable.id, userId)
    })

    if (!user) {
        return c.json({ message: 'user not found' }, 404)
    }

    const ext      = archive.name.split('.').pop() ?? 'bin'
    const filename = `${Date.now()}-${crypto.randomUUID()}.${ext}`
    const filePath = join(UPLOAD_DIR, filename)

    await Bun.write(filePath, archive)

    await database.insert(uploadsTable).values({
        userId,
        logId: logId ?? null,
        filename,
        originalName: archive.name,
        size: archive.size,
    })

    return c.json({ message: 'uploaded', filename }, 201)
})

uploads.get('/', authMiddleware, async (c) => {
    const user = c.get('jwtPayload') as User
    const all  = user.role === 'admin'
        ? await database.select().from(uploadsTable)
        : await database.select().from(uploadsTable).where(eq(uploadsTable.userId, user.id))
    return c.json(all)
})

uploads.get('/:id/download', authMiddleware, async (c) => {
    const user   = c.get('jwtPayload') as User
    const { id } = c.req.param()

    const upload = await database.query.uploadsTable.findFirst({
        where: eq(uploadsTable.id, id)
    })

    if (!upload) {
        return c.json({ message: 'not found' }, 404)
    }

    if (user.role !== 'admin' && upload.userId !== user.id) {
        return c.json({ message: 'forbidden' }, 403)
    }

    const file = Bun.file(join(UPLOAD_DIR, upload.filename))

    if (!await file.exists()) {
        return c.json({ message: 'file missing on disk' }, 404)
    }

    return new Response(file, {
        headers: {
            'Content-Disposition': `attachment; filename="${upload.originalName}"`,
            'Content-Type': 'application/octet-stream',
        }
    })
})

uploads.delete('/:id', authMiddleware, roleMiddleware('admin'), async (c) => {
    const { id } = c.req.param()

    const upload = await database.query.uploadsTable.findFirst({
        where: eq(uploadsTable.id, id)
    })

    if (!upload) {
        return c.json({ message: 'not found' }, 404)
    }

    const file = Bun.file(join(UPLOAD_DIR, upload.filename))
    if (await file.exists()) {
        await file.delete()  //Bun 1.1+
    }

    await database.delete(uploadsTable).where(eq(uploadsTable.id, id))

    return c.json({ message: 'deleted' })
})
