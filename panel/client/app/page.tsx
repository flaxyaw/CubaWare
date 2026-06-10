import PrimaryBtn from "@/components/buttons/primary";
import Logo from "@/components/logo";
import type { Metadata } from "next";
import Link from "next/link";

export const metadata: Metadata = {
  title: 'CubaWare'
}

export default function Home() {
  return (
    <div className="flex flex-col gap-5 h-screen items-center justify-center font-sans">
      
      <Logo />
      {/* @soiderino(todo): put anime flags here :) */}
      <Link href={'/dashboard'}>
        <PrimaryBtn text="Enter to World" /> 
      </Link>

    </div>
  );
}
