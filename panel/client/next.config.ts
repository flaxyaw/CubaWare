import type { NextConfig } from "next";
import { networkInterfaces } from "os";

function localIPs(): string[] {
    return Object.values(networkInterfaces())
        .flat()
        .filter((n): n is NonNullable<typeof n> => n != null && !n.internal && !n.address.includes(':'))
        .map(n => n.address)
}

const API_ORIGIN = process.env.NEXT_PUBLIC_API_URL ?? 'http://localhost:3001'

const nextConfig: NextConfig = {
    allowedDevOrigins: localIPs(),
    async rewrites() {
        return [
            {
                source: '/api/:path*',
                destination: `${API_ORIGIN}/:path*`,
            },
        ]
    },
};

export default nextConfig
