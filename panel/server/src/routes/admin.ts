import { usersTable } from "@/database/schema";
import { authMiddleware } from "@/middleware/auth.middleware";
import { roleMiddleware } from "@/middleware/role.middleware";
import { database } from "@/utils/database";
import { eq } from "drizzle-orm";
import { Hono } from "hono";

export const admin = new Hono()

admin.use(authMiddleware)
admin.use(roleMiddleware('admin'))

admin.get('/users', async (c) => {
    const users = await database.select({
        id: usersTable.id,
        name: usersTable.name,
        role: usersTable.role,
        createdAt: usersTable.createdAt,
    }).from(usersTable)
    return c.json(users)
})

admin.post('/create/user', async (c) => {
    const { username, password, role } = await c.req.json()

    if (!username || !password || !role) {
        return c.json({ message: 'username, password and role are required' }, 400)
    }

    const existing = await database.query.usersTable.findFirst({
        where: eq(usersTable.name, username)
    })

    if (existing) {
        return c.json({ message: 'username is already taken' }, 409)
    }

    await database.insert(usersTable).values({
        name: username,
        password: await Bun.password.hash(password),
        role,
    })

    return c.json({ message: 'user created' }, 201)
})

admin.patch('/edit/user/:id', async (c) => {
    const { id } = c.req.param()
    const { username, password, role } = await c.req.json()

    const user = await database.query.usersTable.findFirst({
        where: eq(usersTable.id, id)
    })

    if (!user) {
        return c.json({ message: 'user not found' }, 404)
    }

    if (username && username !== user.name) {
        const taken = await database.query.usersTable.findFirst({
            where: eq(usersTable.name, username)
        })
        if (taken) {
            return c.json({ message: 'username is already taken' }, 409)
        }
    }

    await database.update(usersTable)
        .set({
            ...(username ? { name: username } : {}),
            ...(password ? { password: await Bun.password.hash(password) } : {}),
            ...(role ? { role } : {}),
        })
        .where(eq(usersTable.id, id))

    return c.json({ message: 'user updated' })
})

admin.delete('/delete/user/:id', async (c) => {
    const { id } = c.req.param()

    const user = await database.query.usersTable.findFirst({
        where: eq(usersTable.id, id)
    })

    if (!user) {
        return c.json({ message: 'user not found' }, 404)
    }

    await database.delete(usersTable).where(eq(usersTable.id, id))

    return c.json({ message: 'user deleted' })
})
