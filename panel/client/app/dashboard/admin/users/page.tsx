'use client'

import PrimaryBtn from "@/components/buttons/primary"
import SecondaryBtn from "@/components/buttons/secondary"
import { useCreateUser, useDeleteUser, useEditUser, useUsers, User } from "@/hooks/use-users"
import { Pencil, Trash2, X } from "lucide-react"
import { useState } from "react"

type ModalState =
    | { type: 'create' }
    | { type: 'edit'; user: User }
    | { type: 'delete'; user: User }
    | null

function Modal({ onClose, children }: { onClose: () => void; children: React.ReactNode }) {
    return (
        <div className="fixed inset-0 bg-black/60 flex items-center justify-center z-50" onClick={onClose}>
            <div className="bg-zinc-900 border border-zinc-700/50 rounded-lg p-6 w-full max-w-sm shadow-xl" onClick={(e) => e.stopPropagation()}>
                {children}
            </div>
        </div>
    )
}

function Field({ label, type = 'text', value, onChange, placeholder }: {
    label: string
    type?: string
    value: string
    onChange: (v: string) => void
    placeholder?: string
}) {
    return (
        <label className="flex flex-col gap-1 text-sm">
            <span className="text-zinc-400">{label}</span>
            <input
                type={type}
                value={value}
                onChange={(e) => onChange(e.target.value)}
                placeholder={placeholder}
                className="bg-zinc-800 border border-zinc-700/50 rounded px-3 py-2 text-white placeholder-zinc-600 focus:outline-none focus:border-zinc-500"
            />
        </label>
    )
}

function RoleSelect({ value, onChange }: { value: string; onChange: (v: string) => void }) {
    return (
        <label className="flex flex-col gap-1 text-sm">
            <span className="text-zinc-400">Role</span>
            <select
                value={value}
                onChange={(e) => onChange(e.target.value)}
                className="bg-zinc-800 border border-zinc-700/50 rounded px-3 py-2 text-white focus:outline-none focus:border-zinc-500"
            >
                <option value="user">user</option>
                <option value="admin">admin</option>
            </select>
        </label>
    )
}

function CreateModal({ onClose, onDone }: { onClose: () => void; onDone: () => void }) {
    const [username, setUsername] = useState('')
    const [password, setPassword] = useState('')
    const [role, setRole] = useState('user')
    const { createUser, isLoading, error } = useCreateUser(onDone)

    return (
        <Modal onClose={onClose}>
            <div className="flex items-center justify-between mb-5">
                <h2 className="text-white font-semibold">New User</h2>
                <X size={16} className="text-zinc-500 cursor-pointer hover:text-white" onClick={onClose} />
            </div>
            <div className="flex flex-col gap-3">
                <Field label="Username" value={username} onChange={setUsername} placeholder="username" />
                <Field label="Password" type="password" value={password} onChange={setPassword} placeholder="••••••••" />
                <RoleSelect value={role} onChange={setRole} />
                {error && <p className="text-red-400 text-xs">{error}</p>}
                <div className="flex gap-2 justify-end mt-2">
                    <SecondaryBtn text="Cancel" onClick={onClose} />
                    <PrimaryBtn text={isLoading ? 'Creating...' : 'Create'} onClick={() => createUser(username, password, role)} />
                </div>
            </div>
        </Modal>
    )
}

function EditModal({ user, onClose, onDone }: { user: User; onClose: () => void; onDone: () => void }) {
    const [username, setUsername] = useState(user.name)
    const [password, setPassword] = useState('')
    const [role, setRole] = useState(user.role)
    const { editUser, isLoading, error } = useEditUser(onDone)

    return (
        <Modal onClose={onClose}>
            <div className="flex items-center justify-between mb-5">
                <h2 className="text-white font-semibold">Edit User</h2>
                <X size={16} className="text-zinc-500 cursor-pointer hover:text-white" onClick={onClose} />
            </div>
            <div className="flex flex-col gap-3">
                <Field label="Username" value={username} onChange={setUsername} />
                <Field label="New Password" type="password" value={password} onChange={setPassword} placeholder="leave blank to keep current" />
                <RoleSelect value={role} onChange={setRole} />
                {error && <p className="text-red-400 text-xs">{error}</p>}
                <div className="flex gap-2 justify-end mt-2">
                    <SecondaryBtn text="Cancel" onClick={onClose} />
                    <PrimaryBtn text={isLoading ? 'Saving...' : 'Save'} onClick={() => editUser(user.id, username, password, role)} />
                </div>
            </div>
        </Modal>
    )
}

function DeleteModal({ user, onClose, onDone }: { user: User; onClose: () => void; onDone: () => void }) {
    const { deleteUser, isLoading, error } = useDeleteUser(onDone)

    return (
        <Modal onClose={onClose}>
            <div className="flex items-center justify-between mb-5">
                <h2 className="text-white font-semibold">Delete User</h2>
                <X size={16} className="text-zinc-500 cursor-pointer hover:text-white" onClick={onClose} />
            </div>
            <p className="text-zinc-400 text-sm mb-5">
                Are you sure you want to delete <span className="text-white font-medium">{user.name}</span>? This cannot be undone.
            </p>
            {error && <p className="text-red-400 text-xs mb-3">{error}</p>}
            <div className="flex gap-2 justify-end">
                <SecondaryBtn text="Cancel" onClick={onClose} />
                <button
                    onClick={() => deleteUser(user.id)}
                    className="bg-red-600 hover:bg-red-700 text-white px-3 py-1 rounded-md shadow cursor-pointer select-none transition-colors duration-200 ease-in-out text-sm"
                >
                    {isLoading ? 'Deleting...' : 'Delete'}
                </button>
            </div>
        </Modal>
    )
}

export default function Page() {
    const { users, isLoading, error, refetch } = useUsers()
    const [modal, setModal] = useState<ModalState>(null)

    const closeAndRefetch = () => {
        setModal(null)
        refetch()
    }

    return (
        <div>
            <div className="flex items-center justify-between mb-6">
                <h1 className="text-white font-semibold text-lg">Users</h1>
                <PrimaryBtn text="New User" onClick={() => setModal({ type: 'create' })} />
            </div>

            {isLoading && <p className="text-zinc-500 text-sm">Loading...</p>}
            {error && <p className="text-red-400 text-sm">{error}</p>}

            {!isLoading && !error && (
                <div className="border border-zinc-700/50 rounded-lg overflow-hidden">
                    <table className="w-full text-sm">
                        <thead>
                            <tr className="border-b border-zinc-700/50 bg-zinc-900/50">
                                <th className="text-left text-zinc-400 font-medium px-4 py-3">Name</th>
                                <th className="text-left text-zinc-400 font-medium px-4 py-3">Role</th>
                                <th className="text-left text-zinc-400 font-medium px-4 py-3">Created</th>
                                <th className="px-4 py-3" />
                            </tr>
                        </thead>
                        <tbody>
                            {users.map((user) => (
                                <tr key={user.id} className="border-b border-zinc-700/30 last:border-0 hover:bg-zinc-800/30 transition-colors">
                                    <td className="px-4 py-3 text-white">{user.name}</td>
                                    <td className="px-4 py-3">
                                        <span className={`px-2 py-0.5 rounded text-xs font-medium ${user.role === 'admin' ? 'bg-purple-500/20 text-purple-300' : 'bg-zinc-700/50 text-zinc-400'}`}>
                                            {user.role}
                                        </span>
                                    </td>
                                    <td className="px-4 py-3 text-zinc-400">
                                        {new Date(user.createdAt).toLocaleDateString()}
                                    </td>
                                    <td className="px-4 py-3">
                                        <div className="flex items-center gap-3 justify-end">
                                            <button onClick={() => setModal({ type: 'edit', user })} className="text-zinc-500 hover:text-white cursor-pointer transition-colors">
                                                <Pencil size={14} />
                                            </button>
                                            <button onClick={() => setModal({ type: 'delete', user })} className="text-zinc-500 hover:text-red-400 cursor-pointer transition-colors">
                                                <Trash2 size={14} />
                                            </button>
                                        </div>
                                    </td>
                                </tr>
                            ))}
                            {users.length === 0 && (
                                <tr>
                                    <td colSpan={4} className="px-4 py-8 text-center text-zinc-600">No users found</td>
                                </tr>
                            )}
                        </tbody>
                    </table>
                </div>
            )}

            {modal?.type === 'create' && <CreateModal onClose={() => setModal(null)} onDone={closeAndRefetch} />}
            {modal?.type === 'edit' && <EditModal user={modal.user} onClose={() => setModal(null)} onDone={closeAndRefetch} />}
            {modal?.type === 'delete' && <DeleteModal user={modal.user} onClose={() => setModal(null)} onDone={closeAndRefetch} />}
        </div>
    )
}
