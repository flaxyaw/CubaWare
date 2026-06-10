import cato_dance from '@/public/catJAM.webp'
import Image from 'next/image'

export default function Logo() {
    return (
        <div className='flex gap-2 items-center'>
            <Image src={cato_dance.src} className='border border-zinc-800 bg-zinc-950/50 rounded bg-cover' alt='car dancer' height={32} width={32} />
            <span className='uppercase'>CubaWare</span>
        </div>
    )
}