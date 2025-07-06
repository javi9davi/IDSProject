//
// Created by root on 2/07/25.
//

#include "SqliteHashDB.h"
#include <iostream>
#include <utility>

SqliteHashDB::SqliteHashDB(std::string  dbPath)
 : path(std::move(dbPath))
{
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "[ERROR] No se pudo abrir BD: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        db = nullptr;
    }
}

SqliteHashDB::~SqliteHashDB() {
    if (db) sqlite3_close(db);
}

bool SqliteHashDB::initialize() const
{
    if (!db) return false;
    const auto ddl = R"(
        CREATE TABLE IF NOT EXISTS process_hash (
            hash          TEXT PRIMARY KEY,
            name          TEXT NOT NULL,
            status        TEXT NOT NULL,
            last_checked  INTEGER NOT NULL
        );
    )";
    return exec(ddl);
}

bool SqliteHashDB::exec(const std::string& sql) const
{
    char* err = nullptr;
    if (const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err); rc != SQLITE_OK) {
        std::cerr << "[ERROR] SQLite exec: " << err << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool SqliteHashDB::setRecord(const std::string& hash,
                             const std::string& name,
                             const std::string& status,
                             const std::time_t lastChecked) const
{
    if (!db) return false;
    const char* upsert = R"(
        INSERT INTO process_hash(hash,name,status,last_checked)
        VALUES(?,?,?,?)
        ON CONFLICT(hash) DO UPDATE SET
          name = excluded.name,
          status = excluded.status,
          last_checked = excluded.last_checked;
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, upsert, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[ERROR] prepare setRecord: " << sqlite3_errmsg(db) << "\n";
        return false;
    }
    sqlite3_bind_text  (stmt, 1, hash.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 2, name.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 3, status.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 4, static_cast<sqlite3_int64>(lastChecked));

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "[ERROR] step setRecord: " << sqlite3_errmsg(db) << "\n";
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SqliteHashDB::getRecord(const std::string& hash, ProcessRecord& out) const
{
    if (!db) return false;
    const auto query = R"(
        SELECT name, status, last_checked
          FROM process_hash
         WHERE hash = ?;
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[ERROR] prepare getRecord: " << sqlite3_errmsg(db) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_TRANSIENT);

    if (const int rc = sqlite3_step(stmt); rc == SQLITE_ROW) {
        out.name        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        out.status      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out.lastChecked = static_cast<std::time_t>(sqlite3_column_int64(stmt, 2));
        sqlite3_finalize(stmt);
        return true;
    }
    sqlite3_finalize(stmt);
    return false;
}
