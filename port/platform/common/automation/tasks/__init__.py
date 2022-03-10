from invoke import Collection, Config
from scripts import u_utils
from . import nrfconnect, nrf5, stm32cubef4, esp_idf, arduino, automation

ns = Collection()

cfg = Config()
cfg['root_dir'] = u_utils.UBXLIB_DIR
cfg['vscode_dir'] = u_utils.UBXLIB_DIR + '/.vscode'
cfg['cfg_dir'] = u_utils.AUTOMATION_DIR

ns.configure(cfg)
ns.add_collection(nrfconnect)
ns.add_collection(nrf5)
ns.add_collection(stm32cubef4)
ns.add_collection(esp_idf)
ns.add_collection(arduino)
ns.add_collection(automation)
