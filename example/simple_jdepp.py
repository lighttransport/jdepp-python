import jdepp

parser = jdepp.Jdepp()

model_path = "model/knbc/"
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

print("--------------------")

# Print parsed result with probability
print(sent.print(prob=True))

print("--------------------")

# Print each bunsetsu(chunk)
for chunk in sent.chunks():
    print("Chunk", chunk)

print("--------------------")
# Print bunsetsu string(concat the surface of tokens)

ss = []
for chunk in sent.chunks():
    
    s = ""
    for token in chunk.tokens():
        s += token.surface()

    ss.append(s)

print("|".join(ss))


print("--------------------")

# Print token in each chunk
for chunk in sent.chunks():
    print("Chunk ID:", chunk.id) 

    for token in chunk.tokens():
        print(token)

    print("")

print("--------------------")

# Show dependents
for chunk in sent.chunks():
    if len(chunk.dependents()):
        for dep_chunk in chunk.dependents():
            print(dep_chunk.id, "->", chunk.id)

        print("")

print("--------------------")

# print tree 
print(jdepp.to_tree(str(sent)))

print("--------------------")

# export as graphviz dot
print(jdepp.to_dot(str(sent)))
