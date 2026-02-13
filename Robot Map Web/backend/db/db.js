const path = require("path");
const Database = require("better-sqlite3");

// 👇 ชี้ path ไปที่ data.db จริงแบบชัวร์
const db = new Database(
  path.join(__dirname, "data.db")
);

// ✅ Auto Create Table if not exists
db.exec(`
  CREATE TABLE IF NOT EXISTS positions (
    id TEXT PRIMARY KEY,
    x REAL,
    y REAL,
    name TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
  )
`);

module.exports = db;
