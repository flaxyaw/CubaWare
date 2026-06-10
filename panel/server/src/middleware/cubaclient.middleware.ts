import { createMiddleware } from "hono/factory";

export const cubaclientMiddleware = createMiddleware(async (c, next) => {
    const key = c.req.header('X-API-Key')

    if (!key || key !== process.env.CUBACLIENT_API_KEY) {
        return c.json({ message: 'forbidden' }, 403)
    }

    await next()
})
