import { usersTable } from "@/database/schema";
import { authMiddleware } from "@/middleware/auth.middleware";
import { database } from "@/utils/database";
import { eq } from "drizzle-orm";
import { Hono } from "hono";
import { setCookie } from "hono/cookie";
import { JwtVariables, sign } from "hono/jwt";

type Variables = JwtVariables
export const auth = new Hono<{ Variables: Variables }>()

auth.post('/sign-in', async (c) => {
    const { username, password } = await c.req.json()

    if (!username || !password) {
        return c.json({ message: 'username and password are required' }, 400)
    }

    const user = database.select().from(usersTable).where(eq(usersTable.name, username)).get()

    if (!user) {
        return c.json({ message: 'incorrect username or password' }, 401)
    }

    const isMatch = await Bun.password.verify(password, user.password)
    if (!isMatch) {
        return c.json({ message: 'incorrect username or password' }, 401)
    }

    const token = await sign({
        id: user.id,
        username: user.name,
        role: user.role,
        exp: Math.floor(Date.now() / 1000) + 7 * 24 * 60 * 60
    }, process.env.JWT_SECRET!, 'HS256')

    setCookie(c, 'auth', token, {
        httpOnly: true,
        secure: process.env.NODE_ENV === 'production',
        sameSite: 'lax',
        path: '/',
        expires: new Date(Date.now() + 7 * 24 * 60 * 60 * 1000),
    })

    return c.json({ message: 'sign in' })
})

auth.post('/sign-out', (c) => {
    setCookie(c, 'auth', '', {
        httpOnly: true,
        secure: process.env.NODE_ENV === 'production',
        sameSite: 'lax',
        path: '/',
        expires: new Date(0),
    })
    return c.json({ message: 'signed out' })
})

auth.get('/me', authMiddleware, async (c) => {
    const { password, createdAt, updatedAt, ...user } = c.get('jwtPayload')
    return c.json(user)
})