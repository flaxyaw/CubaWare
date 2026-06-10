import { api } from "@/lib/api"
import { useCallback, useEffect, useState } from "react"

export interface User {
    id: string
    name: string
    role: string
    createdAt: number
}

export function useUsers() {
    const [users, setUsers] = useState<User[]>([])
    const [isLoading, setIsLoading] = useState(true)
    const [error, setError] = useState<string | null>(null)

    const fetch = useCallback(() => {
        setIsLoading(true)
        setError(null)
        api<User[]>('/admin/users')
            .then(setUsers)
            .catch((err) => setError(err instanceof Error ? err.message : 'Failed to load users'))
            .finally(() => setIsLoading(false))
    }, [])

    useEffect(() => { fetch() }, [fetch])

    return { users, isLoading, error, refetch: fetch }
}

export function useCreateUser(onSuccess: () => void) {
    const [isLoading, setIsLoading] = useState(false)
    const [error, setError] = useState<string | null>(null)

    const createUser = async (username: string, password: string, role: string) => {
        setIsLoading(true)
        setError(null)
        try {
            await api('/admin/create/user', {
                method: 'POST',
                body: JSON.stringify({ username, password, role }),
            })
            onSuccess()
        } catch (err) {
            setError(err instanceof Error ? err.message : 'Failed to create user')
        } finally {
            setIsLoading(false)
        }
    }

    return { createUser, isLoading, error }
}

export function useEditUser(onSuccess: () => void) {
    const [isLoading, setIsLoading] = useState(false)
    const [error, setError] = useState<string | null>(null)

    const editUser = async (id: string, username: string, password: string, role: string) => {
        setIsLoading(true)
        setError(null)
        try {
            await api(`/admin/edit/user/${id}`, {
                method: 'PATCH',
                body: JSON.stringify({ username, password: password || undefined, role }),
            })
            onSuccess()
        } catch (err) {
            setError(err instanceof Error ? err.message : 'Failed to update user')
        } finally {
            setIsLoading(false)
        }
    }

    return { editUser, isLoading, error }
}

export function useDeleteUser(onSuccess: () => void) {
    const [isLoading, setIsLoading] = useState(false)
    const [error, setError] = useState<string | null>(null)

    const deleteUser = async (id: string) => {
        setIsLoading(true)
        setError(null)
        try {
            await api(`/admin/delete/user/${id}`, { method: 'DELETE' })
            onSuccess()
        } catch (err) {
            setError(err instanceof Error ? err.message : 'Failed to delete user')
        } finally {
            setIsLoading(false)
        }
    }

    return { deleteUser, isLoading, error }
}
