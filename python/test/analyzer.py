import os, sys
import grpc
import jumanpp_grpc.jumandic_svc_pb2_grpc as jg
import jumanpp_grpc.jumandic_svc_pb2 as jpb
from google.protobuf import text_format


def loop(svc):
    while True:
        str = input("Sentece: ")
        req = jpb.AnalysisRequest()
        req.sentence = str
        result = svc.LatticeDump(req)
        sys.stdout.buffer.write(text_format.MessageToString(result, as_utf8 = True).encode("utf-8"))


def main():
    url = sys.argv[1]
    chan = grpc.insecure_channel(url)
    svc = jg.JumanppJumandicStub(chan)
    loop(svc)


if __name__ == '__main__':
    main()
