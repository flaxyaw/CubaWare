'use client'

import { useMe } from "@/hooks/use-auth";
import { LogOut } from "lucide-react";

export default function UserCard() {
    const { me,  isLoading } = useMe()

    return (
        <div className="absolute bottom-3 left-3 right-3 text-sm border border-zinc-700/50 bg-zinc-900 rounded p-3">
            <div className="flex items-center justify-between">
                {isLoading
                    ? <span className="text-zinc-500">Loading...</span>
                    : <span>{me?.name ?? '—'}</span>
                }
                <LogOut className="text-zinc-500 hover:text-red-400 cursor-pointer" size={16} />
            </div>
        </div>
    )
}