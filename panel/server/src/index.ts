import { Hono } from 'hono'
import { logger } from 'hono/logger'

import { auth } from '@/routes/auth'
import { logs } from '@/routes/logs'
import { builder } from '@/routes/builder'
import { admin } from '@/routes/admin'
import { uploads } from '@/routes/uploads'
import { api } from '@/routes/api'
import { cors } from 'hono/cors'
import { existsSync } from 'fs'

const app = new Hono()

app.use(logger())
app.use(cors({
    origin: (origin) => origin ?? null,
    credentials: true,
}))

app.route('/auth', auth)
app.route('/logs', logs)
app.route('/admin', admin)
app.route('/builder', builder)
app.route('/uploads', uploads)
app.route('/api', api)

const cert = process.env.TLS_CERT ?? ''
const key  = process.env.TLS_KEY  ?? ''
const tls  = cert && key && existsSync(cert) && existsSync(key)
    ? { cert: Bun.file(cert), key: Bun.file(key) }
    : undefined

export default {
    port:     parseInt(process.env.PORT ?? '3001'),
    hostname: '0.0.0.0',
    fetch:    app.fetch,
    ...(tls ? { tls } : {}),
}
