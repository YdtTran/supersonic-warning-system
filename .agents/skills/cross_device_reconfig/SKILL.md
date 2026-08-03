---
name: cross_device_reconfig
description: Automatically scans the host environment for ESP-IDF paths, Python venvs, PowerShell profiles, and serial COM ports, then generates a local AGENTS.md file from AGENTS.template.md.
---

# Cross-Device Workspace Reconfiguration Skill

This skill provides an automated scanner and systematic procedure to detect local ESP-IDF installations, tools, virtual environments, PowerShell profiles, and hardware COM ports when moving this workspace to a new computer.

---

## Skill Overview

When developers or AI Agents switch machines, environment configurations (such as `IDF_PATH`, Python `venv` paths, and USB serial `COM` ports) vary. This skill uses `AGENTS.template.md` as the master blueprint and generates a machine-specific `AGENTS.md` file.

---

## Automated Reconfiguration (Recommended)

### Step 1: Run the Environment Scanner
Execute the included Python scanner script from the project root:

```cmd
python .agents/skills/cross_device_reconfig/scripts/scan_env.py
```

### What the Scanner Does Automatically:
1. **Detects ESP-IDF Path**: Checks `%IDF_PATH%` environment variable or scans standard directories (`E:\esp\*`, `C:\Espressif\frameworks\*`, `C:\esp\*`).
2. **Detects ESP Tools & Virtual Env**: Locates `C:\Espressif\tools`, versioned Python virtual envs (`python\v6.0.2\venv`), and PowerShell environment export profiles (`Microsoft.v6.0.2.PowerShell_profile.ps1`).
3. **Scans Hardware COM Ports**: Enumerates available USB serial ports using Windows PowerShell APIs and assigns default or connected ports to `sensor-node` and `waveshare-screen`.
4. **Generates `AGENTS.md`**: Reads `AGENTS.template.md`, replaces all `<...>` placeholders with scanned parameters, and writes out the local `AGENTS.md` file.

---

## Manual Verification Procedure

If the scanner script is not run, perform these manual steps:

1. **Copy Template**:
   ```cmd
   copy AGENTS.template.md AGENTS.md
   ```
2. **Identify Local ESP-IDF Configuration**:
   - Locate your ESP-IDF installation directory (e.g. `C:\Espressif\frameworks\esp-idf-v5.3` or `E:\esp\v6.0.2\esp-idf`).
   - Locate the PowerShell profile (e.g. `C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1`).
3. **Identify Connected Hardware Ports**:
   Run in PowerShell:
   ```powershell
   [System.IO.Ports.SerialPort]::GetPortNames()
   ```
4. **Update `AGENTS.md`**:
   Replace placeholders `<IDF_PATH>`, `<SHELL_PROFILE_PATH>`, `<PYTHON_VENV_PATH>`, `<SENSOR_NODE_PORT>`, and `<WAVESHARE_SCREEN_PORT>` in `AGENTS.md` with your local values.

---

## Verification Checklist

After running the skill:

- [ ] `AGENTS.md` exists in the workspace root.
- [ ] `AGENTS.md` contains valid, absolute local paths.
- [ ] Running project batch scripts (e.g. `firmware/sensor-node/build_and_flash.bat build`) successfully activates ESP-IDF.
- [ ] `AGENTS.md` is ignored by git (via `.gitignore`), ensuring local changes are not committed to source control.
