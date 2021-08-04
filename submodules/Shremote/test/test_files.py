import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from shlib import include_loader
from shlib import fmt_config
from shlib import cfg_format
from shlib import logger

logger.set_test_mode()
