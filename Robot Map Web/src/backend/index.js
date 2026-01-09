const express = require('express');
const cors = require('cors');
const { SerialPort } = require('serialport');

const app = express();
app.use(cors());
app.use(express.json());

// ===== Serial config =====
const port = new SerialPort({
  path: 'COM9',        // 🔴 เปลี่ยนถ้าไม่ใช่ COM9
  baudRate: 115200,
});

port.on('open', () => {
  console.log('✅ Serial connected to ESP32');
});

// ===== API รับ setpoint จากเว็บ =====
app.post('/setpoint', (req, res) => {
  const { dir, rpm, time } = req.body;

  // รูปแบบเดียวกับที่ ESP32 รับ
  const cmd = `${dir},${rpm},${time}\n`;
  port.write(cmd);

  console.log('➡ TX:', cmd.trim());
  res.json({ ok: true });
});

app.listen(3001, () => {
  console.log('🌐 Backend running at http://localhost:3001');
});
