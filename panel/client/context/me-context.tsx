'use client'

import { useMe } from "@/hooks/use-auth"
import { createContext, useContext } from "react"

interface Me {
    id: string
    name: string
    role: string
}

interface MeContextValue {
    me: Me | null
    isLoading: boolean
}

const MeContext = createContext<MeContextValue>({ me: null, isLoading: true })

export function MeProvider({ children }: { children: React.ReactNode }) {
    const { me, isLoading } = useMe()

    return <MeContext.Provider value={{me, isLoading}}>{children}</MeContext.Provider>
}

export const useCurrentUser = () => useContext(MeContext)