'use client'

import Link from "next/link"
import { usePathname } from "next/navigation"

interface INavLink {
    text: string
    icon?: React.ReactElement
    href: string
}

export default function NavLink({ text, icon, href }: INavLink) {
    const pathname = usePathname()
    const isActive = pathname === href

    return (
        <Link
         href={href}
         className={`flex gap-2 items-center hover:bg-white/5 font-medium p-2 rounded cursor-pointer select-none transition-colors duration-200 ease-in-out ${isActive ? 'text-white bg-white/10' : 'text-zinc-500 hover:text-white hover:bg-white/5'}`}>
            {icon && icon}
            <span>{text}</span>
        </Link>
    )
}