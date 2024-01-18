import sys

sys.path.append("..")
import jdepp

parser = jdepp.Jdepp()

model_path = "../model/knbc/"
parser.load_model(model_path)

# NOTE: Mecab format: surface + TAB + feature(comma separated 7 fields)
input_postagged = """吾輩	名詞,普通名詞,*,*,吾輩,わがはい,代表表記:我が輩/わがはい カテゴリ:人
は	助詞,副助詞,*,*,は,は,*
猫	名詞,普通名詞,*,*,猫,ねこ,*
である	判定詞,*,判定詞,デアル列基本形,だ,である,*
。	特殊,句点,*,*,。,。,*
名前	名詞,普通名詞,*,*,名前,なまえ,*
は	助詞,副助詞,*,*,は,は,*
まだ	副詞,*,*,*,まだ,まだ,*
ない	形容詞,*,イ形容詞アウオ段,基本形,ない,ない,*
。	特殊,句点,*,*,。,。,*
EOS
"""

sent = parser.parse_from_postagged(input_postagged)
print(sent)

for chunk in sent.chunks():
    print(chunk)
