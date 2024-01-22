import sys
from tqdm import tqdm
import time
import jdepp

parser = jdepp.Jdepp()

#model_path = "model/knbc"
#model_path = "model_2ndpoly/model/knbc"
model_path = "model_3rdpoly/model/knbc"
parser.load_model(model_path)

input_filename = "output-wiki-postagged.txt"
#input_filename = "test-postagged.txt"

print("reading test data:", input_filename)
lines = open(input_filename, 'r', encoding='utf8').readlines()

s = time.time()

nprocessed_sentences = 0

sents = []
for line in tqdm(lines, desc="[jdepp]"):
    if line == '\n':
        # newline only line is not allowed.
        continue

    sents.append(line)

    if line == "EOS\n":
        result = parser.parse_from_postagged(sents)
        print(result)
        
        nprocessed_sentences += 1
        sents = []
        
e = time.time()
proc_sec = e - s
ms_per_sentence = 1000.0 * proc_sec / float(nprocessed_sentences)
sys.stderr.write("J.DepP: Total {} secs({} sentences. {} ms per sentence))\n".format(proc_sec, nprocessed_sentences, ms_per_sentence))
