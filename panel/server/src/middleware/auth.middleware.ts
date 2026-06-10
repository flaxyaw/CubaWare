import { usersTable } from "@/database/schema";
import { database } from "@/utils/database";
import { eq } from "drizzle-orm";
import { getCookie } from "hono/cookie";
import { createMiddleware } from "hono/factory";
import { verify } from "hono/jwt";

// @soiderino: simple token check
export const authMiddleware = createMiddleware(async (c, next) => {
    const token = getCookie(c, 'auth')

    if (!token) {
        return c.json({ message: 'unauthorized' }, 401)
    }

    try {
        const payload = await verify(token, process.env.JWT_SECRET!, 'HS256')
        
        const user = await database.query.usersTable.findFirst({
            where: eq(usersTable.id, payload.id as string)
        })

        if (!user) {
            return c.json({ message: 'unauthorized' }, 401)
        }

        c.set('jwtPayload', user)
    } catch {
        return c.json({ message: 'invalid or expired token' }, 401)
    }

    await next()
})