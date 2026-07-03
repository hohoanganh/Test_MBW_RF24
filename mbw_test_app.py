# -*- coding: utf-8 -*-
"""
mbw_test_app.py - App test MBW RF24 RS485 2.0 (MBW-RF-00-24)
==============================================================
GUI Python (tkinter) test TỪNG BOARD RIÊNG LẺ qua 1 cổng COM. Vì 2 board khi
truyền/nhận thật sẽ được cắm vào 2 MÁY TÍNH KHÁC NHAU (mỗi máy chạy 1 instance
app riêng, mỗi app chỉ thấy board của máy mình), app KHÔNG điều khiển đồng
thời 2 board như thiết kế trước — mỗi cửa sổ app chỉ kết nối 1 board.

Chức năng mặc định của board là TỰ ĐỘNG forward RS485 <-> Wireless ngay khi
cấp nguồn (bridge luôn bật, không cần lệnh kích hoạt). App chỉ CẮM VÀO ĐỂ QUAN
SÁT: mỗi khi board relay 1 khung dữ liệu (2 chiều), firmware in ra console 1
dòng "FWD RS485->RF: ..." / "FWD RF->RS485: ...", app đọc và hiển thị trực
quan lên màn hình (tab "Giám sát Forward") — đây là bằng chứng trực tiếp thiết
bị đang hoạt động đúng chức năng, xem được ngay trên từng máy mà không cần
đồng bộ giữa 2 máy tính.

Chức năng:
  - Kết nối 1 cổng COM, xác thực ID ("MBW_RF24_RS485").
  - Giám sát Forward: bảng log các khung đã relay (giờ, hướng, số byte,
    preview hex), bật/tắt bridge, bật/tắt log forward, xem thống kê, gửi thử
    RS485 để tự kiểm tra forward khi chưa có Modbus Master/Slave thật.
  - Nút "🔌 Modbus Poll Test (RS485 thật)" (tab Giám sát Forward) mở
    modbus_poll_app.py trong 1 tiến trình/cửa sổ riêng - đây là công cụ
    Modbus RTU Master THẬT (đọc/ghi thanh ghi FC3/FC6, log Excel, biểu đồ)
    kết nối qua cổng RS485 VẬT LÝ của board (khác cổng console CLI) - phép
    test thực tế nhất cho chức năng bridge: 1 máy chạy Modbus Master qua
    RS485 của board A, đầu kia là board B nối Modbus Slave thật.
  - Test tự động: id/dip/flash/rtc/rsl (loopback)/rf id trên board đang nối.
  - Terminal: gõ lệnh CLI trực tiếp.
  - Xuất báo cáo nối tiếp vào mbw_test_report.xlsx (hoặc .csv nếu thiếu openpyxl).

Cài đặt:  pip install pyserial openpyxl matplotlib
Chạy:     python mbw_test_app.py
Đóng gói: build_mbw_exe.bat -> dist/MBW_RF24_Test.exe
"""

import os
import re
import sys
import json
import time
import threading
import subprocess
import collections
from datetime import datetime

import tkinter as tk
from tkinter import ttk, messagebox

try:
    import serial
    import serial.tools.list_ports as list_ports
except ImportError:
    serial = None
    list_ports = None

try:
    import openpyxl
except ImportError:
    openpyxl = None

from mbw_theme import (
    MAIN_BG, WHITE, CARD_BD, HDR_BG, HDR_SUB, PRIMARY, PRIMARY_HOV,
    SEC_BG, SEC_HOV, SEC_TX, INK, PASS_FG, FAIL_FG, RUN_FG, WARN_FG, DIS_FG,
    TERM_BG, TERM_FG, TERM_TX, FONT, FONT_B, FONT_SM, FONT_CARD, FONT_MONO,
    resource_path, flat_btn, sec_btn,
)

APP_TITLE = "MBW RF24 RS485 2.0 - Test App"
CONFIG_PATH = resource_path("app_config.json")
REPORT_PATH = os.path.join(
    os.path.dirname(os.path.abspath(sys.argv[0] if not hasattr(sys, "_MEIPASS") else sys.executable)),
    "mbw_test_report.xlsx")

FWD_RE = re.compile(r"^FWD (RS485->RF|RF->RS485): (\d+) bytes:(.*)$")

# ----- Heartbeat / link health (giong "link quality" cua bo telemetry) -----
# 2 dong tuc thoi khi link doi trang thai:
LINK_UP_RE = re.compile(r"^RF LINK: UP \(peer=(\d+)\)")
LINK_DOWN_RE = re.compile(r"^RF LINK: DOWN \(peer=(\d+), (\d+)ms")
# dong "rf stat" day du (doc dinh ky) chua ca so lieu dinh luong:
RF_STAT_RE = re.compile(
    r"RF_LINK=(UP|DOWN)\s+PEER=(\d+)\s+AGE_MS=(\d+)\s+LOSS_PROMILLE=(\d+)\s+"
    r"REDUND=(\d+)\s+HB_TX=(\d+)\s+HB_RX=(\d+)")


def load_config():
    default = {
        "device_id": "MBW_RF24_RS485",
        "console_baud": 115200,
        "test_groups": [],
    }
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            cfg = json.load(f)
        default.update(cfg)
        return default
    except Exception:
        return default


# =========================================================
# PortLink: 1 ket noi serial + doc nen (background thread) + buffer dong
# =========================================================
class PortLink:
    def __init__(self):
        self.ser = None
        self.port = None
        self.baud = 115200
        self.lines = collections.deque(maxlen=2000)
        self.lock = threading.Lock()
        self.running = False
        self.thread = None
        self.on_line = None  # callback(text) - GOI TU THREAD DOC, phai tu after() ben GUI

    def is_open(self):
        return self.ser is not None and self.ser.is_open

    def connect(self, port, baud=115200):
        if serial is None:
            raise RuntimeError("Chua cai pyserial: pip install pyserial")
        self.disconnect()
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.port = port
        self.baud = baud
        self.running = True
        self.thread = threading.Thread(target=self._reader, daemon=True)
        self.thread.start()

    def disconnect(self):
        self.running = False
        if self.ser is not None:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None

    def _reader(self):
        buf = b""
        while self.running and self.ser is not None:
            try:
                data = self.ser.read(256)
            except Exception:
                break
            if data:
                buf += data
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    text = raw.decode(errors="replace").rstrip("\r")
                    with self.lock:
                        self.lines.append(text)
                    if self.on_line:
                        try:
                            self.on_line(text)
                        except Exception:
                            pass

    def send(self, cmd):
        if self.is_open():
            try:
                self.ser.write((cmd + "\n").encode())
            except Exception:
                pass

    def snapshot_len(self):
        with self.lock:
            return len(self.lines)

    def lines_since(self, idx):
        with self.lock:
            return list(self.lines)[idx:]

    def wait_for(self, substr, timeout_s=2.0):
        start_idx = self.snapshot_len()
        t0 = time.time()
        while time.time() - t0 < timeout_s:
            for ln in self.lines_since(start_idx):
                if substr in ln:
                    return ln
            time.sleep(0.02)
        return None

    def cmd_and_wait(self, cmd, expect_substr, timeout_s=2.0):
        self.send(cmd)
        if expect_substr is None:
            time.sleep(min(timeout_s, 0.3))
            return True, ""
        found = self.wait_for(expect_substr, timeout_s)
        return (found is not None), (found or "")


# =========================================================
# APP
# =========================================================
class MbwTestApp:
    def __init__(self, root):
        self.root = root
        self.cfg = load_config()
        self.link = PortLink()
        self.fwd_count = {"RS485->RF": 0, "RF->RS485": 0}

        root.title(APP_TITLE)
        root.configure(bg=MAIN_BG)
        root.geometry("1000x760")

        self._build_style()
        self._build_header()
        self._build_conn_bar()
        self._build_tabs()

        self.link.on_line = self._on_line

        self.test_running = False

    # ---------- STYLE (dong bo voi modbus_poll_app.py) ----------
    def _build_style(self):
        """Style ttk dung chung voi modbus_poll_app.py: cung bang mau/font
        Industrial Flat (mbw_theme.py) cho Notebook (tab)/Treeview, kem fix
        loi tab "bi lech/nhay" khi bam vao (theme clam mac dinh ve them 1
        'focus ring' rieng ben trong tab, gay lech tam thoi luc bam)."""
        style = ttk.Style()
        try:
            style.theme_use("clam")
        except Exception:
            pass

        style.configure(".", background=MAIN_BG, foreground=INK, font=FONT)
        style.configure("TFrame", background=MAIN_BG)
        style.configure("TLabel", background=MAIN_BG, foreground=INK, font=FONT)

        style.configure("TNotebook", background=MAIN_BG, borderwidth=0, tabmargins=(2, 4, 2, 0))
        style.configure("TNotebook.Tab", background=SEC_BG, foreground=SEC_TX,
                        font=FONT, padding=(14, 6), borderwidth=0)
        style.map("TNotebook.Tab",
                  background=[("selected", PRIMARY)],
                  foreground=[("selected", WHITE)],
                  expand=[("selected", (1, 1, 1, 0))])

        # Bo phan tu "Notebook.focus" trong layout mac dinh cua theme "clam" -
        # day la nguyen nhan gay tab bi "nhay"/lech vi tri ngay luc bam vao.
        style.layout("TNotebook.Tab", [
            ("Notebook.tab", {
                "sticky": "nswe",
                "children": [
                    ("Notebook.padding", {
                        "side": "top",
                        "sticky": "nswe",
                        "children": [
                            ("Notebook.label", {"side": "top", "sticky": ""})
                        ],
                    })
                ],
            })
        ])

        style.configure("Treeview", rowheight=24, font=FONT, background=WHITE,
                        fieldbackground=WHITE, foreground=INK, borderwidth=0)
        style.configure("Treeview.Heading", font=FONT_CARD, background=HDR_BG,
                        foreground=WHITE, relief="flat")
        style.map("Treeview.Heading", background=[("active", HDR_BG)])
        style.map("Treeview", background=[("selected", PRIMARY)],
                  foreground=[("selected", WHITE)])

    # ---------- HEADER ----------
    def _build_header(self):
        hdr = tk.Frame(self.root, bg=HDR_BG, height=56)
        hdr.pack(fill="x", side="top")
        tk.Label(hdr, text="MBW RF24 RS485 2.0 - Test App", bg=HDR_BG, fg=WHITE,
                  font=("Segoe UI", 14, "bold")).pack(side="left", padx=16, pady=10)
        tk.Label(hdr, text="MBW-RF-00-24  |  1 board / 1 máy tính  |  Bridge mặc định luôn BẬT",
                  bg=HDR_BG, fg=HDR_SUB, font=FONT_SM).pack(side="left", padx=4)

    # ---------- CONNECTION BAR (1 board) ----------
    def _build_conn_bar(self):
        card = tk.Frame(self.root, bg=WHITE, highlightbackground=CARD_BD, highlightthickness=1)
        card.pack(fill="x", padx=12, pady=8)

        tk.Label(card, text="Kết nối board", bg=WHITE, fg=INK, font=FONT_CARD).grid(
            row=0, column=0, columnspan=5, sticky="w", padx=10, pady=(8, 2))

        self.port_var = tk.StringVar()
        combo = ttk.Combobox(card, textvariable=self.port_var, width=16, state="readonly")
        combo.grid(row=1, column=0, padx=(10, 4), pady=(0, 10))
        self.combo = combo

        def refresh():
            ports = [p.device for p in list_ports.comports()] if list_ports else []
            combo["values"] = ports
            if ports and not self.port_var.get():
                self.port_var.set(ports[0])

        refresh()
        sec_btn(card, "↻", command=refresh, width=3).grid(row=1, column=1, pady=(0, 10))

        self.status_lbl = tk.Label(card, text="● Chưa kết nối", bg=WHITE, fg=DIS_FG, font=FONT_SM)
        self.status_lbl.grid(row=1, column=2, padx=8, pady=(0, 10), sticky="w")

        self.conn_btn = flat_btn(card, "Connect", PRIMARY, PRIMARY_HOV, command=self._toggle_conn)
        self.conn_btn.grid(row=1, column=3, padx=10, pady=(0, 10))

        card.grid_columnconfigure(4, weight=1)

    def _toggle_conn(self):
        if self.link.is_open():
            self.link.disconnect()
            self.conn_btn.config(text="Connect", bg=PRIMARY, activebackground=PRIMARY_HOV)
            self.status_lbl.config(text="● Chưa kết nối", fg=DIS_FG)
            if hasattr(self, "lbl_rf_link"):
                self.lbl_rf_link.config(text="● Chưa rõ", fg=DIS_FG)
                self.lbl_rf_link_detail.config(text="")
            return
        port = self.port_var.get()
        if not port:
            messagebox.showwarning(APP_TITLE, "Chưa chọn cổng COM")
            return
        try:
            self.link.connect(port, self.cfg.get("console_baud", 115200))
        except Exception as e:
            messagebox.showerror(APP_TITLE, "Không mở được cổng: %s" % e)
            return
        self.status_lbl.config(text="● Đang kiểm tra ID...", fg=WARN_FG)
        self.root.after(300, self._verify_id)

    def _verify_id(self):
        self.link.send("id")
        line = self.link.wait_for(self.cfg.get("device_id", "MBW_RF24_RS485"), 1.5)
        if line:
            self.status_lbl.config(text="● Đã kết nối (%s)" % self.link.port, fg=PASS_FG)
            self.conn_btn.config(text="Disconnect", bg=SEC_BG, fg=SEC_TX, activebackground=SEC_HOV)
            self._refresh_bridge_stat()
            self._schedule_rf_link_poll()
        else:
            self.status_lbl.config(text="● Sai thiết bị / không phản hồi", fg=FAIL_FG)

    # ---------- LINE ROUTER (dung chung cho moi tab dang lang nghe) ----------
    def _on_line(self, text):
        m = FWD_RE.match(text)
        m_up = LINK_UP_RE.match(text)
        m_down = LINK_DOWN_RE.match(text)

        def apply():
            if hasattr(self, "raw_log"):
                self.raw_log.insert("end", text + "\n")
                self.raw_log.see("end")
            if hasattr(self, "term_log"):
                self.term_log.insert("end", text + "\n")
                self.term_log.see("end")
            if m:
                self._add_fwd_row(m.group(1), int(m.group(2)), m.group(3).strip())
            # Cap nhat NGAY khi board tu bao doi trang thai link (khong doi
            # vong poll "rf stat" tiep theo) - giong canh bao "mat lien lac"
            # tuc thi cua GCS/telemetry.
            if m_up:
                self._apply_link_ui(True, m_up.group(1))
            elif m_down:
                self._apply_link_ui(False, m_down.group(1))

        self.root.after(0, apply)

    # ---------- TABS ----------
    def _build_tabs(self):
        nb = ttk.Notebook(self.root)
        nb.pack(fill="both", expand=True, padx=12, pady=(0, 12))

        self.tab_fwd = tk.Frame(nb, bg=MAIN_BG)
        self.tab_test = tk.Frame(nb, bg=MAIN_BG)
        self.tab_term = tk.Frame(nb, bg=MAIN_BG)
        nb.add(self.tab_fwd, text="  Giám sát Forward  ")
        nb.add(self.tab_test, text="  Test tự động  ")
        nb.add(self.tab_term, text="  Terminal  ")

        self._build_forward_tab(self.tab_fwd)
        self._build_test_tab(self.tab_test)
        self._build_terminal_tab(self.tab_term)

    # ---------- TAB: GIAM SAT FORWARD ----------
    def _build_forward_tab(self, parent):
        top = tk.Frame(parent, bg=MAIN_BG)
        top.pack(fill="x", pady=(6, 4))

        self.btn_bridge = flat_btn(top, "Bridge: ?", PRIMARY, PRIMARY_HOV,
                                    command=lambda: self._bridge_toggle())
        self.btn_bridge.pack(side="left")

        self.btn_bridge_log = sec_btn(top, "Log: ?", command=lambda: self._bridge_log_toggle())
        self.btn_bridge_log.pack(side="left", padx=6)

        sec_btn(top, "Đọc thống kê (bridge stat)", command=self._refresh_bridge_stat).pack(
            side="left", padx=6)

        self.lbl_stat = tk.Label(top, text="RS485→RF: 0    RF→RS485: 0", bg=MAIN_BG, fg=INK,
                                  font=FONT_B)
        self.lbl_stat.pack(side="left", padx=16)

        # --- cong cu Modbus Master that (poll qua cong RS485 vat ly, khong
        # phai console CLI) - phep test thuc te nhat cho chuc nang bridge ---
        flat_btn(top, "🔌 Modbus Poll Test (RS485 thật)", PRIMARY, PRIMARY_HOV,
                  command=self._open_modbus_poll).pack(side="right", padx=6)

        # --- RF LINK QUALITY: giong dong ho "link/telemetry" - board tu gui
        # heartbeat dinh ky, app doc "rf stat" de hien UP/DOWN + % mat + do
        # du phong (redundant TX) dang tu dieu chinh (xem rf_link.cpp) ---
        link_row = tk.Frame(parent, bg=MAIN_BG)
        link_row.pack(fill="x", pady=(0, 4))
        tk.Label(link_row, text="RF Link:", bg=MAIN_BG, fg=INK, font=FONT_SM).pack(side="left")
        self.lbl_rf_link = tk.Label(link_row, text="● Chưa rõ", bg=MAIN_BG, fg=DIS_FG, font=FONT_B)
        self.lbl_rf_link.pack(side="left", padx=(6, 16))
        self.lbl_rf_link_detail = tk.Label(link_row, text="", bg=MAIN_BG, fg=SEC_TX, font=FONT_SM)
        self.lbl_rf_link_detail.pack(side="left")
        sec_btn(link_row, "Đọc RF Link (rf stat)", command=self._refresh_rf_link_stat).pack(
            side="left", padx=8)

        # --- gui thu RS485 de tu kiem tra forward khi chua co Modbus that ---
        send_row = tk.Frame(parent, bg=MAIN_BG)
        send_row.pack(fill="x", pady=(0, 6))
        tk.Label(send_row, text="Gửi thử lên RS485 của board này:", bg=MAIN_BG, fg=INK,
                 font=FONT_SM).pack(side="left")
        self.entry_rs485 = tk.Entry(send_row, font=FONT_MONO, width=30)
        self.entry_rs485.insert(0, "MBW TEST")
        self.entry_rs485.pack(side="left", padx=6)
        flat_btn(send_row, "Gửi (rs485 <text>)", PRIMARY, PRIMARY_HOV,
                  command=self._send_rs485_test).pack(side="left")

        cols = ("time", "dir", "len", "preview")
        self.fwd_tree = ttk.Treeview(parent, columns=cols, show="headings", height=14)
        self.fwd_tree.heading("time", text="Giờ")
        self.fwd_tree.heading("dir", text="Hướng")
        self.fwd_tree.heading("len", text="Số byte")
        self.fwd_tree.heading("preview", text="Preview (hex)")
        self.fwd_tree.column("time", width=90, anchor="center")
        self.fwd_tree.column("dir", width=120, anchor="center")
        self.fwd_tree.column("len", width=70, anchor="center")
        self.fwd_tree.column("preview", width=560)
        self.fwd_tree.pack(fill="both", expand=True, pady=(0, 6))
        self.fwd_tree.tag_configure("rs485_rf", foreground=PRIMARY)
        self.fwd_tree.tag_configure("rf_rs485", foreground=WARN_FG)

        tk.Label(parent, text="Console thô (mọi dòng board in ra):", bg=MAIN_BG, fg=INK,
                 font=FONT_SM).pack(anchor="w")
        self.raw_log = tk.Text(parent, height=8, bg=TERM_BG, fg=TERM_FG, font=FONT_MONO,
                                insertbackground=WHITE)
        self.raw_log.pack(fill="x")

    def _add_fwd_row(self, direction, length, preview):
        self.fwd_count[direction] = self.fwd_count.get(direction, 0) + 1
        tag = "rs485_rf" if direction == "RS485->RF" else "rf_rs485"
        arrow = "RS485 → RF" if direction == "RS485->RF" else "RF → RS485"
        self.fwd_tree.insert("", 0, values=(datetime.now().strftime("%H:%M:%S"), arrow, length, preview),
                              tags=(tag,))
        self.lbl_stat.config(text="RS485→RF: %d    RF→RS485: %d" %
                              (self.fwd_count.get("RS485->RF", 0), self.fwd_count.get("RF->RS485", 0)))

    def _require_conn(self):
        if not self.link.is_open():
            messagebox.showwarning(APP_TITLE, "Chưa kết nối board.")
            return False
        return True

    def _bridge_toggle(self):
        if not self._require_conn():
            return
        self.link.send("bridge stat")
        line = self.link.wait_for("BRIDGE=", 1.0) or ""
        want_off = "BRIDGE=ON" in line
        self.link.send("bridge off" if want_off else "bridge on")
        self.root.after(200, self._refresh_bridge_stat)

    def _bridge_log_toggle(self):
        if not self._require_conn():
            return
        self.link.send("bridge stat")
        line = self.link.wait_for("BRIDGE=", 1.0) or ""
        want_off = "LOG=ON" in line
        self.link.send("bridge log off" if want_off else "bridge log on")
        self.root.after(200, self._refresh_bridge_stat)

    def _refresh_bridge_stat(self):
        if not self.link.is_open():
            return
        self.link.send("bridge stat")
        line = self.link.wait_for("BRIDGE=", 1.0)
        if not line:
            return
        on = "BRIDGE=ON" in line
        log_on = "LOG=ON" in line
        self.btn_bridge.config(text="Bridge: BẬT" if on else "Bridge: TẮT",
                                bg=PASS_FG if on else FAIL_FG,
                                activebackground=PASS_FG if on else FAIL_FG)
        self.btn_bridge_log.config(text="Log: BẬT" if log_on else "Log: TẮT")

    # ---------- RF LINK QUALITY (heartbeat, giong dong ho telemetry) ----------
    def _refresh_rf_link_stat(self):
        if not self.link.is_open():
            return
        self.link.send("rf stat")
        line = self.link.wait_for("RF_LINK=", 1.0)
        if not line:
            return
        m = RF_STAT_RE.search(line)
        if not m:
            return
        up, peer, age_ms, loss_pm, redund, hb_tx, hb_rx = m.groups()
        self._apply_link_ui(up == "UP", peer)
        loss_pct = int(loss_pm) / 10.0  # phan nghin -> %
        self.lbl_rf_link_detail.config(
            text="peer=%s | mất ~%.1f%% | dự phòng=%sx | HB tx/rx=%s/%s | %sms trước" %
                 (peer, loss_pct, redund, hb_tx, hb_rx, age_ms))

    def _apply_link_ui(self, up, peer):
        self.lbl_rf_link.config(
            text="● LINK UP (peer=%s)" % peer if up else "● LINK DOWN",
            fg=PASS_FG if up else FAIL_FG)

    def _schedule_rf_link_poll(self):
        """Tu dong doc 'rf stat' dinh ky (giong man hinh 'link quality' cua
        GCS/telemetry) - chi chay khi con dang ket noi board."""
        if self.link.is_open():
            self._refresh_rf_link_stat()
            self.root.after(3000, self._schedule_rf_link_poll)

    def _send_rs485_test(self):
        if not self._require_conn():
            return
        text = self.entry_rs485.get().strip() or "MBW TEST"
        self.link.send("rs485 %s" % text)

    def _open_modbus_poll(self):
        """Mo cong cu Modbus Poll Test (modbus_poll_app.py) trong 1 tien trinh/
        cua so rieng - dung de bom Modbus RTU that qua cong RS485 vat ly cua
        board (khac voi cong COM console CLI dang ket noi o tren). Chay tach
        tien trinh de khong block app chinh va de dung dong thoi ca 2 (VD: 1
        may vua giam sat Forward vua chay Modbus Master qua RS485)."""
        try:
            if getattr(sys, "frozen", False):
                subprocess.Popen([sys.executable, "--modbus"])
            else:
                script_dir = os.path.dirname(os.path.abspath(__file__))
                subprocess.Popen([sys.executable,
                                   os.path.join(script_dir, "mbw_test_app.py"), "--modbus"])
        except Exception as e:
            messagebox.showerror(APP_TITLE, "Không mở được Modbus Poll Test:\n%s" % e)

    # ---------- TAB: TEST TU DONG ----------
    def _build_test_tab(self, parent):
        top = tk.Frame(parent, bg=MAIN_BG)
        top.pack(fill="x", pady=(6, 4))

        self.btn_run = flat_btn(top, "▶ Run Test", PRIMARY, PRIMARY_HOV,
                                 command=self.run_test_async)
        self.btn_run.pack(side="left")

        sec_btn(top, "Export Report", command=self.export_report).pack(side="left", padx=8)

        self.progress = ttk.Progressbar(top, mode="determinate")
        self.progress.pack(side="left", fill="x", expand=True, padx=12)

        cols = ("name", "status", "detail")
        self.tree = ttk.Treeview(parent, columns=cols, show="headings", height=10)
        self.tree.heading("name", text="Bước test")
        self.tree.heading("status", text="Kết quả")
        self.tree.heading("detail", text="Chi tiết")
        self.tree.column("name", width=380)
        self.tree.column("status", width=90, anchor="center")
        self.tree.column("detail", width=420)
        self.tree.pack(fill="both", expand=True, pady=6)

        self.tree.tag_configure("pass", foreground=PASS_FG)
        self.tree.tag_configure("fail", foreground=FAIL_FG)
        self.tree.tag_configure("run", foreground=RUN_FG)
        self.tree.tag_configure("pend", foreground=DIS_FG)

        for g in self.cfg.get("test_groups", []):
            self.tree.insert("", "end", iid=g["key"], values=(g["name"], "chờ", ""), tags=("pend",))

        self.log = tk.Text(parent, height=6, bg=TERM_BG, fg=TERM_FG, font=FONT_MONO,
                            insertbackground=WHITE)
        self.log.pack(fill="x", pady=(4, 0))

    def _log(self, msg):
        self.log.insert("end", "[%s] %s\n" % (datetime.now().strftime("%H:%M:%S"), msg))
        self.log.see("end")

    def _set_row(self, key, status, detail=""):
        tagmap = {"PASS": "pass", "FAIL": "fail", "RUN": "run", "PEND": "pend"}
        vals = self.tree.item(key, "values")
        self.tree.item(key, values=(vals[0], status, detail), tags=(tagmap.get(status, "pend"),))
        self.root.update_idletasks()

    def run_test_async(self):
        if self.test_running:
            return
        if not self.link.is_open():
            messagebox.showwarning(APP_TITLE, "Cần kết nối board trước khi Run Test.")
            return
        t = threading.Thread(target=self._run_test_seq, daemon=True)
        t.start()

    def _run_test_seq(self):
        self.test_running = True
        groups = self.cfg.get("test_groups", [])
        self.progress["maximum"] = max(1, len(groups))
        self.progress["value"] = 0
        pass_count = 0

        for g in groups:
            key = g["key"]
            self._set_row(key, "RUN")
            self._log("Chạy: %s" % g["name"])

            prereq = g.get("manual_prereq")
            if prereq:
                self._log("  (*) Yêu cầu: %s" % prereq)

            try:
                ok, detail = self.link.cmd_and_wait(g["cmd"], g.get("expect_contains"))
            except Exception as e:
                ok, detail = False, "Lỗi: %s" % e

            self._set_row(key, "PASS" if ok else "FAIL", detail)
            self._log("  -> %s %s" % ("PASS" if ok else "FAIL", detail))
            if ok:
                pass_count += 1
            self.progress["value"] += 1

        self._log("Hoàn tất: %d/%d bước PASS" % (pass_count, len(groups)))
        self.test_running = False
        self._last_result_summary = (pass_count, len(groups))
        self._append_report_row()

    def _append_report_row(self):
        pass_count, total = getattr(self, "_last_result_summary", (0, 0))
        row = [datetime.now().strftime("%Y-%m-%d %H:%M:%S"), self.link.port, pass_count, total,
               "PASS" if pass_count == total else "FAIL"]
        for g in self.cfg.get("test_groups", []):
            vals = self.tree.item(g["key"], "values")
            row.append(vals[1])

        if openpyxl is not None:
            try:
                if os.path.exists(REPORT_PATH):
                    wb = openpyxl.load_workbook(REPORT_PATH)
                    ws = wb.active
                else:
                    wb = openpyxl.Workbook()
                    ws = wb.active
                    header = ["Thời gian", "Cổng COM", "Số bước PASS", "Tổng số bước",
                               "Kết quả chung"] + [g["name"] for g in self.cfg.get("test_groups", [])]
                    ws.append(header)
                ws.append(row)
                wb.save(REPORT_PATH)
                self._log("Đã ghi báo cáo: %s" % REPORT_PATH)
            except Exception as e:
                self._log("Lỗi ghi report xlsx: %s" % e)
        else:
            csv_path = REPORT_PATH.replace(".xlsx", ".csv")
            new_file = not os.path.exists(csv_path)
            with open(csv_path, "a", encoding="utf-8") as f:
                if new_file:
                    f.write(",".join(["Thoi gian", "Cong COM", "So buoc PASS", "Tong so buoc",
                                        "Ket qua chung"] +
                                       [g["name"] for g in self.cfg.get("test_groups", [])]) + "\n")
                f.write(",".join(str(x) for x in row) + "\n")
            self._log("Đã ghi báo cáo (CSV, thiếu openpyxl): %s" % csv_path)

    def export_report(self):
        self._append_report_row()

    # ---------- TAB: TERMINAL ----------
    def _build_terminal_tab(self, parent):
        self.term_log = tk.Text(parent, bg=TERM_BG, fg=TERM_FG, font=FONT_MONO,
                                 insertbackground=WHITE)
        self.term_log.pack(fill="both", expand=True, padx=8, pady=(8, 4))

        quick = tk.Frame(parent, bg=MAIN_BG)
        quick.pack(fill="x", padx=8, pady=(0, 4))
        for cmd in ["id", "ver", "dip", "flash", "rtc", "rf id", "rf stat",
                     "bridge stat", "bridge log on", "bridge log off"]:
            sec_btn(quick, cmd, command=lambda c=cmd: self._send_and_echo(c),
                    font=FONT_SM).pack(side="left", padx=2, pady=2)

        entry_row = tk.Frame(parent, bg=MAIN_BG)
        entry_row.pack(fill="x", padx=8, pady=(0, 8))
        entry = tk.Entry(entry_row, font=FONT_MONO)
        entry.pack(side="left", fill="x", expand=True)
        self.term_entry = entry

        history = []
        hist_idx = [0]

        def send_entry(event=None):
            cmd = entry.get().strip()
            if not cmd:
                return
            history.append(cmd)
            hist_idx[0] = len(history)
            self._send_and_echo(cmd)
            entry.delete(0, "end")

        def hist_up(event):
            if history and hist_idx[0] > 0:
                hist_idx[0] -= 1
                entry.delete(0, "end")
                entry.insert(0, history[hist_idx[0]])

        def hist_down(event):
            if history and hist_idx[0] < len(history) - 1:
                hist_idx[0] += 1
                entry.delete(0, "end")
                entry.insert(0, history[hist_idx[0]])
            else:
                hist_idx[0] = len(history)
                entry.delete(0, "end")

        entry.bind("<Return>", send_entry)
        entry.bind("<Up>", hist_up)
        entry.bind("<Down>", hist_down)

        flat_btn(entry_row, "Gửi", PRIMARY, PRIMARY_HOV, command=send_entry).pack(side="left", padx=(6, 0))

    def _send_and_echo(self, cmd):
        if not self._require_conn():
            return
        self.term_log.insert("end", "> %s\n" % cmd)
        self.term_log.see("end")
        self.link.send(cmd)


def main():
    if "--modbus" in sys.argv:
        import modbus_poll_app
        modbus_poll_app.main()
        return

    root = tk.Tk()
    app = MbwTestApp(root)

    def on_close():
        app.link.disconnect()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.mainloop()


if __name__ == "__main__":
    main()
