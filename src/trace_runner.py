import subprocess
from pprint import pformat

files = [
    "fp_1.bz2",
    "fp_2.bz2",
    "int_1.bz2",
    "int_2.bz2",
    "mm_1.bz2",
    "mm_2.bz2"
]

gshare_historyBits = 13
global_historyBits = 9
lhistoryBits = 10
pcIndexBits = 10

preds = {
    "gshare": "gshare:{}".format(gshare_historyBits),
    "tournament:": "tournament:{}:{}:{}".format(global_historyBits, lhistoryBits, pcIndexBits)
}


def runPreds():
    outputs = []
    for file in files:
        pout = []
        for name, pred in preds.items():
            pout.append({name: subprocess.check_output(
                "bunzip2 -kc ../traces/{} | ./predictor --{}".format(file, pred), shell=True, text=True).strip()})
        outputs.append(pout)
    return outputs


if __name__ == "__main__":
    with open("data.txt", "w") as f:
        f.write(pformat(runPreds()))
