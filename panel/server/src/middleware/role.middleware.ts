import { usersTable } from "@/database/schema";
import { createMiddleware } from "hono/factory";

type Role = 'admin' | 'user'
type User = typeof usersTable.$inferSelect

export const roleMiddleware = (role: Role) => createMiddleware<{ Variables: { jwtPayload: User } }>(async (c, next) => {
    const payload = c.get('jwtPayload')

    if (!payload) {
        return c.json({ message: 'unauthorized' }, 401)
    }

    if (payload.role !== role) {
        return c.json({ message: 'forbidden' }, 403)
    }

    await next()
})