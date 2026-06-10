export function apiBase(): string {
    if (typeof window === 'undefined') {
        return process.env.NEXT_PUBLIC_API_URL ?? 'http://localhost:3001'
    }
    return '/api'
}

export async function api<T>(path: string, init?: RequestInit): Promise<T> {
    const res = await fetch(`${apiBase()}${path}`, {
        credentials: 'include',
        headers: { 'Content-Type': 'application/json' },
        ...init
    })

    if (!res.ok) {
        const error = await res.json().catch(() => ({ message: 'unknown error' }))
        throw new Error(error.message)
    }

    return res.json()
}
