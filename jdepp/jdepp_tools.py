# From jdepp/to_tree.py ---------------
# Copyright: Naoki Yoshinaga <ynaga@tkl.iis.u-tokyo.ac.jp>
# License: BSD/GPLv2/LGPLv2.1

import sys, re, os
from unicodedata import east_asian_width as width


class Binfo:
    """ bunsetsu infomation """
    def __init__ (self, *args):
        self.id, self.head, self.prob, self.fail, self.gold = args
        self.morph, self.len, self.depth, self.first_child = "", 0, 0, -1
    def str    (self)         : return self.morph + (self.len % 2 and " " or "")
    def width  (self)         : return (self.len + 1) / 2;
    def offset (self, offset) : return offset - self.width () - self.depth


def treeify (binfo, offset: int = 2, prob: bool = False):
    # offset : offset from the left

    indent = prob and 4 or 3 # for one dependency arc

    tree = ''
    for c in reversed (binfo[:-1]):
        c.depth = binfo[c.head].depth + indent
        binfo[c.head].first_child = c.id
    width = offset + max (b.width () + b.depth for b in binfo) # tree width
    for b in binfo:
        if b.head == -1:
            if b.id != len (binfo) - 1:
                return "no head information; chunking output seems missing?"
            tree += "% 3d:%s%s" % (b.id, "　" * int (b.offset (width)), b.str ())
        else:
            if b.fail:
                tree += b.head > int (b.gold[:-1]) and "\033[31m" or "\033[36m"
            tree += "% 3d:%s%s" % (b.id, u"　" * int (b.offset (width)), b.str ())
            h = binfo[b.head]
            tree += "━" * (b.depth - h.depth - (b.prob and 3 or 1))
            tree += b.prob # "─"
            tree += b.id == h.first_child and "┓" or "┫" # "┐" or "┤"
            tree += b.fail and "%-4s\033[0m" % b.gold or ""
            while h.head != -1: # draw arcs spanning from x < b to y > h
                c, h = h, binfo[h.head]
                tree += "　" * (c.depth - h.depth - (b.fail and 3 or 1))
                tree += h.first_child < b.id and "┃" or "　" # "│" or "　"
                b.fail = False
            tree += "\n"
    return tree

def to_tree(lines, verbose: bool = False, prob: bool = False, morph: bool = False, quiet: bool = False, ignore: str = "" ):
    """
    prob(bool)  : Show dependency probability
    ignore(str) : Ignore line starting with STR
    """
    binfo   = []
    header  = ''
    text    = ''
    charset = ''
    wrong   = False
    pat_s = re.compile (r'[\t\s]')
    pat_i = re.compile (re.escape (ignore))
    tag = set (["D", "A", "P", "I"])
    ww ={'Na':1, 'W':2, 'F':2, 'H':1, 'A':2, 'N':1}

    if isinstance(lines, str):
        # make List[str]
        lines = [line + '\n' for line in lines.split('\n')]

    result = ""
    #for line in iter (sys.stdin.readline, ""): # no buffering
    for line in lines:
        if verbose:
            sys.stdout.write (line)
        if line[:7] == '# S-ID:' or (ignore and pat_i.match (line)):
            header += line
        elif line[:-1] == 'EOS': # EOS
            for line_ in text[:-1].split ('\n'):
                if line_[0] == '*':
                    gold, auto = line_[2:].split (' ', 3)[-2:] # [1:3]
                    p = ""
                    pos = auto.find ('@')
                    if pos != -1:
                        if prob: p = "%.2f" % float (auto[pos + 1:])
                        auto = auto[:pos]
                    fail = gold[-1] in tag and auto[:-1] != gold[:-1]
                    wrong |= fail
                    binfo.append (Binfo (len (binfo), int (auto[:-1]), p, fail, gold))
                else:
                    if binfo[-1].morph and morph:
                        binfo[-1].morph += "|"
                    binfo[-1].morph += pat_s.split (line_, 1)[0]
            for b in binfo:
                b.len = sum (ww[width (x)] for x in b.morph)

            if not quiet or wrong:
                text = treeify (binfo)
                result += header
                result += text
                result += line
            binfo[:] = []
            header = ""
            text  = ""
            wrong = False
        else:
            text += line

    return result

# End jdepp/to_tree.py ---------------

# Export as dot(graphviz), based on to_tree.py
# Copyright: Naoki Yoshinaga <ynaga@tkl.iis.u-tokyo.ac.jp>
# Copyright: Light Transport Entertainment, Inc.
# License: BSD

def dottyfy (binfo, graph_name: str = "jdepp", label_name = "# S-ID; 1", prob: bool = False):
    # TODO: better layouting by considering binfo.width
    # TODO: show probability
    # TODO: styles for node and edge.

    s = ''
    s += 'digraph ' + graph_name + ' {\n'

    s += '\n'
    s += '  graph [\n'
    s += '    charset = "UTF-8";\n'
    s += '    label = "{}";\n'.format(label_name)
    s += '  ];\n'
    s += '\n'

    s += '\n'
    s += '  node [ shape = record ];\n'
    s += '\n'

    # define nodes
    for b in binfo:
        # escale dquote
        s += "  bunsetsu{} [label=\"{}\"];\n".format(b.id, b.morph.replace('"', '\"'))

    s += '\n'

    # define edges
    for b in binfo:
        if b.head < 0:
            # root
            continue

        s += "  bunsetsu{} -> bunsetsu{};\n".format(b.id, b.head)


    s += '\n}\n'

    return s


def to_dot(lines, morph: bool = True, ignore: str = ""):

    binfo   = []
    header  = ''
    text    = ''
    charset = ''
    wrong   = False
    pat_s = re.compile (r'[\t\s]')
    pat_i = re.compile (re.escape (ignore))
    tag = set (["D", "A", "P", "I"])
    ww ={'Na':1, 'W':2, 'F':2, 'H':1, 'A':2, 'N':1}

    if isinstance(lines, str):
        # make List[str]
        lines = [line + '\n' for line in lines.split('\n')]

    for line in lines:
        if line[:7] == '# S-ID:' or (ignore and pat_i.match (line)):
            header += line
        elif line[:-1] == 'EOS': # EOS
            for line_ in text[:-1].split ('\n'):
                if line_[0] == '*':
                    gold, auto = line_[2:].split (' ', 3)[-2:] # [1:3]
                    p = ""
                    pos = auto.find ('@')
                    if pos != -1:
                        if prob: p = "%.2f" % float (auto[pos + 1:])
                        auto = auto[:pos]
                    fail = gold[-1] in tag and auto[:-1] != gold[:-1]
                    wrong |= fail
                    binfo.append (Binfo (len (binfo), int (auto[:-1]), p, fail, gold))
                else:
                    if binfo[-1].morph and morph:
                        binfo[-1].morph += "|"
                    binfo[-1].morph += pat_s.split (line_, 1)[0]
            for b in binfo:
                b.len = sum (ww[width (x)] for x in b.morph)
            if not wrong:
                return dottyfy (binfo)

            return None # fail
        else:
            text += line

    return None # failed to parse line
