import { readFileSync, writeFileSync } from 'fs'

function u16(b: Buffer, o: number) { return b.readUInt16LE(o) }
function u32(b: Buffer, o: number) { return b.readUInt32LE(o) }
function pu32(b: Buffer, o: number, v: number) { b.writeUInt32LE(v, o) }

function peChecksum(data: Buffer): number {
    const peOff = u32(data, 0x3c)
    pu32(data, peOff + 0x58, 0)

    let s = 0
    const n = data.length
    let i = 0
    while (i < n - 1) {
        s += u16(data, i)
        if (s > 0xffff) s = (s & 0xffff) + (s >> 16)
        i += 2
    }
    if (n & 1) s += data[n - 1]
    s = (s & 0xffff) + (s >> 16)
    return (s + n) >>> 0
}

const SNIPPETS: readonly Buffer[] = [
    Buffer.from([0x55, 0x48, 0x89, 0xe5]),
    Buffer.from([0x48, 0x83, 0xec, 0x20]),
    Buffer.from([0x48, 0x31, 0xc0]),
    Buffer.from([0x48, 0x89, 0xc3]),
    Buffer.from([0x5d, 0xc3]),
    Buffer.from([0x48, 0x83, 0xc4, 0x20, 0xc3]),
    Buffer.from([0xb8, 0x01, 0x00, 0x00, 0x00]),
    Buffer.from([0x45, 0x31, 0xc0]),
    Buffer.from([0x0f, 0x1f, 0x44, 0x00, 0x00]),
]

const FAKE_STRINGS: readonly Buffer[] = [
    Buffer.from('SystemModule\x00'),
    Buffer.from('ntdll.dll\x00'),
    Buffer.from('Windows NT\x00'),
]

function genJunk(size: number): Buffer {
    const parts: Buffer[] = []
    let total = 0
    while (total < size) {
        const chunk = Math.random() < 0.08
            ? FAKE_STRINGS[Math.floor(Math.random() * FAKE_STRINGS.length)]
            : SNIPPETS[Math.floor(Math.random() * SNIPPETS.length)]
        parts.push(chunk)
        total += chunk.length
    }
    return Buffer.concat(parts).subarray(0, size)
}

export function postprocess(exePath: string): void {
    const data  = Buffer.from(readFileSync(exePath))
    const peOff = u32(data, 0x3c)

    const ts = 0x4f000000 + Math.floor(Math.random() * (0x62000000 - 0x4f000000))
    pu32(data, peOff + 8, ts)

    const nsec    = u16(data, peOff + 6)
    const optSize = u16(data, peOff + 20)
    const secBase = peOff + 24 + optSize
    let lastEnd   = 0
    for (let i = 0; i < nsec; i++) {
        const off  = secBase + i * 40
        const end  = u32(data, off + 20) + u32(data, off + 16)
        if (end > lastEnd) lastEnd = end
    }
    if (lastEnd === 0) lastEnd = data.length

    const junkSize = 0x1000 + Math.floor(Math.random() * 0x2000)
    const junk     = genJunk(junkSize)
    junk.writeUInt32LE(Math.random() * 0xffffffff >>> 0, 0)
    junk.writeUInt32LE(Math.random() * 0xffffffff >>> 0, 4)

    const pad    = lastEnd > data.length ? Buffer.alloc(lastEnd - data.length) : Buffer.alloc(0)
    const final_ = Buffer.concat([data, pad, junk])
    final_.writeUInt32LE(peChecksum(final_), peOff + 0x58)

    writeFileSync(exePath, final_)
}
