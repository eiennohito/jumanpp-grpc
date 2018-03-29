import os, sys
import grpc
import jumanpp_grpc.jumandic_svc_pb2_grpc as jg
import jumanpp_grpc.jumandic_svc_pb2 as jpb


def loop(svc):
    while True:
        str = input("Sentece: ")
        req = jpb.AnalysisRequest()
        req.sentence = str
        result = svc.Juman(req)
        data = [f"{m.surface}_{m.string_pos.pos}" for m in result.morphemes]
        print(" ".join(data))


def main():
    url = sys.argv[1]
    chan = grpc.insecure_channel(url)
    svc = jg.JumanppJumandicStub(chan)
    loop(svc)


if __name__ == '__main__':
    main()
