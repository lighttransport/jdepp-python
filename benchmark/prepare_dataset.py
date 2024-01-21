import os
import functools
import signal
import concurrent.futures

#
import ja_sentence_segmenter
import datasets
import jagger
from tqdm import tqdm

model_path = "model/kwdlc/patterns"
tagger = jagger.Jagger()
tagger.load_model(model_path)


from ja_sentence_segmenter.common.pipeline import make_pipeline
from ja_sentence_segmenter.concatenate.simple_concatenator import  concatenate_matching
from ja_sentence_segmenter.normalize.neologd_normalizer import  normalize
from ja_sentence_segmenter.split.simple_splitter import  split_newline, split_punctuation

# Assume wikitext all uses '。' for punctuation(no period '.' for punctuation)
split_punc = functools.partial(split_punctuation, punctuations=r"。 !?")
concat_tail_no = functools.partial(concatenate_matching, former_matching_rule=r"^(?P<result>.+)(の)$", remove_former_matched=False)
segmenter = make_pipeline(normalize, split_newline, concat_tail_no, split_punc)


interrupted = False

def handler(signum, frame):
    # Gracefull shutfown
    print('Signal handler called with signal', signum)

    global interrupted
    interrupted = True
    


dss = datasets.load_dataset("range3/wiki40b-ja")
print(dss)

def senter(text):
    
    result = list(segmenter(text))

    outputs = ''
    for sent in result:

        toks = tagger.tokenize(sent)

        pos_tagged = ''
        for tok in toks:
            pos_tagged += tok.surface() + '\t' + tok.feature() + '\n'

        pos_tagged += 'EOS\n'

        # no newline-only line between sentence.
        outputs += pos_tagged

    
    return outputs



def singleprocess_proc(f):
    for example in tqdm(dss['train']):
        texts = example['text'].split()

        # extract paragraph only.
        in_paragraph = False

        txts_result = []
        for text in texts:
            if in_paragraph:
                text = text.replace("_NEWLINE_", '\n')
                text = senter(text)
                f.write(text)
                in_paragraph = False

            if text == "_START_PARAGRAPH_":
                in_paragraph = True

def run_task(texts: [str]):
    out_texts = []

    #global interrupted

    for text in texts:
        #print(text)
        #if interrupted:
        #    return None

        lines = text.split()

        # extract paragraph only.
        in_paragraph = False

        txt_result = ''
        for line in lines:
            if in_paragraph:
                line = line.replace("_NEWLINE_", '\n')
                line = senter(line)
                
                txt_result += line
                in_paragraph = False

            if line == "_START_PARAGRAPH_":
                in_paragraph = True

        out_texts.append(txt_result)

    return {'text': out_texts}

def multiprocess_proc(f):

    split_name = 'train'

    nprocs = max(1, os.cpu_count() // 2)
    print("nprocs", nprocs)
    nexamples_per_batch = 10000 # small batch size results in slow(due to Python future object creation?). 10000 or more recommended for wiki40b/ja `train'

    # datasets.map is a easy solution, but consumes lots of disk space.
    # so disabled atm.
    #
    # processed_ds = dss['train'].map(run_task, batched=True, batch_size=nexamples_per_batch, num_proc=nprocs)
    #for p in tqdm(processed_ds['text']):
    #    f.write(p)

    # ProcessPoolExecutor version

    chunks = []
    for i in tqdm(range(0, len(dss[split_name]['text']), nexamples_per_batch), desc="[chunking input]"):
        chunks.append(dss[split_name]['text'][i:i+nexamples_per_batch])

    signal.signal(signal.SIGINT, handler)
    total_ticks = len(chunks)
    with tqdm(total=total_ticks) as pbar:
        with concurrent.futures.ProcessPoolExecutor(max_workers=nprocs) as executor:
            futures = {executor.submit(run_task, chunks[i]): i for i in range(len(chunks))}

            for future in concurrent.futures.as_completed(futures):
                arg = futures[future]
                result = future.result()
                # single IO
                for text in result['text']:
                    f.write(text)

                del result

                pbar.update(1)

                del future

if __name__ == '__main__':

    f = open("output-wiki-postagged.txt", 'w')

    # multiprocessing: finish in few mins, but consumes 20GB~40GB memory.
    # Use singleprocess_proc() if you face out-of-memory

    # singleprocess_proc(f)
    multiprocess_proc(f)
