
interface IButton {
    text: string
    onClick?: () => void;
}

export default function PrimaryBtn({ text, onClick }: IButton) {
    return (
        <button onClick={onClick} className="bg-(--accent-color) text-white hover:bg-(--accent-color)/80 px-3 py-1 rounded-md shadow cursor-pointer select-none transition-colors duration-200 ease-in-out">
            {text}
        </button>
    )
}