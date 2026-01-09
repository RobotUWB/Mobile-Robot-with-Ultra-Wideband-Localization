import { useEffect, useRef, useState } from "react";

/* ================== CONFIG ================== */
const ESP32_IP = "192.168.88.16";
const SSE_URL = `http://${ESP32_IP}/pose`;
const CMD_URL = `http://${ESP32_IP}/cmd`;

const FIELD_W = 3000; // mm
const FIELD_H = 2000; // mm

const ZOOM_MIN = 0.05;
const ZOOM_MAX = 0.25;
const ZOOM_STEP = 0.01;
const ANCHORS = [
  { id: "A1", x_mm: 0, y_mm: 2000 },
  { id: "A2", x_mm: 3000, y_mm: 2000 },
  { id: "A3", x_mm: 0, y_mm: 0 },
  { id: "A4", x_mm: 3000, y_mm: 0 },
];

/* ================== UTILS ================== */
const clamp = (n, a, b) => Math.max(a, Math.min(b, n));
const mmToPx = (mm, scale) => mm * scale;

/* ================== APP ================== */
export default function App() {
  /* ---------- canvas & view ---------- */
  const canvasRef = useRef(null);
  const [scale, setScale] = useState(0.12);
  const scaleRef = useRef(scale);
  useEffect(() => {
    scaleRef.current = scale;
  }, [scale]);

  const [center] = useState({
    x_mm: FIELD_W / 2,
    y_mm: FIELD_H / 2,
  });

  const centerRef = useRef(center);
  useEffect(() => {
    centerRef.current = center;
  }, [center]);
  const stopRobot = () => {
    sendCmd({ cmd: "STOP" }); // หยุดหุ่นจริง
  };
  /* ---------- robot pose ---------- */
  const poseRealRef = useRef(null);                 // ค่าจริงจาก ESP32
  const [lastPose, setLastPose] = useState(null);
  // 👉 state สำหรับแสดงผล x,y ด้านบน (realtime)
  const [uiPose, setUiPose] = useState({ x_mm: 0, y_mm: 0 });

  /* ---------- status ---------- */
  const [connected, setConnected] = useState(false);
  const [statusText, setStatusText] = useState("Connecting…");
  /* ---------- UI toggle ---------- */
  const [showPoseLabel, setShowPoseLabel] = useState(true);
  const showPoseLabelRef = useRef(showPoseLabel);

  useEffect(() => {
    showPoseLabelRef.current = showPoseLabel;
  }, [showPoseLabel]);

  /* ================== SEND CMD ================== */
  const sendCmd = async (payload) => {
    try {
      await fetch(CMD_URL, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
    } catch (e) {
      console.error("CMD error", e);
    }
  };

  /* ================== CANVAS CLICK ================== */
  const onCanvasClick = () => {
    // ❌ ปิดนำทางชั่วคราว
  };

  /* ================== SSE ================== */
  useEffect(() => {
    const es = new EventSource(SSE_URL);

    es.onopen = () => {
      setConnected(true);
      setStatusText("Connected (live)");
    };

    es.onerror = () => {
      setConnected(false);
      setStatusText("Disconnected… reconnecting");
    };

    es.addEventListener("pose", (e) => {
      try {
        const j = JSON.parse(e.data);
        poseRealRef.current = { x_mm: j.x_mm, y_mm: j.y_mm };
        setUiPose({ x_mm: j.x_mm, y_mm: j.y_mm }); // realtime 1:1
        setLastPose(j);

      } catch { }
    });

    return () => es.close();
  }, []);
  /* ================== DRAW LOOP ================== */
  useEffect(() => {
    const cvs = canvasRef.current;
    if (!cvs) return;
    const ctx = cvs.getContext("2d");

    let raf = 0;

    const draw = () => {
      const W = cvs.width;
      const H = cvs.height;
      const cx = W / 2;
      const cy = H / 2;

      const centerNow = centerRef.current;
      const scaleNow = scaleRef.current;

      const toPx = (x_mm, y_mm) => ({
        px: cx + mmToPx(x_mm - centerNow.x_mm, scaleNow),
        py: cy - mmToPx(y_mm - centerNow.y_mm, scaleNow),
      });
      /* ---- clear ---- */
      ctx.fillStyle = "#0b1220";
      ctx.fillRect(0, 0, W, H);

      /* ---- field ---- */
      const p1 = toPx(0, 0);
      const p2 = toPx(FIELD_W, 0);
      const p3 = toPx(FIELD_W, FIELD_H);
      const p4 = toPx(0, FIELD_H);


      ctx.strokeStyle = "rgba(34,197,94,0.6)";
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(p1.px, p1.py);
      ctx.lineTo(p2.px, p2.py);
      ctx.lineTo(p3.px, p3.py);
      ctx.lineTo(p4.px, p4.py);
      ctx.closePath();
      ctx.stroke();
      /* ---- anchors ---- */
      ctx.font = "12px Arial";
      ctx.textAlign = "left";
      ctx.textBaseline = "top";

      for (const a of ANCHORS) {
        const { px, py } = toPx(a.x_mm, a.y_mm);

        // จุด anchor
        ctx.fillStyle = "#e5e7eb";
        ctx.beginPath();
        ctx.arc(px, py, 5, 0, Math.PI * 2);
        ctx.fill();

        // label
        ctx.fillStyle = "rgba(255,255,255,0.85)";
        ctx.fillText(a.id, px + 8, py - 14);
      }
      /* ---- robot ---- */
      if (!poseRealRef.current) {
        raf = requestAnimationFrame(draw);
        return;
      }

      const { x_mm, y_mm } = poseRealRef.current;
      const { px, py } = toPx(x_mm, y_mm);

      ctx.fillStyle = "#22c55e55";
      ctx.beginPath();
      ctx.arc(px, py, 18, 0, Math.PI * 2);
      ctx.fill();

      ctx.fillStyle = "#22c55e";
      ctx.beginPath();
      ctx.arc(px, py, 7, 0, Math.PI * 2);
      ctx.fill();
      /* ---- robot pose label (x,y) ---- */
      if (showPoseLabelRef.current) {

        // ถ้ามีค่าจริงจาก ESP32 ใช้ real
        // ถ้ายังไม่มา ใช้ค่า smooth แทน
        const src = poseRealRef.current;

        if (src) {
          ctx.font = "12px Arial";
          ctx.textAlign = "center";
          ctx.textBaseline = "bottom";

          const label = `x:${src.x_mm.toFixed(0)}  y:${src.y_mm.toFixed(0)}`;

          const padding = 4;
          const metrics = ctx.measureText(label);
          const w = metrics.width + padding * 2;
          const h = 16;

          ctx.fillStyle = "rgba(15,23,42,0.85)";
          ctx.strokeStyle = "#38bdf8";
          ctx.lineWidth = 1;

          ctx.beginPath();
          ctx.roundRect(px - w / 2, py - 26 - h, w, h, 6);
          ctx.fill();
          ctx.stroke();

          ctx.fillStyle = "#e5e7eb";
          ctx.fillText(label, px, py - 28);
        }
      }

      raf = requestAnimationFrame(draw);
    };

    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, []);

  /* ================== UI ================== */
  return (
    <div
      style={{
        minHeight: "100vh",
        background: "#060a13",
        color: "#fff",
        fontFamily: "Arial",
        display: "flex",
        justifyContent: "center",   // ⬅️ จัดกลางแนวนอนทั้งหน้า
      }}
    >
      {/* ===== Dashboard Container ===== */}
      <div
        style={{
          width: "100%",
          maxWidth: 1200,           // ⬅️ ไม่ให้กว้างเกิน
          padding: 18,
        }}
      >
        <h2>Robot Live Map</h2>
        <div style={{ opacity: 0.8 }}>{statusText}</div>

        <div style={{ margin: "10px 0" }}>
          Position: x {uiPose.x_mm.toFixed(2)}, y {uiPose.y_mm.toFixed(2)}
        </div>

        <div style={{ marginBottom: 10, display: "flex", gap: 10 }}>
          <button onClick={() => setScale((s) => clamp(s - ZOOM_STEP, ZOOM_MIN, ZOOM_MAX))}>−</button>
          <input
            type="range"
            min={ZOOM_MIN}
            max={ZOOM_MAX}
            step={ZOOM_STEP}
            value={scale}
            onChange={(e) => setScale(parseFloat(e.target.value))}
          />
          <button onClick={() => setScale((s) => clamp(s + ZOOM_STEP, ZOOM_MIN, ZOOM_MAX))}>+</button>

          {/* ⛔ STOP */}
          <button
            onClick={stopRobot}
            style={{
              marginLeft: 20,
              background: "#ef4444",
              color: "#fff",
              borderRadius: 8,
              padding: "6px 14px",
              fontWeight: "bold",
              border: "none",
              cursor: "pointer",
            }}
          >
            ⛔ STOP
          </button>
          <button
            onClick={() => setShowPoseLabel(v => !v)}
            style={{
              background: "#334155",
              color: "#fff",
              borderRadius: 8,
              padding: "6px 14px",
              border: "1px solid #475569",
              cursor: "pointer",
            }}
          >
            {showPoseLabel ? "Hide x,y" : "Show x,y"}
          </button>

        </div>


        {/* ===== MAP CENTER ===== */}
        <div
          style={{
            display: "flex",
            justifyContent: "center",
            marginTop: 16,
          }}
        >
          <canvas
            ref={canvasRef}
            width={900}
            height={520}
            onClick={onCanvasClick}
            style={{
              maxWidth: "100%",
              borderRadius: 14,
              border: "1px solid rgba(255,255,255,0.10)",
              background: "#0b1220",
              cursor: "crosshair",
            }}
          />
        </div>
      </div>
    </div>
  );
}