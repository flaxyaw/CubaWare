#pragma once

#include "sqlite3_wrapper.h"

namespace sqlite3_wrapper
{
    class migrations
    {
    public:
        template<class T, size_t MigrationsCount>
        static void apply_migrations(db& db, const std::array<T, MigrationsCount>& migrations)
        {
            create_version_info_table(db);
            auto last_version = get_last_applied_version(db);
            if (migrations.size() > last_version)
            {
                db.begin();
                for (auto it = migrations.begin() + last_version; it != migrations.end(); ++it)
                {
                    std::string_view migration(*it);
                    size_t start = 0;
                    size_t end = migration.find(';');

                    while (end != std::string_view::npos)
                    {
                        db.execute(migration.substr(start, end - start));
                        start = end + 1;
                        end = migration.find(';', start);
                    }

                    if (start < migration.size())
                    {
                        db.execute(migration.substr(start));
                    }

                    db.execute(R"(
                        INSERT INTO VersionInfo(Version, AppliedOn)
                        VALUES (?, datetime('now'))
                    )", std::distance(migrations.begin(), it) + 1);
                }
                db.commit();
            }
        }

    private:
        static void create_version_info_table(db& db)
        {
            db.execute(R"(
                CREATE TABLE IF NOT EXISTS VersionInfo
                (
                    Version INTEGER NOT NULL,
                    AppliedOn DATETIME,
                    Description TEXT
                )
            )");
        }

        static int get_last_applied_version(db& db)
        {
            auto statement = db.execute(R"(
                SELECT MAX(Version)
                FROM VersionInfo
            )");

            int last_version = 0;
            statement.fetch(last_version);

            return last_version;
        }
    };
}