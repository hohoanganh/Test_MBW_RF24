# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['opms_test_app.py'],
    pathex=[],
    binaries=[],
    datas=[('app_config.json', '.'), ('OPMS_Device.png', '.'), ('Vivoo_logo.png', '.')],
    hiddenimports=['ch348_test_app', 'opms_theme'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='OPMS_Slave_Test',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
