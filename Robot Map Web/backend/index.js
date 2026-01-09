const express = require("express");
const cors = require("cors");
const { SerialPort } = require("serialport");
const { v4: uuidv4 } = require("uuid");
const db = require("./db/db.js");

const app = express();
app.use(cors());
app.use(express.json());

// ===== Serial =====lppp
const port = new SerialPort({ path: "COM9", baudRate: 115200 });

port.on("open", () => console.log("✅ Serial connected to ESP32"));
port.on("error", (err) => console.error("❌ Serial error:", err.message));

// ===== Health =====
app.get("/", (req, res) => {
  res.send("Backend is running 🚀");
});

// ===== LED API =====
app.post("/led", (req, res) => {
  const { state } = req.body;
  const cmd = state ? "LED1\n" : "LED0\n";
  port.write(cmd, (err) => {
    if (err) return res.status(500).json({ ok: false, error: err.message });
    res.json({ ok: true });
  });
});

// ===== POST position (บันทึกลง DB) =====
app.post("/positions", (req, res) => {
  try {
    const { x, y, name } = req.body;
    if (x == null || y == null || !name) {
      return res.status(400).json({ error: "x,y,name required" });
    }

    db.prepare(`
      INSERT INTO positions (id, x, y, name)
      VALUES (?, ?, ?, ?)
    `).run(uuidv4(), x, y, name);

    res.json({ ok: true });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// ===== GET positions (อ่านจาก DB) =====
app.get("/positions", (req, res) => {
  try {
    const rows = db
      .prepare(`SELECT id, x, y, name FROM positions ORDER BY rowid DESC LIMIT 50`)
      .all();
    res.json(rows);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// ===== START SERVER (ไว้ล่างสุด) =====
app.listen(8000, () => {
  console.log("🌐 Backend running at http://localhost:8000");
});
