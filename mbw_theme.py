# -*- coding: utf-8 -*-
"""
mbw_theme.py - Theme + helper DUNG CHUNG cho app test MBW RF24 RS485 2.0.
====================================================================
Sao chep tu opms_theme.py (du an OPMS 1.6 / Smart PDU) de dong bo phong cach
giua cac app test noi bo: Industrial / Engineering Software (Flat), tham
khao Keysight BenchVue, STM32CubeProgrammer, Siemens TIA. Khong gradient/do
bong/animation.

Dung:  from mbw_theme import *
"""

import os
import sys

# ===== 3 nhom mau: nen sang | card trang | header xanh than =====
WHITE   = "#FFFFFF"
MAIN_BG = "#F4F6F8"   # nen app
CARD_BD = "#D8DEE5"   # vien card
HDR_BG  = "#24364B"   # header (xanh than dam)
CTRL_BG = "#2D3E50"   # thanh dieu khien
HDR_SUB = "#AEB8C4"   # chu phu tren nen toi

PRIMARY     = "#295C9A"   # nut chinh (Connect, Auto Test, Read/Measure...)
PRIMARY_HOV = "#3474C7"
SEC_BG      = "#E7EAEE"   # nut phu (Secondary)
SEC_HOV     = "#D8DEE5"
SEC_TX      = "#39424E"   # chu tren nut phu / chu thuong
INK         = "#39424E"   # chu chinh tren nen sang

# Mau trang thai
PASS_FG = "#22A55A"   # PASS
FAIL_FG = "#D63C3C"   # FAIL
RUN_FG  = "#2D8CFF"   # RUNNING (dang chay)
WARN_FG = "#F5A623"   # WARNING
DIS_FG  = "#B8BDC5"   # DISABLED / Pending

# Xam phu tro (icon, vien, nen phu)
GREY    = "#6B7280"
GREY_DK = "#4B5563"
GREY_LT = "#ECEFF3"
GREY_BD = CARD_BD
GREY_SB = HDR_SUB
NAVY    = HDR_BG
NAVY_DK = "#1B2A38"

HDR_ACC  = HDR_BG
BLUE_ACC = HDR_BG
GREY_TX  = SEC_TX
ON_BG    = PRIMARY
ON_HOV   = PRIMARY_HOV
OFF_BG   = SEC_BG
OFF_HOV  = SEC_HOV
READ_BG  = PRIMARY
READ_HOV = PRIMARY_HOV
DOT_ON   = PASS_FG
DOT_OFF  = DIS_FG

# Terminal (console)
TERM_BG = "#1F2933"
TERM_FG = "#D8E2EA"
TERM_TX = "#FFFFFF"

# Typography (Segoe UI): Main 16 / Card 13 / Normal 11 / Value 12 (~px)
FONT      = ("Segoe UI", 10)
FONT_B    = ("Segoe UI", 10, "bold")
FONT_SM   = ("Segoe UI", 9)
FONT_CARD = ("Segoe UI", 11, "bold")
FONT_VAL  = ("Segoe UI", 11, "bold")
FONT_MONO = ("Consolas", 10)


def resource_path(name):
    """Duong dan tai nguyen (chay .py hoac da dong goi PyInstaller)."""
    base = getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base, name)


def flat_btn(parent, text, bg, hover, fg="white", relief="raised", bd=2,
             font=None, **kw):
    """Nut noi khoi (raised) - cam giac ky thuat."""
    import tkinter as tk
    return tk.Button(parent, text=text, bg=bg, fg=fg, font=font or FONT_B,
                      activebackground=hover, activeforeground=fg,
                      relief=relief, bd=bd, cursor="hand2",
                      disabledforeground="#9AA3AD", **kw)


def sec_btn(parent, text, **kw):
    """Nut phu (Secondary): nen sang #E7EAEE, chu toi #39424E."""
    return flat_btn(parent, text, SEC_BG, SEC_HOV, fg=SEC_TX, **kw)
