
interface IButton {
    text: string
    onClick?: () => void;
}

export default function SecondaryBtn({ text, onClick }: IButton) {
    return (
        <button onClick={onClick} className="bg-white/5 text-white hover:bg-white/10 px-3 py-1 rounded-md shadow cursor-pointer select-none transition-colors duration-200 ease-in-out">
            {text}
        </button>
    )
}