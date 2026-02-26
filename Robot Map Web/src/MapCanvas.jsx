import React, { useEffect, useRef, useState } from "react";

const MapCanvas = ({
    anchors,
    ranges,
    pose,
    yawOffset,
    showTags,
    setShowTags,
    scale,
    setScale,
    ZOOM_MIN,
    ZOOM_MAX,
    ZOOM_STEP,
    FIELD_W,
    FIELD_H,
    onMapClick,
    onRouteComplete,
    navTarget,
    clearPathTrigger,
    showDeadZones,
    deadZones,
}) => {
    const canvasRef = useRef(null);
    const mouseRef = useRef({ x: 0, y: 0, active: false });

    // ✅ Target for Confirmation
    const [target, setTarget] = useState(null); // { x_m, y_m, px, py }

    // ✅ Tools System
    const [activeTool, setActiveTool] = useState("point"); // "point" | "draw"
    const [drawPath, setDrawPath] = useState([]); // [{x_m, y_m}]
    const drawPathRef = useRef([]); // Required for requestAnimationFrame closure
    const isDrawingRef = useRef(false);

    // ✅ Deadzones Props Refs
    const showDeadZonesRef = useRef(showDeadZones);
    const deadZonesRef = useRef(deadZones);

    // Refs for Animation Loop to avoid dependency staleness
    const scaleRef = useRef(scale);
    const poseRef = useRef(pose);
    const anchorsRef = useRef(anchors);
    const showTagsRef = useRef(showTags);
    const rangesRef = useRef(ranges);
    const yawOffsetRef = useRef(yawOffset);
    const navTargetRef = useRef(navTarget);

    // Sync Refs with Props
    useEffect(() => { scaleRef.current = scale; }, [scale]);
    useEffect(() => { anchorsRef.current = anchors; }, [anchors]);
    useEffect(() => { showTagsRef.current = showTags; }, [showTags]);
    useEffect(() => { rangesRef.current = ranges; }, [ranges]);
    useEffect(() => { poseRef.current = pose; }, [pose]);
    useEffect(() => { yawOffsetRef.current = yawOffset; }, [yawOffset]);
    useEffect(() => { navTargetRef.current = navTarget; }, [navTarget]);
    useEffect(() => { drawPathRef.current = drawPath; }, [drawPath]);
    useEffect(() => { showDeadZonesRef.current = showDeadZones; }, [showDeadZones]);
    useEffect(() => { deadZonesRef.current = deadZones; }, [deadZones]);

    // Clear path when requested by parent
    useEffect(() => {
        if (clearPathTrigger > 0) {
            setDrawPath([]);
            drawPathRef.current = [];
        }
    }, [clearPathTrigger]);

    /* --- Helper Functions (Lifted for Render usage) --- */
    const mmToPx = (mm, s) => mm * s;

    const getBoundsFromAnchors = (anchorsList) => {
        if (anchorsList && anchorsList.length) {
            let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
            for (const a of anchorsList) {
                if (typeof a.x_mm !== "number" || typeof a.y_mm !== "number") continue;
                minX = Math.min(minX, a.x_mm);
                maxX = Math.max(maxX, a.x_mm);
                minY = Math.min(minY, a.y_mm);
                maxY = Math.max(maxY, a.y_mm);
            }
            if (isFinite(minX) && isFinite(maxX) && isFinite(minY) && isFinite(maxY) && minX !== maxX && minY !== maxY) {
                return { minX, maxX, minY, maxY };
            }
        }
        return { minX: 0, maxX: FIELD_W, minY: 0, maxY: FIELD_H };
    };

    const toPx = (x_mm, y_mm, cx, cy, s, bounds) => {
        const midX = (bounds.minX + bounds.maxX) / 2;
        const midY = (bounds.minY + bounds.maxY) / 2;
        return {
            px: cx + mmToPx(x_mm - midX, s),
            py: cy + mmToPx(midY - y_mm, s),
        };
    };

    /* --- Canvas Drawing (ลด glow/ความ neon ให้ดูคนทำ) --- */
    useEffect(() => {
        const cvs = canvasRef.current;
        if (!cvs) return;
        const ctx = cvs.getContext("2d");
        let raf;

        const COLORS = {
            bg: "#0f172a",
            grid: "rgba(255,255,255,0.05)",
            boundary: "rgba(59,130,246,0.35)",
            anchorOn: "#16a34a",
            anchorOff: "#64748b",
            tag1: "#3b82f6",

            labelBg: "rgba(17,26,43,0.92)",
            labelBd: "rgba(255,255,255,0.10)",
            text: "#e5e7eb",
            muted: "#9ca3af",
        };

        const drawRobot = (x, y, size, color, yawDeg) => {
            ctx.save();
            ctx.translate(x, y);
            // ชดเชยให้ 0 องศาหันไปทาง "ซ้าย" (Front) และทวนเข็มตามค่า IMU
            const angleRad = ((180 - yawDeg) * Math.PI) / 180;
            ctx.rotate(angleRad);

            ctx.fillStyle = color;
            ctx.beginPath();
            ctx.moveTo(size * 1.5, 0);
            ctx.lineTo(-size, size);
            ctx.lineTo(-size, -size);
            ctx.closePath();
            ctx.fill();

            ctx.strokeStyle = "rgba(255,255,255,0.9)";
            ctx.lineWidth = 2;
            ctx.stroke();
            ctx.restore();
        };

        const drawTagLabel = (text, x, y, above = true) => {
            ctx.save();
            ctx.font = "600 12px Inter";
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";

            const padX = 10;
            const h = 22;
            const w = ctx.measureText(text).width + padX * 2;
            const bx = x - w / 2;
            const by = above ? y - 26 - h : y + 26;

            ctx.fillStyle = COLORS.labelBg;
            ctx.strokeStyle = COLORS.labelBd;
            ctx.lineWidth = 1;

            ctx.beginPath();
            ctx.roundRect(bx, by, w, h, 8);
            ctx.fill();
            ctx.stroke();

            ctx.fillStyle = COLORS.text;
            ctx.fillText(text, x, by + h / 2);
            ctx.restore();
        };

        const draw = () => {
            const W = cvs.width, H = cvs.height;
            const cx = W / 2, cy = H / 2;
            const s = scaleRef.current;
            const show = showTagsRef.current;
            const currentRanges = rangesRef.current;
            const anchorsNow = anchorsRef.current || [];
            const bounds = getBoundsFromAnchors(anchorsNow);

            // Clear
            ctx.fillStyle = COLORS.bg;
            ctx.fillRect(0, 0, W, H);

            // Grid
            ctx.strokeStyle = COLORS.grid;
            ctx.lineWidth = 1;
            ctx.beginPath();
            const STEP = 500;

            const xStart = Math.floor(bounds.minX / STEP) * STEP;
            const xEnd = Math.ceil(bounds.maxX / STEP) * STEP;
            for (let x = xStart; x <= xEnd; x += STEP) {
                let p1 = toPx(x, bounds.minY, cx, cy, s, bounds);
                let p2 = toPx(x, bounds.maxY, cx, cy, s, bounds);
                ctx.moveTo(p1.px, p1.py);
                ctx.lineTo(p2.px, p2.py);
            }

            const yStart = Math.floor(bounds.minY / STEP) * STEP;
            const yEnd = Math.ceil(bounds.maxY / STEP) * STEP;
            for (let y = yStart; y <= yEnd; y += STEP) {
                let p1 = toPx(bounds.minX, y, cx, cy, s, bounds);
                let p2 = toPx(bounds.maxX, y, cx, cy, s, bounds);
                ctx.moveTo(p1.px, p1.py);
                ctx.lineTo(p2.px, p2.py);
            }
            ctx.stroke();

            // Boundary
            const b1 = toPx(bounds.minX, bounds.minY, cx, cy, s, bounds);
            const b2 = toPx(bounds.maxX, bounds.minY, cx, cy, s, bounds);
            const b3 = toPx(bounds.maxX, bounds.maxY, cx, cy, s, bounds);
            const b4 = toPx(bounds.minX, bounds.maxY, cx, cy, s, bounds);

            ctx.strokeStyle = COLORS.boundary;
            ctx.setLineDash([8, 8]);
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.moveTo(b1.px, b1.py);
            ctx.lineTo(b2.px, b2.py);
            ctx.lineTo(b3.px, b3.py);
            ctx.lineTo(b4.px, b4.py);
            ctx.closePath();
            ctx.stroke();
            ctx.setLineDash([]);

            // Anchors
            for (const a of anchorsRef.current) {
                const { px, py } = toPx(a.x_mm, a.y_mm, cx, cy, s, bounds);

                const rangeVal = currentRanges[a.id] || 0;
                const isOnline = rangeVal > 0;

                // ✅ Draw Deadzones if toggled on
                if (showDeadZonesRef.current) {
                    let deadzoneRadiusMm = 0;
                    let startAngle = 0;
                    let endAngle = 0;

                    if (a.id === "A1") {
                        deadzoneRadiusMm = deadZonesRef.current["A1"] * 10;
                        startAngle = -Math.PI / 2;
                        endAngle = 0;
                    } else if (a.id === "A2") {
                        deadzoneRadiusMm = deadZonesRef.current["A2"] * 10;
                        startAngle = 0;
                        endAngle = Math.PI / 2;
                    } else if (a.id === "A3") {
                        deadzoneRadiusMm = deadZonesRef.current["A3"] * 10;
                        startAngle = Math.PI;
                        endAngle = 3 * Math.PI / 2;
                    } else if (a.id === "A4") {
                        deadzoneRadiusMm = deadZonesRef.current["A4"] * 10;
                        startAngle = Math.PI / 2;
                        endAngle = Math.PI;
                    }

                    if (deadzoneRadiusMm > 0) {
                        const pxRadius = mmToPx(deadzoneRadiusMm, s);
                        ctx.fillStyle = "rgba(239, 68, 68, 0.15)"; // Light red fill
                        ctx.strokeStyle = "rgba(239, 68, 68, 0.6)"; // Red border
                        ctx.lineWidth = 1.5;
                        ctx.beginPath();
                        ctx.moveTo(px, py);
                        ctx.arc(px, py, pxRadius, startAngle, endAngle);
                        ctx.closePath();
                        ctx.fill();
                        ctx.stroke();
                    }
                }

                ctx.fillStyle = isOnline ? COLORS.anchorOn : COLORS.anchorOff;
                ctx.beginPath();
                ctx.arc(px, py, isOnline ? 5 : 4, 0, Math.PI * 2);
                ctx.fill();

                if (show) {
                    ctx.strokeStyle = isOnline
                        ? "rgba(22,163,74,0.30)"
                        : "rgba(148,163,184,0.20)";
                    ctx.lineWidth = 1;

                    ctx.beginPath();
                    ctx.arc(px, py, 14, 0, Math.PI * 2);
                    ctx.stroke();

                    ctx.fillStyle = isOnline ? COLORS.text : COLORS.muted;
                    ctx.font = "600 13px Inter";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "alphabetic";

                    const labelText = `${a.id} ${rangeVal.toFixed(2)}m`;
                    ctx.fillText(labelText, px, py - 18);
                }
            }

            // Tag1 (Only)
            const t1 = poseRef.current;

            const t1Finite = Number.isFinite(t1.x_mm) && Number.isFinite(t1.y_mm);
            if (t1Finite) {
                const p1 = toPx(t1.x_mm, t1.y_mm, cx, cy, s, bounds);

                // วาดหุ่นแบบหมุนได้ (0 องศา = หันซ้าย)
                drawRobot(
                    p1.px,
                    p1.py,
                    10,
                    COLORS.tag1,
                    (t1.yaw || 0) - yawOffsetRef.current,
                );
                if (show) drawTagLabel("Robot UWB", p1.px, p1.py, true);

                // XY label (Tag1)
                if (show) {
                    ctx.save();
                    ctx.font = "11px JetBrains Mono";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "alphabetic";

                    const label = `X:${(t1.x_mm / 1000).toFixed(2)} Y:${(t1.y_mm / 1000).toFixed(2)}`;
                    const tw = ctx.measureText(label).width + 16;

                    ctx.fillStyle = COLORS.labelBg;
                    ctx.strokeStyle = COLORS.labelBd;
                    ctx.beginPath();
                    ctx.roundRect(p1.px - tw / 2, p1.py + 24, tw, 24, 6);
                    ctx.fill();
                    ctx.stroke();

                    ctx.fillStyle = COLORS.text;
                    ctx.fillText(label, p1.px, p1.py + 42);
                    ctx.restore();
                }
            }

            // ✅ Route Drawing Path
            const currentDrawPath = drawPathRef.current;
            if (currentDrawPath.length > 0) {
                ctx.save();
                ctx.strokeStyle = "rgba(239, 68, 68, 0.8)"; // Red path
                ctx.lineWidth = 4;
                ctx.lineCap = "round";
                ctx.lineJoin = "round";
                ctx.beginPath();
                for (let i = 0; i < currentDrawPath.length; i++) {
                    const pt = toPx(currentDrawPath[i].x_m * 1000, currentDrawPath[i].y_m * 1000, cx, cy, s, bounds);
                    if (i === 0) ctx.moveTo(pt.px, pt.py);
                    else ctx.lineTo(pt.px, pt.py);
                }
                ctx.stroke();

                // Draw dots at each captured point
                ctx.fillStyle = "rgba(239, 68, 68, 1)";
                for (let i = 0; i < currentDrawPath.length; i++) {
                    const pt = toPx(currentDrawPath[i].x_m * 1000, currentDrawPath[i].y_m * 1000, cx, cy, s, bounds);
                    ctx.beginPath();
                    ctx.arc(pt.px, pt.py, 3, 0, Math.PI * 2);
                    ctx.fill();
                }
                ctx.restore();
            }

            // ✅ Navigation Target Marker
            const nt = navTargetRef.current;
            if (nt) {
                const ntPos = toPx(nt.x_m * 1000, nt.y_m * 1000, cx, cy, s, bounds);
                const time = Date.now() / 1000;
                const pulse = 0.5 + 0.5 * Math.sin(time * 3); // 0..1 pulse

                ctx.save();
                // Outer pulsing ring
                ctx.strokeStyle = `rgba(239, 68, 68, ${0.3 + pulse * 0.4})`;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.arc(ntPos.px, ntPos.py, 10 + pulse * 6, 0, Math.PI * 2);
                ctx.stroke();

                // Inner solid dot
                ctx.fillStyle = "rgba(239, 68, 68, 0.9)";
                ctx.beginPath();
                ctx.arc(ntPos.px, ntPos.py, 5, 0, Math.PI * 2);
                ctx.fill();

                // Crosshair lines
                ctx.strokeStyle = "rgba(239, 68, 68, 0.5)";
                ctx.lineWidth = 1;
                ctx.setLineDash([3, 3]);
                ctx.beginPath();
                ctx.moveTo(ntPos.px - 18, ntPos.py);
                ctx.lineTo(ntPos.px + 18, ntPos.py);
                ctx.moveTo(ntPos.px, ntPos.py - 18);
                ctx.lineTo(ntPos.px, ntPos.py + 18);
                ctx.stroke();
                ctx.setLineDash([]);
                ctx.restore();
            }

            // ✅ Mouse Hover Tooltip
            if (mouseRef.current.active) {
                const mx = mouseRef.current.x;
                const my = mouseRef.current.y;

                // Inverse Calcs
                const midX = (bounds.minX + bounds.maxX) / 2;
                const midY = (bounds.minY + bounds.maxY) / 2;

                const mmX = (mx - cx) / s + midX;
                const mmY = midY - (my - cy) / s;

                const txt = `X:${(mmX / 1000).toFixed(2)} Y:${(mmY / 1000).toFixed(2)}`;

                ctx.save();
                ctx.font = "600 12px Inter";
                ctx.textAlign = "left";
                ctx.textBaseline = "bottom";

                // Draw crosshair
                ctx.strokeStyle = "rgba(255,255,255,0.3)";
                ctx.lineWidth = 1;
                ctx.setLineDash([4, 4]);
                ctx.beginPath();
                ctx.moveTo(mx, 0);
                ctx.lineTo(mx, H);
                ctx.moveTo(0, my);
                ctx.lineTo(W, my);
                ctx.stroke();
                ctx.setLineDash([]);

                // Tooltip
                const pad = 6;
                const tw = ctx.measureText(txt).width + pad * 2;
                const th = 20;
                const tx = mx + 10;
                const ty = my - 10;

                ctx.fillStyle = "rgba(0,0,0,0.8)";
                ctx.strokeStyle = "rgba(255,255,255,0.2)";
                ctx.beginPath();
                ctx.roundRect(tx, ty - th, tw, th, 4);
                ctx.fill();
                ctx.stroke();

                ctx.fillStyle = "#fff";
                ctx.fillText(txt, tx + pad, ty - pad + 2);

                ctx.restore();
            }

            raf = requestAnimationFrame(draw);
        };

        raf = requestAnimationFrame(draw);
        return () => cancelAnimationFrame(raf);
    }, []);

    // Helper for Popup Positioning
    const getTargetPx = () => {
        if (!target) return null;
        const cvs = canvasRef.current;
        if (!cvs) return { px: 0, py: 0 };
        const bounds = getBoundsFromAnchors(anchors);
        return toPx(target.x_m * 1000, target.y_m * 1000, cvs.width / 2, cvs.height / 2, scale, bounds);
    };
    const targetPos = getTargetPx();

    return (
        <div
            className="panel"
            style={{
                padding: 0,
                position: "relative",
                overflow: "hidden",
                display: "flex",
                flexDirection: "column",
            }}
        >
            {/* Map Toolbar */}
            <div
                style={{
                    position: "absolute",
                    top: 12,
                    right: 12,
                    display: "flex",
                    gap: 8,
                    zIndex: 10,
                }}
            >
                <div style={{
                    display: "flex",
                    background: "rgba(255, 255, 255, 0.05)",
                    border: "1px solid var(--border)",
                    borderRadius: 10,
                    overflow: "hidden"
                }}>
                    <button
                        onClick={() => {
                            setActiveTool("point");
                            setDrawPath([]);
                        }}
                        style={{
                            height: 36,
                            padding: "0 12px",
                            fontSize: 12,
                            fontWeight: 600,
                            border: "none",
                            background: activeTool === "point" ? "var(--accent)" : "transparent",
                            color: activeTool === "point" ? "#fff" : "var(--muted)",
                            cursor: "pointer"
                        }}
                    >
                        📍 Point
                    </button>
                    <button
                        onClick={() => {
                            setActiveTool("draw");
                            setTarget(null);
                        }}
                        style={{
                            height: 36,
                            padding: "0 12px",
                            fontSize: 12,
                            fontWeight: 600,
                            border: "none",
                            borderLeft: "1px solid var(--border)",
                            background: activeTool === "draw" ? "rgba(239, 68, 68, 0.5)" : "transparent",
                            color: activeTool === "draw" ? "#fff" : "var(--muted)",
                            cursor: "pointer"
                        }}
                    >
                        ✏️ Draw
                    </button>
                </div>

                <button
                    onClick={() => setShowTags(!showTags)}
                    className="btn btnGhost"
                    style={{
                        height: 36,
                        padding: "0 12px",
                        borderRadius: 10,
                        fontSize: 12,
                    }}
                >
                    {showTags ? "Hide tags" : "Show tags"}
                </button>
            </div>

            {/* Canvas */}
            <div
                style={{
                    flex: 1,
                    background: "var(--surface2)",
                    position: "relative",
                }}
            >
                <canvas
                    ref={canvasRef}
                    width={1200}
                    height={700}
                    style={{
                        width: "100%",
                        height: "100%",
                        objectFit: "contain",
                        display: "block",
                        touchAction: "none",
                        cursor: "crosshair",
                    }}
                    onPointerDown={(e) => {
                        const cvs = canvasRef.current;
                        const rect = cvs.getBoundingClientRect();
                        // objectFit:contain → calc actual rendered area
                        const cvsAR = cvs.width / cvs.height;
                        const rectAR = rect.width / rect.height;
                        let rW, rH, oX, oY;
                        if (rectAR > cvsAR) {
                            rH = rect.height; rW = rH * cvsAR;
                            oX = (rect.width - rW) / 2; oY = 0;
                        } else {
                            rW = rect.width; rH = rW / cvsAR;
                            oX = 0; oY = (rect.height - rH) / 2;
                        }
                        const x = ((e.clientX - rect.left - oX) / rW) * cvs.width;
                        const y = ((e.clientY - rect.top - oY) / rH) * cvs.height;

                        // Inverse Calculation
                        const cx = cvs.width / 2;
                        const cy = cvs.height / 2;
                        const s = scaleRef.current;

                        const bounds = getBoundsFromAnchors(anchorsRef.current);
                        const midX = (bounds.minX + bounds.maxX) / 2;
                        const midY = (bounds.minY + bounds.maxY) / 2;

                        const mmX = (x - cx) / s + midX;
                        const mmY = midY - (y - cy) / s;
                        const x_m = mmX / 1000;
                        const y_m = mmY / 1000;

                        if (activeTool === "point") {
                            // Set Target for Confirmation
                            setTarget({ x_m, y_m });
                        } else if (activeTool === "draw") {
                            // Start drawing a route
                            isDrawingRef.current = true;
                            setDrawPath([{ x_m, y_m }]);
                        }
                    }}
                    onPointerMove={(e) => {
                        const cvs = canvasRef.current;
                        const rect = cvs.getBoundingClientRect();
                        // objectFit:contain → calc actual rendered area
                        const cvsAR = cvs.width / cvs.height;
                        const rectAR = rect.width / rect.height;
                        let rW, rH, oX, oY;
                        if (rectAR > cvsAR) {
                            rH = rect.height; rW = rH * cvsAR;
                            oX = (rect.width - rW) / 2; oY = 0;
                        } else {
                            rW = rect.width; rH = rW / cvsAR;
                            oX = 0; oY = (rect.height - rH) / 2;
                        }

                        const pointerX = ((e.clientX - rect.left - oX) / rW) * cvs.width;
                        const pointerY = ((e.clientY - rect.top - oY) / rH) * cvs.height;

                        mouseRef.current = { x: pointerX, y: pointerY, active: true };

                        if (activeTool === "draw" && isDrawingRef.current) {
                            // Inverse Calculation
                            const cx = cvs.width / 2;
                            const cy = cvs.height / 2;
                            const s = scaleRef.current;
                            const bounds = getBoundsFromAnchors(anchorsRef.current);
                            const midX = (bounds.minX + bounds.maxX) / 2;
                            const midY = (bounds.minY + bounds.maxY) / 2;

                            const mmX = (pointerX - cx) / s + midX;
                            const mmY = midY - (pointerY - cy) / s;
                            const cur_x = mmX / 1000;
                            const cur_y = mmY / 1000;

                            setDrawPath(prev => {
                                if (prev.length === 0) {
                                    return [{ x_m: cur_x, y_m: cur_y }];
                                }
                                const last = prev[prev.length - 1];
                                // Add point if moved at least 0.2 meters
                                const dist = Math.hypot(cur_x - last.x_m, cur_y - last.y_m);
                                if (dist > 0.2) {
                                    return [...prev, { x_m: cur_x, y_m: cur_y }];
                                }
                                return prev;
                            });
                        }
                    }}
                    onPointerUp={() => {
                        if (activeTool === "draw" && isDrawingRef.current) {
                            isDrawingRef.current = false;

                            // Send full route to execute
                            const currentDrawPath = drawPathRef.current;
                            if (currentDrawPath.length > 1 && onRouteComplete) {
                                onRouteComplete(currentDrawPath);
                                // ✅ Tool remains "draw" as requested
                            } else {
                                setDrawPath([]); // Too short, discard
                            }
                        }
                    }}
                    onPointerLeave={() => {
                        mouseRef.current.active = false;
                        if (activeTool === "draw" && isDrawingRef.current) {
                            isDrawingRef.current = false;

                            const currentDrawPath = drawPathRef.current;
                            if (currentDrawPath.length > 1 && onRouteComplete) {
                                onRouteComplete(currentDrawPath);
                            } else {
                                setDrawPath([]); // Too short
                            }
                        }
                    }}
                />

                {/* ✅ Confirmation Popup */}
                {target && targetPos && (
                    <div
                        style={{
                            position: "absolute",
                            left: targetPos.px,
                            top: targetPos.py,
                            transform: "translate(-50%, -100%) translateY(-10px)",
                            background: "var(--surface)",
                            border: "1px solid var(--border)",
                            boxShadow: "0 4px 20px rgba(0,0,0,0.5)",
                            borderRadius: 12,
                            padding: 12,
                            zIndex: 20,
                            minWidth: 160,
                            animation: "popIn 0.2s cubic-bezier(0.18, 0.89, 0.32, 1.28)",
                        }}
                    >
                        <style>{`@keyframes popIn { from { transform: translate(-50%, -100%) translateY(-5px) scale(0.9); opacity:0; } to { transform: translate(-50%, -100%) translateY(-10px) scale(1); opacity:1; } }`}</style>
                        <div style={{ fontSize: 11, fontWeight: 700, color: "var(--muted)", textTransform: "uppercase", marginBottom: 8, textAlign: "center" }}>
                            Confirm Navigation
                        </div>
                        <div style={{ fontSize: 13, color: "var(--text)", textAlign: "center", marginBottom: 12, fontFamily: "'JetBrains Mono', monospace" }}>
                            X: {target.x_m.toFixed(2)} Y: {target.y_m.toFixed(2)}
                        </div>
                        <div style={{ display: "flex", gap: 8 }}>
                            <button
                                className="btn btnGhost"
                                style={{ flex: 1, height: 32, borderRadius: 8, fontSize: 12 }}
                                onClick={() => setTarget(null)}
                            >
                                CANCEL
                            </button>
                            <button
                                className="btn btnPrimary"
                                style={{ flex: 1, height: 32, borderRadius: 8, fontSize: 12 }}
                                onClick={() => {
                                    if (onMapClick) onMapClick(target.x_m, target.y_m);
                                    setTarget(null);
                                }}
                            >
                                GO
                            </button>
                        </div>
                        {/* Triangle Arrow */}
                        <div style={{
                            position: "absolute",
                            bottom: -6,
                            left: "50%",
                            marginLeft: -6,
                            width: 12, height: 12,
                            background: "var(--surface)",
                            borderRight: "1px solid var(--border)",
                            borderBottom: "1px solid var(--border)",
                            transform: "rotate(45deg)",
                        }} />
                    </div>
                )}
            </div>

            {/* Zoom Footer */}
            <div
                style={{
                    padding: "12px 14px",
                    borderTop: "1px solid var(--border)",
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center",
                    background: "var(--surface)",
                }}
            >
                <span
                    style={{ fontSize: 13, color: "var(--muted)", fontWeight: 700 }}
                >
                    MAP VIEW: {(scale * 100).toFixed(0)}%
                </span>

                <div
                    style={{
                        display: "flex",
                        alignItems: "center",
                        gap: 10,
                        width: 240,
                    }}
                >
                    <button
                        className="btn btnGhost"
                        style={{ height: 30, width: 34, borderRadius: 10 }}
                        onClick={() =>
                            setScale((s) => Math.max(ZOOM_MIN, s - ZOOM_STEP))
                        }
                    >
                        −
                    </button>

                    <input
                        type="range"
                        min={ZOOM_MIN}
                        max={ZOOM_MAX}
                        step={ZOOM_STEP}
                        value={scale}
                        onChange={(e) => setScale(parseFloat(e.target.value))}
                    />

                    <button
                        className="btn btnGhost"
                        style={{ height: 30, width: 34, borderRadius: 10 }}
                        onClick={() =>
                            setScale((s) => Math.min(ZOOM_MAX, s + ZOOM_STEP))
                        }
                    >
                        +
                    </button>
                </div>
            </div>
        </div>
    );
};

export default MapCanvas;
