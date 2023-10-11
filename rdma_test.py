#!/usr/bin/python3
# coding=utf-8

import sys

sys.path.append('..')

from pyverbs.addr import AH, AHAttr, GlobalRoute
from pyverbs.cq import CQ
from pyverbs.device import Context
from pyverbs.enums import *
from pyverbs.mr import MR
from pyverbs.pd import PD
from pyverbs.qp import QP, QPCap, QPInitAttr, QPAttr
from pyverbs.wr import SGE, RecvWR, SendWR
from pyverbs import device as d

RECV_WR = 1
SEND_WR = 2
GRH_LENGTH = 40
