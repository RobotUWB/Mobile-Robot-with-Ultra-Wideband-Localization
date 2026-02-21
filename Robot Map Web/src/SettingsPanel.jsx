import React from "react";

const SettingsPanel = ({
    ranges,
    calState,
    CAL_TEXT,
    CAL_COLOR,
    refX,
    setRefX,
    refY,
    setRefY,
    calT1,
    calDirection,
    saveT1,
    resetT1,
    connected,
}) => {
    return (
        <div className="panel" style={{ padding: 16 }}>
            <div className="panelTitle">Anchor distances</div>

            <div style={{ display: "grid", gap: 10 }}>
                {Object.entries(ranges).map(([k, v]) => (
                    <div
                        key={k}
                        style={{
                            display: "flex",
                            justifyContent: "space-between",
                            alignItems: "center",
                        }}
                    >
                        <span
                            style={{
                                color: "#ffffff",
                                fontSize: 14,
                                fontWeight: 700,
                            }}
                        >
                            {k}
                        </span>
                        <span className="mono" style={{ fontSize: 13, color: "#ffffff" }}>
                            {v.toFixed(2)}m
                        </span>
                    </div>
                ))}
            </div>

            <div
                style={{
                    marginTop: 14,
                    paddingTop: 14,
                    borderTop: "1px solid var(--border)",
                }}
            >
                <div
                    style={{
                        display: "flex",
                        justifyContent: "space-between",
                        alignItems: "center",
                        marginBottom: 10,
                    }}
                >
                    <div className="panelTitle" style={{ margin: 0 }}>
                        Tag1 calibration
                    </div>
                    <div
                        style={{
                            fontSize: 12,
                            fontWeight: 800,
                            color: CAL_COLOR[calState] || "var(--muted)",
                        }}
                    >
                        {CAL_TEXT[calState] || "READY"}
                    </div>
                </div>

                <div style={{ display: "grid", gap: 10 }}>
                    <div
                        style={{
                            display: "flex",
                            alignItems: "center",
                            gap: 10,
                            flexWrap: "wrap",
                        }}
                    >
                        <span
                            style={{
                                fontSize: 12,
                                color: "var(--muted)",
                                fontWeight: 800,
                            }}
                        >
                            REF
                        </span>

                        <input
                            className="cal-in"
                            type="number"
                            step="0.01"
                            value={refX}
                            onChange={(e) => setRefX(e.target.value)}
                            inputMode="decimal"
                            onFocus={(e) => e.target.select()}
                        />

                        <input
                            className="cal-in"
                            type="number"
                            step="0.01"
                            value={refY}
                            onChange={(e) => setRefY(e.target.value)}
                            inputMode="decimal"
                            onFocus={(e) => e.target.select()}
                        />
                    </div>

                    <div
                        style={{
                            display: "flex",
                            alignItems: "center",
                            gap: 10,
                            flexWrap: "wrap",
                        }}
                    >
                        <button
                            onClick={calT1}
                            disabled={!connected}
                            className="btn btnPrimary"
                            style={{
                                height: 34,
                                padding: "0 12px",
                                borderRadius: 10,
                            }}
                        >
                            CAL
                        </button>

                        <button
                            onClick={calDirection}
                            disabled={!connected}
                            className="btn btnNeutral"
                            style={{
                                height: 34,
                                padding: "0 12px",
                                borderRadius: 10,
                            }}
                            title="Set Current Direction as 0 (Front)"
                        >
                            CAL DIR
                        </button>

                        <button
                            onClick={saveT1}
                            disabled={!connected}
                            className="btn btnSuccess"
                            style={{
                                height: 34,
                                padding: "0 12px",
                                borderRadius: 10,
                            }}
                        >
                            SAVE
                        </button>

                        <button
                            onClick={resetT1}
                            disabled={!connected}
                            className="btn btnNeutral"
                            style={{
                                height: 34,
                                padding: "0 12px",
                                borderRadius: 10,
                            }}
                        >
                            RESET
                        </button>
                    </div>
                </div>

                <div
                    style={{
                        marginTop: 10,
                        fontSize: 12,
                        color: "var(--muted)",
                        lineHeight: 1.35,
                    }}
                />
            </div>
        </div>
    );
};

export default SettingsPanel;
