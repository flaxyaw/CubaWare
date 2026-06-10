//https://github.com/herolover/sqlite3_wrapper


#pragma once
#include <sqlite_utils/sqlite3.h>
#include <stdexcept>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sqlite3_wrapper
{
    class exception : public std::runtime_error
    {
    public:
        exception(std::string_view sql, sqlite3* db)
            : std::runtime_error("'" + std::string(sql) + "' failed: " + sqlite3_errmsg(db))
        {
        }

        exception(sqlite3* db)
            : std::runtime_error(sqlite3_errmsg(db))
        {
        }

        exception(sqlite3_stmt* statement)
            : exception(sqlite3_db_handle(statement))
        {
        }
    };

    enum class bind_policy
    {
        STATIC,
        TRANSIENT
    };

    template<class T, class Enable = void>
    struct type_traits
    {
        static int bind(sqlite3_stmt* statement, int index, const T& arg, bind_policy policy);
        static void column(sqlite3_stmt* statement, int column, T& arg);
    };

    class statement
    {
    public:
        statement(sqlite3* db, std::string_view sql, unsigned int prepare_flags = 0)
        {
            auto res = sqlite3_prepare_v3(db, sql.data(), static_cast<int>(sql.size()), prepare_flags, &_statement, nullptr);
            if (res != SQLITE_OK)
            {
                throw exception(sql, db);
            }
        }
        statement(statement&& another)
        {
            std::swap(_statement, another._statement);
            std::swap(_can_fetch, another._can_fetch);
        }
        statement(const statement&) = delete;

        statement& operator=(statement&& another)
        {
            std::swap(_statement, another._statement);
            std::swap(_can_fetch, another._can_fetch);
            return *this;
        }
        statement& operator=(const statement&) = delete;

        ~statement()
        {
            if (_statement)
            {
                sqlite3_finalize(_statement);
            }
        }

        template<class... Args>
        void execute(const Args &... args)
        {
            reset();
            bind(bind_policy::TRANSIENT, args...);
            step();
        }

        template<class... Args>
        void execute(bind_policy policy, const Args &... args)
        {
            reset();
            bind(policy, args...);
            step();
        }

        template<class... Args>
        bool fetch(Args &... args)
        {
            if (!_can_fetch)
            {
                step();
            }

            if (_can_fetch)
            {
                column(args...);
                _can_fetch = false;

                return true;
            }

            return false;
        }

    private:
        void reset()
        {
            auto res = sqlite3_reset(_statement);
            if (res != SQLITE_OK)
            {
                throw exception(_statement);
            }
        }

        void step()
        {
            auto res = sqlite3_step(_statement);
            if (res != SQLITE_ROW && res != SQLITE_DONE)
            {
                throw exception(_statement);
            }
            _can_fetch = res == SQLITE_ROW;
        }

        template<int Index = 1>
        void bind(bind_policy)
        {
        }

        template<int Index = 1, class T, class... Args>
        void bind(bind_policy policy, const T& arg, const Args &... args)
        {
            auto res = type_traits<T>::bind(_statement, Index, arg, policy);
            if (res != SQLITE_OK)
            {
                throw exception(_statement);
            }

            bind<Index + 1>(policy, args...);
        }

        template<int Column = 0>
        void column()
        {
        }

        template<int Column = 0, class T, class... Args>
        void column(T& arg, Args &... args)
        {
            type_traits<T>::column(_statement, Column, arg);
            column<Column + 1>(args...);
        }

        bool _can_fetch = false;
        sqlite3_stmt* _statement = nullptr;
    };

    enum class transaction_type
    {
        DEFERRED,
        IMMEDIATE,
        EXCLUSIVE
    };

    class db
    {
    public:
        db(std::string_view filename, int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
        {
            auto res = sqlite3_open_v2(filename.data(), &_db, flags, nullptr);
            if (res != SQLITE_OK)
            {
                sqlite3_close_v2(_db);
                throw exception(_db);
            }
        }
        db(db&& another)
        {
            std::swap(_db, another._db);
        }
        db(const db&) = delete;

        db& operator=(db&& another)
        {
            std::swap(_db, another._db);
            return *this;
        }
        db& operator=(const db&) = delete;

        ~db()
        {
            if (_db)
            {
                sqlite3_close_v2(_db);
            }
        }

        sqlite3* native_handle()
        {
            return _db;
        }

        void begin(transaction_type type = transaction_type::DEFERRED)
        {
            switch (type)
            {
            default:
            case transaction_type::DEFERRED:
                execute("BEGIN DEFERRED TRANSACTION");
                break;
            case transaction_type::IMMEDIATE:
                execute("BEGIN IMMEDIATE TRANSACTION");
                break;
            case transaction_type::EXCLUSIVE:
                execute("BEGIN EXCLUSIVE TRANSACTION");
                break;
            }
        }

        void commit()
        {
            execute("COMMIT TRANSACTION");
        }

        void rollback()
        {
            execute("ROLLBACK TRANSACTION");
        }

        [[nodiscard]] statement prepare(std::string_view sql, unsigned int prepare_flags = SQLITE_PREPARE_PERSISTENT)
        {
            return statement(_db, sql, prepare_flags);
        }

        template<class... Args>
        statement execute(std::string_view sql, const Args &... args)
        {
            statement s(_db, sql);
            s.execute(args...);

            return s;
        }

    private:
        sqlite3* _db = nullptr;
    };

    template<>
    struct type_traits<bool>
    {
        static int bind(sqlite3_stmt* statement, int index, bool arg, bind_policy)
        {
            return sqlite3_bind_int(statement, index, arg ? 1 : 0);
        }

        static void column(sqlite3_stmt* statement, int column, bool& arg)
        {
            arg = sqlite3_column_int(statement, column) != 0;
        }
    };

    template<class T>
    struct type_traits<T, std::enable_if_t<std::is_integral_v<T> && sizeof(T) <= 4>>
    {
        static int bind(sqlite3_stmt* statement, int index, T arg, bind_policy)
        {
            return sqlite3_bind_int(statement, index, static_cast<int>(arg));
        }

        static void column(sqlite3_stmt* statement, int column, T& arg)
        {
            arg = static_cast<T>(sqlite3_column_int(statement, column));
        }
    };

    template<class T>
    struct type_traits<T, std::enable_if_t<std::is_integral_v<T> && sizeof(T) == 8>>
    {
        static int bind(sqlite3_stmt* statement, int index, T arg, bind_policy)
        {
            return sqlite3_bind_int64(statement, index, static_cast<int64_t>(arg));
        }

        static void column(sqlite3_stmt* statement, int column, T& arg)
        {
            arg = static_cast<T>(sqlite3_column_int64(statement, column));
        }
    };

    template<class T>
    struct type_traits<T, std::enable_if_t<std::is_enum_v<T>>>
    {
        using type = std::underlying_type_t<T>;

        static int bind(sqlite3_stmt* statement, int index, T arg, bind_policy bind_policy)
        {
            return type_traits<type>::bind(statement, index, static_cast<type>(arg), bind_policy);
        }

        static void column(sqlite3_stmt* statement, int column, T& arg)
        {
            type tmp;
            type_traits<type>::column(statement, column, tmp);
            arg = static_cast<T>(tmp);
        }
    };

    template<>
    struct type_traits<double>
    {
        static int bind(sqlite3_stmt* statement, int index, double arg, bind_policy)
        {
            return sqlite3_bind_double(statement, index, arg);
        }

        static void column(sqlite3_stmt* statement, int column, double& arg)
        {
            arg = sqlite3_column_double(statement, column);
        }
    };

    template<>
    struct type_traits<const char*>
    {
        static int bind(sqlite3_stmt* statement, int index, const char* arg, bind_policy policy)
        {
            return sqlite3_bind_text(statement, index, arg, -1, policy == bind_policy::STATIC ? SQLITE_STATIC : SQLITE_TRANSIENT);
        }
    };

    template<int Size>
    struct type_traits<char[Size]>
    {
        static int bind(sqlite3_stmt* statement, int index, const char(&arg)[Size], bind_policy policy)
        {
            return sqlite3_bind_text(statement, index, arg, Size - 1, policy == bind_policy::STATIC ? SQLITE_STATIC : SQLITE_TRANSIENT);
        }

        static void column(sqlite3_stmt* statement, int column, char(&arg)[Size])
        {
            auto data = sqlite3_column_text(statement, column);
            if (data)
            {
                strncpy_s(arg, reinterpret_cast<const char*>(data), Size - 1);
            }
        }
    };

    template<>
    struct type_traits<std::string>
    {
        static int bind(sqlite3_stmt* statement, int index, const std::string& arg, bind_policy policy)
        {
            return sqlite3_bind_text(statement, index, arg.data(), static_cast<int>(arg.size()), policy == bind_policy::STATIC ? SQLITE_STATIC : SQLITE_TRANSIENT);
        }

        static void column(sqlite3_stmt* statement, int column, std::string& arg)
        {
            auto data = sqlite3_column_text(statement, column);
            auto size = sqlite3_column_bytes(statement, column);
            arg.assign(reinterpret_cast<const char*>(data), size);
        }
    };

    template<>
    struct type_traits<std::nullptr_t>
    {
        static int bind(sqlite3_stmt* statement, int index, const std::nullptr_t& arg, bind_policy)
        {
            return sqlite3_bind_null(statement, index);
        }
    };

    template<class T>
    struct type_traits<std::optional<T>>
    {
        static int bind(sqlite3_stmt* statement, int index, const std::optional<T>& arg, bind_policy policy)
        {
            if (arg)
            {
                return type_traits<T>::bind(statement, index, *arg, policy);
            }
            else
            {
                return sqlite3_bind_null(statement, index);
            }
        }

        static void column(sqlite3_stmt* statement, int column, std::optional<T>& arg)
        {
            if (sqlite3_column_type(statement, column) != SQLITE_NULL)
            {
                type_traits<T>::column(statement, column, arg.emplace());
            }
            else
            {
                arg = std::nullopt;
            }
        }
    };
}