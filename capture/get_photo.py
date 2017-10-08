import os, sys
import subprocess
#os.system(sys.executable)
args = ["notepad.exe",
        "D:\\Panda\\capture\\test.txt",        
        ]
argscam = ["search_client.exe",
        "192.168.1.23",
        "24069",
        "S,90,70",
        ]
status = subprocess.call(argscam)
print status


