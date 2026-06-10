import type { Metadata } from "next";
import { Exo_2 } from "next/font/google";
import "./globals.css";

const Exo = Exo_2({
  variable: "--font-exo-sans",
  subsets: ["latin", 'cyrillic'],
});

export const metadata: Metadata = {
  title: { 
    default: 'CubaWare', 
    template: 'CubaWare | %s' 
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html
      lang="en"
      className={`${Exo.variable} h-full antialiased`}
    >
      <body className="min-h-full flex flex-col">{children}</body>
    </html>
  );
}
