import React, { useEffect, useRef, useState } from "react";

/* ================== GLOBAL STYLES (INJECTED) ================== */
const GlobalStyles = () => (
  <style>{`
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&family=JetBrains+Mono:wght@400;700&display=swap');
    
    body {
      margin: 0;
      background-color: #0b1a3e;
      font-family: 'Inter', sans-serif;
      -webkit-font-smoothing: antialiased;
    }

    input[type=range] {
      -webkit-appearance: none;
      width: 100%;
      background: transparent;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      height: 16px;
      width: 16px;
      border-radius: 50%;
      background: #38bdf8;
      cursor: pointer;
      margin-top: -6px;
      box-shadow: 0 0 10px rgba(56, 189, 248, 0.5);
    }
    input[type=range]::-webkit-slider-runnable-track {
      width: 100%;
      height: 4px;
      cursor: pointer;
      background: #334155;
      border-radius: 2px;
    }
    
    /* General Button Effects */
    .btn-hover:hover { filter: brightness(1.2); transform: translateY(-1px); }
    .btn-active:active { transform: translateY(1px); }
    
    /* Input Styling */
    .cal-input {
      background: #000;
      border: 1px solid #334155;
      color: #fff;
      font-family: 'JetBrains Mono', monospace;
      font-size: 14px;
      padding: 6px 10px;
      border-radius: 6px;
      width: 60px;
      text-align: center;
      outline: none;
    }
    .cal-input:focus { border-color: #3b82f6; }

    @keyframes spin { 100% { transform: rotate(360deg); } }
    .spin-anim { animation: spin 1s linear infinite; }
  `}</style>
);

/* ================== CONFIG ================== */
const ESP32_IP = "192.168.88.16"; 
const SSE_URL = `http://${ESP32_IP}/pose`;
const CMD_URL = `http://${ESP32_IP}/cmd`;

const FIELD_W = 3000;
const FIELD_H = 2000;

const ZOOM_MIN = 0.05;
const ZOOM_MAX = 0.40;
const ZOOM_STEP = 0.01;

const INITIAL_ANCHORS = [
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
  const [scale, setScale] = useState(0.15);
  const scaleRef = useRef(scale);
  useEffect(() => { scaleRef.current = scale; }, [scale]);

  const [center] = useState({ x_mm: FIELD_W / 2, y_mm: FIELD_H / 2 });
  const centerRef = useRef(center);
  useEffect(() => { centerRef.current = center; }, [center]);

  /* ---------- Anchors State ---------- */
  const [anchors, setAnchors] = useState(INITIAL_ANCHORS);
  const anchorsRef = useRef(anchors);
  useEffect(() => { anchorsRef.current = anchors; }, [anchors]);

  const [anchorRanges, setAnchorRanges] = useState({
    A1: 0, A2: 0, A3: 0, A4: 0
  });

  /* ---------- Robot Logic ---------- */
  const [isPaused, setIsPaused] = useState(false);

  /* ---------- Calibration Logic (NEW) ---------- */
  const [calX, setCalX] = useState("1.50");
  const [calY, setCalY] = useState("1.00");
  const [calMsg, setCalMsg] = useState(""); // e.g. "SUCCESS! (PRESS SAVE)"
  const [calMsgColor, setCalMsgColor] = useState("#22c55e"); // Green by default

  const stopRobot = () => {
    sendCmd({ cmd: "STOP" });
    setIsPaused(false);
  };

  const togglePause = () => {
    if (isPaused) {
      sendCmd({ cmd: "RESUME" });
      setIsPaused(false);
    } else {
      sendCmd({ cmd: "PAUSE" });
      setIsPaused(true);
    }
  };

  /* ---------- API ACTIONS ---------- */
  const sendCmd = async (payload) => {
    try {
      console.log("Sending CMD:", payload);
      await fetch(CMD_URL, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
    } catch (e) {
      console.error("CMD error", e);
    }
  };

  /* --- Calibration Handlers --- */
  const handleCalibrate = () => {
    // ส่งค่า Calibrate ไปที่ ESP32
    const xVal = parseFloat(calX);
    const yVal = parseFloat(calY);
    if (isNaN(xVal) || isNaN(yVal)) {
        setCalMsg("INVALID INPUT");
        setCalMsgColor("#ef4444");
        return;
    }
    
    sendCmd({ cmd: "CAL_POINT", x: xVal, y: yVal });
    
    // จำลอง UI Feedback
    setCalMsg("PROCESSING...");
    setCalMsgColor("#eab308"); // Yellow
    
    // สมมติว่าสำเร็จ (ในของจริงอาจจะรอ SSE ตอบกลับ)
    setTimeout(() => {
        setCalMsg("SUCCESS! (PRESS SAVE)");
        setCalMsgColor("#22c55e"); // Green
    }, 800);
  };

  const handleSaveCal = () => {
    sendCmd({ cmd: "SAVE_CAL" });
    setCalMsg("SAVED TO FLASH");
    setCalMsgColor("#3b82f6"); // Blue
    setTimeout(() => setCalMsg(""), 3000);
  };

  const handleResetCal = () => {
    sendCmd({ cmd: "RESET_CAL" });
    setCalMsg("CALIBRATION RESET");
    setCalMsgColor("#ef4444"); // Red
    setTimeout(() => setCalMsg(""), 3000);
  };

  /* ---------- robot pose ---------- */
  const poseRealRef = useRef(null);
  const [lastPose, setLastPose] = useState(null);
  const [uiPose, setUiPose] = useState({ x_mm: 0, y_mm: 0 });

  /* ---------- status & logic ---------- */
  const [connected, setConnected] = useState(false);
  const [statusText, setStatusText] = useState("Waiting for connection...");
  const [refreshKey, setRefreshKey] = useState(0); 
  const [isRefreshing, setIsRefreshing] = useState(false); 

  /* ---------- UI toggle ---------- */
  const [showPoseLabel, setShowPoseLabel] = useState(true);
  const showPoseLabelRef = useRef(showPoseLabel);
  useEffect(() => { showPoseLabelRef.current = showPoseLabel; }, [showPoseLabel]);

  const handleRefresh = () => {
    setIsRefreshing(true);
    setConnected(false);
    setStatusText("Refreshing connection...");
    setTimeout(() => {
      setRefreshKey(prev => prev + 1); 
      setIsRefreshing(false);
    }, 500);
  };

  /* ================== SSE LOGIC ================== */
  useEffect(() => {
    console.log("Connecting SSE to:", SSE_URL);
    const es = new EventSource(SSE_URL);
    
    es.onopen = () => {
      setConnected(true);
      setStatusText("System Online");
    };
    
    es.onerror = () => {
      setConnected(false);
      setStatusText("Disconnected... retrying");
    };
    
    es.addEventListener("pose", (e) => {
      try {
        const j = JSON.parse(e.data);
        if (j.x_mm !== undefined && j.y_mm !== undefined) {
          poseRealRef.current = { x_mm: j.x_mm, y_mm: j.y_mm };
          setUiPose({ x_mm: j.x_mm, y_mm: j.y_mm });
          setLastPose(j);
        }
        if (j.anchors && Array.isArray(j.anchors)) {
            setAnchors(j.anchors);
        }
        if (j.ranges && Array.isArray(j.ranges)) {
           setAnchorRanges({
             A1: j.ranges[0] || 0,
             A2: j.ranges[1] || 0,
             A3: j.ranges[2] || 0,
             A4: j.ranges[3] || 0
           });
        } else if (j.A1 !== undefined) {
           setAnchorRanges({ A1: j.A1, A2: j.A2, A3: j.A3, A4: j.A4 });
        }
      } catch (err) { console.error(err); }
    });

    return () => es.close();
  }, [refreshKey]); 

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
      const showTags = showPoseLabelRef.current; 

      const toPx = (x_mm, y_mm) => ({
        px: cx + mmToPx(x_mm - centerNow.x_mm, scaleNow),
        py: cy - mmToPx(y_mm - centerNow.y_mm, scaleNow),
      });

      /* ---- Clear & Grid ---- */
      ctx.fillStyle = "#1a283f"; 
      ctx.fillRect(0, 0, W, H);

      ctx.strokeStyle = "rgba(56, 189, 248, 0.08)";
      ctx.lineWidth = 1;
      ctx.beginPath();
      const GRID_SIZE = 500;
      for (let x = 0; x <= FIELD_W; x += GRID_SIZE) {
         const top = toPx(x, FIELD_H);
         const bottom = toPx(x, 0);
         ctx.moveTo(top.px, top.py);
         ctx.lineTo(bottom.px, bottom.py);
      }
      for (let y = 0; y <= FIELD_H; y += GRID_SIZE) {
         const left = toPx(0, y);
         const right = toPx(FIELD_W, y);
         ctx.moveTo(left.px, left.py);
         ctx.lineTo(right.px, right.py);
      }
      ctx.stroke();

      /* ---- Boundary ---- */
      const p1 = toPx(0, 0);
      const p2 = toPx(FIELD_W, 0);
      const p3 = toPx(FIELD_W, FIELD_H);
      const p4 = toPx(0, FIELD_H);

      ctx.strokeStyle = "rgba(56, 189, 248, 0.5)";
      ctx.lineWidth = 2;
      ctx.setLineDash([]);
      ctx.beginPath();
      ctx.moveTo(p1.px, p1.py);
      ctx.lineTo(p2.px, p2.py);
      ctx.lineTo(p3.px, p3.py);
      ctx.lineTo(p4.px, p4.py);
      ctx.closePath();
      ctx.stroke();

      /* ---- Anchors ---- */
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      for (const a of anchorsRef.current) {
        const { px, py } = toPx(a.x_mm, a.y_mm);
        ctx.shadowBlur = 10;
        ctx.shadowColor = "rgba(234, 179, 8, 0.5)";
        ctx.fillStyle = "#eab308";
        ctx.beginPath();
        ctx.arc(px, py, 4, 0, Math.PI * 2);
        ctx.fill();
        ctx.shadowBlur = 0;

        if (showTags) {
          const label = `${a.id} (${a.x_mm},${a.y_mm})`; 
          ctx.font = "10px JetBrains Mono";
          const metrics = ctx.measureText(label);
          const bgW = metrics.width + 10;
          const bgH = 20;
          ctx.fillStyle = "rgba(15, 23, 42, 0.9)";
          ctx.strokeStyle = "rgba(234, 179, 8, 0.5)"; 
          ctx.lineWidth = 1;
          const lx = px - bgW / 2;
          const ly = py - 30; 
          ctx.beginPath();
          ctx.roundRect(lx, ly, bgW, bgH, 4);
          ctx.fill();
          ctx.stroke();
          ctx.fillStyle = "#facc15"; 
          ctx.fillText(label, px, ly + bgH/2 + 1);
        }
      }

      /* ---- Robot ---- */
      if (poseRealRef.current) {
        const { x_mm, y_mm } = poseRealRef.current;
        const { px, py } = toPx(x_mm, y_mm);

        ctx.fillStyle = "rgba(34, 197, 94, 0.15)";
        ctx.beginPath();
        ctx.arc(px, py, 20, 0, Math.PI * 2);
        ctx.fill();

        ctx.shadowBlur = 15;
        ctx.shadowColor = "#22c55e";
        ctx.fillStyle = "#4ade80";
        ctx.beginPath();
        ctx.arc(px, py, 6, 0, Math.PI * 2);
        ctx.fill();
        ctx.shadowBlur = 0;

        if (showTags) {
          const label = `X:${x_mm.toFixed(0)} Y:${y_mm.toFixed(0)}`;
          ctx.font = "11px JetBrains Mono";
          const metrics = ctx.measureText(label);
          const bgW = metrics.width + 12;
          const bgH = 22;
          ctx.fillStyle = "rgba(15, 23, 42, 0.9)";
          ctx.strokeStyle = "rgba(34, 197, 94, 0.5)";
          ctx.lineWidth = 1;
          const lx = px - bgW / 2;
          const ly = py - 35;
          ctx.beginPath();
          ctx.roundRect(lx, ly, bgW, bgH, 4);
          ctx.fill();
          ctx.stroke();
          ctx.fillStyle = "#4ade80";
          ctx.fillText(label, px, ly + bgH/2 + 1);
        }
      }
      raf = requestAnimationFrame(draw);
    };
    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, []);

  /* ================== UI RENDER ================== */
  return (
    <>
      <GlobalStyles />
      <div style={{
        minHeight: "100vh",
        display: "flex",
        justifyContent: "center",
        alignItems: "center",
        padding: 20
      }}>
        
        <div style={{
          width: "100%",
          maxWidth: 1100,
          background: "rgba(30, 41, 59, 0.5)",
          backdropFilter: "blur(12px)",
          borderRadius: 24,
          border: "1px solid rgba(255,255,255,0.08)",
          padding: 32,
          boxShadow: "0 25px 50px -12px rgba(0, 0, 0, 0.5)"
        }}>

          {/* ----- Header Row ----- */}
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", marginBottom: 20 }}>
            <div>
              <h1 style={{ margin: 0, fontSize: 24, fontWeight: 700, color: "#f8fafc" }}>
                UWB Robot Localization
              </h1>
              <div style={{ display:"flex", alignItems: "center", gap: 8, marginTop: 6 }}>
                <div style={{
                  width: 8, height: 8, borderRadius: "50%",
                  background: connected ? "#22c55e" : "#ef4444",
                  boxShadow: connected ? "0 0 8px #22c55e" : "none"
                }} />
                <span style={{ fontSize: 13, color: connected ? "#86efac" : "#fca5a5" }}>
                  {statusText}
                </span>
              </div>
            </div>

            <div style={{ display: "flex", gap: 24, alignItems: "center" }}>
               {/* Anchor Status Panel */}
               <AnchorStatusPanel ranges={anchorRanges} />

               {/* Position Display */}
               <div style={{
                 display: "flex", gap: 24,
                 background: "#0f172a",
                 padding: "12px 24px",
                 borderRadius: 12,
                 border: "1px solid rgba(255,255,255,0.05)",
                 height: "fit-content"
               }}>
                 <DataValue label="POSITION X" value={uiPose.x_mm.toFixed(0)} unit="mm" />
                 <div style={{ width: 1, background: "#334155" }}></div>
                 <DataValue label="POSITION Y" value={uiPose.y_mm.toFixed(0)} unit="mm" />
               </div>
            </div>
          </div>

          {/* 🔥 [NEW] SYSTEM CALIBRATION PANEL 🔥 */}
          <div style={{
            background: "#0f172a",
            border: "1px solid #1e293b",
            borderRadius: 12,
            padding: "12px 20px",
            marginBottom: 20,
            display: "flex",
            alignItems: "center",
            justifyContent: "space-between",
            flexWrap: "wrap",
            gap: 16
          }}>
            <div style={{ display: "flex", flexDirection: "column" }}>
                <span style={{ color: "#94a3b8", fontSize: 12, fontWeight: 700, letterSpacing: 0.5 }}>
                    SYSTEM CALIBRATION
                </span>
                {/* Dynamic Status Message */}
                <span style={{ color: calMsgColor, fontSize: 13, fontWeight: 600, marginTop: 2, minHeight: 18 }}>
                    {calMsg}
                </span>
            </div>

            <div style={{ display: "flex", alignItems: "center", gap: 12 }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                    <span style={{ color: "#cbd5e1", fontSize: 13 }}>REF X:</span>
                    <input 
                        className="cal-input" 
                        value={calX} 
                        onChange={(e) => setCalX(e.target.value)} 
                    />
                </div>
                <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                    <span style={{ color: "#cbd5e1", fontSize: 13 }}>REF Y:</span>
                    <input 
                        className="cal-input" 
                        value={calY} 
                        onChange={(e) => setCalY(e.target.value)} 
                    />
                </div>
                <div style={{width: 1, height: 24, background: "#334155", margin: "0 8px"}}></div>
                
                {/* Calibration Buttons */}
                <button onClick={handleCalibrate} className="btn-hover" style={{
                    background: "#3b82f6", color: "#fff", border: "none",
                    borderRadius: 6, padding: "8px 16px", fontSize: 12, fontWeight: 700,
                    cursor: "pointer"
                }}>
                    CAL (POINT)
                </button>
                <button onClick={handleSaveCal} className="btn-hover" style={{
                    background: "#1e293b", color: "#e2e8f0", border: "1px solid #334155",
                    borderRadius: 6, padding: "8px 16px", fontSize: 12, fontWeight: 700,
                    cursor: "pointer"
                }}>
                    SAVE
                </button>
                <button onClick={handleResetCal} className="btn-hover" style={{
                    background: "#1e293b", color: "#ef4444", border: "1px solid #334155",
                    borderRadius: 6, padding: "8px 16px", fontSize: 12, fontWeight: 700,
                    cursor: "pointer"
                }}>
                    RESET
                </button>
            </div>
          </div>

          {/* ----- Canvas Container ----- */}
          <div style={{
            position: "relative",
            borderRadius: 16,
            overflow: "hidden",
            border: "1px solid rgba(56, 189, 248, 0.2)",
            background: "#1e293b", 
            boxShadow: "inset 0 0 40px rgba(0,0,0,0.3)"
          }}>
            <canvas
              ref={canvasRef}
              width={1036}
              height={600}
              style={{ width: "100%", display: "block", cursor: "crosshair" }}
            />
            
            {/* Top Right Controls */}
            <div style={{ position: "absolute", top: 16, right: 16, display: "flex", flexDirection: "column", gap: 8, alignItems: "flex-end" }}>
               <ToggleButton 
                 active={isRefreshing} 
                 onClick={handleRefresh}
                 label={<div style={{display: 'flex', alignItems: 'center', gap: 6}}><span className={isRefreshing ? "spin-anim" : ""} style={{fontSize: 14}}>↻</span>{isRefreshing ? "Refreshing..." : "Refresh"}</div>}
               />
               <ToggleButton 
                 active={showPoseLabel} 
                 onClick={() => setShowPoseLabel(!showPoseLabel)}
                 label={showPoseLabel ? "Hide Tags" : "Show Tags"} 
               />
            </div>
          </div>

          {/* ----- Bottom Control Bar ----- */}
          <div style={{ 
            display: "flex", justifyContent: "space-between", alignItems: "center", 
            marginTop: 24, paddingTop: 24, borderTop: "1px solid rgba(255,255,255,0.05)"
          }}>
            <div style={{ display: "flex", alignItems: "center", gap: 16 }}>
              <span style={{ fontSize: 12, color: "#94a3b8", fontWeight: 600, letterSpacing: 1 }}>ZOOM LEVEL</span>
              <div style={{ display: "flex", alignItems: "center", gap: 12, background: "rgba(255,255,255,0.03)", padding: "8px 16px", borderRadius: 30 }}>
                <IconButton onClick={() => setScale(s => clamp(s - ZOOM_STEP, ZOOM_MIN, ZOOM_MAX))}>−</IconButton>
                <input
                  type="range"
                  min={ZOOM_MIN} max={ZOOM_MAX} step={ZOOM_STEP}
                  value={scale}
                  onChange={(e) => setScale(parseFloat(e.target.value))}
                  style={{ width: 100 }}
                />
                <IconButton onClick={() => setScale(s => clamp(s + ZOOM_STEP, ZOOM_MIN, ZOOM_MAX))}>+</IconButton>
              </div>
            </div>

            <div style={{ display: "flex", gap: 12 }}>
              <button
                onClick={togglePause}
                className="btn-hover btn-active"
                style={{
                  background: isPaused ? "rgba(34, 197, 94, 0.2)" : "rgba(234, 179, 8, 0.2)",
                  color: isPaused ? "#4ade80" : "#facc15",
                  border: `1px solid ${isPaused ? "#22c55e" : "#eab308"}`,
                  borderRadius: 8,
                  padding: "12px 24px",
                  fontSize: 14, fontWeight: "700",
                  letterSpacing: 1, cursor: "pointer",
                  display: "flex", alignItems: "center", gap: 8
                }}
              >
                <span style={{ fontSize: 16 }}>{isPaused ? "▶" : "⏸"}</span>
                {isPaused ? "RESUME" : "PAUSE"}
              </button>

              <button
                onClick={stopRobot}
                className="btn-hover btn-active"
                style={{
                  background: "linear-gradient(135deg, #ef4444 0%, #b91c1c 100%)",
                  color: "white", border: "none", borderRadius: 8,
                  padding: "12px 32px", fontSize: 14, fontWeight: "700",
                  letterSpacing: 1, cursor: "pointer",
                  boxShadow: "0 4px 14px rgba(239, 68, 68, 0.4)",
                  display: "flex", alignItems: "center", gap: 8
                }}
              >
                <span style={{ fontSize: 18 }}>⛔</span> EMERGENCY STOP
              </button>
            </div>
          </div>

        </div>
      </div>
    </>
  );
}

/* ================== SUB COMPONENTS ================== */

const AnchorStatusPanel = ({ ranges }) => {
  return (
    <div style={{
      background: "#0f172a",
      border: "1px solid rgba(255,255,255,0.05)",
      borderRadius: 12,
      padding: "12px 16px",
      minWidth: 240
    }}>
      <div style={{ 
        fontSize: 12, color: "#94a3b8", fontWeight: 700, 
        marginBottom: 10, letterSpacing: 0.5, textTransform: "uppercase" 
      }}>
        Anchor Status
      </div>
      <div style={{ 
        display: "grid", 
        gridTemplateColumns: "1fr 1fr", 
        gap: "8px 24px"
      }}>
        <AnchorItem label="A1" hex="(0x84)" value={ranges.A1} />
        <AnchorItem label="A2" hex="(0x85)" value={ranges.A2} />
        <AnchorItem label="A3" hex="(0x86)" value={ranges.A3} />
        <AnchorItem label="A4" hex="(0x87)" value={ranges.A4} />
      </div>
    </div>
  );
};

const AnchorItem = ({ label, hex, value }) => (
  <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
    <div style={{ fontSize: 13, color: "#cbd5e1", fontFamily: "'Inter', sans-serif" }}>
      {label} <span style={{ color: "#64748b", fontSize: 11 }}>{hex}</span>
    </div>
    <div style={{ 
      fontSize: 14, 
      fontFamily: "'JetBrains Mono', monospace", 
      color: "#3b82f6", 
      fontWeight: 600 
    }}>
      {value ? Number(value).toFixed(2) : "0.00"} <span style={{fontSize: 11, color: "#3b82f6"}}>m</span>
    </div>
  </div>
);

const DataValue = ({ label, value, unit }) => (
  <div style={{ display: "flex", flexDirection: "column", minWidth: 80 }}>
    <span style={{ fontSize: 10, color: "#64748b", fontWeight: 700, letterSpacing: 0.5 }}>{label}</span>
    <span style={{ fontSize: 20, fontFamily: "'JetBrains Mono', monospace", color: "#f1f5f9", marginTop: 2 }}>
      {value} <span style={{ fontSize: 12, color: "#475569" }}>{unit}</span>
    </span>
  </div>
);

const IconButton = ({ children, onClick }) => (
  <button onClick={onClick} className="btn-hover" style={{
    width: 28, height: 28, borderRadius: "50%", border: "1px solid #475569",
    background: "transparent", color: "#cbd5e1", cursor: "pointer",
    display: "flex", alignItems: "center", justifyContent: "center", fontSize: 16
  }}>
    {children}
  </button>
);

const ToggleButton = ({ active, onClick, label }) => (
  <button onClick={onClick} className="btn-hover" style={{
    background: active ? "rgba(56, 189, 248, 0.2)" : "rgba(15, 23, 42, 0.6)",
    color: active ? "#38bdf8" : "#94a3b8",
    border: `1px solid ${active ? "#38bdf8" : "rgba(255,255,255,0.1)"}`,
    backdropFilter: "blur(4px)",
    borderRadius: 6,
    padding: "6px 12px",
    fontSize: 11, fontWeight: 600,
    cursor: "pointer", transition: "all 0.2s"
  }}>
    {label}
  </button>
);