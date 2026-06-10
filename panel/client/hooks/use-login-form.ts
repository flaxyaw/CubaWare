import { zodResolver } from "@hookform/resolvers/zod";
import { useForm } from "react-hook-form";
import z from "zod";

const loginSchema = z.object({
    username: z.string({ error: 'Username is required' }),
    password: z.string({ error: 'Password is required' })
})

export type LoginFormData = z.infer<typeof loginSchema>

export function useLoginForm() {
    return useForm<LoginFormData>({
        resolver: zodResolver(loginSchema),
        defaultValues: {
            username: '',
            password: '',
        }
    })
}