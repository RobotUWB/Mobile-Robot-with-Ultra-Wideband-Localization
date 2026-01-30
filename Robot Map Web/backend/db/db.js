const path = require("path");
const Database = require("better-sqlite3");

// 👇 ชี้ path ไปที่ data.db จริงแบบชัวร์
const db = new Database(
  path.join(__dirname, "data.db")
);

module.exports = db;
