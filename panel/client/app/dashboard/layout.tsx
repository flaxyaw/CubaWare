import AdminNav from "@/components/admin-nav"
import DevBadge from "@/components/devbadge"
import Logo from "@/components/logo"
import NavLink from "@/components/navlinks"
import UserCard from "@/components/user-card"
import { MeProvider } from "@/context/me-context"
import { GaugeCircle, Hammer, List } from "lucide-react"

import { Metadata } from "next"

export const metadata: Metadata = {
	title: 'Dashboard'
}

export default function DashboardLayout({ children }: Readonly<{ children: React.ReactNode }>) {
	return (
		<MeProvider>
		<div className="flex h-screen">
			{/* left */}
			<div className="flex relative p-3 flex-col w-50 sm:w-60 h-full bg-zinc-950/30 border-r border-zinc-600/30 transition-all duration-200 ease-in-out">
				<div className="select-none">
					<Logo />
					<DevBadge/>
				</div>

				<nav className="flex flex-col gap-2 mt-20 text-sm">

					<p className="uppercase tracking-widest text-xs text-zinc-300 select-none">Navigation</p>
					<NavLink href="/dashboard" icon={ <GaugeCircle size={16} /> } text="Dashboard" />
					<NavLink href="/dashboard/logs" icon={ <List size={16} /> } text="Logs" />
					<NavLink href="/dashboard/builder" icon={ <Hammer size={16} /> } text="Builder" />
<AdminNav/>

				</nav>

				<UserCard /> 

			</div>

			{/* main */}
			<div className="flex-1 m-5">{children}</div>
		</div>
		</MeProvider>
	)
}