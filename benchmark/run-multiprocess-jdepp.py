import os
import time
import sys
import signal

from tqdm import tqdm
import concurrent.futures

import jdepp

input_filename = "output-wiki-postagged.txt"
#input_filename = "test-postagged.txt"

interrupted = False

def handler(signum, frame):
    # Gracefull shutfown
    print('Signal handler called with signal', signum)

    global interrupted
    interrupted = True

print("reading test data:", input_filename)
lines = open(input_filename, 'r', encoding='utf8').readlines()


ninput_sentences = 0
input_sents = []
sents = []
for line in tqdm(lines, desc="[prepare]"):
    if line == '\n':
        # newline only line is not allowed.
        continue

    sents.append(line)

    if line == "EOS\n":
        # List[str]
        text = ''.join(sents)

        input_sents.append(text)
        
        ninput_sentences += 1
        sents = []

s = time.time()

parser = jdepp.Jdepp()

#model_path = "model/knbc"
#model_path = "model_2ndpoly/model/knbc"
model_path = "model_3rdpoly/model/knbc"
parser.load_model(model_path)

def run_task(in_sents):

    sents = parser.parse_from_postagged_batch(in_sents)
    print(sents)

    n = len(sents)
    del sents

    return n

signal.signal(signal.SIGINT, handler)

nbatches = 1000
total_ticks = len(input_sents) // nbatches

nprocs = os.cpu_count() // 2
with tqdm(total=total_ticks) as pbar:
    with concurrent.futures.ProcessPoolExecutor(max_workers=nprocs) as executor:
        futures = {executor.submit(run_task, input_sents[i:i+nbatches]): i for i in range(0, len(input_sents), nbatches)}

        for future in concurrent.futures.as_completed(futures):
            arg = futures[future]
            result = future.result()

            del result

            pbar.update(1)

            del future
        
e = time.time()
proc_sec = e - s
ms_per_sentence = 1000.0 * proc_sec / float(ninput_sentences)
sys.stderr.write("J.DepP: Total {} secs({} sentences. {} ms per sentence))\n".format(proc_sec, ninput_sentences, ms_per_sentence))
