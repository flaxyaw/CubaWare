'use client'

import { useStats } from "@/hooks/use-stats"
import { Bitcoin, Cookie, CreditCard, KeyRound, List } from "lucide-react"
import {
    Area,
    AreaChart,
    Bar,
    BarChart,
    CartesianGrid,
    Cell,
    ResponsiveContainer,
    Tooltip,
    XAxis,
    YAxis,
} from "recharts"

const ACCENT = '#a78bfa'
const MUTED  = '#52525b'

function StatCard({ label, value, icon, color = ACCENT }: {
    label: string
    value: number
    icon: React.ReactNode
    color?: string
}) {
    return (
        <div className="border border-zinc-700/50 bg-zinc-900/40 rounded-lg p-4 flex flex-col gap-3">
            <div className="flex items-center justify-between">
                <span className="text-zinc-400 text-xs uppercase tracking-widest">{label}</span>
                <span style={{ color }} className="opacity-70">{icon}</span>
            </div>
            <span className="text-white text-2xl font-semibold tabular-nums">
                {value.toLocaleString()}
            </span>
        </div>
    )
}

const tooltipStyle = {
    contentStyle: { background: '#18181b', border: '1px solid #3f3f46', borderRadius: 8, fontSize: 12 },
    labelStyle: { color: '#a1a1aa' },
    itemStyle: { color: '#fff' },
    cursor: { fill: 'rgba(255,255,255,0.04)' },
}

function ChartCard({ title, children }: { title: string; children: React.ReactNode }) {
    return (
        <div className="border border-zinc-700/50 bg-zinc-900/40 rounded-lg p-4">
            <p className="text-zinc-400 text-xs uppercase tracking-widest mb-4">{title}</p>
            {children}
        </div>
    )
}

const COUNTRY_COLORS = ['#a78bfa','#818cf8','#60a5fa','#34d399','#fbbf24','#f87171','#c084fc','#38bdf8','#4ade80','#fb923c']

export default function Page() {
    const { stats, isLoading, error } = useStats()

    if (isLoading) {
        return <div className="text-zinc-500 text-sm">Loading...</div>
    }

    if (error || !stats) {
        return <div className="text-red-400 text-sm">{error ?? 'Failed to load stats'}</div>
    }

    const { totals, byDay, byCountry } = stats

    const dataBreakdown = [
        { name: 'Passwords',    value: totals.passwords,        fill: '#f87171' },
        { name: 'Cookies',      value: totals.cookies,          fill: '#fbbf24' },
        { name: 'Credit Cards', value: totals.creditcards,      fill: '#34d399' },
        { name: 'Cryptos',      value: totals.cryptocurrencies, fill: '#60a5fa' },
    ]

    const shortDate = (iso: string) => {
        const [, m, d] = iso.split('-')
        return `${m}/${d}`
    }

    return (
        <div className="flex flex-col gap-6">

            {/* stat cards */}
            <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-5 gap-3">
                <StatCard label="Total Logs"   value={totals.logs}             icon={<List size={16} />} />
                <StatCard label="Passwords"    value={totals.passwords}        icon={<KeyRound size={16} />}    color="#f87171" />
                <StatCard label="Cookies"      value={totals.cookies}          icon={<Cookie size={16} />}      color="#fbbf24" />
                <StatCard label="Credit Cards" value={totals.creditcards}      icon={<CreditCard size={16} />}  color="#34d399" />
                <StatCard label="Cryptos"      value={totals.cryptocurrencies} icon={<Bitcoin size={16} />}     color="#60a5fa" />
            </div>

            {/* activity + countries */}
            <div className="grid grid-cols-1 lg:grid-cols-3 gap-4">

                <ChartCard title="Activity — last 30 days">
                    <ResponsiveContainer width="100%" height={200}>
                        <AreaChart data={byDay} margin={{ top: 4, right: 4, left: -28, bottom: 0 }}>
                            <defs>
                                <linearGradient id="actGrad" x1="0" y1="0" x2="0" y2="1">
                                    <stop offset="5%"  stopColor={ACCENT} stopOpacity={0.3} />
                                    <stop offset="95%" stopColor={ACCENT} stopOpacity={0} />
                                </linearGradient>
                            </defs>
                            <CartesianGrid stroke="#27272a" strokeDasharray="3 3" vertical={false} />
                            <XAxis
                                dataKey="date"
                                tickFormatter={shortDate}
                                tick={{ fill: MUTED, fontSize: 10 }}
                                tickLine={false}
                                axisLine={false}
                                interval={4}
                            />
                            <YAxis
                                allowDecimals={false}
                                tick={{ fill: MUTED, fontSize: 10 }}
                                tickLine={false}
                                axisLine={false}
                            />
                            <Tooltip {...tooltipStyle} labelFormatter={(label) => shortDate(String(label))} formatter={(v) => [v ?? 0, 'Logs']} />
                            <Area
                                type="monotone"
                                dataKey="count"
                                stroke={ACCENT}
                                strokeWidth={2}
                                fill="url(#actGrad)"
                                dot={false}
                            />
                        </AreaChart>
                    </ResponsiveContainer>
                </ChartCard>

                <ChartCard title="Top Countries">
                    <ResponsiveContainer width="100%" height={200}>
                        <BarChart
                            layout="vertical"
                            data={byCountry}
                            margin={{ top: 0, right: 8, left: 0, bottom: 0 }}
                        >
                            <CartesianGrid stroke="#27272a" strokeDasharray="3 3" horizontal={false} />
                            <XAxis
                                type="number"
                                tick={{ fill: MUTED, fontSize: 10 }}
                                tickLine={false}
                                axisLine={false}
                                allowDecimals={false}
                            />
                            <YAxis
                                type="category"
                                dataKey="country"
                                tick={{ fill: '#a1a1aa', fontSize: 11 }}
                                tickLine={false}
                                axisLine={false}
                                width={28}
                            />
                            <Tooltip {...tooltipStyle} formatter={(v) => [v ?? 0, 'Logs']} />
                            <Bar dataKey="count" radius={[0, 4, 4, 0]}>
                                {byCountry.map((_, i) => (
                                    <Cell key={i} fill={COUNTRY_COLORS[i % COUNTRY_COLORS.length]} />
                                ))}
                            </Bar>
                        </BarChart>
                    </ResponsiveContainer>
                </ChartCard>

                <ChartCard title="Data Breakdown">
                    <ResponsiveContainer width="100%" height={200}>
                        <BarChart
                            data={dataBreakdown}
                            margin={{ top: 4, right: 4, left: -28, bottom: 0 }}
                        >
                            <CartesianGrid stroke="#27272a" strokeDasharray="3 3" vertical={false} />
                            <XAxis
                                dataKey="name"
                                tick={{ fill: MUTED, fontSize: 10 }}
                                tickLine={false}
                                axisLine={false}
                            />
                            <YAxis
                                allowDecimals={false}
                                tick={{ fill: MUTED, fontSize: 10 }}
                                tickLine={false}
                                axisLine={false}
                            />
                            <Tooltip {...tooltipStyle} formatter={(v, name) => [v ?? 0, name]} />
                            <Bar dataKey="value" radius={[4, 4, 0, 0]}>
                                {dataBreakdown.map((entry, i) => (
                                    <Cell key={i} fill={entry.fill} />
                                ))}
                            </Bar>
                        </BarChart>
                    </ResponsiveContainer>
                </ChartCard>

            </div>
        </div>
    )
}
