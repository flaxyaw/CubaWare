import { database } from "@/utils/database";
import { usersTable } from "./schema";

const hashedPassword = async (password: string) => {
    return await Bun.password.hash(password)
}

async function seed() {
   await database.insert(usersTable).values([
    {
        name: 'admin',
        password: await hashedPassword('admin'),
        role: 'admin'
    }
   ])
}

seed()
console.log('🍃 seed created!')