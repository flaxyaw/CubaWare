import { NextRequest, NextResponse } from "next/server";

export async function proxy (req: NextRequest) {
    const token = req.cookies.get('auth')

    if (!token) {
        return NextResponse.redirect(new URL('/auth/sign-in', req.url))
    }

    return NextResponse.next()
}

export const config = {
    matcher: '/dashboard/:path*'
}