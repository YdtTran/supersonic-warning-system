import os
import sys
import glob
import subprocess
import re

def find_idf_paths():
    paths = {}
    
    # Check IDF_PATH env
    if 'IDF_PATH' in os.environ and os.path.exists(os.environ['IDF_PATH']):
        paths['idf_path'] = os.environ['IDF_PATH']
    else:
        # Check standard locations
        candidates = [
            r"E:\esp\v6.0.2\esp-idf",
            r"C:\Espressif\frameworks\esp-idf*",
            r"E:\esp\*\esp-idf",
            r"C:\esp\esp-idf*",
            r"C:\Users\*\esp\esp-idf*"
        ]
        for pattern in candidates:
            matches = glob.glob(pattern)
            if matches:
                paths['idf_path'] = matches[0]
                break
    
    # Check IDF_TOOLS_PATH
    tools_path = os.environ.get('IDF_TOOLS_PATH', r"C:\Espressif\tools")
    if os.path.exists(tools_path):
        paths['idf_tools_path'] = tools_path
        
        # Look for PowerShell profile
        profiles = glob.glob(os.path.join(tools_path, "*PowerShell_profile.ps1"))
        if profiles:
            paths['shell_profile'] = profiles[0]
            
        # Look for Python venv (prefer directories starting with 'v' like v6.0.2)
        python_dir = os.path.join(tools_path, "python")
        if os.path.exists(python_dir):
            entries = sorted(os.listdir(python_dir), reverse=True)
            for entry in entries:
                if entry.startswith("v"):
                    venv_candidate = os.path.join(python_dir, entry, "venv")
                    if os.path.isdir(venv_candidate):
                        paths['python_venv'] = venv_candidate
                        break

    return paths

def get_com_ports():
    ports = []
    try:
        # Use PowerShell to get COM ports
        cmd = "powershell -NoProfile -Command \"[System.IO.Ports.SerialPort]::GetPortNames()\""
        out = subprocess.check_output(cmd, shell=True, text=True)
        ports = [p.strip() for p in out.splitlines() if p.strip()]
    except Exception:
        pass
    return ports

def replace_placeholder(text, placeholder_pattern, replacement_value):
    # Use re.sub with a lambda to prevent backslash escape errors in replacement strings
    return re.sub(placeholder_pattern, lambda m: replacement_value, text)

def scan_and_update(write_file=True):
    print("===================================================")
    print("[Cross-Device] Scanning Local ESP-IDF Environment...")
    print("===================================================")
    info = find_idf_paths()
    com_ports = get_com_ports()
    
    idf_path = info.get('idf_path', r'E:\esp\v6.0.2\esp-idf')
    idf_tools = info.get('idf_tools_path', r'C:\Espressif\tools')
    py_venv = info.get('python_venv', r'C:\Espressif\tools\python\v6.0.2\venv')
    sh_profile = info.get('shell_profile', r'C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1')
    
    # Keep current ports in AGENTS.md if matching defaults, or use scanned ports
    sensor_port = "COM8"
    screen_port = "COM9"
    if com_ports:
        if "COM8" in com_ports:
            sensor_port = "COM8"
        elif len(com_ports) > 0:
            sensor_port = com_ports[0]
            
        if "COM9" in com_ports:
            screen_port = "COM9"
        elif len(com_ports) > 1:
            screen_port = com_ports[1]
    
    print(f" - IDF Path:            {idf_path}")
    print(f" - IDF Tools Path:      {idf_tools}")
    print(f" - Python Virtual Env:  {py_venv}")
    print(f" - Shell Profile:       {sh_profile}")
    print(f" - Detected COM Ports:  {', '.join(com_ports) if com_ports else 'None (using defaults)'}")
    print(f"   -> Sensor Node Port: {sensor_port}")
    print(f"   -> Waveshare Port:   {screen_port}")
    
    template_file = "AGENTS.template.md"
    if write_file and os.path.exists(template_file):
        with open(template_file, "r", encoding="utf-8") as f:
            content = f.read()
        
        # Replace placeholders safely
        content = replace_placeholder(content, r"<IDF_PATH.*?>", idf_path)
        content = replace_placeholder(content, r"<IDF_TOOLS_PATH.*?>", idf_tools)
        content = replace_placeholder(content, r"<PYTHON_VENV_PATH.*?>", py_venv)
        content = replace_placeholder(content, r"<SHELL_PROFILE_PATH.*?>", sh_profile)
        content = replace_placeholder(content, r"<SENSOR_NODE_PORT.*?>", sensor_port)
        content = replace_placeholder(content, r"<WAVESHARE_SCREEN_PORT.*?>", screen_port)
        
        output_file = "AGENTS.md"
        with open(output_file, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"\n[SUCCESS] Generated/Updated '{output_file}' with local configuration!")

if __name__ == "__main__":
    scan_and_update(write_file=True)
