'use client'

import { useCurrentUser } from "@/context/me-context";
import { UserCircle } from "lucide-react";
import NavLink from "../navlinks";

export default function AdminNav() {
    const { me, isLoading } = useCurrentUser()

    if ( isLoading ) return null
    if ( me?.role !== 'admin' ) return null

    return (
        <>
            <p className="uppercase tracking-widest text-xs text-zinc-300 mt-10 select-none">Admin Panel</p>
            <NavLink href="/dashboard/admin/users" icon={<UserCircle size={16} />} text="User List" />
        </>
    )
}