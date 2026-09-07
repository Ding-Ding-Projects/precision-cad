import type { Metadata } from 'next';
import './globals.css';
export const metadata: Metadata = {title:'Precision CAD | Open-source mechanical design',description:'A new open-source mechanical CAD, CAM and structural-analysis project for Windows. Explore specifications, local Git history plans and the roadmap. In development.',metadataBase:new URL('https://precision-cad.dayteetjer.chatgpt.site'),openGraph:{title:'Precision CAD',description:'Precise mechanical design. Open foundations. Explore the roadmap and documentation.',type:'website'}};
export default function RootLayout({children}:Readonly<{children:React.ReactNode}>){return <html lang="en"><body>{children}</body></html>;}
