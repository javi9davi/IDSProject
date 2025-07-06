//
// Created by root on 2/07/25.
//

#ifndef DATABASE_SERVICE_H
#define DATABASE_SERVICE_H

// SqliteHashDB.h
#pragma once

#include <string>
#include <ctime>
#include <sqlite3.h>

struct ProcessRecord {
    std::string name;
    std::string status;
    std::time_t lastChecked;
};

class SqliteHashDB {
public:
    /// Path puede ser relativo o absoluto
    SqliteHashDB(std::string  dbPath);
    ~SqliteHashDB();

    /// Devuelve true si la tabla existe o se creó con éxito
    bool initialize() const;

    /// Inserta o actualiza un registro
    bool setRecord(const std::string& hash,
                   const std::string& name,
                   const std::string& status,
                   std::time_t lastChecked) const;

    /// Recupera un registro; devuelve false si no existe
    bool getRecord(const std::string& hash, ProcessRecord& out) const;

private:
    sqlite3* db = nullptr;
    std::string path;

    bool exec(const std::string& sql) const;
};


#endif //DATABASE_SERVICE_H
