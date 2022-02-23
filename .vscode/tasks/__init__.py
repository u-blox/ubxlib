from invoke import Collection, Config
import os
from . import nrfconnect, nrf5, stm32cubef4, esp_idf, arduino, automation

tasks_dir = os.path.dirname(os.path.abspath(__file__))
vscode_dir = os.path.abspath(os.path.join(tasks_dir, ".."))
root_dir = os.path.abspath(os.path.join(vscode_dir, ".."))
cfg_dir = vscode_dir

ns = Collection()

cfg = Config()
cfg['root_dir'] = root_dir
cfg['cfg_dir'] = cfg_dir

ns.configure(cfg)
ns.add_collection(nrfconnect)
ns.add_collection(nrf5)
ns.add_collection(stm32cubef4)
ns.add_collection(esp_idf)
ns.add_collection(arduino)
ns.add_collection(automation)
