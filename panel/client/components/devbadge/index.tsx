export default function DevBadge() {
    return (
        <div className="inline-flex mt-2 text-xs uppercase animate-pulse">
            {process.env.NODE_ENV === 'development' ? (
                <div className="text-yellow-300 border border-yellow-300/20 bg-yellow-300/5 px-3 py-1 rounded-md">
                    {process.env.NODE_ENV === 'development' && 'DEV MODE'}
                </div>
            ) : (
                <div className="text-green-300 border border-green-300/20 bg-green-200/5 px-3 py-1 rounded-md">
                    {process.env.NODE_ENV === 'production' && 'LIVE MODE'}
                </div>
            )}
        </div>
    )
}