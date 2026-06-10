import { api } from "@/lib/api"
import { useEffect, useState } from "react"

export interface Log {
    id: string
    userId: string
    name: string
    ip: string
    country: string
    cookies: number
    passwords: number
    creditcards: number
    cryptocurrencies: number
    windowsVersion: string
    zipPass: string
    createdAt: number
    uploadId: string | null
}

export function useLogs() {
    const [logs, setLogs] = useState<Log[]>([])
    const [isLoading, setIsLoading] = useState(true)
    const [error, setError] = useState<string | null>(null)

    const fetch = () => {
        setIsLoading(true)
        setError(null)
        api<Log[]>('/logs')
            .then(setLogs)
            .catch((err) => setError(err instanceof Error ? err.message : 'Failed to load logs'))
            .finally(() => setIsLoading(false))
    }

    useEffect(() => { fetch() }, [])

    return { logs, isLoading, error, refetch: fetch }
}
