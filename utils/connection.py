import time

from more_itertools import only
from pyverbs.addr import GID
import pyverbs.cm_enums as ce
from pyverbs.cmid import CMID, AddrInfo
from pyverbs.qp import QPInitAttr, QPCap

HANDSHAKE_WORDS = "Hello!"
RESERVED_LEN = 20
class CommError(Exception):
    def __init__(self, message):
        self.message = message
        super(CommError, self).__init__(message)

    def __str__(self):
        return self.message
    
class CommBase:
    def __init__(self):
        pass

    @staticmethod
    def prepare_send_msg(**kwargs):
        if not bool(kwargs):
            send_msg = HANDSHAKE_WORDS

        else:
            print("-- Local Info")
            send_msg = ""
            for key, value in kwargs.items():
                print(key, ":", value)
                send_msg = send_msg + key + ':' + type(value).__name__ + str(value) + ','

            send_msg = send_msg[:-1]
            print('-' * 80)
        return send_msg

    @staticmethod
    def parse_recv_msg(only_handshake, recv_msg):
        if only_handshake:
            try:
                if recv_msg != HANDSHAKE_WORDS:
                    raise CommError("Failed to handshake with remote peer by " + CommBase.__class__.__name__)
            except CommError as e:
                print(e)

        else:
            key_value = {}
            print("-- Remote Info")
            for item in recv_msg.split(','):
                key, value_type, value = item.split(':', 2)
                key_value[key.strip()] = globals()[value_type.strip()](value.strip())
                print(key, ":", value)

            print('-' * 80)

            return key_value
        
class CM(CommBase):
    def __init__(self, port, ip=None):
        super(CommBase, self).__init__()
        cap = QPCap(max_recv_wr=1)
        qp_init_attr = QPInitAttr(cap=cap)
        self.isServer = False

        if ip is None:
            self.isServer = True
            cai = AddrInfo(src='0.0.0.0', src_service=str(port), port_space=ce.RDMA_PS_TCP, flags=ce.RAI_PASSIVE)
            self.cmid = CMID(creator=cai, qp_init_attr=qp_init_attr)
            self.cmid.listen()
            client_cmid = self.cmid.get_request()
            client_cmid.accept()
            self.cmid.close()
            self.cmid = client_cmid

        else:
            cai = AddrInfo(src='0.0.0.0', dst=ip, dst_service=str(port), port_space=ce.RDMA_PS_TCP, flags=0)
            self.cmid = CMID(creator=cai, qp_init_attr=qp_init_attr)
            self.cmid.connect()

    def handshake(self, **kwargs):

        just_hanshake = not bool(kwargs)
        send_msg = self.prepare_send_msg(**kwargs)
        size = len(send_msg)

        recv_mr = self.cmid.reg_msgs(size + RESERVED_LEN)
        self.cmid.post_recv(recv_mr,size)

        send_mr = self.cmid.reg_msgs(size)
        self.cmid.post_send(send_mr, 0, size)

        recv_wc = self.cmid.get_recv_comp()
        send_wc = self.cmid.get_send_comp()

        recv_msg = recv_mr.read(recv_wc.byte_len, 0).decode('utf-8')

        return self.parse_recv_msg(just_hanshake, recv_msg)
    
    def close(self):
        if not self.isServer:
            self.cmid.close()

    def __del__(self):
        self.cmid.close()
