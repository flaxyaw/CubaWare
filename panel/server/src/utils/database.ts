import { env } from 'bun' // @soiderino: for some reason it doesn't want see process.env.XXXX
import { Database } from 'bun:sqlite'
import { drizzle } from "drizzle-orm/bun-sqlite";
import * as schema from '@/database/schema'

const sqlite = new Database(env.DB_FILE_NAME!)
try { sqlite.run("ALTER TABLE logs ADD COLUMN windows_version TEXT NOT NULL DEFAULT ''") } catch {}
try { sqlite.run("ALTER TABLE logs ADD COLUMN zip_pass TEXT NOT NULL DEFAULT ''") } catch {}

export const database = drizzle(sqlite, { schema })