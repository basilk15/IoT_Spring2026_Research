from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import cm
from reportlab.graphics.shapes import Drawing, Line, Polygon, Rect, String
from reportlab.platypus import Preformatted, SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle


OUTPUT_PATH = "FL_freertos_cpu_measurement_report.pdf"
CODE_PATH = Path("FL_freertos_task/FL_freertos_task.ino")

SAMPLE_OUTPUT = """12:30:26.565 -> [FL] Federated round complete
12:30:26.565 -> [CPU] Task usage during one FL round
12:30:26.598 -> [CPU] Task Name         Delta(us)   Share
12:30:26.598 -> [CPU] loopTask         2248       0.12%
12:30:26.598 -> [CPU] IDLE0            1840106    98.04%
12:30:26.598 -> [CPU] IDLE1            1817634    96.84%
12:30:26.598 -> [CPU] tiT              6047       0.32%
12:30:26.598 -> [CPU] esp_timer        1949       0.10%
12:30:26.598 -> [CPU] wifi             28897      1.54%
12:30:26.598 -> [CPU] FLTask           57026      3.04%
12:30:26.598 -> [CPU] TOTAL_RUNTIME_DELTA(us): 1876901

12:30:58.932 -> [FL] Upload OK
12:30:58.932 -> [FL] Federated round complete
12:30:58.932 -> [CPU] Task usage during one FL round
12:30:58.932 -> [CPU] Task Name         Delta(us)   Share
12:30:58.932 -> [CPU] loopTask         2247       0.10%
12:30:58.932 -> [CPU] IDLE0            2279216    98.29%
12:30:58.932 -> [CPU] IDLE1            2259133    97.42%
12:30:58.965 -> [CPU] tiT              5891       0.25%
12:30:58.965 -> [CPU] esp_timer        2048       0.09%
12:30:58.965 -> [CPU] wifi             31844      1.37%
12:30:58.965 -> [CPU] FLTask           57526      2.48%
12:30:58.965 -> [CPU] TOTAL_RUNTIME_DELTA(us): 2318900"""


def build_styles():
    styles = getSampleStyleSheet()
    styles.add(
        ParagraphStyle(
            name="ReportTitle",
            parent=styles["Title"],
            fontName="Helvetica-Bold",
            fontSize=18,
            leading=22,
            alignment=TA_CENTER,
            textColor=colors.HexColor("#143642"),
            spaceAfter=12,
        )
    )
    styles.add(
        ParagraphStyle(
            name="Section",
            parent=styles["Heading2"],
            fontName="Helvetica-Bold",
            fontSize=12,
            leading=14,
            textColor=colors.HexColor("#0F4C5C"),
            spaceBefore=10,
            spaceAfter=6,
        )
    )
    styles.add(
        ParagraphStyle(
            name="Body",
            parent=styles["BodyText"],
            fontName="Helvetica",
            fontSize=10,
            leading=14,
            alignment=TA_LEFT,
            spaceAfter=6,
        )
    )
    styles.add(
        ParagraphStyle(
            name="Mono",
            parent=styles["Code"],
            fontName="Courier",
            fontSize=8.4,
            leading=10,
            backColor=colors.HexColor("#F4F7F8"),
            borderPadding=6,
        )
    )
    return styles


def bullet(text, styles):
    return Paragraph(f"• {text}", styles["Body"])


def extract_code_excerpt(start_line, end_line):
    lines = CODE_PATH.read_text().splitlines()
    selected = []
    for line_no in range(start_line, end_line + 1):
        selected.append(f"{line_no:>4}  {lines[line_no - 1]}")
    return "\n".join(selected)


def add_arrow(drawing, x1, y1, x2, y2, color):
    drawing.add(Line(x1, y1, x2, y2, strokeColor=color, strokeWidth=2))
    if x2 >= x1:
        drawing.add(Polygon(
            [x2, y2, x2 - 8, y2 + 4, x2 - 8, y2 - 4],
            fillColor=color,
            strokeColor=color,
        ))
    else:
        drawing.add(Polygon(
            [x2, y2, x2 + 8, y2 + 4, x2 + 8, y2 - 4],
            fillColor=color,
            strokeColor=color,
        ))


def add_box(drawing, x, y, w, h, fill, stroke, title, lines):
    drawing.add(Rect(x, y, w, h, rx=8, ry=8, fillColor=fill, strokeColor=stroke, strokeWidth=1.5))
    drawing.add(String(x + 10, y + h - 18, title, fontName="Helvetica-Bold", fontSize=12, fillColor=colors.HexColor("#374151")))
    current_y = y + h - 34
    for line in lines:
        drawing.add(String(x + 10, current_y, line, fontName="Helvetica", fontSize=9, fillColor=colors.HexColor("#374151")))
        current_y -= 12


def build_flow_diagram():
    drawing = Drawing(520, 300)

    lane_fill = colors.HexColor("#F8FBFD")
    lane_stroke = colors.HexColor("#AABCC3")
    drawing.add(Rect(10, 18, 240, 264, rx=10, ry=10, fillColor=lane_fill, strokeColor=lane_stroke))
    drawing.add(Rect(270, 18, 240, 264, rx=10, ry=10, fillColor=lane_fill, strokeColor=lane_stroke))
    drawing.add(String(20, 270, "Controller lane: setup() + loop()", fontName="Helvetica-Bold", fontSize=12, fillColor=colors.HexColor("#1e40af")))
    drawing.add(String(280, 270, "Worker lane: FLTask", fontName="Helvetica-Bold", fontSize=12, fillColor=colors.HexColor("#1e40af")))

    add_box(
        drawing, 28, 198, 200, 56,
        colors.HexColor("#fed7aa"), colors.HexColor("#c2410c"),
        "setup()",
        ["init Serial, I2C, Wi-Fi", "create FLTask"],
    )
    add_box(
        drawing, 28, 124, 200, 60,
        colors.HexColor("#dbeafe"), colors.HexColor("#1e3a5f"),
        "loop() controller",
        ["captureStats(before)", "start FLTask and wait"],
    )
    add_box(
        drawing, 28, 34, 200, 56,
        colors.HexColor("#dbeafe"), colors.HexColor("#1e3a5f"),
        "post-round report",
        ["captureStats(after)", "print report, then delay"],
    )

    add_box(
        drawing, 292, 104, 198, 100,
        colors.HexColor("#93c5fd"), colors.HexColor("#1e3a5f"),
        "FLTask execution",
        ["download model", "collect samples", "local training", "upload weights"],
    )
    add_box(
        drawing, 292, 34, 198, 56,
        colors.HexColor("#a7f3d0"), colors.HexColor("#047857"),
        "task completion",
        ["print completion", "notify controller task"],
    )

    add_arrow(drawing, 128, 198, 128, 184, colors.HexColor("#1e3a5f"))
    add_arrow(drawing, 128, 124, 128, 90, colors.HexColor("#1e3a5f"))
    add_arrow(drawing, 228, 154, 292, 154, colors.HexColor("#1e3a5f"))
    add_arrow(drawing, 391, 104, 391, 90, colors.HexColor("#1e3a5f"))
    add_arrow(drawing, 292, 62, 228, 62, colors.HexColor("#047857"))
    return drawing


def main():
    styles = build_styles()
    doc = SimpleDocTemplate(
        OUTPUT_PATH,
        pagesize=A4,
        leftMargin=1.6 * cm,
        rightMargin=1.6 * cm,
        topMargin=1.5 * cm,
        bottomMargin=1.5 * cm,
    )

    story = []
    story.append(Paragraph("CPU Usage Measurement of Federated Learning on ESP32 Using FreeRTOS Run-Time Statistics", styles["ReportTitle"]))
    story.append(Paragraph("Project context: Arduino IDE based ESP32 federated-learning client with a dedicated FreeRTOS task named FLTask.", styles["Body"]))

    story.append(Paragraph("1. Objective", styles["Section"]))
    story.append(Paragraph(
        "The aim of this measurement is to estimate how much CPU time the ESP32 spends executing the federated-learning workflow itself, while also showing the CPU time consumed by background system tasks such as Wi-Fi, timers, and idle tasks.",
        styles["Body"],
    ))
    story.append(Paragraph(
        "This method was chosen because a simple cycle-count difference around a code block includes time consumed by other concurrent activity on the ESP32. FreeRTOS run-time statistics provide task-level accounting, which gives a more meaningful view of CPU usage.",
        styles["Body"],
    ))

    story.append(Paragraph("2. Original Approach and Limitation", styles["Section"]))
    story.append(bullet("Earlier, the FL client used ESP.getCycleCount() around download, sensing, local training, upload, and the full round.", styles))
    story.append(bullet("That approach measures elapsed core cycles during the observation window, but those cycles can also include RTOS scheduling, Wi-Fi stack activity, timer work, and other concurrent system tasks.", styles))
    story.append(bullet("Because of this, the measured value is not a clean attribution of CPU usage caused only by the FL process.", styles))

    story.append(Paragraph("3. Updated Methodology", styles["Section"]))
    story.append(bullet("The full federated-learning flow was moved from Arduino loopTask into a dedicated FreeRTOS task named FLTask.", styles))
    story.append(bullet("The controller in loop() captures a task snapshot before starting FLTask and another snapshot after the FL round completes.", styles))
    story.append(bullet("The snapshots are collected using uxTaskGetSystemState(), which reports accumulated run-time counters for all FreeRTOS tasks.", styles))
    story.append(bullet("By subtracting the before and after snapshots, the method estimates how much CPU time each task consumed during one FL round.", styles))

    story.append(Paragraph("4. What the Modified FL Code Does", styles["Section"]))
    story.append(bullet("FLTask performs one complete round: download model, collect sensor samples, run local training, and upload updated weights.", styles))
    story.append(bullet("loop() no longer performs the FL work itself. It only coordinates measurement and waits for FLTask to finish.", styles))
    story.append(bullet("The server behavior remains the same, except a separate measurement server copy was created with MIN_CLIENTS set to 1 for one-device testing.", styles))

    story.append(Paragraph("5. Code Explanation and High-Level View", styles["Section"]))
    story.append(Paragraph(
        "At a high level, the modified sketch separates control and workload. The Arduino loop() acts only as a measurement controller, while FLTask performs the actual federated-learning computation and communication.",
        styles["Body"],
    ))
    story.append(build_flow_diagram())
    story.append(Spacer(1, 8))
    story.append(bullet("setup() initializes Serial, I2C, MPU6050 access, Wi-Fi, and creates the dedicated FLTask.", styles))
    story.append(bullet("loop() captures a before snapshot, wakes FLTask, waits for completion, captures an after snapshot, and prints the CPU usage report.", styles))
    story.append(bullet("FLTask executes the complete FL round in sequence: download model, collect samples, train locally, then upload weights.", styles))
    story.append(bullet("captureStats() uses uxTaskGetSystemState() to read the accumulated run-time counters of all active FreeRTOS tasks.", styles))
    story.append(bullet("printStatsDelta() subtracts the two snapshots and prints the CPU time and percentage share of each task during the measured window.", styles))
    story.append(Paragraph(
        "The important design idea is that FLTask contains the whole FL pipeline, so the ESP32 can account for the CPU time of that pipeline under one named task instead of mixing it into the default loopTask.",
        styles["Body"],
    ))
    story.append(Paragraph(
        "The following excerpts come directly from FL_freertos_task/FL_freertos_task.ino and show the main measurement logic.",
        styles["Body"],
    ))
    story.append(Paragraph("5.1 Task creation in setup()", styles["Section"]))
    story.append(Paragraph(
        "This part creates the dedicated FLTask. The parent handle passed as the task parameter is the controller task, so FLTask can notify it again when one round finishes.",
        styles["Body"],
    ))
    story.append(Preformatted(extract_code_excerpt(433, 441), styles["Mono"]))
    story.append(Paragraph("5.2 Controller logic in loop()", styles["Section"]))
    story.append(Paragraph(
        "This is the core measurement flow: take a before snapshot, start FLTask, block until FLTask signals completion, take the after snapshot, and finally print the delta report.",
        styles["Body"],
    ))
    story.append(Preformatted(extract_code_excerpt(451, 461), styles["Mono"]))
    story.append(Paragraph("5.3 FLTask workload logic", styles["Section"]))
    story.append(Paragraph(
        "Inside FLTask, the actual federated-learning round is executed in sequence. This is why the run-time consumed by these steps is attributed to FLTask in the printed report.",
        styles["Body"],
    ))
    story.append(Preformatted(extract_code_excerpt(372, 414), styles["Mono"]))
    story.append(Paragraph("5.4 Snapshot and report functions", styles["Section"]))
    story.append(Paragraph(
        "These helper functions collect the accumulated task counters and convert them into the per-round CPU usage table that appears on the serial monitor.",
        styles["Body"],
    ))
    story.append(Preformatted(extract_code_excerpt(295, 365), styles["Mono"]))

    story.append(Paragraph("6. Measurement Principle", styles["Section"]))
    story.append(Paragraph(
        "Let T_before(task) be the accumulated run-time counter of a task before the FL round, and T_after(task) be the counter after the round. Then:",
        styles["Body"],
    ))
    story.append(Preformatted(
        "Delta(task) = T_after(task) - T_before(task)\n"
        "Share(task) = Delta(task) / TotalRuntimeDelta * 100",
        styles["Mono"],
    ))
    story.append(Paragraph(
        "Here, Delta(task) is the CPU time attributed to that task during the measured FL window. TotalRuntimeDelta is the increase in the global run-time counter over the same window.",
        styles["Body"],
    ))

    story.append(Paragraph("7. Sample Result from the ESP32", styles["Section"]))
    story.append(Preformatted(SAMPLE_OUTPUT, styles["Mono"]))

    story.append(Paragraph("8. How to Interpret the Sample Result", styles["Section"]))
    cell_style = ParagraphStyle(
        name="TableCell",
        parent=styles["Body"],
        fontName="Helvetica",
        fontSize=9,
        leading=11,
        spaceAfter=0,
    )
    head_style = ParagraphStyle(
        name="TableHead",
        parent=cell_style,
        fontName="Helvetica-Bold",
    )
    data = [
        [Paragraph("Printed field", head_style), Paragraph("Meaning", head_style)],
        [Paragraph("FLTask", cell_style), Paragraph("CPU time attributed to the full federated-learning application task during one round.", cell_style)],
        [Paragraph("wifi", cell_style), Paragraph("CPU time consumed by Wi-Fi related system activity during the same round.", cell_style)],
        [Paragraph("esp_timer / tiT", cell_style), Paragraph("Background timer and internal RTOS/system activity.", cell_style)],
        [Paragraph("IDLE0 / IDLE1", cell_style), Paragraph("Time when the CPU cores were idle and not heavily loaded by application work.", cell_style)],
        [Paragraph("TOTAL_RUNTIME_DELTA(us)", cell_style), Paragraph("Total measured task run-time change in the observation window.", cell_style)],
    ]
    table = Table(data, colWidths=[5.2 * cm, 10.1 * cm])
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#D9EAF0")),
        ("TEXTCOLOR", (0, 0), (-1, 0), colors.black),
        ("GRID", (0, 0), (-1, -1), 0.5, colors.HexColor("#AABCC3")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#F7FAFB")]),
        ("LEFTPADDING", (0, 0), (-1, -1), 6),
        ("RIGHTPADDING", (0, 0), (-1, -1), 6),
        ("TOPPADDING", (0, 0), (-1, -1), 5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
    ]))
    story.append(table)
    story.append(Spacer(1, 8))
    story.append(bullet("In the provided output, FLTask uses only a small percentage of the total task run time, while IDLE0 and IDLE1 dominate the report. This means the ESP32 is spending most of the measured window idle rather than heavily computing.", styles))
    story.append(bullet("The likely reason is that one FL round contains waiting periods such as sensor sampling delays and network operations, while the pure training section is comparatively short.", styles))
    story.append(bullet("Therefore, this result should be interpreted as CPU usage of the complete FL round, not CPU usage of local training alone.", styles))
    story.append(Paragraph(
        "The sample output can be interpreted more concretely as follows. Upload OK means the ESP32 successfully posted its updated weights to the server. Federated round complete means one full round finished: download model, collect samples, train locally, and upload weights.",
        styles["Body"],
    ))
    story.append(bullet("Delta(us) is the additional run time accumulated by each task during that round's measurement window.", styles))
    story.append(bullet("Share is computed as Delta(task) / TOTAL_RUNTIME_DELTA * 100, matching the reporting logic in FL_freertos_task.ino.", styles))
    story.append(bullet("In the two shown rounds, TOTAL_RUNTIME_DELTA is about 1.88 s and 2.32 s.", styles))
    story.append(bullet("FLTask is about 57 ms in both rounds, which is roughly 3.04% and 2.48% of the measured window.", styles))
    story.append(bullet("wifi is about 29-32 ms, which is roughly 1.37% to 1.54%.", styles))
    story.append(bullet("IDLE0 and IDLE1 are very high, around 97-98%, which means the chip is mostly idle and not strongly CPU-bound during the measured round.", styles))
    story.append(bullet("The percentages do not sum to 100%. On ESP32 this is expected because the device has two cores, and both idle tasks are counted in the run-time statistics.", styles))
    story.append(bullet("The practical conclusion is that the FL round is dominated by waiting, sensor pacing, and communication overhead rather than raw compute.", styles))
    story.append(bullet("The pure FL application work attributed to FLTask is comparatively small and stable, while the total round window changes because of waiting and communication effects.", styles))

    story.append(Paragraph("9. What This Method Measures Well", styles["Section"]))
    story.append(bullet("It shows CPU usage attributable to the dedicated FLTask instead of mixing the application into Arduino loopTask.", styles))
    story.append(bullet("It reveals concurrent background overhead from Wi-Fi and internal ESP32 tasks during the same round.", styles))
    story.append(bullet("It is practical in Arduino IDE because ESP32 Arduino already runs on top of FreeRTOS.", styles))

    story.append(Paragraph("10. Remaining Limitations", styles["Section"]))
    story.append(bullet("This version measures the full FL workflow as one task, so it does not separately report download, sensing, training, and upload.", styles))
    story.append(bullet("Large idle percentages are expected when the task contains delay calls or waits for I/O.", styles))
    story.append(bullet("Interrupt-level work is not always attributed cleanly to FLTask, so this remains an estimate of task-level CPU usage rather than exact hardware-cycle attribution.", styles))

    story.append(Paragraph("11. Conclusion", styles["Section"]))
    story.append(Paragraph(
        "The modified FreeRTOS-based approach is more defensible than plain cycle counting for measuring CPU usage on the ESP32. By moving the federated-learning pipeline into a dedicated FLTask and comparing task snapshots before and after each round, the method provides a clearer estimate of how much CPU time the FL application consumes and how much background ESP32 activity occurs alongside it.",
        styles["Body"],
    ))

    doc.build(story)


if __name__ == "__main__":
    main()
