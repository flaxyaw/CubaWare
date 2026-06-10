import { useRouter } from "next/navigation";
import { useEffect, useState } from "react";
import { LoginFormData } from "./use-login-form";
import { api } from "@/lib/api";

interface Me {
    id: string
    name: string
    role: string
}

export function useSignIn() {
    const router = useRouter()
    const [error, setError] = useState<string | null>(null)
    const [isLoading, setIsLoading] = useState(false)

    const signIn = async (data: LoginFormData) => {
        setIsLoading(true)
        setError(null)

        try {
            await api('/auth/sign-in', {
                method: 'post',
                body: JSON.stringify(data),
            })

            router.push('/dashboard')
        } catch (err) {
            setError(err instanceof Error ? err.message : 'Sign in failed')
        } finally {
            setIsLoading(false)
        }
    }

    return { signIn, error, isLoading }
}


export function useMe() {
    const [me, setMe] = useState<Me | null>(null)
    const [isLoading, setIsLoading] = useState(true)
    const [error, setError] = useState<string | null>(null)

    useEffect(() => {
        api<Me>('/auth/me')
            .then(setMe)
            .catch((err) => setError(err instanceof Error ? err.message : 'Failed to fetch user'))
            .finally(() => setIsLoading(false))
    }, [])

    return { me, isLoading, error }
}

export function useSignOut() {
    const router = useRouter()
    const [isLoading, setIsLoading] = useState(false)

    const signOut = async () => {
        setIsLoading(true)
        try {
            await api('/auth/sign-out', { method: 'post' })
        } finally {
            setIsLoading(false)
            router.push('/')
        }
    }

    return { signOut, isLoading }
}