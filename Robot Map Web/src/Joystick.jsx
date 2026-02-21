import React from "react";

const KeyBtn = ({
    label,
    active,
    disabled,
    onDown,
    onUp,
    wide = false,
    danger = false,
}) => {
    const baseBg = danger ? "rgba(220,38,38,0.20)" : "rgba(255,255,255,0.04)";
    const activeBg = danger ? "rgba(220,38,38,0.35)" : "rgba(59,130,246,0.22)";

    const baseBd = danger ? "rgba(220,38,38,0.35)" : "var(--border)";
    const activeBd = danger ? "rgba(220,38,38,0.60)" : "rgba(59,130,246,0.55)";

    return (
        <button
            className="btn"
            disabled={disabled}
            onPointerDown={(e) => {
                e.preventDefault();
                e.currentTarget.setPointerCapture?.(e.pointerId);
                onDown?.();
            }}
            onPointerUp={(e) => {
                e.preventDefault();
                onUp?.();
            }}
            onPointerLeave={() => onUp?.()}
            onPointerCancel={() => onUp?.()}
            style={{
                height: 44,
                borderRadius: 12,
                fontWeight: 800,
                letterSpacing: 0.5,
                background: active ? activeBg : baseBg,
                borderColor: active ? activeBd : baseBd,
                width: wide ? "100%" : "auto",
                gridColumn: wide ? "1 / -1" : undefined,
            }}
            title={label}
        >
            {label}
        </button>
    );
};

const Joystick = ({
    rotKey,
    driveKey,
    stopHeld,
    connected,
    rotatePress,
    rotateRelease,
    manualPress,
    manualRelease,
    stopHoldPress,
    stopHoldRelease,
}) => {
    return (
        <div className="panel" style={{ padding: 16 }}>
            <div className="panelTitle">Manual drive (test)</div>

            <div
                style={{
                    display: "grid",
                    gridTemplateColumns: "repeat(3, 60px)",
                    gap: 8,
                    justifyContent: "center",
                }}
            >
                {/* Q */}
                <button
                    className={`btn ${rotKey === "Q" ? "btnPrimary" : "btnNeutral"}`}
                    onMouseDown={() => rotatePress("Q")}
                    onMouseUp={rotateRelease}
                    onMouseLeave={rotateRelease}
                >
                    Q
                </button>

                {/* W */}
                <button
                    className={`btn ${driveKey === "W" ? "btnPrimary" : "btnNeutral"}`}
                    onMouseDown={() => manualPress("W")}
                    onMouseUp={() => manualRelease("W")}
                    onMouseLeave={() => manualRelease("W")}
                >
                    W
                </button>

                {/* E */}
                <button
                    className={`btn ${rotKey === "E" ? "btnPrimary" : "btnNeutral"}`}
                    onMouseDown={() => rotatePress("E")}
                    onMouseUp={rotateRelease}
                    onMouseLeave={rotateRelease}
                >
                    E
                </button>

                {/* A */}
                <button
                    className={`btn ${driveKey === "A" ? "btnPrimary" : "btnNeutral"}`}
                    onMouseDown={() => manualPress("A")}
                    onMouseUp={() => manualRelease("A")}
                    onMouseLeave={() => manualRelease("A")}
                >
                    A
                </button>

                {/* S */}
                <button
                    className={`btn ${driveKey === "S" ? "btnPrimary" : "btnNeutral"}`}
                    onMouseDown={() => manualPress("S")}
                    onMouseUp={() => manualRelease("S")}
                    onMouseLeave={() => manualRelease("S")}
                >
                    S
                </button>

                {/* D */}
                <button
                    className={`btn ${driveKey === "D" ? "btnPrimary" : "btnNeutral"}`}
                    onMouseDown={() => manualPress("D")}
                    onMouseUp={() => manualRelease("D")}
                    onMouseLeave={() => manualRelease("D")}
                >
                    D
                </button>
            </div>

            <div
                style={{
                    marginTop: 12,
                    display: "flex",
                    gap: 10,
                    alignItems: "center",
                }}
            >
                <KeyBtn
                    label="STOP"
                    wide
                    danger
                    active={stopHeld} // ✅ ให้มีสถานะกดค้างเหมือนปุ่มอื่น
                    disabled={!connected}
                    onDown={stopHoldPress} // ✅ กดค้าง
                    onUp={stopHoldRelease} // ✅ ปล่อยแล้ว resume ถ้ามีปุ่มค้างอยู่
                />
            </div>

            <div
                style={{
                    marginTop: 10,
                    fontSize: 12,
                    color: "var(--muted)",
                    lineHeight: 1.35,
                }}
            ></div>
        </div>
    );
};

export default Joystick;
