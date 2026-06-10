import { api } from '@/lib/api'
import { useCallback, useEffect, useState } from 'react'

export interface Feature {
    key:   string
    label: string
    group: 'evasion' | 'stealing'
}

export interface BuildState {
    status:    'idle' | 'building' | 'done' | 'error'
    log:       string
    zipPass:   string | null
    buildType: 'release' | 'debug' | null
    filename:  string | null
}

export interface BuildConfig {
    buildType: 'release' | 'debug'
    c2Host:    string
    c2Port:    string
    apiKey:    string
    zipPass:   string
    features:  Record<string, boolean>
}

export function useBuilderFeatures() {
    const [features,  setFeatures]  = useState<Feature[]>([])
    const [apiKey,    setApiKey]    = useState('')
    const [zipPass,   setZipPass]   = useState('')
    const [isLoading, setIsLoading] = useState(true)

    useEffect(() => {
        api<{ features: Feature[], apiKey: string, zipPass: string }>('/builder/features')
            .then(r => { setFeatures(r.features); setApiKey(r.apiKey); setZipPass(r.zipPass) })
            .catch(() => {})
            .finally(() => setIsLoading(false))
    }, [])

    return { features, apiKey, zipPass, isLoading }
}

export function useBuilderState() {
    const [state, setState] = useState<BuildState>({
        status:    'idle',
        log:       '',
        zipPass:   null,
        buildType: null,
        filename:  null,
    })

    const refetch = useCallback(() => {
        api<BuildState>('/builder/state').then(setState).catch(() => {})
    }, [])

    useEffect(() => { refetch() }, [refetch])

    useEffect(() => {
        if (state.status !== 'building') return
        const id = setInterval(refetch, 1000)
        return () => clearInterval(id)
    }, [state.status, refetch])

    return { state, refetch }
}

export function useBuild(onStart: () => void) {
    const [isLoading, setIsLoading] = useState(false)
    const [error, setError]         = useState<string | null>(null)

    const build = async (cfg: BuildConfig) => {
        setIsLoading(true)
        setError(null)
        try {
            await api('/builder/build', {
                method: 'POST',
                body: JSON.stringify(cfg),
            })
            onStart()
        } catch (err) {
            setError(err instanceof Error ? err.message : 'build failed')
        } finally {
            setIsLoading(false)
        }
    }

    return { build, isLoading, error }
}
