'use client'

import Logo from "@/components/logo";
import { useSignIn } from "@/hooks/use-auth";
import { useLoginForm } from "@/hooks/use-login-form";

export default function Page() {
    const { register, handleSubmit, formState: { errors } } = useLoginForm()
    const { signIn, error, isLoading } = useSignIn()

    return (
        <div className="flex gap-3 flex-col justify-center mx-auto h-screen w-[25%]">
            <Logo />

            <div className="flex flex-col mt-5 mb-3">
                <h2 className="text-2xl text-white">Welcome back</h2>
                <span className="text-zinc-500">Sign in to your account</span>
            </div>

            <form onSubmit={handleSubmit(signIn)}>
               <div className="flex flex-col gap-3 mt-5">
                    <input {...register('username')} id="username" className="border w-full bg-zinc-950/30 border-zinc-700/50 rounded-md px-3 py-2 outline-none" type="text" placeholder="TomAndJerry" />
                    <input {...register('password')} id="password" className="border w-full bg-zinc-950/30 border-zinc-700/50 rounded-md px-3 py-2 outline-none" type="password" placeholder="••••••••••" />
                    
                    {error && <p className="text-red-500 text-sm">{error}</p>}

                    <button disabled={isLoading} className="bg-(--accent-color) mt-1 p-2 rounded-md cursor-pointer hover:bg-(--accent-color)/80 transition-colors duration-200 ease-in-out" type="submit">Sign In</button>
                </div>
            </form>
        </div>
    )

}